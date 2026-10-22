/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-14
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _INPUT_H_
#define _INPUT_H_

#include <remote_action.h>
#include <keyboard_state.h>

namespace crossdesk::harmony {
class Input {
 public:
  bool Configure(int width, int height, int display);
  bool Apply(const RemoteAction& action);
  bool Release();
  unsigned ReleaseMouseButtons(unsigned mask);
  bool LockWorkstation();
 private:
  bool Key(const KeyboardKey& key, bool down);
  bool Pointer(int flag, float x, float y, int scroll);
  int width_ = 0, height_ = 0, display_ = 0;
  float x_ = 0.5f, y_ = 0.5f;
  PressedKeys keys_;
  int last_key_result_ = -1, last_mouse_result_ = -1;
  unsigned buttons_ = 0;
  uint32_t key_sequence_ = 0;
  bool have_key_sequence_ = false;
};
}

#endif
