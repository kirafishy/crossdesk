/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-22
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _SHUTDOWN_SESSION_GUARD_H_
#define _SHUTDOWN_SESSION_GUARD_H_

#include <Windows.h>

#include <atomic>
#include <memory>
#include <string>
#include <string_view>

namespace crossdesk {

// Owned and pumped by the UI thread. The transport may revoke the shared
// connection flag from another thread, including between UI ticks.
class ShutdownSessionGuard {
 public:
  explicit ShutdownSessionGuard(std::shared_ptr<std::atomic<bool>> connected);
  ~ShutdownSessionGuard();
  ShutdownSessionGuard(const ShutdownSessionGuard&) = delete;
  ShutdownSessionGuard& operator=(const ShutdownSessionGuard&) = delete;

  bool Update(std::wstring_view reason);

 private:
  bool Start();
  bool SyncReason();
  bool ClearReason();
  bool ReportError(const char* operation);
  static LRESULT CALLBACK WindowProc(HWND window, UINT message,
                                    WPARAM wparam, LPARAM lparam);

  std::shared_ptr<std::atomic<bool>> connected_;
  HWND window_ = nullptr;
  std::wstring reason_;
  bool reason_registered_ = false;
  bool shutdown_allowed_ = false;
  ULONGLONG last_error_tick_ = 0;
};

}  // namespace crossdesk

#endif
