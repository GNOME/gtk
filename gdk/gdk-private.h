#ifndef __GDK__PRIVATE_H__
#define __GDK__PRIVATE_H__

#include <gdk/gdk.h>
#include "gdk/gdkinternals.h"

/* Private API for use in GTK+ */

GdkDisplay *    gdk_display_open_default        (void);

gboolean        gdk_device_grab_info            (GdkDisplay  *display,
                                                 GdkDevice   *device,
                                                 GdkSurface  **grab_surface,
                                                 gboolean    *owner_events);

void            gdk_pre_parse                   (void);

void            gdk_surface_freeze_toplevel_updates      (GdkSurface *surface);
void            gdk_surface_thaw_toplevel_updates        (GdkSurface *surface);

guint32         gdk_display_get_last_seen_time  (GdkDisplay *display);

void gdk_display_set_double_click_time     (GdkDisplay   *display,
                                            guint         msec);
void gdk_display_set_double_click_distance (GdkDisplay   *display,
                                            guint         distance);
void gdk_display_set_cursor_theme          (GdkDisplay   *display,
                                            const char   *theme,
                                            int           size);
gboolean gdk_running_in_sandbox (void);
gboolean gdk_should_use_portal (void);

const gchar *   gdk_get_startup_notification_id (void);

PangoDirection gdk_unichar_direction (gunichar    ch);
PangoDirection gdk_find_base_dir     (const char *text,
                                      int         len);

void           gdk_surface_set_widget (GdkSurface *surface,
                                       gpointer    widget);
gpointer       gdk_surface_get_widget (GdkSurface *surface);

#endif /* __GDK__PRIVATE_H__ */
