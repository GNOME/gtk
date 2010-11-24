/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1999 Tor Lillqvist
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.
 */

/*
 * GTK+ DirectFB backend
 * Copyright (C) 2001-2002  convergence integrated media GmbH
 * Copyright (C) 2002-2004  convergence GmbH
 * Written by Denis Oliver Kropp <dok@convergence.de> and
 *            Sven Neumann <sven@convergence.de>
 */

#include "config.h"
#include "gdk.h"

#include "gdkdirectfb.h"
#include "gdkprivate-directfb.h"
#include "gdkinput-directfb.h"

#include "gdkinput.h"
#include "gdkkeysyms.h"
#include "gdkalias.h"


static GdkDeviceAxis gdk_input_core_axes[] =
  {
    { GDK_AXIS_X, 0, 0 },
    { GDK_AXIS_Y, 0, 0 }
  };


GdkDevice *_gdk_core_pointer      = NULL;
GList     *_gdk_input_devices     = NULL;
gboolean   _gdk_input_ignore_core = FALSE;

int _gdk_directfb_mouse_x = 0;
int _gdk_directfb_mouse_y = 0;


void
_gdk_init_input_core (void)
{
  GdkDisplay *display = GDK_DISPLAY_OBJECT (_gdk_display);
  
  _gdk_core_pointer = g_object_new (GDK_TYPE_DEVICE, NULL);
  
  _gdk_core_pointer->name       = "Core Pointer";
  _gdk_core_pointer->source     = GDK_SOURCE_MOUSE;
  _gdk_core_pointer->mode       = GDK_MODE_SCREEN;
  _gdk_core_pointer->has_cursor = TRUE;
  _gdk_core_pointer->num_axes   = 2;
  _gdk_core_pointer->axes       = gdk_input_core_axes;
  _gdk_core_pointer->num_keys   = 0;
  _gdk_core_pointer->keys       = NULL;
  display->core_pointer         = _gdk_core_pointer;
}

static void
gdk_device_finalize (GObject *object)
{
  g_error ("A GdkDevice object was finalized. This should not happen");
}

static void
gdk_device_class_init (GObjectClass *class)
{
  class->finalize = gdk_device_finalize;
}

GType
gdk_device_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
        {
          sizeof (GdkDeviceClass),
          (GBaseInitFunc) NULL,
          (GBaseFinalizeFunc) NULL,
          (GClassInitFunc) gdk_device_class_init,
          NULL,           /* class_finalize */
          NULL,           /* class_data */
          sizeof (GdkDevice),
          0,              /* n_preallocs */
          (GInstanceInitFunc) NULL,
        };

      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkDevice",
                                            &object_info, 0);
    }

  return object_type;
}

/**
 * gdk_devices_list:
 *
 * Returns the list of available input devices for the default display.
 * The list is statically allocated and should not be freed.
 *
 * Return value: a list of #GdkDevice
 **/
GList *
gdk_devices_list (void)
{
  return _gdk_input_devices;
}


/**
 * gdk_display_list_devices:
 * @display: a #GdkDisplay
 *
 * Returns the list of available input devices attached to @display.
 * The list is statically allocated and should not be freed.
 *
 * Return value: a list of #GdkDevice
 *
 * Since: 2.2
 **/
GList *
gdk_display_list_devices (GdkDisplay *dpy)
{
  return _gdk_input_devices;
}

/**
 * gdk_device_get_name:
 * @device: a #GdkDevice
 *
 * Determines the name of the device.
 *
 * Return value: a name
 *
 * Since: 2.22
 **/
const gchar *
gdk_device_get_name (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), NULL);

  return device->name;
}

/**
 * gdk_device_get_source:
 * @device: a #GdkDevice
 *
 * Determines the type of the device.
 *
 * Return value: a #GdkInputSource
 *
 * Since: 2.22
 **/
GdkInputSource
gdk_device_get_source (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->source;
}

/**
 * gdk_device_get_mode:
 * @device: a #GdkDevice
 *
 * Determines the mode of the device.
 *
 * Return value: a #GdkInputSource
 *
 * Since: 2.22
 **/
GdkInputMode
gdk_device_get_mode (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->mode;
}

/**
 * gdk_device_get_has_cursor:
 * @device: a #GdkDevice
 *
 * Determines whether the pointer follows device motion.
 *
 * Return value: %TRUE if the pointer follows device motion
 *
 * Since: 2.22
 **/
gboolean
gdk_device_get_has_cursor (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), FALSE);

  return device->has_cursor;
}

void
gdk_device_set_source (GdkDevice      *device,
                       GdkInputSource  source)
{
  g_return_if_fail (device != NULL);
  device->source = source;
}

/**
 * gdk_device_get_key:
 * @device: a #GdkDevice.
 * @index: the index of the macro button to get.
 * @keyval: return value for the keyval.
 * @modifiers: return value for modifiers.
 *
 * If @index has a valid keyval, this function will
 * fill in @keyval and @modifiers with the keyval settings.
 *
 * Since: 2.22
 **/
void
gdk_device_get_key (GdkDevice       *device,
                    guint            index,
                    guint           *keyval,
                    GdkModifierType *modifiers)
{
  g_return_if_fail (GDK_IS_DEVICE (device));
  g_return_if_fail (index < device->num_keys);

  if (!device->keys[index].keyval &&
      !device->keys[index].modifiers)
    return;

  if (keyval)
    *keyval = device->keys[index].keyval;

  if (modifiers)
    *modifiers = device->keys[index].modifiers;
}

