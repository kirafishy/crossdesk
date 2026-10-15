#include "capture.h"
#include <native_buffer/native_buffer.h>
#include <native_buffer/buffer_common.h>
#include <multimedia/player_framework/native_avbuffer.h>
#include <libyuv.h>
#include <algorithm>
#include <hilog/log.h>
#include <native_window/external_window.h>
#include <poll.h>
#include <unistd.h>
#include <cerrno>

namespace crossdesk::harmony {
bool Capture::Start(int width, int height, uint64_t display, Frame frame, State state,
                    Audio audio, int fps, bool persistent_permission, bool show_cursor) {
  Stop(); frame_ = std::move(frame); state_ = std::move(state); audio_ = std::move(audio);
  show_cursor_ = show_cursor;
  first_video_ = true;
  first_data_ = true;
  max_frame_rate_ = std::clamp(fps, 1, 60);
  capture_ = OH_AVScreenCapture_Create();
  if (!capture_) return false;
  window_ = gpu_surface_.Start(width, height, [this](const uint8_t* pixels, int width, int height, int stride) {
    if (active_) ConvertRgba(pixels, width, height, stride);
  });
  const bool surface_mode = window_ != nullptr || PrepareSurface(width, height);
  const auto state_result = OH_AVScreenCapture_SetStateCallback(capture_, [](OH_AVScreenCapture* capture, OH_AVScreenCaptureStateCode code, void* user) {
    auto* self = static_cast<Capture*>(user);
    if (code == OH_SCREEN_CAPTURE_STATE_STARTED) {
      // Both operations require a running capture. StartScreenCapture may
      // return while the system is still awaiting the user's authorization.
      const auto rate = OH_AVScreenCapture_SetMaxVideoFrameRate(capture, self->max_frame_rate_);
      if (rate != AV_SCREEN_CAPTURE_ERR_OK)
        OH_LOG_Print(LOG_APP, LOG_WARN, 0, "SlintDesktop", "Capture frame rate unavailable: %{public}d", int(rate));
    }
    if (code == OH_SCREEN_CAPTURE_STATE_STARTED || code == OH_SCREEN_CAPTURE_STATE_EXIT_PRIVATE_SCENE ||
        code == OH_SCREEN_CAPTURE_STATE_RESUMED_BY_USER || code == OH_SCREEN_CAPTURE_STATE_RESUMED_BY_APP) {
      // Native viewers draw the separately transmitted cursor. Only advertise
      // that mode once the recorder confirms it has removed the video cursor.
      const auto result = OH_AVScreenCapture_ShowCursor(capture, self->show_cursor_);
      if (result == AV_SCREEN_CAPTURE_ERR_OK) {
        self->cursor_mode_ = self->show_cursor_ ? CaptureCursorMode::embedded : CaptureCursorMode::separate;
      } else {
        self->cursor_mode_ = !self->show_cursor_ &&
            OH_AVScreenCapture_ShowCursor(capture, true) == AV_SCREEN_CAPTURE_ERR_OK
            ? CaptureCursorMode::embedded : CaptureCursorMode::unknown;
        OH_LOG_Print(LOG_APP, LOG_WARN, 0, "SlintDesktop", "Capture cursor setting failed: %{public}d", int(result));
      }
    }
    self->active_ = CaptureActiveForState(code, self->active_);
    self->state_(static_cast<int>(code));
  }, this);
  const auto error_result = OH_AVScreenCapture_SetErrorCallback(capture_, [](OH_AVScreenCapture*, int32_t error, void* user) {
    OH_LOG_Print(LOG_APP, LOG_ERROR, 0, "SlintDesktop", "Capture error: %{public}d", error);
    auto* self = static_cast<Capture*>(user); self->active_ = false; self->state_(-1);
  }, this);
  const auto data_result = OH_AVScreenCapture_SetDataCallback(capture_, [](OH_AVScreenCapture*, OH_AVBuffer* buffer,
      OH_AVScreenCaptureBufferType type, int64_t, void* user) {
    auto* self = static_cast<Capture*>(user);
    if (self->first_data_.exchange(false))
      OH_LOG_Print(LOG_APP, LOG_INFO, 0, "SlintDesktop", "Capture first data: type=%{public}d active=%{public}d", int(type), int(self->active_.load()));
    if (!self->active_) return;
    if (type == OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_INNER && self->audio_) {
      OH_AVCodecBufferAttr attr{};
      auto* data = OH_AVBuffer_GetAddr(buffer);
      if (data && OH_AVBuffer_GetBufferAttr(buffer, &attr) == AV_ERR_OK && attr.offset >= 0 && attr.size > 0 &&
          int64_t(attr.offset) + attr.size <= OH_AVBuffer_GetCapacity(buffer))
        self->audio_(reinterpret_cast<const char*>(data + attr.offset), attr.size);
      return;
    }
    if (type != OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO) return;
    const bool first_video = self->first_video_.exchange(false);
    auto* native = OH_AVBuffer_GetNativeBuffer(buffer);
    if (!native) {
      if (first_video) OH_LOG_Print(LOG_APP, LOG_WARN, 0, "SlintDesktop", "Capture video has no native buffer");
      return;
    }
    self->Convert(native, first_video);
    OH_NativeBuffer_Unreference(native);
  }, this);
  OH_AVScreenCaptureConfig config{};
  OH_LOG_Print(LOG_APP, LOG_INFO, 0, "SlintDesktop", "Capture request: %{public}dx%{public}d display=%{public}llu fps=%{public}d",
      width, height, static_cast<unsigned long long>(display), max_frame_rate_);
  config.captureMode = OH_CAPTURE_SPECIFIED_SCREEN; config.dataType = OH_ORIGINAL_STREAM;
  config.videoInfo.videoCapInfo = {display, nullptr, 0, width, height, OH_VIDEO_SOURCE_SURFACE_RGBA};
  if (audio_) config.audioInfo.innerCapInfo = {48000, 1, OH_ALL_PLAYBACK};
  const auto check = [](const char* stage, OH_AVSCREEN_CAPTURE_ErrCode result) {
    if (result == AV_SCREEN_CAPTURE_ERR_OK) return true;
    OH_LOG_Print(LOG_APP, LOG_ERROR, 0, "SlintDesktop", "Capture %{public}s failed: %{public}d", stage, int(result));
    return false;
  };
  if (!check("state callback", state_result) || !check("error callback", error_result) ||
      !check("data callback", data_result) || !check("initialization", OH_AVScreenCapture_Init(capture_, config)) ||
      !check("microphone disable", OH_AVScreenCapture_SetMicrophoneEnabled(capture_, false))) {
    Stop(); return false;
  }
  if (persistent_permission) {
    // With CUSTOM_SCREEN_RECORDING granted, the system can reuse consent for
    // this specified display. Disabling the picker alone never grants access.
    auto* strategy = OH_AVScreenCapture_CreateCaptureStrategy();
    bool configured = false;
    if (strategy) {
      configured = OH_AVScreenCapture_StrategyForPickerPopUp(strategy, false) == AV_SCREEN_CAPTURE_ERR_OK &&
                   OH_AVScreenCapture_SetCaptureStrategy(capture_, strategy) == AV_SCREEN_CAPTURE_ERR_OK;
      OH_AVScreenCapture_ReleaseCaptureStrategy(strategy);
    }
    if (!configured)
      OH_LOG_Print(LOG_APP, LOG_WARN, 0, "SlintDesktop", "Persistent capture strategy unavailable; using system authorization");
  }
  if (!check("start", surface_mode ? OH_AVScreenCapture_StartScreenCaptureWithSurface(capture_, window_)
                                   : OH_AVScreenCapture_StartScreenCapture(capture_))) {
    Stop(); return false;
  }
  return true;
}
bool Capture::PrepareSurface(int width, int height) {
  image_ = OH_ConsumerSurface_Create();
  if (!image_) return false;
  if (OH_ConsumerSurface_SetDefaultUsage(image_, NATIVEBUFFER_USAGE_CPU_READ | NATIVEBUFFER_USAGE_CPU_READ_OFTEN |
                                               NATIVEBUFFER_USAGE_HW_RENDER) != 0 ||
      OH_ConsumerSurface_SetDefaultSize(image_, width, height) != 0 ||
      !(window_ = OH_NativeImage_AcquireNativeWindow(image_))) {
    OH_NativeImage_Destroy(&image_); return false;
  }
  surface_pending_ = false; surface_stopping_ = false;
  OH_OnFrameAvailableListener listener{this, [](void* context) {
    auto* self = static_cast<Capture*>(context);
    std::lock_guard lock(self->surface_mutex_);
    self->surface_pending_ = true; self->surface_ready_.notify_one();
  }};
  if (OH_NativeImage_SetOnFrameAvailableListener(image_, listener) != 0) {
    window_ = nullptr; // The native image owns its producer window.
    OH_NativeImage_Destroy(&image_); return false;
  }
  surface_thread_ = std::thread([this] { ConsumeSurface(); });
  return true;
}
void Capture::ConsumeSurface() {
  for (;;) {
    {
      std::unique_lock lock(surface_mutex_);
      surface_ready_.wait(lock, [this] { return surface_stopping_ || surface_pending_; });
      if (surface_stopping_) return;
      surface_pending_ = false;
    }
    // Notifications may coalesce while conversion is running. Drain the queue
    // so a static desktop cannot leave its newest frame waiting indefinitely.
    for (;;) {
      { std::lock_guard lock(surface_mutex_); if (surface_stopping_) return; }
      OHNativeWindowBuffer* buffer = nullptr;
      int fence = -1;
      if (OH_NativeImage_AcquireNativeWindowBuffer(image_, &buffer, &fence) != 0) break;
      bool ready = true;
      if (fence >= 0) {
        pollfd descriptor{fence, POLLIN, 0};
        int result;
        do { result = poll(&descriptor, 1, 1000); } while (result < 0 && errno == EINTR);
        ready = result > 0 && (descriptor.revents & POLLIN);
      }
      OH_NativeBuffer* native = nullptr;
      if (ready && active_ && OH_NativeBuffer_FromNativeWindowBuffer(buffer, &native) == 0 && native)
        Convert(native, first_video_.exchange(false));
      // Return an unsignaled fence to the producer on timeout; never let it
      // reuse memory while a GPU write is still outstanding.
      OH_NativeImage_ReleaseNativeWindowBuffer(image_, buffer, ready ? -1 : fence);
      if (ready && fence >= 0) close(fence);
    }
  }
}
void Capture::Convert(OH_NativeBuffer* native, bool first_video) {
  OH_NativeBuffer_Config config{}; OH_NativeBuffer_GetConfig(native, &config);
  if (first_video)
    OH_LOG_Print(LOG_APP, LOG_INFO, 0, "SlintDesktop", "Capture buffer: %{public}dx%{public}d format=%{public}d stride=%{public}d",
        config.width, config.height, config.format, config.stride);
  void* pixels = nullptr;
  if (config.format == NATIVEBUFFER_PIXEL_FMT_RGBA_8888 &&
      config.width > 0 && config.height > 0 && config.width <= 4096 && config.height <= 4096 &&
      !(config.width & 1) && !(config.height & 1) && config.stride >= config.width * 4 &&
      OH_NativeBuffer_Map(native, &pixels) == 0 && pixels) {
    ConvertRgba(static_cast<const uint8_t*>(pixels), config.width, config.height, config.stride);
    OH_NativeBuffer_Unmap(native);
  }
}
void Capture::ConvertRgba(const uint8_t* pixels, int width, int height, int stride) {
  std::lock_guard lock(data_mutex_);
  const size_t area = size_t(width) * height;
  nv12_.resize(area * 3 / 2);
  if (libyuv::ABGRToNV12(pixels, stride, nv12_.data(), width, nv12_.data() + area, width, width, height) == 0 && active_) {
    MiniRtcVideoFrame video{};
    video.data = reinterpret_cast<const char*>(nv12_.data()); video.size = nv12_.size();
    video.width = width; video.height = height;
    frame_(video);
  }
}
void Capture::Stop() {
  cursor_mode_ = CaptureCursorMode::unknown;
  active_ = false;
  if (capture_) { OH_AVScreenCapture_StopScreenCapture(capture_); OH_AVScreenCapture_Release(capture_); capture_ = nullptr; }
  gpu_surface_.Stop();
  if (image_) {
    OH_NativeImage_UnsetOnFrameAvailableListener(image_);
    { std::lock_guard lock(surface_mutex_); surface_stopping_ = true; surface_ready_.notify_one(); }
    if (surface_thread_.joinable()) surface_thread_.join();
    window_ = nullptr; // OH_NativeImage_Destroy releases the producer window.
    OH_NativeImage_Destroy(&image_);
  }
  window_ = nullptr;
}
}
