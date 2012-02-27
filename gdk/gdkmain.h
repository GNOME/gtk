/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_MAIN_H__
#define __GDK_MAIN_H__

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>

G_BEGIN_DECLS


/* Initialization, exit and events
 */

#define GDK_PRIORITY_EVENTS (G_PRIORITY_DEFAULT)

void                  gdk_parse_args                      (gint           *argc,
                                                           gchar        ***argv);
void                  gdk_init                            (gint           *argc,
                                                           gchar        ***argv);
gboolean              gdk_init_check                      (gint           *argc,
                                                           gchar        ***argv);
void                  gdk_add_option_entries_libgtk_only  (GOptionGroup   *group);
void                  gdk_pre_parse_libgtk_only           (void);

const gchar *         gdk_get_program_class               (void);
void                  gdk_set_program_class               (const gchar    *program_class);

void                  gdk_notify_startup_complete         (void);
void                  gdk_notify_startup_complete_with_id (const gchar* startup_id);

/* Push and pop error handlers for X errors
 */
void                           gdk_error_trap_push        (void);
/* warn unused because you could use pop_ignored otherwise */
G_GNUC_WARN_UNUSED_RESULT gint gdk_error_trap_pop         (void);
void                           gdk_error_trap_pop_ignored (void);


const gchar *         gdk_get_display_arg_name (void);

/**
 * gdk_get_display:
 *
 * Gets the name of the display, which usually comes from the
 * <envar>DISPLAY</envar> environment variable or the
 * <option>--display</option> command line option.
 *
 * Returns: the name of the display.
 */
gchar*        gdk_get_display        (void);

#ifndef GDK_MULTIDEVICE_SAFE
GDK_DEPRECATED_IN_3_0_FOR(gdk_device_grab)
GdkGrabStatus gdk_pointer_grab       (GdkWindow    *window,
                                      gboolean      owner_events,
                                      GdkEventMask  event_mask,
                                      GdkWindow    *confine_to,
                                      GdkCursor    *cursor,
                                      guint32       time_);
GDK_DEPRECATED_IN_3_0_FOR(gdk_device_grab)
GdkGrabStatus gdk_keyboard_grab      (GdkWindow    *window,
                                      gboolean      owner_events,
                                      guint32       time_);
#endif /* GDK_MULTIDEVICE_SAFE */

#ifndef GDK_MULTIHEAD_SAFE

#ifndef GDK_MULTIDEVICE_SAFE
GDK_DEPRECATED_IN_3_0_FOR(gdk_device_ungrab)
void          gdk_pointer_ungrab     (guint32       time_);
GDK_DEPRECATED_IN_3_0_FOR(gdk_device_ungrab)
void          gdk_keyboard_ungrab    (guint32       time_);
GDK_DEPRECATED_IN_3_0_FOR(gdk_display_device_is_grabbed)
gboolean      gdk_pointer_is_grabbed (void);
#endif /* GDK_MULTIDEVICE_SAFE */

gint gdk_screen_width  (void) G_GNUC_CONST;
gint gdk_screen_height (void) G_GNUC_CONST;

gint gdk_screen_width_mm  (void) G_GNUC_CONST;
gint gdk_screen_height_mm (void) G_GNUC_CONST;

void gdk_set_double_click_time (guint msec);

void gdk_beep (void);

#endif /* GDK_MULTIHEAD_SAFE */

void gdk_flush (void);

void gdk_disable_multidevice (void);

G_END_DECLS

#endif /* __GDK_MAIN_H__ */
