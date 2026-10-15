/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _HARMONY_TRAY_H_
#define _HARMONY_TRAY_H_

#include <functional>
#include <string>

namespace crossdesk {
// Constructed and invoked on the shared Slint event thread.
class HarmonyTray {
 public:
  HarmonyTray(std::function<void()> show, std::function<void()> hide,
              std::function<void()> settings, std::function<void()> exit,
              const std::string& tooltip, int language);
  ~HarmonyTray();
  bool MinimizeToTray();
  void RemoveTrayIcon();
  static void Dispatch(const std::string& action);
 private:
  std::function<void()> show_, hide_, settings_, exit_;
  bool active_ = false;
};
}

#endif
