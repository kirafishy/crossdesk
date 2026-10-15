/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-09
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _MOBILE_FILE_TRANSFER_SESSION_H_
#define _MOBILE_FILE_TRANSFER_SESSION_H_

#include "file_transfer_core.h"

namespace crossdesk::mobile_controller {

// Mobile pickers provide disposable copies and expose completed files through
// their existing per-transfer directories. The transfer engine is shared with PC.
class FileTransferSession : public file_transfer::Session {
 public:
  FileTransferSession(Send send, Progress progress)
      : Session(std::move(send), std::move(progress), {true, true, 4}) {}
};

}  // namespace crossdesk::mobile_controller

#endif
