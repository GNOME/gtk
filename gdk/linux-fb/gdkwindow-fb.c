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

#include "gdk.h"
#include "config.h"

#include "gdkwindow.h"
#include "gdkinputprivate.h"
#include "gdkprivate-fb.h"
#include "gdkinternals.h"

#include <limits.h>

/* Global variables: */
static GdkWindow *gdk_fb_window_containing_pointer = NULL;
static gpointer parent_class = NULL;

static void recompute_drawable (GdkDrawable *drawable);
static void gdk_fb_window_raise (GdkWindow *window);
static GdkRegion* gdk_window_fb_get_visible_region (GdkDrawable *drawable);

typedef struct
{
  GdkWindowChildChanged changed;
  GdkWindowChildGetPos get_pos;
  gpointer user_data;
}  GdkWindowChildHandlerData;

static void
g_free_2nd (gpointer a, gpointer b, gpointer data)
{
  g_free (b);
}

static void
gdk_window_impl_fb_finalize (GObject *object)
{
  GdkWindowFBData *fbd = GDK_WINDOW_FBDATA (object);

  if (GDK_WINDOW_P (fbd->drawable_data.wrapper)->mapped)
    gdk_window_hide (fbd->drawable_data.wrapper);

  if (fbd->cursor)
    gdk_cursor_unref (fbd->cursor);

  if (fbd->properties)
    {
      g_hash_table_foreach (fbd->properties, g_free_2nd, NULL);
      g_hash_table_destroy (fbd->properties);
    }

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_window_impl_fb_class_init (GdkWindowFBClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_impl_fb_finalize;
  
  /* Visible and clip regions are the same */
  drawable_class->get_clip_region = gdk_window_fb_get_visible_region;
  drawable_class->get_visible_region = gdk_window_fb_get_visible_region;
}

static void
gdk_window_impl_fb_init (GdkWindowFBData *impl)
{
  impl->drawable_data.depth = gdk_display->modeinfo.bits_per_pixel;
  impl->drawable_data.colormap = gdk_colormap_get_system ();
  impl->event_mask = GDK_STRUCTURE_MASK;
}

GType
_gdk_window_impl_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkWindowFBClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_window_impl_fb_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkWindowFBData),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_window_impl_fb_init,
      };
      
      object_type = g_type_register_static (gdk_drawable_impl_fb_get_type(),
                                            "GdkWindowFB",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

void
_gdk_windowing_window_init (void)
{
  GdkWindowAttr attr;

  GdkCursor *cursor;
  GdkWindowObject *private;

  attr.width = gdk_screen_width ();
  attr.height = gdk_screen_height ();
  attr.window_type = GDK_WINDOW_ROOT;
  attr.cursor = NULL;
  attr.event_mask = GDK_EXPOSURE_MASK;
  attr.wclass = GDK_INPUT_OUTPUT;
  gdk_parent_root = gdk_window_new (NULL, &attr, GDK_WA_CURSOR);
  private = (GdkWindowObject *)gdk_parent_root;

  private->mapped = TRUE;

  GDK_DRAWABLE_IMPL_FBDATA (gdk_parent_root)->lim_x = attr.width;
  GDK_DRAWABLE_IMPL_FBDATA (gdk_parent_root)->lim_y = attr.height;

  _gdk_fb_screen_gc = gdk_gc_new (gdk_parent_root);

  gdk_fb_drawable_clear (gdk_parent_root);
  
  /* Must be done after root is created, since gdk_cursor_new()
   * references gdk_parent_root.
   */
  cursor = gdk_cursor_new (GDK_LEFT_PTR);
  gdk_window_set_cursor (gdk_parent_root, cursor);
}

GdkWindow*
gdk_window_new (GdkWindow     *parent,
		GdkWindowAttr *attributes,
		gint           attributes_mask)
{
  GdkWindow *window;
  GdkWindowObject *private;
  GdkWindowObject *parent_private;
  GdkVisual *visual;
  GdkWindowFBData *impl;

  int x, y, depth;
  
  g_return_val_if_fail (attributes != NULL, NULL);
  
  if (!parent || attributes->window_type != GDK_WINDOW_CHILD)
    parent = gdk_parent_root;
  
  parent_private = (GdkWindowObject*) parent;
  
  window = (GdkWindow *)g_object_new (GDK_TYPE_WINDOW, NULL);
  private = (GdkWindowObject *)window;

  private->parent = parent_private;

  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = 0;

  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else
    y = 0;

  gdk_window_set_events (window, attributes->event_mask);

  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;
  else
    visual = gdk_visual_get_system ();

  impl = (GdkWindowFBData *)private->impl;
  impl->drawable_data.wrapper = window;
  private->x = x;
  private->y = y;
  impl->drawable_data.width = (attributes->width > 1) ? (attributes->width) : (1);
  impl->drawable_data.height = (attributes->height > 1) ? (attributes->height) : (1);
  private->window_type = impl->drawable_data.window_type = attributes->window_type;
  impl->drawable_data.mem = gdk_display->fbmem;
  impl->drawable_data.rowstride = gdk_display->sinfo.line_length;
  gdk_window_move_resize (window, x, y,
			  impl->drawable_data.width, impl->drawable_data.height);

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      depth = visual->depth;

      private->input_only = FALSE;
      private->depth = impl->drawable_data.depth;
      
      if ((attributes_mask & GDK_WA_COLORMAP) &&
	  attributes->colormap)
	impl->drawable_data.colormap = attributes->colormap;
      else
	impl->drawable_data.colormap = gdk_colormap_get_system ();
      
      switch (impl->drawable_data.window_type)
	{
	case GDK_WINDOW_TOPLEVEL:
	case GDK_WINDOW_CHILD:
	case GDK_WINDOW_DIALOG:
	case GDK_WINDOW_TEMP:
	default:
	  break;
	  
	case GDK_WINDOW_ROOT:
	  if (gdk_parent_root)
	    g_error ("cannot make windows of type GDK_WINDOW_ROOT");
	  break;
	case GDK_DRAWABLE_PIXMAP:
	  g_error ("cannot make windows of type GDK_DRAWABLE_PIXMAP (use gdk_pixmap_new)");
	  break;
	}
    }
  else
    {
      depth = 0;
      private->input_only = TRUE;
      impl->drawable_data.colormap = NULL;
    }

  gdk_drawable_ref (window);
  
  if (impl->drawable_data.colormap)
    gdk_colormap_ref (impl->drawable_data.colormap);
  
  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));

  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);
  
  return window;
}

