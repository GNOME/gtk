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

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_DEVICE_MANAGER_H__
#define __GDK_DEVICE_MANAGER_H__

#include <gdk/gdktypes.h>
#include <gdk/gdkdevice.h>
#include <gdk/gdkdisplay.h>

G_BEGIN_DECLS

#define GDK_TYPE_DEVICE_MANAGER         (gdk_device_manager_get_type ())
#define GDK_DEVICE_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE_MANAGER, GdkDeviceManager))
#define GDK_DEVICE_MANAGER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_MANAGER, GdkDeviceManagerClass))
#define GDK_IS_DEVICE_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE_MANAGER))
#define GDK_IS_DEVICE_MANAGER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_MANAGER))
#define GDK_DEVICE_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_MANAGER, GdkDeviceManagerClass))

typedef struct _GdkDeviceManager GdkDeviceManager;
typedef struct _GdkDeviceManagerClass GdkDeviceManagerClass;

typedef enum {
  GDK_DEVICE_TYPE_MASTER,
  GDK_DEVICE_TYPE_SLAVE,
  GDK_DEVICE_TYPE_FLOATING
} GdkDeviceType;

struct _GdkDeviceManager
{
  GObject parent_instance;
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
  GList * (* get_devices) (GdkDeviceManager *device_manager,
                           GdkDeviceType     type);
};

GType gdk_device_manager_get_type (void) G_GNUC_CONST;

GdkDeviceManager * gdk_device_manager_get_for_display (GdkDisplay *display);

GdkDisplay *             gdk_device_manager_get_display      (GdkDeviceManager *device_manager);
GList *                  gdk_device_manager_get_devices      (GdkDeviceManager *device_manager,
                                                              GdkDeviceType     type);

G_END_DECLS

#endif /* __GDK_DEVICE_MANAGER_H__ */
