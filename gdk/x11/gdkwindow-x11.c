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

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <netinet/in.h>
#include "gdk.h"
#include "config.h"

#include "gdkwindow.h"
#include "gdkinputprivate.h"
#include "gdkprivate.h"
#include "gdkx.h"
#include "MwmUtil.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#ifdef HAVE_SHAPE_EXT
#include <X11/extensions/shape.h>
#endif

const int gdk_event_mask_table[20] =
{
  ExposureMask,
  PointerMotionMask,
  PointerMotionHintMask,
  ButtonMotionMask,
  Button1MotionMask,
  Button2MotionMask,
  Button3MotionMask,
  ButtonPressMask | OwnerGrabButtonMask,
  ButtonReleaseMask | OwnerGrabButtonMask,
  KeyPressMask,
  KeyReleaseMask,
  EnterWindowMask,
  LeaveWindowMask,
  FocusChangeMask,
  StructureNotifyMask,
  PropertyChangeMask,
  VisibilityChangeMask,
  0,				/* PROXIMITY_IN */
  0,				/* PROXIMTY_OUT */
  SubstructureNotifyMask
};
const int gdk_nevent_masks = sizeof (gdk_event_mask_table) / sizeof (int);

/* Forward declarations */
static gboolean gdk_window_gravity_works (void);
static void     gdk_window_set_static_win_gravity (GdkWindow *window, 
						   gboolean   on);
static gboolean gdk_window_have_shape_ext (void);

/* internal function created for and used by gdk_window_xid_at_coords */
Window
gdk_window_xid_at (Window   base,
		   gint     bx,
		   gint     by,
		   gint     x,
		   gint     y, 
		   GList   *excludes,
		   gboolean excl_child)
{
  GdkWindow *window;
  GdkDrawablePrivate *private;
  Display *disp;
  Window *list = NULL;
  Window child = 0, parent_win = 0, root_win = 0;
  int i;
  unsigned int ww, wh, wb, wd, num;
  int wx, wy;
  
  window = (GdkWindow*) &gdk_root_parent;
  private = (GdkDrawablePrivate*) window;
  disp = private->xdisplay;
  if (!XGetGeometry (disp, base, &root_win, &wx, &wy, &ww, &wh, &wb, &wd))
    return 0;
  wx += bx;
  wy += by;
  
  if (!((x >= wx) &&
	(y >= wy) &&
	(x < (int) (wx + ww)) &&
	(y < (int) (wy + wh))))
    return 0;
  
  if (!XQueryTree (disp, base, &root_win, &parent_win, &list, &num))
    return base;
  
  if (list)
    {
      for (i = num - 1; ; i--)
	{
	  if ((!excl_child) || (!g_list_find (excludes, (gpointer *) list[i])))
	    {
	      if ((child = gdk_window_xid_at (list[i], wx, wy, x, y, excludes, excl_child)) != 0)
		{
		  XFree (list);
		  return child;
		}
	    }
	  if (!i)
	    break;
	}
      XFree (list);
    }
  return base;
}

/* 
 * The following fucntion by The Rasterman <raster@redhat.com>
 * This function returns the X Window ID in which the x y location is in 
 * (x and y being relative to the root window), excluding any windows listed
 * in the GList excludes (this is a list of X Window ID's - gpointer being
 * the Window ID).
 * 
 * This is primarily designed for internal gdk use - for DND for example
 * when using a shaped icon window as the drag object - you exclude the
 * X Window ID of the "icon" (perhaps more if excludes may be needed) and
 * You can get back an X Window ID as to what X Window ID is infact under
 * those X,Y co-ordinates.
 */
Window
gdk_window_xid_at_coords (gint     x,
			  gint     y,
			  GList   *excludes,
			  gboolean excl_child)
{
  GdkWindow *window;
  GdkDrawablePrivate *private;
  Display *disp;
  Window *list = NULL;
  Window root, child = 0, parent_win = 0, root_win = 0;
  unsigned int num;
  int i;

  window = (GdkWindow*) &gdk_root_parent;
  private = (GdkDrawablePrivate*) window;
  disp = private->xdisplay;
  root = private->xwindow;
  num = g_list_length (excludes);
  
  XGrabServer (disp);
  if (!XQueryTree (disp, root, &root_win, &parent_win, &list, &num))
    {
      XUngrabServer (disp);
      return root;
    }
  if (list)
    {
      i = num - 1;
      do
	{
	  XWindowAttributes xwa;
	  
	  XGetWindowAttributes (disp, list [i], &xwa);
	  
	  if (xwa.map_state != IsViewable)
	    continue;
	  
	  if (excl_child && g_list_find (excludes, (gpointer *) list[i]))
	    continue;
	  
	  if ((child = gdk_window_xid_at (list[i], 0, 0, x, y, excludes, excl_child)) == 0)
	    continue;
	  
	  if (excludes)
	    {
	      if (!g_list_find (excludes, (gpointer *) child))
		{
		  XFree (list);
		  XUngrabServer (disp);
		  return child;
		}
	    }
	  else
	    {
	      XFree (list);
	      XUngrabServer (disp);
	      return child;
	    }
	} while (--i > 0);
      XFree (list);
    }
  XUngrabServer (disp);
  return root;
}

void
gdk_window_init (void)
{
  XWindowAttributes xattributes;
  unsigned int width;
  unsigned int height;
  unsigned int border_width;
  unsigned int depth;
  int x, y;
  
  XGetGeometry (gdk_display, gdk_root_window, &gdk_root_window,
		&x, &y, &width, &height, &border_width, &depth);
  XGetWindowAttributes (gdk_display, gdk_root_window, &xattributes);
  
  gdk_root_parent.drawable.xwindow = gdk_root_window;
  gdk_root_parent.drawable.xdisplay = gdk_display;
  gdk_root_parent.drawable.window_type = GDK_WINDOW_ROOT;
  gdk_root_parent.drawable.drawable.user_data = NULL;
  gdk_root_parent.drawable.width = width;
  gdk_root_parent.drawable.height = height;
  gdk_root_parent.children = NULL;
  gdk_root_parent.drawable.colormap = NULL;
  gdk_root_parent.drawable.ref_count = 1;
  
  gdk_xid_table_insert (&gdk_root_window, &gdk_root_parent);
}

static GdkAtom wm_client_leader_atom = GDK_NONE;

GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowPrivate *private;
  GdkWindowPrivate *parent_private;
  GdkVisual *visual;
  Display *parent_display;
  Window xparent;
  Visual *xvisual;
  XSetWindowAttributes xattributes;
  long xattributes_mask;
  XSizeHints size_hints;
  XWMHints wm_hints;
  XClassHint *class_hint;
  int x, y, depth;
  unsigned int class;
  char *title;
  int i;
  
  g_return_val_if_fail (attributes != NULL, NULL);
  
  if (!parent)
    parent = (GdkWindow*) &gdk_root_parent;
  
  parent_private = (GdkWindowPrivate*) parent;
  if (GDK_DRAWABLE_DESTROYED (parent))
    return NULL;
  
  xparent = parent_private->drawable.xwindow;
  parent_display = parent_private->drawable.xdisplay;
  
  private = g_new (GdkWindowPrivate, 1);
  window = (GdkWindow*) private;
  
  private->parent = parent;
  
  private->drawable.xdisplay = parent_display;
  private->drawable.destroyed = FALSE;
  private->mapped = FALSE;
  private->guffaw_gravity = FALSE;
  private->resize_count = 0;
  private->drawable.ref_count = 1;
  xattributes_mask = 0;
  
  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = 0;
  
  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else
    y = 0;
  
  private->x = x;
  private->y = y;
  private->drawable.width = (attributes->width > 1) ? (attributes->width) : (1);
  private->drawable.height = (attributes->height > 1) ? (attributes->height) : (1);
  private->drawable.window_type = attributes->window_type;
  private->extension_events = FALSE;
  
  private->filters = NULL;
  private->children = NULL;
  
  window->user_data = NULL;
  
  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_visual_get_system ();
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;
  
  xattributes.event_mask = StructureNotifyMask;
  for (i = 0; i < gdk_nevent_masks; i++)
    {
      if (attributes->event_mask & (1 << (i + 1)))
	xattributes.event_mask |= gdk_event_mask_table[i];
    }
  
  if (xattributes.event_mask)
    xattributes_mask |= CWEventMask;
  
  if (attributes_mask & GDK_WA_NOREDIR)
    {
      xattributes.override_redirect =
	(attributes->override_redirect == FALSE)?False:True;
      xattributes_mask |= CWOverrideRedirect;
    } 
  else
    xattributes.override_redirect = False;
  
  if (parent_private && parent_private->guffaw_gravity)
    {
      xattributes.win_gravity = StaticGravity;
      xattributes_mask |= CWWinGravity;
    }
  
  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      class = InputOutput;
      depth = visual->depth;
      
      if (attributes_mask & GDK_WA_COLORMAP)
	private->drawable.colormap = attributes->colormap;
      else
	{
	  if ((((GdkVisualPrivate*)gdk_visual_get_system ())->xvisual) == xvisual)
	    private->drawable.colormap = gdk_colormap_get_system ();
	  else
	    private->drawable.colormap = gdk_colormap_new (visual, False);
	}
      
      xattributes.background_pixel = BlackPixel (gdk_display, gdk_screen);
      xattributes.border_pixel = BlackPixel (gdk_display, gdk_screen);
      xattributes_mask |= CWBorderPixel | CWBackPixel;
      
      switch (private->drawable.window_type)
	{
	case GDK_WINDOW_TOPLEVEL:
	  xattributes.colormap = ((GdkColormapPrivate*) private->drawable.colormap)->xcolormap;
	  xattributes_mask |= CWColormap;
	  
	  xparent = gdk_root_window;
	  break;
	  
	case GDK_WINDOW_CHILD:
	  xattributes.colormap = ((GdkColormapPrivate*) private->drawable.colormap)->xcolormap;
	  xattributes_mask |= CWColormap;
	  break;
	  
	case GDK_WINDOW_DIALOG:
	  xattributes.colormap = ((GdkColormapPrivate*) private->drawable.colormap)->xcolormap;
	  xattributes_mask |= CWColormap;
	  
	  xparent = gdk_root_window;
	  break;
	  
	case GDK_WINDOW_TEMP:
	  xattributes.colormap = ((GdkColormapPrivate*) private->drawable.colormap)->xcolormap;
	  xattributes_mask |= CWColormap;
	  
	  xparent = gdk_root_window;
	  
	  xattributes.save_under = True;
	  xattributes.override_redirect = True;
	  xattributes.cursor = None;
	  xattributes_mask |= CWSaveUnder | CWOverrideRedirect;
	  break;
	case GDK_WINDOW_ROOT:
	  g_error ("cannot make windows of type GDK_WINDOW_ROOT");
	  break;
	case GDK_WINDOW_PIXMAP:
	  g_error ("cannot make windows of type GDK_WINDOW_PIXMAP (use gdk_pixmap_new)");
	  break;
	}
    }
  else
    {
      depth = 0;
      class = InputOnly;
      private->drawable.colormap = NULL;
    }
  
  private->drawable.xwindow = XCreateWindow (private->drawable.xdisplay, xparent,
				    x, y, private->drawable.width, private->drawable.height,
				    0, depth, class, xvisual,
				    xattributes_mask, &xattributes);
  gdk_window_ref (window);
  gdk_xid_table_insert (&private->drawable.xwindow, window);
  
  if (private->drawable.colormap)
    gdk_colormap_ref (private->drawable.colormap);
  
  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));
  
  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);
  
  switch (private->drawable.window_type)
    {
    case GDK_WINDOW_DIALOG:
      XSetTransientForHint (private->drawable.xdisplay, private->drawable.xwindow, xparent);
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      XSetWMProtocols (private->drawable.xdisplay, private->drawable.xwindow, gdk_wm_window_protocols, 2);
      break;
    case GDK_WINDOW_CHILD:
      if ((attributes->wclass == GDK_INPUT_OUTPUT) &&
	  (private->drawable.colormap != gdk_colormap_get_system ()) &&
	  (private->drawable.colormap != gdk_window_get_colormap (gdk_window_get_toplevel (window))))
	{
	  GDK_NOTE (MISC, g_message ("adding colormap window\n"));
	  gdk_window_add_colormap_windows (window);
	}
      
      return window;
    default:
      
      return window;
    }
  
  size_hints.flags = PSize;
  size_hints.width = private->drawable.width;
  size_hints.height = private->drawable.height;
  
  wm_hints.flags = InputHint | StateHint | WindowGroupHint;
  wm_hints.window_group = gdk_leader_window;
  wm_hints.input = True;
  wm_hints.initial_state = NormalState;
  
  /* FIXME: Is there any point in doing this? Do any WM's pay
   * attention to PSize, and even if they do, is this the
   * correct value???
   */
  XSetWMNormalHints (private->drawable.xdisplay, private->drawable.xwindow, &size_hints);
  
  XSetWMHints (private->drawable.xdisplay, private->drawable.xwindow, &wm_hints);
  
  if (!wm_client_leader_atom)
    wm_client_leader_atom = gdk_atom_intern ("WM_CLIENT_LEADER", FALSE);
  
  XChangeProperty (private->drawable.xdisplay, private->drawable.xwindow,
	   	   wm_client_leader_atom,
		   XA_WINDOW, 32, PropModeReplace,
		   (guchar*) &gdk_leader_window, 1);
  
  if (attributes_mask & GDK_WA_TITLE)
    title = attributes->title;
  else
    title = g_get_prgname ();
  
  XmbSetWMProperties (private->drawable.xdisplay, private->drawable.xwindow,
                      title, title,
                      NULL, 0,
                      NULL, NULL, NULL);
  
  if (attributes_mask & GDK_WA_WMCLASS)
    {
      class_hint = XAllocClassHint ();
      class_hint->res_name = attributes->wmclass_name;
      class_hint->res_class = attributes->wmclass_class;
      XSetClassHint (private->drawable.xdisplay, private->drawable.xwindow, class_hint);
      XFree (class_hint);
    }
  
  
  return window;
}

GdkWindow *
gdk_window_foreign_new (guint32 anid)
{
  GdkWindow *window;
  GdkWindowPrivate *private;
  GdkWindowPrivate *parent_private;
  XWindowAttributes attrs;
  Window root, parent;
  Window *children = NULL;
  guint nchildren;
  gboolean result;

  gdk_error_trap_push ();
  result = XGetWindowAttributes (gdk_display, anid, &attrs);
  if (gdk_error_trap_pop () || !result)
    return NULL;

  /* FIXME: This is pretty expensive. Maybe the caller should supply
   *        the parent */
  gdk_error_trap_push ();
  result = XQueryTree (gdk_display, anid, &root, &parent, &children, &nchildren);
  if (gdk_error_trap_pop () || !result)
    return NULL;
  
  private = g_new (GdkWindowPrivate, 1);
  window = (GdkWindow*) private;
  
  if (children)
    XFree (children);
  private->parent = gdk_xid_table_lookup (parent);
  
  parent_private = (GdkWindowPrivate *)private->parent;
  
  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);
  
  private->drawable.xwindow = anid;
  private->drawable.xdisplay = gdk_display;
  private->x = attrs.x;
  private->y = attrs.y;
  private->drawable.width = attrs.width;
  private->drawable.height = attrs.height;
  private->resize_count = 0;
  private->drawable.ref_count = 1;
  private->drawable.window_type = GDK_WINDOW_FOREIGN;
  private->drawable.destroyed = FALSE;
  private->mapped = (attrs.map_state != IsUnmapped);
  private->guffaw_gravity = FALSE;
  private->extension_events = 0;
  
  private->drawable.colormap = NULL;
  
  private->filters = NULL;
  private->children = NULL;
  
  window->user_data = NULL;
  
  gdk_window_ref (window);
  gdk_xid_table_insert (&private->drawable.xwindow, window);
  
  return window;
}

