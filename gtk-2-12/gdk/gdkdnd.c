/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1999 Peter Mattis, Spencer Kimball and Josh MacDonald
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
#include <gdkdnd.h>
#include <gdkdrawable.h>
#include <gdkdisplay.h>
#include "gdkalias.h"

/**
 * gdk_drag_find_window:
 * @context: a #GdkDragContext.
 * @drag_window: a window which may be at the pointer position, but
 *      should be ignored, since it is put up by the drag source as an icon.
 * @x_root: the x position of the pointer in root coordinates.
 * @y_root: the y position of the pointer in root coordinates.
 * @dest_window: location to store the destination window in.
 * @protocol: location to store the DND protocol in.
 * 
 * Finds the destination window and DND protocol to use at the
 * given pointer position. 
 *
 * This function is called by the drag source to obtain the 
 * @dest_window and @protocol parameters for gdk_drag_motion().
 **/
void
gdk_drag_find_window (GdkDragContext  *context,
		      GdkWindow       *drag_window,
		      gint             x_root,
		      gint             y_root,
		      GdkWindow      **dest_window,
		      GdkDragProtocol *protocol)
{
  gdk_drag_find_window_for_screen (context, drag_window,
				   gdk_drawable_get_screen (context->source_window),
				   x_root, y_root, dest_window, protocol);
}

/**
 * gdk_drag_get_protocol:
 * @xid: the X id of the destination window.
 * @protocol: location where the supported DND protocol is returned.
 * 
 * Finds out the DND protocol supported by a window.
 * 
 * Return value: the X id of the window where the drop should happen. This 
 *    may be @xid or the X id of a proxy window, or None if @xid doesn't
 *    support Drag and Drop.
 **/
guint32
gdk_drag_get_protocol (guint32          xid,
		       GdkDragProtocol *protocol)
{
  return gdk_drag_get_protocol_for_display (gdk_display_get_default (), xid, protocol);
}

#define __GDK_DND_C__
#include "gdkaliasdef.c"
