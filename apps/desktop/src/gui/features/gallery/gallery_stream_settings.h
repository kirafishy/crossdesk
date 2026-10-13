/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-12
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _GALLERY_STREAM_SETTINGS_H_
#define _GALLERY_STREAM_SETTINGS_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <preview_subscription.h>

#include "preview_resolution.h"

namespace crossdesk::gallery {
struct StreamSettings {
  // Persisted preset order: automatic, fixed bounds, then original size.
  static constexpr std::array<PreviewResolution, 8> kResolutions{{
      {}, {320, 180}, {640, 360}, {1280, 720}, {1920, 1080},
      {2560, 1440}, {3840, 2160}, {}}};
  static constexpr std::array<int, 4> kFrameRates{0, 15, 30, 60};
  int resolution = 0;
  int frame_rate = 0;  // 0 follows the session's existing 30/60 FPS preference.

  static std::optional<StreamSettings> FromIndices(int resolution, int fps) {
    if (resolution < 0 || resolution >= int(kResolutions.size()) ||
        fps < 0 || fps >= int(kFrameRates.size()))
      return {};
    return StreamSettings{resolution, kFrameRates[fps]};
  }
  int FrameRateIndex() const {
    return int(std::find(kFrameRates.begin(), kFrameRates.end(), frame_rate) -
               kFrameRates.begin());
  }
  bool Valid() const {
    return resolution >= 0 && resolution < int(kResolutions.size()) &&
           FrameRateIndex() < int(kFrameRates.size());
  }
  static StreamSettings FromJson(const nlohmann::json& json) {
    try {
      if (!json.at("resolution").is_number_integer() ||
          !json.at("frame_rate").is_number_integer()) return {};
      const auto resolution = json.at("resolution").get<int64_t>();
      const auto fps = json.at("frame_rate").get<int64_t>();
      if (resolution < 0 || resolution >= int(kResolutions.size()) ||
          fps < 0 || fps > 60) return {};
      // Retired sparse presets migrate to the lowest current setting.
      StreamSettings result{int(resolution), fps == 1 || fps == 8 ? 15 : int(fps)};
      return result.Valid() ? result : StreamSettings{};
    } catch (...) {
      return {};
    }
  }
  nlohmann::json ToJson() const {
    return {{"resolution", resolution}, {"frame_rate", frame_rate}};
  }
  PreviewResolution Resolution(bool wall_managed, PreviewResolution wall_view,
                               PreviewResolution stream_view) const {
    if (!wall_managed) return {};
    if (resolution != 0 && Valid()) return kResolutions[resolution];
    // A square bound limits the longest edge of an unexpanded wall tile.
    if (!wall_view.width && !stream_view.width) return {320, 320};
    return {std::max(wall_view.width, stream_view.width),
            std::max(wall_view.height, stream_view.height)};
  }
  void ApplyFrameRate(PreviewSubscription& demand, bool supports_fps15) const {
    if (!Valid() || !frame_rate) return;
    // Older peers reject fps=15. Keep their existing interactive wire format.
    demand.fps = frame_rate == 15 && supports_fps15 ? 15 : 60;
    demand.frame_rate = frame_rate <= 30 ? 30 : 60;
  }
};
}  // namespace crossdesk::gallery
#endif
