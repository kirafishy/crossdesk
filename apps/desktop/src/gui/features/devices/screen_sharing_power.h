/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-24
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _SCREEN_SHARING_POWER_H_
#define _SCREEN_SHARING_POWER_H_

#include <memory>

namespace crossdesk {

// Owns temporary display/system idle-sleep inhibitors for the shared capture
// source. Call on the UI thread; repeated updates do not acquire extra requests.
class ScreenSharingPower {
 public:
  ScreenSharingPower();
  ~ScreenSharingPower();
  ScreenSharingPower(const ScreenSharingPower&) = delete;
  ScreenSharingPower& operator=(const ScreenSharingPower&) = delete;

  void SetSharing(bool sharing);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
  bool sharing_ = false;
};

}  // namespace crossdesk

#endif
