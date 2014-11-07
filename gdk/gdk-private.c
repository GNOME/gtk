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
  };

  return &table;
}
