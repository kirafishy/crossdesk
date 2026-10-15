/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _AUDIO_H_
#define _AUDIO_H_

#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <ohaudio/native_audiorenderer.h>

namespace crossdesk::harmony {
class AudioPlayer {
 public:
  ~AudioPlayer() { Stop(); }
  bool Start();
  void Stop();
  void Push(const char* data, size_t size);
 private:
  OH_AudioRenderer* renderer_ = nullptr;
  std::mutex mutex_;
  std::deque<uint8_t> queue_;
  uint64_t rendered_bytes_ = 0;
};

}

#endif
