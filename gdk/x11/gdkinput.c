/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include <stdlib.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include "../config.h"
#include "gdk.h"
#include "gdkx.h"
#include "gdkprivate.h"
#include "gdkinput.h"


/* Forward declarations */

static gint gdk_input_enable_window (GdkWindow *window,
				     GdkDevicePrivate *gdkdev);
static gint gdk_input_disable_window (GdkWindow *window,
				      GdkDevicePrivate *gdkdev);
static GdkInputWindow *gdk_input_window_find (GdkWindow *window);
static GdkDevicePrivate *gdk_input_find_device (guint32 id);


/* Incorporate the specific routines depending on compilation options */

static GdkAxisUse gdk_input_core_axes[] = { GDK_AXIS_X, GDK_AXIS_Y };

static GdkDeviceInfo gdk_input_core_info =
{
  GDK_CORE_POINTER,
  "Core Pointer",
  GDK_SOURCE_MOUSE,
  GDK_MODE_SCREEN,
  TRUE,
  2,
  gdk_input_core_axes
};

/* Global variables  */

GdkInputVTable    gdk_input_vtable;
/* information about network port and host for gxid daemon */
gchar            *gdk_input_gxid_host;
gint              gdk_input_gxid_port;
gint              gdk_input_ignore_core;

/* Local variables */

static GList            *gdk_input_devices;
static GList            *gdk_input_windows;

#include "gdkinputnone.h"
#include "gdkinputcommon.h"
#include "gdkinputxfree.h"
#include "gdkinputgxi.h"

GList *
gdk_input_list_devices (void)
{
  return gdk_input_devices;
}

void
gdk_input_set_source (guint32 deviceid, GdkInputSource source)
{
  GdkDevicePrivate *gdkdev = gdk_input_find_device(deviceid);
  g_return_if_fail (gdkdev != NULL);

  gdkdev->info.source = source;
}

gint
gdk_input_set_mode (guint32 deviceid, GdkInputMode mode)
{
  if (deviceid == GDK_CORE_POINTER)
    return FALSE;

  if (gdk_input_vtable.set_mode)
    return gdk_input_vtable.set_mode(deviceid,mode);
  else
    return FALSE;
}

void
gdk_input_set_axes (guint32 deviceid, GdkAxisUse *axes)
{
  if (deviceid != GDK_CORE_POINTER && gdk_input_vtable.set_axes)
    gdk_input_vtable.set_axes (deviceid, axes);
}

void gdk_input_set_key (guint32 deviceid,
			guint   index,
			guint   keyval,
			GdkModifierType modifiers)
{
  if (deviceid != GDK_CORE_POINTER && gdk_input_vtable.set_key)
    gdk_input_vtable.set_key (deviceid, index, keyval, modifiers);
}

GdkTimeCoord *
gdk_input_motion_events (GdkWindow *window,
			 guint32 deviceid,
			 guint32 start,
			 guint32 stop,
			 gint *nevents_return)
{
  GdkWindowPrivate *window_private;
  XTimeCoord *xcoords;
  GdkTimeCoord *coords;
  int i;

  g_return_val_if_fail (window != NULL, NULL);
  window_private = (GdkWindowPrivate *) window;
  if (window_private->destroyed)
    return NULL;

  if (deviceid == GDK_CORE_POINTER)
    {
      xcoords = XGetMotionEvents (gdk_display,
				  window_private->xwindow,
				  start, stop, nevents_return);
      if (xcoords)
	{
	  coords = g_new (GdkTimeCoord, *nevents_return);
	  for (i=0; i<*nevents_return; i++)
	    {
	      coords[i].time = xcoords[i].time;
	      coords[i].x = xcoords[i].x;
	      coords[i].y = xcoords[i].y;
	      coords[i].pressure = 0.5;
	      coords[i].xtilt = 0.0;
	      coords[i].ytilt = 0.0;
	    }

	  XFree(xcoords);

	  return coords;
	}
      else
	return NULL;
    }
  else
    {
      if (gdk_input_vtable.motion_events)
	{
	  return gdk_input_vtable.motion_events(window,
						deviceid, start, stop,
						nevents_return);
	}
      else
	{
	  *nevents_return = 0;
	  return NULL;
	}
    }
}

static gint
gdk_input_enable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  if (gdk_input_vtable.enable_window)
    return gdk_input_vtable.enable_window (window, gdkdev);
  else
    return TRUE;
}

static gint
gdk_input_disable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  if (gdk_input_vtable.disable_window)
    return gdk_input_vtable.disable_window(window,gdkdev);
  else
    return TRUE;
}


static GdkInputWindow *
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
  GdkWindowPrivate *window_private;
  GList *tmp_list;
  GdkInputWindow *iw;

  g_return_if_fail (window != NULL);
  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
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

      gdk_input_windows = g_list_append(gdk_input_windows,iw);
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
	  gdk_input_windows = g_list_remove(gdk_input_windows,iw);
	  g_free(iw);
	}

      window_private->extension_events = 0;
    }

  for (tmp_list = gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDevicePrivate *gdkdev = (GdkDevicePrivate *)(tmp_list->data);

      if (gdkdev->info.deviceid != GDK_CORE_POINTER)
	{
	  if (mask != 0 && gdkdev->info.mode != GDK_MODE_DISABLED
	      && (gdkdev->info.has_cursor || mode == GDK_EXTENSION_EVENTS_ALL))
	    gdk_input_enable_window(window,gdkdev);
	  else
	    gdk_input_disable_window(window,gdkdev);
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
  g_free(input_window);
}

void
gdk_input_exit (void)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;

  for (tmp_list = gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (gdkdev->info.deviceid != GDK_CORE_POINTER)
	{
	  gdk_input_set_mode(gdkdev->info.deviceid,GDK_MODE_DISABLED);

	  g_free(gdkdev->info.name);
#ifndef XINPUT_NONE	  
	  g_free(gdkdev->axes);
#endif	  
	  g_free(gdkdev->info.axes);
	  g_free(gdkdev->info.keys);
	  g_free(gdkdev);
	}
    }

  g_list_free(gdk_input_devices);

  for (tmp_list = gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
    {
      g_free(tmp_list->data);
    }
  g_list_free(gdk_input_windows);
}

static GdkDevicePrivate *
gdk_input_find_device(guint32 id)
{
  GList *tmp_list = gdk_input_devices;
  GdkDevicePrivate *gdkdev;
  while (tmp_list)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (gdkdev->info.deviceid == id)
	return gdkdev;
      tmp_list = tmp_list->next;
    }
  return NULL;
}

void
gdk_input_window_get_pointer (GdkWindow       *window,
			      guint32	  deviceid,
			      gdouble         *x,
			      gdouble         *y,
			      gdouble         *pressure,
			      gdouble         *xtilt,
			      gdouble         *ytilt,
			      GdkModifierType *mask)
{
  if (gdk_input_vtable.get_pointer)
    gdk_input_vtable.get_pointer (window, deviceid, x, y, pressure,
				  xtilt, ytilt, mask);
}