/* Call this function when you want a window and all its children to
 * disappear.  When xdestroy is true, a request to destroy the XWindow
 * is sent out.  When it is false, it is assumed that the XWindow has
 * been or will be destroyed by destroying some ancestor of this
 * window.
 */
static void
gdk_window_internal_destroy (GdkWindow *window,
			     gboolean   xdestroy,
			     gboolean   our_destroy)
{
  GdkWindowPrivate *private;
  GdkWindowPrivate *temp_private;
  GdkWindow *temp_window;
  GList *children;
  GList *tmp;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  
  switch (private->drawable.window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
    case GDK_WINDOW_FOREIGN:
      if (!private->drawable.destroyed)
	{
	  if (private->parent)
	    {
	      GdkWindowPrivate *parent_private = (GdkWindowPrivate *)private->parent;
	      if (parent_private->children)
		parent_private->children = g_list_remove (parent_private->children, window);
	    }
	  
	  if (GDK_DRAWABLE_TYPE (window) != GDK_WINDOW_FOREIGN)
	    {
	      children = tmp = private->children;
	      private->children = NULL;
	      
	      while (tmp)
		{
		  temp_window = tmp->data;
		  tmp = tmp->next;
		  
		  temp_private = (GdkWindowPrivate*) temp_window;
		  if (temp_private)
		    gdk_window_internal_destroy (temp_window, FALSE,
						 our_destroy);
		}
	      
	      g_list_free (children);
	    }
	  
	  if (private->extension_events != 0)
	    gdk_input_window_destroy (window);
	  
	  if (private->filters)
	    {
	      tmp = private->filters;
	      
	      while (tmp)
		{
		  g_free (tmp->data);
		  tmp = tmp->next;
		}
	      
	      g_list_free (private->filters);
	      private->filters = NULL;
	    }
	  
	  if (private->drawable.window_type == GDK_WINDOW_FOREIGN)
	    {
	      if (our_destroy && (private->parent != NULL))
		{
		  /* It's somebody elses window, but in our heirarchy,
		   * so reparent it to the root window, and then send
		   * it a delete event, as if we were a WM
		   */
		  XClientMessageEvent xevent;

		  gdk_error_trap_push ();
		  gdk_window_hide (window);
		  gdk_window_reparent (window, NULL, 0, 0);
		  
		  xevent.type = ClientMessage;
		  xevent.window = private->drawable.xwindow;
		  xevent.message_type = gdk_wm_protocols;
		  xevent.format = 32;
		  xevent.data.l[0] = gdk_wm_delete_window;
		  xevent.data.l[1] = CurrentTime;

		  XSendEvent (private->drawable.xdisplay, private->drawable.xwindow,
			      False, 0, (XEvent *)&xevent);
		  gdk_flush ();
		  gdk_error_trap_pop ();
		}
	    }
	  else if (xdestroy)
	    XDestroyWindow (private->drawable.xdisplay, private->drawable.xwindow);
	  
	  if (private->drawable.colormap)
	    gdk_colormap_unref (private->drawable.colormap);
	  
	  private->mapped = FALSE;
	  private->drawable.destroyed = TRUE;
	}
      break;
      
    case GDK_WINDOW_ROOT:
      g_error ("attempted to destroy root window");
      break;
      
    case GDK_WINDOW_PIXMAP:
      g_error ("called gdk_window_destroy on a pixmap (use gdk_pixmap_unref)");
      break;
    }
}

/* Like internal_destroy, but also destroys the reference created by
   gdk_window_new. */

void
gdk_window_destroy (GdkWindow *window)
{
  gdk_window_internal_destroy (window, TRUE, TRUE);
  gdk_window_unref (window);
}

/* This function is called when the XWindow is really gone.  */

void
gdk_window_destroy_notify (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      if (GDK_DRAWABLE_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("GdkWindow %#lx unexpectedly destroyed", GDK_DRAWABLE_XID (window));

      gdk_window_internal_destroy (window, FALSE, FALSE);
    }
  
  gdk_xid_table_remove (GDK_DRAWABLE_XID (window));
  gdk_window_unref (window);
}

GdkWindow*
gdk_window_ref (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  g_return_val_if_fail (window != NULL, NULL);
  
  private->drawable.ref_count += 1;
  return window;
}

void
gdk_window_unref (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  g_return_if_fail (window != NULL);
  
  private->drawable.ref_count -= 1;
  if (private->drawable.ref_count == 0)
    {
      if (!private->drawable.destroyed)
	{
	  if (private->drawable.window_type == GDK_WINDOW_FOREIGN)
	    gdk_xid_table_remove (private->drawable.xwindow);
	  else
	    g_warning ("losing last reference to undestroyed window\n");
	}
      g_dataset_destroy (window);
      g_free (window);
    }
}

void
gdk_window_show (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (!private->drawable.destroyed)
    {
      private->mapped = TRUE;
      XRaiseWindow (private->drawable.xdisplay, private->drawable.xwindow);
      XMapWindow (private->drawable.xdisplay, private->drawable.xwindow);
    }
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (!private->drawable.destroyed)
    {
      private->mapped = FALSE;
      XUnmapWindow (private->drawable.xdisplay, private->drawable.xwindow);
    }
}

void
gdk_window_withdraw (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (!private->drawable.destroyed)
    XWithdrawWindow (private->drawable.xdisplay, private->drawable.xwindow, 0);
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (!private->drawable.destroyed)
    {
      XMoveWindow (private->drawable.xdisplay, private->drawable.xwindow, x, y);
      
      if (private->drawable.window_type == GDK_WINDOW_CHILD)
	{
	  private->x = x;
	  private->y = y;
	}
    }
}

void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  private = (GdkWindowPrivate*) window;
  
  if (!private->drawable.destroyed &&
      ((private->resize_count > 0) ||
       (private->drawable.width != (guint16) width) ||
       (private->drawable.height != (guint16) height)))
    {
      XResizeWindow (GDK_DRAWABLE_XDISPLAY (private),
		     GDK_DRAWABLE_XID (private),
		     width, height);
      private->resize_count += 1;
      
      if (GDK_DRAWABLE_TYPE (private) == GDK_WINDOW_CHILD)
	{
	  private->drawable.width = width;
	  private->drawable.height = height;
	}
    }
}

void
gdk_window_move_resize (GdkWindow *window,
			gint       x,
			gint       y,
			gint       width,
			gint       height)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;
  
  private = (GdkWindowPrivate*) window;

  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      XMoveResizeWindow (GDK_DRAWABLE_XDISPLAY (window),
			 GDK_DRAWABLE_XID (window),
			 x, y, width, height);
      
      if (private->guffaw_gravity)
	{
	  GList *tmp_list = private->children;
	  while (tmp_list)
	    {
	      GdkWindowPrivate *child_private = tmp_list->data;
	      
	      child_private->x -= x - private->x;
	      child_private->y -= y - private->y;
	      
	      tmp_list = tmp_list->next;
	    }
	}
      
      if (GDK_DRAWABLE_TYPE (private) == GDK_WINDOW_CHILD)
	{
	  private->x = x;
	  private->y = y;
	  private->drawable.width = width;
	  private->drawable.height = height;
	}
    }
}

void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  GdkWindowPrivate *window_private;
  GdkWindowPrivate *parent_private;
  GdkWindowPrivate *old_parent_private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (new_parent != NULL);
  g_return_if_fail (GDK_IS_WINDOW (new_parent));
  
  if (!new_parent)
    new_parent = (GdkWindow*) &gdk_root_parent;
  
  window_private = (GdkWindowPrivate*) window;
  old_parent_private = (GdkWindowPrivate*)window_private->parent;
  parent_private = (GdkWindowPrivate*) new_parent;
  
  if (!GDK_DRAWABLE_DESTROYED (window) && !GDK_DRAWABLE_DESTROYED (new_parent))
    XReparentWindow (GDK_DRAWABLE_XDISPLAY (window),
		     GDK_DRAWABLE_XID (window),
		     GDK_DRAWABLE_XID (new_parent),
		     x, y);
  
  window_private->parent = new_parent;
  
  if (old_parent_private)
    old_parent_private->children = g_list_remove (old_parent_private->children, window);
  
  if ((old_parent_private &&
       (!old_parent_private->guffaw_gravity != !parent_private->guffaw_gravity)) ||
      (!old_parent_private && parent_private->guffaw_gravity))
    gdk_window_set_static_win_gravity (window, parent_private->guffaw_gravity);
  
  parent_private->children = g_list_prepend (parent_private->children, window);
}

