/*
 * Copyright © 2018 Benjamin Otte
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gdkdndprivate.h"
#include "gdkdisplay.h"
#include "gdksurface.h"
#include "gdkintl.h"
#include "gdkcontentformats.h"
#include "gdkcontentprovider.h"
#include "gdkcontentserializer.h"
#include "gdkcursor.h"
#include "gdkenumtypes.h"
#include "gdkeventsprivate.h"

typedef struct _GdkDropPrivate GdkDropPrivate;

struct _GdkDropPrivate {
  GdkDevice *device;
};

enum {
  PROP_0,
  PROP_DEVICE,
  PROP_DISPLAY,
  N_PROPERTIES
};

static GParamSpec *properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE_WITH_PRIVATE (GdkDrop, gdk_drop, G_TYPE_OBJECT)

/**
 * GdkDrop:
 *
 * The GdkDrop struct contains only private fields and
 * should not be accessed directly.
 */

static void
gdk_drop_init (GdkDrop *self)
{
}

static void
gdk_drop_set_property (GObject      *gobject,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  GdkDrop *self = GDK_DROP (gobject);
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DEVICE:
      priv->device = g_value_dup_object (value);
      g_assert (priv->device != NULL);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_drop_get_property (GObject    *gobject,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  GdkDrop *self = GDK_DROP (gobject);
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  switch (prop_id)
    {
    case PROP_DEVICE:
      g_value_set_object (value, priv->device);
      break;

    case PROP_DISPLAY:
      g_value_set_object (value, gdk_device_get_display (priv->device));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_drop_finalize (GObject *object)
{
  GdkDrop *self = GDK_DROP (object);
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_clear_object (&priv->device);

  G_OBJECT_CLASS (gdk_drop_parent_class)->finalize (object);
}

static void
gdk_drop_class_init (GdkDropClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->get_property = gdk_drop_get_property;
  object_class->set_property = gdk_drop_set_property;
  object_class->finalize = gdk_drop_finalize;

  /**
   * GdkDrop:device:
   *
   * The #GdkDevice performing the drop
   */
  properties[PROP_DEVICE] =
    g_param_spec_object ("device",
                         "Device",
                         "The device performing the drop",
                         GDK_TYPE_DEVICE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkDrop:display:
   *
   * The #GdkDisplay that the drag self belongs to.
   */
  properties[PROP_DISPLAY] =
    g_param_spec_object ("display",
                         "Display",
                         "Display this drag belongs to",
                         GDK_TYPE_DISPLAY,
                         G_PARAM_READABLE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, N_PROPERTIES, properties);
}

/**
 * gdk_drop_get_display:
 * @self: a #GdkDrop
 *
 * Gets the #GdkDisplay that @self was created for.
 *
 * Returns: (transfer none): a #GdkDisplay
 **/
GdkDisplay *
gdk_drop_get_display (GdkDrop *self)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_DROP (self), NULL);

  return gdk_device_get_display (priv->device);
}

/**
 * gdk_drop_get_device:
 * @self: a #GdkDrop
 *
 * Returns the #GdkDevice performing the drop.
 *
 * Returns: (transfer none): The #GdkDevice performing the drop.
 **/
GdkDevice *
gdk_drop_get_device (GdkDrop *self)
{
  GdkDropPrivate *priv = gdk_drop_get_instance_private (self);

  g_return_val_if_fail (GDK_IS_DROP (self), NULL);

  return priv->device;
}

