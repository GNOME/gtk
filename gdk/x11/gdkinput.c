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

#include <config.h>

#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "gdkx.h"
#include "gdkinput.h"
#include "gdkprivate.h"
#include "gdkinputprivate.h"
#include "gdkscreen-x11.h"
#include "gdkdisplay-x11.h"
#include "gdkalias.h"

static GdkDeviceAxis gdk_input_core_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 }
};

void
_gdk_init_input_core (GdkDisplay *display)
{
  GdkDevicePrivate *private;
  
  display->core_pointer = g_object_new (GDK_TYPE_DEVICE, NULL);
  private = (GdkDevicePrivate *)display->core_pointer;
  
  display->core_pointer->name = "Core Pointer";
  display->core_pointer->source = GDK_SOURCE_MOUSE;
  display->core_pointer->mode = GDK_MODE_SCREEN;
  display->core_pointer->has_cursor = TRUE;
  display->core_pointer->num_axes = 2;
  display->core_pointer->axes = gdk_input_core_axes;
  display->core_pointer->num_keys = 0;
  display->core_pointer->keys = NULL;

  private->display = display;
}

GType
gdk_device_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
	{
	  sizeof (GdkDeviceClass),
	  (GBaseInitFunc) NULL,
	  (GBaseFinalizeFunc) NULL,
	  (GClassInitFunc) NULL,
	  NULL,           /* class_finalize */
	  NULL,           /* class_data */
	  sizeof (GdkDevicePrivate),
	  0,              /* n_preallocs */
	  (GInstanceInitFunc) NULL,
	};
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            g_intern_static_string ("GdkDevice"),
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
  return gdk_display_list_devices (gdk_display_get_default ());
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
gdk_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  
  return GDK_DISPLAY_X11 (display)->input_devices;
}

void
gdk_device_set_source (GdkDevice      *device,
		       GdkInputSource  source)
{
  g_return_if_fail (device != NULL);

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

/**
 * gdk_device_get_history:
 * @device: a #GdkDevice
 * @window: the window with respect to which which the event coordinates will be reported
 * @start: starting timestamp for range of events to return
 * @stop: ending timestamp for the range of events to return
 * @events: location to store a newly-allocated array of #GdkTimeCoord, or %NULL
 * @n_events: location to store the length of @events, or %NULL
 * 
 * Obtains the motion history for a device; given a starting and
 * ending timestamp, return all events in the motion history for
 * the device in the given range of time. Some windowing systems
 * do not support motion history, in which case, %FALSE will
 * be returned. (This is not distinguishable from the case where
 * motion history is supported and no events were found.)
 * 
 * Return value: %TRUE if the windowing system supports motion history and
 *  at least one event was found.
 **/
gboolean
gdk_device_get_history  (GdkDevice         *device,
			 GdkWindow         *window,
			 guint32            start,
			 guint32            stop,
			 GdkTimeCoord    ***events,
			 gint              *n_events)
{
  GdkTimeCoord **coords = NULL;
  gboolean result = FALSE;
  int tmp_n_events = 0;
  int i;

  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (GDK_WINDOW_DESTROYED (window))
    /* Nothing */ ;
  else if (GDK_IS_CORE (device))
    {
      XTimeCoord *xcoords;
      
      xcoords = XGetMotionEvents (GDK_DRAWABLE_XDISPLAY (window),
				  GDK_DRAWABLE_XID (window),
				  start, stop, &tmp_n_events);
      if (xcoords)
	{
	  coords = _gdk_device_allocate_history (device, tmp_n_events);
	  for (i=0; i<tmp_n_events; i++)
	    {
	      coords[i]->time = xcoords[i].time;
	      coords[i]->axes[0] = xcoords[i].x;
	      coords[i]->axes[1] = xcoords[i].y;
	    }

	  XFree (xcoords);

	  result = TRUE;
	}
      else
	result = FALSE;
    }
  else
    result = _gdk_device_get_history (device, window, start, stop, &coords, &tmp_n_events);

  if (n_events)
    *n_events = tmp_n_events;
  if (events)
    *events = coords;
  else if (coords)
    gdk_device_free_history (coords, tmp_n_events);

  return result;
}

GdkTimeCoord ** 
_gdk_device_allocate_history (GdkDevice *device,
			      gint       n_events)
{
  GdkTimeCoord **result = g_new (GdkTimeCoord *, n_events);
  gint i;

  for (i=0; i<n_events; i++)
    result[i] = g_malloc (sizeof (GdkTimeCoord) -
			  sizeof (double) * (GDK_MAX_TIMECOORD_AXES - device->num_axes));

  return result;
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

GdkInputWindow *
_gdk_input_window_find(GdkWindow *window)
{
  GList *tmp_list;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));

  for (tmp_list=display_x11->input_windows; tmp_list; tmp_list=tmp_list->next)
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
  GdkDisplayX11 *display_x11;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  window_private = (GdkWindowObject*) window;
  display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));
  if (GDK_WINDOW_DESTROYED (window))
    return;

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

      display_x11->input_windows = g_list_append(display_x11->input_windows,iw);
      window_private->extension_events = mask;

      /* Add enter window events to the event mask */
      /* FIXME, this is not needed for XINPUT_NONE */
      gdk_window_set_events (window,
			     gdk_window_get_events (window) | 
			     GDK_ENTER_NOTIFY_MASK);
    }
  else
    {
      iw = _gdk_input_window_find (window);
      if (iw)
	{
	  display_x11->input_windows = g_list_remove(display_x11->input_windows,iw);
	  g_free(iw);
	}

      window_private->extension_events = 0;
    }

  for (tmp_list = display_x11->input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDevicePrivate *gdkdev = tmp_list->data;

      if (!GDK_IS_CORE (gdkdev))
	{
	  if (mask != 0 && gdkdev->info.mode != GDK_MODE_DISABLED
	      && (gdkdev->info.has_cursor || mode == GDK_EXTENSION_EVENTS_ALL))
	    _gdk_input_enable_window (window,gdkdev);
	  else
	    _gdk_input_disable_window (window,gdkdev);
	}
    }
}

