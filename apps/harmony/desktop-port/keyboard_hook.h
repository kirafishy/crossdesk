/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _KEYBOARD_HOOK_H_
#define _KEYBOARD_HOOK_H_

#include "device_controller.h"
#include <multimodalinput/oh_input_manager.h>

namespace crossdesk::harmony::desktop {
// Process-wide OS hook; the shared SessionDeviceManager owns when it runs.
// ArkUI updates focus immediately, even while the Slint thread waits for an OS
// service. A late callback must never capture input for another application.
void SetKeyboardFocus(int window, bool active);
void SetKeyboardHookPermission(bool granted);
bool KeyboardHookPermissionGranted();
class KeyboardHook {
 public:
  ~KeyboardHook();
  int Start(OnKeyAction callback, void* context);
  int Stop();
  bool Active() const;
 private:
  static void Receive(const Input_KeyEvent* event);
};
}

#endif
