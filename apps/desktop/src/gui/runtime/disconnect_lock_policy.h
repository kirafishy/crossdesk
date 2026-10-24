/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-25
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _DISCONNECT_LOCK_POLICY_H_
#define _DISCONNECT_LOCK_POLICY_H_

#include <string>
#include <unordered_set>
#include <utility>

namespace crossdesk {

// Guarded by GuiRuntime's connection_status_mutex_. Only incoming sessions
// that reached Connected can request a lock; terminal callbacks may repeat.
class DisconnectLockPolicy {
 public:
  void Connected(const std::string& remote_id) {
    controllers_.insert(remote_id);
    pending_ = false;
  }

  void Disconnected(const std::string& remote_id, bool enabled) {
    if (controllers_.erase(remote_id) && controllers_.empty() && enabled)
      pending_ = true;
  }

  bool TakeRequest(bool enabled) {
    return std::exchange(pending_, false) && enabled && controllers_.empty();
  }

 private:
  std::unordered_set<std::string> controllers_;
  bool pending_ = false;
};

}  // namespace crossdesk

#endif
