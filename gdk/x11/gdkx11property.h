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

#pragma once

#if !defined (__GDKX_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gdk/x11/gdkx.h> can be included directly."
#endif

#include <gdk/gdk.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

G_BEGIN_DECLS

GDK_DEPRECATED_IN_4_18
Atom                  gdk_x11_get_xatom_by_name_for_display (GdkDisplay  *display,
                                                             const char *atom_name);
GDK_DEPRECATED_IN_4_18
const char *         gdk_x11_get_xatom_name_for_display (GdkDisplay  *display,
                                                          Atom         xatom);

G_END_DECLS

