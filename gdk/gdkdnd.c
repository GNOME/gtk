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
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <string.h>
#include "gdkx.h"
#include "gdk.h"

/* Nothing much here now, but we have to make a start some time ;-) */

void
gdk_dnd_set_drag_cursors(GdkCursor *default_cursor, GdkCursor *goahead_cursor)
{
  gdk_dnd.c->gdk_cursor_dragdefault =
    ((GdkCursorPrivate *)default_cursor)->xcursor;
  gdk_dnd.c->gdk_cursor_dragok = ((GdkCursorPrivate *)goahead_cursor)->xcursor;
  
  if(gdk_dnd.dnd_grabbed)
    {
      if(gdk_dnd.c->drag_pm_default)
	/* We were displaying pixmaps for the drag */
	{
	  gdk_window_hide(gdk_dnd.c->drag_pm_default);
	  gdk_window_unref(gdk_dnd.c->drag_pm_default);
	  if(gdk_dnd.c->drag_pm_ok)
	    {
	      gdk_window_hide(gdk_dnd.c->drag_pm_ok);
	      gdk_window_unref(gdk_dnd.c->drag_pm_ok);
	    }
	  gdk_dnd.c->drag_pm_default = gdk_dnd.c->drag_pm_ok = NULL;
	  g_list_free(gdk_dnd.c->xids);
	  gdk_dnd.c->xids = NULL;
	}
      gdk_dnd_display_drag_cursor(-1, -1,
				  gdk_dnd.dnd_drag_target?TRUE:FALSE,
				  TRUE);
    }
}

void
gdk_dnd_set_drag_shape(GdkWindow *default_pixmapwin,
		       GdkPoint *default_hotspot,
		       GdkWindow *goahead_pixmapwin,
		       GdkPoint *goahead_hotspot)
{
  g_return_if_fail(default_pixmapwin != NULL);
  
  g_list_free(gdk_dnd.c->xids); gdk_dnd.c->xids = NULL;
  if(gdk_dnd.c->drag_pm_default)
    {
      gdk_window_hide(gdk_dnd.c->drag_pm_default);
      gdk_window_unref(gdk_dnd.c->drag_pm_default);
    }
  if(gdk_dnd.c->drag_pm_ok)
    {
      gdk_window_hide(gdk_dnd.c->drag_pm_ok);
      gdk_window_unref(gdk_dnd.c->drag_pm_ok);
    }
  
  gdk_dnd.c->drag_pm_ok = NULL;
  
  gdk_window_ref(default_pixmapwin);
  gdk_dnd.c->drag_pm_default = default_pixmapwin;
  gdk_dnd.c->default_hotspot = *default_hotspot;
  gdk_dnd.c->xids = g_list_append(gdk_dnd.c->xids, GUINT_TO_POINTER (((GdkWindowPrivate *)default_pixmapwin)->xwindow));
  if(goahead_pixmapwin)
    {
      gdk_window_ref(goahead_pixmapwin);
      gdk_dnd.c->xids = g_list_append(gdk_dnd.c->xids, GUINT_TO_POINTER (((GdkWindowPrivate *)goahead_pixmapwin)->xwindow));
      gdk_dnd.c->drag_pm_ok = goahead_pixmapwin;
      gdk_dnd.c->ok_hotspot = *goahead_hotspot;
    }
  
  if(gdk_dnd.dnd_grabbed)
    {
      gdk_dnd_display_drag_cursor(-1, -1,
				  gdk_dnd.dnd_drag_target?TRUE:FALSE,
				  TRUE);
      XChangeActivePointerGrab (gdk_display,
				ButtonMotionMask |
				ButtonPressMask |
				ButtonReleaseMask |
				EnterWindowMask | LeaveWindowMask,
				None,
				CurrentTime);      
    }
}

void
gdk_dnd_display_drag_cursor(gint x, gint y, gboolean drag_ok,
			    gboolean change_made)
{
  if(!gdk_dnd.dnd_grabbed)
    return;
  
  if(gdk_dnd.c->drag_pm_default)
    {
      /* We're doing pixmaps here... */
      GdkWindow *mypix, *opix;
      GdkPoint *myhotspot;
      gint itmp;
      guint masktmp;
      Window wtmp;
      
      if(x == -2 && y == -2) /* Hide the cursors */
	{
	  gdk_window_hide(gdk_dnd.c->drag_pm_ok);
	  gdk_window_hide(gdk_dnd.c->drag_pm_default);
	  GDK_NOTE(DND, g_message("Hiding both drag cursors\n"));
	  return;
	}
      
      if(x == -1 && y == -1) /* We're supposed to find it out for ourselves */
	XQueryPointer(gdk_display, gdk_root_window,
		      &wtmp, &wtmp, &x, &y, &itmp, &itmp, &masktmp);
      
      if(drag_ok)
	{
	  GDK_NOTE(DND, g_message("Switching to drag_ok cursor\n"));
	  mypix = gdk_dnd.c->drag_pm_ok;
	  opix = gdk_dnd.c->drag_pm_default;
	  myhotspot = &gdk_dnd.c->ok_hotspot;
	}
      else
	{
	  GDK_NOTE(DND, g_message("Switching to drag_default cursor\n"));
	  mypix = gdk_dnd.c->drag_pm_default;
	  opix = gdk_dnd.c->drag_pm_ok;
	  myhotspot = &gdk_dnd.c->default_hotspot;
	}
      gdk_window_move(mypix, x - myhotspot->x, y - myhotspot->y);
      if(change_made)
	{
	  GDK_NOTE(DND, g_message("Cursors switched, hide & show\n"));
	  gdk_window_hide(opix);
	}
      gdk_window_move(mypix, x - myhotspot->x, y - myhotspot->y);
      if (change_made)
        {
	  gdk_window_show(mypix); /* There ought to be a way to know if
				     a window is already mapped etc. */
	}      
    }
  else if(change_made)
    {
      Cursor c;
      /* Move cursors around */
      if(drag_ok)
	c = gdk_dnd.c->gdk_cursor_dragok;
      else
	c = gdk_dnd.c->gdk_cursor_dragdefault;
      XChangeActivePointerGrab (gdk_display,
				ButtonMotionMask |
				ButtonPressMask |
				ButtonReleaseMask |
				EnterWindowMask | LeaveWindowMask,
				c,
				CurrentTime);
    }
}
