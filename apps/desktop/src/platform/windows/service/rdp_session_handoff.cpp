#include "rdp_session_handoff.h"

#include <WtsApi32.h>

#include <filesystem>
#include <vector>

namespace crossdesk {
namespace {
struct Handle {
  HANDLE value = nullptr;
  ~Handle() { if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value); }
};

bool ReadTokenUser(HANDLE token, std::vector<unsigned char>& data) {
  DWORD bytes = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
  if (bytes == 0) return false;
  data.resize(bytes);
  return GetTokenInformation(token, TokenUser, data.data(), bytes, &bytes) != FALSE;
}

bool RunningAsSystem() {
  Handle token;
  std::vector<unsigned char> user;
  return OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token.value) &&
         ReadTokenUser(token.value, user) &&
         IsWellKnownSid(reinterpret_cast<TOKEN_USER*>(user.data())->User.Sid,
                        WinLocalSystemSid);
}

template <typename Value>
bool QuerySessionValue(DWORD session, WTS_INFO_CLASS field, Value& value) {
  LPWSTR data = nullptr;
  DWORD bytes = 0;
  if (!WTSQuerySessionInformationW(WTS_CURRENT_SERVER_HANDLE, session, field,
                                   &data, &bytes)) return false;
  const bool valid = data && bytes >= sizeof(Value);
  if (valid) value = *reinterpret_cast<const Value*>(data);
  WTSFreeMemory(data);
  return valid;
}

bool IsInstalledClient(HANDLE process) {
  wchar_t service_path[32768]{}, client_path[32768]{};
  const DWORD length = GetModuleFileNameW(nullptr, service_path, ARRAYSIZE(service_path));
  DWORD capacity = ARRAYSIZE(client_path);
  if (!length || length >= ARRAYSIZE(service_path) ||
      !QueryFullProcessImageNameW(process, 0, client_path, &capacity)) return false;
  const auto expected = std::filesystem::path(service_path).parent_path() / L"crossdesk.exe";
  return _wcsicmp(expected.c_str(), client_path) == 0;
}

bool VerifyOwner(HANDLE process, DWORD session) {
  Handle caller, owner;
  std::vector<unsigned char> caller_user, owner_user;
  DWORD token_session = 0xFFFFFFFF, bytes = 0;
  if (!OpenProcessToken(process, TOKEN_QUERY, &caller.value) ||
      !GetTokenInformation(caller.value, TokenSessionId, &token_session,
                           sizeof(token_session), &bytes) || token_session != session ||
      !ReadTokenUser(caller.value, caller_user) ||
      !WTSQueryUserToken(session, &owner.value) || !ReadTokenUser(owner.value, owner_user))
    return false;
  return EqualSid(reinterpret_cast<TOKEN_USER*>(caller_user.data())->User.Sid,
                  reinterpret_cast<TOKEN_USER*>(owner_user.data())->User.Sid) != FALSE;
}

const char* ValidateSessions(DWORD session, bool observed_remote,
                             DWORD& console_session) {
  console_session = WTSGetActiveConsoleSessionId();
  if (session == console_session) return nullptr;
  if (console_session == 0 || console_session == 0xFFFFFFFF)
    return "rdp_handoff_state_unavailable";

  WTS_CONNECTSTATE_CLASS source_state{}, console_state{};
  USHORT protocol = 0;
  wchar_t source_user = 0, console_user = 0;
  // Only the first character is needed to distinguish an empty user name.
  if (!QuerySessionValue(session, WTSConnectState, source_state) ||
      (!observed_remote &&
       !QuerySessionValue(session, WTSClientProtocolType, protocol)) ||
      !QuerySessionValue(session, WTSUserName, source_user) || !source_user ||
      !QuerySessionValue(console_session, WTSUserName, console_user) ||
      !QuerySessionValue(console_session, WTSConnectState, console_state))
    return "rdp_handoff_state_unavailable";
  if (source_state != WTSDisconnected || (!observed_remote && protocol != 2))
    return "rdp_handoff_session_not_disconnected";
  // Even a locked console with the same account must not be displaced.
  if (console_user || (console_state != WTSConnected && console_state != WTSActive))
    return "rdp_handoff_console_busy";
  return nullptr;
}
}  // namespace

bool IsCurrentProcessSessionDisconnected() {
  DWORD session = 0xFFFFFFFF;
  WTS_CONNECTSTATE_CLASS state{};
  return ProcessIdToSessionId(GetCurrentProcessId(), &session) &&
         QuerySessionValue(session, WTSConnectState, state) && state == WTSDisconnected;
}

RdpHandoffResult HandoffDisconnectedRdpSession(HANDLE pipe, DWORD client_session,
                                              bool observed_rdp_session) {
  if (!RunningAsSystem()) return {"rdp_handoff_requires_service"};
  if (client_session == 0 || client_session == 0xFFFFFFFF)
    return {"rdp_handoff_client_denied", ERROR_ACCESS_DENIED};
  ULONG pid = 0, pipe_session = 0xFFFFFFFF;
  if (!GetNamedPipeClientProcessId(pipe, &pid) ||
      !GetNamedPipeClientSessionId(pipe, &pipe_session))
    return {"rdp_handoff_client_denied", GetLastError()};
  if (pipe_session != client_session)
    return {"rdp_handoff_client_denied", ERROR_ACCESS_DENIED};
  Handle process{OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE, FALSE, pid)};
  if (!process.value) return {"rdp_handoff_process_query_failed", GetLastError()};
  if (!IsInstalledClient(process.value))
    return {"rdp_handoff_client_path_mismatch", ERROR_ACCESS_DENIED};
  if (!VerifyOwner(process.value, client_session))
    return {"rdp_handoff_owner_unverified", ERROR_ACCESS_DENIED};

  DWORD console_session = 0xFFFFFFFF;
  if (const char* error = ValidateSessions(client_session, observed_rdp_session,
                                          console_session))
    return {error};
  if (client_session == console_session) return {nullptr, 0, true};

  // Recheck immediately before acting: WTS enumeration and token queries can
  // span a logon, reconnect or console switch. Retain the process object by
  // handle to prevent PID reuse and refuse a change of console destination.
  if (WaitForSingleObject(process.value, 0) != WAIT_TIMEOUT ||
      !VerifyOwner(process.value, client_session))
    return {"rdp_handoff_client_denied", ERROR_ACCESS_DENIED};
  DWORD current_console = 0xFFFFFFFF;
  if (const char* error = ValidateSessions(client_session, observed_rdp_session,
                                          current_console))
    return {error};
  if (client_session == current_console) return {nullptr, 0, true};
  if (console_session != current_console)
    return {"rdp_handoff_console_changed"};

  // Ask Windows to perform the same session routing as tscon /dest:console.
  // Asynchronous: the capture/status loops observe the resulting WTS change;
  // the single service IPC thread must remain available for status and input.
  wchar_t password[] = L"";
  if (!WTSConnectSessionW(client_session, current_console, password, FALSE))
    return {"rdp_handoff_connect_failed", GetLastError()};
  return {};
}
}  // namespace crossdesk
