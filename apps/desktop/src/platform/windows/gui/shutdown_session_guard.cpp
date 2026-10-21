#include "shutdown_session_guard.h"

#include <utility>

#include "rd_log.h"

namespace crossdesk {
namespace {
constexpr wchar_t kShutdownWindowClass[] = L"CrossDesk.ShutdownSessionGuard";
}

ShutdownSessionGuard::ShutdownSessionGuard(
    std::shared_ptr<std::atomic<bool>> connected)
    : connected_(std::move(connected)) {}

ShutdownSessionGuard::~ShutdownSessionGuard() {
  ClearReason();
  if (window_) DestroyWindow(window_);
}

bool ShutdownSessionGuard::ReportError(const char* operation) {
  const DWORD error = GetLastError();
  const ULONGLONG now = GetTickCount64();
  if (last_error_tick_ == 0 || now - last_error_tick_ >= 5000) {
    LOG_WARN("Windows shutdown session guard {} failed, error={}", operation, error);
    last_error_tick_ = now;
  }
  SetLastError(error);
  return false;
}

bool ShutdownSessionGuard::Start() {
  if (window_) return true;
  WNDCLASSW klass{};
  klass.lpfnWndProc = WindowProc;
  klass.hInstance = GetModuleHandleW(nullptr);
  klass.lpszClassName = kShutdownWindowClass;
  if (!RegisterClassW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    return ReportError("register window");
  // A top-level window receives session-end broadcasts even when the Slint
  // windows are hidden in the tray. HWND_MESSAGE does not receive them.
  window_ = CreateWindowExW(WS_EX_TOOLWINDOW, kShutdownWindowClass, L"CrossDesk",
      WS_OVERLAPPED, 0, 0, 0, 0, nullptr, nullptr, klass.hInstance, this);
  if (!window_) return ReportError("create window");
  return true;
}

bool ShutdownSessionGuard::ClearReason() {
  if (!reason_registered_) return true;
  if (window_ && !ShutdownBlockReasonDestroy(window_))
    return ReportError("clear reason");
  reason_registered_ = false;
  LOG_INFO("Windows shutdown session guard released");
  return true;
}

bool ShutdownSessionGuard::SyncReason() {
  if (shutdown_allowed_ || !connected_ ||
      !connected_->load(std::memory_order_relaxed)) {
    return ClearReason();
  }
  if (reason_registered_) return true;
  if (!ShutdownBlockReasonCreate(window_, reason_.c_str()))
    return ReportError("register reason");
  reason_registered_ = true;
  LOG_INFO("Windows shutdown session guard armed for connected controller");
  return true;
}

bool ShutdownSessionGuard::Update(std::wstring_view reason) {
  if (!Start()) return false;
  if (std::wstring_view(reason_) != reason) {
    // Keep the old registration and text on failure so the next tick retries
    // removal before registering the newly selected language.
    if (!ClearReason()) return false;
    reason_.assign(reason);
  }
  return SyncReason();
}

LRESULT CALLBACK ShutdownSessionGuard::WindowProc(
    HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    auto* guard = static_cast<ShutdownSessionGuard*>(create->lpCreateParams);
    guard->window_ = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(guard));
  }
  auto* guard = reinterpret_cast<ShutdownSessionGuard*>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
  if (guard && message == WM_QUERYENDSESSION) {
    const auto flags = static_cast<DWORD>(lparam);
    // "Shut down anyway", logoff and Restart Manager must remain available.
    if ((flags & (ENDSESSION_CRITICAL | ENDSESSION_LOGOFF | ENDSESSION_CLOSEAPP)) != 0)
      guard->shutdown_allowed_ = true;
    guard->SyncReason();
    const bool retain = !guard->shutdown_allowed_ && guard->reason_registered_ &&
        guard->connected_ && guard->connected_->load(std::memory_order_relaxed);
    LOG_INFO("Windows shutdown query: flags=0x{:x}, retain_remote_session={}", flags, retain);
    return retain ? FALSE : TRUE;
  }
  if (guard && message == WM_ENDSESSION) {
    LOG_INFO("Windows session end: confirmed={}, flags=0x{:x}",
             wparam != FALSE, static_cast<DWORD>(lparam));
    guard->shutdown_allowed_ = wparam != FALSE;
    guard->SyncReason();
    // A cancelled shutdown must leave transport, capture and input running.
    return 0;
  }
  if (guard && message == WM_NCDESTROY) {
    guard->ClearReason();
    // Destruction removes the native registration even if explicit removal
    // failed. A later window must not inherit the destroyed window's state.
    guard->reason_registered_ = false;
    guard->window_ = nullptr;
    SetWindowLongPtrW(window, GWLP_USERDATA, 0);
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

}  // namespace crossdesk
