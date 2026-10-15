/*
 * @Author: DI JUNKUN
 * @Date: 2025-12-18
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _FILE_TRANSFER_H_
#define _FILE_TRANSFER_H_

#include <filesystem>
#include <functional>
#include <file_transfer_core.h>

namespace crossdesk {

// Desktop destination policy; protocol, assembly, ACKs and cleanup live in the
// same engine used by the mobile adapter.
class FileReceiver {
 public:
  using OnSendAck = std::function<int(const FileTransferAck&)>;
  FileReceiver();
  explicit FileReceiver(const std::filesystem::path& output_dir);
  bool OnData(const char* data, size_t size) { return session_.Receive(data, size); }
  void Poll() { session_.Pump(); }
  void Cancel() { session_.Cancel(); }
  // Set once, before the receiver is published to transport callbacks.
  void SetOnSendAck(OnSendAck cb) { on_send_ack_ = std::move(cb); }
  std::filesystem::path OutputDir() const {
    std::lock_guard lock(directory_mutex_);
    return output_dir_;
  }
  void SetOutputDir(const std::filesystem::path& dir) {
    std::lock_guard lock(directory_mutex_);
    output_dir_ = dir;
    session_.SetDirectory(dir);
  }

 private:
  static std::filesystem::path GetDefaultDesktopPath();
  mutable std::mutex directory_mutex_;
  std::filesystem::path output_dir_;
  OnSendAck on_send_ack_;
  file_transfer::Session session_;
};

}  // namespace crossdesk

#endif
