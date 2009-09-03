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

#include "gdkdevicemanager.h"
#include "gdkintl.h"
#include "gdkinternals.h"
#include "gdkalias.h"


static void gdk_device_manager_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static void gdk_device_manager_get_property (GObject      *object,
                                             guint         prop_id,
                                             GValue       *value,
                                             GParamSpec   *pspec);


G_DEFINE_ABSTRACT_TYPE (GdkDeviceManager, gdk_device_manager, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DISPLAY
};

enum {
  DEVICE_ADDED,
  DEVICE_REMOVED,
  DEVICE_CHANGED,
  LAST_SIGNAL
};

static guint signals [LAST_SIGNAL] = { 0 };
static GHashTable *device_managers = NULL;

typedef struct GdkDeviceManagerPrivate GdkDeviceManagerPrivate;

struct GdkDeviceManagerPrivate
{
  GdkDisplay *display;
};

#define GDK_DEVICE_MANAGER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), GDK_TYPE_DEVICE_MANAGER, GdkDeviceManagerPrivate))

static void
gdk_device_manager_class_init (GdkDeviceManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gdk_device_manager_set_property;
  object_class->get_property = gdk_device_manager_get_property;

  g_object_class_install_property (object_class,
				   PROP_DISPLAY,
				   g_param_spec_object ("display",
 							P_("Display"),
 							P_("Display for the device manager"),
							GDK_TYPE_DISPLAY,
 							G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  signals [DEVICE_ADDED] =
    g_signal_new (g_intern_static_string ("device-added"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDeviceManagerClass, device_added),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DEVICE);

  signals [DEVICE_REMOVED] =
    g_signal_new (g_intern_static_string ("device-removed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDeviceManagerClass, device_removed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DEVICE);

  signals [DEVICE_CHANGED] =
    g_signal_new (g_intern_static_string ("device-changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (GdkDeviceManagerClass, device_changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__OBJECT,
                  G_TYPE_NONE, 1,
                  GDK_TYPE_DEVICE);

  g_type_class_add_private (object_class, sizeof (GdkDeviceManagerPrivate));
}

static void
gdk_device_manager_init (GdkDeviceManager *manager)
{

}

static void
gdk_device_manager_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GdkDeviceManagerPrivate *priv;

  priv = GDK_DEVICE_MANAGER_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      priv->display = g_value_get_object (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_manager_get_property (GObject      *object,
                                 guint         prop_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  GdkDeviceManagerPrivate *priv;

  priv = GDK_DEVICE_MANAGER_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

/**
 * gdk_device_manager_get_for_display:
 * @display: A #GdkDisplay
 *
 * Returns the #GdkDeviceManager attached to @display.
 *
 * Returns: the #GdkDeviceManager attached to @display. This memory
 *          is owned by GTK+, and must not be freed or unreffed.
 **/
GdkDeviceManager *
gdk_device_manager_get_for_display (GdkDisplay *display)
{
  GdkDeviceManager *device_manager;

  if (G_UNLIKELY (!device_managers))
    device_managers = g_hash_table_new (g_direct_hash,
                                        g_direct_equal);

  device_manager = g_hash_table_lookup (device_managers, display);

  if (G_UNLIKELY (!device_manager))
    {
      device_manager = _gdk_device_manager_new (display);
      g_hash_table_insert (device_managers, display, device_manager);
    }

  return device_manager;
}

/**
 * gdk_device_manager_get_display:
 * @device_manager: a #GdkDeviceManager
 *
 * Gets the #GdkDisplay associated to @device_manager.
 *
 * Returns: the #GdkDisplay to which @device_manager is
 *          associated to, or #NULL.
 **/
GdkDisplay *
gdk_device_manager_get_display (GdkDeviceManager *device_manager)
{
  GdkDeviceManagerPrivate *priv;

  g_return_val_if_fail (GDK_IS_DEVICE_MANAGER (device_manager), NULL);

  priv = GDK_DEVICE_MANAGER_GET_PRIVATE (device_manager);

  return priv->display;
}

/**
 * gdk_device_manager_get_devices:
 * @device_manager: a #GdkDeviceManager
 * @type: device type to get.
 *
 * Returns the list of devices of type @type currently attached to
 * @device_manager.
 *
 * Returns: a list of #GdkDevice<!-- -->s. The returned list must be
 *          freed with g_list_free ().
 **/
GList *
gdk_device_manager_get_devices (GdkDeviceManager *device_manager,
                                GdkDeviceType     type)
{
  g_return_val_if_fail (GDK_IS_DEVICE_MANAGER (device_manager), NULL);

  return GDK_DEVICE_MANAGER_GET_CLASS (device_manager)->get_devices (device_manager, type);
}

#define __GDK_DEVICE_MANAGER_C__
#include "gdkaliasdef.c"
