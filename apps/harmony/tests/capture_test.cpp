#include "capture.h"
#include <native_buffer/native_buffer.h>
#include <native_buffer/buffer_common.h>
#include <multimedia/player_framework/native_avbuffer.h>
#include <hilog/log.h>
#include <libyuv.h>
#include <cassert>
#include <cstring>
#include <deque>
#include <fcntl.h>
#include <unistd.h>

// Test the production capture adapter with asynchronous SDK callbacks, without
// opening a recorder or granting access to the workstation's screen.
struct OH_AVScreenCapture {
  OH_AVScreenCapture_OnStateChange state{};
  OH_AVScreenCapture_OnError error{};
  OH_AVScreenCapture_OnBufferAvailable data{};
  void *state_user{}, *error_user{}, *data_user{};
  bool started = false;
};
struct OH_NativeBuffer {};
struct OH_AVScreenCapture_CaptureStrategy { bool picker = true; };
struct OH_AVBuffer {};
struct OH_NativeImage {
  OH_OnFrameAvailableListener listener{};
  std::mutex mutex;
  std::deque<int> fences;
};
OH_NativeImage* surface{};
int window_token = 0, buffer_token = 0;
bool surface_available = false, reject_surface_listener = false;
int surface_starts = 0, surface_destroys = 0;
std::atomic<int> surface_releases{0}, released_fence{-2};
OH_AVScreenCapture* current{};
OH_NativeBuffer native_buffer;
OH_AVBuffer av_buffer;
OH_NativeBuffer_Config buffer_config{4, 2, NATIVEBUFFER_PIXEL_FMT_RGBA_8888, 0, 16};
OH_AVCodecBufferAttr audio_attr{};
unsigned char pixels[32]{};
int rate_calls = 0, rate = 0, starts = 0, releases = 0, unrefs = 0;
std::atomic<int> unmaps{0};
bool reject_init = false, no_native = false, reject_map = false;
bool gpu_available = false;
int gpu_stops = 0;
int strategy_creates = 0, strategy_releases = 0, strategy_applies = 0;
bool reject_strategy_create = false, reject_picker = false, reject_strategy_apply = false;
bool picker_disabled = false;
bool cursor_shown = false, reject_cursor_hide = false, reject_cursor_show = false;
crossdesk::harmony::CaptureSurface::Frame gpu_frame;

// Only replace EGL/driver ownership here. Capture's authorization, conversion,
// SDK calls, fallback and cleanup remain the production implementation.
namespace crossdesk::harmony {
OHNativeWindow* CaptureSurface::Start(int width, int height, Frame frame) {
  assert(width == 4 && height == 2);
  if (!gpu_available) return nullptr;
  gpu_frame = std::move(frame);
  return reinterpret_cast<OHNativeWindow*>(&window_token);
}
void CaptureSurface::Stop() {
  if (gpu_frame) { ++gpu_stops; gpu_frame = {}; }
}
}

