/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _WINDOW_STATE_H_
#define _WINDOW_STATE_H_

namespace slint { class Window; }
namespace crossdesk {
// GUI-thread query of the native window focus delivered by Harmony ArkUI.
bool IsHarmonyWindowActive(const slint::Window& window);
}

#endif