/* This function is called when the XWindow is really gone.  */
void
_gdk_windowing_window_destroy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   foreign_destroy)
{
  GdkWindowObject *private;
  GdkRectangle r;

  private = (GdkWindowObject*) window;

  _gdk_selection_window_destroyed (window);

  /* Invalidate the area. */
  r.x = private->x;
  r.y = private->y;
  r.width = GDK_DRAWABLE_IMPL_FBDATA (window)->width;
  r.height = GDK_DRAWABLE_IMPL_FBDATA (window)->height;
  gdk_window_invalidate_rect_clear ((GdkWindow *)private->parent, &r);
}

static gboolean
all_parents_shown (GdkWindowObject *private)
{
  while (private->mapped)
    {
      if (private->parent)
	private = (GdkWindowObject *)private->parent;
      else
	return TRUE;
    }

  return FALSE;
}

static void
send_map_events (GdkWindowObject *private, gboolean is_map)
{
  GList *l;
  GdkWindow *parent = (GdkWindow *)private->parent;

  g_assert (is_map);

  if (!private->mapped)
    return;

  if (is_map)
    gdk_event_make ((GdkWindow *)private, GDK_MAP, TRUE);

  if (private->input_only)
    return;

  if (!parent)
    parent = (GdkWindow *)private;

  if (((GDK_DRAWABLE_IMPL_FBDATA (private)->abs_x > GDK_DRAWABLE_IMPL_FBDATA (parent)->lim_x) ||
       (GDK_DRAWABLE_IMPL_FBDATA (private)->abs_y > GDK_DRAWABLE_IMPL_FBDATA (parent)->lim_y) ||
       (GDK_DRAWABLE_IMPL_FBDATA (private)->lim_x < GDK_DRAWABLE_IMPL_FBDATA (parent)->llim_x) ||
       (GDK_DRAWABLE_IMPL_FBDATA (private)->lim_y < GDK_DRAWABLE_IMPL_FBDATA (parent)->llim_y)))
     return;

  if (is_map)
    gdk_window_clear ((GdkWindow *)private);

  for (l = private->children; l; l = l->next)
    send_map_events (l->data, is_map);
}

/* Cut & paste versions of the stuff in gdkwindow.c, with the addition of clearing the newly exposed region. */
void
gdk_window_invalidate_region_clear(GdkWindow *window,
				   GdkRegion *region)
{
  int i;
  GdkWindowObject *private = GDK_WINDOW_P (window);

  if (private->input_only || !private->mapped)
    return;

  if (private->bg_pixmap != GDK_NO_BG)
    for (i = 0; i < region->numRects; i++)
      gdk_window_clear_area (window,
			     region->rects[i].x1,
			     region->rects[i].y1,
			     region->rects[i].x2 - region->rects[i].x1,
			     region->rects[i].y2 - region->rects[i].y1);

  gdk_window_invalidate_region (window, region, FALSE);

  {
    GList *tmp_list;
    GdkRectangle child_rect;
    GdkRegion *child_region;

    tmp_list = private->children;
    while (tmp_list)
      {
	GdkWindowObject *child = tmp_list->data;
	tmp_list = tmp_list->next;

	if (!child->input_only)
	  {
	    gint width, height;

	    gdk_drawable_get_size (GDK_DRAWABLE (child),
				   &width, &height);
              
	    child_rect.x = child->x;
	    child_rect.y = child->y;
	    child_rect.width = width;
	    child_rect.height = height;
	      
	    child_region = gdk_region_rectangle (&child_rect);
	    gdk_region_intersect (child_region, region);
	      
	    if (!gdk_region_empty (child_region))
	      {
		gdk_region_offset (child_region, - child_rect.x, - child_rect.y);
		gdk_window_invalidate_region_clear ((GdkWindow *)child, child_region);
	      }
	      
	    gdk_region_destroy (child_region);
	  }
      }
  }  
}

void
gdk_window_invalidate_rect_clear (GdkWindow *window,
				  GdkRectangle *rect)
{
  GdkWindowObject *private = GDK_WINDOW_P (window);

  if (private->input_only || !private->mapped)
    return;

  if (GDK_WINDOW_P (window)->bg_pixmap != GDK_NO_BG)
    gdk_window_clear_area (window, rect->x, rect->y, rect->width, rect->height);
  gdk_window_invalidate_rect (window, rect, FALSE);

  {
    GList *tmp_list;
    GdkRectangle child_rect, new_rect;

    tmp_list = private->children;
    while (tmp_list)
      {
	GdkWindowObject *child = tmp_list->data;
	tmp_list = tmp_list->next;

	if (!child->input_only)
	  {
	    gint width, height;

	    width = GDK_DRAWABLE_IMPL_FBDATA (child)->width;
	    height = GDK_DRAWABLE_IMPL_FBDATA (child)->height;
              
	    child_rect.x = child->x;
	    child_rect.y = child->y;
	    child_rect.width = width;
	    child_rect.height = height;
	      
	    if (gdk_rectangle_intersect (rect, &child_rect, &new_rect))
	      {
		new_rect.x -= child_rect.x;
		new_rect.y -= child_rect.y;
		  
		gdk_window_invalidate_rect_clear ((GdkWindow *)child, &new_rect);
	      }
	  }
      }
  }
}

void
gdk_fb_redraw_all (void)
{
  GdkRectangle r;
  r.x = r.y = 0;
  r.width = GDK_DRAWABLE_IMPL_FBDATA (gdk_parent_root)->width;
  r.height = GDK_DRAWABLE_IMPL_FBDATA (gdk_parent_root)->height;
  gdk_window_invalidate_rect_clear (gdk_parent_root, &r);
  gdk_window_process_all_updates ();
}


