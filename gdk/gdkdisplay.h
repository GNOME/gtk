/*
 * gdkdisplay.h
 *
 * Copyright 2001 Sun Microsystems Inc.
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_DISPLAY_H__
#define __GDK_DISPLAY_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkevents.h>
#include <gdk/gdkdevicemanager.h>

G_BEGIN_DECLS

#define GDK_TYPE_DISPLAY              (gdk_display_get_type ())
#define GDK_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY, GdkDisplay))
#define GDK_IS_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY))
#ifndef GDK_DISABLE_DEPRECATED
#define GDK_DISPLAY_OBJECT(object)    GDK_DISPLAY(object)
#endif

GType       gdk_display_get_type (void) G_GNUC_CONST;
GdkDisplay *gdk_display_open                (const gchar *display_name);

const gchar * gdk_display_get_name         (GdkDisplay *display);

gint        gdk_display_get_n_screens      (GdkDisplay  *display);
GdkScreen * gdk_display_get_screen         (GdkDisplay  *display,
                                            gint         screen_num);
GdkScreen * gdk_display_get_default_screen (GdkDisplay  *display);

#ifndef GDK_MULTIDEVICE_SAFE
GDK_DEPRECATED_IN_3_0_FOR(gdk_device_ungrab)
void        gdk_display_pointer_ungrab     (GdkDisplay  *display,
                                            guint32      time_);
GDK_DEPRECATED_IN_3_0_FOR(gdk_device_ungrab)
void        gdk_display_keyboard_ungrab    (GdkDisplay  *display,
                                            guint32      time_);
GDK_DEPRECATED_IN_3_0_FOR(gdk_display_device_is_grabbed)
gboolean    gdk_display_pointer_is_grabbed (GdkDisplay  *display);
#endif /* GDK_MULTIDEVICE_SAFE */

gboolean    gdk_display_device_is_grabbed  (GdkDisplay  *display,
                                            GdkDevice   *device);
void        gdk_display_beep               (GdkDisplay  *display);
void        gdk_display_sync               (GdkDisplay  *display);
void        gdk_display_flush              (GdkDisplay  *display);

void        gdk_display_close                  (GdkDisplay  *display);
gboolean    gdk_display_is_closed          (GdkDisplay  *display);

GDK_DEPRECATED_IN_3_0_FOR(gdk_device_manager_list_devices)
GList *     gdk_display_list_devices       (GdkDisplay  *display);

GdkEvent* gdk_display_get_event  (GdkDisplay     *display);
GdkEvent* gdk_display_peek_event (GdkDisplay     *display);
void      gdk_display_put_event  (GdkDisplay     *display,
                                  const GdkEvent *event);
gboolean  gdk_display_has_pending (GdkDisplay  *display);

void gdk_display_set_double_click_time     (GdkDisplay   *display,
                                            guint         msec);
void gdk_display_set_double_click_distance (GdkDisplay   *display,
                                            guint         distance);

GdkDisplay *gdk_display_get_default (void);

#ifndef GDK_MULTIDEVICE_SAFE
GDK_DEPRECATED_IN_3_0_FOR(gdk_device_get_position)
void             gdk_display_get_pointer           (GdkDisplay             *display,
                                                    GdkScreen             **screen,
                                                    gint                   *x,
                                                    gint                   *y,
                                                    GdkModifierType        *mask);
GDK_DEPRECATED_IN_3_0_FOR(gdk_device_get_window_at_position)
GdkWindow *      gdk_display_get_window_at_pointer (GdkDisplay             *display,
                                                    gint                   *win_x,
                                                    gint                   *win_y);
GDK_DEPRECATED_IN_3_0_FOR(gdk_device_warp)
void             gdk_display_warp_pointer          (GdkDisplay             *display,
                                                    GdkScreen              *screen,
                                                    gint                   x,
                                                    gint                   y);
#endif /* GDK_MULTIDEVICE_SAFE */

GdkDisplay *gdk_display_open_default_libgtk_only (void);

gboolean gdk_display_supports_cursor_alpha     (GdkDisplay    *display);
gboolean gdk_display_supports_cursor_color     (GdkDisplay    *display);
guint    gdk_display_get_default_cursor_size   (GdkDisplay    *display);
void     gdk_display_get_maximal_cursor_size   (GdkDisplay    *display,
                                                guint         *width,
                                                guint         *height);

GdkWindow *gdk_display_get_default_group       (GdkDisplay *display); 

gboolean gdk_display_supports_selection_notification (GdkDisplay *display);
gboolean gdk_display_request_selection_notification  (GdkDisplay *display,
                                                      GdkAtom     selection);

gboolean gdk_display_supports_clipboard_persistence (GdkDisplay    *display);
void     gdk_display_store_clipboard                (GdkDisplay    *display,
                                                     GdkWindow     *clipboard_window,
                                                     guint32        time_,
                                                     const GdkAtom *targets,
                                                     gint           n_targets);

gboolean gdk_display_supports_shapes           (GdkDisplay    *display);
gboolean gdk_display_supports_input_shapes     (GdkDisplay    *display);
gboolean gdk_display_supports_composite        (GdkDisplay    *display);
void     gdk_display_notify_startup_complete   (GdkDisplay    *display,
                                                const gchar   *startup_id);

GdkDeviceManager * gdk_display_get_device_manager (GdkDisplay *display);

GdkAppLaunchContext *gdk_display_get_app_launch_context (GdkDisplay *display);

G_END_DECLS

#endif  /* __GDK_DISPLAY_H__ */