void
gdk_window_clear (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    XClearWindow (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window));
}

void
gdk_window_clear_area (GdkWindow *window,
		       gint       x,
		       gint       y,
		       gint       width,
		       gint       height)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    XClearArea (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window),
		x, y, width, height, False);
}

void
gdk_window_clear_area_e (GdkWindow *window,
		         gint       x,
		         gint       y,
		         gint       width,
		         gint       height)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    XClearArea (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window),
		x, y, width, height, True);
}

void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    XRaiseWindow (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window));
}

void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    XLowerWindow (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window));
}

void
gdk_window_set_user_data (GdkWindow *window,
			  gpointer   user_data)
{
  g_return_if_fail (window != NULL);
  
  window->user_data = user_data;
}

void
gdk_window_set_hints (GdkWindow *window,
		      gint       x,
		      gint       y,
		      gint       min_width,
		      gint       min_height,
		      gint       max_width,
		      gint       max_height,
		      gint       flags)
{
  XSizeHints size_hints;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_DRAWABLE_DESTROYED (window))
    return;
  
  size_hints.flags = 0;
  
  if (flags & GDK_HINT_POS)
    {
      size_hints.flags |= PPosition;
      size_hints.x = x;
      size_hints.y = y;
    }
  
  if (flags & GDK_HINT_MIN_SIZE)
    {
      size_hints.flags |= PMinSize;
      size_hints.min_width = min_width;
      size_hints.min_height = min_height;
    }
  
  if (flags & GDK_HINT_MAX_SIZE)
    {
      size_hints.flags |= PMaxSize;
      size_hints.max_width = max_width;
      size_hints.max_height = max_height;
    }
  
  /* FIXME: Would it be better to delete this property of
   *        flags == 0? It would save space on the server
   */
  XSetWMNormalHints (GDK_DRAWABLE_XDISPLAY (window),
		     GDK_DRAWABLE_XID (window),
		     &size_hints);
}

void 
gdk_window_set_geometry_hints (GdkWindow      *window,
			       GdkGeometry    *geometry,
			       GdkWindowHints  geom_mask)
{
  XSizeHints size_hints;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (GDK_DRAWABLE_DESTROYED (window))
    return;
  
  size_hints.flags = 0;
  
  if (geom_mask & GDK_HINT_POS)
    {
      size_hints.flags |= PPosition;
      /* We need to initialize the following obsolete fields because KWM 
       * apparently uses these fields if they are non-zero.
       * #@#!#!$!.
       */
      size_hints.x = 0;
      size_hints.y = 0;
    }
  
  if (geom_mask & GDK_HINT_MIN_SIZE)
    {
      size_hints.flags |= PMinSize;
      size_hints.min_width = geometry->min_width;
      size_hints.min_height = geometry->min_height;
    }
  
  if (geom_mask & GDK_HINT_MAX_SIZE)
    {
      size_hints.flags |= PMaxSize;
      size_hints.max_width = MAX (geometry->max_width, 1);
      size_hints.max_height = MAX (geometry->max_height, 1);
    }
  
  if (geom_mask & GDK_HINT_BASE_SIZE)
    {
      size_hints.flags |= PBaseSize;
      size_hints.base_width = geometry->base_width;
      size_hints.base_height = geometry->base_height;
    }
  
  if (geom_mask & GDK_HINT_RESIZE_INC)
    {
      size_hints.flags |= PResizeInc;
      size_hints.width_inc = geometry->width_inc;
      size_hints.height_inc = geometry->height_inc;
    }
  
  if (geom_mask & GDK_HINT_ASPECT)
    {
      size_hints.flags |= PAspect;
      if (geometry->min_aspect <= 1)
	{
	  size_hints.min_aspect.x = 65536 * geometry->min_aspect;
	  size_hints.min_aspect.y = 65536;
	}
      else
	{
	  size_hints.min_aspect.x = 65536;
	  size_hints.min_aspect.y = 65536 / geometry->min_aspect;;
	}
      if (geometry->max_aspect <= 1)
	{
	  size_hints.max_aspect.x = 65536 * geometry->max_aspect;
	  size_hints.max_aspect.y = 65536;
	}
      else
	{
	  size_hints.max_aspect.x = 65536;
	  size_hints.max_aspect.y = 65536 / geometry->max_aspect;;
	}
    }

  /* FIXME: Would it be better to delete this property of
   *        geom_mask == 0? It would save space on the server
   */
  XSetWMNormalHints (GDK_DRAWABLE_XDISPLAY (window),
		     GDK_DRAWABLE_XID (window),
		     &size_hints);
}

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    XmbSetWMProperties (GDK_DRAWABLE_XDISPLAY (window),
			GDK_DRAWABLE_XID (window),
			title, title, NULL, 0, NULL, NULL, NULL);
}

void          
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      if (role)
	XChangeProperty (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window),
			 gdk_atom_intern ("WM_WINDOW_ROLE", FALSE), XA_STRING,
			 8, PropModeReplace, role, strlen (role));
      else
	XDeleteProperty (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window),
			 gdk_atom_intern ("WM_WINDOW_ROLE", FALSE));
    }
}

void          
gdk_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
  GdkWindowPrivate *private;
  GdkWindowPrivate *parent_private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowPrivate*) window;
  parent_private = (GdkWindowPrivate*) parent;
  
  if (!GDK_DRAWABLE_DESTROYED (window) && !GDK_DRAWABLE_DESTROYED (parent))
    XSetTransientForHint (GDK_DRAWABLE_XDISPLAY (window), 
			  GDK_DRAWABLE_XID (window),
			  GDK_DRAWABLE_XID (parent));
}

void
gdk_window_set_background (GdkWindow *window,
			   GdkColor  *color)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    XSetWindowBackground (GDK_DRAWABLE_XDISPLAY (window),
			  GDK_DRAWABLE_XID (window), color->pixel);
}

void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gint       parent_relative)
{
  Pixmap xpixmap;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (pixmap)
    xpixmap = GDK_DRAWABLE_XID (pixmap);
  else
    xpixmap = None;
  
  if (parent_relative)
    xpixmap = ParentRelative;
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    XSetWindowBackgroundPixmap (GDK_DRAWABLE_XDISPLAY (window),
				GDK_DRAWABLE_XID (window), xpixmap);
}

void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkCursorPrivate *cursor_private;
  Cursor xcursor;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  cursor_private = (GdkCursorPrivate*) cursor;
  
  if (!cursor)
    xcursor = None;
  else
    xcursor = cursor_private->xcursor;
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    XDefineCursor (GDK_DRAWABLE_XDISPLAY (window),
		   GDK_DRAWABLE_XID (window),
		   xcursor);
}

void
gdk_window_get_user_data (GdkWindow *window,
			  gpointer  *data)
{
  g_return_if_fail (window != NULL);
  
  *data = window->user_data;
}

void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  Window root;
  gint tx;
  gint ty;
  guint twidth;
  guint theight;
  guint tborder_width;
  guint tdepth;
  
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));
  
  if (!window)
    window = (GdkWindow *) &gdk_root_parent;
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      XGetGeometry (GDK_DRAWABLE_XDISPLAY (window),
		    GDK_DRAWABLE_XID (window),
		    &root, &tx, &ty, &twidth, &theight, &tborder_width, &tdepth);
      
      if (x)
	*x = tx;
      if (y)
	*y = ty;
      if (width)
	*width = twidth;
      if (height)
	*height = theight;
      if (depth)
	*depth = tdepth;
    }
}

