#pragma once

#include <glib.h>

GSource *               gdk_win32_message_source_new            (void);

guint                   gdk_win32_message_source_add            (GMainContext   *context);