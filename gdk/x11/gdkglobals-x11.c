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

#include <stdio.h>

#include "gdktypes.h"
#include "gdkprivate-x11.h"
#include "config.h"

gboolean          _gdk_use_xshm = TRUE;
gchar            *_gdk_display_name = NULL;
Display          *gdk_display = NULL;
gint              _gdk_screen;
Window            _gdk_root_window;
Window            _gdk_leader_window;
Atom              _gdk_wm_window_protocols[3];
GdkAtom           _gdk_selection_property;

GdkWindowObject *_gdk_xgrab_window = NULL;  /* Window that currently holds the
                                            *	x pointer grab
                                            */
