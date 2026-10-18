/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-17
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _WAYLAND_ACTIVATION_H_
#define _WAYLAND_ACTIVATION_H_

#include <string>

namespace crossdesk::platform {

// Uses Slint's native handles and xdg-activation-v1 when the compositor offers
// it. A successful request still leaves the focus decision to the compositor.
bool ActivateWaylandWindow(void* display, void* surface,
                           const std::string& activation_token);

}  // namespace crossdesk::platform

#endif
