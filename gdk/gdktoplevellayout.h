/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2020 Red Hat
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
 *
 */

#ifndef __GDK_TOPLEVEL_LAYOUT_H__
#define __GDK_TOPLEVEL_LAYOUT_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkmonitor.h>
#include <gdk/gdksurface.h>

G_BEGIN_DECLS

/**
 * GdkTopLevelLayout:
 */
typedef struct _GdkToplevelLayout GdkToplevelLayout;

#define GDK_TYPE_TOPLEVEL_LAYOUT (gdk_toplevel_layout_get_type ())

GDK_AVAILABLE_IN_ALL
GType                   gdk_toplevel_layout_get_type    (void);

GDK_AVAILABLE_IN_ALL
GdkToplevelLayout *     gdk_toplevel_layout_new         (int         min_width,
                                                         int         min_height,
                                                         int         max_width,
                                                         int         max_height);

GDK_AVAILABLE_IN_ALL
GdkToplevelLayout *     gdk_toplevel_layout_ref         (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_unref       (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
GdkToplevelLayout *     gdk_toplevel_layout_copy        (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_equal       (GdkToplevelLayout *layout,
                                                         GdkToplevelLayout *other);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_maximized (GdkToplevelLayout *layout,
                                                           gboolean           maximized);
GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_fullscreen (GdkToplevelLayout *layout,
                                                            gboolean           fullscreen,
                                                            GdkMonitor        *monitor);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_get_min_size (GdkToplevelLayout *layout,
                                                          int               *width,
                                                          int               *height);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_get_max_size (GdkToplevelLayout *layout,
                                                          int               *width,
                                                          int               *height);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_maximized (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_fullscreen (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
GdkMonitor *            gdk_toplevel_layout_get_fullscreen_monitor (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_modal (GdkToplevelLayout *layout,
                                                       gboolean           modal);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_modal (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_type_hint (GdkToplevelLayout *layout,
                                                           GdkSurfaceTypeHint hint);

GDK_AVAILABLE_IN_ALL
GdkSurfaceTypeHint      gdk_toplevel_layout_get_type_hint (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_raise (GdkToplevelLayout *layout,
                                                       gboolean           raise);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_raise (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_lower (GdkToplevelLayout *layout,
                                                       gboolean           lower);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_lower (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_minimized (GdkToplevelLayout *layout,
                                                           gboolean           minimized);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_minimized (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_stick (GdkToplevelLayout *layout,
                                                       gboolean           stick);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_stick (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_keep_above (GdkToplevelLayout *layout,
                                                            gboolean           keep_above);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_keep_above (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_keep_below (GdkToplevelLayout *layout,
                                                            gboolean           keep_below);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_keep_below (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_decorations (GdkToplevelLayout *layout,
                                                             GdkWMDecoration    decorations);

GDK_AVAILABLE_IN_ALL
GdkWMDecoration         gdk_toplevel_layout_get_decorations (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_functions   (GdkToplevelLayout *layout,
                                                             GdkWMFunction      functions);

GDK_AVAILABLE_IN_ALL
GdkWMFunction           gdk_toplevel_layout_get_functions   (GdkToplevelLayout *layout);

G_END_DECLS

#endif /* __GDK_TOPLEVEL_LAYOUT_H__ */
