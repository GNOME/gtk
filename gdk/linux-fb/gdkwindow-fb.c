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

#include "gdk.h"
#include "config.h"

#include "gdkwindow.h"
#include "gdkinputprivate.h"
#include "gdkprivate-fb.h"
#include "gdkinternals.h"

#include <limits.h>

GdkDrawableClass _gdk_windowing_window_class;

static void recompute_drawable(GdkDrawable *drawable);

static void
g_free_2nd(gpointer a, gpointer b, gpointer data)
{
  g_free(b);
}

static void
gdk_fb_window_destroy (GdkDrawable *drawable)
{
  GdkWindowFBData *fbd = GDK_WINDOW_FBDATA(drawable);

  if (!GDK_DRAWABLE_DESTROYED (drawable))
    g_warning ("losing last reference to undestroyed window\n");

  if(GDK_WINDOW_P(drawable)->mapped)
    gdk_window_hide(drawable);

  if(fbd->cursor)
    gdk_cursor_unref(fbd->cursor);

  if(fbd->properties)
    {
      g_hash_table_foreach(fbd->properties, g_free_2nd, NULL);
      g_hash_table_destroy(fbd->properties);
    }

  g_free (GDK_DRAWABLE_FBDATA (drawable));
}

static GdkWindow *
gdk_fb_window_alloc (void)
{
  GdkWindow *window;
  GdkWindowPrivate *private;
  
  static gboolean initialized = FALSE;

  if (!initialized)
    {
      initialized = TRUE;

      _gdk_windowing_window_class = _gdk_fb_drawable_class;
      _gdk_windowing_window_class.destroy = gdk_fb_window_destroy;
    }

  window = _gdk_window_alloc ();
  private = (GdkWindowPrivate *)window;

  private->drawable.klass = &_gdk_window_class;
  private->drawable.klass_data = g_new0 (GdkWindowFBData, 1);
  private->drawable.depth = gdk_display->modeinfo.bits_per_pixel;
  private->drawable.colormap = gdk_colormap_get_system ();
  GDK_WINDOW_FBDATA(private)->event_mask = GDK_STRUCTURE_MASK;

  return window;
}

#include "/usr/include/X11/bitmaps/left_ptr"
#include "/usr/include/X11/bitmaps/left_ptrmsk"

