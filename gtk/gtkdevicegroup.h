/* GTK - The GIMP Toolkit
 * Copyright (C) 2009 Carlos Garnacho  <carlosg@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#if defined(GTK_DISABLE_SINGLE_INCLUDES) && !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#ifndef __GTK_DEVICE_GROUP_H__
#define __GTK_DEVICE_GROUP_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

typedef struct GtkDeviceGroup GtkPointerGroup;

#define GTK_TYPE_DEVICE_GROUP         (gtk_device_group_get_type ())
#define GTK_DEVICE_GROUP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GTK_TYPE_DEVICE_GROUP, GtkDeviceGroup))
#define GTK_DEVICE_GROUP_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST    ((c), GTK_TYPE_DEVICE_GROUP, GtkDeviceGroupClass))
#define GTK_IS_DEVICE_GROUP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GTK_TYPE_DEVICE_GROUP))
#define GTK_IS_DEVICE_GROUP_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE    ((c), GTK_TYPE_DEVICE_GROUP))
#define GTK_DEVICE_GROUP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS  ((o), GTK_TYPE_DEVICE_GROUP, GtkDeviceGroupClass))

typedef struct GtkDeviceGroup GtkDeviceGroup;
typedef struct GtkDeviceGroupClass GtkDeviceGroupClass;

struct GtkDeviceGroup
{
  GObject parent_instance;
};

struct GtkDeviceGroupClass
{
  GObjectClass parent_class;

  void (* device_added)   (GtkDeviceGroup *group,
                           GdkDevice      *device);
  void (* device_removed) (GtkDeviceGroup *group,
                           GdkDevice      *device);
};

GType   gtk_device_group_get_type       (void) G_GNUC_CONST;

void    gtk_device_group_add_device     (GtkDeviceGroup  *group,
                                         GdkDevice       *device);

void    gtk_device_group_remove_device  (GtkDeviceGroup  *group,
                                         GdkDevice       *device);

GList * gtk_device_group_get_devices    (GtkDeviceGroup  *group);


G_END_DECLS

#endif /* __GTK_DEVICE_GROUP_H__ */