void
gdk_window_get_position (GdkWindow *window,
			 gint      *x,
			 gint      *y)
{
  GdkWindowPrivate *window_private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  window_private = (GdkWindowPrivate*) window;
  
  if (x)
    *x = window_private->x;
  if (y)
    *y = window_private->y;
}

gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  gint return_val;
  Window child;
  gint tx = 0;
  gint ty = 0;
  
  g_return_val_if_fail (window != NULL, 0);
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      return_val = XTranslateCoordinates (GDK_DRAWABLE_XDISPLAY (window),
					  GDK_DRAWABLE_XID (window),
					  gdk_root_window,
					  0, 0, &tx, &ty,
					  &child);
      
    }
  else
    return_val = 0;
  
  if (x)
    *x = tx;
  if (y)
    *y = ty;
  
  return return_val;
}

gboolean
gdk_window_get_deskrelative_origin (GdkWindow *window,
				    gint      *x,
				    gint      *y)
{
  gboolean return_val = FALSE;
  gint num_children, format_return;
  Window win, *child, parent, root;
  gint tx = 0;
  gint ty = 0;
  Atom type_return;
  static Atom atom = 0;
  gulong number_return, bytes_after_return;
  guchar *data_return;
  
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      if (!atom)
	atom = gdk_atom_intern ("ENLIGHTENMENT_DESKTOP", FALSE);
      win = GDK_DRAWABLE_XID (window);
      
      while (XQueryTree (GDK_DRAWABLE_XDISPLAY (window), win, &root, &parent,
			 &child, (unsigned int *)&num_children))
	{
	  if ((child) && (num_children > 0))
	    XFree (child);
	  
	  if (!parent)
	    break;
	  else
	    win = parent;
	  
	  if (win == root)
	    break;
	  
	  data_return = NULL;
	  XGetWindowProperty (GDK_DRAWABLE_XDISPLAY (window), win, atom, 0, 0,
			      False, XA_CARDINAL, &type_return, &format_return,
			      &number_return, &bytes_after_return, &data_return);
	  if (type_return == XA_CARDINAL)
	    {
	      XFree (data_return);
              break;
	    }
	}
      
      return_val = XTranslateCoordinates (GDK_DRAWABLE_XDISPLAY (window),
					  GDK_DRAWABLE_XID (window),
					  win,
					  0, 0, &tx, &ty,
					  &root);
      if (x)
	*x = tx;
      if (y)
	*y = ty;
    }
  
  
  return return_val;
}

void
gdk_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
  GdkWindowPrivate *private;
  Window xwindow;
  Window xparent;
  Window root;
  Window *children;
  unsigned int nchildren;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowPrivate*) window;
  if (x)
    *x = 0;
  if (y)
    *y = 0;

  if (GDK_DRAWABLE_DESTROYED (window))
    return;
  
  while (private->parent && ((GdkWindowPrivate*) private->parent)->parent)
    private = (GdkWindowPrivate*) private->parent;
  if (GDK_DRAWABLE_DESTROYED (window))
    return;
  
  xparent = GDK_DRAWABLE_XID (window);
  do
    {
      xwindow = xparent;
      if (!XQueryTree (GDK_DRAWABLE_XDISPLAY (window), xwindow,
		       &root, &xparent,
		       &children, &nchildren))
	return;
      
      if (children)
	XFree (children);
    }
  while (xparent != root);
  
  if (xparent == root)
    {
      unsigned int ww, wh, wb, wd;
      int wx, wy;
      
      if (XGetGeometry (GDK_DRAWABLE_XDISPLAY (window), xwindow, &root, &wx, &wy, &ww, &wh, &wb, &wd))
	{
	  if (x)
	    *x = wx;
	  if (y)
	    *y = wy;
	}
    }
}

GdkWindow*
gdk_window_get_pointer (GdkWindow       *window,
			gint            *x,
			gint            *y,
			GdkModifierType *mask)
{
  GdkWindow *return_val;
  Window root;
  Window child;
  int rootx, rooty;
  int winx = 0;
  int winy = 0;
  unsigned int xmask = 0;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);
  
  if (!window)
    window = (GdkWindow*) &gdk_root_parent;
  
  return_val = NULL;
  if (!GDK_DRAWABLE_DESTROYED (window) &&
      XQueryPointer (GDK_DRAWABLE_XDISPLAY (window),
		     GDK_DRAWABLE_XID (window),
		     &root, &child, &rootx, &rooty, &winx, &winy, &xmask))
    {
      if (child)
	return_val = gdk_window_lookup (child);
    }
  
  if (x)
    *x = winx;
  if (y)
    *y = winy;
  if (mask)
    *mask = xmask;
  
  return return_val;
}

GdkWindow*
gdk_window_at_pointer (gint *win_x,
		       gint *win_y)
{
  GdkWindowPrivate *private;
  GdkWindow *window;
  Window root;
  Window xwindow;
  Window xwindow_last = 0;
  int rootx = -1, rooty = -1;
  int winx, winy;
  unsigned int xmask;
  
  private = &gdk_root_parent;
  
  xwindow = private->drawable.xwindow;
  
  XGrabServer (private->drawable.xdisplay);
  while (xwindow)
    {
      xwindow_last = xwindow;
      XQueryPointer (private->drawable.xdisplay,
		     xwindow,
		     &root, &xwindow,
		     &rootx, &rooty,
		     &winx, &winy,
		     &xmask);
    }
  XUngrabServer (private->drawable.xdisplay);
  
  window = gdk_window_lookup (xwindow_last);
  
  if (win_x)
    *win_x = window ? winx : -1;
  if (win_y)
    *win_y = window ? winy : -1;
  
  return window;
}

GdkWindow*
gdk_window_get_parent (GdkWindow *window)
{
  g_return_val_if_fail (window != NULL, NULL);
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  
  return ((GdkWindowPrivate*) window)->parent;
}

GdkWindow*
gdk_window_get_toplevel (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_val_if_fail (window != NULL, NULL);
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  private = (GdkWindowPrivate *)window;
  while (GDK_DRAWABLE_TYPE (private) == GDK_WINDOW_CHILD)
    private = (GdkWindowPrivate *)private->parent;
  
  return (GdkWindow *)window;
}

GList*
gdk_window_get_children (GdkWindow *window)
{
  GdkWindow *child;
  GList *children;
  Window root;
  Window parent;
  Window *xchildren;
  unsigned int nchildren;
  unsigned int i;
  
  g_return_val_if_fail (window != NULL, NULL);
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_DRAWABLE_DESTROYED (window))
    return NULL;
  
  XQueryTree (GDK_DRAWABLE_XDISPLAY (window),
	      GDK_DRAWABLE_XID (window),
	      &root, &parent, &xchildren, &nchildren);
  
  children = NULL;
  
  if (nchildren > 0)
    {
      for (i = 0; i < nchildren; i++)
	{
	  child = gdk_window_lookup (xchildren[i]);
          if (child)
            children = g_list_prepend (children, child);
	}
      
      if (xchildren)
	XFree (xchildren);
    }
  
  return children;
}

GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
  XWindowAttributes attrs;
  GdkEventMask event_mask;
  int i;
  
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_DRAWABLE_DESTROYED (window))
    return 0;
  else
    {
      XGetWindowAttributes (GDK_DRAWABLE_XDISPLAY (window),
			    GDK_DRAWABLE_XID (window), 
			    &attrs);
      
      event_mask = 0;
      for (i = 0; i < gdk_nevent_masks; i++)
	{
	  if (attrs.your_event_mask & gdk_event_mask_table[i])
	    event_mask |= 1 << (i + 1);
	}
  
      return event_mask;
    }
}

