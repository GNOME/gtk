/*
 * Copyright © 2020 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#ifndef __GDK_TOPLEVEL_H__
#define __GDK_TOPLEVEL_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdksurface.h>
#include <gdk/gdktoplevellayout.h>

G_BEGIN_DECLS

#define GDK_TYPE_TOPLEVEL               (gdk_toplevel_get_type ())

GDK_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GdkToplevel, gdk_toplevel, GDK, TOPLEVEL, GObject)

GDK_AVAILABLE_IN_ALL
gboolean        gdk_toplevel_present            (GdkToplevel       *toplevel,
                                                 int                width,
                                                 int                height,
                                                 GdkToplevelLayout *layout);

GDK_AVAILABLE_IN_ALL
gboolean        gdk_toplevel_minimize           (GdkToplevel       *toplevel);

GDK_AVAILABLE_IN_ALL
void            gdk_toplevel_focus              (GdkToplevel       *toplevel,
                                                 guint32            timestamp);

GDK_AVAILABLE_IN_ALL
GdkSurfaceState gdk_toplevel_get_state          (GdkToplevel       *toplevel);

GDK_AVAILABLE_IN_ALL
void            gdk_toplevel_set_title          (GdkToplevel       *toplevel,
                                                 const char        *title);

GDK_AVAILABLE_IN_ALL
void            gdk_toplevel_set_startup_id     (GdkToplevel       *toplevel,
                                                 const char        *startup_id);

GDK_AVAILABLE_IN_ALL
void            gdk_toplevel_set_transient_for  (GdkToplevel       *toplevel,
                                                 GdkSurface        *parent);

GDK_AVAILABLE_IN_ALL
void            gdk_toplevel_set_icon_list      (GdkToplevel       *toplevel,
                                                 GList             *surfaces);

GDK_AVAILABLE_IN_ALL
gboolean        gdk_toplevel_show_window_menu   (GdkToplevel       *toplevel,
                                                 GdkEvent          *event);

GDK_AVAILABLE_IN_ALL
void            gdk_toplevel_set_sticky         (GdkToplevel       *toplevel,
                                                 gboolean           sticky);

GDK_AVAILABLE_IN_ALL
void            gdk_toplevel_set_keep_above     (GdkToplevel       *toplevel,
                                                 gboolean           above);
GDK_AVAILABLE_IN_ALL
void            gdk_toplevel_set_keep_below     (GdkToplevel       *toplevel,
                                                 gboolean           below);

G_END_DECLS

#endif /* __GDK_TOPLEVEL_H__ */
