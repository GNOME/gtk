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
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <netinet/in.h>
#include "gdk.h"
#include "../config.h"
#include "gdkinput.h"
#include "gdkprivate.h"
#include "MwmUtil.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef HAVE_SHAPE_EXT
#include <X11/extensions/shape.h>
#endif

int nevent_masks = 17;
int event_mask_table[19] =
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
  0				/* PROXIMTY_OUT */
};


/* internal function created for and used by gdk_window_xid_at_coords */
Window
gdk_window_xid_at(Window base, gint bx, gint by, gint x, gint y, 
		  GList *excludes, gboolean excl_child)
{
   GdkWindow *window;
   GdkWindowPrivate *private;
   Display *disp;
   Window *list=NULL;
   Window child=0,parent_win=0,root_win=0;

   int i;
   unsigned int ww, wh, wb, wd, num;
   int wx,wy;
   
   window=(GdkWindow*)&gdk_root_parent;
   private=(GdkWindowPrivate*)window;
   disp=private->xdisplay;
   if (!XGetGeometry(disp,base,&root_win,&wx,&wy,&ww,&wh,&wb,&wd))
     return 0;
   wx+=bx;wy+=by;
   if (!((x>=wx)&&(y>=wy)&&(x<(int)(wx+ww))&&(y<(int)(wy+wh))))
     return 0;
   if (!XQueryTree(disp,base,&root_win,&parent_win,&list,&num))
     return base;
   if (list)
     {
	for (i=num-1;;i--)
	  {
	     if ((!excl_child)||(!g_list_find(excludes,(gpointer *)list[i])))
	       {
		 if ((child=gdk_window_xid_at(list[i],wx,wy,x,y,excludes,excl_child))!=0)
		   {
		     XFree(list);
		     return child;
		   }
	       }
	     if (!i) break;
	  }
	XFree(list);
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
gdk_window_xid_at_coords(gint x, gint y, GList *excludes, gboolean excl_child)
{
   GdkWindow *window;
   GdkWindowPrivate *private;
   Display *disp;
   Window *list=NULL;
   Window root,child=0,parent_win=0,root_win=0;
   unsigned int num;
   int i;
   
   window=(GdkWindow*)&gdk_root_parent;
   private=(GdkWindowPrivate*)window;
   disp=private->xdisplay;
   root=private->xwindow;
   XGrabServer(disp);
   num=g_list_length(excludes);
   if (!XQueryTree(disp,root,&root_win,&parent_win,&list,&num))
	return root;
   if (list)
     {
       i = num - 1;
       do
	 {
	   XWindowAttributes xwa;

	   XGetWindowAttributes (disp, list [i], &xwa);

	   if (xwa.map_state != IsViewable)
	     continue;

	   if (excl_child && g_list_find(excludes,(gpointer *)list[i]))
	     continue;
	   
	   if ((child = gdk_window_xid_at (list[i], 0, 0, x, y, excludes, excl_child)) == 0)
	     continue;

	   if (excludes)
	     {
	       if (!g_list_find(excludes,(gpointer *)child))
		 {
		   XFree(list);
		   XUngrabServer(disp);
		   return child;
		 }
	     }
	   else
	     {
	       XFree(list);
	       XUngrabServer(disp);
	       return child;
	     }
	 } while (--i > 0);
	XFree(list);
     }
   XUngrabServer(disp);
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

  gdk_root_parent.xdisplay = gdk_display;
  gdk_root_parent.xwindow = gdk_root_window;
  gdk_root_parent.window_type = GDK_WINDOW_ROOT;
  gdk_root_parent.window.user_data = NULL;
  gdk_root_parent.width = width;
  gdk_root_parent.height = height;
  gdk_root_parent.children = NULL;
  gdk_root_parent.colormap = NULL;
}

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
  if (parent_private->destroyed)
    return NULL;

  xparent = parent_private->xwindow;
  parent_display = parent_private->xdisplay;

  private = g_new (GdkWindowPrivate, 1);
  window = (GdkWindow*) private;

  private->parent = parent;

  if (parent_private != &gdk_root_parent)
    parent_private->children = g_list_prepend (parent_private->children, window);

  private->xdisplay = parent_display;
  private->destroyed = FALSE;
  private->resize_count = 0;
  private->ref_count = 1;
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
  private->width = (attributes->width > 1) ? (attributes->width) : (1);
  private->height = (attributes->height > 1) ? (attributes->height) : (1);
  private->window_type = attributes->window_type;
  private->extension_events = FALSE;
  private->dnd_drag_data_type = None;
  private->dnd_drag_data_typesavail =
    private->dnd_drop_data_typesavail = NULL;
  private->dnd_drop_enabled = private->dnd_drag_enabled =
    private->dnd_drag_accepted = private->dnd_drag_datashow =
    private->dnd_drop_data_numtypesavail =
    private->dnd_drag_data_numtypesavail = 0;
  private->dnd_drag_eventmask = private->dnd_drag_savedeventmask = 0;

  private->filters = NULL;
  private->children = NULL;

  window->user_data = NULL;

  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_visual_get_system ();
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;

  xattributes.event_mask = StructureNotifyMask;
  for (i = 0; i < nevent_masks; i++)
    {
      if (attributes->event_mask & (1 << (i + 1)))
	xattributes.event_mask |= event_mask_table[i];
    }

  if (xattributes.event_mask)
    xattributes_mask |= CWEventMask;

  if(attributes_mask & GDK_WA_NOREDIR) {
	xattributes.override_redirect =
		(attributes->override_redirect == FALSE)?False:True;
	xattributes_mask |= CWOverrideRedirect;
  } else
    xattributes.override_redirect = False;

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      class = InputOutput;
      depth = visual->depth;

      if (attributes_mask & GDK_WA_COLORMAP)
	private->colormap = attributes->colormap;
      else
	private->colormap = gdk_colormap_get_system ();

      xattributes.background_pixel = BlackPixel (gdk_display, gdk_screen);
      xattributes.border_pixel = BlackPixel (gdk_display, gdk_screen);
      xattributes_mask |= CWBorderPixel | CWBackPixel;

      switch (private->window_type)
	{
	case GDK_WINDOW_TOPLEVEL:
	  xattributes.colormap = ((GdkColormapPrivate*) private->colormap)->xcolormap;
	  xattributes_mask |= CWColormap;

	  xparent = gdk_root_window;
	  break;

	case GDK_WINDOW_CHILD:
	  xattributes.colormap = ((GdkColormapPrivate*) private->colormap)->xcolormap;
	  xattributes_mask |= CWColormap;
	  break;

	case GDK_WINDOW_DIALOG:
	  xattributes.colormap = ((GdkColormapPrivate*) private->colormap)->xcolormap;
	  xattributes_mask |= CWColormap;

	  xparent = gdk_root_window;
	  break;

	case GDK_WINDOW_TEMP:
	  xattributes.colormap = ((GdkColormapPrivate*) private->colormap)->xcolormap;
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
      private->colormap = NULL;
    }

  private->xwindow = XCreateWindow (private->xdisplay, xparent,
				    x, y, private->width, private->height,
				    0, depth, class, xvisual,
				    xattributes_mask, &xattributes);
  gdk_window_ref (window);
  gdk_xid_table_insert (&private->xwindow, window);

  if (private->colormap)
    gdk_colormap_ref (private->colormap);

  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  switch (private->window_type)
    {
    case GDK_WINDOW_DIALOG:
      XSetTransientForHint (private->xdisplay, private->xwindow, xparent);
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_TEMP:
      XSetWMProtocols (private->xdisplay, private->xwindow, gdk_wm_window_protocols, 2);
      break;
    case GDK_WINDOW_CHILD:
      if ((attributes->wclass == GDK_INPUT_OUTPUT) &&
	  (private->colormap != gdk_colormap_get_system ()) &&
	  (private->colormap != gdk_window_get_colormap (gdk_window_get_toplevel (window))))
	{
	  GDK_NOTE (MISC, g_print ("adding colormap window\n"));
	  gdk_window_add_colormap_windows (window);
	}

      return window;
    default:

      return window;
    }

  size_hints.flags = PSize;
  size_hints.width = private->width;
  size_hints.height = private->height;

  wm_hints.flags = InputHint | StateHint | WindowGroupHint;
  wm_hints.window_group = gdk_leader_window;
  wm_hints.input = True;
  wm_hints.initial_state = NormalState;

  /* FIXME: Is there any point in doing this? Do any WM's pay
   * attention to PSize, and even if they do, is this the
   * correct value???
   */
  XSetWMNormalHints (private->xdisplay, private->xwindow, &size_hints);

  XSetWMHints (private->xdisplay, private->xwindow, &wm_hints);

  if (attributes_mask & GDK_WA_TITLE)
    title = attributes->title;
  else
    title = gdk_progname;

  XmbSetWMProperties (private->xdisplay, private->xwindow,
                      title, title,
                      NULL, 0,
                      NULL, NULL, NULL);

  if (attributes_mask & GDK_WA_WMCLASS)
    {
      class_hint = XAllocClassHint ();
      class_hint->res_name = attributes->wmclass_name;
      class_hint->res_class = attributes->wmclass_class;
      XSetClassHint (private->xdisplay, private->xwindow, class_hint);
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
  Window *children;
  guint nchildren;

  private = g_new (GdkWindowPrivate, 1);
  window = (GdkWindow*) private;

  XGetWindowAttributes (gdk_display, anid, &attrs);

  /* FIXME: This is pretty expensive. Maybe the caller should supply
   *        the parent */
  XQueryTree (gdk_display, anid, &root, &parent, &children, &nchildren);
  XFree (children);
  private->parent = gdk_xid_table_lookup (parent);

  parent_private = (GdkWindowPrivate *)private->parent;
  
  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);

  private->xwindow = anid;
  private->xdisplay = gdk_display;
  private->x = attrs.x;
  private->y = attrs.y;
  private->width = attrs.width;
  private->height = attrs.height;
  private->resize_count = 0;
  private->ref_count = 1;
  private->window_type = GDK_WINDOW_FOREIGN;
  private->destroyed = FALSE;
  private->extension_events = 0;

  private->colormap = NULL;

  private->dnd_drag_data_type = None;
  private->dnd_drag_data_typesavail =
    private->dnd_drop_data_typesavail = NULL;
  private->dnd_drop_enabled = private->dnd_drag_enabled =
    private->dnd_drag_accepted = private->dnd_drag_datashow =
    private->dnd_drop_data_numtypesavail =
    private->dnd_drag_data_numtypesavail = 0;
  private->dnd_drag_eventmask = private->dnd_drag_savedeventmask = 0;

  private->filters = NULL;
  private->children = NULL;

  window->user_data = NULL;

  gdk_window_ref (window);
  gdk_xid_table_insert (&private->xwindow, window);

  return window;
}

/* Call this function when you want a window and all its children to
   disappear.  When xdestroy is true, a request to destroy the XWindow
   is sent out.  When it is false, it is assumed that the XWindow has
   been or will be destroyed by destroying some ancestor of this
   window.  */

static void
gdk_window_internal_destroy (GdkWindow *window, gboolean xdestroy,
			     gboolean our_destroy)
{
  GdkWindowPrivate *private;
  GdkWindowPrivate *temp_private;
  GdkWindow *temp_window;
  GList *children;
  GList *tmp;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  switch (private->window_type)
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
    case GDK_WINDOW_FOREIGN:
      if (!private->destroyed)
	{
	  if (private->parent)
	    {
	      GdkWindowPrivate *parent_private = (GdkWindowPrivate *)private->parent;
	      if (parent_private->children)
		parent_private->children = g_list_remove (parent_private->children, window);
	    }

	  if (private->window_type != GDK_WINDOW_FOREIGN)
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

	  if(private->dnd_drag_data_numtypesavail > 0) 
	    {
	      g_free (private->dnd_drag_data_typesavail);
	      private->dnd_drag_data_typesavail = NULL;
	    }
	  if(private->dnd_drop_data_numtypesavail > 0) 
	    {
	      g_free (private->dnd_drop_data_typesavail);
	      private->dnd_drop_data_typesavail = NULL;
	    }

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
	  
	  if (private->window_type == GDK_WINDOW_FOREIGN)
	    {
	      if (our_destroy && (private->parent != NULL))
		{
		  /* It's somebody elses window, but in our heirarchy,
		   * so reparent it to the root window, and then send
		   * it a delete event, as if we were a WM
		   */
		  XClientMessageEvent xevent;
		  
		  gdk_window_hide (window);
		  gdk_window_reparent (window, NULL, 0, 0);
		  
		  xevent.type = ClientMessage;
		  xevent.window = private->xwindow;
		  xevent.message_type = gdk_wm_protocols;
		  xevent.format = 32;
		  xevent.data.l[0] = gdk_wm_delete_window;
		  xevent.data.l[1] = CurrentTime;
		  
		  XSendEvent (private->xdisplay, private->xwindow,
			      False, 0, (XEvent *)&xevent);
		}
	    }
	  else if (xdestroy)
	    XDestroyWindow (private->xdisplay, private->xwindow);

	  if (private->colormap)
	    gdk_colormap_unref (private->colormap);

	  private->destroyed = TRUE;
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
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed)
    {
      if (private->window_type == GDK_WINDOW_FOREIGN)
	gdk_window_internal_destroy (window, FALSE, FALSE);
      else
	g_warning ("GdkWindow %#lx unexpectedly destroyed", private->xwindow);
    }
  
  gdk_xid_table_remove (private->xwindow);
  gdk_window_unref (window);
}

GdkWindow*
gdk_window_ref (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  g_return_val_if_fail (window != NULL, NULL);

  private->ref_count += 1;
  return window;
}

void
gdk_window_unref (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  g_return_if_fail (window != NULL);

  private->ref_count -= 1;
  if (private->ref_count == 0)
    {
      if (!private->destroyed)
	g_warning ("losing last reference to undestroyed window\n");
      g_free (window);
    }
}

void
gdk_window_show (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      XRaiseWindow (private->xdisplay, private->xwindow);
      XMapWindow (private->xdisplay, private->xwindow);
    }
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    XUnmapWindow (private->xdisplay, private->xwindow);
}