/* Focus follows pointer */
GdkWindow *
_gdk_fb_window_find_focus (GdkWindow *window_with_mouse)
{
  if (_gdk_fb_keyboard_grab_window)
    return _gdk_fb_keyboard_grab_window;
  else if (window_with_mouse)
    {
      GdkWindowObject *priv = (GdkWindowObject *)window_with_mouse;
      while (priv != (GdkWindowObject *)gdk_parent_root)
	{
	  if ((priv->parent == (GdkWindowObject *)gdk_parent_root) && priv->mapped)
	    return (GdkWindow *)priv;
	  priv = priv->parent;
	}
    }
 
  return gdk_parent_root;
}

GdkWindow *
gdk_fb_window_find_focus (void)
{
  return _gdk_fb_window_find_focus (gdk_fb_window_containing_pointer);
}


static void
gdk_fb_send_focus_change (GdkWindow *old_window_containing_pointer,
			  GdkWindow *new_window_containing_pointer)
{
  GdkEventFocus *event;
  GdkWindow *old_win, *new_win;
  old_win = _gdk_fb_window_find_focus (old_window_containing_pointer);
  new_win = _gdk_fb_window_find_focus (new_window_containing_pointer);

  if (old_win != new_win)
    {
      event = (GdkEventFocus *)gdk_event_make (old_win, GDK_FOCUS_CHANGE, TRUE);
      if (event)
	event->in = FALSE;
      event = (GdkEventFocus *)gdk_event_make (new_win, GDK_FOCUS_CHANGE, TRUE);
      if (event)
	event->in = TRUE;
    }
}

static GdkWindow *
gdk_fb_find_common_ancestor (GdkWindow *win1,
			     GdkWindow *win2)
{
  GdkWindowObject *tmp;
  GList *path1 = NULL, *path2 = NULL;
  GList *list1, *list2;
  
  tmp = GDK_WINDOW_OBJECT (win1);
  while (tmp) 
    {
      path1 = g_list_prepend(path1, tmp);
      tmp = tmp->parent;
    }

  tmp = GDK_WINDOW_OBJECT (win2);
  while (tmp) 
    {
      path2 = g_list_prepend(path2, tmp);
      tmp = tmp->parent;
    }
  
  list1 = path1;
  list2 = path2;
  tmp = NULL;
  while (list1 && list2 && (list1->data == list2->data)) 
    {
      tmp = (GdkWindowObject *)list1->data;
      list1 = g_list_next (list1);
      list2 = g_list_next (list2);
    }
  g_list_free (path1);
  g_list_free (path2);
  return GDK_WINDOW (tmp);
}

void
gdk_fb_window_send_crossing_events (GdkWindow *dest,
				    GdkCrossingMode mode)
{
  GdkWindow *c;
  GdkWindow *win, *last, *next;
  GdkEvent *event;
  gint x, y, x_int, y_int;
  GdkModifierType my_mask;
  GList *path, *list;
  gboolean non_linear;
  gboolean only_grabbed_window;
  GdkWindow *a;
  GdkWindow *b;

  if ((mode == GDK_CROSSING_NORMAL) &&
      (dest == gdk_fb_window_containing_pointer))
    return;

  if (gdk_fb_window_containing_pointer == NULL)
    gdk_fb_window_containing_pointer = gdk_window_ref (gdk_parent_root);

  if (mode == GDK_CROSSING_UNGRAB)
    a = _gdk_fb_pointer_grab_window;
  else
    a = gdk_fb_window_containing_pointer;
  b = dest;

  /* When grab in progress only send normal crossing events about
   * the grabbed window.
   */
  only_grabbed_window = (_gdk_fb_pointer_grab_window_events != NULL) &&
                        (mode == GDK_CROSSING_NORMAL);
  
  if (a==b)
    return;

  gdk_fb_mouse_get_info (&x, &y, &my_mask);

  c = gdk_fb_find_common_ancestor (a, b);

  non_linear = (c != a) && (c != b);

  if (!only_grabbed_window || (a == _gdk_fb_pointer_grab_window))
    event = gdk_event_make (a, GDK_LEAVE_NOTIFY, TRUE);
  else
    event = NULL;
  if (event)
    {
      event->crossing.subwindow = NULL;
      gdk_window_get_root_origin (a, &x_int, &y_int);
      event->crossing.x = x - x_int;
      event->crossing.y = y - y_int;
      event->crossing.x_root = x;
      event->crossing.y_root = y;
      event->crossing.mode = mode;
      if (non_linear)
	event->crossing.detail = GDK_NOTIFY_NONLINEAR;
      else if (c==a)
	event->crossing.detail = GDK_NOTIFY_INFERIOR;
      else
	event->crossing.detail = GDK_NOTIFY_ANCESTOR;
      event->crossing.focus = FALSE;
      event->crossing.state = my_mask;
    }
  
  /* Traverse up from a to (excluding) c */
  if (c != a)
    {
      last = a;
      win = GDK_WINDOW (GDK_WINDOW_OBJECT (a)->parent);
      while (win != c)
	{
	  if (!only_grabbed_window || (win == _gdk_fb_pointer_grab_window))
	    event = gdk_event_make (win, GDK_LEAVE_NOTIFY, TRUE);
	  else
	    event = NULL;
	  if (event)
	    {
	      event->crossing.subwindow = gdk_window_ref (last);
	      gdk_window_get_root_origin (win, &x_int, &y_int);
	      event->crossing.x = x - x_int;
	      event->crossing.y = y - y_int;
	      event->crossing.x_root = x;
	      event->crossing.y_root = y;
	      event->crossing.mode = mode;
	      if (non_linear)
		event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	      else
		event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	      event->crossing.focus = FALSE;
	      event->crossing.state = my_mask;
	    }
	  last = win;
	  win = GDK_WINDOW (GDK_WINDOW_OBJECT (win)->parent);
	}
    }
  
  /* Traverse down from c to b */
  if (c != b) 
    {
      path = NULL;
      win = GDK_WINDOW( GDK_WINDOW_OBJECT (b)->parent);
      while (win != c)
	{
	  path = g_list_prepend (path, win);
	  win = GDK_WINDOW( GDK_WINDOW_OBJECT (win)->parent);
	}
      
      list = path;
      while (list) 
	{
	  win = (GdkWindow *)list->data;
	  list = g_list_next (list);
	  if (list)
	    next = (GdkWindow *)list->data;
	  else 
	    next = b;
	  
	  if (!only_grabbed_window || (win == _gdk_fb_pointer_grab_window))
	    event = gdk_event_make (win, GDK_ENTER_NOTIFY, TRUE);
	  else
	    event = NULL;
	  if (event)
	    {
	      event->crossing.subwindow = gdk_window_ref (next);
	      gdk_window_get_root_origin (win, &x_int, &y_int);
	      event->crossing.x = x - x_int;
	      event->crossing.y = y - y_int;
	      event->crossing.x_root = x;
	      event->crossing.y_root = y;
	      event->crossing.mode = mode;
	      if (non_linear)
		event->crossing.detail = GDK_NOTIFY_NONLINEAR_VIRTUAL;
	      else
		event->crossing.detail = GDK_NOTIFY_VIRTUAL;
	      event->crossing.focus = FALSE;
	      event->crossing.state = my_mask;
	    }
	}
      g_list_free (path);
    }

  if (!only_grabbed_window || (b == _gdk_fb_pointer_grab_window))
    event = gdk_event_make (b, GDK_ENTER_NOTIFY, TRUE);
  else
    event = NULL;
  if (event)
    {
      event->crossing.subwindow = NULL;
      gdk_window_get_root_origin (b, &x_int, &y_int);
      event->crossing.x = x - x_int;
      event->crossing.y = y - y_int;
      event->crossing.x_root = x;
      event->crossing.y_root = y;
      event->crossing.mode = mode;
      if (non_linear)
	event->crossing.detail = GDK_NOTIFY_NONLINEAR;
      else if (c==a)
	event->crossing.detail = GDK_NOTIFY_ANCESTOR;
      else
	event->crossing.detail = GDK_NOTIFY_INFERIOR;
      event->crossing.focus = FALSE;
      event->crossing.state = my_mask;
    }

  if ((mode != GDK_CROSSING_GRAB) &&
      (b != gdk_fb_window_containing_pointer))
    {
      gdk_fb_send_focus_change (gdk_fb_window_containing_pointer, b);
      gdk_window_unref (gdk_fb_window_containing_pointer);
      gdk_fb_window_containing_pointer = gdk_window_ref (b);
    }
}

