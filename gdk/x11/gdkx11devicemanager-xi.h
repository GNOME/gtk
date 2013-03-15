/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GDK_X11_DEVICE_MANAGER_XI_H__
#define __GDK_X11_DEVICE_MANAGER_XI_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GDK_TYPE_X11_DEVICE_MANAGER_XI         (gdk_x11_device_manager_xi_get_type ())
#define GDK_X11_DEVICE_MANAGER_XI(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_X11_DEVICE_MANAGER_XI, GdkX11DeviceManagerXI))
#define GDK_X11_DEVICE_MANAGER_XI_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_X11_DEVICE_MANAGER_XI, GdkX11DeviceManagerXIClass))
#define GDK_IS_X11_DEVICE_MANAGER_XI(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_X11_DEVICE_MANAGER_XI))
#define GDK_IS_X11_DEVICE_MANAGER_XI_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_X11_DEVICE_MANAGER_XI))
#define GDK_X11_DEVICE_MANAGER_XI_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_X11_DEVICE_MANAGER_XI, GdkX11DeviceManagerXIClass))

typedef struct _GdkX11DeviceManagerXI GdkX11DeviceManagerXI;
typedef struct _GdkX11DeviceManagerXIClass GdkX11DeviceManagerXIClass;


GDK_AVAILABLE_IN_ALL
GType gdk_x11_device_manager_xi_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __GDK_X11_DEVICE_MANAGER_XI_H__ */
