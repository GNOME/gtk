#include "config.h"
#include "gdk-private.h"

GdkPrivateVTable *
gdk__private__ (void)
{
  static GdkPrivateVTable table = {
    gdk_device_grab_info,
    gdk_display_open_default,
    gdk_add_option_entries,
    gdk_pre_parse,
    gdk_gl_get_flags,
    gdk_gl_set_flags,
    gdk_window_freeze_toplevel_updates,
    gdk_window_thaw_toplevel_updates,
    gdk_display_get_rendering_mode,
    gdk_display_set_rendering_mode,
    gdk_display_get_debug_updates,
    gdk_display_set_debug_updates,
    gdk_get_desktop_startup_id,
    gdk_get_desktop_autostart_id,
  };

  return &table;
}