void
gdk_window_show (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindow *mousewin;

  g_return_if_fail (window != NULL);
  
  private = (GdkWindowObject*) window;

  if (!private->destroyed && !private->mapped)
    {
      private->mapped = TRUE;
      gdk_fb_window_raise (window);
      
      if (all_parents_shown ((GdkWindowObject *)private->parent))
	{
	  GdkRectangle rect;

	  recompute_drawable ((GdkDrawable *)window);

	  send_map_events (private, TRUE);

	  mousewin = gdk_window_at_pointer (NULL, NULL);
	  gdk_fb_window_send_crossing_events (mousewin, 
					      GDK_CROSSING_NORMAL);

	  if (private->input_only)
	    return;

	  rect.x = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_x;
	  rect.y = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_y;
	  rect.width = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_x - rect.x;
	  rect.height = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_y - rect.y;
	  gdk_window_invalidate_rect_clear (gdk_parent_root, &rect);
	}
    }
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowObject *private;
  GdkWindow *mousewin;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowObject*) window;

  if (!private->destroyed && private->mapped)
    {
      GdkEvent *event;
      GdkRectangle r;
      gboolean do_hide;

      event = gdk_event_make (window, GDK_UNMAP, TRUE);

      r.x = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_x;
      r.y = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_y;
      r.width = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_x - r.x;
      r.height = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_y - r.y;

      private->mapped = FALSE;

      if (private->parent == GDK_WINDOW_P(gdk_parent_root))
	gdk_fb_drawable_clear(gdk_parent_root);

      mousewin = gdk_window_at_pointer (NULL, NULL);
      gdk_fb_window_send_crossing_events (mousewin, 
					  GDK_CROSSING_NORMAL);

      do_hide = gdk_fb_cursor_need_hide (&r);

      if (do_hide)
	gdk_fb_cursor_hide ();
      if (window == _gdk_fb_pointer_grab_window)
	gdk_pointer_ungrab (GDK_CURRENT_TIME);
      if (window == _gdk_fb_keyboard_grab_window)
	gdk_keyboard_ungrab (GDK_CURRENT_TIME);
      gdk_window_invalidate_rect_clear (gdk_parent_root, &r);
      if (do_hide)
	gdk_fb_cursor_unhide ();
    }
}

void
gdk_window_withdraw (GdkWindow *window)
{
  gdk_window_hide (window);
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_window_move_resize (window, x, y,
			  GDK_DRAWABLE_IMPL_FBDATA (private)->width,
			  GDK_DRAWABLE_IMPL_FBDATA (private)->height);
}

void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GdkWindowObject *private;
  gint x, y;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowObject*) window;
  
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  x = private->x;
  y = private->y;
  
  if (private->parent && (private->parent->window_type != GDK_WINDOW_CHILD))
    {
      GdkWindowChildHandlerData *data;
      
      data = g_object_get_data (G_OBJECT (private->parent), "gdk-window-child-handler");

      if (data)
	(*data->get_pos) (window, &x, &y, data->user_data);
    }

  gdk_window_move_resize (window, x, y, width, height);
}

