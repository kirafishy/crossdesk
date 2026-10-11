/*
 * @Author: DI JUNKUN
 * @Date: 2026-09-25
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _CLIPBOARD_CONTROLLER_H_
#define _CLIPBOARD_CONTROLLER_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace crossdesk::gui_detail {
struct RemoteSession;
}

namespace crossdesk {

class GuiRuntime;

// Owns clipboard synchronization state and keeps feedback-loop prevention out
// of the main UI coordinator.
class ClipboardController {
public:
  explicit ClipboardController(GuiRuntime &owner);

  void SetEventType(uint32_t event_type);
  uint32_t event_type() const;

  void Initialize();
  void Shutdown();
  void QueueRemoteText(const char* data, size_t size,
                       const std::string& remote_id = {});
  void ApplyPendingRemoteText();
  void HandleLocalUpdate();

private:
  int SendToPeers(const std::string &text);

  GuiRuntime &owner_;
  uint32_t event_type_ = 0;
  std::atomic<bool> events_enabled_{false};
  std::mutex pending_mutex_;
  std::optional<std::string> pending_remote_text_;
  std::string pending_remote_id_;
  std::weak_ptr<gui_detail::RemoteSession> pending_session_;
  uint64_t pending_generation_ = 0;
  std::string last_text_;
};

} // namespace crossdesk

#endif // _CLIPBOARD_CONTROLLER_H_