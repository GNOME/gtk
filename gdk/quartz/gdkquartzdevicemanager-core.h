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

#ifndef __GDK_QUARTZ_DEVICE_MANAGER_CORE_H__
#define __GDK_QUARTZ_DEVICE_MANAGER_CORE_H__

#if !defined(__GDKQUARTZ_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdkquartz.h> can be included directly."
#endif

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE         (gdk_quartz_device_manager_core_get_type ())
#define GDK_QUARTZ_DEVICE_MANAGER_CORE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE, GdkQuartzDeviceManagerCore))
#define GDK_QUARTZ_DEVICE_MANAGER_CORE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE, GdkQuartzDeviceManagerCoreClass))
#define GDK_IS_QUARTZ_DEVICE_MANAGER_CORE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE))
#define GDK_IS_QUARTZ_DEVICE_MANAGER_CORE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE))
#define GDK_QUARTZ_DEVICE_MANAGER_CORE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_QUARTZ_DEVICE_MANAGER_CORE, GdkQuartzDeviceManagerCoreClass))

typedef struct _GdkQuartzDeviceManagerCore GdkQuartzDeviceManagerCore;
typedef struct _GdkQuartzDeviceManagerCoreClass GdkQuartzDeviceManagerCoreClass;


GDK_AVAILABLE_IN_ALL
GType gdk_quartz_device_manager_core_get_type (void) G_GNUC_CONST;


G_END_DECLS

#endif /* __GDK_QUARTZ_DEVICE_MANAGER_CORE_H__ */