static void
recompute_abs_positions(GdkDrawable *drawable,
			gint parent_x, gint parent_y,
			gint parent_llim_x, gint parent_llim_y,
			gint parent_lim_x, gint parent_lim_y)
{
  GList *l;

  if (GDK_IS_WINDOW (drawable))
    {
      GdkWindowObject *private = GDK_WINDOW_P (drawable);
      int x, y;

      if (!private->mapped)
	return;

      
      GDK_DRAWABLE_IMPL_FBDATA (private)->abs_x = parent_x + private->x;
      GDK_DRAWABLE_IMPL_FBDATA (private)->abs_y = parent_y + private->y;
      x = MAX (parent_llim_x, GDK_DRAWABLE_IMPL_FBDATA (private)->abs_x);
      x = MIN (x, parent_lim_x);
      GDK_DRAWABLE_IMPL_FBDATA (private)->llim_x = x;
      y = MAX (parent_llim_y, GDK_DRAWABLE_IMPL_FBDATA (private)->abs_y);
      y = MIN (y, parent_lim_y);
      GDK_DRAWABLE_IMPL_FBDATA (private)->llim_y = y;
      x = MIN (parent_lim_x,
	       GDK_DRAWABLE_IMPL_FBDATA (private)->abs_x + GDK_DRAWABLE_IMPL_FBDATA (private)->width);
      x = MAX (x, GDK_DRAWABLE_IMPL_FBDATA (private)->llim_x);
      GDK_DRAWABLE_IMPL_FBDATA (private)->lim_x = x;
      y = MIN (parent_lim_y,
	       GDK_DRAWABLE_IMPL_FBDATA (private)->abs_y + GDK_DRAWABLE_IMPL_FBDATA (private)->height);
      y = MAX (y, GDK_DRAWABLE_IMPL_FBDATA (private)->llim_y);
      GDK_DRAWABLE_IMPL_FBDATA (private)->lim_y = y;

      g_assert (GDK_DRAWABLE_IMPL_FBDATA (private)->llim_x <= GDK_DRAWABLE_IMPL_FBDATA (private)->lim_x);
      g_assert (GDK_DRAWABLE_IMPL_FBDATA (private)->llim_y <= GDK_DRAWABLE_IMPL_FBDATA (private)->lim_y);

      for (l = private->children; l; l = l->next)
	recompute_abs_positions (l->data,
				 GDK_DRAWABLE_IMPL_FBDATA (private)->abs_x,
				 GDK_DRAWABLE_IMPL_FBDATA (private)->abs_y,
				 GDK_DRAWABLE_IMPL_FBDATA (private)->llim_x,
				 GDK_DRAWABLE_IMPL_FBDATA (private)->llim_y,
				 GDK_DRAWABLE_IMPL_FBDATA (private)->lim_x,
				 GDK_DRAWABLE_IMPL_FBDATA (private)->lim_y);
    }
  else
    {
      GDK_DRAWABLE_IMPL_FBDATA (drawable)->abs_x = 0;
      GDK_DRAWABLE_IMPL_FBDATA (drawable)->abs_y = 0;
      GDK_DRAWABLE_IMPL_FBDATA (drawable)->llim_x = 0;
      GDK_DRAWABLE_IMPL_FBDATA (drawable)->llim_y = 0;
      GDK_DRAWABLE_IMPL_FBDATA (drawable)->lim_x = GDK_DRAWABLE_IMPL_FBDATA (drawable)->width;
      GDK_DRAWABLE_IMPL_FBDATA (drawable)->lim_y = GDK_DRAWABLE_IMPL_FBDATA (drawable)->height;
    }
}

static void
recompute_drawable (GdkDrawable *drawable)
{
  if (GDK_IS_WINDOW (drawable))
    {
      GdkWindowObject *private = GDK_WINDOW_P (drawable);
      GdkWindow *parent;

      parent = (GdkWindow *)private->parent;
      if (!parent)
	parent = gdk_parent_root;

      recompute_abs_positions (drawable,
			       GDK_DRAWABLE_IMPL_FBDATA (parent)->abs_x,
			       GDK_DRAWABLE_IMPL_FBDATA (parent)->abs_y,
			       GDK_DRAWABLE_IMPL_FBDATA (parent)->llim_x,
			       GDK_DRAWABLE_IMPL_FBDATA (parent)->llim_y,
			       GDK_DRAWABLE_IMPL_FBDATA (parent)->lim_x,
			       GDK_DRAWABLE_IMPL_FBDATA (parent)->lim_y);
    }
  else
    recompute_abs_positions (drawable, 0, 0, 0, 0, INT_MAX, INT_MAX);
}

