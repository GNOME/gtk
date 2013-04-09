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

#ifndef __GDK_WAYLAND_DISPLAY_MANAGER_H__
#define __GDK_WAYLAND_DISPLAY_MANAGER_H__

#if !defined (__GDKWAYLAND_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkwayland.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct _GdkWaylandDisplayManager GdkWaylandDisplayManager;
typedef struct _GdkWaylandDisplayManagerClass GdkWaylandDisplayManagerClass;

#define GDK_TYPE_WAYLAND_DISPLAY_MANAGER              (gdk_wayland_display_manager_get_type())
#define GDK_WAYLAND_DISPLAY_MANAGER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_DISPLAY_MANAGER, GdkWaylandDisplayManager))
#define GDK_WAYLAND_DISPLAY_MANAGER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_DISPLAY_MANAGER, GdkWaylandDisplayManagerClass))
#define GDK_IS_WAYLAND_DISPLAY_MANAGER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_DISPLAY_MANAGER))
#define GDK_IS_WAYLAND_DISPLAY_MANAGER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_DISPLAY_MANAGER))
#define GDK_WAYLAND_DISPLAY_MANAGER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_DISPLAY_MANAGER, GdkWaylandDisplayManagerClass))

GType      gdk_wayland_display_manager_get_type (void);

G_END_DECLS

#endif /* __GDK_WAYLAND_DISPLAY_MANAGER_H__ */