void
gdk_window_withdraw (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    XWithdrawWindow (private->xdisplay, private->xwindow, 0);
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      XMoveWindow (private->xdisplay, private->xwindow, x, y);
      
      if (private->window_type == GDK_WINDOW_CHILD)
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

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed &&
      ((private->resize_count > 0) ||
       (private->width != (guint16) width) ||
       (private->height != (guint16) height)))
    {
      XResizeWindow (private->xdisplay, private->xwindow, width, height);
      private->resize_count += 1;

      if (private->window_type == GDK_WINDOW_CHILD)
	{
	  private->width = width;
	  private->height = height;
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

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    {
      XMoveResizeWindow (private->xdisplay, private->xwindow, x, y, width, height);
      
      if (private->window_type == GDK_WINDOW_CHILD)
	{
	  private->x = x;
	  private->y = y;
	  private->width = width;
	  private->height = height;
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

  if (!new_parent)
    new_parent = (GdkWindow*) &gdk_root_parent;

  window_private = (GdkWindowPrivate*) window;
  old_parent_private = (GdkWindowPrivate*)window_private->parent;
  parent_private = (GdkWindowPrivate*) new_parent;

  if (!window_private->destroyed && !parent_private->destroyed)
    XReparentWindow (window_private->xdisplay,
		     window_private->xwindow,
		     parent_private->xwindow,
		     x, y);

  old_parent_private->children = g_list_remove (old_parent_private->children, window);
  parent_private->children = g_list_prepend (parent_private->children, window);
  
}

void
gdk_window_clear (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed)
    XClearWindow (private->xdisplay, private->xwindow);
}

void
gdk_window_clear_area (GdkWindow *window,
		       gint       x,
		       gint       y,
		       gint       width,
		       gint       height)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  
  if (!private->destroyed)
    XClearArea (private->xdisplay, private->xwindow,
		x, y, width, height, False);
}

void
gdk_window_clear_area_e (GdkWindow *window,
		         gint       x,
		         gint       y,
		         gint       width,
		         gint       height)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  
  if (!private->destroyed)
    XClearArea (private->xdisplay, private->xwindow,
		x, y, width, height, True);
}