void
gdk_window_init (void)
{
  GdkBitmap *ptr, *mask;
  GdkCursor *cursor;
  GdkWindowPrivate *private;

  gdk_parent_root = gdk_fb_window_alloc ();
  private = (GdkWindowPrivate *)gdk_parent_root;

  private->drawable.window_type = GDK_WINDOW_ROOT;
  private->drawable.width = gdk_screen_width ();
  private->drawable.height = gdk_screen_height ();
  private->mapped = TRUE;
  private->x = 0;
  private->y = 0;

  GDK_DRAWABLE_FBDATA(private)->mem = gdk_display->fbmem;
#if 0
  GDK_DRAWABLE_FBDATA(private)->rowstride = gdk_display->modeinfo.xres * (gdk_display->modeinfo.bits_per_pixel >> 3);
#else
  GDK_DRAWABLE_FBDATA(private)->rowstride = gdk_display->sinfo.line_length;
#endif
  GDK_DRAWABLE_FBDATA(private)->lim_x = gdk_display->modeinfo.xres;
  GDK_DRAWABLE_FBDATA(private)->lim_y = gdk_display->modeinfo.yres;
  GDK_WINDOW_FBDATA(private)->event_mask = GDK_EXPOSURE_MASK;

  gdk_fb_drawable_clear(gdk_parent_root);

  ptr = gdk_bitmap_create_from_data(gdk_parent_root, left_ptr_bits, left_ptr_width, left_ptr_height);
  mask = gdk_bitmap_create_from_data(gdk_parent_root, left_ptrmsk_bits, left_ptrmsk_width, left_ptrmsk_height);
  cursor = gdk_cursor_new_from_pixmap(ptr, mask, NULL, NULL, left_ptr_x_hot, left_ptr_y_hot);

  gdk_window_set_cursor(gdk_parent_root, cursor);
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

  int x, y, depth;
  
  g_return_val_if_fail (attributes != NULL, NULL);
  
  if (!parent || attributes->window_type != GDK_WINDOW_CHILD)
    parent = gdk_parent_root;
  
  parent_private = (GdkWindowPrivate*) parent;
  if (GDK_DRAWABLE_DESTROYED (parent))
    return NULL;
  
  window = gdk_fb_window_alloc ();
  private = (GdkWindowPrivate *)window;

  private->parent = parent;

  if (attributes_mask & GDK_WA_X)
    x = attributes->x;
  else
    x = 0;

  if (attributes_mask & GDK_WA_Y)
    y = attributes->y;
  else
    y = 0;

  gdk_window_set_events(window, attributes->event_mask);

  if (attributes_mask & GDK_WA_VISUAL)
    visual = attributes->visual;

  private->x = x;
  private->y = y;
  private->drawable.width = (attributes->width > 1) ? (attributes->width) : (1);
  private->drawable.height = (attributes->height > 1) ? (attributes->height) : (1);
  private->drawable.window_type = attributes->window_type;
  GDK_DRAWABLE_FBDATA(private)->mem = gdk_display->fbmem;
  GDK_DRAWABLE_FBDATA(private)->rowstride = GDK_DRAWABLE_FBDATA(gdk_parent_root)->rowstride;
  gdk_window_move_resize (window, x, y,
			  private->drawable.width, private->drawable.height);

  if (attributes->wclass == GDK_INPUT_OUTPUT)
    {
      depth = visual->depth;

      private->input_only = FALSE;
      private->drawable.depth = depth;
      
      if ((attributes_mask & GDK_WA_COLORMAP)
	  && attributes->colormap)
	private->drawable.colormap = attributes->colormap;
      
      switch (private->drawable.window_type)
	{
	case GDK_WINDOW_TOPLEVEL:
	case GDK_WINDOW_CHILD:
	case GDK_WINDOW_DIALOG:
	case GDK_WINDOW_TEMP:
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
      private->input_only = TRUE;
      GDK_WINDOW_FBDATA(private)->level = 10000;
      private->drawable.colormap = NULL;
    }

  gdk_drawable_ref (window);
  
  if (private->drawable.colormap)
    gdk_colormap_ref (private->drawable.colormap);
  
  gdk_window_set_cursor (window, ((attributes_mask & GDK_WA_CURSOR) ?
				  (attributes->cursor) :
				  NULL));
  
  if (parent_private)
    parent_private->children = g_list_prepend (parent_private->children, window);
  if(parent_private && parent_private->children->next)
    GDK_WINDOW_FBDATA(private)->level = GDK_WINDOW_FBDATA(parent_private->children->next->data)->level + 1;
  
  return window;
}

/* Call this function when you want a window and all its children to
 * disappear.  When xdestroy is true, a request to destroy the XWindow
 * is sent out.  When it is false, it is assumed that the XWindow has
 * been or will be destroyed by destroying some ancestor of this
 * window.
 */
void
_gdk_windowing_window_destroy(GdkWindow *window,
			      gboolean   recursing,
			      gboolean   foreign_destroy)
{
  GdkWindowPrivate *private;
  GdkWindowPrivate *temp_private;
  GdkWindow *temp_window;
  GList *children;
  GList *tmp;
  gboolean our_destroy = !foreign_destroy;
  
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

	  if (private->bg_pixmap && private->bg_pixmap != GDK_PARENT_RELATIVE_BG)
	    {
	      gdk_pixmap_unref (private->bg_pixmap);
	      private->bg_pixmap = NULL;
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
		    _gdk_windowing_window_destroy (temp_window, !FALSE,
						   !our_destroy);
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
		  gdk_error_trap_push ();
		  gdk_window_hide (window);
		  gdk_window_reparent (window, NULL, 0, 0);
		  
		  gdk_flush ();
		  gdk_error_trap_pop ();
		}
	    }
	  
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

/* This function is called when the XWindow is really gone.  */

void
gdk_window_destroy_notify (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      if (GDK_DRAWABLE_TYPE(window) != GDK_WINDOW_FOREIGN)
	g_warning ("GdkWindow %#x unexpectedly destroyed", GPOINTER_TO_UINT(window));

      _gdk_windowing_window_destroy (window, TRUE, TRUE);
    }
  
  gdk_drawable_unref (window);
}

