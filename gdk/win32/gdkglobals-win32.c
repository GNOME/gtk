/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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

#include "gdktypes.h"
#include "gdkprivate-win32.h"

HWND              gdk_root_window = NULL;
HDC		  gdk_display_hdc;
HINSTANCE	  gdk_dll_hinstance;
HINSTANCE	  gdk_app_hmodule;

WORD  		  cf_rtf;
WORD		  cf_utf8_string;

GdkAtom           utf8_string;
GdkAtom		  compound_text;
GdkAtom		  text_uri_list;

GdkAtom		  local_dnd;
GdkAtom		  gdk_win32_dropfiles;
GdkAtom		  gdk_ole2_dnd;

GdkAtom           _gdk_selection_property;

DWORD		  windows_version;

gint		  gdk_input_ignore_wintab = FALSE;
gint              gdk_event_func_from_window_proc = FALSE;
gint		  gdk_max_colors = 0;
