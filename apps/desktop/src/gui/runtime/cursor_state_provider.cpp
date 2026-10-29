#include "runtime/cursor_state_provider.h"

#include <remote_action.h>

#if defined(_WIN32)

#include <windows.h>

#include "platform/windows/input/windows_cursor_state.h"
#include "platform/windows/screen_capturer/captured_cursor_state.h"
#include "platform/windows/screen_capturer/dxgi_cursor_state.h"
#include "platform/windows/screen_capturer/secure_desktop_cursor_state.h"
#include "runtime/cursor_position.h"

namespace crossdesk {

struct CursorStateProvider::Impl {
  WindowsCursorState cursor;
};

CursorStateProvider::CursorStateProvider() : impl_(std::make_unique<Impl>()) {}
CursorStateProvider::~CursorStateProvider() = default;

bool CursorStateProvider::Sample(const std::vector<DisplayInfo>& displays,
                                 int preferred_display, CursorState* state) {
  if (!state) return false;

  SecureDesktopCursorSnapshot secure_cursor{};
  if (SharedSecureDesktopCursorState().Get(&secure_cursor)) {
    if (!secure_cursor.valid) {
      state->hidden_reason = CursorHiddenReason::secure_desktop_pending;
      return false;
    }
    state->seq = 0;
    NormalizeCursorPosition(secure_cursor.x, secure_cursor.y, displays,
                            preferred_display, state);
    state->visible = secure_cursor.visible != 0;
    state->render_mode = static_cast<CursorRenderMode>(secure_cursor.render_mode);
    state->hidden_reason =
        static_cast<CursorHiddenReason>(secure_cursor.hidden_reason);
    state->shape = state->visible
                       ? static_cast<RemoteCursorShape>(secure_cursor.shape)
                       : RemoteCursorShape::none;
    return true;
  }

  CURSORINFO info{};
  info.cbSize = sizeof(info);
  if (!GetCursorInfo(&info)) return false;

  state->seq = 0;
  NormalizeCursorPosition(info.ptScreenPos.x, info.ptScreenPos.y, displays,
                          preferred_display, state);
  void* cursor_monitor = state->position_valid
                             ? displays[state->display_id].handle
                             : nullptr;
  const bool embedded = SharedCapturedCursorState().IsEmbedded(cursor_monitor) ||
      ((info.flags & CURSOR_SHOWING) != 0 &&
       !SharedDxgiCursorState().ShouldDrawCursor(true, cursor_monitor));
  impl_->cursor.Sample(info, embedded, state);
  return true;
}

}  // namespace crossdesk

#elif (defined(__linux__) && !defined(__OHOS__)) && !defined(__APPLE__)

#include <X11/Xlib.h>
#include <X11/extensions/Xfixes.h>

#include <string>

#include "linux_cursor_shape.h"
#include "linux_pointer_presence.h"
#include "platform.h"
#include "shared_cursor_state.h"
#include "runtime/cursor_position.h"

namespace crossdesk {
namespace {

bool CursorHasVisiblePixel(const XFixesCursorImage& image) {
  const size_t pixel_count =
      static_cast<size_t>(image.width) * static_cast<size_t>(image.height);
  for (size_t index = 0; index < pixel_count; ++index) {
    if (((image.pixels[index] >> 24U) & 0xffU) != 0) return true;
  }
  return false;
}

}  // namespace

struct CursorStateProvider::Impl {
  const bool wayland = IsWaylandSession();
  Display* display = XOpenDisplay(nullptr);
  LinuxPointerPresence pointer_presence;

  ~Impl() {
    if (display) XCloseDisplay(display);
  }
};

CursorStateProvider::CursorStateProvider() : impl_(std::make_unique<Impl>()) {}
CursorStateProvider::~CursorStateProvider() = default;

bool CursorStateProvider::Sample(const std::vector<DisplayInfo>& displays,
                                 int preferred_display, CursorState* state) {
  if (!state || !impl_) return false;
  *state = {};
  ResetCursorPosition(state);

  SharedCursorState shared{};
  if (impl_->wayland && GetSharedCursorState(&shared)) {
    // XWayland only knows about X11 surfaces, not the captured Wayland
    // desktop. Missing native metadata must remain an unavailable sample.
    state->visible = shared.visible;
    state->shape = shared.shape;
    state->render_mode = shared.render_mode;
    state->hidden_reason = shared.hidden_reason;
    if (state->render_mode == CursorRenderMode::hidden &&
        impl_->pointer_presence.NoPointingDevice(nullptr, true)) {
      state->hidden_reason = CursorHiddenReason::no_pointing_device;
    }
    return state->render_mode != CursorRenderMode::unknown;
  }

  // Linux may use X11 capture even in a Wayland session. Only an active
  // portal publishes shared state; preserve X11 sampling for that backend.
  if (!impl_->display) return false;

  XFixesCursorImage* image = XFixesGetCursorImage(impl_->display);
  if (!image) return false;

  const std::string name = image->name ? image->name : "";

  state->seq = 0;
  state->visible = CursorHasVisiblePixel(*image);
  state->shape = state->visible ? ShapeFromLinuxCursorName(name)
                                : RemoteCursorShape::none;
  state->render_mode = state->visible ? CursorRenderMode::separate
                                      : CursorRenderMode::hidden;
  state->hidden_reason = state->visible ? CursorHiddenReason::unspecified
      : impl_->pointer_presence.NoPointingDevice(impl_->display, false)
          ? CursorHiddenReason::no_pointing_device
          : CursorHiddenReason::system_hidden;
  NormalizeCursorPosition(image->x, image->y, displays, preferred_display,
                          state);
  XFree(image);
  return true;
}

}  // namespace crossdesk

#endif