void
gdk_window_copy_area (GdkWindow    *window,
		      GdkGC        *gc,
		      gint          x,
		      gint          y,
		      GdkWindow    *source_window,
		      gint          source_x,
		      gint          source_y,
		      gint          width,
		      gint          height)
{
  GdkWindowPrivate *src_private;
  GdkWindowPrivate *dest_private;
  GdkGCPrivate *gc_private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (gc != NULL);
  
  if (source_window == NULL)
    source_window = window;
  
  src_private = (GdkWindowPrivate*) source_window;
  dest_private = (GdkWindowPrivate*) window;
  gc_private = (GdkGCPrivate*) gc;
  
  if (!src_private->destroyed && !dest_private->destroyed)
    {
      XCopyArea (dest_private->xdisplay, src_private->xwindow, dest_private->xwindow,
		 gc_private->xgc,
		 source_x, source_y,
		 width, height,
		 x, y);
    }
}

void
gdk_window_raise (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  
  if (!private->destroyed)
    XRaiseWindow (private->xdisplay, private->xwindow);
}

void
gdk_window_lower (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  
  if (!private->destroyed)
    XLowerWindow (private->xdisplay, private->xwindow);
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
  GdkWindowPrivate *private;
  XSizeHints size_hints;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
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
  
  if (flags)
    XSetWMNormalHints (private->xdisplay, private->xwindow, &size_hints);
}

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    XmbSetWMProperties (private->xdisplay, private->xwindow,
			title, title, NULL, 0, NULL, NULL, NULL);
}

