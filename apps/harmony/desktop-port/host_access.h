/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _HOST_ACCESS_H_
#define _HOST_ACCESS_H_

namespace crossdesk::harmony::desktop {
// Values come from AbilityKit's actual permission checks, never saved settings.
void SetHostPermissions(bool control_device, bool screen_recording);
}

#endif
