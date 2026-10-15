/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _WINDOW_MODE_H_
#define _WINDOW_MODE_H_

#include <nlohmann/json.hpp>

namespace crossdesk::harmony::desktop {
// Slint setters synchronously notify the platform one property at a time.
// A native state snapshot must be acknowledged atomically without echoing
// those intermediate combinations back to the native compositor.
class WindowMode {
  nlohmann::json known_ = nlohmann::json::object();
  bool acknowledging_ = false;
 public:
  nlohmann::json Request(const nlohmann::json& desired) {
    auto changes = nlohmann::json::object();
    if (acknowledging_) return changes;
    for (const auto& [name, value] : desired.items())
      if (!known_.contains(name) || known_.at(name) != value) changes[name] = value;
    known_ = desired;
    return changes;
  }
  template <typename Apply>
  void Acknowledge(const nlohmann::json& actual, Apply apply) {
    known_.update(actual);
    struct Reset { bool& flag; ~Reset() { flag = false; } } reset{acknowledging_};
    acknowledging_ = true;
    apply();
  }
};
}

#endif
