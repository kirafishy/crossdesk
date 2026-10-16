/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-16
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _RDP_SESSION_HANDOFF_H_
#define _RDP_SESSION_HANDOFF_H_

#include <Windows.h>

namespace crossdesk {
inline constexpr char kRdpHandoffCommand[] = "resume-rdp-session";

struct RdpHandoffResult {
  const char* error = nullptr;
  DWORD error_code = 0;
  bool already_console = false;
};

// The session comes from GetNamedPipeClientSessionId, never request payloads.
RdpHandoffResult HandoffDisconnectedRdpSession(HANDLE pipe, DWORD client_session,
                                              bool observed_rdp_session);
bool IsCurrentProcessSessionDisconnected();
}  // namespace crossdesk

#endif