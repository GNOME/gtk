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

#include "config.h"

#include "gdktouchcluster.h"
#include "gdkintl.h"

/**
 * SECTION:touchcluster
 * @Short_description: Multitouch handling
 * @Title: Multitouch
 * @See_also: #GdkEventMultiTouch
 *
 * #GdkTouchCluster is an object that gathers touch IDs from a
 * touch-enabled slave #GdkDevice, in order to send
 * #GdkEventMultiTouch<!-- --> events whenever a contained touch ID
 * is updated.
 *
 * #GdkTouchCluster<!-- -->s are always associated to a window,
 * you need to create them through gdk_window_create_touch_cluster(),
 * and free them through gdk_window_remove_touch_cluster().
 *
 * Touch IDs from devices can be obtained from %GDK_TOUCH_PRESS,
 * %GDK_TOUCH_MOTION or %GDK_TOUCH_RELEASE events through
 * gdk_event_get_touch_id(), and then be added via
 * gdk_touch_cluster_add_touch(). Note that touch IDs are
 * highly transitive, even be an incrementable number only
 * identifying the current touch event stream, so touch IDs
 * should always be gotten from these events.
 *
 * Anytime a touch ID is within a cluster, no %GDK_TOUCH_PRESS,
 * %GDK_TOUCH_MOTION or %GDK_TOUCH_RELEASE events will happen
 * for the individual touch. The event will be available instead
 * as part of the #GdkMultitouchEvent that will be emitted. This
 * will hold true until gdk_touch_cluster_remove_touch() is
 * called for it. Note that GTK+ will automatically take a
 * touch ID out of any cluster if %GDK_TOUCH_RELEASE is gotten
 * internally.
 */

typedef struct GdkTouchClusterPrivate GdkTouchClusterPrivate;

struct GdkTouchClusterPrivate
{
  GdkDevice *device;
  GList *touches;
};

enum {
  PROP_0,
  PROP_DEVICE
};

enum {
  TOUCH_ADDED,
  TOUCH_REMOVED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };

static void   gdk_touch_cluster_finalize     (GObject      *object);
static void   gdk_touch_cluster_set_property (GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec);
static void   gdk_touch_cluster_get_property (GObject      *object,
                                              guint         prop_id,
                                              GValue       *value,
                                              GParamSpec   *pspec);


G_DEFINE_TYPE (GdkTouchCluster, gdk_touch_cluster, G_TYPE_OBJECT)

