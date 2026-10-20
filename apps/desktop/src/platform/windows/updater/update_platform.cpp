#include "updater/update_platform.h"

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <sddl.h>
#include <aclapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <chrono>
#include <cwctype>
#include <stdexcept>
#include <thread>

namespace crossdesk::updater {
namespace {
bool service_was_running = false;

std::wstring Wide(const std::string& text) { return fs::u8path(text).wstring(); }

std::wstring Quote(const std::wstring& value) {
  std::wstring result = L"\"";
  unsigned slashes = 0;
  for (wchar_t c : value) {
    if (c == L'\\') { ++slashes; continue; }
    result.append(c == L'"' ? slashes * 2 + 1 : slashes, L'\\');
    slashes = 0;
    result += c;
  }
  result.append(slashes * 2, L'\\');
  return result + L"\"";
}

std::wstring Arguments(const std::vector<std::string>& args) {
  std::wstring result;
  for (const auto& argument : args) {
    if (!result.empty()) result += L' ';
    result += Quote(Wide(argument));
  }
  return result;
}

std::wstring Normalized(const fs::path& path) {
  auto value = fs::weakly_canonical(path).wstring();
  std::transform(value.begin(), value.end(), value.begin(), towlower);
  return value;
}

std::wstring LockName(const fs::path& app) {
  return L"Global\\CrossDeskUpdate-" + Wide(Sha256(fs::path(Normalized(app)).u8string()));
}

void StopService() {
  SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
  if (!manager) throw std::runtime_error("Cannot query CrossDesk service");
  SC_HANDLE service = OpenServiceW(manager, L"CrossDeskService", SERVICE_STOP | SERVICE_QUERY_STATUS);
  const DWORD error = GetLastError();
  CloseServiceHandle(manager);
  if (!service) {
    if (error == ERROR_SERVICE_DOES_NOT_EXIST) return;
    throw std::runtime_error("Cannot open CrossDesk service");
  }
  SERVICE_STATUS_PROCESS status{};
  DWORD bytes = 0;
  if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
      reinterpret_cast<BYTE*>(&status), sizeof(status), &bytes)) {
    CloseServiceHandle(service);
    throw std::runtime_error("Cannot query CrossDesk service state");
  }
  service_was_running = status.dwCurrentState == SERVICE_RUNNING ||
                        status.dwCurrentState == SERVICE_START_PENDING;
  SERVICE_STATUS ignored{};
  if (status.dwCurrentState != SERVICE_STOPPED && status.dwCurrentState != SERVICE_STOP_PENDING &&
      !ControlService(service, SERVICE_CONTROL_STOP, &ignored)) {
    CloseServiceHandle(service);
    throw std::runtime_error("Cannot stop CrossDesk service");
  }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (std::chrono::steady_clock::now() < deadline) {
    if (!QueryServiceStatusEx(service, SC_STATUS_PROCESS_INFO,
        reinterpret_cast<BYTE*>(&status), sizeof(status), &bytes)) break;
    if (status.dwCurrentState == SERVICE_STOPPED) {
      CloseServiceHandle(service);
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }
  CloseServiceHandle(service);
  throw std::runtime_error("CrossDesk service did not stop");
}
}  // namespace

fs::path ExecutablePath() {
  wchar_t buffer[32768];
  const DWORD size = GetModuleFileNameW(nullptr, buffer, 32768);
  if (!size || size >= 32768) throw std::runtime_error("Cannot locate executable");
  return fs::path(std::wstring(buffer, size));
}

fs::path HelperPath(const fs::path& application) {
  return application.parent_path() / L"crossdesk_updater.exe";
}

bool SupportedInstallation(const fs::path& application) {
  // Only the installer owns this value. Portable/development directories cannot
  // become destinations for an elevated update.
  wchar_t directory[32768];
  DWORD bytes = sizeof(directory);
  if (RegGetValueW(HKEY_LOCAL_MACHINE,
      L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\CrossDesk",
      L"InstallLocation", RRF_RT_REG_SZ | RRF_SUBKEY_WOW6432KEY, nullptr,
      directory, &bytes) != ERROR_SUCCESS) return false;
  return Normalized(fs::path(directory) / L"CrossDesk.exe") == Normalized(application) &&
         fs::is_regular_file(HelperPath(application));
}

