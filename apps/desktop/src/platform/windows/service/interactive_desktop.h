/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-22
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _INTERACTIVE_DESKTOP_H_
#define _INTERACTIVE_DESKTOP_H_

#include <Windows.h>
#include <WtsApi32.h>

#include <string>

namespace crossdesk {

inline bool IsDesktopTransitionInputError(DWORD error) {
  return error == ERROR_ACCESS_DENIED ||
         error == ERROR_REQUIRES_INTERACTIVE_WINDOWSTATION;
}

// Only call with an observation from the interactive session, never Session 0.
// Process presence (including LogonUI) cannot prove that Default is accessible:
// shutdown blocking UI may be active while WTS still reports an unlocked user.
inline bool IsSecureInputDesktop(bool available, const std::string& name,
                                 DWORD error) {
  if (!available) return error == ERROR_ACCESS_DENIED;
  return !name.empty() && _stricmp(name.c_str(), "Default") != 0;
}

inline std::wstring GetDesktopNameW(HDESK desktop) {
  DWORD bytes_needed = 0;
  if (desktop == nullptr) {
    return {};
  }
  GetUserObjectInformationW(desktop, UOI_NAME, nullptr, 0, &bytes_needed);
  if (bytes_needed == 0) {
    return {};
  }
  std::wstring name(bytes_needed / sizeof(wchar_t), L'\0');
  if (!GetUserObjectInformationW(desktop, UOI_NAME, name.data(), bytes_needed,
                                 &bytes_needed)) {
    return {};
  }
  while (!name.empty() && name.back() == L'\0') {
    name.pop_back();
  }
  return name;
}

inline bool IsDesktopReceivingInput(HDESK desktop) {
  BOOL receiving_input = FALSE;
  return desktop != nullptr &&
         GetUserObjectInformationW(desktop, UOI_IO, &receiving_input,
                                   sizeof(receiving_input), nullptr) &&
         receiving_input != FALSE;
}

inline bool IsCurrentProcessSession(DWORD session_id) {
  DWORD process_session_id = 0xFFFFFFFF;
  return session_id != 0xFFFFFFFF &&
         ProcessIdToSessionId(GetCurrentProcessId(), &process_session_id) &&
         session_id == process_session_id;
}

inline bool DesktopProbeFailed(std::string* reason, const char* stage, DWORD value) {
  if (reason) *reason = std::string(stage) + ":" + std::to_string(value);
  return false;
}

inline bool IsCurrentSessionUnlocked(std::string* reason = nullptr) {
  if (reason) reason->clear();
  DWORD session_id = 0xFFFFFFFF;
  if (!ProcessIdToSessionId(GetCurrentProcessId(), &session_id))
    return DesktopProbeFailed(reason, "session_id_query_failed", GetLastError());
  PWTSINFOEXW info = nullptr;
  DWORD bytes = 0;
  if (!WTSQuerySessionInformationW(
          WTS_CURRENT_SERVER_HANDLE, session_id, WTSSessionInfoEx,
          reinterpret_cast<LPWSTR*>(&info), &bytes)) {
    return DesktopProbeFailed(reason, "session_info_query_failed", GetLastError());
  }
  const bool valid = info && bytes >= sizeof(WTSINFOEXW) && info->Level == 1;
  const DWORD state = valid ? info->Data.WTSInfoExLevel1.SessionState : 0xFFFFFFFF;
  const DWORD flags = valid ? info->Data.WTSInfoExLevel1.SessionFlags : 0xFFFFFFFF;
  if (info != nullptr) WTSFreeMemory(info);
  if (!valid) return DesktopProbeFailed(reason, "session_info_invalid", bytes);
  if (state != WTSActive) return DesktopProbeFailed(reason, "session_not_active", state);
  if (flags != WTS_SESSIONSTATE_UNLOCK)
    return DesktopProbeFailed(reason, "session_not_unlocked", flags);
  return true;
}

inline bool IsCurrentSessionUserDesktopActive(std::string* reason = nullptr) {
  // LockApp can be visible on Default while the session is still locked.
  if (!IsCurrentSessionUnlocked(reason)) return false;
  HDESK desktop = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
  if (desktop == nullptr)
    return DesktopProbeFailed(reason, "open_input_desktop_failed", GetLastError());
  // A minimized RDP connection can suppress desktop output while WTS still
  // reports Active/Unlocked. Do not treat its black WGC callbacks as recovery.
  BOOL receiving_input = FALSE;
  const bool queried = GetUserObjectInformationW(
      desktop, UOI_IO, &receiving_input, sizeof(receiving_input), nullptr) != FALSE;
  const DWORD error = queried ? ERROR_SUCCESS : GetLastError();
  const bool is_default = queried && receiving_input &&
      _wcsicmp(GetDesktopNameW(desktop).c_str(), L"Default") == 0;
  CloseDesktop(desktop);
  if (!queried) return DesktopProbeFailed(reason, "desktop_input_query_failed", error);
  if (!receiving_input) return DesktopProbeFailed(reason, "desktop_not_receiving_input", 0);
  if (!is_default) return DesktopProbeFailed(reason, "desktop_not_default", 0);
  return true;
}

// Use only on worker threads without windows or hooks. Restore the original
// desktop before closing the handle: Windows cannot close a desktop in use.
class ScopedInteractiveDesktop {
 public:
  ScopedInteractiveDesktop()
      : original_(GetThreadDesktop(GetCurrentThreadId())) {}
  ScopedInteractiveDesktop(const ScopedInteractiveDesktop&) = delete;
  ScopedInteractiveDesktop& operator=(const ScopedInteractiveDesktop&) = delete;

