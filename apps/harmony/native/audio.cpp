#include "audio.h"
#include <ohaudio/native_audiostreambuilder.h>
#include <algorithm>
#include <cstring>
#include <hilog/log.h>

namespace crossdesk::harmony {
bool AudioPlayer::Start() {
  Stop();
  OH_AudioStreamBuilder* builder = nullptr;
  if (OH_AudioStreamBuilder_Create(&builder, AUDIOSTREAM_TYPE_RENDERER) != AUDIOSTREAM_SUCCESS) return false;
  OH_AudioStreamBuilder_SetSamplingRate(builder, 48000);
  OH_AudioStreamBuilder_SetChannelCount(builder, 1);
  OH_AudioStreamBuilder_SetSampleFormat(builder, AUDIOSTREAM_SAMPLE_S16LE);
  OH_AudioStreamBuilder_SetEncodingType(builder, AUDIOSTREAM_ENCODING_TYPE_RAW);
  OH_AudioStreamBuilder_SetRendererInfo(builder, AUDIOSTREAM_USAGE_MUSIC);
  OH_AudioRenderer_Callbacks callbacks{};
  callbacks.OH_AudioRenderer_OnWriteData = [](OH_AudioRenderer*, void* user, void* buffer, int32_t length) {
    auto* self = static_cast<AudioPlayer*>(user);
    if (!buffer || length <= 0) return 0;
    std::memset(buffer, 0, length);
    std::lock_guard lock(self->mutex_);
    auto* output = static_cast<uint8_t*>(buffer);
    const auto count = std::min(size_t(length), self->queue_.size());
    for (size_t i = 0; i < count; ++i) {
      output[i] = self->queue_.front(); self->queue_.pop_front();
    }
    if (count && !self->rendered_bytes_)
      OH_LOG_Print(LOG_APP, LOG_INFO, 0, "CrossDesk", "Audio playback received PCM: %{public}zu bytes at 48000 Hz mono", count);
    self->rendered_bytes_ += count;
    return 0;
  };
  callbacks.OH_AudioRenderer_OnError = [](OH_AudioRenderer*, void*, OH_AudioStream_Result error) {
    OH_LOG_Print(LOG_APP, LOG_ERROR, 0, "CrossDesk", "Audio playback error: %{public}d", int(error));
    return 0;
  };
  OH_AudioStreamBuilder_SetRendererCallback(builder, callbacks, this);
  auto result = OH_AudioStreamBuilder_GenerateRenderer(builder, &renderer_);
  OH_AudioStreamBuilder_Destroy(builder);
  if (result != AUDIOSTREAM_SUCCESS || OH_AudioRenderer_Start(renderer_) != AUDIOSTREAM_SUCCESS) {
    Stop(); return false;
  }
  return true;
}
void AudioPlayer::Stop() {
  if (renderer_) { OH_AudioRenderer_Stop(renderer_); OH_AudioRenderer_Release(renderer_); renderer_ = nullptr; }
  std::lock_guard lock(mutex_);
  if (rendered_bytes_) OH_LOG_Print(LOG_APP, LOG_INFO, 0, "CrossDesk", "Audio playback stopped after %{public}llu PCM bytes", (unsigned long long)rendered_bytes_);
  queue_.clear(); rendered_bytes_ = 0;
}
void AudioPlayer::Push(const char* data, size_t size) {
  if (!data || size == 0 || size > 19200 || (size & 1)) return;
  std::lock_guard lock(mutex_);
  if (queue_.size() + size > 19200) queue_.clear(); // At most 200 ms of mono PCM.
  queue_.insert(queue_.end(), data, data + size);
}

}