static void
gdk_touch_cluster_class_init (GdkTouchClusterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_touch_cluster_finalize;
  object_class->get_property = gdk_touch_cluster_get_property;
  object_class->set_property = gdk_touch_cluster_set_property;

  g_object_class_install_property (object_class,
                                   PROP_DEVICE,
                                   g_param_spec_object ("device",
                                                        P_("Device"),
                                                        P_("Device attached to the cluster"),
                                                        GDK_TYPE_DEVICE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  signals[TOUCH_ADDED] =
    g_signal_new (g_intern_static_string ("touch-added"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkTouchClusterClass, touch_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 1, G_TYPE_UINT);
  signals[TOUCH_REMOVED] =
    g_signal_new (g_intern_static_string ("touch-removed"),
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkTouchClusterClass, touch_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__UINT,
                  G_TYPE_NONE, 1, G_TYPE_UINT);

  g_type_class_add_private (object_class, sizeof (GdkTouchClusterPrivate));
}

static void
gdk_touch_cluster_init (GdkTouchCluster *cluster)
{
  cluster->priv = G_TYPE_INSTANCE_GET_PRIVATE (cluster,
                                               GDK_TYPE_TOUCH_CLUSTER,
                                               GdkTouchClusterPrivate);
}

static void
gdk_touch_cluster_finalize (GObject *object)
{
  GdkTouchClusterPrivate *priv;

  priv = GDK_TOUCH_CLUSTER (object)->priv;
  g_list_free (priv->touches);

  G_OBJECT_CLASS (gdk_touch_cluster_parent_class)->finalize (object);
}

static void
gdk_touch_cluster_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GdkTouchClusterPrivate *priv;

  priv = GDK_TOUCH_CLUSTER (object)->priv;

  switch (prop_id)
    {
    case PROP_DEVICE:
      gdk_touch_cluster_set_device (GDK_TOUCH_CLUSTER (object),
                                    g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_touch_cluster_get_property (GObject      *object,
                                guint         prop_id,
                                GValue       *value,
                                GParamSpec   *pspec)
{
  GdkTouchClusterPrivate *priv;

  priv = GDK_TOUCH_CLUSTER (object)->priv;

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, priv->device);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gdk_touch_cluster_add_touch:
 * @cluster: a #GdkTouchCluster
 * @touch_id: a touch ID from a touch event
 *
 * Adds a touch ID to @cluster, so it will generate a
 * %GDK_MULTITOUCH_ADDED event, followed by %GDK_MULTITOUCH_UPDATED
 * events whenever this touch ID is updated.
 *
 * If @touch_id already pertained to another #GdkTouchCluster, it
 * will be removed from it, generating a %GDK_MULTITOUCH_REMOVED
 * for that another cluster.
 **/
void
gdk_touch_cluster_add_touch (GdkTouchCluster *cluster,
                             guint            touch_id)
{
  GdkTouchClusterPrivate *priv;

  g_return_if_fail (GDK_IS_TOUCH_CLUSTER (cluster));

  priv = cluster->priv;

  if (!g_list_find (priv->touches, GUINT_TO_POINTER (touch_id)))
    {
      priv->touches = g_list_prepend (priv->touches, GUINT_TO_POINTER (touch_id));
      g_signal_emit (cluster, signals [TOUCH_ADDED], 0, touch_id);
    }
}

/**
 * gdk_touch_cluster_remove_touch:
 * @cluster: a #GdkTouchCluster
 * @touch_id: a touch ID from a touch event
 *
 * Removes a touch ID from @cluster, generating a %GDK_MULTITOUCH_REMOVED
 * event for @cluster, and causing any further input from @touch_id
 * to be reported trough %GDK_TOUCH_MOTION events.
 *
 * <note><para>
 * Note that GTK+ automatically removes a touch ID from any cluster
 * if a %GDK_TOUCH_RELEASE event is gotten internally.
 * </para></note>
 **/
void
gdk_touch_cluster_remove_touch (GdkTouchCluster *cluster,
                                guint            touch_id)
{
  GdkTouchClusterPrivate *priv;
  GList *link;

  g_return_if_fail (GDK_IS_TOUCH_CLUSTER (cluster));

  priv = cluster->priv;

  link = g_list_find (priv->touches, GUINT_TO_POINTER (touch_id));

  if (link)
    {
      priv->touches = g_list_remove_link (priv->touches, link);
      g_signal_emit (cluster, signals [TOUCH_REMOVED], 0, touch_id);
      g_list_free1 (link);
    }
}

/**
 * gdk_touch_cluster_remove_all:
 * @cluster: a #GdkTouchCluster
 *
 * Removes all touch IDs from @cluster.
 **/
void
gdk_touch_cluster_remove_all (GdkTouchCluster *cluster)
{
  GdkTouchClusterPrivate *priv;
  GList *link;

  g_return_if_fail (GDK_IS_TOUCH_CLUSTER (cluster));

  priv = cluster->priv;
  link = priv->touches;

  while (link)
    {
      priv->touches = g_list_remove_link (priv->touches, link);
      g_signal_emit (cluster, signals [TOUCH_REMOVED], 0, link->data);
    }

  g_list_free (priv->touches);
  priv->touches = NULL;
}


/**
 * gdk_touch_cluster_get_touches:
 * @cluster: a #GdkTouchCluster
 *
 * Returns a const list of touch IDs as #guint.
 *
 * Returns: (transfer none): A list of touch IDs.
 **/
GList *
gdk_touch_cluster_get_touches (GdkTouchCluster *cluster)
{
  GdkTouchClusterPrivate *priv;

  g_return_val_if_fail (GDK_IS_TOUCH_CLUSTER (cluster), NULL);

  priv = cluster->priv;
  return priv->touches;
}

/**
 * gdk_touch_cluster_set_device:
 * @cluster: a #GdkTouchCluster
 * @device: a #GdkDevice
 *
 * Sets the current device associated to @cluster, all contained
 * touch IDs must pertain to this device. As a consequence,
 * gdk_touch_cluster_remove_all() will be called on @cluster
 * if the current device changes.
 **/
void
gdk_touch_cluster_set_device (GdkTouchCluster *cluster,
                              GdkDevice       *device)
{
  GdkTouchClusterPrivate *priv;

  g_return_if_fail (GDK_IS_TOUCH_CLUSTER (cluster));
  g_return_if_fail (!device || GDK_IS_DEVICE (device));

  priv = cluster->priv;

  if (priv->device != device)
    gdk_touch_cluster_remove_all (cluster);

  priv->device = device;
}

/**
 * gdk_touch_cluster_get_device:
 * @cluster: a #GdkTouchCluster
 *
 * Returns the slave/floating device this touch cluster pertains to,
 * only touch IDs from this device can be included in @cluster.
 * the #GdkDevice will typically have the %GDK_SOURCE_TOUCH input source.
 *
 * Returns: (transfer none): The #GdkDevice generating the contained touch IDs
 **/
GdkDevice *
gdk_touch_cluster_get_device (GdkTouchCluster *cluster)
{
  GdkTouchClusterPrivate *priv;

  g_return_val_if_fail (GDK_IS_TOUCH_CLUSTER (cluster), NULL);

  priv = cluster->priv;
  return priv->device;
}
