/*
 * @Author: DI JUNKUN
 * @Date: 2026-10-25
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#include "screen_lock.h"

#include <dbus/dbus.h>

#include "rd_log.h"

namespace crossdesk {

bool LockScreen() {
  dbus_threads_init_default();
  DBusError error;
  dbus_error_init(&error);
  DBusConnection* connection = dbus_bus_get_private(DBUS_BUS_SESSION, &error);
  if (!connection) {
    LOG_WARN("Cannot connect to screen lock service: {}",
             error.message ? error.message : "session bus unavailable");
    dbus_error_free(&error);
    return false;
  }
  dbus_connection_set_exit_on_disconnect(connection, false);

  // Address the current desktop's session bus, including headless desktops;
  // never lock every logged-in user or an unrelated system login session.
  struct Service { const char* name; const char* path; };
  constexpr Service services[] = {
      {"org.freedesktop.ScreenSaver", "/ScreenSaver"},
      {"org.gnome.ScreenSaver", "/org/gnome/ScreenSaver"},
      {"org.cinnamon.ScreenSaver", "/org/cinnamon/ScreenSaver"},
      {"org.mate.ScreenSaver", "/org/mate/ScreenSaver"},
      {"org.xfce.ScreenSaver", "/org/xfce/ScreenSaver"},
  };
  bool accepted = false;
  for (const auto& service : services) {
    DBusMessage* message = dbus_message_new_method_call(
        service.name, service.path, service.name, "Lock");
    if (!message) break;
    dbus_message_set_auto_start(message, false);
    DBusMessage* reply = dbus_connection_send_with_reply_and_block(
        connection, message, 1000, &error);
    dbus_message_unref(message);
    if (reply) {
      accepted = dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_METHOD_RETURN;
      dbus_message_unref(reply);
    }
    dbus_error_free(&error);
    if (accepted) break;
  }
  dbus_connection_close(connection);
  dbus_connection_unref(connection);
  if (!accepted) LOG_WARN("No desktop screen lock service accepted the request");
  return accepted;
}

}  // namespace crossdesk
