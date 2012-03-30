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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
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

  _gdk_input_wintab_init_check (GDK_DEVICE_MANAGER (device_manager));

}
