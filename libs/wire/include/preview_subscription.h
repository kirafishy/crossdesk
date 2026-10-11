/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-12
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _PREVIEW_SUBSCRIPTION_H_
#define _PREVIEW_SUBSCRIPTION_H_

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>

namespace crossdesk {
// Versioned independently from the interactive 30/60 FPS video settings.
struct PreviewSubscription {
  uint32_t request_id = 0;
  int fps =
      0; // 0 pauses encoding/delivery, 1 overview, 8 focus, 60 interactive.
  int quality = 0;
  int frame_rate = 30;
  int preference = 0;
  bool audio = false;

  static PreviewSubscription Interactive(int quality, int frame_rate,
                                         int preference, bool audio = false) {
    return {0, 60, quality, frame_rate, preference, audio};
  }

  bool SameSettings(const PreviewSubscription &other) const {
    return fps == other.fps && quality == other.quality &&
           frame_rate == other.frame_rate &&
           EncoderPreference() == other.EncoderPreference() &&
           audio == other.audio;
  }

  int EncoderPreference() const {
    // Sparse preview input is intentional. Preserve spatial detail instead of
    // applying the interactive frame-rate/keyframe latency budget to it.
    return fps == 1 || fps == 8 ? 1 : preference;
  }

  nlohmann::json ToJson() const {
    return {{"type", "deskhub_preview"}, {"version", 1},
            {"request_id", request_id},  {"fps", fps},
            {"quality", quality},        {"frame_rate", frame_rate},
            {"preference", EncoderPreference()}, {"audio", audio}};
  }
  static std::optional<PreviewSubscription> Parse(const nlohmann::json &json) {
    try {
      if (json.at("type") != "deskhub_preview" || json.at("version") != 1)
        return {};
      for (const char *key :
           {"request_id", "fps", "quality", "frame_rate", "preference"})
        if (!json.at(key).is_number_integer())
          return {};
      const auto id = json.at("request_id").get<int64_t>();
      if (id <= 0 || id > UINT32_MAX)
        return {};
      for (const char *key : {"fps", "quality", "frame_rate", "preference"}) {
        const auto value = json.at(key).get<int64_t>();
        if (value < 0 || value > 60)
          return {};
      }
      PreviewSubscription result{static_cast<uint32_t>(id), json.at("fps"),
                                 json.at("quality"), json.at("frame_rate"),
                                 json.at("preference")};
      if ((result.fps != 0 && result.fps != 1 && result.fps != 8 &&
           result.fps != 60) ||
          result.quality < 0 || result.quality > 2 ||
          (result.frame_rate != 30 && result.frame_rate != 60) ||
          result.preference < 0 || result.preference > 2)
        return {};
      if (json.contains("audio")) {
        if (!json.at("audio").is_boolean())
          return {};
        result.audio = json.at("audio").get<bool>();
      }
      return result;
    } catch (...) {
      return {};
    }
  }
};
} // namespace crossdesk

#endif
