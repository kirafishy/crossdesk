/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-15
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _CURSOR_SHAPE_H_
#define _CURSOR_SHAPE_H_

#include <multimodalinput/oh_pointer_style.h>
#include <remote_cursor_shape.h>

namespace crossdesk::harmony::desktop {
inline RemoteCursorShape ShapeFromPointerStyle(Input_PointerStyle style) {
  using Shape = RemoteCursorShape;
  switch (style) {
    case EAST: case MIDDLE_BTN_EAST: return Shape::e_resize;
    case WEST: case MIDDLE_BTN_WEST: return Shape::w_resize;
    case SOUTH: case MIDDLE_BTN_SOUTH: return Shape::s_resize;
    case NORTH: case MIDDLE_BTN_NORTH: return Shape::n_resize;
    case WEST_EAST: case MIDDLE_BTN_EAST_WEST: return Shape::ew_resize;
    case NORTH_SOUTH: case MIDDLE_BTN_NORTH_SOUTH: return Shape::ns_resize;
    case NORTH_EAST: case MIDDLE_BTN_NORTH_EAST: return Shape::ne_resize;
    case NORTH_WEST: case MIDDLE_BTN_NORTH_WEST: return Shape::nw_resize;
    case SOUTH_EAST: case MIDDLE_BTN_SOUTH_EAST: return Shape::se_resize;
    case SOUTH_WEST: case MIDDLE_BTN_SOUTH_WEST: return Shape::sw_resize;
    case NORTH_EAST_SOUTH_WEST: return Shape::nesw_resize;
    case NORTH_WEST_SOUTH_EAST: return Shape::nwse_resize;
    case CROSS: case CURSOR_CROSS: case COLOR_SUCKER:
    case SCREENSHOT_CHOOSE: case SCREENSHOT_CURSOR: return Shape::crosshair;
    case CURSOR_COPY: return Shape::copy;
    case CURSOR_FORBID: return Shape::not_allowed;
    case HAND_GRABBING: return Shape::grabbing;
    case HAND_OPEN: return Shape::grab;
    case HAND_POINTING: return Shape::pointer;
    case HELP: return Shape::help;
    case MOVE: case MIDDLE_BTN_NORTH_SOUTH_WEST_EAST: return Shape::move;
    case RESIZE_LEFT_RIGHT: return Shape::col_resize;
    case RESIZE_UP_DOWN: return Shape::row_resize;
    case TEXT_CURSOR: case HORIZONTAL_TEXT_CURSOR: return Shape::text;
    case LOADING: return Shape::wait;
    case RUNNING: case RUNNING_LEFT: case RUNNING_RIGHT: return Shape::progress;
    // As on the other PC platforms, unsupported/custom artwork uses an arrow.
    default: return Shape::default_cursor;
  }
}
}

#endif