void
gdk_fb_window_move_resize (GdkWindow *window,
			   gint       x,
			   gint       y,
			   gint       width,
			   gint       height,
			   gboolean   send_expose_events)
{
  GdkWindowObject *private;
  gint dx, dy, dw, dh;
  gint i, draw_dir;
  GdkEvent *event;
  GdkWindow *mousewin;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;

  private = (GdkWindowObject*) window;

  if (private->parent && (private->parent->window_type != GDK_WINDOW_CHILD))
    {
      GdkWindowChildHandlerData *data;
      
      data = g_object_get_data (G_OBJECT (private->parent), "gdk-window-child-handler");

      if (data && (*data->changed) (window, x, y, width, height, data->user_data))
	  return;
    }
  
  if (!private->destroyed)
    {
      GdkRegion *old_region = NULL;

      if (private->input_only)
	send_expose_events = FALSE;

      if (private->mapped && send_expose_events)
	old_region = gdk_fb_clip_region (GDK_DRAWABLE_IMPL(window), NULL, TRUE, FALSE);

      dx = x - private->x;
      dy = y - private->y;
      dw = width - GDK_DRAWABLE_IMPL_FBDATA (private)->width;
      dh = height - GDK_DRAWABLE_IMPL_FBDATA (private)->height;

      private->x = x;
      private->y = y;
      GDK_DRAWABLE_IMPL_FBDATA (private)->width = width;
      GDK_DRAWABLE_IMPL_FBDATA (private)->height = height;

      if (private->mapped)
	{
	  recompute_drawable ((GdkDrawable *)window);

	  if (send_expose_events)
	    {
	      GdkRegion *new_region, *region;
	      gboolean handle_cursor = FALSE;

	      new_region = gdk_fb_clip_region (GDK_DRAWABLE_IMPL (window), NULL, TRUE, FALSE);

	      region = gdk_region_copy (old_region);
	      gdk_region_offset (region, dx, dy);
	      gdk_region_intersect (region, new_region);

	      if (region->numRects && ((dx != 0) || (dy != 0)))
		{
		  GdkFBDrawingContext fbdc;

		  if (gdk_fb_cursor_region_need_hide (old_region) ||
		      gdk_fb_cursor_region_need_hide (new_region))
		    {
		      gdk_fb_cursor_hide ();
		      handle_cursor = TRUE;
		    }

		  gdk_fb_drawing_context_init (&fbdc, GDK_DRAWABLE_IMPL (gdk_parent_root), NULL, FALSE, FALSE);
		  
		  draw_dir = 1;
		  if ((dy>0) || ((dy==0) && (dx>0)))
		    draw_dir = -1;
			      
		  for (i=(draw_dir>0)?0:region->numRects-1;i >= 0 && i < region->numRects; i+=draw_dir)
		    {
		      GdkRegionBox *reg = &region->rects[i];

		      gdk_fb_draw_drawable_3 (GDK_DRAWABLE_IMPL(gdk_parent_root),
					      _gdk_fb_screen_gc,
					      GDK_DRAWABLE_IMPL(gdk_parent_root),
					      &fbdc,
					      (reg->x1 - dx),
					      (reg->y1 - dy),
					      (reg->x1),
					      (reg->y1),
					      (reg->x2 - reg->x1),
					      (reg->y2 - reg->y1));
		    }
		  gdk_fb_drawing_context_finalize (&fbdc);
		}

	      gdk_region_union (new_region, old_region);
	      gdk_region_subtract (new_region, region);
	      gdk_region_destroy (region);

	      gdk_window_invalidate_region_clear (gdk_parent_root, new_region);
	      if (handle_cursor)
		gdk_fb_cursor_unhide ();

	      gdk_region_destroy (old_region);
	      gdk_region_destroy (new_region);
	    }
	  /* Send GdkEventConfigure for toplevel windows */
	  if (private->window_type != GDK_WINDOW_CHILD)
	    {
	      event = gdk_event_make (window, GDK_CONFIGURE, TRUE);
	      if (event) 
		{
		  event->configure.x = private->x;
		  event->configure.y = private->y;
		  event->configure.width = GDK_DRAWABLE_IMPL_FBDATA (private)->width;
		  event->configure.height = GDK_DRAWABLE_IMPL_FBDATA (private)->height;
		}
	    }

	  /* The window the pointer is in might have changed */
	  mousewin = gdk_window_at_pointer (NULL, NULL);
	  gdk_fb_window_send_crossing_events (mousewin, 
					      GDK_CROSSING_NORMAL);
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
  gdk_fb_window_move_resize (window, x, y, width, height, TRUE);
}

void
gdk_window_reparent (GdkWindow *window,
		     GdkWindow *new_parent,
		     gint       x,
		     gint       y)
{
  GdkWindowObject *window_private;
  GdkWindowObject *parent_private;
  GdkWindowObject *old_parent_private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (new_parent != NULL);
  g_return_if_fail (GDK_IS_WINDOW (new_parent));
  
  if (!new_parent)
    new_parent = gdk_parent_root;

  window_private = (GdkWindowObject*) window;
  old_parent_private = (GdkWindowObject*)window_private->parent;
  parent_private = (GdkWindowObject*) new_parent;
  
  g_assert(GDK_DRAWABLE_IMPL_FBDATA (window_private)->colormap);

  window_private->parent = (GdkWindowObject *)new_parent;
  
  if (old_parent_private)
    old_parent_private->children = g_list_remove (old_parent_private->children, window);

  parent_private->children = g_list_prepend (parent_private->children, window);

  if (window_private->mapped)
    {
      GdkRectangle r;
      GdkRegion *region;

      r.x = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_x;
      r.y = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_y;
      r.width = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_x - r.x;
      r.height = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_y - r.y;
      region = gdk_region_rectangle (&r);

      recompute_drawable ((GdkDrawable *)window);
      r.x = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_x;
      r.y = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_y;
      r.width = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_x - r.x;
      r.height = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_y - r.y;
      gdk_region_union_with_rect (region, &r);

      gdk_window_invalidate_region_clear (gdk_parent_root, region);
      gdk_region_destroy (region);
    }
}


void
_gdk_windowing_window_clear_area (GdkWindow *window,
				  gint       x,
				  gint       y,
				  gint       width,
				  gint       height)
{
  GdkPixmap *bgpm;
  GdkWindow *relto;

  if (GDK_WINDOW_P (window)->input_only)
    return;

  bgpm = GDK_WINDOW_P (window)->bg_pixmap;

  if (width == 0)
    width = GDK_DRAWABLE_IMPL_FBDATA (window)->width - x;
  
  if (height == 0)
    height = GDK_DRAWABLE_IMPL_FBDATA (window)->height - y;
  
#if 0  
  for (relto = window; bgpm == GDK_PARENT_RELATIVE_BG && relto; relto = (GdkWindow *)GDK_WINDOW_P(relto)->parent)
    bgpm = GDK_WINDOW_P (relto)->bg_pixmap;
#endif

  if (bgpm && bgpm != GDK_NO_BG)
    {
      int curx, cury;
      int xtrans, ytrans;
      int xstep, ystep;
      GdkFBDrawingContext fbdc;

      return; /* Don't bother doing this - gtk+ will do it itself using GC tiles. If removing this line,
		 then also remove the #if 0 stuff */

      gdk_fb_drawing_context_init (&fbdc, window, NULL, FALSE, TRUE);

      xtrans = GDK_DRAWABLE_IMPL_FBDATA (relto)->abs_x - GDK_DRAWABLE_IMPL_FBDATA (window)->abs_x;
      ytrans = GDK_DRAWABLE_IMPL_FBDATA (relto)->abs_y - GDK_DRAWABLE_IMPL_FBDATA (window)->abs_y;

      for (cury = y - ytrans; cury < (y - ytrans + height); cury += ystep)
	{
	  int drawh = cury % GDK_DRAWABLE_FBDATA (bgpm)->height;
	  ystep = GDK_DRAWABLE_FBDATA (bgpm)->height - drawh;

	  for(curx = x - xtrans; curx < (x - xtrans + width); curx += xstep)
	    {
	      int draww = curx % GDK_DRAWABLE_IMPL_FBDATA (bgpm)->width;
	      xstep = GDK_DRAWABLE_IMPL_FBDATA (bgpm)->width - draww;

	      gdk_fb_draw_drawable_3 (GDK_DRAWABLE_IMPL (window),
				      _gdk_fb_screen_gc,
				      GDK_DRAWABLE_IMPL (bgpm),
				      &fbdc,
				      draww, drawh,
				      curx + xtrans, cury + ytrans,
				      xstep, ystep);
	    }
	}

      gdk_fb_drawing_context_finalize (&fbdc);
    }
  else if (!bgpm)
    gdk_fb_draw_rectangle (GDK_DRAWABLE_IMPL (window), _gdk_fb_screen_gc, TRUE, x, y, width, height);
}

/* What's the diff? */
void
_gdk_windowing_window_clear_area_e (GdkWindow *window,
				    gint       x,
				    gint       y,
				    gint       width,
				    gint       height)
{
  _gdk_windowing_window_clear_area (window, x, y, width, height);
}

static void
gdk_fb_window_raise (GdkWindow *window)
{
  GdkWindowObject *parent;

  parent = GDK_WINDOW_OBJECT (window)->parent;

  parent->children = g_list_remove (parent->children, window);
  parent->children = g_list_prepend (parent->children, window);
}

static void
gdk_fb_window_lower (GdkWindow *window)
{
  GdkWindowObject *parent;

  parent = GDK_WINDOW_OBJECT (window)->parent;

  parent->children = g_list_remove (parent->children, window);
  parent->children = g_list_append (parent->children, window);
}


void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_fb_window_raise (window);
  
  if (GDK_WINDOW_OBJECT (window)->parent)
    gdk_window_invalidate_rect (window, NULL, TRUE);
}

void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  gdk_fb_window_lower (window);
  
  if (GDK_WINDOW_OBJECT (window)->parent)
    gdk_window_invalidate_rect (window, NULL, TRUE);
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
}

