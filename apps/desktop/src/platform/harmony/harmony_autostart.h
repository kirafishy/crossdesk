/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _HARMONY_AUTOSTART_H_
#define _HARMONY_AUTOSTART_H_

namespace crossdesk {
enum class HarmonyAutostartStatus { checking, enabled, disabled, unsupported, failed };

// ArkTS publishes system state; the GUI reads it without waiting on a service.
HarmonyAutostartStatus GetHarmonyAutostartStatus();
void SetHarmonyAutostartStatus(HarmonyAutostartStatus status);
void RefreshHarmonyAutostartStatus();
}

#endif
