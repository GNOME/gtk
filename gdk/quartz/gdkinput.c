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

#include "config.h"
#include <stdlib.h>

#include "gdkprivate-quartz.h"
#include "gdkinput.h"
#include "gdkprivate.h"
#include "gdkinputprivate.h"
#include <gdkdevice.h>
#include <gdkdeviceprivate.h>

/* Addition used for extension_events mask */
#define GDK_ALL_DEVICES_MASK (1<<30)

GdkDevice *_gdk_core_pointer = NULL;

/* Global variables  */

gchar            *_gdk_input_gxid_host;
gint              _gdk_input_gxid_port;
gint              _gdk_input_ignore_core;
GList            *_gdk_input_windows;
GList            *_gdk_input_devices;


GList *
gdk_devices_list (void)
{
  return _gdk_input_devices;
}

GList *
gdk_display_list_devices (GdkDisplay *dpy)
{
  return _gdk_input_devices;
}

static void
_gdk_input_select_device_events (GdkWindow *impl_window,
                                 GdkDevice *device)
{
  guint event_mask;
  GdkWindowObject *w;
  GdkInputWindow *iw;
  GdkInputMode mode;
  gboolean has_cursor;
  GdkDeviceType type;
  GList *l;

  event_mask = 0;
  iw = ((GdkWindowObject *)impl_window)->input_window;

  g_object_get (device,
                "type", &type,
                "input-mode", &mode,
                "has-cursor", &has_cursor,
                NULL);

  if (iw == NULL ||
      mode == GDK_MODE_DISABLED ||
      type == GDK_DEVICE_TYPE_MASTER)
    return;

  for (l = _gdk_input_windows; l != NULL; l = l->next)
    {
      w = l->data;

      if (has_cursor || (w->extension_events & GDK_ALL_DEVICES_MASK))
        {
          event_mask = w->extension_events;

          if (event_mask)
            event_mask |= GDK_PROXIMITY_OUT_MASK
                | GDK_BUTTON_PRESS_MASK
                | GDK_BUTTON_RELEASE_MASK;

          gdk_window_set_device_events ((GdkWindow *) w, device, event_mask);
        }
    }
}

gint
_gdk_input_enable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  return TRUE;
}

gint
_gdk_input_disable_window (GdkWindow *window, GdkDevicePrivate *gdkdev)
{
  return TRUE;
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
gdk_input_set_extension_events (GdkWindow        *window,
                                gint              mask,
                                GdkExtensionMode  mode)
{
  GdkWindowObject *window_private;
  GdkWindowObject *impl_window;
  GList *tmp_list;
  GdkInputWindow *iw;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_WINDOW_IS_QUARTZ (window));

  window_private = (GdkWindowObject*) window;
  impl_window = (GdkWindowObject *)_gdk_window_get_impl_window (window);

  if (mode == GDK_EXTENSION_EVENTS_NONE)
    mask = 0;

  if (mask != 0)
    {
      iw = g_new (GdkInputWindow, 1);

      iw->window = window;
      iw->mode = mode;

      iw->obscuring = NULL;
      iw->num_obscuring = 0;
      iw->grabbed = FALSE;

      _gdk_input_windows = g_list_append (_gdk_input_windows, iw);
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
          _gdk_input_windows = g_list_remove (_gdk_input_windows,iw);
          g_free (iw);
        }

      window_private->extension_events = 0;
    }

  for (tmp_list = _gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      GdkDevice *dev = tmp_list->data;

      _gdk_input_select_device_events (GDK_WINDOW (impl_window), dev);
    }
}

void
_gdk_input_window_destroy (GdkWindow *window)
{
  GdkInputWindow *input_window;

  input_window = _gdk_input_window_find (window);
  g_return_if_fail (input_window != NULL);

  _gdk_input_windows = g_list_remove (_gdk_input_windows,input_window);
  g_free (input_window);
}

void
_gdk_input_check_extension_events (GdkDevice *device)
{
}

void
_gdk_input_init (void)
{
  GdkDeviceManager *device_manager;
  GList *list, *l;

  device_manager = gdk_display_get_device_manager (_gdk_display);

  /* For backward compatibility, just add floating devices that are
   * not keyboards.
   */
  list = gdk_device_manager_list_devices (device_manager,
                                          GDK_DEVICE_TYPE_FLOATING);
  for (l = list; l; l = l->next)
    {
      GdkDevice *device = l->data;

      if (gdk_device_get_source(device) == GDK_SOURCE_KEYBOARD)
        continue;

      _gdk_input_devices = g_list_prepend (_gdk_input_devices, l->data);
    }

  g_list_free (list);

  /* Now set "core" pointer to the first master device that is a pointer.
   */
  list = gdk_device_manager_list_devices (device_manager,
                                          GDK_DEVICE_TYPE_MASTER);

  for (l = list; l; l = l->next)
    {
      GdkDevice *device = list->data;

      if (gdk_device_get_source(device) != GDK_SOURCE_MOUSE)
        continue;

      _gdk_display->core_pointer = device;
      break;
    }

  g_list_free (list);

  /* Add the core pointer to the devices list */
  _gdk_input_devices = g_list_prepend (_gdk_input_devices,
                                       _gdk_display->core_pointer);

  _gdk_input_ignore_core = FALSE;
}

void
_gdk_input_exit (void)
{
  GList *tmp_list;
  GdkDevicePrivate *gdkdev;

  for (tmp_list = _gdk_input_devices; tmp_list; tmp_list = tmp_list->next)
    {
      gdkdev = (GdkDevicePrivate *)(tmp_list->data);
      if (gdkdev != (GdkDevicePrivate *)_gdk_core_pointer)
	{
	  gdk_device_set_mode ((GdkDevice *)gdkdev, GDK_MODE_DISABLED);
	  g_object_unref(gdkdev);
	}
    }

  g_list_free (_gdk_input_devices);

  for (tmp_list = _gdk_input_windows; tmp_list; tmp_list = tmp_list->next)
    {
      g_free (tmp_list->data);
    }
  g_list_free (_gdk_input_windows);
}
