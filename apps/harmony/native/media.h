/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-14
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _MEDIA_H_
#define _MEDIA_H_

#include <minirtc.h>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace crossdesk::harmony {
class Renderer {
 public:
  using Presented = std::function<void(const uint8_t*, int, int, uint64_t, uint64_t, uint64_t)>;
  explicit Renderer(Presented presented = {});
  ~Renderer();
  void Surface(uint64_t id);
  void Video(const MiniRtcVideoFrame& frame, uint64_t generation = 0);
  void Clear();
 private:
  void Run();
  std::mutex mutex_;
  std::condition_variable ready_;
  uint64_t surface_ = 0;
  bool changed_ = false, stop_ = false;
  int width_ = 0, height_ = 0;
  std::vector<uint8_t> latest_;
  uint64_t captured_ = 0, received_ = 0, generation_ = 0;
  Presented presented_;
  std::atomic<uint64_t> rendered_{0};
  // Start only after all state used by Run() has been initialized.
  std::thread thread_;
};

}

#endif
