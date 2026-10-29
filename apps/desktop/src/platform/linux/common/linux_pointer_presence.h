/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-29
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _LINUX_POINTER_PRESENCE_H_
#define _LINUX_POINTER_PRESENCE_H_

#include <memory>

namespace crossdesk {

// Owned by the cursor sampler. Only confirmed absence permits a remote
// locator; unavailable device information must preserve normal hiding.
class LinuxPointerPresence {
 public:
  LinuxPointerPresence();
  ~LinuxPointerPresence();

  bool NoPointingDevice(void* x11_display, bool wayland);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace crossdesk

#endif
