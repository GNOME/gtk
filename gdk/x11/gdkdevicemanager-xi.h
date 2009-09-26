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

#ifndef __GDK_DEVICE_MANAGER_XI_H__
#define __GDK_DEVICE_MANAGER_XI_H__

#include "gdkdevicemanager-core.h"

G_BEGIN_DECLS

#define GDK_TYPE_DEVICE_MANAGER_XI         (gdk_device_manager_xi_get_type ())
#define GDK_DEVICE_MANAGER_XI(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE_MANAGER_XI, GdkDeviceManagerXI))
#define GDK_DEVICE_MANAGER_XI_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_MANAGER_XI, GdkDeviceManagerXIClass))
#define GDK_IS_DEVICE_MANAGER_XI(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE_MANAGER_XI))
#define GDK_IS_DEVICE_MANAGER_XI_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_MANAGER_XI))
#define GDK_DEVICE_MANAGER_XI_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_MANAGER_XI, GdkDeviceManagerXIClass))

typedef struct _GdkDeviceManagerXI GdkDeviceManagerXI;
typedef struct _GdkDeviceManagerXIClass GdkDeviceManagerXIClass;

struct _GdkDeviceManagerXI
{
  GdkDeviceManagerCore parent_object;
  GdkDevice *core_pointer;
  GdkDevice *core_keyboard;
};

struct _GdkDeviceManagerXIClass
{
  GdkDeviceManagerCoreClass parent_class;
};

GType gdk_device_manager_xi_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GDK_DEVICE_MANAGER_XI_H__ */
