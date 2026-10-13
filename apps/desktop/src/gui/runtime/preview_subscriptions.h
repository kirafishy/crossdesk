/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-12
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _PREVIEW_SUBSCRIPTIONS_H_
#define _PREVIEW_SUBSCRIPTIONS_H_

#include <preview_subscription.h>

#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>

namespace crossdesk {
// Accessed by the network callback and screen-capture callback. Legacy
// controllers have no entry and continue receiving their normal stream.
class PreviewSubscriptions {
public:
  using Clock = std::chrono::steady_clock;
  bool IsNewer(const std::string &id, uint32_t request_id) {
    std::lock_guard lock(mutex_);
    const auto found = states_.find(id);
    return found == states_.end() ||
           static_cast<int32_t>(request_id -
                                found->second.request.request_id) > 0;
  }
  bool Set(const std::string &id, const PreviewSubscription &request) {
    std::lock_guard lock(mutex_);
    const auto found = states_.find(id);
    if (found != states_.end() &&
        static_cast<int32_t>(request.request_id -
                             found->second.request.request_id) <= 0)
      return false;
    auto &state = states_[id];
    state.request = request;
    state.next.clear();
    return true;
  }
  bool ShouldSend(const std::string &id, Clock::time_point now = Clock::now(),
                  const std::string &stream = {}) {
    std::lock_guard lock(mutex_);
    const auto found = states_.find(id);
    if (found == states_.end())
      return true;
    auto &state = found->second;
    if (!state.request.fps)
      return false;
    // Interactive streams use the encoder's own 30/60 FPS cadence. A second
    // time gate here can drop valid frames because capture timing varies.
    if (state.request.fps == 60)
      return true;
    auto &next = state.next[stream];
    if (state.request.fps == 15) {
      // Keep a stable 15 FPS deadline across capture jitter. Rebasing every
      // frame can turn small timing errors into extra dropped capture frames.
      const auto interval = std::chrono::nanoseconds(1'000'000'000 / 15);
      if (now + std::chrono::milliseconds(1) < next) return false;
      next = next == Clock::time_point{} || now - next > interval
          ? now + interval : next + interval;
      return true;
    }
    if (now < next)
      return false;
    next = now + std::chrono::milliseconds(1000 / state.request.fps);
    return true;
  }
  bool ShouldSendAudio(const std::string &id) {
    std::lock_guard lock(mutex_);
    const auto found = states_.find(id);
    return found == states_.end() || found->second.request.audio;
  }
  void Remove(const std::string &id) {
    std::lock_guard lock(mutex_);
    states_.erase(id);
  }

private:
  struct State {
    PreviewSubscription request;
    // One display must not consume another display's preview frame budget.
    std::unordered_map<std::string, Clock::time_point> next;
  };
  std::mutex mutex_;
  std::unordered_map<std::string, State> states_;
};
} // namespace crossdesk

#endif