void 
gdk_window_set_geometry_hints (GdkWindow      *window,
			       GdkGeometry    *geometry,
			       GdkWindowHints  geom_mask)
{
}

void
gdk_window_set_title (GdkWindow   *window,
		      const gchar *title)
{
}

void          
gdk_window_set_role (GdkWindow   *window,
		     const gchar *role)
{
}

void          
gdk_window_set_transient_for (GdkWindow *window, 
			      GdkWindow *parent)
{
  GdkWindowObject *private = GDK_WINDOW_OBJECT (window);
  GdkWindowObject *root = GDK_WINDOW_OBJECT (gdk_parent_root);
  int i;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  g_return_if_fail (parent != NULL);
  g_return_if_fail (GDK_IS_WINDOW (parent));
  
  g_return_if_fail ((GdkWindow *)private->parent == gdk_parent_root);
  g_return_if_fail ((GdkWindow *)GDK_WINDOW_OBJECT (parent)->parent == gdk_parent_root);
  
  root->children = g_list_remove (root->children, window);

  i = g_list_index (root->children, parent);
  if (i<0)
    root->children = g_list_prepend (root->children, window);
  else 
    root->children = g_list_insert (root->children, window, i);
}

void
gdk_window_set_background (GdkWindow *window,
			   GdkColor  *color)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private->bg_color = *color;

  if (private->bg_pixmap &&
      private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
      private->bg_pixmap != GDK_NO_BG)
    {
      gdk_pixmap_unref (private->bg_pixmap);
      private->bg_pixmap = NULL;
    }
}

void
gdk_window_set_back_pixmap (GdkWindow *window,
			    GdkPixmap *pixmap,
			    gboolean   parent_relative)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkPixmap *old_pixmap;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (pixmap == NULL || !parent_relative);

  old_pixmap = private->bg_pixmap;

  if (parent_relative)
    {
      private->bg_pixmap = GDK_PARENT_RELATIVE_BG;
    }
  else
    {
      if (pixmap)
	{
	  gdk_pixmap_ref (pixmap);
	  private->bg_pixmap = pixmap;
	}
      else
	{
	  private->bg_pixmap = GDK_NO_BG;
	}
    }

  if (old_pixmap &&
      old_pixmap != GDK_PARENT_RELATIVE_BG &&
      old_pixmap != GDK_NO_BG)
    gdk_pixmap_unref (old_pixmap);
}

void
gdk_window_set_cursor (GdkWindow *window,
		       GdkCursor *cursor)
{
  GdkCursor *old_cursor = GDK_WINDOW_IMPL_FBDATA(window)->cursor;
  GdkRectangle window_reg;

  GDK_WINDOW_IMPL_FBDATA (window)->cursor = cursor ? gdk_cursor_ref (cursor) : NULL;

  if (old_cursor)
    gdk_cursor_unref (old_cursor);

  window_reg.x = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_x;
  window_reg.y = GDK_DRAWABLE_IMPL_FBDATA (window)->llim_y;
  window_reg.width = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_x - window_reg.x;
  window_reg.height = GDK_DRAWABLE_IMPL_FBDATA (window)->lim_y - window_reg.y;
  if (gdk_fb_cursor_need_hide (&window_reg))
    gdk_fb_cursor_reset ();
}

void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));
  
  if (!window)
    window = gdk_parent_root;
  
  if (!private->destroyed)
    {

      if (x)
	*x = private->x;
      if (y)
	*y = private->y;
      if (width)
	*width = GDK_DRAWABLE_IMPL_FBDATA (window)->width;
      if (height)
	*height = GDK_DRAWABLE_IMPL_FBDATA (window)->height;
      if (depth)
	*depth = gdk_display->modeinfo.bits_per_pixel;
    }
}

gboolean
gdk_window_get_origin (GdkWindow *window,
		       gint      *x,
		       gint      *y)
{
  g_return_val_if_fail (window != NULL, 0);
  
  if (x)
    *x = GDK_DRAWABLE_IMPL_FBDATA (window)->abs_x;
  if (y)
    *y = GDK_DRAWABLE_IMPL_FBDATA (window)->abs_y;

  return TRUE;
}

gboolean
gdk_window_get_deskrelative_origin (GdkWindow *window,
				    gint      *x,
				    gint      *y)
{
  gint tx = 0;
  gint ty = 0;
  gboolean return_val;

  g_return_val_if_fail (window != NULL, 0);
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      tx = GDK_DRAWABLE_IMPL_FBDATA (window)->abs_x;
      ty = GDK_DRAWABLE_IMPL_FBDATA (window)->abs_y;

      return_val = TRUE;
    }
  else
    return_val = FALSE;
  
  if (x)
    *x = tx;
  if (y)
    *y = ty;

  return return_val;
}

