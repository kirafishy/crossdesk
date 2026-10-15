/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _CAPTURE_SURFACE_H_
#define _CAPTURE_SURFACE_H_

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <native_image/native_image.h>

namespace crossdesk::harmony {
// Owns the capture texture and its EGL context on one worker. Some graphics
// drivers expose GPU-backed capture buffers that cannot be read via mmap.
class CaptureSurface {
 public:
  using Frame = std::function<void(const uint8_t*, int, int, int)>;
  ~CaptureSurface() { Stop(); }
  OHNativeWindow* Start(int width, int height, Frame frame);
  void Stop();
 private:
  void Run(int width, int height, Frame frame);
  std::mutex mutex_;
  std::condition_variable ready_;
  bool prepared_ = false, stopping_ = true;
  unsigned pending_ = 0;
  OHNativeWindow* window_ = nullptr; // Owned by the worker's OH_NativeImage.
  std::thread thread_;
};
}

#endif
