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

#ifndef __GDK_QUARTZ_DEVICE_MANAGER_CORE_H__
#define __GDK_QUARTZ_DEVICE_MANAGER_CORE_H__

#include <gdk/gdkdevicemanagerprivate.h>

G_BEGIN_DECLS

#define GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE         (gdk_quartz_device_manager_core_get_type ())
#define GDK_QUARTZ_DEVICE_MANAGER_CORE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE, GdkQuartzDeviceManagerCore))
#define GDK_QUARTZ_DEVICE_MANAGER_CORE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE, GdkQuartzDeviceManagerCoreClass))
#define GDK_IS_QUARTZ_DEVICE_MANAGER_CORE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE))
#define GDK_IS_QUARTZ_DEVICE_MANAGER_CORE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE))
#define GDK_QUARTZ_DEVICE_MANAGER_CORE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE, GdkQuartzDeviceManagerCoreClass))

typedef struct _GdkQuartzDeviceManagerCore GdkQuartzDeviceManagerCore;
typedef struct _GdkQuartzDeviceManagerCoreClass GdkQuartzDeviceManagerCoreClass;

struct _GdkQuartzDeviceManagerCore
{
  GdkDeviceManager parent_object;
  GdkDevice *core_pointer;
  GdkDevice *core_keyboard;
};

struct _GdkQuartzDeviceManagerCoreClass
{
  GdkDeviceManagerClass parent_class;
};

GType gdk_quartz_device_manager_core_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __GDK_QUARTZ_DEVICE_MANAGER_CORE_H__ */
