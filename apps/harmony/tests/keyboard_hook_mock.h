/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _KEYBOARD_HOOK_MOCK_H_
#define _KEYBOARD_HOOK_MOCK_H_

#include <multimodalinput/oh_input_manager.h>
#include <vector>
struct Input_KeyEvent { int code = 0, action = 0, id = 0; };
inline Input_KeyEventCallback hook_callback = nullptr;
inline Input_Result hook_add_result = INPUT_SUCCESS, hook_remove_result = INPUT_SUCCESS;
inline int hook_adds = 0, hook_removes = 0, focus_during_add = 0;
inline std::vector<int> forwarded_events;
extern "C" {
Input_Result OH_Input_AddKeyEventHook(Input_KeyEventCallback cb) {
  ++hook_adds;
  if (focus_during_add) crossdesk::harmony::desktop::SetKeyboardFocus(focus_during_add, true);
  if (hook_add_result == INPUT_SUCCESS) hook_callback = cb;
  return hook_add_result;
}
Input_Result OH_Input_RemoveKeyEventHook(Input_KeyEventCallback) {
  ++hook_removes;
  if (hook_remove_result == INPUT_SUCCESS) hook_callback = nullptr;
  return hook_remove_result;
}
int32_t OH_Input_GetKeyEventAction(const Input_KeyEvent* event) { return event->action; }
int32_t OH_Input_GetKeyEventKeyCode(const Input_KeyEvent* event) { return event->code; }
Input_Result OH_Input_GetKeyEventId(const Input_KeyEvent* event, int32_t* id) { *id = event->id; return INPUT_SUCCESS; }
Input_Result OH_Input_DispatchToNextHandler(int32_t id) { forwarded_events.push_back(id); return INPUT_SUCCESS; }
}

#endif
