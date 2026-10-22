/*
 * @Author: DI JUNKUN
 * @Date: 2026-09-19
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _WINDOWS_INPUT_INJECTOR_H_
#define _WINDOWS_INPUT_INJECTOR_H_

#include <Windows.h>

namespace crossdesk {

// Opt in only on the remote input worker. UI/hook callers keep using the
// fallback worker. Desktop attachment is still checked for each injection.
class ScopedUserDesktopInputThread {
 public:
  ScopedUserDesktopInputThread() { ++depth_; }
  ~ScopedUserDesktopInputThread() { --depth_; }
  ScopedUserDesktopInputThread(const ScopedUserDesktopInputThread&) = delete;
  ScopedUserDesktopInputThread& operator=(const ScopedUserDesktopInputThread&) =
      delete;
  static bool IsCurrent() { return depth_ != 0; }
 private:
  inline static thread_local unsigned depth_ = 0;
};

// Executes directly on an opted-in worker, or on the fallback input worker,
// attached to this process's current user input desktop. Returns a SendInput
// count and preserves the failure code in the calling thread's last-error value.
UINT SendInputOnUserDesktop(const INPUT& input);

// Synchronously injects one ordered batch. The caller owns the buffer until
// this function returns, including while the worker is using it.
UINT SendInputOnUserDesktop(INPUT* inputs, UINT count);

}  // namespace crossdesk

#endif
