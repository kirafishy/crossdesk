/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-13
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _PREVIEW_BANDWIDTH_H_
#define _PREVIEW_BANDWIDTH_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace crossdesk::gallery {
struct PreviewBandwidthDemand {
  int desired_bps = 0;
  bool foreground = false;

  bool operator==(const PreviewBandwidthDemand& other) const {
    return desired_bps == other.desired_bps && foreground == other.foreground;
  }

  static int Estimate(int width, int height, int fps) {
    // A demand ceiling, not a throughput measurement. Include room for video
    // repair traffic; the sender's congestion control may allocate less.
    const int64_t pixels = width > 0 && height > 0
        ? int64_t(width) * height : 3840LL * 2160;
    const int64_t estimate = pixels * std::clamp(fps, 1, 60) * 15 / 100;
    return int(std::clamp<int64_t>((estimate + 49'999) / 50'000 * 50'000,
                                   150'000, 20'000'000));
  }
};

inline std::vector<int> AllocatePreviewBandwidth(
    const std::vector<PreviewBandwidthDemand>& demands, int budget_bps) {
  std::vector<int> result(demands.size(), 0);
  int remaining = std::max(0, budget_bps);
  int active = 0;
  int64_t desired_total = 0;
  for (const auto& demand : demands) {
    active += demand.desired_bps > 0;
    desired_total += std::max(0, demand.desired_bps);
  }
  if (!active || !remaining) return result;
  if (desired_total <= remaining) {
    for (std::size_t i = 0; i < demands.size(); ++i)
      result[i] = std::max(0, demands[i].desired_bps);
    return result;
  }
  // First preserve a small fair share for every visible device, then perform
  // weighted max-min allocation. Satisfied devices return unused capacity.
  const int baseline = std::min(100'000, remaining / active);
  for (std::size_t i = 0; i < demands.size(); ++i) {
    result[i] = std::min(std::max(0, demands[i].desired_bps), baseline);
    remaining -= result[i];
  }
  while (remaining > 0) {
    int weights = 0;
    for (std::size_t i = 0; i < demands.size(); ++i)
      if (result[i] < demands[i].desired_bps)
        weights += demands[i].foreground ? 4 : 1;
    if (!weights) break;
    const int available = remaining;
    for (std::size_t i = 0; i < demands.size() && remaining > 0; ++i) {
      if (result[i] >= demands[i].desired_bps) continue;
      const int share = int(int64_t(available) *
                            (demands[i].foreground ? 4 : 1) / weights);
      const int extra = std::min({remaining, demands[i].desired_bps - result[i],
                                  std::max(1, share)});
      result[i] += extra;
      remaining -= extra;
    }
  }
  return result;
}

// UI refreshes much more often than video demand changes. Keep the previous
// result while membership, dimensions, cadence, priority and budget are stable.
class PreviewBandwidthAllocator {
 public:
  const std::vector<int>& Allocate(
      const std::vector<PreviewBandwidthDemand>& demands, int budget_bps) {
    if (budget_bps_ != budget_bps || demands_ != demands) {
      demands_ = demands;
      budget_bps_ = budget_bps;
      allocations_ = AllocatePreviewBandwidth(demands, budget_bps);
    }
    return allocations_;
  }

 private:
  int budget_bps_ = -1;
  std::vector<PreviewBandwidthDemand> demands_;
  std::vector<int> allocations_;
};
}  // namespace crossdesk::gallery

#endif
