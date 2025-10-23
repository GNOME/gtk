#pragma once

#include "gdk/gdktypes.h"

/* Private API for use in GTK+ */

void            gdk_pre_parse                   (void);
gboolean        gdk_is_initialized              (void);

#define gdk_ensure_initialized() \
G_STMT_START { \
  if (!gdk_is_initialized ()) \
    g_error ("%s() was called before gtk_init()", G_STRFUNC); \
} G_STMT_END

gboolean gdk_running_in_sandbox (void);
void     gdk_disable_portals    (void);
void     gdk_set_portals_app_id (const char *app_id);

gboolean gdk_display_should_use_portal (GdkDisplay *display,
                                        const char *portal_interface,
                                        guint       min_version);

const char *   gdk_get_startup_notification_id (void);

PangoDirection gdk_unichar_direction (gunichar    ch) G_GNUC_CONST;
PangoDirection gdk_find_base_dir     (const char *text,
                                      int         len);

void gdk_source_set_static_name_by_id (guint       tag,
                                       const char *name);

#ifndef I_
#define I_(string) g_intern_static_string (string)
#endif