void          
gdk_window_set_events (GdkWindow       *window,
		       GdkEventMask     event_mask)
{
  long xevent_mask;
  int i;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      xevent_mask = StructureNotifyMask;
      for (i = 0; i < gdk_nevent_masks; i++)
	{
	  if (event_mask & (1 << (i + 1)))
	    xevent_mask |= gdk_event_mask_table[i];
	}
      
      XSelectInput (GDK_DRAWABLE_XDISPLAY (window),
		    GDK_DRAWABLE_XID (window),
		    xevent_mask);
    }
}

void
gdk_window_add_colormap_windows (GdkWindow *window)
{
  GdkWindow *toplevel;
  Window *old_windows;
  Window *new_windows;
  int i, count;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  toplevel = gdk_window_get_toplevel (window);
  if (GDK_DRAWABLE_DESTROYED (toplevel))
    return;
  
  old_windows = NULL;
  if (!XGetWMColormapWindows (GDK_DRAWABLE_XDISPLAY (toplevel),
			      GDK_DRAWABLE_XID (toplevel),
			      &old_windows, &count))
    {
      count = 0;
    }
  
  for (i = 0; i < count; i++)
    if (old_windows[i] == GDK_DRAWABLE_XID (window))
      {
	XFree (old_windows);
	return;
      }
  
  new_windows = g_new (Window, count + 1);
  
  for (i = 0; i < count; i++)
    new_windows[i] = old_windows[i];
  new_windows[count] = GDK_DRAWABLE_XID (window);
  
  XSetWMColormapWindows (GDK_DRAWABLE_XDISPLAY (toplevel),
			 GDK_DRAWABLE_XID (toplevel),
			 new_windows, count + 1);
  
  g_free (new_windows);
  if (old_windows)
    XFree (old_windows);
}

static gboolean
gdk_window_have_shape_ext (void)
{
  enum { UNKNOWN, NO, YES };
  static gint have_shape = UNKNOWN;
  
  if (have_shape == UNKNOWN)
    {
      int ignore;
      if (XQueryExtension (gdk_display, "SHAPE", &ignore, &ignore, &ignore))
	have_shape = YES;
      else
	have_shape = NO;
    }
  
  return (have_shape == YES);
}

/*
 * This needs the X11 shape extension.
 * If not available, shaped windows will look
 * ugly, but programs still work.    Stefan Wille
 */
void
gdk_window_shape_combine_mask (GdkWindow *window,
			       GdkBitmap *mask,
			       gint x, gint y)
{
  Pixmap pixmap;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
#ifdef HAVE_SHAPE_EXT
  if (GDK_DRAWABLE_DESTROYED (window))
    return;
  
  if (gdk_window_have_shape_ext ())
    {
      if (mask)
	{
	  pixmap = GDK_DRAWABLE_XID (mask);
	}
      else
	{
	  x = 0;
	  y = 0;
	  pixmap = None;
	}
      
      XShapeCombineMask (GDK_DRAWABLE_XDISPLAY (window),
			 GDK_DRAWABLE_XID (window),
			 ShapeBounding,
			 x, y,
			 pixmap,
			 ShapeSet);
    }
#endif /* HAVE_SHAPE_EXT */
}

void          
gdk_window_add_filter (GdkWindow     *window,
		       GdkFilterFunc  function,
		       gpointer       data)
{
  GdkWindowPrivate *private;
  GList *tmp_list;
  GdkEventFilter *filter;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowPrivate*) window;
  if (private && GDK_DRAWABLE_DESTROYED (window))
    return;
  
  if (private)
    tmp_list = private->filters;
  else
    tmp_list = gdk_default_filters;
  
  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      if ((filter->function == function) && (filter->data == data))
	return;
      tmp_list = tmp_list->next;
    }
  
  filter = g_new (GdkEventFilter, 1);
  filter->function = function;
  filter->data = data;
  
  if (private)
    private->filters = g_list_append (private->filters, filter);
  else
    gdk_default_filters = g_list_append (gdk_default_filters, filter);
}

void
gdk_window_remove_filter (GdkWindow     *window,
			  GdkFilterFunc  function,
			  gpointer       data)
{
  GdkWindowPrivate *private;
  GList *tmp_list, *node;
  GdkEventFilter *filter;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowPrivate*) window;
  
  if (private)
    tmp_list = private->filters;
  else
    tmp_list = gdk_default_filters;
  
  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      node = tmp_list;
      tmp_list = tmp_list->next;
      
      if ((filter->function == function) && (filter->data == data))
	{
	  if (private)
	    private->filters = g_list_remove_link (private->filters, node);
	  else
	    gdk_default_filters = g_list_remove_link (gdk_default_filters, node);
	  g_list_free_1 (node);
	  g_free (filter);
	  
	  return;
	}
    }
}

void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean override_redirect)
{
  XSetWindowAttributes attr;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_DRAWABLE_DESTROYED (window))
    {
      attr.override_redirect = (override_redirect == FALSE)?False:True;
      XChangeWindowAttributes (GDK_DRAWABLE_XDISPLAY (window),
			       GDK_DRAWABLE_XID (window),
			       CWOverrideRedirect,
			       &attr);
    }
}

void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
  XWMHints *wm_hints;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_DRAWABLE_DESTROYED (window))
    return;

  wm_hints = XGetWMHints (GDK_DRAWABLE_XDISPLAY (window),
			  GDK_DRAWABLE_XID (window));
  if (!wm_hints)
    wm_hints = XAllocWMHints ();

  if (icon_window != NULL)
    {
      wm_hints->flags |= IconWindowHint;
      wm_hints->icon_window = GDK_DRAWABLE_XID (icon_window);
    }
  
  if (pixmap != NULL)
    {
      wm_hints->flags |= IconPixmapHint;
      wm_hints->icon_pixmap = GDK_DRAWABLE_XID (pixmap);
    }
  
  if (mask != NULL)
    {
      wm_hints->flags |= IconMaskHint;
      wm_hints->icon_mask = GDK_DRAWABLE_XID (mask);
    }

  XSetWMHints (GDK_DRAWABLE_XDISPLAY (window),
	       GDK_DRAWABLE_XID (window), wm_hints);
  XFree (wm_hints);
}

void          
gdk_window_set_icon_name (GdkWindow *window, 
			  gchar *    name)
{
  XTextProperty property;
  gint res;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_DRAWABLE_DESTROYED (window))
    return;
  
  res = XmbTextListToTextProperty (GDK_DRAWABLE_XDISPLAY (window),
				   &name, 1, XStdICCTextStyle,
                               	   &property);
  if (res < 0)
    {
      g_warning ("Error converting icon name to text property: %d\n", res);
      return;
    }
  
  XSetWMIconName (GDK_DRAWABLE_XDISPLAY (window),
		  GDK_DRAWABLE_XID (window),
		  &property);
  
  if (property.value)
    XFree (property.value);
}

void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  XWMHints *wm_hints;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (leader != NULL);
  g_return_if_fail (GDK_IS_WINDOW (leader));

  if (GDK_DRAWABLE_DESTROYED (window) || GDK_DRAWABLE_DESTROYED (leader))
    return;
  
  wm_hints = XGetWMHints (GDK_DRAWABLE_XDISPLAY (window),
			  GDK_DRAWABLE_XID (window));
  if (!wm_hints)
    wm_hints = XAllocWMHints ();

  wm_hints->flags |= WindowGroupHint;
  wm_hints->window_group = GDK_DRAWABLE_XID (leader);

  XSetWMHints (GDK_DRAWABLE_XDISPLAY (window),
	       GDK_DRAWABLE_XID (window), wm_hints);
  XFree (wm_hints);
}

