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

/* This file should really be one level up, in the backend-independent
 * GDK, and the x11/gdkinput.c could also be removed.
 * 
 * That stuff in x11/gdkinput.c which really *is* X11-dependent should
 * be in x11/gdkinput-x11.c.
 */

#include <config.h>

#include "gdkdisplay.h"
#include "gdkinput.h"
#include "gdkprivate.h"

/* When ther necessary stuff is in
 * gdkinput.h, gdkinternals.h and
 * gdkprivate.h, these includes shouldn't be here.
 */

#include <windows.h>
#ifdef HAVE_WINTAB
#include <wintab.h>
#endif

#include "gdkinput-win32.h"

static GdkDeviceAxis gdk_input_core_axes[] = {
  { GDK_AXIS_X, 0, 0 },
  { GDK_AXIS_Y, 0, 0 }
};

/* Global variables  */

gint              _gdk_input_ignore_core;

GList            *_gdk_input_devices;
GList            *_gdk_input_windows;

void
_gdk_init_input_core (GdkDisplay *display)
{
  display->core_pointer = g_object_new (GDK_TYPE_DEVICE, NULL);
  
  display->core_pointer->name = "Core Pointer";
  display->core_pointer->source = GDK_SOURCE_MOUSE;
  display->core_pointer->mode = GDK_MODE_SCREEN;
  display->core_pointer->has_cursor = TRUE;
  display->core_pointer->num_axes = 2;
  display->core_pointer->axes = gdk_input_core_axes;
  display->core_pointer->num_keys = 0;
  display->core_pointer->keys = NULL;
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
      static const GTypeInfo object_info =
      {
        sizeof (GdkDeviceClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_device_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDevicePrivate),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkDevice",
                                            &object_info, 0);
    }
  
  return object_type;
}

GList *
gdk_devices_list (void)
{
#ifdef HAVE_WINTAB
  _gdk_input_wintab_init_check ();
#endif /* HAVE_WINTAB */
  return _gdk_input_devices;
}

GList *
gdk_display_list_devices (GdkDisplay *dpy)
{
#ifdef HAVE_WINTAB
  _gdk_input_wintab_init_check ();
#endif /* HAVE_WINTAB */
  return _gdk_input_devices;
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

  if (n_events)
    *n_events = 0;
  if (events)
    *events = NULL;

  if (GDK_WINDOW_DESTROYED (window))
    return FALSE;
    
  if (GDK_IS_CORE (device))
    return FALSE;
  else
    return _gdk_device_get_history (device, window, start, stop, events, n_events);
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

  for (tmp_list=_gdk_input_windows; tmp_list; tmp_list=tmp_list->next)
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
  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (mode == GDK_EXTENSION_EVENTS_NONE)
    mask = 0;

  if (mask != 0)
    {
#ifdef HAVE_WINTAB
      _gdk_input_wintab_init_check ();
#endif /* HAVE_WINTAB */
      iw = g_new(GdkInputWindow,1);

      iw->window = window;
      iw->mode = mode;

      iw->obscuring = NULL;
      iw->num_obscuring = 0;
      iw->grabbed = FALSE;

      _gdk_input_windows = g_list_append(_gdk_input_windows,iw);
      window_private->extension_events = mask;

      /* Add enter window events to the event mask */
      if (g_list_length (_gdk_input_devices) > 1)
	gdk_window_set_events (window,
			       gdk_window_get_events (window) | 
			       GDK_ENTER_NOTIFY_MASK);
    }
  else
    {
      iw = _gdk_input_window_find (window);
      if (iw)
	{
	  _gdk_input_windows = g_list_remove(_gdk_input_windows,iw);
	  g_free(iw);
	}

      window_private->extension_events = 0;
    }

  for (tmp_list = _gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
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

  input_window = _gdk_input_window_find (window);
  g_return_if_fail (input_window != NULL);

  _gdk_input_windows = g_list_remove (_gdk_input_windows,input_window);
  g_free(input_window);
}

void
_gdk_input_exit (void)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;

  for (tmp_list = _gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (!GDK_IS_CORE (gdkdev))
	{
	  gdk_device_set_mode (&gdkdev->info, GDK_MODE_DISABLED);

	  g_free(gdkdev->info.name);
	  g_free(gdkdev->axes);
	  g_free(gdkdev->info.axes);
	  g_free(gdkdev->info.keys);
	  g_free(gdkdev);
	}
    }

  g_list_free(_gdk_input_devices);

  for (tmp_list = _gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
    g_free(tmp_list->data);

  g_list_free(_gdk_input_windows);
}

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

gboolean
gdk_device_set_mode (GdkDevice   *device,
		     GdkInputMode mode)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;
  GdkInputMode old_mode;
  GdkInputWindow *input_window;

  if (GDK_IS_CORE (device))
    return FALSE;

  gdkdev = (GdkDevicePrivate *)device;

  if (device->mode == mode)
    return TRUE;

  old_mode = device->mode;
  device->mode = mode;

  if (mode == GDK_MODE_WINDOW)
    {
      device->has_cursor = FALSE;
      for (tmp_list = _gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	{
	  input_window = (GdkInputWindow *)tmp_list->data;
	  if (input_window->mode != GDK_EXTENSION_EVENTS_CURSOR)
	    _gdk_input_enable_window (input_window->window, gdkdev);
	  else
	    if (old_mode != GDK_MODE_DISABLED)
	      _gdk_input_disable_window (input_window->window, gdkdev);
	}
    }
  else if (mode == GDK_MODE_SCREEN)
    {
      device->has_cursor = TRUE;
      for (tmp_list = _gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	_gdk_input_enable_window (((GdkInputWindow *)tmp_list->data)->window,
				  gdkdev);
    }
  else  /* mode == GDK_MODE_DISABLED */
    {
      for (tmp_list = _gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
	{
	  input_window = (GdkInputWindow *)tmp_list->data;
	  if (old_mode != GDK_MODE_WINDOW ||
	      input_window->mode != GDK_EXTENSION_EVENTS_CURSOR)
	    _gdk_input_disable_window (input_window->window, gdkdev);
	}
    }

  return TRUE;
}
