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

/*
 * Private uninstalled header defining things local to X windowing code
 */

#ifndef __GDK_PRIVATE_X11_H__
#define __GDK_PRIVATE_X11_H__

#include <gdk/gdkprivate.h>
#include "gdkx.h"

void          gdk_xid_table_insert     (XID             *xid,
					gpointer         data);
void          gdk_xid_table_remove     (XID              xid);
gint          gdk_send_xevent          (Window           window,
					gboolean         propagate,
					glong            event_mask,
					XEvent          *event_send);
GdkGC *       _gdk_x11_gc_new          (GdkDrawable     *drawable,
					GdkGCValues     *values,
					GdkGCValuesMask  values_mask);
GdkColormap * gdk_colormap_lookup      (Colormap         xcolormap);
GdkVisual *   gdk_visual_lookup        (Visual          *xvisual);

void gdk_window_add_colormap_windows (GdkWindow *window);

/* Please see gdkwindow.c for comments on how to use */ 
Window gdk_window_xid_at        (Window    base,
				 gint      bx,
				 gint      by,
				 gint      x,
				 gint      y,
				 GList    *excludes,
				 gboolean  excl_child);
Window gdk_window_xid_at_coords (gint      x,
				 gint      y,
				 GList    *excludes,
				 gboolean  excl_child);

/* Routines from gdkgeometry-x11.c */
void _gdk_window_init_position     (GdkWindow     *window);
void _gdk_window_move_resize_child (GdkWindow     *window,
                                    gint           x,
                                    gint           y,
                                    gint           width,
                                    gint           height);
void _gdk_window_process_expose    (GdkWindow     *window,
                                    gulong         serial,
                                    GdkRectangle  *area);

extern GdkDrawableClass  _gdk_x11_drawable_class;
extern gboolean	         gdk_use_xshm;
extern Atom		 gdk_wm_delete_window;
extern Atom		 gdk_wm_take_focus;
extern Atom		 gdk_wm_protocols;
extern Atom		 gdk_wm_window_protocols[];
extern GdkWindow	*selection_owner[];
extern gboolean          gdk_null_window_warnings;
extern const int         gdk_nevent_masks;
extern const int         gdk_event_mask_table[];

extern GdkWindowObject *gdk_xgrab_window;  /* Window that currently holds the
					    * x pointer grab
					    */

#ifdef USE_XIM
extern GdkICPrivate *gdk_xim_ic;		/* currently using IC */
extern GdkWindow *gdk_xim_window;	        /* currently using Window */
#endif /* USE_XIM */


#endif /* __GDK_PRIVATE_X11_H__ */


