/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-25
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#include "screen_lock.h"

#include <windows.h>

#include "rd_log.h"

namespace crossdesk {

bool LockScreen() {
  if (LockWorkStation()) return true;
  LOG_WARN("LockWorkStation failed: {}", GetLastError());
  return false;
}

}  // namespace crossdesk
