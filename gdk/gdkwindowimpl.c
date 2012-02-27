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

#include "gdkwindowimpl.h"

#include "gdkinternals.h"


G_DEFINE_TYPE (GdkWindowImpl, gdk_window_impl, G_TYPE_OBJECT);

static gboolean
gdk_window_impl_beep (GdkWindow *window)
{
  /* FALSE means windows can't beep, so the display will be
   * made to beep instead. */
  return FALSE;
}

static void
gdk_window_impl_class_init (GdkWindowImplClass *impl_class)
{
  impl_class->beep = gdk_window_impl_beep;
}

static void
gdk_window_impl_init (GdkWindowImpl *impl)
{
}