static gboolean all_parents_shown(GdkWindowPrivate *private)
{
  while(private->mapped)
    {
      if(private->parent)
	private = (GdkWindowPrivate *)private->parent;
      else
	return TRUE;
    }

  return FALSE;
}

static void
send_map_events(GdkWindowPrivate *private, gboolean is_map)
{
  GdkEvent *event;
  GList *l;
  GdkWindow *parent = private->parent;

  if(!private->mapped || GDK_DRAWABLE_DESTROYED(private))
    return;

  if(is_map)
    gdk_event_make((GdkWindow *)private, GDK_MAP, TRUE);

  if(private->input_only)
    return;

  if(!parent)
    parent = (GdkWindow *)private;

  if(((GDK_DRAWABLE_FBDATA(private)->abs_x > GDK_DRAWABLE_FBDATA(parent)->lim_x)
      || (GDK_DRAWABLE_FBDATA(private)->abs_y > GDK_DRAWABLE_FBDATA(parent)->lim_y)
      || (GDK_DRAWABLE_FBDATA(private)->lim_x < GDK_DRAWABLE_FBDATA(parent)->llim_x)
      || (GDK_DRAWABLE_FBDATA(private)->lim_y < GDK_DRAWABLE_FBDATA(parent)->llim_y)))
     return;

  if(is_map)
    gdk_window_clear((GdkWindow *)private);

  event = gdk_event_new();
  event->expose.type = GDK_EXPOSE;
  event->expose.window = gdk_window_ref((GdkWindow *)private);
  if(GDK_DRAWABLE_FBDATA(private)->abs_x > GDK_DRAWABLE_FBDATA(parent)->llim_x)
    event->expose.area.x = 0;
  else
    event->expose.area.x = GDK_DRAWABLE_FBDATA(parent)->llim_x - GDK_DRAWABLE_FBDATA(private)->abs_x;

  if(GDK_DRAWABLE_FBDATA(private)->abs_y > GDK_DRAWABLE_FBDATA(parent)->llim_y)
    event->expose.area.y = 0;
  else
    event->expose.area.y = GDK_DRAWABLE_FBDATA(parent)->llim_y - GDK_DRAWABLE_FBDATA(private)->abs_y;

  event->expose.area.width = MIN(private->drawable.width,
				 GDK_DRAWABLE_FBDATA(private)->lim_x - GDK_DRAWABLE_FBDATA(private)->abs_x);
  event->expose.area.height = MIN(private->drawable.height,
				  GDK_DRAWABLE_FBDATA(private)->lim_y - GDK_DRAWABLE_FBDATA(private)->abs_y);
  if(event->expose.area.width > 0
     && event->expose.area.height > 0)
    {
      gdk_event_queue_append(event);
      for(l = private->children; l; l = l->next)
	send_map_events(l->data, is_map);
    }
  else
    gdk_event_free(event);

}

void
gdk_fb_redraw_all(void)
{
  send_map_events((GdkWindowPrivate *)gdk_parent_root, FALSE);
}

