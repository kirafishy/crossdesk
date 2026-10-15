#include "input.h"
#include "key_mapping.h"
#include <multimodalinput/oh_input_manager.h>
#include <algorithm>
#include <cmath>
#include <hilog/log.h>

namespace crossdesk::harmony {
namespace {
void ReportResult(const char* kind, int result, int& previous) {
  if (result == previous) return;
  previous = result;
  OH_LOG_Print(LOG_APP, result == INPUT_SUCCESS ? LOG_INFO : LOG_WARN, 0, "SlintDesktop",
      "Host %{public}s injection result: %{public}d", kind, result);
}
KeyboardKey NormalizeKey(int key, uint32_t scan, bool extended) {
  const int native = FromVirtualKey(key, scan, extended);
  return {native ? ToVirtualKey(native) : 0, scan, extended};
}
}
bool Input::Configure(int width, int height, int display) {
  if (width <= 0 || height <= 0 || display < 0 || !Release()) return false;
  width_ = width; height_ = height; display_ = display;
  return true;
}
bool Input::Key(const KeyboardKey& key, bool down) {
  const int code = FromVirtualKey(key.key_code, key.scan_code, key.extended);
  if (!code) return false;
  auto* event = OH_Input_CreateKeyEvent();
  if (!event) return false;
  OH_Input_SetKeyEventKeyCode(event, code);
  OH_Input_SetKeyEventAction(event, down ? KEY_ACTION_DOWN : KEY_ACTION_UP);
  const int result = OH_Input_InjectKeyEvent(event);
  ReportResult("keyboard", result, last_key_result_);
  const bool ok = result == INPUT_SUCCESS;
  OH_Input_DestroyKeyEvent(&event);
  if (ok) { if (down) keys_[key.key_code] = key; else keys_.erase(key.key_code); }
  return ok;
}
bool Input::Pointer(int flag, float x, float y, int scroll) {
  if (flag < move || flag > wheel_horizontal || width_ <= 0 || height_ <= 0 ||
      !std::isfinite(x) || !std::isfinite(y)) return false;
  auto* event = OH_Input_CreateMouseEvent();
  if (!event) return false;
  x_ = std::clamp(x, 0.f, 1.f); y_ = std::clamp(y, 0.f, 1.f);
  OH_Input_SetMouseEventDisplayId(event, display_);
  OH_Input_SetMouseEventDisplayX(event, std::lround(x_ * (width_ - 1)));
  OH_Input_SetMouseEventDisplayY(event, std::lround(y_ * (height_ - 1)));
  const int buttons[] = {MOUSE_BUTTON_LEFT, MOUSE_BUTTON_RIGHT, MOUSE_BUTTON_MIDDLE};
  int button = MOUSE_BUTTON_NONE;
  // WMS rejects drag moves with BUTTON_NONE, even if a preceding down event
  // populated the SDK's pressed-button set. Carry the held button on moves.
  for (int i = 0; i < 3; ++i) if (buttons_ & (1u << i)) { button = buttons[i]; break; }
  OH_Input_SetMouseEventButton(event, button);
  bool ok = true;
  const auto inject = [&] {
    const int result = OH_Input_InjectMouseEvent(event);
    ReportResult("mouse", result, last_mouse_result_);
    return result == INPUT_SUCCESS;
  };
  if (flag >= wheel_vertical) {
    OH_Input_SetMouseEventAxisType(event, flag == wheel_vertical ? MOUSE_AXIS_SCROLL_VERTICAL : MOUSE_AXIS_SCROLL_HORIZONTAL);
    for (int action : {MOUSE_ACTION_AXIS_BEGIN, MOUSE_ACTION_AXIS_UPDATE, MOUSE_ACTION_AXIS_END}) {
      // Wire values are wheel steps. Only UPDATE carries a delta; repeating
      // it on all three phases scrolls three times for one remote event.
      OH_Input_SetMouseEventAxisValue(event, action == MOUSE_ACTION_AXIS_UPDATE ? std::clamp(scroll, -1200, 1200) : 0);
      OH_Input_SetMouseEventAction(event, action);
      ok = inject() && ok;
    }
  } else {
    OH_Input_SetMouseEventAction(event, flag == move ? MOUSE_ACTION_MOVE :
        (flag & 1) ? MOUSE_ACTION_BUTTON_DOWN : MOUSE_ACTION_BUTTON_UP);
    if (flag != move) {
      const int index = (flag - 1) / 2;
      OH_Input_SetMouseEventButton(event, buttons[index]);
    }
    ok = inject();
    if (ok && flag != move) {
      const unsigned bit = 1u << ((flag - 1) / 2);
      if (flag & 1) buttons_ |= bit; else buttons_ &= ~bit;
    }
  }
  OH_Input_DestroyMouseEvent(&event);
  return ok;
}
bool Input::Apply(const RemoteAction& action) {
  if (action.type == mouse) return Pointer(action.m.flag, action.m.x, action.m.y, action.m.s);
  if (action.type == keyboard) {
    if (action.k.flag != key_down && action.k.flag != key_up) return false;
    return Key(NormalizeKey(static_cast<int>(action.k.key_value), action.k.scan_code, action.k.extended), action.k.flag == key_down);
  }
  if (action.type == keyboard_state) {
    if (action.ks.pressed_count > kMaxKeyboardStateKeys) return false;
    if (action.ks.seq && have_key_sequence_ && static_cast<int32_t>(action.ks.seq - key_sequence_) <= 0) return true;
    key_sequence_ = action.ks.seq; have_key_sequence_ = true;
    PressedKeys next;
    for (size_t i = 0; i < action.ks.pressed_count; ++i) {
      const auto& wire = action.ks.pressed_keys[i];
      const auto key = NormalizeKey(wire.key_value, wire.scan_code, wire.extended);
      if (key.key_code) next[key.key_code] = key;
    }
    bool ok = true;
    for (const auto& change : ReconcileKeyboardState(keys_, next)) {
      if (change.is_down && !ok) continue;
      ok = Key({change.key_code, change.scan_code, change.extended}, change.is_down) && ok;
    }
    return ok;
  }
  return false;
}
bool Input::Release() {
  for (const auto& change : ReconcileKeyboardState(keys_, {}))
    Key({change.key_code, change.scan_code, change.extended}, false);
  for (int button = 0; button < 3; ++button)
    if (buttons_ & (1u << button)) Pointer(2 + button * 2, x_, y_, 0);
  // Failed releases remain pending so a later cleanup can retry them.
  have_key_sequence_ = false;
  return keys_.empty() && buttons_ == 0;
}
bool Input::LockWorkstation() {
  // The public Harmony shortcut is Meta+L. Never carry a prior remote drag or
  // modifier into it, and retain failed releases for the normal cleanup path.
  if (!Release()) return false;
  const KeyboardKey meta{0x5B, 0x5B, true}, letter{0x4C, 0, false};
  bool ok = Key(meta, true);
  if (ok) ok = Key(letter, true);
  if (keys_.count(letter.key_code)) ok = Key(letter, false) && ok;
  if (keys_.count(meta.key_code)) ok = Key(meta, false) && ok;
  return ok;
}
}