bool IsPrivileged() {
  SID_IDENTIFIER_AUTHORITY authority = SECURITY_NT_AUTHORITY;
  PSID admins = nullptr;
  BOOL member = FALSE;
  if (AllocateAndInitializeSid(&authority, 2, SECURITY_BUILTIN_DOMAIN_RID,
      DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &admins)) {
    CheckTokenMembership(nullptr, admins, &member);
    FreeSid(admins);
  }
  return member != FALSE;
}

fs::path PrivateDirectory() {
  PWSTR base = nullptr;
  if (FAILED(SHGetKnownFolderPath(IsPrivileged() ? FOLDERID_ProgramData : FOLDERID_LocalAppData,
      0, nullptr, &base))) throw std::runtime_error("Cannot locate update storage");
  const fs::path directory = fs::path(base) / (L"CrossDesk-update-" + Wide(RandomId()));
  CoTaskMemFree(base);
  std::wstring acl = IsPrivileged() ? L"O:BAG:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)"
                                    : L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)";
  if (!IsPrivileged()) {
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
      throw std::runtime_error("Cannot read update owner");
    DWORD bytes = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
    std::vector<unsigned char> data(bytes);
    const BOOL ok = GetTokenInformation(token, TokenUser, data.data(), bytes, &bytes);
    CloseHandle(token);
    if (!ok) throw std::runtime_error("Cannot read update owner");
    PWSTR sid = nullptr;
    if (!ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER*>(data.data())->User.Sid, &sid))
      throw std::runtime_error("Cannot format update owner");
    acl += L"(A;OICI;FA;;;" + std::wstring(sid) + L")";
    LocalFree(sid);
  }
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  if (!ConvertStringSecurityDescriptorToSecurityDescriptorW(acl.c_str(), SDDL_REVISION_1,
      &descriptor, nullptr)) throw std::runtime_error("Cannot protect update storage");
  SECURITY_ATTRIBUTES attributes{sizeof(attributes), descriptor, FALSE};
  const BOOL created = CreateDirectoryW(directory.c_str(), &attributes);
  LocalFree(descriptor);
  if (!created) throw std::runtime_error("Cannot create private update directory");
  return directory;
}

bool OwnedJobDirectory(const fs::path& job) {
  if (!IsPrivileged()) return true;
  // Elevated coordinators only write to a directory owned by Administrators,
  // never a caller-controlled directory with reparse points or writable ACLs.
  PSID owner = nullptr;
  PACL acl = nullptr;
  PSECURITY_DESCRIPTOR descriptor = nullptr;
  if (GetNamedSecurityInfoW(job.c_str(), SE_FILE_OBJECT,
      OWNER_SECURITY_INFORMATION | DACL_SECURITY_INFORMATION,
      &owner, nullptr, &acl, nullptr, &descriptor) != ERROR_SUCCESS) return false;
  bool safe = owner && IsWellKnownSid(owner, WinBuiltinAdministratorsSid) && acl;
  for (DWORD i = 0; safe && i < acl->AceCount; ++i) {
    void* raw = nullptr;
    if (!GetAce(acl, i, &raw)) { safe = false; break; }
    const auto ace = static_cast<ACCESS_ALLOWED_ACE*>(raw);
    if (ace->Header.AceType != ACCESS_ALLOWED_ACE_TYPE ||
        (!IsWellKnownSid(&ace->SidStart, WinBuiltinAdministratorsSid) &&
         !IsWellKnownSid(&ace->SidStart, WinLocalSystemSid))) safe = false;
  }
  LocalFree(descriptor);
  return safe;
}

void Launch(const fs::path& executable, const std::vector<std::string>& arguments) {
  auto command = Quote(executable.wstring()) + L" " + Arguments(arguments);
  STARTUPINFOW startup{sizeof(startup)};
  PROCESS_INFORMATION process{};
  const auto cwd = executable.parent_path();
  if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, FALSE,
      CREATE_NO_WINDOW, nullptr, cwd.c_str(), &startup, &process))
    throw std::runtime_error("Cannot start update process");
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
}

