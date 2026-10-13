/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-12
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _RECEIVED_VIDEO_STATS_H_
#define _RECEIVED_VIDEO_STATS_H_

#include <chrono>
#include <cmath>
#include <mutex>
#include <optional>

namespace crossdesk::gui_detail {
// Measured at decode delivery, including wall-only and background sessions.
// Independent of the selected window's presentation cadence and FPS setting.
class ReceivedVideoStats {
 public:
  using Clock = std::chrono::steady_clock;
  struct Snapshot {
    int width = 0;
    int height = 0;
    std::optional<int> fps;
  };

  void Record(int width, int height, Clock::time_point now = Clock::now()) {
    if (width <= 0 || height <= 0) return;
    std::lock_guard lock(mutex_);
    const bool restart = snapshot_.width == 0 ||
                         now - last_frame_ >= std::chrono::seconds(3);
    snapshot_.width = width;
    snapshot_.height = height;
    last_frame_ = now;
    if (restart) {
      started_ = now;
      frames_ = 0;
      snapshot_.fps.reset();
      return;
    }
    ++frames_;
    const double elapsed = std::chrono::duration<double>(now - started_).count();
    if (elapsed >= 1.0) {
      snapshot_.fps = static_cast<int>(std::lround(frames_ / elapsed));
      frames_ = 0;
      started_ = now;
    }
  }

  Snapshot Get(Clock::time_point now = Clock::now()) const {
    std::lock_guard lock(mutex_);
    auto result = snapshot_;
    if (result.width > 0 && now - last_frame_ >= std::chrono::seconds(1))
      result.fps = 0;
    return result;
  }

  void Reset() {
    std::lock_guard lock(mutex_);
    snapshot_ = {};
    frames_ = 0;
    started_ = last_frame_ = {};
  }

 private:
  mutable std::mutex mutex_;
  Snapshot snapshot_;
  unsigned frames_ = 0;
  Clock::time_point started_{}, last_frame_{};
};
}  // namespace crossdesk::gui_detail
#endif
