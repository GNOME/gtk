/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdlib.h>
#include "config.h"

#include "gdkprivate-fb.h"
#include "gdkinput.h"
#include "gdkprivate.h"
#include "gdkinputprivate.h"

static const GdkDeviceAxis gdk_input_core_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 }
};

static GdkDevice gdk_input_core_info =
{
  "Core Pointer",
  GDK_SOURCE_MOUSE,
  GDK_MODE_SCREEN,
  TRUE,
  2,
  (GdkDeviceAxis *)gdk_input_core_axes,
  0,
  NULL
};

GdkDevice *gdk_core_pointer = (GdkDevice *)&gdk_input_core_info;

/* Global variables  */

gchar            *gdk_input_gxid_host;
gint              gdk_input_gxid_port;
gint              gdk_input_ignore_core;
GList            *gdk_input_windows;
GList            *gdk_input_devices;

GList *
gdk_devices_list (void)
{
  return gdk_input_devices;
}

void
gdk_device_set_source (GdkDevice *device,
		       GdkInputSource source)
{
  device->source = source;
}


void
gdk_device_set_key (GdkDevice      *device,
		    guint           index,
		    guint           keyval,
		    GdkModifierType modifiers)
{
  g_return_if_fail (device != NULL);
  g_return_if_fail (index < device->num_keys);

  device->keys[index].keyval = keyval;
  device->keys[index].modifiers = modifiers;
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

void 
gdk_device_get_state (GdkDevice       *device,
                      GdkWindow       *window,
                      gdouble         *axes,
                      GdkModifierType *mask)
{
  gint x_int, y_int;

  g_assert (device == gdk_core_pointer);
      
  gdk_window_get_pointer (window, &x_int, &y_int, mask);

  if (axes)
    {
      axes[0] = x_int;
      axes[1] = y_int;
    }
}

void 
gdk_device_free_history (GdkTimeCoord **events,
			 gint           n_events)
{
  gint i;
  
  for (i=0; i<n_events; i++)
    g_free (events[i]);

  g_free (events);
}

gboolean
gdk_device_get_history  (GdkDevice         *device,
			 GdkWindow         *window,
			 guint32            start,
			 guint32            stop,
			 GdkTimeCoord    ***events,
			 gint              *n_events)
{
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  g_return_val_if_fail (events != NULL, FALSE);
  g_return_val_if_fail (n_events != NULL, FALSE);

  *n_events = 0;
  *events = NULL;
  return FALSE;
}

gboolean
gdk_device_set_mode (GdkDevice   *device,
                     GdkInputMode mode)
{
  return FALSE;
}

gint
gdk_input_enable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  return TRUE;
}

gint
gdk_input_disable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  return TRUE;
}


GdkInputWindow *
gdk_input_window_find(GdkWindow *window)
{
  GList *tmp_list;

  for (tmp_list=gdk_input_windows; tmp_list; tmp_list=tmp_list->next)
    if (((GdkInputWindow *)(tmp_list->data))->window == window)
      return (GdkInputWindow *)(tmp_list->data);

  return NULL;      /* Not found */
}

/* FIXME: this routine currently needs to be called between creation
   and the corresponding configure event (because it doesn't get the
   root_relative_geometry).  This should work with
   gtk_window_set_extension_events, but will likely fail in other
   cases */

void
gdk_input_set_extension_events (GdkWindow *window, gint mask,
				GdkExtensionMode mode)
{
  GdkWindowObject *window_private;
  GList *tmp_list;
  GdkInputWindow *iw;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  window_private = (GdkWindowObject*) window;

  if (mode == GDK_EXTENSION_EVENTS_NONE)
    mask = 0;

  if (mask != 0)
    {
      iw = g_new(GdkInputWindow,1);

      iw->window = window;
      iw->mode = mode;

      iw->obscuring = NULL;
      iw->num_obscuring = 0;
      iw->grabbed = FALSE;

      gdk_input_windows = g_list_append (gdk_input_windows,iw);
      window_private->extension_events = mask;

      /* Add enter window events to the event mask */
      /* FIXME, this is not needed for XINPUT_NONE */
      gdk_window_set_events (window,
			     gdk_window_get_events (window) | 
			     GDK_ENTER_NOTIFY_MASK);
    }
  else
    {
      iw = gdk_input_window_find (window);
      if (iw)
	{
	  gdk_input_windows = g_list_remove (gdk_input_windows,iw);
	  g_free (iw);
	}

      window_private->extension_events = 0;
    }

  for (tmp_list = gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDevicePrivate *gdkdev = (GdkDevicePrivate *)(tmp_list->data);

      if (gdkdev != (GdkDevicePrivate *)gdk_core_pointer)
	{
	  if (mask != 0 && gdkdev->info.mode != GDK_MODE_DISABLED
	      && (gdkdev->info.has_cursor || mode == GDK_EXTENSION_EVENTS_ALL))
	    gdk_input_enable_window (window,gdkdev);
	  else
	    gdk_input_disable_window (window,gdkdev);
	}
    }
}

void
gdk_input_window_destroy (GdkWindow *window)
{
  GdkInputWindow *input_window;

  input_window = gdk_input_window_find (window);
  g_return_if_fail (input_window != NULL);

  gdk_input_windows = g_list_remove (gdk_input_windows,input_window);
  g_free (input_window);
}

void
gdk_input_init (void)
{
  gdk_input_devices = g_list_append (NULL, gdk_core_pointer);
  gdk_input_ignore_core = FALSE;
}

void
gdk_input_exit (void)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;

  for (tmp_list = gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (gdkdev != (GdkDevicePrivate *)gdk_core_pointer)
	{
	  gdk_device_set_mode ((GdkDevice *)gdkdev, GDK_MODE_DISABLED);

	  g_free (gdkdev->info.name);
	  g_free (gdkdev->info.axes);
	  g_free (gdkdev->info.keys);
	  g_free (gdkdev);
	}
    }

  g_list_free (gdk_input_devices);

  for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
    {
      g_free (tmp_list->data);
    }
  g_list_free (gdk_input_windows);
}

/**
 * gdk_device_get_axis:
 * @axis: a #GdkDevice
 * @axes: pointer to an array of axes
 * @use: the use to look for
 * @value: location to store the found value.
 * 
 * Interprets an array of double as axis values for a given device,
 * and locates the value in the array for a given axis use.
 * 
 * Return value: %TRUE if the given axis use was found, otherwies %FALSE
 **/
gboolean
gdk_device_get_axis (GdkDevice *device, gdouble *axes, GdkAxisUse use, gdouble *value)
{
  gint i;
  
  g_return_val_if_fail (device != NULL, FALSE);

  if (axes == NULL)
    return FALSE;
  
  for (i=0; i<device->num_axes; i++)
    if (device->axes[i].use == use)
      {
	if (value)
	  *value = axes[i];
	return TRUE;
      }
  
  return FALSE;
}
