/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-22
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _REMOTE_MOUSE_BUTTONS_H_
#define _REMOTE_MOUSE_BUTTONS_H_

#include <remote_action.h>

#include <array>
#include <mutex>
#include <string>

#include "input_outcome.h"

namespace crossdesk {

// One physical button cannot belong to two controllers at once. A second
// controller's down/up must not steal or release an existing drag.
class RemoteMouseButtons {
 public:
  template <typename Send>
  InputOutcome Apply(const std::string& id, MouseFlag flag, Send send) {
    if (id.empty()) return InputOutcome::ignored;
    std::lock_guard lock(mutex_);
    const int value = static_cast<int>(flag);
    if (value < 1 || value > 6) return send();
    auto& owner = owners_[(value - 1) / 2];
    const bool down = value % 2 == 1;
    if ((!owner.empty() && owner != id) || (!down && owner.empty()))
      return InputOutcome::ignored;
    // A service timeout may follow successful injection. Retain an attempted
    // down until an up or explicit cleanup is confirmed, including exceptions.
    if (down) owner = id;
    const auto outcome = send();
    if (!down && outcome == InputOutcome::applied) owner.clear();
    return outcome;
  }

  template <typename Release>
  bool ReleaseController(const std::string& id, Release release,
                         unsigned requested_mask = 7) {
    if (id.empty()) return true;
    std::lock_guard lock(mutex_);
    unsigned mask = 0;
    for (size_t i = 0; i < owners_.size(); ++i)
      if (owners_[i] == id && (requested_mask & (1u << i))) mask |= 1u << i;
    return ReleaseLocked(mask, release);
  }

  template <typename Release>
  bool ReleaseAll(Release release) {
    std::lock_guard lock(mutex_);
    // Also clear any backend state retained after a desktop/route transition.
    return ReleaseLocked(7, release);
  }

 private:
  template <typename Release>
  bool ReleaseLocked(unsigned mask, Release release) {
    if (!mask) return true;
    // The sink returns the subset confirmed released, so one failed button
    // does not retain ownership of other successfully released buttons.
    const unsigned released = release(mask) & mask;
    for (size_t i = 0; i < owners_.size(); ++i)
      if (released & (1u << i)) owners_[i].clear();
    return released == mask;
  }

  std::mutex mutex_;
  std::array<std::string, 3> owners_;
};

}  // namespace crossdesk

#endif