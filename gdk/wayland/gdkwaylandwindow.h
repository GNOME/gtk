/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2013 Jan Arne Petersen
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

#ifndef __GDK_WAYLAND_WINDOW_H__
#define __GDK_WAYLAND_WINDOW_H__

#if !defined (__GDKWAYLAND_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkwayland.h> can be included directly."
#endif

#include <gdk/gdk.h>

#include <wayland-client.h>

G_BEGIN_DECLS

#ifdef GDK_COMPILATION
typedef struct _GdkWaylandWindow GdkWaylandWindow;
#else
typedef GdkWindow GdkWaylandWindow;
#endif
typedef struct _GdkWaylandWindowClass GdkWaylandWindowClass;

#define GDK_TYPE_WAYLAND_WINDOW              (gdk_wayland_window_get_type())
#define GDK_WAYLAND_WINDOW(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_WAYLAND_WINDOW, GdkWaylandWindow))
#define GDK_WAYLAND_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_WINDOW, GdkWaylandWindowClass))
#define GDK_IS_WAYLAND_WINDOW(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_WAYLAND_WINDOW))
#define GDK_IS_WAYLAND_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_WAYLAND_WINDOW))
#define GDK_WAYLAND_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_WINDOW, GdkWaylandWindowClass))

GType                    gdk_wayland_window_get_type             (void);

struct wl_surface       *gdk_wayland_window_get_wl_surface       (GdkWindow *window);
struct wl_shell_surface *gdk_wayland_window_get_wl_shell_surface (GdkWindow *window);

G_END_DECLS

#endif /* __GDK_WAYLAND_WINDOW_H__ */
