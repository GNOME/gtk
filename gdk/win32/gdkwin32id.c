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

#include "config.h"
#include <gdk/gdk.h>

#include "gdkprivate-win32.h"
#include "gdkdisplay-win32.h"

#define GDK_DISPLAY_HANDLE_HT(d) GDK_WIN32_DISPLAY(d)->display_surface_record->handle_ht

void
gdk_win32_display_handle_table_insert (GdkDisplay *display,
                                       HANDLE     *handle,
                                       gpointer    data)
{
  g_return_if_fail (handle != NULL);

  g_hash_table_insert (GDK_DISPLAY_HANDLE_HT (display), handle, data);
}

void
gdk_win32_display_handle_table_remove (GdkDisplay *display,
                                       HANDLE      handle)
{
  g_hash_table_remove (GDK_DISPLAY_HANDLE_HT (display), &handle);
}

gpointer
gdk_win32_display_handle_table_lookup_ (GdkDisplay *display,
                                        HWND        handle)
{
  gpointer data = NULL;
  if (display == NULL)
    display = gdk_display_get_default ();

  data = g_hash_table_lookup (GDK_DISPLAY_HANDLE_HT (display), &handle);

  return data;
}

gpointer
gdk_win32_handle_table_lookup (HWND handle)
{
  return gdk_win32_display_handle_table_lookup_ (NULL, handle);
}
