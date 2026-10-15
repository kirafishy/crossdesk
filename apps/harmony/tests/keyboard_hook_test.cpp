#include "keyboard_hook.h"
#include "keyboard_hook_mock.h"
#include "keyboard_state.h"
#include <cassert>
#include <memory>

using namespace crossdesk;
using namespace crossdesk::harmony::desktop;
std::vector<KeyboardInput> delivered;
void Sink(int key, bool down, uint32_t scan, bool extended, void* context) {
  assert(context == &delivered); delivered.push_back({key, down, scan, extended});
}
void Event(int code, int action) {
  static int id = 0;
  const Input_KeyEvent event{code, action, ++id};
  assert(hook_callback); hook_callback(&event);
}
int main() {
  KeyboardHook hook;
  assert(hook.Start(Sink, &delivered) == INPUT_APP_NOT_FOCUSED);
  SetKeyboardFocus(2, true);
  hook_add_result = INPUT_PERMISSION_DENIED;
  assert(hook.Start(Sink, &delivered) == INPUT_PERMISSION_DENIED);
  assert(!hook.Active() && hook.Stop() == INPUT_SUCCESS);
  hook_add_result = INPUT_SUCCESS;
  assert(hook.Start(Sink, &delivered) == INPUT_SUCCESS && hook.Active());
  const int installed_count = hook_adds;
  assert(hook.Start(Sink, &delivered) == INPUT_SUCCESS && hook_adds == installed_count);
  KeyboardHook other;
  assert(other.Start(Sink, &delivered) == INPUT_REPEAT_INTERCEPTOR);
  assert(other.Stop() == INPUT_SUCCESS && hook.Active());
  // Alt+Tab and Meta+L reach the same shared input queue as ordinary keys.
  for (auto [modifier, key] : {std::pair{2045, 2049}, std::pair{2076, 2028}}) {
    Event(modifier, KEY_ACTION_DOWN); Event(key, KEY_ACTION_DOWN);
    Event(key, KEY_ACTION_UP); Event(modifier, KEY_ACTION_UP);
  }
  assert(delivered.size() == 8 && forwarded_events.empty());
  assert(delivered[0].key_code == 0xA4 && delivered[1].key_code == 0x09);
  assert(delivered[4].key_code == 0x5B && delivered[4].extended && delivered[5].key_code == 'L');
  Event(2073, KEY_ACTION_DOWN); Event(2119, KEY_ACTION_DOWN); Event(2119, KEY_ACTION_DOWN);
  assert(delivered.back().key_code == 0x0D && delivered.back().scan_code == 0x1C && delivered.back().extended);
  Event(2119, KEY_ACTION_CANCEL);
  assert(!delivered.back().is_down && delivered.back().key_code == 0xA3);
  assert(delivered[delivered.size()-2].key_code == 0x0D);
  const auto count = delivered.size();
  Event(9999, KEY_ACTION_DOWN); Event(2017, KEY_ACTION_UP);
  assert(delivered.size() == count && forwarded_events.size() == 2);
  Event(2072, KEY_ACTION_DOWN);
  SetKeyboardFocus(2, false);
  assert(!hook.Active() && !delivered.back().is_down);
  Event(2017, KEY_ACTION_DOWN);
  assert(forwarded_events.size() == 3);
  SetKeyboardFocus(1, true); Event(2017, KEY_ACTION_DOWN);
  assert(!hook.Active() && forwarded_events.size() == 4);
  hook.Stop(); SetKeyboardFocus(2, true);
  hook.Start(Sink, &delivered); Event(2076, KEY_ACTION_DOWN);
  SetKeyboardHookPermission(false);
  assert(!hook.Active() && !delivered.back().is_down);
  Event(2017, KEY_ACTION_DOWN); assert(forwarded_events.size() == 5);
  // Failed unregistration must leave a pass-through callback, never a dangling sink.
  hook_remove_result = INPUT_SERVICE_EXCEPTION;
  assert(hook.Stop() == INPUT_SERVICE_EXCEPTION);
  const auto after_stop = delivered.size();
  Event(2017, KEY_ACTION_DOWN); assert(delivered.size() == after_stop);
  assert(hook.Start(Sink, &delivered) == INPUT_SERVICE_EXCEPTION);
  hook_remove_result = INPUT_SUCCESS;
  assert(hook.Start(Sink, &delivered) == INPUT_SUCCESS && hook.Active());
  hook.Stop();
  // A focus change inside the OS registration call cannot bind the hook to
  // a local dialog that replaced the control window during installation.
  focus_during_add = 1;
  hook.Start(Sink, &delivered); assert(!hook.Active());
  Event(2017, KEY_ACTION_DOWN); assert(delivered.size() == after_stop);
  hook.Stop();
  focus_during_add = 0;
  {
    auto transient = std::make_unique<KeyboardHook>();
    transient->Start(Sink, &delivered);
    hook_remove_result = INPUT_SERVICE_EXCEPTION;
  }
  Event(2017, KEY_ACTION_DOWN); assert(delivered.size() == after_stop);
  hook_remove_result = INPUT_SUCCESS; hook.Stop();
}