int Run(const fs::path& executable, const std::vector<std::string>& arguments, bool elevate) {
  const auto parameters = Arguments(arguments);
  const auto cwd = executable.parent_path();
  SHELLEXECUTEINFOW execution{sizeof(execution)};
  execution.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
  execution.lpVerb = elevate ? L"runas" : L"open";
  execution.lpFile = executable.c_str();
  execution.lpParameters = parameters.c_str();
  execution.lpDirectory = cwd.c_str();
  execution.nShow = SW_HIDE;
  if (!ShellExecuteExW(&execution) || !execution.hProcess)
    throw std::runtime_error("Installation authorization was cancelled or could not start");
  WaitForSingleObject(execution.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(execution.hProcess, &code);
  CloseHandle(execution.hProcess);
  return static_cast<int>(code);
}

int RunElevated(const fs::path& executable, const fs::path& job) {
  return Run(executable, {"--install", job.u8string()}, true);
}

void WaitForApplicationExit(const fs::path& application) {
  StopService();
  const auto expected = Normalized(application);
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
  do {
    bool running = false;
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot enumerate application processes");
    PROCESSENTRY32W entry{sizeof(entry)};
    if (Process32FirstW(snapshot, &entry)) do {
      if (_wcsicmp(entry.szExeFile, application.filename().c_str()) != 0) continue;
      HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE,
                                   FALSE, entry.th32ProcessID);
      if (!process) { running = true; continue; }
      wchar_t image[32768];
      DWORD size = 32768;
      if (!QueryFullProcessImageNameW(process, 0, image, &size)) running = true;
      else if (Normalized(fs::path(std::wstring(image, size))) == expected &&
               WaitForSingleObject(process, 0) == WAIT_TIMEOUT) running = true;
      CloseHandle(process);
    } while (Process32NextW(snapshot, &entry));
    CloseHandle(snapshot);
    if (!running) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  } while (std::chrono::steady_clock::now() < deadline);
  throw std::runtime_error("CrossDesk is still running in this or another session");
}

void InstallPackage(const fs::path& package, const fs::path& application) {
  // NSIS requires /D to be the final argument, without quotes around its value.
  std::wstring command = Quote(package.wstring()) + L" /S /CROSSDESK_UPDATE /D=" +
                         application.parent_path().wstring();
  STARTUPINFOW startup{sizeof(startup)};
  PROCESS_INFORMATION process{};
  const auto cwd = package.parent_path();
  if (!CreateProcessW(package.c_str(), command.data(), nullptr, nullptr, FALSE,
      CREATE_NO_WINDOW, nullptr, cwd.c_str(), &startup, &process))
    throw std::runtime_error("Cannot start CrossDesk installer");
  WaitForSingleObject(process.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(process.hProcess, &code);
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  if (code != 0) throw std::runtime_error("CrossDesk installer failed: " + std::to_string(code));
}

void RestoreServices() {
  if (!service_was_running) return;
  SC_HANDLE manager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
  if (!manager) return;
  SC_HANDLE service = OpenServiceW(manager, L"CrossDeskService", SERVICE_START);
  if (service) { StartServiceW(service, 0, nullptr); CloseServiceHandle(service); }
  CloseServiceHandle(manager);
}

void Restart(const fs::path& application) { Launch(application, {}); }

std::shared_ptr<void> PinHelper(const fs::path& helper) {
  HANDLE handle = CreateFileW(helper.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (handle == INVALID_HANDLE_VALUE) throw std::runtime_error("Cannot lock update helper");
  return {handle, [](void* value) { CloseHandle(value); }};
}

MaintenanceLock::MaintenanceLock(const fs::path& application) {
  HANDLE mutex = CreateMutexExW(nullptr, LockName(application).c_str(), 0, SYNCHRONIZE);
  const DWORD error = GetLastError();
  if (!mutex || error == ERROR_ALREADY_EXISTS) {
    if (mutex) CloseHandle(mutex);
    throw std::runtime_error("An update is already running");
  }
  handle_ = reinterpret_cast<std::intptr_t>(mutex);
}
MaintenanceLock::~MaintenanceLock() {
  if (handle_ != -1) CloseHandle(reinterpret_cast<HANDLE>(handle_));
}
bool MaintenanceActive(const fs::path& application) {
  HANDLE mutex = OpenMutexW(SYNCHRONIZE, FALSE, LockName(application).c_str());
  if (mutex) { CloseHandle(mutex); return true; }
  return GetLastError() != ERROR_FILE_NOT_FOUND;
}
}  // namespace crossdesk::updater
