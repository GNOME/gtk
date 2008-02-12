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
#include <stdio.h>

#include "gdktypes.h"
#include "gdkprivate-fb.h"

GdkWindow *_gdk_fb_pointer_grab_window, *_gdk_fb_keyboard_grab_window, *_gdk_fb_pointer_grab_confine = NULL;
gboolean _gdk_fb_pointer_grab_owner_events;
gboolean _gdk_fb_keyboard_grab_owner_events;
GdkEventMask _gdk_fb_pointer_grab_events, _gdk_fb_keyboard_grab_events;

GdkDisplay *_gdk_display = NULL;
GdkScreen *_gdk_screen = NULL;
GdkWindow *_gdk_parent_root = NULL;

GdkFBWindow *gdk_root_window = NULL;
GdkFBDisplay *gdk_display = NULL;
GdkCursor *_gdk_fb_pointer_grab_cursor;
GdkGC *_gdk_fb_screen_gc = NULL;
GdkAtom _gdk_selection_property;
GdkFBAngle _gdk_fb_screen_angle = GDK_FB_0_DEGREES;
volatile gboolean _gdk_fb_is_active_vt = FALSE;
