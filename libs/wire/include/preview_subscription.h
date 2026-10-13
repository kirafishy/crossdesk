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
      0; // 0 pauses; 1/8 legacy previews; 15 negotiated preview; 60 interactive.
  int quality = 0;
  int frame_rate = 30;
  int preference = 0;
  bool audio = false;
  // v2 bounds are physical pixels. 0/0 restores the legacy source limit.
  int width = 0;
  int height = 0;
  int version = 1;
  // Optional negotiated extension: total video budget for this peer, in bps.
  // 0 restores normal congestion control; absent preserves legacy wire format.
  std::optional<int> max_bitrate_bps;

  static bool ValidResolution(int width, int height) {
    return (width == 0 && height == 0) ||
           (width >= 2 && height >= 2 && width <= 16384 && height <= 16384);
  }

  static PreviewSubscription Interactive(int quality, int frame_rate,
                                         int preference, bool audio = false) {
    return {0, 60, quality, frame_rate, preference, audio};
  }

  bool IsNewerThan(const PreviewSubscription &other) const {
    return static_cast<int32_t>(request_id - other.request_id) > 0;
  }

  bool SameSettings(const PreviewSubscription &other) const {
    return fps == other.fps && quality == other.quality &&
           frame_rate == other.frame_rate &&
           EncoderPreference() == other.EncoderPreference() &&
           audio == other.audio && width == other.width &&
           height == other.height && version == other.version &&
           max_bitrate_bps == other.max_bitrate_bps;
  }

  int EncoderPreference() const {
    // Sparse preview input is intentional. Preserve spatial detail instead of
    // applying the interactive frame-rate/keyframe latency budget to it.
    return fps == 1 || fps == 8 || fps == 15 ? 1 : preference;
  }

  nlohmann::json ToJson() const {
    nlohmann::json json = {{"type", "deskhub_preview"}, {"version", version},
            {"request_id", request_id},  {"fps", fps},
            {"quality", quality},        {"frame_rate", frame_rate},
            {"preference", EncoderPreference()}, {"audio", audio}};
    if (version == 2) {
      json["width"] = width;
      json["height"] = height;
    }
    if (max_bitrate_bps) json["max_bitrate_bps"] = *max_bitrate_bps;
    return json;
  }
  static std::optional<PreviewSubscription> Parse(const nlohmann::json &json) {
    try {
      if (json.at("type") != "deskhub_preview" ||
          !json.at("version").is_number_integer() ||
          (json.at("version") != 1 && json.at("version") != 2))
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
           result.fps != 15 && result.fps != 60) ||
          result.quality < 0 || result.quality > 2 ||
          (result.frame_rate != 30 && result.frame_rate != 60) ||
          result.preference < 0 || result.preference > 2)
        return {};
      if (json.contains("audio")) {
        if (!json.at("audio").is_boolean())
          return {};
        result.audio = json.at("audio").get<bool>();
      }
      result.version = json.at("version").get<int>();
      if (result.version == 2) {
        for (const char *key : {"width", "height"}) {
          if (!json.at(key).is_number_integer()) return {};
          const auto value = json.at(key).get<int64_t>();
          if (value < 0 || value > 16384) return {};
        }
        result.width = json.at("width").get<int>();
        result.height = json.at("height").get<int>();
        if (!ValidResolution(result.width, result.height)) return {};
      }
      if (json.contains("max_bitrate_bps")) {
        if (!json.at("max_bitrate_bps").is_number_integer()) return {};
        const auto bitrate = json.at("max_bitrate_bps").get<int64_t>();
        if (bitrate < 0 || bitrate > 100'000'000) return {};
        result.max_bitrate_bps = int(bitrate);
      }
      return result;
    } catch (...) {
      return {};
    }
  }
};
} // namespace crossdesk

#endif
