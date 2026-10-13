/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-12
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _PREVIEW_RESOLUTION_H_
#define _PREVIEW_RESOLUTION_H_

#include <algorithm>
#include <chrono>
#include <cmath>

namespace crossdesk::gallery {
struct PreviewResolution {
  int width = 0;
  int height = 0;

  bool operator==(const PreviewResolution& other) const {
    return width == other.width && height == other.height;
  }
  static PreviewResolution Viewport(float width, float height, float scale) {
    if (!std::isfinite(width) || !std::isfinite(height) ||
        !std::isfinite(scale) || width <= 0 || height <= 0 || scale <= 0)
      return {};
    return {int(std::clamp(std::ceil(double(width) * scale), 2.0, 16384.0)),
            int(std::clamp(std::ceil(double(height) * scale), 2.0, 16384.0))};
  }
};

class PreviewResolutionDebouncer {
 public:
  using Clock = std::chrono::steady_clock;
  PreviewResolution Update(PreviewResolution desired, Clock::time_point now,
                           bool reset = false) {
    if (!initialized_ || reset) {
      initialized_ = true;
      applied_ = candidate_ = desired;
      changed_at_ = now;
    } else if (!(candidate_ == desired)) {
      candidate_ = desired;
      changed_at_ = now;
    } else if (now - changed_at_ >= std::chrono::milliseconds(200)) {
      applied_ = candidate_;
    }
    return applied_;
  }

 private:
  bool initialized_ = false;
  PreviewResolution applied_, candidate_;
  Clock::time_point changed_at_{};
};
}  // namespace crossdesk::gallery
#endif