void
gdk_window_set_background (GdkWindow *window,
			   GdkColor  *color)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;
  if (!private->destroyed)
    XSetWindowBackground (private->xdisplay, private->xwindow, color->pixel);
}

void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gint       parent_relative)
{
  GdkWindowPrivate *window_private;
  GdkPixmapPrivate *pixmap_private;
  Pixmap xpixmap;
  
  g_return_if_fail (window != NULL);
  
  window_private = (GdkWindowPrivate*) window;
  pixmap_private = (GdkPixmapPrivate*) pixmap;
  
  if (pixmap)
    xpixmap = pixmap_private->xwindow;
  else
    xpixmap = None;
  
  if (parent_relative)
    xpixmap = ParentRelative;
  
  if (!window_private->destroyed)
    XSetWindowBackgroundPixmap (window_private->xdisplay, window_private->xwindow, xpixmap);
}

void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkWindowPrivate *window_private;
  GdkCursorPrivate *cursor_private;
  Cursor xcursor;
  
  g_return_if_fail (window != NULL);
  
  window_private = (GdkWindowPrivate*) window;
  cursor_private = (GdkCursorPrivate*) cursor;
  
  if (!cursor)
    xcursor = None;
  else
    xcursor = cursor_private->xcursor;
  
  if (!window_private->destroyed)
    XDefineCursor (window_private->xdisplay, window_private->xwindow, xcursor);
}

