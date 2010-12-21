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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GDK_X11_DEVICE_XI_H__
#define __GDK_X11_DEVICE_XI_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GDK_TYPE_X11_DEVICE_XI         (gdk_x11_device_xi_get_type ())
#define GDK_X11_DEVICE_XI(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_X11_DEVICE_XI, GdkX11DeviceXI))
#define GDK_X11_DEVICE_XI_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_X11_DEVICE_XI, GdkX11DeviceXIClass))
#define GDK_IS_X11_DEVICE_XI(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_X11_DEVICE_XI))
#define GDK_IS_X11_DEVICE_XI_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_X11_DEVICE_XI))
#define GDK_X11_DEVICE_XI_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_X11_DEVICE_XI, GdkX11DeviceXIClass))

typedef struct _GdkX11DeviceXI GdkX11DeviceXI;
typedef struct _GdkX11DeviceXIClass GdkX11DeviceXIClass;


GType gdk_x11_device_xi_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GDK_X11_DEVICE_XI_H__ */