static void
gdk_window_set_mwm_hints (GdkWindow *window,
			  MotifWmHints *new_hints)
{
  static Atom hints_atom = None;
  MotifWmHints *hints;
  Atom type;
  gint format;
  gulong nitems;
  gulong bytes_after;
  
  if (GDK_DRAWABLE_DESTROYED (window))
    return;
  
  if (!hints_atom)
    hints_atom = XInternAtom (GDK_DRAWABLE_XDISPLAY (window), 
			      _XA_MOTIF_WM_HINTS, FALSE);
  
  XGetWindowProperty (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window),
		      hints_atom, 0, sizeof (MotifWmHints)/sizeof (long),
		      False, AnyPropertyType, &type, &format, &nitems,
		      &bytes_after, (guchar **)&hints);
  
  if (type == None)
    hints = new_hints;
  else
    {
      if (new_hints->flags & MWM_HINTS_FUNCTIONS)
	{
	  hints->flags |= MWM_HINTS_FUNCTIONS;
	  hints->functions = new_hints->functions;
	}
      if (new_hints->flags & MWM_HINTS_DECORATIONS)
	{
	  hints->flags |= MWM_HINTS_DECORATIONS;
	  hints->decorations = new_hints->decorations;
	}
    }
  
  XChangeProperty (GDK_DRAWABLE_XDISPLAY (window), GDK_DRAWABLE_XID (window),
		   hints_atom, hints_atom, 32, PropModeReplace,
		   (guchar *)hints, sizeof (MotifWmHints)/sizeof (long));
  
  if (hints != new_hints)
    XFree (hints);
}

void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  MotifWmHints hints;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  hints.flags = MWM_HINTS_DECORATIONS;
  hints.decorations = decorations;
  
  gdk_window_set_mwm_hints (window, &hints);
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  MotifWmHints hints;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  hints.flags = MWM_HINTS_FUNCTIONS;
  hints.functions = functions;
  
  gdk_window_set_mwm_hints (window, &hints);
}

GList *
gdk_window_get_toplevels (void)
{
  GList *new_list = NULL;
  GList *tmp_list;
  
  tmp_list = gdk_root_parent.children;
  while (tmp_list)
    {
      new_list = g_list_prepend (new_list, tmp_list->data);
      tmp_list = tmp_list->next;
    }
  
  return new_list;
}

/* 
 * propagate the shapes from all child windows of a GDK window to the parent 
 * window. Shamelessly ripped from Enlightenment's code
 * 
 * - Raster
 */

struct _gdk_span
{
  gint                start;
  gint                end;
  struct _gdk_span    *next;
};

static void
gdk_add_to_span (struct _gdk_span **s,
		 gint               x,
		 gint               xx)
{
  struct _gdk_span *ptr1, *ptr2, *noo, *ss;
  gchar             spanning;
  
  ptr2 = NULL;
  ptr1 = *s;
  spanning = 0;
  ss = NULL;
  /* scan the spans for this line */
  while (ptr1)
    {
      /* -- -> new span */
      /* == -> existing span */
      /* ## -> spans intersect */
      /* if we are in the middle of spanning the span into the line */
      if (spanning)
	{
	  /* case: ---- ==== */
	  if (xx < ptr1->start - 1)
	    {
	      /* ends before next span - extend to here */
	      ss->end = xx;
	      return;
	    }
	  /* case: ----##=== */
	  else if (xx <= ptr1->end)
	    {
	      /* crosses into next span - delete next span and append */
	      ss->end = ptr1->end;
	      ss->next = ptr1->next;
	      g_free (ptr1);
	      return;
	    }
	  /* case: ---###--- */
	  else
	    {
	      /* overlaps next span - delete and keep checking */
	      ss->next = ptr1->next;
	      g_free (ptr1);
	      ptr1 = ss;
	    }
	}
      /* otherwise havent started spanning it in yet */
      else
	{
	  /* case: ---- ==== */
	  if (xx < ptr1->start - 1)
	    {
	      /* insert span here in list */
	      noo = g_malloc (sizeof (struct _gdk_span));
	      
	      if (noo)
		{
		  noo->start = x;
		  noo->end = xx;
		  noo->next = ptr1;
		  if (ptr2)
		    ptr2->next = noo;
		  else
		    *s = noo;
		}
	      return;
	    }
	  /* case: ----##=== */
	  else if ((x < ptr1->start) && (xx <= ptr1->end))
	    {
	      /* expand this span to the left point of the new one */
	      ptr1->start = x;
	      return;
	    }
	  /* case: ===###=== */
	  else if ((x >= ptr1->start) && (xx <= ptr1->end))
	    {
	      /* throw the span away */
	      return;
	    }
	  /* case: ---###--- */
	  else if ((x < ptr1->start) && (xx > ptr1->end))
	    {
	      ss = ptr1;
	      spanning = 1;
	      ptr1->start = x;
	      ptr1->end = xx;
	    }
	  /* case: ===##---- */
	  else if ((x >= ptr1->start) && (x <= ptr1->end + 1) && (xx > ptr1->end))
	    {
	      ss = ptr1;
	      spanning = 1;
	      ptr1->end = xx;
	    }
	  /* case: ==== ---- */
	  /* case handled by next loop iteration - first case */
	}
      ptr2 = ptr1;
      ptr1 = ptr1->next;
    }
  /* it started in the middle but spans beyond your current list */
  if (spanning)
    {
      ptr2->end = xx;
      return;
    }
  /* it does not start inside a span or in the middle, so add it to the end */
  noo = g_malloc (sizeof (struct _gdk_span));
  
  if (noo)
    {
      noo->start = x;
      noo->end = xx;
      if (ptr2)
	{
	  noo->next = ptr2->next;
	  ptr2->next = noo;
	}
      else
	{
	  noo->next = NULL;
	  *s = noo;
	}
    }
  return;
}

static void
gdk_add_rectangles (Display           *disp,
		    Window             win,
		    struct _gdk_span **spans,
		    gint               basew,
		    gint               baseh,
		    gint               x,
		    gint               y)
{
  gint a, k;
  gint x1, y1, x2, y2;
  gint rn, ord;
  XRectangle *rl;
  
  rl = XShapeGetRectangles (disp, win, ShapeBounding, &rn, &ord);
  if (rl)
    {
      /* go through all clip rects in this window's shape */
      for (k = 0; k < rn; k++)
	{
	  /* for each clip rect, add it to each line's spans */
	  x1 = x + rl[k].x;
	  x2 = x + rl[k].x + (rl[k].width - 1);
	  y1 = y + rl[k].y;
	  y2 = y + rl[k].y + (rl[k].height - 1);
	  if (x1 < 0)
	    x1 = 0;
	  if (y1 < 0)
	    y1 = 0;
	  if (x2 >= basew)
	    x2 = basew - 1;
	  if (y2 >= baseh)
	    y2 = baseh - 1;
	  for (a = y1; a <= y2; a++)
	    {
	      if ((x2 - x1) >= 0)
		gdk_add_to_span (&spans[a], x1, x2);
	    }
	}
      XFree (rl);
    }
}