void
gdk_window_set_colormap (GdkWindow   *window,
			 GdkColormap *colormap)
{
  GdkWindowPrivate *window_private;
  GdkColormapPrivate *colormap_private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (colormap != NULL);
  
  window_private = (GdkWindowPrivate*) window;
  colormap_private = (GdkColormapPrivate*) colormap;
  
  if (!window_private->destroyed)
    {
      XSetWindowColormap (window_private->xdisplay,
			  window_private->xwindow,
			  colormap_private->xcolormap);

      if (window_private->colormap)
	gdk_colormap_unref (window_private->colormap);
      window_private->colormap = colormap;
      gdk_colormap_ref (window_private->colormap);
      
      if (window_private->window_type != GDK_WINDOW_TOPLEVEL)
	gdk_window_add_colormap_windows (window);
    }
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
  GdkWindowPrivate *window_private;
  Window root;
  gint tx;
  gint ty;
  guint twidth;
  guint theight;
  guint tborder_width;
  guint tdepth;
  
  if (!window)
    window = (GdkWindow*) &gdk_root_parent;
  
  window_private = (GdkWindowPrivate*) window;
  
  if (!window_private->destroyed)
    {
      XGetGeometry (window_private->xdisplay, window_private->xwindow,
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
  
  window_private = (GdkWindowPrivate*) window;
  
  if (x)
    *x = window_private->x;
  if (y)
    *y = window_private->y;
}

void
gdk_window_get_size (GdkWindow *window,
		     gint       *width,
		     gint       *height)
{
  GdkWindowPrivate *window_private;
  
  g_return_if_fail (window != NULL);
  
  window_private = (GdkWindowPrivate*) window;
  
  if (width)
    *width = window_private->width;
  if (height)
    *height = window_private->height;
}

GdkVisual*
gdk_window_get_visual (GdkWindow *window)
{
  GdkWindowPrivate *window_private;
  XWindowAttributes window_attributes;
   
  g_return_val_if_fail (window != NULL, NULL);

  window_private = (GdkWindowPrivate*) window;
  /* Huh? ->parent is never set for a pixmap. We should just return
   * null immeditately
   */
  while (window_private && (window_private->window_type == GDK_WINDOW_PIXMAP))
    window_private = (GdkWindowPrivate*) window_private->parent;
  
  if (window_private && !window_private->destroyed)
    {
       if (window_private->colormap == NULL)
	 {
	    XGetWindowAttributes (window_private->xdisplay,
				  window_private->xwindow,
				  &window_attributes);
	    return gdk_visual_lookup (window_attributes.visual);
	 }
       else
	 return ((GdkColormapPrivate *)window_private->colormap)->visual;
    }
  
  return NULL;
}

GdkColormap*
gdk_window_get_colormap (GdkWindow *window)
{
  GdkWindowPrivate *window_private;
  XWindowAttributes window_attributes;
  
  g_return_val_if_fail (window != NULL, NULL);
  window_private = (GdkWindowPrivate*) window;

  g_return_val_if_fail (window_private->window_type != GDK_WINDOW_PIXMAP, NULL);
  if (!window_private->destroyed)
    {
      if (window_private->colormap == NULL)
	{
	  XGetWindowAttributes (window_private->xdisplay,
				window_private->xwindow,
				&window_attributes);
	  return gdk_colormap_lookup (window_attributes.colormap);
	 }
       else
	 return window_private->colormap;
    }
  
  return NULL;
}

GdkWindowType
gdk_window_get_type (GdkWindow *window)
{
  GdkWindowPrivate *window_private;

  g_return_val_if_fail (window != NULL, (GdkWindowType) -1);

  window_private = (GdkWindowPrivate*) window;
  return window_private->window_type;
}

gint
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  GdkWindowPrivate *private;
  gint return_val;
  Window child;
  gint tx, ty;

  g_return_val_if_fail (window != NULL, 0);

  private = (GdkWindowPrivate*) window;

  if (!private->destroyed)
    {
      return_val = XTranslateCoordinates (private->xdisplay,
					  private->xwindow,
					  gdk_root_window,
					  0, 0, &tx, &ty,
					  &child);
      
      if (x)
	*x = tx;
      if (y)
	*y = ty;
    }
  else
    return_val = 0;
  
  return return_val;
}

GdkWindow*
gdk_window_get_pointer (GdkWindow       *window,
			gint            *x,
			gint            *y,
			GdkModifierType *mask)
{
  GdkWindowPrivate *private;
  GdkWindow *return_val;
  Window root;
  Window child;
  int rootx, rooty;
  int winx, winy;
  unsigned int xmask;

  if (!window)
    window = (GdkWindow*) &gdk_root_parent;

  private = (GdkWindowPrivate*) window;

  return_val = NULL;
  if (!private->destroyed &&
      XQueryPointer (private->xdisplay, private->xwindow, &root, &child,
		     &rootx, &rooty, &winx, &winy, &xmask))
    {
      if (x) *x = winx;
      if (y) *y = winy;
      if (mask) *mask = xmask;
      
      if (child)
	return_val = gdk_window_lookup (child);
    }
  
  return return_val;
}

GdkWindow*
gdk_window_get_parent (GdkWindow *window)
{
  g_return_val_if_fail (window != NULL, NULL);

  return ((GdkWindowPrivate*) window)->parent;
}

GdkWindow*
gdk_window_get_toplevel (GdkWindow *window)
{
  GdkWindowPrivate *private;

  g_return_val_if_fail (window != NULL, NULL);

  private = (GdkWindowPrivate*) window;

  while (private->window_type == GDK_WINDOW_CHILD)
    {
      window = ((GdkWindowPrivate*) window)->parent;
      private = (GdkWindowPrivate*) window;
    }

  return window;
}

GList*
gdk_window_get_children (GdkWindow *window)
{
  GdkWindowPrivate *private;
  GdkWindow *child;
  GList *children;
  Window root;
  Window parent;
  Window *xchildren;
  unsigned int nchildren;
  unsigned int i;

  g_return_val_if_fail (window != NULL, NULL);

  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return NULL;

  XQueryTree (private->xdisplay, private->xwindow,
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

      XFree (xchildren);
    }

  return children;
}

GdkEventMask  
gdk_window_get_events      (GdkWindow       *window)
{
  GdkWindowPrivate *private;
  XWindowAttributes attrs;
  GdkEventMask event_mask;
  int i;

  g_return_val_if_fail (window != NULL, 0);

  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return 0;

  XGetWindowAttributes (gdk_display, private->xwindow, 
			&attrs);

  event_mask = 0;
  for (i = 0; i < nevent_masks; i++)
    {
      if (attrs.your_event_mask & event_mask_table[i])
	event_mask |= 1 << (i + 1);
    }

  return event_mask;
}

void          
gdk_window_set_events      (GdkWindow       *window,
			    GdkEventMask     event_mask)
{
  GdkWindowPrivate *private;
  long xevent_mask;
  int i;

  g_return_if_fail (window != NULL);

  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return;

  xevent_mask = StructureNotifyMask;
  for (i = 0; i < nevent_masks; i++)
    {
      if (event_mask & (1 << (i + 1)))
	xevent_mask |= event_mask_table[i];
    }
  
  XSelectInput (gdk_display, private->xwindow, 
		xevent_mask);
}

void
gdk_window_add_colormap_windows (GdkWindow *window)
{
  GdkWindow *toplevel;
  GdkWindowPrivate *toplevel_private;
  GdkWindowPrivate *window_private;
  Window *old_windows;
  Window *new_windows;
  int i, count;

  g_return_if_fail (window != NULL);

  toplevel = gdk_window_get_toplevel (window);
  toplevel_private = (GdkWindowPrivate*) toplevel;
  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
    return;

  if (!XGetWMColormapWindows (toplevel_private->xdisplay,
			      toplevel_private->xwindow,
			      &old_windows, &count))
    {
      old_windows = NULL;
      count = 0;
    }

  for (i = 0; i < count; i++)
    if (old_windows[i] == window_private->xwindow)
      return;

  new_windows = g_new (Window, count + 1);

  for (i = 0; i < count; i++)
    new_windows[i] = old_windows[i];
  new_windows[count] = window_private->xwindow;

  XSetWMColormapWindows (toplevel_private->xdisplay,
			 toplevel_private->xwindow,
			 new_windows, count + 1);

  g_free (new_windows);
  if (old_windows)
    XFree (old_windows);
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
  enum { UNKNOWN, NO, YES };

  static gint have_shape = UNKNOWN;

  GdkWindowPrivate *window_private;
  Pixmap pixmap;

  g_return_if_fail (window != NULL);

#ifdef HAVE_SHAPE_EXT
  if (have_shape == UNKNOWN)
    {
      int ignore;
      if (XQueryExtension(gdk_display, "SHAPE", &ignore, &ignore, &ignore))
	have_shape = YES;
      else
	have_shape = NO;
    }
  
  if (have_shape == YES)
    {
      window_private = (GdkWindowPrivate*) window;
      if (window_private->destroyed)
	return;
      
      if (mask)
	{
	  GdkWindowPrivate *pixmap_private;
	  
	  pixmap_private = (GdkWindowPrivate*) mask;
	  pixmap = (Pixmap) pixmap_private->xwindow;
	}
      else
	{
	  x = 0;
	  y = 0;
	  pixmap = None;
	}
      
      XShapeCombineMask  (window_private->xdisplay,
			  window_private->xwindow,
			  ShapeBounding,
			  x, y,
			  pixmap,
			  ShapeSet);
    }
#endif /* HAVE_SHAPE_EXT */
}

void
gdk_dnd_drag_addwindow (GdkWindow *window)
{
  GdkWindowPrivate *window_private;
  
  g_return_if_fail (window != NULL);
  
  window_private = (GdkWindowPrivate *) window;
  if (window_private->destroyed)
    return;
  
  if (window_private->dnd_drag_enabled == 1 && gdk_dnd.drag_really == 0)
    {
      gdk_dnd.drag_numwindows++;
      gdk_dnd.drag_startwindows = g_realloc (gdk_dnd.drag_startwindows,
					     gdk_dnd.drag_numwindows
					     * sizeof(GdkWindow *));
      gdk_dnd.drag_startwindows[gdk_dnd.drag_numwindows - 1] = window;
      window_private->dnd_drag_accepted = 0;
    } 
  else
    g_warning ("dnd_really is 1 or drag is not enabled! can't addwindow\n");
}

void
gdk_window_dnd_drag_set (GdkWindow   *window,
			 guint8       drag_enable,
			 gchar      **typelist,
			 guint        numtypes)
{
  GdkWindowPrivate *window_private;
  int i, wasset = 0;
  
  g_return_if_fail (window != NULL);
  window_private = (GdkWindowPrivate *) window;
  if (window_private->destroyed)
    return;
  
  window_private->dnd_drag_enabled = drag_enable ? 1 : 0;
  
  if (drag_enable)
    {
      g_return_if_fail(typelist != NULL);
      
      if (window_private->dnd_drag_data_numtypesavail > 3)
	wasset = 1;
      window_private->dnd_drag_data_numtypesavail = numtypes;
      
      window_private->dnd_drag_data_typesavail =
	g_realloc (window_private->dnd_drag_data_typesavail,
		   (numtypes + 1) * sizeof (GdkAtom));
      
      for (i = 0; i < numtypes; i++)
	{
	  /* Allow blanket use of ALL to get anything... */
	  if (strcmp (typelist[i], "ALL"))
	    window_private->dnd_drag_data_typesavail[i] =
	      gdk_atom_intern (typelist[i], FALSE);
	  else
	    window_private->dnd_drag_data_typesavail[i] = None;
	}
      
      /* 
       * set our extended type list if we need to 
       */
      if (numtypes > 3)
	gdk_property_change(window, gdk_dnd.gdk_XdeTypelist,
			    XA_PRIMARY, 32, GDK_PROP_MODE_REPLACE,
			    (guchar *)(window_private->dnd_drag_data_typesavail
			     + (sizeof(GdkAtom) * 3)),
			    (numtypes - 3) * sizeof(GdkAtom));
      else if (wasset)
	gdk_property_delete (window, gdk_dnd.gdk_XdeTypelist);
    }
  else
    {
      g_free (window_private->dnd_drag_data_typesavail);
      window_private->dnd_drag_data_typesavail = NULL;
      window_private->dnd_drag_data_numtypesavail = 0;
    }
}

void
gdk_window_dnd_drop_set (GdkWindow   *window,
			 guint8       drop_enable,
			 gchar      **typelist,
			 guint        numtypes,
			 guint8       destructive_op)
{
  GdkWindowPrivate *window_private;
  int i;
  
  g_return_if_fail (window != NULL);
  window_private = (GdkWindowPrivate *) window;
  if (window_private->destroyed)
    return;
  
  window_private->dnd_drop_enabled = drop_enable ? 1 : 0;
  if (drop_enable)
    {
      g_return_if_fail(typelist != NULL);
      
      window_private->dnd_drop_data_numtypesavail = numtypes;
      
      window_private->dnd_drop_data_typesavail =
	g_realloc (window_private->dnd_drop_data_typesavail,
		   (numtypes + 1) * sizeof (GdkAtom));
      
      for (i = 0; i < numtypes; i++)
	window_private->dnd_drop_data_typesavail[i] =
	  gdk_atom_intern (typelist[i], FALSE);
      
      window_private->dnd_drop_destructive_op = destructive_op;
    }
}

/* 
 * This is used to reply to a GDK_DRAG_REQUEST event
 * (which may be generated by XdeRequest or a confirmed drop... 
 */
void
gdk_window_dnd_data_set (GdkWindow       *window,
			 GdkEvent        *event,
			 gpointer         data,
			 gulong           data_numbytes)
{
  GdkWindowPrivate *window_private;
  XEvent sev;
  GdkEventDropDataAvailable tmp_ev;
  gchar *tmp;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (event != NULL);
  g_return_if_fail (data != NULL);
  g_return_if_fail (data_numbytes > 0);
  g_return_if_fail (event->type == GDK_DRAG_REQUEST);

  window_private = (GdkWindowPrivate *) window;
  g_return_if_fail (window_private->dnd_drag_accepted != 0);    
  if (window_private->destroyed)
    return;
  
  /* We set the property on our window... */
  gdk_property_change (window, window_private->dnd_drag_data_type,
		       XA_PRIMARY, 8, GDK_PROP_MODE_REPLACE, data,
		       data_numbytes);
  tmp = gdk_atom_name(window_private->dnd_drag_data_type);
#ifdef DEBUG_DND
  g_print("DnD type %s on window %ld\n", tmp, window_private->xwindow);
#endif
  g_free(tmp);
  
  /* 
   * Then we send the event to tell the receiving window that the
   * drop has happened 
   */
  tmp_ev.u.allflags = 0;
  tmp_ev.u.flags.protocol_version = DND_PROTOCOL_VERSION;
  tmp_ev.u.flags.isdrop = event->dragrequest.isdrop;
  
  sev.xclient.type = ClientMessage;
  sev.xclient.format = 32;
  sev.xclient.window = event->dragrequest.requestor;
  sev.xclient.message_type = gdk_dnd.gdk_XdeDataAvailable;
  sev.xclient.data.l[0] = window_private->xwindow;
  sev.xclient.data.l[1] = tmp_ev.u.allflags;
  sev.xclient.data.l[2] = window_private->dnd_drag_data_type;

  if (event->dragrequest.isdrop)
    sev.xclient.data.l[3] = event->dragrequest.drop_coords.x +
      (event->dragrequest.drop_coords.y << 16);
  else
    sev.xclient.data.l[3] = 0;

  sev.xclient.data.l[4] = event->dragrequest.timestamp;

  if (!gdk_send_xevent (event->dragrequest.requestor, False,
		       StructureNotifyMask, &sev))
    GDK_NOTE (DND, g_print("Sending XdeDataAvailable to %#x failed\n",
			   event->dragrequest.requestor));

}

void          
gdk_window_add_filter     (GdkWindow     *window,
			   GdkFilterFunc  function,
			   gpointer       data)
{
  GdkWindowPrivate *private;
  GList *tmp_list;
  GdkEventFilter *filter;

  private = (GdkWindowPrivate*) window;
  if (private && private->destroyed)
    return;

  if(private)
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

  if(private)
    private->filters = g_list_append (private->filters, filter);
  else
    gdk_default_filters = g_list_append (gdk_default_filters, filter);
}

void
gdk_window_remove_filter  (GdkWindow     *window,
			   GdkFilterFunc  function,
			   gpointer       data)
{
  GdkWindowPrivate *private;
  GList *tmp_list;
  GdkEventFilter *filter;

  private = (GdkWindowPrivate*) window;

  if(private)
    tmp_list = private->filters;
  else
    tmp_list = gdk_default_filters;

  while (tmp_list)
    {
      filter = (GdkEventFilter *)tmp_list->data;
      tmp_list = tmp_list->next;

      if ((filter->function == function) && (filter->data == data))
	{
	  if(private)
	    private->filters = g_list_remove_link (private->filters, tmp_list);
	  else
	    gdk_default_filters = g_list_remove_link (gdk_default_filters, tmp_list);
	  g_list_free_1 (tmp_list);
	  g_free (filter);
	  
	  return;
	}
    }
}

void
gdk_window_set_override_redirect(GdkWindow *window,
				 gboolean override_redirect)
{
  GdkWindowPrivate *private;
  XSetWindowAttributes attr;

  g_return_if_fail (window != NULL);
  private = (GdkWindowPrivate*) window;
  if (private->destroyed)
    return;

  attr.override_redirect = (override_redirect == FALSE)?False:True;
  XChangeWindowAttributes(gdk_display,
			  ((GdkWindowPrivate *)window)->xwindow,
			  CWOverrideRedirect,
			  &attr);
}

void          
gdk_window_set_icon        (GdkWindow *window, 
			    GdkWindow *icon_window,
			    GdkPixmap *pixmap,
			    GdkBitmap *mask)
{
  XWMHints wm_hints;
  GdkWindowPrivate *window_private;
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);
  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
    return;

  wm_hints.flags = 0;
  
  if (icon_window != NULL)
    {
      private = (GdkWindowPrivate *)icon_window;
      wm_hints.flags |= IconWindowHint;
      wm_hints.icon_window = private->xwindow;
    }

  if (pixmap != NULL)
    {
      private = (GdkWindowPrivate *)pixmap;
      wm_hints.flags |= IconPixmapHint;
      wm_hints.icon_pixmap = private->xwindow;
    }

  if (mask != NULL)
    {
      private = (GdkWindowPrivate *)mask;
      wm_hints.flags |= IconMaskHint;
      wm_hints.icon_mask = private->xwindow;
    }

  XSetWMHints (window_private->xdisplay, window_private->xwindow, &wm_hints);
}

