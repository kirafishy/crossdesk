/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-22
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _PENDING_KEY_RELEASES_H_
#define _PENDING_KEY_RELEASES_H_

#include <string>
#include <unordered_map>
#include <vector>

#include "keyboard_state.h"

namespace crossdesk {

// Caller serializes access with KeyboardController's remote-input mutex.
// Pending releases outlive the peer's active key/heartbeat state, but retain
// their controller identity so cleanup cannot report success prematurely.
class PendingKeyReleases {
 public:
  void Queue(const std::string& id, const PressedKeys& keys) {
    if (keys.empty()) return;
    auto& pending = peers_[id];
    for (const auto& [code, key] : keys) pending[code] = key;
  }

  bool Empty() const { return peers_.empty(); }
  bool Contains(const std::string& id) const { return peers_.count(id) != 0; }

  std::vector<std::string> ControllerIds() const {
    std::vector<std::string> ids;
    ids.reserve(peers_.size());
    for (const auto& [id, _] : peers_) ids.push_back(id);
    return ids;
  }

  // A confirmed new event supersedes an older pending release of that key,
  // including one belonging to a disconnected controller.
  void Supersede(int key_code) {
    for (auto it = peers_.begin(); it != peers_.end();) {
      it->second.erase(key_code);
      if (it->second.empty())
        it = peers_.erase(it);
      else
        ++it;
    }
  }

  template <typename SendUp>
  bool ReleaseController(const std::string& id, SendUp send_up) {
    auto peer = peers_.find(id);
    if (peer == peers_.end()) return true;
    auto& keys = peer->second;
    for (auto it = keys.begin(); it != keys.end();) {
      if (send_up(it->second))
        it = keys.erase(it);
      else
        ++it;
    }
    if (!keys.empty()) return false;
    peers_.erase(peer);
    return true;
  }

  template <typename SendUp>
  void RetryAll(SendUp send_up) {
    // One bounded pass, even when every native release fails.
    for (const auto& id : ControllerIds()) ReleaseController(id, send_up);
  }

 private:
  std::unordered_map<std::string, PressedKeys> peers_;
};

}  // namespace crossdesk

#endif