/**
 * gdk_device_get_axis_use:
 * @device: a #GdkDevice.
 * @index: the index of the axis.
 *
 * Returns the axis use for @index.
 *
 * Returns: a #GdkAxisUse specifying how the axis is used.
 *
 * Since: 2.22
 **/
GdkAxisUse
gdk_device_get_axis_use (GdkDevice *device,
                         guint      index)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), GDK_AXIS_IGNORE);
  g_return_val_if_fail (index < device->num_axes, GDK_AXIS_IGNORE);

  return device->axes[index].use;
}

gint
gdk_device_get_n_keys (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->num_keys;
}

/**
 * gdk_device_get_n_axes:
 * @device: a #GdkDevice.
 *
 * Gets the number of axes of a device.
 *
 * Returns: the number of axes of @device
 *
 * Since: 2.22
 **/
gint
gdk_device_get_n_axes (GdkDevice *device)
{
  g_return_val_if_fail (GDK_IS_DEVICE (device), 0);

  return device->num_axes;
}

void
gdk_device_set_axis_use (GdkDevice   *device,
                         guint        index,
                         GdkAxisUse   use)
{
  g_return_if_fail (device != NULL);
  g_return_if_fail (index < device->num_axes);

  device->axes[index].use = use;

  switch (use)
    {
    case GDK_AXIS_X:
    case GDK_AXIS_Y:
      device->axes[index].min =  0.0;
      device->axes[index].max =  0.0;
      break;
    case GDK_AXIS_XTILT:
    case GDK_AXIS_YTILT:
      device->axes[index].min = -1.0;
      device->axes[index].max =  1.0;
      break;
    default:
      device->axes[index].min =  0.0;
      device->axes[index].max =  1.0;
      break;
    }
}

gboolean
gdk_device_set_mode (GdkDevice    *device,
                     GdkInputMode  mode)
{
  g_message ("unimplemented %s", G_STRFUNC);

  return FALSE;
}

gboolean
gdk_device_get_history  (GdkDevice      *device,
                         GdkWindow      *window,
                         guint32         start,
                         guint32         stop,
                         GdkTimeCoord ***events,
                         gint           *n_events)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (events != NULL, FALSE);
  g_return_val_if_fail (n_events != NULL, FALSE);

  *n_events = 0;
  *events = NULL;

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;

  if (GDK_IS_CORE (device))
    return FALSE;
  else
    return FALSE;
  //TODODO_gdk_device_get_history (device, window, start, stop, events, n_events);
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

void
gdk_device_get_state (GdkDevice       *device,
                      GdkWindow       *window,
                      gdouble         *axes,
                      GdkModifierType *mask)
{
  g_return_if_fail (device != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (mask)
    *mask = _gdk_directfb_modifiers;
}

void
gdk_directfb_mouse_get_info (gint            *x,
                             gint            *y,
                             GdkModifierType *mask)
{
  if (x)
    *x = _gdk_directfb_mouse_x;

  if (y)
    *y = _gdk_directfb_mouse_y;

  if (mask)
    *mask = _gdk_directfb_modifiers;
}

void
gdk_input_set_extension_events (GdkWindow        *window,
                                gint              mask,
                                GdkExtensionMode  mode)
{
  g_message ("unimplemented %s", G_STRFUNC);
}

void
gdk_device_set_key (GdkDevice       *device,
                    guint            index,
                    guint            keyval,
                    GdkModifierType  modifiers)
{
  g_return_if_fail (device != NULL);
  g_return_if_fail (index < device->num_keys);

  device->keys[index].keyval    = keyval;
  device->keys[index].modifiers = modifiers;
}

/**
 * gdk_device_get_axis:
 * @device: a #GdkDevice
 * @axes: pointer to an array of axes
 * @use: the use to look for
 * @value: location to store the found value.
 *
 * Interprets an array of double as axis values for a given device,
 * and locates the value in the array for a given axis use.
 *
 * Return value: %TRUE if the given axis use was found, otherwise %FALSE
 **/
gboolean
gdk_device_get_axis (GdkDevice  *device,
                     gdouble    *axes,
                     GdkAxisUse  use,
                     gdouble    *value)
{
  gint i;
  g_return_val_if_fail (device != NULL, FALSE);

  if (axes == NULL)
    return FALSE;

  for (i = 0; i < device->num_axes; i++)
    if (device->axes[i].use == use)
      {
	if (value)
	  *value = axes[i];
	return TRUE;
      }

  return FALSE;
}

void
_gdk_input_init (void)
{
  _gdk_init_input_core ();
  _gdk_input_devices = g_list_append (NULL, _gdk_core_pointer);
  _gdk_input_ignore_core = FALSE;
}

void
_gdk_input_exit (void)
{
  GList     *tmp_list;
  GdkDevice *gdkdev;

  for (tmp_list = _gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      gdkdev = (GdkDevice *)(tmp_list->data);
      if (!GDK_IS_CORE (gdkdev))
	{
	  gdk_device_set_mode ((GdkDevice *)gdkdev, GDK_MODE_DISABLED);

	  g_free (gdkdev->name);
	  g_free (gdkdev->axes);
	  g_free (gdkdev->keys);
	  g_free (gdkdev);
	}
    }

  g_list_free (_gdk_input_devices);
}

#define __GDK_INPUT_NONE_C__
#define __GDK_INPUT_C__
#include "gdkaliasdef.c"