  ~ScopedInteractiveDesktop() {
    if (desktop_ != nullptr) {
      SetThreadDesktop(original_);
      CloseDesktop(desktop_);
    }
  }

  bool Bind(const std::wstring& fallback_name) {
    constexpr ACCESS_MASK access =
        DESKTOP_CREATEWINDOW | DESKTOP_WRITEOBJECTS | DESKTOP_READOBJECTS |
        DESKTOP_SWITCHDESKTOP | DESKTOP_JOURNALPLAYBACK;
    // SendInput requires playback access on the attached desktop; otherwise
    // it returns ERROR_ACCESS_DENIED even for a correctly attached SYSTEM helper.
    // The service's stage/name can lag a lock, unlock, or UAC transition.
    // Opening a cached desktop can succeed even when it is no longer visible.
    HDESK desktop = OpenInputDesktop(0, FALSE, access);
    if (desktop == nullptr && !fallback_name.empty()) {
      desktop = OpenDesktopW(fallback_name.c_str(), 0, FALSE, access);
    }
    if (desktop == nullptr) {
      return false;
    }
    if (!SetThreadDesktop(desktop)) {
      const DWORD error = GetLastError();
      CloseDesktop(desktop);
      SetLastError(error);
      return false;
    }
    if (desktop_ != nullptr) {
      CloseDesktop(desktop_);
    }
    desktop_ = desktop;
    name_ = GetDesktopNameW(desktop_);
    return true;
  }

  bool IsInputDesktop() const {
    HDESK input = OpenInputDesktop(0, FALSE, DESKTOP_READOBJECTS);
    if (input == nullptr) {
      return false;
    }
    const std::wstring input_name = GetDesktopNameW(input);
    CloseDesktop(input);
    return !name_.empty() && _wcsicmp(name_.c_str(), input_name.c_str()) == 0;
  }

  bool IsReceivingInput() const {
    return IsDesktopReceivingInput(desktop_);
  }

  const std::wstring& name() const { return name_; }

 private:
  HDESK original_ = nullptr;
  HDESK desktop_ = nullptr;
  std::wstring name_;
};

}  // namespace crossdesk

#endif