static
void gdk_fb_window_visibility_crossing(GdkWindow *window, gboolean is_show)
{
  gint winx, winy;
  GdkModifierType my_mask;

  gdk_input_ps2_get_mouseinfo(&winx, &winy, &my_mask);

  if(winx >= GDK_DRAWABLE_FBDATA(window)->llim_x
     && winx < GDK_DRAWABLE_FBDATA(window)->lim_x
     && winy >= GDK_DRAWABLE_FBDATA(window)->llim_y
     && winy < GDK_DRAWABLE_FBDATA(window)->lim_y && 0)
    {
      GdkWindow *oldwin, *newwin, *curwin;
      GdkEvent *event;

      curwin = gdk_window_get_pointer(NULL, NULL, NULL, NULL);

      if(is_show)
	{
	  /* Window is about to be shown */
	  oldwin = curwin;
	  newwin = window;
	}
      else
	{
	  /* Window is about to be hidden */
	  oldwin = window;
	  newwin = curwin;
	}
      event = gdk_event_make(oldwin, GDK_LEAVE_NOTIFY, TRUE);
      if(event)
	{
	  guint x_int, y_int;
	  event->crossing.subwindow = gdk_window_ref(newwin);
	  gdk_window_get_root_origin(oldwin, &x_int, &y_int);
	  event->crossing.x = winx - x_int;
	  event->crossing.y = winy - y_int;
	  event->crossing.x_root = winx;
	  event->crossing.y_root = winy;
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  event->crossing.focus = FALSE;
	  event->crossing.state = my_mask;
	}

      event = gdk_event_make(newwin, GDK_ENTER_NOTIFY, TRUE);
      if(event)
	{
	  guint x_int, y_int;
	  event->crossing.subwindow = gdk_window_ref(oldwin);
	  gdk_window_get_root_origin(newwin, &x_int, &y_int);
	  event->crossing.x = winx - x_int;
	  event->crossing.y = winy - y_int;
	  event->crossing.x_root = winx;
	  event->crossing.y_root = winy;
	  event->crossing.mode = GDK_CROSSING_NORMAL;
	  event->crossing.detail = GDK_NOTIFY_UNKNOWN;
	  event->crossing.focus = FALSE;
	  event->crossing.state = my_mask;
	}
    }
}

void
gdk_window_show (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;

  if (!private->drawable.destroyed && !private->mapped)
    {
      private->mapped = TRUE;

      if(all_parents_shown((GdkWindowPrivate *)private->parent))
	{
	  recompute_drawable((GdkDrawable *)window);
	  send_map_events(private, TRUE);

	  private->mapped = FALSE; /* a hack, ayup, to make gdk_window_get_pointer get the other window */
	  gdk_fb_window_visibility_crossing(window, TRUE);
	  private->mapped = TRUE;
	}
    }
}

static gboolean
rects_overlap(GdkRectangle *r1, GdkRectangle *r2)
{
  if(r1->x > (r2->x + r2->width))
    return FALSE;
  if(r2->x > (r1->x + r1->width))
    return FALSE;
  if(r1->y > (r2->y + r2->height))
    return FALSE;
  if(r2->y > (r1->y + r1->height))
    return FALSE;
  
  return TRUE;
}

