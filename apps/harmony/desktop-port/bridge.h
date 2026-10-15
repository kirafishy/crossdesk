/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-14
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _BRIDGE_H_
#define _BRIDGE_H_

#include <nlohmann/json.hpp>
#include <string>
namespace crossdesk::harmony::desktop {
using Json = nlohmann::json;
void Emit(Json command);
// Calls a platform service on the ArkTS thread. The Slint thread can wait while
// ArkUI keeps pumping window and picker events; shutdown cancels pending calls.
Json Request(Json command);
Json Environment();
}

#endif
