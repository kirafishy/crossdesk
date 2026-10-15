/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _CAPTURE_H_
#define _CAPTURE_H_

#include <minirtc.h>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <multimedia/player_framework/native_avscreen_capture.h>
#include <native_image/native_image.h>
#include "capture_surface.h"

namespace crossdesk::harmony {
// Microphone notifications do not change video/input availability.
inline bool CaptureActiveForState(int state, bool current) {
  if (state < 0) return false;
  switch (state) {
    case OH_SCREEN_CAPTURE_STATE_STARTED:
    case OH_SCREEN_CAPTURE_STATE_EXIT_PRIVATE_SCENE:
    case OH_SCREEN_CAPTURE_STATE_RESUMED_BY_USER:
    case OH_SCREEN_CAPTURE_STATE_RESUMED_BY_APP: return true;
    case OH_SCREEN_CAPTURE_STATE_CANCELED:
    case OH_SCREEN_CAPTURE_STATE_STOPPED_BY_USER:
    case OH_SCREEN_CAPTURE_STATE_INTERRUPTED_BY_OTHER:
    case OH_SCREEN_CAPTURE_STATE_STOPPED_BY_CALL:
    case OH_SCREEN_CAPTURE_STATE_ENTER_PRIVATE_SCENE:
    case OH_SCREEN_CAPTURE_STATE_STOPPED_BY_USER_SWITCHES:
    case OH_SCREEN_CAPTURE_STATE_PAUSED_BY_USER:
    case OH_SCREEN_CAPTURE_STATE_PAUSED_BY_APP: return false;
    default: return current;
  }
}
enum class CaptureCursorMode { unknown, separate, embedded };

class Capture {
 public:
  using Frame = std::function<void(const MiniRtcVideoFrame&)>;
  using State = std::function<void(int)>;
  using Audio = std::function<void(const char*, size_t)>;
  ~Capture() { Stop(); }
  bool Start(int width, int height, uint64_t display, Frame frame, State state,
             Audio audio = {}, int fps = 30, bool persistent_permission = false,
             bool show_cursor = true);
  void Stop();
  CaptureCursorMode CursorMode() const { return cursor_mode_; }
  bool CursorEmbedded() const { return CursorMode() == CaptureCursorMode::embedded; }
 private:
  bool PrepareSurface(int width, int height);
  void ConsumeSurface();
  void Convert(OH_NativeBuffer* native, bool first_video);
  void ConvertRgba(const uint8_t* pixels, int width, int height, int stride);
  CaptureSurface gpu_surface_;
  OH_NativeImage* image_ = nullptr;
  OHNativeWindow* window_ = nullptr;
  std::mutex surface_mutex_;
  std::condition_variable surface_ready_;
  bool surface_stopping_ = true, surface_pending_ = false;
  std::thread surface_thread_;
  OH_AVScreenCapture* capture_ = nullptr;
  std::atomic<CaptureCursorMode> cursor_mode_{CaptureCursorMode::unknown};
  bool show_cursor_ = true;
  int max_frame_rate_ = 30;
  Frame frame_;
  State state_;
  Audio audio_;
  std::mutex data_mutex_;
  std::vector<uint8_t> nv12_;
  std::atomic<bool> active_{false};
  std::atomic<bool> first_video_{true};
  std::atomic<bool> first_data_{true};
};
}

#endif
