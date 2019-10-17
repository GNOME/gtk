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
#include <gdk/gdkseat.h>
#include <gdk/gdkmonitor.h>

G_BEGIN_DECLS

#define GDK_TYPE_DISPLAY              (gdk_display_get_type ())
#define GDK_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_DISPLAY, GdkDisplay))
#define GDK_IS_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_DISPLAY))

GDK_AVAILABLE_IN_ALL
GType       gdk_display_get_type (void) G_GNUC_CONST;
GDK_AVAILABLE_IN_ALL
GdkDisplay *gdk_display_open                (const gchar *display_name);

GDK_AVAILABLE_IN_ALL
const gchar * gdk_display_get_name         (GdkDisplay *display);

GDK_AVAILABLE_IN_ALL
gboolean    gdk_display_device_is_grabbed  (GdkDisplay  *display,
                                            GdkDevice   *device);
GDK_AVAILABLE_IN_ALL
void        gdk_display_beep               (GdkDisplay  *display);
GDK_AVAILABLE_IN_ALL
void        gdk_display_sync               (GdkDisplay  *display);
GDK_AVAILABLE_IN_ALL
void        gdk_display_flush              (GdkDisplay  *display);

GDK_AVAILABLE_IN_ALL
void        gdk_display_close                  (GdkDisplay  *display);
GDK_AVAILABLE_IN_ALL
gboolean    gdk_display_is_closed          (GdkDisplay  *display);

GDK_AVAILABLE_IN_ALL
gboolean    gdk_display_is_composited      (GdkDisplay  *display);
GDK_AVAILABLE_IN_ALL
gboolean    gdk_display_is_rgba            (GdkDisplay  *display);

GDK_AVAILABLE_IN_ALL
GdkEvent* gdk_display_get_event  (GdkDisplay     *display);
GDK_AVAILABLE_IN_ALL
GdkEvent* gdk_display_peek_event (GdkDisplay     *display);
GDK_AVAILABLE_IN_ALL
void      gdk_display_put_event  (GdkDisplay     *display,
                                  const GdkEvent *event);
GDK_AVAILABLE_IN_ALL
gboolean  gdk_display_has_pending (GdkDisplay  *display);

GDK_AVAILABLE_IN_ALL
GdkDisplay *gdk_display_get_default (void);

GDK_AVAILABLE_IN_ALL
GdkSurface *gdk_display_get_default_group       (GdkDisplay *display); 

GDK_AVAILABLE_IN_ALL
GdkClipboard *          gdk_display_get_clipboard               (GdkDisplay     *display);
GDK_AVAILABLE_IN_ALL
GdkClipboard *          gdk_display_get_primary_clipboard       (GdkDisplay     *display);

GDK_AVAILABLE_IN_ALL
gboolean gdk_display_supports_shapes           (GdkDisplay    *display);
GDK_AVAILABLE_IN_ALL
gboolean gdk_display_supports_input_shapes     (GdkDisplay    *display);
GDK_AVAILABLE_IN_ALL
void     gdk_display_notify_startup_complete   (GdkDisplay    *display,
                                                const gchar   *startup_id);
GDK_AVAILABLE_IN_ALL
const gchar * gdk_display_get_startup_notification_id (GdkDisplay *display);

GDK_AVAILABLE_IN_ALL
GdkAppLaunchContext *gdk_display_get_app_launch_context (GdkDisplay *display);

GDK_AVAILABLE_IN_ALL
GdkSeat * gdk_display_get_default_seat (GdkDisplay *display);

GDK_AVAILABLE_IN_ALL
GList   * gdk_display_list_seats       (GdkDisplay *display);

GDK_AVAILABLE_IN_ALL
int          gdk_display_get_n_monitors        (GdkDisplay *display);
GDK_AVAILABLE_IN_ALL
GdkMonitor * gdk_display_get_monitor           (GdkDisplay *display,
                                                int         monitor_num);
GDK_AVAILABLE_IN_ALL
GdkMonitor * gdk_display_get_primary_monitor   (GdkDisplay *display);
GDK_AVAILABLE_IN_ALL
GdkMonitor * gdk_display_get_monitor_at_surface (GdkDisplay *display,
                                                GdkSurface  *surface);

GDK_AVAILABLE_IN_ALL
GdkKeymap *  gdk_display_get_keymap  (GdkDisplay *display);

GDK_AVAILABLE_IN_ALL
gboolean     gdk_display_get_setting (GdkDisplay *display,
                                      const char *name,
                                      GValue     *value);



G_END_DECLS

#endif  /* __GDK_DISPLAY_H__ */
