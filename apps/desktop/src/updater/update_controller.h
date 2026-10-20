/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-20
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _UPDATE_CONTROLLER_H_
#define _UPDATE_CONTROLLER_H_

#include "updater/update_core.h"
#include <future>

namespace crossdesk::updater {
class Controller {
 public:
  enum class State { Idle, Downloading, Preparing, Ready, Failed };
  explicit Controller(fs::path cache);
  ~Controller();
  bool Available(const std::string& envelope) const;
  void Start(const std::string& envelope);
  // Returns true only after a live helper has acknowledged the prepared job.
  bool Poll();
  void Cancel();
  void ConfirmStartup();
  State state() const { return state_; }
  int progress() const { return progress_.load(); }
  const std::string& error() const { return error_; }
  bool busy() const { return state_ == State::Downloading || state_ == State::Preparing; }
 private:
  fs::path cache_, job_;
  State state_ = State::Idle;
  std::string error_;
  mutable std::string checked_envelope_;
  mutable bool checked_ = false;
  mutable bool available_ = false;
  std::atomic<int> progress_{0};
  std::atomic<bool> cancel_{false};
  std::future<fs::path> pending_;
  std::chrono::steady_clock::time_point deadline_{};
};
}  // namespace crossdesk::updater

#endif