void          
gdk_window_set_icon_name   (GdkWindow *window, 
			    gchar *    name)
{
  GdkWindowPrivate *window_private;
  XTextProperty property;
  gint res;

  g_return_if_fail (window != NULL);
  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
    return;
  res = XmbTextListToTextProperty (window_private->xdisplay,
				   &name, 1, XStdICCTextStyle,
                               	   &property);
  if (res < 0)
    {
      g_warning("Error converting icon name to text property: %d\n", res);
      return;
    }

  XSetWMIconName (window_private->xdisplay, window_private->xwindow,
		  &property);

  XFree(property.value);
}

void          
gdk_window_set_group   (GdkWindow *window, 
			GdkWindow *leader)
{
  XWMHints wm_hints;
  GdkWindowPrivate *window_private;
  GdkWindowPrivate *private;

  g_return_if_fail (window != NULL);
  g_return_if_fail (leader != NULL);
  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
    return;

  private = (GdkWindowPrivate *)leader;
  wm_hints.flags = WindowGroupHint;
  wm_hints.window_group = private->xwindow;

  XSetWMHints (window_private->xdisplay, window_private->xwindow, &wm_hints);
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

  GdkWindowPrivate *window_private;

  g_return_if_fail (window != NULL);
  window_private = (GdkWindowPrivate*) window;
  if (window_private->destroyed)
    return;

  if (!hints_atom)
    hints_atom = XInternAtom (window_private->xdisplay, 
			      _XA_MOTIF_WM_HINTS, FALSE);
  
  XGetWindowProperty (window_private->xdisplay, window_private->xwindow,
		      hints_atom, 0, sizeof(MotifWmHints)/4,
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

  XChangeProperty (window_private->xdisplay, window_private->xwindow,
		   hints_atom, hints_atom, 32, PropModeReplace,
		   (guchar *)hints, sizeof(MotifWmHints)/4);

  if (hints != new_hints)
    XFree (hints);
}

void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  MotifWmHints hints;

  hints.flags = MWM_HINTS_DECORATIONS;
  hints.decorations = decorations;

  gdk_window_set_mwm_hints (window, &hints);
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  MotifWmHints hints;

  hints.flags = MWM_HINTS_FUNCTIONS;
  hints.functions = functions;

  gdk_window_set_mwm_hints (window, &hints);
}
