/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2011 Carlos Garnacho <carlosg@gnome.org>
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

#ifndef __GDK_TOUCH_CLUSTER_H__
#define __GDK_TOUCH_CLUSTER_H__

#include <glib-object.h>
#include <gdk/gdkdevice.h>

G_BEGIN_DECLS

#define GDK_TYPE_TOUCH_CLUSTER         (gdk_touch_cluster_get_type ())
#define GDK_TOUCH_CLUSTER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_TOUCH_CLUSTER, GdkTouchCluster))
#define GDK_IS_TOUCH_CLUSTER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_TOUCH_CLUSTER))

typedef struct _GdkTouchCluster GdkTouchCluster;
typedef struct _GdkTouchClusterClass GdkTouchClusterClass;

struct _GdkTouchCluster
{
  GObject parent_instance;
  gpointer priv;
};

struct _GdkTouchClusterClass
{
  GObjectClass parent_class;

  void (* touch_added)   (GdkTouchCluster *cluster,
                          guint            touch_id);
  void (* touch_removed) (GdkTouchCluster *cluster,
                          guint            touch_id);

  gpointer padding[16];
};

GType       gdk_touch_cluster_get_type     (void) G_GNUC_CONST;

void        gdk_touch_cluster_add_touch    (GdkTouchCluster *cluster,
                                            guint            touch_id);
void        gdk_touch_cluster_remove_touch (GdkTouchCluster *cluster,
                                            guint            touch_id);
void        gdk_touch_cluster_remove_all   (GdkTouchCluster *cluster);

guint *     gdk_touch_cluster_get_touches  (GdkTouchCluster *cluster,
                                            gint            *length);
gint        gdk_touch_cluster_get_n_touches (GdkTouchCluster *cluster);

void        gdk_touch_cluster_set_device   (GdkTouchCluster *cluster,
                                            GdkDevice       *device);
GdkDevice * gdk_touch_cluster_get_device   (GdkTouchCluster *cluster);

G_END_DECLS

#endif /* __GDK_TOUCH_CLUSTER_H__ */
