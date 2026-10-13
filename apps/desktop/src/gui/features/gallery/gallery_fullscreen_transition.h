/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-12
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _GALLERY_FULLSCREEN_TRANSITION_H_
#define _GALLERY_FULLSCREEN_TRANSITION_H_

#include <chrono>
#include <cstdint>

namespace crossdesk::gallery {

// Guard input while native geometry settles, without hiding the live content.
// Driven by the existing UI tick; never pumps events or creates nested timers.
class FullscreenTransition {
public:
  using Clock = std::chrono::steady_clock;
  enum class Action { Idle, Apply, Finished };

  bool Begin(bool fullscreen, Clock::time_point now) {
    if (Active())
      return false;
    target_ = fullscreen;
    phase_ = Phase::Pending;
    phase_at_ = now;
    width_ = height_ = 0;
    scale_ = 0;
    geometry_changed_ = false;
    return true;
  }

  Action Advance(Clock::time_point now, uint32_t width, uint32_t height,
                 float scale, bool fullscreen, bool restoring) {
    using namespace std::chrono_literals;
    if (phase_ == Phase::Pending) {
      if (restoring && now - phase_at_ >= 1500ms) {
        Cancel();
        return Action::Finished;
      }
      // Apply on the next UI tick as soon as window restoration is complete.
      if (restoring)
        return Action::Idle;
      phase_ = Phase::Settling;
      phase_at_ = stable_at_ = now;
      width_ = width;
      height_ = height;
      scale_ = scale;
      return Action::Apply;
    }
    if (phase_ == Phase::Settling) {
      const bool changed =
          width != width_ || height != height_ || scale != scale_;
      if (changed && width > 0 && height > 0)
        geometry_changed_ = true;
      if (changed || fullscreen != target_ || restoring) {
        width_ = width;
        height_ = height;
        scale_ = scale;
        stable_at_ = now;
      }
      // Some backends report the requested flag before delivering new bounds.
      // Also allow fullscreen modes whose client dimensions genuinely match.
      if ((width > 0 && height > 0 && fullscreen == target_ && !restoring &&
           (geometry_changed_ || now - phase_at_ >= 300ms) &&
           now - stable_at_ >= 32ms) ||
          now - phase_at_ >= 1500ms) {
        Cancel();
        return Action::Finished;
      }
    }
    return Action::Idle;
  }

  void Cancel() { phase_ = Phase::Idle; }
  bool Active() const { return phase_ != Phase::Idle; }
  bool Target() const { return target_; }

private:
  enum class Phase { Idle, Pending, Settling };
  Phase phase_ = Phase::Idle;
  bool target_ = false;
  bool geometry_changed_ = false;
  uint32_t width_ = 0, height_ = 0;
  float scale_ = 0;
  Clock::time_point phase_at_{}, stable_at_{};
};

} // namespace crossdesk::gallery

#endif
