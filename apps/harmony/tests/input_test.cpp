#include "input.h"
#include "key_mapping.h"
#include <multimodalinput/oh_input_manager.h>
#include <cassert>
#include <cmath>
#include <set>
#include <vector>
#include <hilog/log.h>

// Execute the production input adapter against an in-memory Input Kit. These
// tests exercise releases and coordinate bounds without injecting real input.
struct Input_KeyEvent { int code = 0, action = 0; };
struct Input_MouseEvent { int action = 0, button = -1, x = 0, y = 0, display = 0, axis = 0; double value = 0; };
std::set<int> pressed_keys;
std::set<int> pressed_buttons;
std::vector<Input_MouseEvent> pointer_events;
std::vector<Input_KeyEvent> key_events;
bool allow = true;
int rejected_key = -1;
extern "C" {
int OH_LOG_Print(LogType, LogLevel, unsigned int, const char*, const char*, ...) { return 0; }
Input_KeyEvent* OH_Input_CreateKeyEvent() { return new Input_KeyEvent; }
void OH_Input_DestroyKeyEvent(Input_KeyEvent** e) { delete *e; *e = nullptr; }
void OH_Input_SetKeyEventKeyCode(Input_KeyEvent* e, int32_t code) { e->code = code; }
void OH_Input_SetKeyEventAction(Input_KeyEvent* e, int32_t action) { e->action = action; }
int32_t OH_Input_InjectKeyEvent(const Input_KeyEvent* e) {
  if (!allow || e->code == rejected_key) return INPUT_PERMISSION_DENIED;
  key_events.push_back(*e);
  if (e->action == KEY_ACTION_DOWN) pressed_keys.insert(e->code); else pressed_keys.erase(e->code);
  return INPUT_SUCCESS;
}
Input_MouseEvent* OH_Input_CreateMouseEvent() { return new Input_MouseEvent; }
void OH_Input_DestroyMouseEvent(Input_MouseEvent** e) { delete *e; *e = nullptr; }
void OH_Input_SetMouseEventAction(Input_MouseEvent* e, int32_t action) { e->action = action; }
void OH_Input_SetMouseEventButton(Input_MouseEvent* e, int32_t button) { e->button = button; }
void OH_Input_SetMouseEventDisplayId(Input_MouseEvent* e, int32_t display) { e->display = display; }
void OH_Input_SetMouseEventDisplayX(Input_MouseEvent* e, int32_t x) { e->x = x; }
void OH_Input_SetMouseEventDisplayY(Input_MouseEvent* e, int32_t y) { e->y = y; }
void OH_Input_SetMouseEventAxisType(Input_MouseEvent* e, int32_t axis) { e->axis = axis; }
void OH_Input_SetMouseEventAxisValue(Input_MouseEvent* e, float value) { e->value = value; }
int32_t OH_Input_InjectMouseEvent(const Input_MouseEvent* e) {
  if (!allow) return INPUT_PERMISSION_DENIED;
  pointer_events.push_back(*e);
  if (e->action == MOUSE_ACTION_BUTTON_DOWN) pressed_buttons.insert(e->button);
  if (e->action == MOUSE_ACTION_BUTTON_UP) pressed_buttons.erase(e->button);
  return INPUT_SUCCESS;
}
}