extern "C" {
OH_NativeImage* OH_ConsumerSurface_Create() { return surface_available ? surface = new OH_NativeImage : nullptr; }
int32_t OH_ConsumerSurface_SetDefaultUsage(OH_NativeImage*, uint64_t usage) {
  assert((usage & NATIVEBUFFER_USAGE_CPU_READ) && (usage & NATIVEBUFFER_USAGE_HW_RENDER)); return 0;
}
int32_t OH_ConsumerSurface_SetDefaultSize(OH_NativeImage*, int32_t width, int32_t height) { assert(width == 4 && height == 2); return 0; }
OHNativeWindow* OH_NativeImage_AcquireNativeWindow(OH_NativeImage*) { return reinterpret_cast<OHNativeWindow*>(&window_token); }
int32_t OH_NativeImage_SetOnFrameAvailableListener(OH_NativeImage* image, OH_OnFrameAvailableListener listener) {
  if (reject_surface_listener) return -1;
  image->listener = listener; return 0;
}
int32_t OH_NativeImage_UnsetOnFrameAvailableListener(OH_NativeImage* image) { image->listener = {}; return 0; }
void OH_NativeImage_Destroy(OH_NativeImage** image) {
  assert(!(*image)->listener.onFrameAvailable);
  delete *image; *image = nullptr; surface = nullptr; ++surface_destroys;
}
int32_t OH_NativeImage_AcquireNativeWindowBuffer(OH_NativeImage* image, OHNativeWindowBuffer** buffer, int* fence) {
  std::lock_guard lock(image->mutex);
  if (image->fences.empty()) return 40601000;
  *fence = image->fences.front(); image->fences.pop_front();
  *buffer = reinterpret_cast<OHNativeWindowBuffer*>(&buffer_token); return 0;
}
int32_t OH_NativeImage_ReleaseNativeWindowBuffer(OH_NativeImage*, OHNativeWindowBuffer*, int fence) {
  released_fence = fence;
  if (fence >= 0) close(fence);
  ++surface_releases; return 0;
}
int32_t OH_NativeBuffer_FromNativeWindowBuffer(OHNativeWindowBuffer*, OH_NativeBuffer** buffer) {
  *buffer = &native_buffer; return 0; // Borrowed; must not decrement its reference count.
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_StartScreenCaptureWithSurface(OH_AVScreenCapture*, OHNativeWindow* window) {
  assert(window == reinterpret_cast<OHNativeWindow*>(&window_token));
  ++starts; ++surface_starts; return AV_SCREEN_CAPTURE_ERR_OK;
}
int OH_LOG_Print(LogType, LogLevel, unsigned int, const char*, const char*, ...) { return 0; }
OH_AVScreenCapture* OH_AVScreenCapture_Create() { return current = new OH_AVScreenCapture; }
OH_AVScreenCapture_CaptureStrategy* OH_AVScreenCapture_CreateCaptureStrategy() {
  ++strategy_creates;
  return reject_strategy_create ? nullptr : new OH_AVScreenCapture_CaptureStrategy;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_ReleaseCaptureStrategy(OH_AVScreenCapture_CaptureStrategy* strategy) {
  ++strategy_releases; delete strategy; return AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_StrategyForPickerPopUp(OH_AVScreenCapture_CaptureStrategy* strategy, bool value) {
  assert(!value); strategy->picker = value;
  return reject_picker ? AV_SCREEN_CAPTURE_ERR_INVALID_VAL : AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_SetCaptureStrategy(OH_AVScreenCapture* p, OH_AVScreenCapture_CaptureStrategy* strategy) {
  assert(p == current && !p->started);
  ++strategy_applies;
  if (reject_strategy_apply) return AV_SCREEN_CAPTURE_ERR_INVALID_VAL;
  picker_disabled = !strategy->picker;
  return AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_Release(OH_AVScreenCapture* p) {
  ++releases; delete p; current = nullptr; return AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_SetStateCallback(OH_AVScreenCapture* p, OH_AVScreenCapture_OnStateChange cb, void* user) {
  p->state = cb; p->state_user = user; return AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_SetErrorCallback(OH_AVScreenCapture* p, OH_AVScreenCapture_OnError cb, void* user) {
  p->error = cb; p->error_user = user; return AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_SetDataCallback(OH_AVScreenCapture* p, OH_AVScreenCapture_OnBufferAvailable cb, void* user) {
  p->data = cb; p->data_user = user; return AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_Init(OH_AVScreenCapture*, OH_AVScreenCaptureConfig config) {
  assert(config.captureMode == OH_CAPTURE_SPECIFIED_SCREEN);
  assert(config.videoInfo.videoCapInfo.displayId == 42);
  assert(config.videoInfo.videoCapInfo.videoFrameWidth == 4 && config.videoInfo.videoCapInfo.videoFrameHeight == 2);
  return reject_init ? AV_SCREEN_CAPTURE_ERR_INVALID_VAL : AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_SetMicrophoneEnabled(OH_AVScreenCapture*, bool enabled) {
  assert(!enabled); return AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_StartScreenCapture(OH_AVScreenCapture*) {
  ++starts; return AV_SCREEN_CAPTURE_ERR_OK; // Consent is still pending.
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_StopScreenCapture(OH_AVScreenCapture* p) {
  p->started = false; p->state(p, OH_SCREEN_CAPTURE_STATE_STOPPED_BY_USER, p->state_user);
  return AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_SetMaxVideoFrameRate(OH_AVScreenCapture* p, int32_t fps) {
  assert(p->started); ++rate_calls; rate = fps; return AV_SCREEN_CAPTURE_ERR_OK;
}
OH_AVSCREEN_CAPTURE_ErrCode OH_AVScreenCapture_ShowCursor(OH_AVScreenCapture* p, bool visible) {
  assert(p->started);
  if (visible ? reject_cursor_show : reject_cursor_hide) return AV_SCREEN_CAPTURE_ERR_OPERATE_NOT_PERMIT;
  cursor_shown = visible;
  return AV_SCREEN_CAPTURE_ERR_OK;
}
OH_NativeBuffer* OH_AVBuffer_GetNativeBuffer(OH_AVBuffer*) { return no_native ? nullptr : &native_buffer; }
void OH_NativeBuffer_GetConfig(OH_NativeBuffer*, OH_NativeBuffer_Config* config) { *config = buffer_config; }
int32_t OH_NativeBuffer_Map(OH_NativeBuffer*, void** out) { *out = pixels; return reject_map ? -1 : 0; }
int32_t OH_NativeBuffer_Unmap(OH_NativeBuffer*) { ++unmaps; return 0; }
int32_t OH_NativeBuffer_Unreference(OH_NativeBuffer*) { ++unrefs; return 0; }
uint8_t* OH_AVBuffer_GetAddr(OH_AVBuffer*) { return pixels; }
int32_t OH_AVBuffer_GetCapacity(OH_AVBuffer*) { return sizeof(pixels); }
OH_AVErrCode OH_AVBuffer_GetBufferAttr(OH_AVBuffer*, OH_AVCodecBufferAttr* out) { *out = audio_attr; return AV_ERR_OK; }
}
namespace libyuv {
extern "C" int ABGRToNV12(const uint8_t* src, int stride, uint8_t* y, int y_stride,
                         uint8_t* uv, int uv_stride, int width, int height) {
  assert(src == pixels && stride == 16 && width == 4 && height == 2);
  assert(y_stride == 4 && uv_stride == 4 && uv == y + 8);
  std::memset(y, 128, 12); return 0;
}
}
void State(OH_AVScreenCaptureStateCode state) {
  if (state == OH_SCREEN_CAPTURE_STATE_STARTED) current->started = true;
  current->state(current, state, current->state_user);
}
void Data(OH_AVScreenCaptureBufferType type = OH_SCREEN_CAPTURE_BUFFERTYPE_VIDEO) {
  current->data(current, &av_buffer, type, 0, current->data_user);
}
void SurfaceData(std::initializer_list<int> fences) {
  { std::lock_guard lock(surface->mutex); surface->fences.insert(surface->fences.end(), fences); }
  surface->listener.onFrameAvailable(surface->listener.context);
}
template<class F> void Wait(F condition) {
  const auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!condition() && std::chrono::steady_clock::now() < limit)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(condition());
}
int main() {
  crossdesk::harmony::Capture capture;
  std::atomic<int> frames{0};
  int audio_bytes = 0, last_state = -1;
  const auto frame = [&](const MiniRtcVideoFrame& value) {
    assert(value.width == 4 && value.height == 2 && value.size == 12);
    assert(static_cast<unsigned char>(value.data[0]) == 128); ++frames;
  };
  const auto state = [&](int code) { last_state = code; };
  const auto audio = [&](const char*, size_t size) { audio_bytes += int(size); };
  assert(capture.Start(4, 2, 42, frame, state, audio, 30));
  assert(starts == 1 && rate_calls == 0 && !capture.CursorEmbedded());
  Data(); assert(frames == 0); // Do not send data before consent completes.
  State(OH_SCREEN_CAPTURE_STATE_STARTED);
  assert(rate == 30 && rate_calls == 1 && capture.CursorEmbedded());
  Data(); assert(frames == 1 && unrefs == 1 && unmaps == 1);
  State(OH_SCREEN_CAPTURE_STATE_ENTER_PRIVATE_SCENE); Data(); assert(frames == 1);
  State(OH_SCREEN_CAPTURE_STATE_EXIT_PRIVATE_SCENE); Data(); assert(frames == 2);
  State(OH_SCREEN_CAPTURE_STATE_PAUSED_BY_USER); Data(); assert(frames == 2);
  State(OH_SCREEN_CAPTURE_STATE_RESUMED_BY_USER); Data(); assert(frames == 3);
  State(OH_SCREEN_CAPTURE_STATE_MIC_MUTED_BY_USER); Data(); assert(frames == 4);
  audio_attr.offset = 2; audio_attr.size = 4;
  Data(OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_INNER); assert(audio_bytes == 4);
  audio_attr.offset = 31; audio_attr.size = 4;
  Data(OH_SCREEN_CAPTURE_BUFFERTYPE_AUDIO_INNER); assert(audio_bytes == 4);
  no_native = true; Data(); no_native = false; assert(frames == 4 && unrefs == 4);
  reject_map = true; Data(); reject_map = false; assert(frames == 4 && unrefs == 5 && unmaps == 4);
  buffer_config.width = 3; Data(); buffer_config.width = 4; assert(frames == 4 && unrefs == 6);
  current->error(current, -10, current->error_user); Data(); assert(frames == 4 && last_state == -1);
  capture.Stop(); assert(current == nullptr && releases == 1 && !capture.CursorEmbedded());
  capture.Stop(); assert(releases == 1);
  reject_init = true;
  assert(!capture.Start(4, 2, 42, frame, state, audio, 120));
  assert(starts == 1 && releases == 2 && current == nullptr);
  reject_init = false;
  assert(capture.Start(4, 2, 42, frame, state, audio, 120));
  State(OH_SCREEN_CAPTURE_STATE_STARTED); assert(rate == 60);
  State(OH_SCREEN_CAPTURE_STATE_CANCELED); Data(); assert(frames == 4);
  capture.Stop();

  surface_available = true;
  assert(capture.Start(4, 2, 42, frame, state, audio, 30));
  assert(surface_starts == 1);
  SurfaceData({-1}); Wait([] { return surface_releases == 1; }); assert(frames == 4);
  State(OH_SCREEN_CAPTURE_STATE_STARTED);
  const int references_before = unrefs;
  SurfaceData({-1, -1, -1}); // One callback for three queued buffers must drain all three.
  Wait([&] { return frames == 7 && surface_releases == 4; });
  assert(unrefs == references_before);
  State(OH_SCREEN_CAPTURE_STATE_ENTER_PRIVATE_SCENE);
  SurfaceData({-1}); Wait([] { return surface_releases == 5; }); assert(frames == 7);
  State(OH_SCREEN_CAPTURE_STATE_EXIT_PRIVATE_SCENE);
  int pipe_fd[2]; assert(pipe(pipe_fd) == 0);
  assert(write(pipe_fd[1], "x", 1) == 1);
  SurfaceData({pipe_fd[0]}); Wait([&] { return frames == 8 && surface_releases == 6; });
  Wait([&] { return fcntl(pipe_fd[0], F_GETFD) == -1; }); close(pipe_fd[1]);
  assert(released_fence == -1);
  assert(pipe(pipe_fd) == 0);
  SurfaceData({pipe_fd[0]}); // Timeout must return the outstanding fence to its owner.
  Wait([] { return surface_releases == 7; });
  assert(frames == 8 && released_fence == pipe_fd[0] && fcntl(pipe_fd[0], F_GETFD) == -1);
  close(pipe_fd[1]);
  capture.Stop(); assert(surface == nullptr && surface_destroys == 1);
  capture.Stop(); assert(surface_destroys == 1);
  reject_surface_listener = true;
  assert(capture.Start(4, 2, 42, frame, state, audio, 30));
  assert(surface == nullptr && surface_destroys == 2 && surface_starts == 1);
  State(OH_SCREEN_CAPTURE_STATE_STARTED); Data(); assert(frames == 9);
  capture.Stop();
  reject_surface_listener = false;
  reject_init = true;
  assert(!capture.Start(4, 2, 42, frame, state, audio, 30));
  assert(surface == nullptr && surface_destroys == 3); // Worker exits even when capture never starts.

  reject_init = false;
  gpu_available = true;
  assert(capture.Start(4, 2, 42, frame, state, audio, 30));
  assert(surface == nullptr && surface_starts == 2); // Prefer GPU over CPU mapping.
  gpu_frame(pixels, 4, 2, 16); assert(frames == 9);
  State(OH_SCREEN_CAPTURE_STATE_STARTED);
  gpu_frame(pixels, 4, 2, 16); assert(frames == 10);
  State(OH_SCREEN_CAPTURE_STATE_ENTER_PRIVATE_SCENE);
  gpu_frame(pixels, 4, 2, 16); assert(frames == 10);
  State(OH_SCREEN_CAPTURE_STATE_EXIT_PRIVATE_SCENE);
  gpu_frame(pixels, 4, 2, 16); assert(frames == 11);
  State(OH_SCREEN_CAPTURE_STATE_STOPPED_BY_USER);
  gpu_frame(pixels, 4, 2, 16); assert(frames == 11);
  capture.Stop(); capture.Stop(); assert(gpu_stops == 1 && !gpu_frame);
  reject_init = true;
  assert(!capture.Start(4, 2, 42, frame, state, audio, 30));
  assert(gpu_stops == 2 && !gpu_frame && current == nullptr);
  assert(strategy_creates == 0); // Ordinary sessions keep the system's default authorization UI.

  reject_init = false;
  assert(capture.Start(4, 2, 42, frame, state, audio, 30, true));
  assert(picker_disabled && strategy_applies == 1 && strategy_creates == strategy_releases);
  gpu_frame(pixels, 4, 2, 16); assert(frames == 11); // A permission hint cannot replace STARTED.
  State(OH_SCREEN_CAPTURE_STATE_STARTED);
  gpu_frame(pixels, 4, 2, 16); assert(frames == 12);
  capture.Stop();
  assert(capture.Start(4, 2, 42, frame, state, audio, 30, true));
  assert(strategy_applies == 2 && strategy_creates == strategy_releases);
  State(OH_SCREEN_CAPTURE_STATE_CANCELED); // The OS can still reject a revoked grant.
  gpu_frame(pixels, 4, 2, 16); assert(frames == 12);
  capture.Stop();
  for (bool* failure : {&reject_strategy_create, &reject_picker, &reject_strategy_apply}) {
    *failure = true; picker_disabled = false;
    assert(capture.Start(4, 2, 42, frame, state, audio, 30, true));
    assert(!picker_disabled); // Failed optional configuration falls back to system authorization.
    capture.Stop(); *failure = false;
  }
  assert(strategy_creates == strategy_releases + 1); // Failed allocation has no resource to release.

  using crossdesk::harmony::CaptureCursorMode;
  assert(capture.Start(4, 2, 42, frame, state, {}, 30, false, false));
  assert(capture.CursorMode() == CaptureCursorMode::unknown);
  State(OH_SCREEN_CAPTURE_STATE_STARTED);
  assert(!cursor_shown && capture.CursorMode() == CaptureCursorMode::separate);
  State(OH_SCREEN_CAPTURE_STATE_ENTER_PRIVATE_SCENE);
  cursor_shown = true;
  State(OH_SCREEN_CAPTURE_STATE_EXIT_PRIVATE_SCENE);
  assert(!cursor_shown && capture.CursorMode() == CaptureCursorMode::separate);
  State(OH_SCREEN_CAPTURE_STATE_PAUSED_BY_USER);
  cursor_shown = true;
  State(OH_SCREEN_CAPTURE_STATE_RESUMED_BY_USER);
  assert(!cursor_shown && capture.CursorMode() == CaptureCursorMode::separate);
  capture.Stop();
  assert(capture.CursorMode() == CaptureCursorMode::unknown);

  reject_cursor_hide = true;
  assert(capture.Start(4, 2, 42, frame, state, {}, 30, false, false));
  State(OH_SCREEN_CAPTURE_STATE_STARTED);
  assert(cursor_shown && capture.CursorMode() == CaptureCursorMode::embedded);
  capture.Stop();
  reject_cursor_show = true;
  assert(capture.Start(4, 2, 42, frame, state, {}, 30, false, false));
  State(OH_SCREEN_CAPTURE_STATE_STARTED);
  assert(capture.CursorMode() == CaptureCursorMode::unknown);
  capture.Stop();
}
