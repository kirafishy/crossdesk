#pragma once

#include <string>

namespace crossdesk {

// Starts the platform's native move loop for the window receiving the current
// left-mouse-down event. Returns false when no suitable native event exists.
bool StartNativeWindowDrag();

// Sets a white native titlebar and hides its zoom button in both main-window
// modes. DeskHub remains resizable. |slint_view| is its AppKit NSView*.
// Returns false while the native window or its titlebar controls are not ready.
bool ConfigureMainWindowChrome(void* slint_view, bool gallery = false);
// Winit's initial macOS position is a content origin, while subsequent moves
// use the outer frame. Return the logical titlebar inset for initial placement.
float MainWindowInitialPositionYOffset();
bool SetMainWindowFullscreen(bool fullscreen);
bool IsMainWindowFullscreen();
bool IsMainWindowActive();
// AppKit's animated zoom spins a nested run loop and cannot be called from a
// Slint timer. Restore maximization by setting the visible screen frame directly.
bool MaximizeMainWindowWithoutAnimation();
bool IsMainWindowMaximized();

// Configures live resize and replaces the stream window's native Space
// fullscreen action with an immediate, single-window fullscreen transition.
// |slint_view| is the current stream window's AppKit NSView*. Passing the
// identity explicitly prevents a closing window with the same title from
// being selected during a later connection.
bool ConfigureStreamWindowLiveResize(void* slint_view);

// Drops the native stream-window reference if it belongs to |slint_view|.
// This must run before the corresponding Slint component is destroyed.
void UnregisterStreamWindow(void* slint_view);

// Returns whether the configured stream window is currently the active AppKit
// key window.
bool IsStreamWindowActive();

// Enters or leaves the stream window's animation-free fullscreen mode.
bool SetStreamWindowFullscreen(bool fullscreen);
bool IsStreamWindowFullscreen();

// Opens an application-modal macOS file picker. The panel is made key before
// entering its modal loop so it receives keyboard input immediately.
std::string OpenNativeFileDialog(const std::string &title);

} // namespace crossdesk
