/* GTK - The GIMP Toolkit
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

#ifndef __GTK_SOCKET_PRIVATE_H__
#define __GTK_SOCKET_PRIVATE_H__

#include "gtkplug.h"
#include "gtksocket.h"

struct _GtkSocketPrivate
{
  gint resize_count;

  guint16 request_width;
  guint16 request_height;
  guint16 current_width;
  guint16 current_height;

  GdkWindow *plug_window;
  GtkWidget *plug_widget;

  gshort xembed_version; /* -1 == not xembed */
  guint same_app  : 1;
  guint focus_in  : 1;
  guint have_size : 1;
  guint need_map  : 1;
  guint is_mapped : 1;
  guint active    : 1;

  GtkAccelGroup *accel_group;
  GtkWidget *toplevel;
};

/* from gtkplug.c */
void _gtk_plug_add_to_socket      (GtkPlug   *plug,
				   GtkSocket *socket_);
void _gtk_plug_remove_from_socket (GtkPlug   *plug,
				   GtkSocket *socket_);

#endif /* __GTK_SOCKET_PRIVATE_H__ */