static void
gdk_window_on_hide(GdkWindow *window, GdkRectangle *in_rect, GdkWindow *ignore)
{
  GdkEvent *event;
  GdkRectangle this_rect, test_rect;

  if(!GDK_WINDOW_P(window)->mapped || window == ignore || GDK_WINDOW_P(window)->input_only)
    return;

  this_rect.x = MAX(GDK_DRAWABLE_FBDATA(window)->llim_x - GDK_DRAWABLE_FBDATA(window)->abs_x, 0);

  this_rect.y = MAX(GDK_DRAWABLE_FBDATA(window)->llim_y - GDK_DRAWABLE_FBDATA(window)->abs_y, 0);

  this_rect.width = MIN(GDK_DRAWABLE_P(window)->width,
			GDK_DRAWABLE_FBDATA(window)->lim_x - GDK_DRAWABLE_FBDATA(window)->abs_x);
  this_rect.height = MIN(GDK_DRAWABLE_P(window)->height,
			 GDK_DRAWABLE_FBDATA(window)->lim_y - GDK_DRAWABLE_FBDATA(window)->abs_y);

  test_rect = this_rect;
  test_rect.x += GDK_DRAWABLE_FBDATA(window)->abs_x;
  test_rect.y += GDK_DRAWABLE_FBDATA(window)->abs_y;
  if(this_rect.width > 0
     && this_rect.height > 0
     && rects_overlap(&test_rect, in_rect))
    {
      GList *l;

      event = gdk_event_make(window, GDK_EXPOSE, TRUE);
      if(event)
	event->expose.area = this_rect;

      for(l = GDK_WINDOW_P(window)->children; l; l = l->next)
	gdk_window_on_hide(l->data, in_rect, ignore);
    }
}

void
gdk_window_hide (GdkWindow *window)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowPrivate*) window;

  if (!private->drawable.destroyed && private->mapped)
    {
      GdkEvent *event;
      GdkRectangle r;

      event = gdk_event_make(window, GDK_UNMAP, TRUE);

      r.x = GDK_DRAWABLE_FBDATA(window)->llim_x;
      r.y = GDK_DRAWABLE_FBDATA(window)->llim_y;
      r.width = GDK_DRAWABLE_FBDATA(window)->lim_x - r.x;
      r.height = GDK_DRAWABLE_FBDATA(window)->lim_y - r.y;

      private->mapped = FALSE;
      gdk_window_on_hide(private->parent, &r, NULL);

      if(private->parent == gdk_parent_root)
	gdk_fb_drawable_clear((GdkDrawable *)gdk_parent_root);

      if(all_parents_shown((GdkWindowPrivate *)private->parent))
	gdk_fb_window_visibility_crossing(window, FALSE);
    }
}

void
gdk_window_withdraw (GdkWindow *window)
{
  gdk_window_hide(window);
}

void
gdk_window_move (GdkWindow *window,
		 gint       x,
		 gint       y)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_window_move_resize (window, x, y,
			  private->drawable.width, private->drawable.height);
}

void
gdk_window_resize (GdkWindow *window,
		   gint       width,
		   gint       height)
{
  GdkWindowPrivate *private;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  private = (GdkWindowPrivate*) window;
  
  if (width < 1)
    width = 1;
  if (height < 1)
    height = 1;
  gdk_window_move_resize(window, private->x, private->y, width, height);
}

