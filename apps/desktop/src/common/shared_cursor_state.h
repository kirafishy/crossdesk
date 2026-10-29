#ifndef CROSSDESK_COMMON_SHARED_CURSOR_STATE_H_
#define CROSSDESK_COMMON_SHARED_CURSOR_STATE_H_

#include <cstdint>

#include <remote_action.h>

namespace crossdesk {

struct SharedCursorState {
  uint64_t generation = 0;
  bool visible = false;
  RemoteCursorShape shape = RemoteCursorShape::default_cursor;
  CursorRenderMode render_mode = CursorRenderMode::unknown;
  CursorHiddenReason hidden_reason = CursorHiddenReason::sampling_failed;
};

void PublishSharedCursorState(CursorRenderMode mode, RemoteCursorShape shape);
bool GetSharedCursorState(SharedCursorState* state);
void ClearSharedCursorState();

}  // namespace crossdesk

#endif  // CROSSDESK_COMMON_SHARED_CURSOR_STATE_H_
