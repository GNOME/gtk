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

#ifndef __GDK_DEVICE_MANAGER_PRIVATE_CORE_H__
#define __GDK_DEVICE_MANAGER_PRIVATE_CORE_H__

#include "gdkx11devicemanager-core.h"
#include "gdkdevicemanagerprivate.h"

G_BEGIN_DECLS

struct _GdkX11DeviceManagerCore
{
  GdkDeviceManager parent_object;
  GdkDevice *core_pointer;
  GdkDevice *core_keyboard;
};

struct _GdkX11DeviceManagerCoreClass
{
  GdkDeviceManagerClass parent_class;
};

G_END_DECLS

#endif /* __GDK_DEVICE_MANAGER_PRIVATE_CORE_H__ */
