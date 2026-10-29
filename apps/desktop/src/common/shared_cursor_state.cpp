#include "shared_cursor_state.h"

#include <remote_action.h>

#include <mutex>

namespace crossdesk {
namespace {

std::mutex g_cursor_state_mutex;
SharedCursorState g_cursor_state;
bool g_cursor_state_available = false;

}  // namespace

void PublishSharedCursorState(CursorRenderMode mode, RemoteCursorShape shape) {
  std::lock_guard<std::mutex> lock(g_cursor_state_mutex);
  ++g_cursor_state.generation;
  g_cursor_state.visible = mode == CursorRenderMode::separate;
  g_cursor_state.shape = g_cursor_state.visible ? shape : RemoteCursorShape::none;
  g_cursor_state.render_mode = mode;
  g_cursor_state.hidden_reason = mode == CursorRenderMode::hidden
      ? CursorHiddenReason::system_hidden
      : mode == CursorRenderMode::unknown ? CursorHiddenReason::sampling_failed
                                         : CursorHiddenReason::unspecified;
  g_cursor_state_available = true;
}

bool GetSharedCursorState(SharedCursorState* state) {
  if (!state) {
    return false;
  }
  std::lock_guard<std::mutex> lock(g_cursor_state_mutex);
  if (!g_cursor_state_available) {
    return false;
  }
  *state = g_cursor_state;
  return true;
}

void ClearSharedCursorState() {
  std::lock_guard<std::mutex> lock(g_cursor_state_mutex);
  ++g_cursor_state.generation;
  g_cursor_state.visible = false;
  g_cursor_state.shape = RemoteCursorShape::none;
  g_cursor_state.render_mode = CursorRenderMode::unknown;
  g_cursor_state.hidden_reason = CursorHiddenReason::sampling_failed;
  g_cursor_state_available = false;
}

}  // namespace crossdesk
