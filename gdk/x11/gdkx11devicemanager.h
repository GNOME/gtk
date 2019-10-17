/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2011 Carlos Garnacho
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

#ifndef __GDK_X11_DEVICE_MANAGER_H__
#define __GDK_X11_DEVICE_MANAGER_H__

#if !defined (__GDKX_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkx.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gdk/x11/gdkx11devicemanager-xi2.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

G_BEGIN_DECLS

GDK_AVAILABLE_IN_ALL
GdkDevice * gdk_x11_device_manager_lookup (GdkX11DeviceManagerXI2 *device_manager,
                                           gint                    device_id);

G_END_DECLS

#endif /* __GDK_X11_DEVICE_MANAGER_H__ */
