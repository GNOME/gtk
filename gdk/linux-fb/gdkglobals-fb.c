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

/*
 * Modified by the GTK+ Team and others 1997-1999.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/. 
 */

#include <stdio.h>

#include "gdktypes.h"
#include "gdkprivate-fb.h"
#include "config.h"

const gchar            *gdk_progclass = "none";
gboolean          gdk_null_window_warnings = TRUE;

GdkWindow *_gdk_fb_pointer_grab_window, *_gdk_fb_keyboard_grab_window, *_gdk_fb_pointer_grab_confine = NULL;
GdkEventMask _gdk_fb_pointer_grab_events, _gdk_fb_keyboard_grab_events;

GdkFBWindow *gdk_root_window = NULL;
GdkFBDisplay *gdk_display = NULL;
GdkCursor *_gdk_fb_pointer_grab_cursor;
