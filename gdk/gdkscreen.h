/*
 * gdkscreen.h
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

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_SCREEN_H__
#define __GDK_SCREEN_H__

#include <cairo.h>
#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkdisplay.h>

G_BEGIN_DECLS

#define GDK_TYPE_SCREEN            (gdk_screen_get_type ())
#define GDK_SCREEN(object)         (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_SCREEN, GdkScreen))
#define GDK_IS_SCREEN(object)      (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_SCREEN))


GType        gdk_screen_get_type              (void) G_GNUC_CONST;

GdkVisual *  gdk_screen_get_system_visual     (GdkScreen   *screen);
GdkVisual *  gdk_screen_get_rgba_visual       (GdkScreen   *screen);
gboolean     gdk_screen_is_composited         (GdkScreen   *screen);

GdkWindow *  gdk_screen_get_root_window       (GdkScreen   *screen);
GdkDisplay * gdk_screen_get_display           (GdkScreen   *screen);
gint         gdk_screen_get_number            (GdkScreen   *screen);
gint         gdk_screen_get_width             (GdkScreen   *screen);
gint         gdk_screen_get_height            (GdkScreen   *screen);
gint         gdk_screen_get_width_mm          (GdkScreen   *screen);
gint         gdk_screen_get_height_mm         (GdkScreen   *screen);

GList *      gdk_screen_list_visuals          (GdkScreen   *screen);
GList *      gdk_screen_get_toplevel_windows  (GdkScreen   *screen);
gchar *      gdk_screen_make_display_name     (GdkScreen   *screen);

gint         gdk_screen_get_n_monitors        (GdkScreen    *screen);
gint         gdk_screen_get_primary_monitor   (GdkScreen    *screen);
void         gdk_screen_get_monitor_geometry  (GdkScreen    *screen,
                                               gint          monitor_num,
                                               GdkRectangle *dest);
GDK_AVAILABLE_IN_3_4
void         gdk_screen_get_monitor_workarea  (GdkScreen    *screen,
                                               gint          monitor_num,
                                               GdkRectangle *dest);

gint          gdk_screen_get_monitor_at_point  (GdkScreen *screen,
                                                gint       x,
                                                gint       y);
gint          gdk_screen_get_monitor_at_window (GdkScreen *screen,
                                                GdkWindow *window);
gint          gdk_screen_get_monitor_width_mm  (GdkScreen *screen,
                                                gint       monitor_num);
gint          gdk_screen_get_monitor_height_mm (GdkScreen *screen,
                                                gint       monitor_num);
gchar *       gdk_screen_get_monitor_plug_name (GdkScreen *screen,
                                                gint       monitor_num);

GdkScreen *gdk_screen_get_default (void);

gboolean   gdk_screen_get_setting (GdkScreen   *screen,
                                   const gchar *name,
                                   GValue      *value);

void                        gdk_screen_set_font_options (GdkScreen                  *screen,
                                                         const cairo_font_options_t *options);
const cairo_font_options_t *gdk_screen_get_font_options (GdkScreen                  *screen);

void    gdk_screen_set_resolution (GdkScreen *screen,
                                   gdouble    dpi);
gdouble gdk_screen_get_resolution (GdkScreen *screen);

GdkWindow *gdk_screen_get_active_window (GdkScreen *screen);
GList     *gdk_screen_get_window_stack  (GdkScreen *screen);

G_END_DECLS

#endif  /* __GDK_SCREEN_H__ */
