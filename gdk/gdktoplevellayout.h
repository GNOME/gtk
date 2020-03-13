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

#if !defined(__GDK_H_INSIDE__) && !defined(GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkmonitor.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkversionmacros.h>

G_BEGIN_DECLS

/**
 * GdkTopLevelLayout:
 */
typedef struct _GdkToplevelLayout GdkToplevelLayout;

#define GDK_TYPE_TOPLEVEL_LAYOUT (gdk_toplevel_layout_get_type ())

GDK_AVAILABLE_IN_ALL
GType                   gdk_toplevel_layout_get_type    (void);

GDK_AVAILABLE_IN_ALL
GdkToplevelLayout *     gdk_toplevel_layout_new         (int min_width,
                                                         int min_height);

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
int                     gdk_toplevel_layout_get_min_width  (GdkToplevelLayout *layout);
GDK_AVAILABLE_IN_ALL
int                     gdk_toplevel_layout_get_min_height (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_maximized (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_fullscreen (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
GdkMonitor *            gdk_toplevel_layout_get_fullscreen_monitor (GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
void                    gdk_toplevel_layout_set_resizable (GdkToplevelLayout *layout,
                                                           gboolean           resizable);

GDK_AVAILABLE_IN_ALL
gboolean                gdk_toplevel_layout_get_resizable (GdkToplevelLayout *layout);

G_END_DECLS

#endif /* __GDK_TOPLEVEL_LAYOUT_H__ */
