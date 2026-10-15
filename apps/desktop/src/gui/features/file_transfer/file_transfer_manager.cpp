#include "features/file_transfer/file_transfer_manager.h"

#include <algorithm>
#include <limits>
#include <thread>

#include "filesystem_utf8.h"
#include "rd_log.h"
#include "runtime/gui_runtime.h"

namespace crossdesk {

FileTransferManager::FileTransferManager(GuiRuntime& owner) : owner_(owner) {}
FileTransferManager::~FileTransferManager() { Shutdown(); }

void FileTransferManager::Shutdown() {
  std::vector<std::future<void>> workers;
  {
    std::lock_guard lock(workers_mutex_);
    stopping_ = true;
    workers.swap(workers_);
  }
  {
    std::lock_guard lock(sessions_mutex_);
    for (auto& [id, sender] : senders_) sender.session->Cancel();
    for (auto& [handler, peers] : receivers_)
      for (auto& [id, receiver] : peers) receiver.session->Cancel();
    receivers_.clear();
  }
  wake_.notify_all();
  for (auto& worker : workers) worker.wait();
}

FileTransferManager::FileTransferState& FileTransferManager::global_state() { return global_state_; }
FileTransferManager::FileTransferState& FileTransferManager::state_for(const std::shared_ptr<RemoteSession>& props) {
  return props ? props->file_transfer_ : global_state_;
}

void FileTransferManager::ProcessSelectedFile(const std::string& path,
    const std::shared_ptr<RemoteSession>& props, const std::string& file_label,
    const std::string& remote_id) {
  if (path.empty() || stopping_ || (props && props->closing_)) return;
  const auto file_path = PathFromUtf8(path);
  std::error_code ec;
  if (!std::filesystem::is_regular_file(file_path, ec)) return;
  const auto size = std::filesystem::file_size(file_path, ec);
  if (ec) return;
  auto& state = state_for(props);
  {
    std::lock_guard lock(state.file_transfer_list_mutex_);
    FileTransferState::FileTransferInfo info;
    const auto name = file_path.filename().u8string();
    info.file_name.assign(name.begin(), name.end());
    info.file_path = file_path; info.file_size = size;
    info.status = FileTransferState::FileTransferStatus::Queued;
    state.file_transfer_list_.push_back(std::move(info));
  }
  state.file_transfer_window_visible_ = true;
  {
    std::lock_guard lock(state.file_queue_mutex_);
    state.file_send_queue_.push({file_path, file_label, props ? props->remote_id_ : remote_id});
  }
  ProcessQueue(props);
}

void FileTransferManager::ProcessQueue(std::shared_ptr<RemoteSession> props) {
  std::lock_guard workers_lock(workers_mutex_);
  auto& state = state_for(props);
  if (stopping_ || state.file_sending_) return;
  FileTransferState::QueuedFile file;
  {
    std::lock_guard lock(state.file_queue_mutex_);
    if (state.file_send_queue_.empty()) return;
    file = state.file_send_queue_.front(); state.file_send_queue_.pop();
    state.file_sending_ = true;
  }
  std::erase_if(workers_, [](auto& worker) {
    return worker.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
  });
  workers_.push_back(std::async(std::launch::async, [this, props, file] {
    auto& state = state_for(props);
    const auto id = file_transfer::Session::NextFileId();
    PeerPtr* peer;
    std::shared_ptr<PeerEventHandler> events;
    {
      std::shared_lock lock(owner_.remote_sessions_mutex_);
      peer = props ? props->peer_ : owner_.peer_;
      events = props ? props->peer_events_ : owner_.peer_events_;
    }
    auto session = std::make_shared<file_transfer::Session>(
        [peer, events, file, props](const char*, const char* data, size_t size) {
          if (!peer || !events || (props && props->closing_)) return -1;
          auto callback = events->EnterCallback();
          if (!callback) return -1;
          return SendReliableDataFrameToPeer(peer, data, size, file.file_label.c_str(),
                                             file.remote_id.c_str(), file.remote_id.size());
        }, file_transfer::Session::Progress{});
    state.file_sent_bytes_ = 0; state.file_total_bytes_ = 0; state.file_send_rate_bps_ = 0;
    state.current_file_id_ = id;
    {
      std::lock_guard lock(state.file_transfer_mutex_);
      state.file_send_start_time_ = state.file_send_last_update_time_ = std::chrono::steady_clock::now();
      state.file_send_last_bytes_ = 0;
    }
    {
      std::lock_guard lock(state.file_transfer_list_mutex_);
      for (auto& info : state.file_transfer_list_) {
        if (info.file_path == file.file_path && info.status == FileTransferState::FileTransferStatus::Queued) {
          info.status = FileTransferState::FileTransferStatus::Sending;
          info.file_id = id; info.sent_bytes = 0;
          break;
        }
      }
    }
    const auto name = file.file_path.filename().u8string();
    bool started = false;
    {
      std::lock_guard lock(sessions_mutex_);
      const auto closed = disconnected_.find(events.get());
      const bool disconnected = closed != disconnected_.end() &&
          (closed->second.count("") || closed->second.count(file.remote_id));
      if (!stopping_ && peer && events && events->IsActive() && !disconnected &&
          (!props || !props->closing_) && !file.remote_id.empty()) {
        started = session->Start(file.file_path, std::string(name.begin(), name.end()),
                                 file_transfer::Session::Clock::now(), id);
        if (started) senders_.emplace(id, Sender{session, events, file.remote_id});
      }
    }
    uint64_t previous = UINT64_MAX;
    auto progress = file_transfer::SendState{id, 0, 0, file_transfer::Status::Failed};
    if (started) {
      do {
        if (stopping_ || !events->IsActive() || (props && props->closing_)) session->Cancel();
        const bool pumped = session->Pump();
        progress = session->Snapshot();
        if (progress.acknowledged != previous || progress.status != file_transfer::Status::Sending) {
          UpdateProgress(state, props, progress); previous = progress.acknowledged;
        }
        if (progress.status != file_transfer::Status::Sending) break;
        if (!pumped) {
          std::unique_lock lock(wake_mutex_);
          wake_.wait_for(lock, std::chrono::milliseconds(5));
        }
      } while (true);
    }
    {
      std::lock_guard lock(sessions_mutex_);
      senders_.erase(id);
    }
    {
      std::lock_guard lock(state.file_transfer_list_mutex_);
      for (auto& info : state.file_transfer_list_) if (info.file_id == id) {
        info.status = progress.status == file_transfer::Status::Completed
            ? FileTransferState::FileTransferStatus::Completed : FileTransferState::FileTransferStatus::Failed;
        if (info.status == FileTransferState::FileTransferStatus::Completed) info.sent_bytes = info.file_size;
        break;
      }
    }
    if (progress.status != file_transfer::Status::Completed) {
      state.file_transfer_window_visible_ = false;
      state.file_sent_bytes_ = 0; state.file_total_bytes_ = 0;
      state.file_send_rate_bps_ = 0; state.current_file_id_ = 0;
    } else state.file_transfer_window_visible_ = true;
    state.file_sending_ = false;
    // Only the worker advances the queue, after all sends for this file drained.
    ProcessQueue(props);
  }));
}

void FileTransferManager::UpdateProgress(FileTransferState& state,
    const std::shared_ptr<RemoteSession>& props, const file_transfer::SendState& progress) {
  state.file_sent_bytes_ = progress.acknowledged; state.file_total_bytes_ = progress.total;
  const auto now = std::chrono::steady_clock::now();
  const uint32_t current = state.file_send_rate_bps_.load();
  uint32_t rate = current;
  {
    std::lock_guard lock(state.file_transfer_mutex_);
    if (props) {
      const auto bitrate = props->net_traffic_stats_.DataOutboundBitrate();
      if (bitrate) rate = current ? static_cast<uint32_t>(current * .7 + bitrate * .99 * .3)
                                 : static_cast<uint32_t>(bitrate * .99);
    } else {
      const double seconds = std::chrono::duration<double>(now - state.file_send_last_update_time_).count();
      if (seconds > 0 && progress.acknowledged > state.file_send_last_bytes_) {
        const double measured = (progress.acknowledged - state.file_send_last_bytes_) * 8.0 / seconds;
        const auto value = static_cast<uint32_t>((std::min)(measured,
            static_cast<double>((std::numeric_limits<uint32_t>::max)())));
        rate = current ? static_cast<uint32_t>(current * .7 + value * .3) : value;
      }
    }
    state.file_send_last_bytes_ = progress.acknowledged; state.file_send_last_update_time_ = now;
  }
  state.file_send_rate_bps_ = rate;
  std::lock_guard lock(state.file_transfer_list_mutex_);
  for (auto& info : state.file_transfer_list_) if (info.file_id == progress.id) {
    info.sent_bytes = progress.acknowledged; info.file_size = progress.total; info.rate_bps = rate;
    break;
  }
}

void FileTransferManager::HandleAck(const char* data, size_t size,
    PeerEventHandler* handler, const std::string& remote) {
  FileTransferAck ack{};
  if (!DecodeFileTransferAck(data, size, &ack)) return;
  std::shared_ptr<file_transfer::Session> session;
  {
    std::lock_guard lock(sessions_mutex_);
    const auto it = senders_.find(ack.file_id);
    if (it == senders_.end() || it->second.handler.get() != handler || it->second.remote != remote) return;
    session = it->second.session;
  }
  session->Ack(data, size);
  wake_.notify_all();
}

void FileTransferManager::Receive(const char* data, size_t size,
    PeerEventHandler* handler, const std::string& remote) {
  const auto props = owner_.FindRemoteSession(remote);
  PeerPtr* peer;
  std::shared_ptr<PeerEventHandler> events;
  {
    std::shared_lock lock(owner_.remote_sessions_mutex_);
    peer = props ? props->peer_ : owner_.peer_;
    events = props ? props->peer_events_ : owner_.peer_events_;
  }
  if (!peer || !events || events.get() != handler || (props && props->closing_)) return;
  std::shared_ptr<FileReceiver> receiver;
  std::filesystem::path default_directory;
  {
    std::lock_guard lock(sessions_mutex_);
    const auto closed = disconnected_.find(handler);
    if (stopping_ || (closed != disconnected_.end() &&
        (closed->second.count("") || closed->second.count(remote)))) return;
    auto& entry = receivers_[handler][remote];
    if (!entry.session) {
      entry.session = std::make_shared<FileReceiver>(); entry.handler = events;
      entry.default_directory = entry.session->OutputDir();
      entry.session->SetOnSendAck([peer, events, remote](const FileTransferAck& ack) {
        auto callback = events->EnterCallback();
        if (!callback) return -1;
        const auto bytes = EncodeFileTransferAck(ack);
        return SendReliableDataFrameToPeer(peer, bytes.data(), bytes.size(), kFileFeedbackStream,
                                           remote.c_str(), remote.size());
      });
    }
    receiver = entry.session;
    default_directory = entry.default_directory;
  }
  const auto configured = owner_.config_center_->GetFileTransferSavePath();
  receiver->SetOutputDir(configured.empty() ? default_directory : PathFromUtf8(configured));
  receiver->OnData(data, size);
}

void FileTransferManager::Connected(PeerEventHandler* handler, const std::string& remote) {
  std::lock_guard lock(sessions_mutex_);
  if (remote.empty()) disconnected_.erase(handler);
  else if (const auto it = disconnected_.find(handler); it != disconnected_.end()) {
    it->second.erase(remote);
    if (it->second.empty()) disconnected_.erase(it);
  }
}

void FileTransferManager::Cancel(PeerEventHandler* handler, const std::string& remote) {
  std::lock_guard lock(sessions_mutex_);
  disconnected_[handler].insert(remote);
  for (auto& [id, sender] : senders_)
    if (sender.handler.get() == handler && (remote.empty() || sender.remote == remote)) sender.session->Cancel();
  const auto it = receivers_.find(handler);
  if (it != receivers_.end()) {
    for (auto entry = it->second.begin(); entry != it->second.end();) {
      if (remote.empty() || entry->first == remote) {
        entry->second.session->Cancel(); entry = it->second.erase(entry);
      } else ++entry;
    }
    if (it->second.empty()) receivers_.erase(it);
  }
  wake_.notify_all();
}

void FileTransferManager::Poll() {
  std::vector<std::shared_ptr<FileReceiver>> receivers;
  {
    std::lock_guard lock(sessions_mutex_);
    for (const auto& [handler, peers] : receivers_)
      for (const auto& [remote, receiver] : peers) receivers.push_back(receiver.session);
  }
  for (const auto& receiver : receivers) receiver->Poll();
}

}  // namespace crossdesk