static void
recompute_abs_positions(GdkDrawable *drawable, gint parent_x, gint parent_y,
			gint parent_llim_x, gint parent_llim_y,
			gint parent_lim_x, gint parent_lim_y)
{
  GList *l;

  if(GDK_IS_WINDOW(drawable))
    {
      GdkWindowPrivate *private = GDK_WINDOW_P(drawable);

      if(!private->mapped)
	return;

      GDK_DRAWABLE_FBDATA(private)->abs_x = parent_x + private->x;
      GDK_DRAWABLE_FBDATA(private)->abs_y = parent_y + private->y;
      GDK_DRAWABLE_FBDATA(private)->llim_x = MIN(MAX(parent_llim_x, GDK_DRAWABLE_FBDATA(private)->abs_x),
						 parent_lim_x);
      GDK_DRAWABLE_FBDATA(private)->llim_y = MIN(MAX(parent_llim_y, GDK_DRAWABLE_FBDATA(private)->abs_y),
						 parent_lim_y);
      GDK_DRAWABLE_FBDATA(private)->lim_x = MAX(MIN(parent_lim_x, GDK_DRAWABLE_FBDATA(private)->abs_x + GDK_DRAWABLE_P(private)->width),
						GDK_DRAWABLE_FBDATA(private)->llim_x);
      GDK_DRAWABLE_FBDATA(private)->lim_y = MAX(MIN(parent_lim_y,
						GDK_DRAWABLE_FBDATA(private)->abs_y + GDK_DRAWABLE_P(private)->height),
						GDK_DRAWABLE_FBDATA(private)->llim_y);

      g_assert(GDK_DRAWABLE_FBDATA(private)->llim_x <= GDK_DRAWABLE_FBDATA(private)->lim_x);
      g_assert(GDK_DRAWABLE_FBDATA(private)->llim_y <= GDK_DRAWABLE_FBDATA(private)->lim_y);

      for(l = private->children; l; l = l->next)
	recompute_abs_positions(l->data, GDK_DRAWABLE_FBDATA(private)->abs_x, GDK_DRAWABLE_FBDATA(private)->abs_y,
				GDK_DRAWABLE_FBDATA(private)->llim_x, GDK_DRAWABLE_FBDATA(private)->llim_y,
				GDK_DRAWABLE_FBDATA(private)->lim_x, GDK_DRAWABLE_FBDATA(private)->lim_y);
    }
  else
    {
      GDK_DRAWABLE_FBDATA(drawable)->abs_x = 0;
      GDK_DRAWABLE_FBDATA(drawable)->abs_y = 0;
      GDK_DRAWABLE_FBDATA(drawable)->llim_x = 0;
      GDK_DRAWABLE_FBDATA(drawable)->llim_y = 0;
      GDK_DRAWABLE_FBDATA(drawable)->lim_x = GDK_DRAWABLE_P(drawable)->width;
      GDK_DRAWABLE_FBDATA(drawable)->lim_y = GDK_DRAWABLE_P(drawable)->height;
    }
}

