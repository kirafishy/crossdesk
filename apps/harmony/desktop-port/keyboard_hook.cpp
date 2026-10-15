#include "keyboard_hook.h"
#include "key_mapping.h"
#include "keyboard_state.h"
#include "windows_key_metadata.h"
#include <mutex>

namespace crossdesk::harmony::desktop {
namespace {
std::mutex lifecycle, delivery;
KeyboardHook* owner = nullptr;
OnKeyAction sink = nullptr;
void* sink_context = nullptr;
int focused_window = 0, capture_window = 0;
bool installed = false, permission = false;
PressedKeys pressed;

void ReleaseCaptured() {
  if (sink) for (const auto& key : ReconcileKeyboardState(pressed, {}))
    sink(key.key_code, false, key.scan_code, key.extended, sink_context);
  pressed.clear();
}
}
void SetKeyboardFocus(int window, bool active) {
  std::lock_guard lock(delivery);
  if (active) focused_window = window;
  else if (focused_window == window) focused_window = 0;
  if (capture_window != focused_window) ReleaseCaptured();
}
void SetKeyboardHookPermission(bool granted) {
  std::lock_guard lock(delivery);
  permission = granted;
  if (!granted) ReleaseCaptured();
}
bool KeyboardHookPermissionGranted() { std::lock_guard lock(delivery); return permission; }
KeyboardHook::~KeyboardHook() { Stop(); }
int KeyboardHook::Start(OnKeyAction callback, void* context) {
  if (!callback) return INPUT_PARAMETER_ERROR;
  std::lock_guard operation(lifecycle);
  int target_window = 0;
  {
    std::lock_guard lock(delivery);
    if (owner) return owner == this ? INPUT_SUCCESS : INPUT_REPEAT_INTERCEPTOR;
    if (!focused_window) return INPUT_APP_NOT_FOCUSED;
    target_window = focused_window;
  }
  // A failed removal leaves a pass-through callback installed. Retry removal
  // before registering again; never interpret REPEAT_INTERCEPTOR as success.
  if (installed) {
    const auto result = OH_Input_RemoveKeyEventHook(Receive);
    if (result != INPUT_SUCCESS) return result;
    installed = false;
  }
  const auto result = OH_Input_AddKeyEventHook(Receive);
  if (result != INPUT_SUCCESS) return result;
  installed = true;
  {
    std::lock_guard lock(delivery);
    owner = this; sink = callback; sink_context = context;
    capture_window = target_window; permission = true; pressed.clear();
  }
  return INPUT_SUCCESS;
}
int KeyboardHook::Stop() {
  std::lock_guard operation(lifecycle);
  {
    std::lock_guard lock(delivery);
    if (owner && owner != this) return INPUT_SUCCESS;
    // Wait for an in-flight delivery, then detach the context before invoking
    // the OS. Failed removal or a queued callback cannot access a dead owner.
    owner = nullptr; sink = nullptr; sink_context = nullptr;
    capture_window = 0; pressed.clear();
  }
  if (!installed) return INPUT_SUCCESS;
  const auto result = OH_Input_RemoveKeyEventHook(Receive);
  if (result == INPUT_SUCCESS) installed = false;
  return result;
}
bool KeyboardHook::Active() const {
  std::lock_guard lock(delivery);
  return owner == this && permission && capture_window != 0 && capture_window == focused_window;
}
void KeyboardHook::Receive(const Input_KeyEvent* event) {
  if (!event) return;
  bool consumed = false;
  {
    std::lock_guard lock(delivery);
    if (owner && sink && permission && capture_window != 0 && capture_window == focused_window) {
      const auto action = OH_Input_GetKeyEventAction(event);
      const int code = OH_Input_GetKeyEventKeyCode(event);
      const int key = ToVirtualKey(code);
      if (action == KEY_ACTION_CANCEL) {
        consumed = !pressed.empty(); ReleaseCaptured();
      } else if (key && (action == KEY_ACTION_DOWN || action == KEY_ACTION_UP)) {
        const bool down = action == KEY_ACTION_DOWN;
        // Do not swallow a release whose press belonged to the local app.
        if (down || pressed.count(key)) {
          uint32_t scan = 0; bool extended = false;
          LookupWindowsKeyMetadataFromVk(key, &scan, &extended);
          if (code == 2119) { scan = 0x1C; extended = true; }
          if (down) pressed[key] = {key, scan, extended};
          else pressed.erase(key);
          sink(key, down, scan, extended, sink_context); consumed = true;
        }
      }
    }
  }
  // The SDK owns event storage. Return unhandled events in original order,
  // without reinjecting them (which would re-enter the hook).
  if (!consumed) {
    int32_t id = 0;
    if (OH_Input_GetKeyEventId(event, &id) == INPUT_SUCCESS)
      OH_Input_DispatchToNextHandler(id);
  }
}
}
