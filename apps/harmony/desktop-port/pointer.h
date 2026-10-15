/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _POINTER_H_
#define _POINTER_H_

#include <array>
#include <utility>

namespace crossdesk::harmony::desktop {
// Preserve each button's last position. Cancellation/focus loss must release
// every held button, while merely leaving the surface must preserve a drag.
class PointerState {
  std::array<bool, 3> pressed_{};
  float x_ = 0, y_ = 0;
 public:
  void Update(int kind, int button, float x, float y) {
    x_ = x; y_ = y;
    if (button >= 0 && button < 3 && (kind == 0 || kind == 1)) pressed_[button] = kind == 0;
  }
  unsigned Buttons() const {
    return (pressed_[0] ? 1u : 0u) | (pressed_[1] ? 4u : 0u) | (pressed_[2] ? 2u : 0u);
  }
  template<class Release> void Cancel(Release release) {
    const auto pressed = std::exchange(pressed_, {});
    for (int i = 0; i < 3; ++i) if (pressed[i]) release(i, x_, y_);
  }
};
}

#endif
