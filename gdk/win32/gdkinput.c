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

#include "config.h"

#include "gdkdisplay.h"
#include "gdkdevice.h"
#include "gdkdisplayprivate.h"

#include "gdkprivate-win32.h"
#include "gdkdevicemanager-win32.h"

gint              _gdk_input_ignore_core;

GList            *_gdk_input_devices;
GList            *_gdk_input_windows;

GList *
gdk_devices_list (void)
{
  return _gdk_win32_display_list_devices (_gdk_display);
}

GList *
_gdk_win32_display_list_devices (GdkDisplay *dpy)
{
  g_return_val_if_fail (dpy == _gdk_display, NULL);

  return _gdk_input_devices;
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
  GdkDeviceManager *device_manager;
  GList *devices, *d;

  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;

  if (mode == GDK_EXTENSION_EVENTS_NONE)
    mask = 0;

  window->extension_events = mask;

  device_manager = gdk_display_get_device_manager (_gdk_display);
  devices = gdk_device_manager_list_devices (device_manager,
                                             GDK_DEVICE_TYPE_FLOATING);

  for (d = devices; d; d = d->next)
    {
      GdkDevice *dev;
      gint dev_mask;

      dev = d->data;
      dev_mask = mask;

      if (gdk_device_get_mode (dev) == GDK_MODE_DISABLED ||
          (!gdk_device_get_has_cursor (dev) && mode == GDK_EXTENSION_EVENTS_CURSOR))
        dev_mask = 0;

      gdk_window_set_device_events (window, dev, mask);
    }

  g_list_free (devices);
}

void
_gdk_input_init (GdkDisplay *display)
{
  GdkDeviceManagerWin32 *device_manager;

  _gdk_input_ignore_core = FALSE;

  device_manager = g_object_new (GDK_TYPE_DEVICE_MANAGER_WIN32,
                                 "display", display,
                                 NULL);
  display->device_manager = GDK_DEVICE_MANAGER (device_manager);

  display->core_pointer = device_manager->core_pointer;

  _gdk_input_devices = g_list_append (NULL, display->core_pointer);
  _gdk_input_devices = g_list_concat (_gdk_input_devices,
                                      g_list_copy (device_manager->wintab_devices));
}
