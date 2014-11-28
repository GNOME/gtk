/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010 Red Hat, Inc
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

#ifndef __GDK_DEVICE_MANAGER_PRIVATE_H__
#define __GDK_DEVICE_MANAGER_PRIVATE_H__

#include "gdkdevicemanager.h"

G_BEGIN_DECLS


#define GDK_DEVICE_MANAGER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_MANAGER, GdkDeviceManagerClass))
#define GDK_IS_DEVICE_MANAGER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_MANAGER))
#define GDK_DEVICE_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_MANAGER, GdkDeviceManagerClass))


typedef struct _GdkDeviceManagerClass GdkDeviceManagerClass;

struct _GdkDeviceManager
{
  GObject parent_instance;

  /*< private >*/
  GdkDisplay *display;
  GdkDevice *current_device;
};

struct _GdkDeviceManagerClass
{
  GObjectClass parent_class;

  /* Signals */
  void (* device_added)   (GdkDeviceManager *device_manager,
                           GdkDevice        *device);
  void (* device_removed) (GdkDeviceManager *device_manager,
                           GdkDevice        *device);
  void (* device_changed) (GdkDeviceManager *device_manager,
                           GdkDevice        *device);

  /* VMethods */
  GList *     (* list_devices)       (GdkDeviceManager *device_manager,
                                      GdkDeviceType     type);
  GdkDevice * (* get_client_pointer) (GdkDeviceManager *device_manager);
};

void
_gdk_device_manager_update_current_device (GdkDeviceManager *device_manager,
                                           GdkDevice        *device);

G_END_DECLS

#endif
