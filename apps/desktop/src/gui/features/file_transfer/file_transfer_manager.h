#ifndef CROSSDESK_GUI_FILE_TRANSFER_MANAGER_H_
#define CROSSDESK_GUI_FILE_TRANSFER_MANAGER_H_

#include <atomic>
#include <condition_variable>
#include <future>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "file_transfer.h"
#include "runtime/remote_session.h"

namespace crossdesk {

class GuiRuntime;
class PeerEventHandler;

class FileTransferManager {
 public:
  using FileTransferState = gui_detail::FileTransferState;
  using RemoteSession = gui_detail::RemoteSession;
  explicit FileTransferManager(GuiRuntime& owner);
  ~FileTransferManager();
  FileTransferState& global_state();
  FileTransferState& state_for(const std::shared_ptr<RemoteSession>& props);
  void ProcessSelectedFile(const std::string& path, const std::shared_ptr<RemoteSession>& props,
                           const std::string& file_label, const std::string& remote_id = "");
  void HandleAck(const char* data, size_t size, PeerEventHandler* handler, const std::string& remote);
  void Receive(const char* data, size_t size, PeerEventHandler* handler, const std::string& remote);
  void Connected(PeerEventHandler* handler, const std::string& remote);
  void Cancel(PeerEventHandler* handler, const std::string& remote = "");
  void Poll();
  void Shutdown();

 private:
  struct Sender {
    std::shared_ptr<file_transfer::Session> session;
    std::shared_ptr<PeerEventHandler> handler;
    std::string remote;
  };
  struct Receiver {
    std::shared_ptr<FileReceiver> session;
    std::shared_ptr<PeerEventHandler> handler;
    std::filesystem::path default_directory;
  };
  void ProcessQueue(std::shared_ptr<RemoteSession> props);
  void UpdateProgress(FileTransferState& state, const std::shared_ptr<RemoteSession>& props,
                      const file_transfer::SendState& progress);
  std::mutex workers_mutex_, sessions_mutex_, wake_mutex_;
  std::condition_variable wake_;
  std::vector<std::future<void>> workers_;
  std::atomic<bool> stopping_{false};
  std::unordered_map<uint32_t, Sender> senders_;
  std::unordered_map<PeerEventHandler*, std::unordered_map<std::string, Receiver>> receivers_;
  std::unordered_map<PeerEventHandler*, std::set<std::string>> disconnected_;
  GuiRuntime& owner_;
  FileTransferState global_state_;
};

}  // namespace crossdesk

#endif