static void
recompute_drawable(GdkDrawable *drawable)
{
  if(GDK_IS_WINDOW(drawable))
    {
      GdkWindowPrivate *private = GDK_WINDOW_P(drawable);
      GdkWindow *parent;

      parent = private->parent;
      if(!parent)
	parent = gdk_parent_root;

      recompute_abs_positions(drawable, GDK_DRAWABLE_FBDATA(parent)->abs_x,
			      GDK_DRAWABLE_FBDATA(parent)->abs_y,
			      GDK_DRAWABLE_FBDATA(parent)->llim_x,
			      GDK_DRAWABLE_FBDATA(parent)->llim_y,
			      GDK_DRAWABLE_FBDATA(parent)->lim_x,
			      GDK_DRAWABLE_FBDATA(parent)->lim_y);
    }
  else
    recompute_abs_positions(drawable, 0, 0, 0, 0, INT_MAX, INT_MAX);
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
      private->x = x;
      private->y = y;
      GDK_DRAWABLE_P(private)->width = width;
      GDK_DRAWABLE_P(private)->height = height;

      if(private->mapped)
	{
	  GdkRectangle r;

	  r.x = GDK_DRAWABLE_FBDATA(window)->llim_x;
	  r.y = GDK_DRAWABLE_FBDATA(window)->llim_y;
	  r.width = GDK_DRAWABLE_FBDATA(window)->lim_x - r.x;
	  r.height = GDK_DRAWABLE_FBDATA(window)->lim_y - r.y;

	  recompute_drawable((GdkDrawable *)window);
	  send_map_events(private, FALSE);

	  gdk_window_on_hide(private->parent, &r, window);
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
    new_parent = gdk_parent_root;

  window_private = (GdkWindowPrivate*) window;
  old_parent_private = (GdkWindowPrivate*)window_private->parent;
  parent_private = (GdkWindowPrivate*) new_parent;
  
  g_assert(window_private->drawable.colormap);

  window_private->parent = new_parent;
  
  if (old_parent_private)
    old_parent_private->children = g_list_remove (old_parent_private->children, window);

  parent_private->children = g_list_prepend (parent_private->children, window);

  if(window_private->mapped)
    recompute_drawable((GdkDrawable *)window);
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

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if(GDK_WINDOW_P(window)->input_only)
    return;

  bgpm = GDK_WINDOW_P(window)->bg_pixmap;
  for(relto = window; bgpm == GDK_PARENT_RELATIVE_BG && relto; relto = GDK_WINDOW_P(relto)->parent)
    bgpm = GDK_WINDOW_P(relto)->bg_pixmap;

  if(bgpm && bgpm != GDK_NO_BG)
    {
      int curx, cury;
      int xtrans, ytrans;
      int xstep, ystep;

      return; /* Don't bother doing this - gtk+ will do it itself using GC tiles */

      xtrans = GDK_DRAWABLE_FBDATA(relto)->abs_x - GDK_DRAWABLE_FBDATA(window)->abs_x;
      ytrans = GDK_DRAWABLE_FBDATA(relto)->abs_y - GDK_DRAWABLE_FBDATA(window)->abs_y;

      for(cury = y - ytrans; cury < (y - ytrans + height); cury += ystep)
	{
	  int drawh = cury % GDK_DRAWABLE_P(bgpm)->height;
	  ystep = GDK_DRAWABLE_P(bgpm)->height - drawh;

	  for(curx = x - xtrans; curx < (x - xtrans + width); curx += xstep)
	    {
	      int draww = curx % GDK_DRAWABLE_P(bgpm)->width;
	      xstep = GDK_DRAWABLE_P(bgpm)->width - draww;

	      gdk_fb_draw_drawable_2(window, NULL, bgpm,
				     draww, drawh, curx + xtrans, cury + ytrans,
				     xstep, ystep, FALSE, TRUE);
	    }
	}
    }
  else if(!bgpm)
    gdk_fb_draw_rectangle(window, NULL, TRUE, x, y, width, height);
}

/* What's the diff? */
void
_gdk_windowing_window_clear_area_e (GdkWindow *window,
				    gint       x,
				    gint       y,
				    gint       width,
				    gint       height)
{
  _gdk_windowing_window_clear_area(window, x, y, width, height);
}

static gint
compare_window_levels(gconstpointer a, gconstpointer b)
{
  return (GDK_WINDOW_FBDATA(b)->level - GDK_WINDOW_FBDATA(a)->level);
}

/* Child list is sorted bottom-to-top */
static void
gdk_window_resort_children(GdkWindow *win)
{
  GdkWindowPrivate *private = GDK_WINDOW_P(win);

  private->children = g_list_sort(private->children, compare_window_levels);

  /* Now the fun part - redraw */
  if(GDK_WINDOW_P(win)->parent)
    send_map_events(private, FALSE);
}

void
gdk_window_raise (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  GDK_WINDOW_FBDATA(window)->level++;

  if(GDK_WINDOW_P(window)->parent)
    gdk_window_resort_children(GDK_WINDOW_P(window)->parent);
}

void
gdk_window_lower (GdkWindow *window)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  GDK_WINDOW_FBDATA(window)->level--;

  if(GDK_WINDOW_P(window)->parent)
    gdk_window_resort_children(GDK_WINDOW_P(window)->parent);
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
  GDK_WINDOW_FBDATA(window)->level = GDK_WINDOW_FBDATA(parent)->level + 1;
}

void
gdk_window_set_background (GdkWindow *window,
			   GdkColor  *color)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  
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
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
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
  GdkCursor *old_cursor = GDK_WINDOW_FBDATA(window)->cursor;

  GDK_WINDOW_FBDATA(window)->cursor = cursor?gdk_cursor_ref(cursor):NULL;

  if(old_cursor)
    gdk_cursor_unref(old_cursor);
}