static void
gdk_propagate_shapes (Display *disp,
		      Window   win,
		      gboolean merge)
{
  Window              rt, par, *list = NULL;
  gint                i, j, num = 0, num_rects = 0;
  gint                x, y, contig;
  guint               w, h, d;
  gint                baseh, basew;
  XRectangle         *rects = NULL;
  struct _gdk_span  **spans = NULL, *ptr1, *ptr2, *ptr3;
  XWindowAttributes   xatt;
  
  XGetGeometry (disp, win, &rt, &x, &y, &w, &h, &d, &d);
  if (h <= 0)
    return;
  basew = w;
  baseh = h;
  spans = g_malloc (sizeof (struct _gdk_span *) * h);
  
  for (i = 0; i < h; i++)
    spans[i] = NULL;
  XQueryTree (disp, win, &rt, &par, &list, (unsigned int *)&num);
  if (list)
    {
      /* go through all child windows and create/insert spans */
      for (i = 0; i < num; i++)
	{
	  if (XGetWindowAttributes (disp, list[i], &xatt) && (xatt.map_state != IsUnmapped))
	    if (XGetGeometry (disp, list[i], &rt, &x, &y, &w, &h, &d, &d))
	      gdk_add_rectangles (disp, list[i], spans, basew, baseh, x, y);
	}
      if (merge)
	gdk_add_rectangles (disp, win, spans, basew, baseh, x, y);
      
      /* go through the spans list and build a list of rects */
      rects = g_malloc (sizeof (XRectangle) * 256);
      num_rects = 0;
      for (i = 0; i < baseh; i++)
	{
	  ptr1 = spans[i];
	  /* go through the line for all spans */
	  while (ptr1)
	    {
	      rects[num_rects].x = ptr1->start;
	      rects[num_rects].y = i;
	      rects[num_rects].width = ptr1->end - ptr1->start + 1;
	      rects[num_rects].height = 1;
	      j = i + 1;
	      /* if there are more lines */
	      contig = 1;
	      /* while contigous rects (same start/end coords) exist */
	      while ((contig) && (j < baseh))
		{
		  /* search next line for spans matching this one */
		  contig = 0;
		  ptr2 = spans[j];
		  ptr3 = NULL;
		  while (ptr2)
		    {
		      /* if we have an exact span match set contig */
		      if ((ptr2->start == ptr1->start) &&
			  (ptr2->end == ptr1->end))
			{
			  contig = 1;
			  /* remove the span - not needed */
			  if (ptr3)
			    {
			      ptr3->next = ptr2->next;
			      g_free (ptr2);
			      ptr2 = NULL;
			    }
			  else
			    {
			      spans[j] = ptr2->next;
			      g_free (ptr2);
			      ptr2 = NULL;
			    }
			  break;
			}
		      /* gone past the span point no point looking */
		      else if (ptr2->start < ptr1->start)
			break;
		      if (ptr2)
			{
			  ptr3 = ptr2;
			  ptr2 = ptr2->next;
			}
		    }
		  /* if a contiguous span was found increase the rect h */
		  if (contig)
		    {
		      rects[num_rects].height++;
		      j++;
		    }
		}
	      /* up the rect count */
	      num_rects++;
	      /* every 256 new rects increase the rect array */
	      if ((num_rects % 256) == 0)
		rects = g_realloc (rects, sizeof (XRectangle) * (num_rects + 256));
	      ptr1 = ptr1->next;
	    }
	}
      /* set the rects as the shape mask */
      if (rects)
	{
	  XShapeCombineRectangles (disp, win, ShapeBounding, 0, 0, rects, num_rects,
				   ShapeSet, YXSorted);
	  g_free (rects);
	}
      XFree (list);
    }
  /* free up all the spans we made */
  for (i = 0; i < baseh; i++)
    {
      ptr1 = spans[i];
      while (ptr1)
	{
	  ptr2 = ptr1;
	  ptr1 = ptr1->next;
	  g_free (ptr2);
	}
    }
  g_free (spans);
}

void
gdk_window_set_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
#ifdef HAVE_SHAPE_EXT
  if (!GDK_DRAWABLE_DESTROYED (window) &&
      gdk_window_have_shape_ext ())
    gdk_propagate_shapes (GDK_DRAWABLE_XDISPLAY (window),
			  GDK_DRAWABLE_XID (window), FALSE);
#endif   
}

void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
#ifdef HAVE_SHAPE_EXT
  if (!GDK_DRAWABLE_DESTROYED (window) &&
      gdk_window_have_shape_ext ())
    gdk_propagate_shapes (GDK_DRAWABLE_XDISPLAY (window),
			  GDK_DRAWABLE_XID (window), TRUE);
#endif   
}

/*************************************************************
 * gdk_window_is_visible:
 *     Check if the given window is mapped.
 *   arguments:
 *     window: 
 *   results:
 *     is the window mapped
 *************************************************************/

gboolean 
gdk_window_is_visible (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  
  return private->mapped;
}

/*************************************************************
 * gdk_window_is_viewable:
 *     Check if the window and all ancestors of the window
 *     are mapped. (This is not necessarily "viewable" in
 *     the X sense, since we only check as far as we have
 *     GDK window parents, not to the root window)
 *   arguments:
 *     window:
 *   results:
 *     is the window viewable
 *************************************************************/

gboolean 
gdk_window_is_viewable (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  
  while (private && 
	 (private != &gdk_root_parent) &&
	 (private->drawable.window_type != GDK_WINDOW_FOREIGN))
    {
      if (!private->mapped)
	return FALSE;
      
      private = (GdkWindowPrivate *)private->parent;
    }
  
  return TRUE;
}

/* Support for windows that can be guffaw-scrolled
 * (See http://www.gtk.org/~otaylor/whitepapers/guffaw-scrolling.txt)
 */

static gboolean
gdk_window_gravity_works (void)
{
  enum { UNKNOWN, NO, YES };
  static gint gravity_works = UNKNOWN;
  
  if (gravity_works == UNKNOWN)
    {
      GdkWindowAttr attr;
      GdkWindow *parent;
      GdkWindow *child;
      gint y;
      
      /* This particular server apparently has a bug so that the test
       * works but the actual code crashes it
       */
      if ((!strcmp (XServerVendor (gdk_display), "Sun Microsystems, Inc.")) &&
	  (VendorRelease (gdk_display) == 3400))
	{
	  gravity_works = NO;
	  return FALSE;
	}
      
      attr.window_type = GDK_WINDOW_TEMP;
      attr.wclass = GDK_INPUT_OUTPUT;
      attr.x = 0;
      attr.y = 0;
      attr.width = 100;
      attr.height = 100;
      attr.event_mask = 0;
      
      parent = gdk_window_new (NULL, &attr, GDK_WA_X | GDK_WA_Y);
      
      attr.window_type = GDK_WINDOW_CHILD;
      child = gdk_window_new (parent, &attr, GDK_WA_X | GDK_WA_Y);
      
      gdk_window_set_static_win_gravity (child, TRUE);
      
      gdk_window_resize (parent, 100, 110);
      gdk_window_move (parent, 0, -10);
      gdk_window_move_resize (parent, 0, 0, 100, 100);
      
      gdk_window_resize (parent, 100, 110);
      gdk_window_move (parent, 0, -10);
      gdk_window_move_resize (parent, 0, 0, 100, 100);
      
      gdk_window_get_geometry (child, NULL, &y, NULL, NULL, NULL);
      
      gdk_window_destroy (parent);
      gdk_window_destroy (child);
      
      gravity_works = ((y == -20) ? YES : NO);
    }
  
  return (gravity_works == YES);
}

static void
gdk_window_set_static_bit_gravity (GdkWindow *window, gboolean on)
{
  XSetWindowAttributes xattributes;
  
  g_return_if_fail (window != NULL);
  
  xattributes.bit_gravity = on ? StaticGravity : ForgetGravity;
  XChangeWindowAttributes (GDK_DRAWABLE_XDISPLAY (window),
			   GDK_DRAWABLE_XID (window),
			   CWBitGravity,  &xattributes);
}

static void
gdk_window_set_static_win_gravity (GdkWindow *window, gboolean on)
{
  XSetWindowAttributes xattributes;
  
  g_return_if_fail (window != NULL);
  
  xattributes.win_gravity = on ? StaticGravity : NorthWestGravity;
  
  XChangeWindowAttributes (GDK_DRAWABLE_XDISPLAY (window),
			   GDK_DRAWABLE_XID (window),
			   CWWinGravity,  &xattributes);
}

/*************************************************************
 * gdk_window_set_static_gravities:
 *     Set the bit gravity of the given window to static,
 *     and flag it so all children get static subwindow
 *     gravity.
 *   arguments:
 *     window: window for which to set static gravity
 *     use_static: Whether to turn static gravity on or off.
 *   results:
 *     Does the XServer support static gravity?
 *************************************************************/

gboolean 
gdk_window_set_static_gravities (GdkWindow *window,
				 gboolean   use_static)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  GList *tmp_list;
  
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);

  if (!use_static == !private->guffaw_gravity)
    return TRUE;
  
  if (use_static && !gdk_window_gravity_works ())
    return FALSE;
  
  private->guffaw_gravity = use_static;
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      gdk_window_set_static_bit_gravity (window, use_static);
      
      tmp_list = private->children;
      while (tmp_list)
	{
	  gdk_window_set_static_win_gravity (window, use_static);
	  
	  tmp_list = tmp_list->next;
	}
    }
  
  return TRUE;
}
