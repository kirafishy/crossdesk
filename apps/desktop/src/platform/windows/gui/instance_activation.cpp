#include "platform/instance_activation.h"
#include "platform/single_instance.h"

#include <sddl.h>

#include <utility>
#include <vector>

namespace crossdesk::platform {
namespace {

constexpr wchar_t kWindowClass[] = L"CrossDesk.InstanceActivationWindow";
constexpr wchar_t kActivationMessage[] = L"CrossDesk.ActivateMainWindow.v1";
constexpr ULONGLONG kStartupWaitMs = 5000;

std::wstring ActivationWindowName(const std::string& application,
                                  std::string& error) {
  if (!ValidInstanceApplication(application)) {
    error = "Invalid activation application name";
    return {};
  }
  HANDLE token = nullptr;
  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
    error =
        "Cannot read activation user token: " + std::to_string(GetLastError());
    return {};
  }
  DWORD size = 0;
  GetTokenInformation(token, TokenUser, nullptr, 0, &size);
  std::vector<unsigned char> buffer(size);
  if (!GetTokenInformation(token, TokenUser, buffer.data(), size, &size)) {
    const DWORD code = GetLastError();
    CloseHandle(token);
    error = "Cannot read activation user SID: " + std::to_string(code);
    return {};
  }
  CloseHandle(token);
  wchar_t* sid = nullptr;
  if (!ConvertSidToStringSidW(
          reinterpret_cast<TOKEN_USER*>(buffer.data())->User.Sid, &sid)) {
    error =
        "Cannot format activation user SID: " + std::to_string(GetLastError());
    return {};
  }
  const std::wstring name =
      std::wstring(application.begin(), application.end()) + L".Activation." + sid;
  LocalFree(sid);
  return name;
}

}  // namespace

InstanceActivationReceiver::InstanceActivationReceiver(std::string application)
    : application_(std::move(application)) {}

InstanceActivationReceiver::~InstanceActivationReceiver() {
  if (window_) DestroyWindow(window_);
}

bool InstanceActivationReceiver::Start(std::string& error) {
  error.clear();
  if (window_) return true;
  const auto name = ActivationWindowName(application_, error);
  if (name.empty()) return false;
  activation_message_ = RegisterWindowMessageW(kActivationMessage);
  if (!activation_message_) {
    error =
        "Cannot register activation message: " + std::to_string(GetLastError());
    return false;
  }
  WNDCLASSW window_class{};
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = GetModuleHandleW(nullptr);
  window_class.lpszClassName = kWindowClass;
  if (!RegisterClassW(&window_class) &&
      GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
    error =
        "Cannot register activation window: " + std::to_string(GetLastError());
    return false;
  }
  window_ =
      CreateWindowExW(0, kWindowClass, name.c_str(), 0, 0, 0, 0, 0,
                      HWND_MESSAGE, nullptr, window_class.hInstance, this);
  if (!window_) {
    error =
        "Cannot create activation window: " + std::to_string(GetLastError());
    return false;
  }
  // An unelevated portable launch may need to activate an elevated installed
  // client. Allow only this payload-free notification through UIPI.
  if (!ChangeWindowMessageFilterEx(window_, activation_message_, MSGFLT_ALLOW,
                                   nullptr)) {
    error =
        "Cannot allow activation message: " + std::to_string(GetLastError());
    DestroyWindow(window_);
    window_ = nullptr;
    return false;
  }
  return true;
}

bool InstanceActivationReceiver::TakeRequest(std::string* activation_token) {
  if (activation_token) activation_token->clear();
  return std::exchange(requested_, false);
}

LRESULT CALLBACK InstanceActivationReceiver::WindowProc(HWND window,
                                                        UINT message,
                                                        WPARAM wparam,
                                                        LPARAM lparam) {
  if (message == WM_NCCREATE) {
    const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
    SetWindowLongPtrW(window, GWLP_USERDATA,
                      reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }
  auto* receiver = reinterpret_cast<InstanceActivationReceiver*>(
      GetWindowLongPtrW(window, GWLP_USERDATA));
  if (receiver && message == receiver->activation_message_) {
    // Let the regular UI tick restore the window, rather than entering Slint
    // from a native message callback (which may run inside a nested event
    // loop).
    receiver->requested_ = true;
    return 0;
  }
  return DefWindowProcW(window, message, wparam, lparam);
}

bool RequestInstanceActivation(std::string& error, const std::string& application) {
  error.clear();
  const auto name = ActivationWindowName(application, error);
  if (name.empty()) return false;
  const UINT message = RegisterWindowMessageW(kActivationMessage);
  if (!message) {
    error =
        "Cannot register activation message: " + std::to_string(GetLastError());
    return false;
  }
  const ULONGLONG deadline = GetTickCount64() + kStartupWaitMs;
  DWORD last_error = ERROR_NOT_FOUND;
  do {
    if (const HWND window =
            FindWindowExW(HWND_MESSAGE, nullptr, kWindowClass, name.c_str())) {
      DWORD process_id = 0;
      if (GetWindowThreadProcessId(window, &process_id) && process_id != 0) {
        // Transfer the new launch's foreground permission to the GUI process,
        // not its supervisor, before posting the request. Windows may still
        // deny foreground focus; the receiver will restore visibility anyway.
        AllowSetForegroundWindow(process_id);
        if (PostMessageW(window, message, 0, 0)) return true;
        last_error = GetLastError();
      }
    }
    if (GetTickCount64() >= deadline) break;
    Sleep(50);
  } while (true);
  error = "Cannot notify the existing CrossDesk window on this desktop: " +
          std::to_string(last_error);
  return false;
}

}  // namespace crossdesk::platform