void
gdk_window_get_root_origin (GdkWindow *window,
			    gint      *x,
			    gint      *y)
{
  gdk_window_get_deskrelative_origin (window, x, y);
}

static GdkRegion*
gdk_window_fb_get_visible_region (GdkDrawable *drawable)
{
  GdkDrawableFBData *priv = GDK_DRAWABLE_FBDATA (drawable);
  GdkRectangle result_rect;
  GdkRectangle screen_rect;

  result_rect.x = 0;
  result_rect.y = 0;
  result_rect.width = priv->width;
  result_rect.height = priv->height;

  screen_rect.x = -priv->abs_x;
  screen_rect.y = -priv->abs_y;
  screen_rect.width = gdk_display->modeinfo.xres;
  screen_rect.height = gdk_display->modeinfo.yres;
  
  gdk_rectangle_intersect (&result_rect, &screen_rect, &result_rect);
  
  return gdk_region_rectangle (&result_rect);
}

GdkWindow*
gdk_window_get_pointer (GdkWindow       *window,
			gint            *x,
			gint            *y,
			GdkModifierType *mask)
{
  GdkWindow *return_val;
  int winx = 0;
  int winy = 0;
  int x_int, y_int;
  GdkModifierType my_mask;

  g_return_val_if_fail (window == NULL || GDK_IS_WINDOW (window), NULL);
  
  if (!window)
    window = gdk_parent_root;
  
  gdk_window_get_root_origin (window, &x_int, &y_int);
  gdk_fb_mouse_get_info (&winx, &winy, &my_mask);

  winx -= x_int;
  winy -= y_int;

  if (x)
    *x = winx;
  if (y)
    *y = winy;
  if (mask)
    *mask = my_mask;
  
  return_val = NULL;
  
  if ((winx >= 0) && (winx < GDK_DRAWABLE_IMPL_FBDATA (window)->width) &&
      (winy >= 0) && (winy < GDK_DRAWABLE_IMPL_FBDATA (window)->height))
    {
      GdkWindowObject *private;
      GdkWindowObject *sub;
      int subx = winx, suby = winy;

      for (private = sub = (GdkWindowObject *)window; sub; private = sub)
	{
	  GList *ltmp;

	  for (ltmp = private->children; ltmp; ltmp = ltmp->next)
	    {
	      sub = ltmp->data;

	      if (!sub->mapped)
		continue;

	      if (subx >= sub->x &&
		  (subx < (GDK_DRAWABLE_IMPL_FBDATA (sub)->width + sub->x)) &&
		  (suby >= sub->y) &&
		  (suby < (GDK_DRAWABLE_IMPL_FBDATA (sub)->height + sub->y)))
		{
		  subx -= sub->x;
		  suby -= sub->y;
		  break;
		}
	    }

	  if (!ltmp)
	    {
	      sub = NULL;
	      break;
	    }
	}

      return_val = (GdkWindow *)private;
    }

  if (!return_val)
    return_val = gdk_parent_root;

  return return_val;
}

GdkWindow*
gdk_window_at_pointer (gint *win_x,
		       gint *win_y)
{
  gint rx, ry;
  GdkWindow *retval = gdk_window_get_pointer (NULL, win_x, win_y, NULL);

  if (retval)
    {
      gdk_window_get_origin (retval, &ry, &rx);
      if (win_x)
	(*win_x) -= rx;
      if (win_y)
	(*win_y) -= ry;
    }

  return retval;
}

GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_WINDOW_DESTROYED (window))
    return 0;
  else
    return GDK_WINDOW_IMPL_FBDATA (window)->event_mask;
}

void          
gdk_window_set_events (GdkWindow       *window,
		       GdkEventMask     event_mask)
{
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_WINDOW_DESTROYED (window))
    GDK_WINDOW_IMPL_FBDATA (window)->event_mask = event_mask;
}

void
gdk_window_add_colormap_windows (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  /* N/A */
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
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
}

void
gdk_window_set_override_redirect (GdkWindow *window,
				  gboolean override_redirect)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* N/A */
}

void          
gdk_window_set_icon (GdkWindow *window, 
		     GdkWindow *icon_window,
		     GdkPixmap *pixmap,
		     GdkBitmap *mask)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* N/A */
}

void          
gdk_window_set_icon_name (GdkWindow *window, 
			  const gchar *    name)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* N/A */
}

void          
gdk_window_set_group (GdkWindow *window, 
		      GdkWindow *leader)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (leader != NULL);
  g_return_if_fail (GDK_IS_WINDOW (leader));

  /* N/A */
}

void
gdk_fb_window_set_child_handler (GdkWindow             *window,
				 GdkWindowChildChanged changed,
				 GdkWindowChildGetPos  get_pos,
				 gpointer              user_data)
{
  GdkWindowChildHandlerData *data;
    
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  data = g_new (GdkWindowChildHandlerData, 1);
  data->changed = changed;
  data->get_pos = get_pos;
  data->user_data = user_data;

  g_object_set_data_full (G_OBJECT (window), "gdk-window-child-handler",
			  data, (GDestroyNotify) g_free);
}

void
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  GdkWMDecoration *dec;
    
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  dec = g_new (GdkWMDecoration, 1);
  *dec = decorations;

  g_object_set_data_full (G_OBJECT (window), "gdk-window-decorations",
			  dec, (GDestroyNotify) g_free);
}

gboolean
gdk_window_get_decorations(GdkWindow *window,
			   GdkWMDecoration *decorations)
{
  GdkWMDecoration *dec;
    
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);


  dec = g_object_get_data (G_OBJECT (window), "gdk-window-decorations");
  if (dec)
    {
      *decorations = *dec;
      return TRUE;
    }
  return FALSE;
}

void
gdk_window_set_functions (GdkWindow    *window,
			  GdkWMFunction functions)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* N/A */
}

void
gdk_window_set_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
}

void
gdk_window_merge_child_shapes (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
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
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  
  return TRUE;
}

void
_gdk_windowing_window_get_offsets      (GdkWindow  *window,
					gint       *x_offset,
					gint       *y_offset)
{
  *x_offset = *y_offset = 0;
}

gboolean
_gdk_windowing_window_queue_antiexpose (GdkWindow  *window,
					GdkRegion  *area)
{
  return FALSE;
}