void
gdk_window_get_geometry (GdkWindow *window,
			 gint      *x,
			 gint      *y,
			 gint      *width,
			 gint      *height,
			 gint      *depth)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  
  g_return_if_fail (window == NULL || GDK_IS_WINDOW (window));
  
  if (!window)
    window = gdk_parent_root;
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {

      if (x)
	*x = private->x;
      if (y)
	*y = private->y;
      if (width)
	*width = GDK_DRAWABLE_P(window)->width;
      if (height)
	*height = GDK_DRAWABLE_P(window)->height;
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
    *x = GDK_DRAWABLE_FBDATA(window)->abs_x;
  if (y)
    *y = GDK_DRAWABLE_FBDATA(window)->abs_y;

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
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    {
      tx = GDK_DRAWABLE_FBDATA(window)->abs_x;
      ty = GDK_DRAWABLE_FBDATA(window)->abs_y;

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
  gdk_window_get_deskrelative_origin(window, x, y);
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
  
  gdk_window_get_root_origin(window, &x_int, &y_int);
  gdk_input_ps2_get_mouseinfo(&winx, &winy, &my_mask);

  winx -= x_int;
  winy -= y_int;

  if (x)
    *x = winx;
  if (y)
    *y = winy;
  if (mask)
    *mask = my_mask;
  
  return_val = NULL;
  
  if((winx >= 0) && (winx < GDK_DRAWABLE_P(window)->width)
     && (winy >= 0) && (winy < GDK_DRAWABLE_P(window)->height))
    {
      GdkWindowPrivate *private;
      GdkWindowPrivate *sub;
      int subx = winx, suby = winy;

      for(private = sub = (GdkWindowPrivate *)window; sub; private = sub)
	{
	  GList *ltmp;

	  for(ltmp = private->children; ltmp; ltmp = ltmp->next)
	    {
	      sub = ltmp->data;

	      if(!sub->mapped)
		continue;

	      if(subx >= sub->x
		 && (subx < (GDK_DRAWABLE_P(sub)->width + sub->x))
		 && (suby >= sub->y)
		 && (suby < (GDK_DRAWABLE_P(sub)->height + sub->y)))
		{
		  subx -= sub->x;
		  suby -= sub->y;
		  break;
		}
	    }

	  if(!ltmp)
	    {
	      sub = NULL;
	      break;
	    }
	}

      return_val = (GdkWindow *)private;
    }

  if(!return_val)
    return_val = gdk_parent_root;

  return return_val;
}

GdkWindow*
gdk_window_at_pointer (gint *win_x,
		       gint *win_y)
{
  gint rx, ry;
  GdkWindow *retval = gdk_window_get_pointer(NULL, win_x, win_y, NULL);

  if(retval)
    {
      gdk_window_get_origin(retval, &ry, &rx);
      if(win_x)
	(*win_x) -= rx;
      if(win_y)
	(*win_y) -= ry;
    }

  return retval;
}

GList*
gdk_window_get_children (GdkWindow *window)
{
  GList *children;
  
  g_return_val_if_fail (window != NULL, NULL);
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_DRAWABLE_DESTROYED (window))
    return NULL;
  
  children = NULL;

  return ((GdkWindowPrivate *)window)->children; /* Need to copy this list? */
}

GdkEventMask  
gdk_window_get_events (GdkWindow *window)
{
  g_return_val_if_fail (window != NULL, 0);
  g_return_val_if_fail (GDK_IS_WINDOW (window), 0);

  if (GDK_DRAWABLE_DESTROYED (window))
    return 0;
  else
    return GDK_WINDOW_FBDATA(window)->event_mask;
}

void          
gdk_window_set_events (GdkWindow       *window,
		       GdkEventMask     event_mask)
{
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (!GDK_DRAWABLE_DESTROYED (window))
    GDK_WINDOW_FBDATA(window)->event_mask = event_mask;
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
gdk_window_set_decorations (GdkWindow      *window,
			    GdkWMDecoration decorations)
{
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  /* N/A */
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