void
_gdk_input_window_destroy (GdkWindow *window)
{
  GdkInputWindow *input_window;
  GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (GDK_WINDOW_DISPLAY (window));

  input_window = _gdk_input_window_find (window);
  g_return_if_fail (input_window != NULL);

  display_x11->input_windows = g_list_remove (display_x11->input_windows, input_window);
  g_free(input_window);
}

void
_gdk_input_exit (void)
{
  GList *tmp_list;
  GSList *display_list;
  GdkDevicePrivate *gdkdev;

  for (display_list = _gdk_displays ; display_list ; display_list = display_list->next)
    {
      GdkDisplayX11 *display_x11 = GDK_DISPLAY_X11 (display_list->data);
      
      for (tmp_list = display_x11->input_devices; tmp_list; tmp_list = tmp_list->next)
	{
	  gdkdev = (GdkDevicePrivate *)(tmp_list->data);
	  if (!GDK_IS_CORE (gdkdev))
	    {
	      gdk_device_set_mode (&gdkdev->info, GDK_MODE_DISABLED);
	      
	      g_free(gdkdev->info.name);
#ifndef XINPUT_NONE	  
	      g_free(gdkdev->axes);
#endif	  
	      g_free(gdkdev->info.axes);
	      g_free(gdkdev->info.keys);
	      g_free(gdkdev);
	    }
	}
      
      g_list_free(display_x11->input_devices);
      
      for (tmp_list = display_x11->input_windows; tmp_list; tmp_list = tmp_list->next)
	g_free(tmp_list->data);
      
      g_list_free(display_x11->input_windows);
    }
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
  
  for (i=0; i<device->num_axes; i++)
    if (device->axes[i].use == use)
      {
	if (value)
	  *value = axes[i];
	return TRUE;
      }
  
  return FALSE;
}

#define __GDK_INPUT_C__
#include "gdkaliasdef.c"
