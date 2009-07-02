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

#include "gdkdevice.h"
#include "gdkintl.h"
#include "gdkinternals.h"
#include "gdkalias.h"

#define GDK_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GDK_TYPE_DEVICE, GdkDevicePrivate))

typedef struct _GdkDevicePrivate GdkDevicePrivate;

struct _GdkDevicePrivate
{
  GdkDisplay *display;
};

static void gdk_device_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec);
static void gdk_device_get_property (GObject      *object,
                                     guint         prop_id,
                                     GValue       *value,
                                     GParamSpec   *pspec);


G_DEFINE_ABSTRACT_TYPE (GdkDevice, gdk_device, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_DISPLAY,
  PROP_NAME,
  PROP_INPUT_SOURCE,
  PROP_HAS_CURSOR,
};


static void
gdk_device_class_init (GdkDeviceClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->set_property = gdk_device_set_property;
  object_class->get_property = gdk_device_get_property;

  g_object_class_install_property (object_class,
				   PROP_DISPLAY,
				   g_param_spec_object ("display",
                                                        P_("Device Display"),
                                                        P_("Display to which the device belongs to"),
                                                        GDK_TYPE_DISPLAY,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_NAME,
				   g_param_spec_string ("name",
                                                        P_("Device name"),
                                                        P_("Device name"),
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_INPUT_SOURCE,
				   g_param_spec_enum ("input-source",
                                                      P_("Input source"),
                                                      P_("Source type for the device"),
                                                      GDK_TYPE_INPUT_SOURCE,
                                                      GDK_SOURCE_MOUSE,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property (object_class,
				   PROP_HAS_CURSOR,
				   g_param_spec_boolean ("has-cursor",
                                                         P_("Whether the device has cursor"),
                                                         P_("Whether there is a visible cursor following device motion"),
                                                         FALSE,
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  g_type_class_add_private (object_class, sizeof (GdkDevicePrivate));
}

static void
gdk_device_init (GdkDevice *device)
{
}

static void
gdk_device_set_property (GObject      *object,
                         guint         prop_id,
                         const GValue *value,
                         GParamSpec   *pspec)
{
  GdkDevice *device = GDK_DEVICE (object);
  GdkDevicePrivate *priv = GDK_DEVICE_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      priv->display = g_value_get_object (value);
      break;
    case PROP_NAME:
      if (device->name)
        g_free (device->name);

      device->name = g_value_dup_string (value);
      break;
    case PROP_INPUT_SOURCE:
      device->source = g_value_get_enum (value);
      break;
    case PROP_HAS_CURSOR:
      device->has_cursor = g_value_get_boolean (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_device_get_property (GObject    *object,
                         guint       prop_id,
                         GValue     *value,
                         GParamSpec *pspec)
{
  GdkDevice *device = GDK_DEVICE (object);
  GdkDevicePrivate *priv = GDK_DEVICE_GET_PRIVATE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY:
      g_value_set_object (value, priv->display);
      break;
    case PROP_NAME:
      g_value_set_string (value,
                          device->name);
      break;
    case PROP_INPUT_SOURCE:
      g_value_set_enum (value,
                        device->source);
      break;
    case PROP_HAS_CURSOR:
      g_value_set_boolean (value,
                           device->has_cursor);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

void
gdk_device_get_state (GdkDevice       *device,
                      GdkWindow       *window,
                      gdouble         *axes,
                      GdkModifierType *mask)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_DEVICE_GET_CLASS (device)->get_state)
    GDK_DEVICE_GET_CLASS (device)->get_state (device, window, axes, mask);
}

gboolean
gdk_device_get_history (GdkDevice      *device,
                        GdkWindow      *window,
                        guint32         start,
                        guint32         stop,
                        GdkTimeCoord ***events,
                        guint          *n_events)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (!GDK_DEVICE_GET_CLASS (device)->get_history)
    return FALSE;

  return GDK_DEVICE_GET_CLASS (device)->get_history (device, window,
                                                     start, stop,
                                                     events, n_events);
}

void
gdk_device_free_history (GdkTimeCoord **events,
                         gint           n_events)
{
  gint i;

  for (i = 0; i < n_events; i++)
    g_free (events[i]);

  g_free (events);
}

gboolean
gdk_device_get_axis (GdkDevice  *device,
                     gdouble    *axes,
                     GdkAxisUse  use,
                     gdouble    *value)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);
  g_return_val_if_fail (axes != NULL, FALSE);
  g_return_val_if_fail (value != NULL, FALSE);

  if (!GDK_DEVICE_GET_CLASS (device)->get_axis)
    return FALSE;

  return GDK_DEVICE_GET_CLASS (device)->get_axis (device, axes, use, value);
}

void
gdk_device_set_source (GdkDevice      *device,
		       GdkInputSource  source)
{
  g_return_if_fail (GDK_IS_DEVICE (device));

  device->source = source;
}

gboolean
gdk_device_set_mode (GdkDevice    *device,
                     GdkInputMode  mode)
{
  return FALSE;
}

void
gdk_device_set_key (GdkDevice      *device,
		    guint           index,
		    guint           keyval,
		    GdkModifierType modifiers)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (index < device->num_keys);

  device->keys[index].keyval = keyval;
  device->keys[index].modifiers = modifiers;
}

void
gdk_device_set_axis_use (GdkDevice   *device,
			 guint        index,
			 GdkAxisUse   use)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (index < device->num_axes);

  device->axes[index].use = use;

  switch (use)
    {
    case GDK_AXIS_X:
    case GDK_AXIS_Y:
      device->axes[index].min = 0.;
      device->axes[index].max = 0.;
      break;
    case GDK_AXIS_XTILT:
    case GDK_AXIS_YTILT:
      device->axes[index].min = -1.;
      device->axes[index].max = 1;
      break;
    default:
      device->axes[index].min = 0.;
      device->axes[index].max = 1;
      break;
    }
}

#define __GDK_DEVICE_C__
#include "gdkaliasdef.c"