int main() {
  using namespace crossdesk;
  using namespace crossdesk::harmony;
  for (auto [native, wire] : kSpecialKeys) {
    assert(ToVirtualKey(native) == wire); assert(FromVirtualKey(wire) == native);
  }
  assert(ToVirtualKey(2017) == 'A' && FromVirtualKey('Z') == 2042);
  assert(ToVirtualKey(2068) == 0x21 && ToVirtualKey(2069) == 0x22);
  assert(ToVirtualKey(0) == 0 && FromVirtualKey(0xFFFF) == 0);
  Input input;
  input.Configure(1920, 1080, 7);
  RemoteAction mouse_action{}; mouse_action.type = mouse;
  mouse_action.m = {-2, 3, 0, left_down};
  assert(input.Apply(mouse_action));
  assert(pointer_events.back().x == 0 && pointer_events.back().y == 1079);
  assert(pointer_events.back().display == 7 && pressed_buttons.count(MOUSE_BUTTON_LEFT));
  RemoteAction key{}; key.type = keyboard; key.k = {0xA2, 0, false, key_down};
  assert(input.Apply(key) && pressed_keys.count(2072));
  RemoteAction state{}; state.type = keyboard_state; state.ks.seq = 10;
  state.ks.pressed_count = 1; state.ks.pressed_keys[0] = {0x41, 0, false};
  assert(input.Apply(state) && !pressed_keys.count(2072) && pressed_keys.count(2017));
  state.ks.seq = 9; state.ks.pressed_count = 0;
  assert(input.Apply(state) && pressed_keys.count(2017)); // Ignore reordered snapshots.
  input.Release();
  assert(pressed_keys.empty() && pressed_buttons.empty());
  input.Release(); // Repeated teardown is safe.
  auto count = pointer_events.size();
  mouse_action.m.x = NAN;
  assert(!input.Apply(mouse_action) && pointer_events.size() == count);
  mouse_action.m = {.5f, .5f, 9000, wheel_vertical};
  assert(input.Apply(mouse_action));
  assert(pointer_events.size() == count + 3 && pointer_events.back().action == MOUSE_ACTION_AXIS_END);
  assert(pointer_events[count].value == 0 && pointer_events[count + 1].value == 1200 && pointer_events[count + 2].value == 0);
  mouse_action.m = {.25f, .75f, 0, left_down};
  assert(input.Apply(mouse_action));
  assert(input.Apply(key));
  input.Configure(2560, 1440, 42); // Switching screens releases the old screen first.
  assert(pressed_keys.empty() && pressed_buttons.empty());
  assert(pointer_events.back().display == 7 && pointer_events.back().action == MOUSE_ACTION_BUTTON_UP);
  mouse_action.m = {1.f, 1.f, 0, move};
  assert(input.Apply(mouse_action));
  assert(pointer_events.back().display == 42 && pointer_events.back().x == 2559 && pointer_events.back().y == 1439);
  allow = false;
  assert(!input.Apply(key) && pressed_keys.empty());
  allow = true;
  assert(FromVirtualKey(0x10, 0x36) == 2048);
  assert(FromVirtualKey(0x11, 0x1D, true) == 2073);
  assert(FromVirtualKey(0x12, 0x38, true) == 2046);
  assert(FromVirtualKey(0x0D, 0x1C, true) == 2119);
  assert(FromVirtualKey(0x23, 0x4F, false) == 2104);
  assert(FromVirtualKey(0x23, 0x4F, true) == 2082);
  assert(FromVirtualKey(0x7C) == 2816 && ToVirtualKey(2827) == 0x87);
  mouse_action.m = {.5f, .5f, 0, left_down}; assert(input.Apply(mouse_action));
  mouse_action.m.flag = move; assert(input.Apply(mouse_action));
  assert(pointer_events.back().button == MOUSE_BUTTON_LEFT); // Required by WMS for a drag.
  mouse_action.m.flag = right_down; assert(input.Apply(mouse_action));
  mouse_action.m.flag = left_up; assert(input.Apply(mouse_action));
  mouse_action.m.flag = move; assert(input.Apply(mouse_action));
  assert(pointer_events.back().button == MOUSE_BUTTON_RIGHT);
  assert(input.Release());
  state.ks.seq = 11; state.ks.pressed_count = 2;
  state.ks.pressed_keys[0] = {0x41, 0x1E, false};
  state.ks.pressed_keys[1] = {0x11, 0x1D, true};
  key_events.clear(); assert(input.Apply(state));
  assert(key_events.size() == 2 && key_events[0].code == 2073 && key_events[1].code == 2017);
  allow = false; assert(!input.Release());
  assert(!input.Configure(800, 600, 8)); // Do not change screen while a release is still pending.
  allow = true; key_events.clear(); assert(input.Release());
  assert(key_events.size() == 2 && key_events[0].code == 2017 && key_events[1].code == 2073);
  assert(pressed_keys.empty() && pressed_buttons.empty());
  assert(input.Release());
  state.ks.pressed_count = kMaxKeyboardStateKeys + 1; assert(!input.Apply(state));
  key_events.clear();
  assert(input.LockWorkstation());
  assert(key_events.size() == 4 && key_events[0].code == 2076 && key_events[1].code == 2028);
  assert(key_events[2].action == KEY_ACTION_UP && key_events[3].action == KEY_ACTION_UP);
  assert(pressed_keys.empty());
  allow = false; assert(!input.LockWorkstation());
  assert(pressed_keys.empty());
  allow = true; rejected_key = 2028; key_events.clear();
  assert(!input.LockWorkstation()); // Letter fails after Meta was already pressed.
  assert(key_events.size() == 2 && key_events.back().code == 2076 && key_events.back().action == KEY_ACTION_UP);
  assert(pressed_keys.empty());
  return 0;
}
