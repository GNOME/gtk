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

#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdk.h"		/* For gdk_rectangle_union() */
#include "gdkpixmap.h"

#ifndef USE_BACKING_STORE
#ifndef GDK_WINDOWING_WIN32
#define USE_BACKING_STORE	/* Doesn't work yet on Win32 */
#endif
#endif

typedef struct _GdkWindowPaint GdkWindowPaint;

struct _GdkWindowPaint
{
  GdkRegion *region;
  GdkPixmap *pixmap;
  gint x_offset;
  gint y_offset;
};

static void   gdk_window_draw_destroy   (GdkDrawable     *drawable);
static GdkGC *gdk_window_draw_create_gc (GdkDrawable     *drawable,
					 GdkGCValues     *values,
					 GdkGCValuesMask  mask);
static void   gdk_window_draw_rectangle (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gint             filled,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height);
static void   gdk_window_draw_arc       (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gint             filled,
					 gint             x,
					 gint             y,
					 gint             width,
					 gint             height,
					 gint             angle1,
					 gint             angle2);
static void   gdk_window_draw_polygon   (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 gint             filled,
					 GdkPoint        *points,
					 gint             npoints);
static void   gdk_window_draw_text      (GdkDrawable     *drawable,
					 GdkFont         *font,
					 GdkGC           *gc,
					 gint             x,
					 gint             y,
					 const gchar     *text,
					 gint             text_length);
static void   gdk_window_draw_text_wc   (GdkDrawable     *drawable,
					 GdkFont         *font,
					 GdkGC           *gc,
					 gint             x,
					 gint             y,
					 const GdkWChar  *text,
					 gint             text_length);
static void   gdk_window_draw_drawable  (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPixmap       *src,
					 gint             xsrc,
					 gint             ysrc,
					 gint             xdest,
					 gint             ydest,
					 gint             width,
					 gint             height);
static void   gdk_window_draw_points    (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPoint        *points,
					 gint             npoints);
static void   gdk_window_draw_segments  (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkSegment      *segs,
					 gint             nsegs);
static void   gdk_window_draw_lines     (GdkDrawable     *drawable,
					 GdkGC           *gc,
					 GdkPoint        *points,
					 gint             npoints);


/* All drawing operations on windows are forwarded through the following
 * class to enable the automatic-backing-store feature.
 */
GdkDrawableClass _gdk_window_class = {
  gdk_window_draw_destroy,
  gdk_window_draw_create_gc,
  gdk_window_draw_rectangle,
  gdk_window_draw_arc,
  gdk_window_draw_polygon,
  gdk_window_draw_text,
  gdk_window_draw_text_wc,
  gdk_window_draw_drawable,
  gdk_window_draw_points,
  gdk_window_draw_segments,
  gdk_window_draw_lines
};

GdkWindow *
_gdk_window_alloc (void)
{
  GdkWindowPrivate *private = g_new (GdkWindowPrivate, 1);
  GdkWindow *window = (GdkWindow*) private;
  
  window->user_data = NULL;

  private->drawable.ref_count = 1;
  private->drawable.destroyed = FALSE;
  private->drawable.klass = NULL;
  private->drawable.klass_data = NULL;
  private->drawable.window_type = GDK_WINDOW_CHILD;

  private->drawable.width = 1;
  private->drawable.height = 1;

  private->drawable.colormap = NULL;

  private->parent = NULL;
  private->x = 0;
  private->y = 0;
  private->resize_count = 0;

  private->mapped = FALSE;
  private->guffaw_gravity = FALSE;
  private->extension_events = FALSE;
  
  private->filters = NULL;
  private->children = NULL;

  private->bg_color.pixel = 0;
  private->bg_color.red = 0;
  private->bg_color.green = 0;
  private->bg_color.blue = 0;

  private->bg_pixmap = NULL;

  private->paint_stack = NULL;

  private->update_area = NULL;
  private->update_freeze_count = 0;
  
  return window;
}

void
gdk_window_set_user_data (GdkWindow *window,
			  gpointer   user_data)
{
  g_return_if_fail (window != NULL);
  
  window->user_data = user_data;
}

void
gdk_window_get_user_data (GdkWindow *window,
			  gpointer  *data)
{
  g_return_if_fail (window != NULL);
  
  *data = window->user_data;
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

GList *
gdk_window_get_toplevels (void)
{
  GList *new_list = NULL;
  GList *tmp_list;
  
  tmp_list = ((GdkWindowPrivate *)gdk_parent_root)->children;
  while (tmp_list)
    {
      new_list = g_list_prepend (new_list, tmp_list->data);
      tmp_list = tmp_list->next;
    }
  
  return new_list;
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
	 (private != (GdkWindowPrivate *)gdk_parent_root) &&
	 (private->drawable.window_type != GDK_WINDOW_FOREIGN))
    {
      if (!private->mapped)
	return FALSE;
      
      private = (GdkWindowPrivate *)private->parent;
    }
  
  return TRUE;
}

void
gdk_window_begin_paint_rect (GdkWindow    *window,
			     GdkRectangle *rectangle)
{
  GdkRegion *region;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  region = gdk_region_rectangle (rectangle);
  gdk_window_begin_paint_region (window, region);
  gdk_region_destroy (region);
}

static GdkGC *
gdk_window_get_bg_gc (GdkWindow *window, GdkWindowPaint *paint)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  guint gc_mask = 0;
  GdkGCValues gc_values;

  if (private->bg_pixmap == GDK_PARENT_RELATIVE_BG && private->parent)
    {
      GdkWindowPaint tmp_paint = *paint;
      tmp_paint.x_offset += private->x;
      tmp_paint.y_offset += private->y;
      
      return gdk_window_get_bg_gc (private->parent, &tmp_paint);
    }
  else if (private->bg_pixmap && private->bg_pixmap != GDK_PARENT_RELATIVE_BG && private->bg_pixmap != GDK_NO_BG)
    {
      gc_values.fill = GDK_TILED;
      gc_values.tile = private->bg_pixmap;
      gc_values.ts_x_origin = - paint->x_offset;
      gc_values.ts_y_origin = - paint->y_offset;
      
      gc_mask = GDK_GC_FILL | GDK_GC_TILE | GDK_GC_TS_X_ORIGIN | GDK_GC_TS_Y_ORIGIN;
    }
  else
    {
      gc_values.foreground = private->bg_color;
      gc_mask = GDK_GC_FOREGROUND;
    }

  return gdk_gc_new_with_values (paint->pixmap, &gc_values, gc_mask);
}

static void
gdk_window_paint_init_bg (GdkWindow      *window,
			  GdkWindowPaint *paint,
			  GdkRegion      *init_region)
{
  GdkGC *tmp_gc;

  tmp_gc = gdk_window_get_bg_gc (window, paint);
  gdk_draw_rectangle (paint->pixmap, tmp_gc, TRUE, 0, 0, -1, -1);
  gdk_gc_unref (tmp_gc);
}
  
void	      
gdk_window_begin_paint_region (GdkWindow *window,
			       GdkRegion *region)
{
#ifdef USE_BACKING_STORE
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  GdkRectangle clip_box;
  GdkWindowPaint *paint;
  GdkRegion *init_region;
  GdkGC *tmp_gc;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  paint = g_new (GdkWindowPaint, 1);

  paint->region = gdk_region_copy (region);

  init_region = gdk_region_copy (region);
  gdk_region_get_clipbox (paint->region, &clip_box);

  if (private->paint_stack)
    {
      gint old_width, old_height;
      GdkWindowPaint *tmp_paint = private->paint_stack->data;
      GdkRectangle old_rect, new_rect;
      GSList *tmp_list;

      gdk_drawable_get_size (tmp_paint->pixmap, &old_width, &old_height);
      old_rect.x = tmp_paint->x_offset;
      old_rect.y = tmp_paint->y_offset;
      old_rect.width = old_width;
      old_rect.height = old_height;

      gdk_rectangle_union (&clip_box, &old_rect, &new_rect);

      if (new_rect.width > old_rect.width || new_rect.height > old_rect.height)
	{
	  paint->pixmap = gdk_pixmap_new (window, new_rect.width, new_rect.height, -1);
          tmp_gc = gdk_gc_new (paint->pixmap);
	  gdk_draw_drawable (paint->pixmap, tmp_gc, tmp_paint->pixmap,
			     0, 0, old_rect.width, old_rect.height,
			     old_rect.x - new_rect.x, old_rect.y - new_rect.y);
          gdk_gc_unref (tmp_gc);
	  gdk_drawable_unref (tmp_paint->pixmap);

	  paint->x_offset = new_rect.x;
	  paint->y_offset = new_rect.y;
	  
	  tmp_list = private->paint_stack;
	  while (tmp_list)
	    {
	      tmp_paint = private->paint_stack->data;
	      gdk_region_subtract (init_region, tmp_paint->region);

	      tmp_paint->pixmap = paint->pixmap;
	      tmp_paint->x_offset = paint->x_offset;
	      tmp_paint->y_offset = paint->x_offset;

	      tmp_list = tmp_list->next;
	    }
	}
      else
	{
	  paint->x_offset = tmp_paint->x_offset;
	  paint->y_offset = tmp_paint->y_offset;
	  paint->pixmap = tmp_paint->pixmap;

	  tmp_list = private->paint_stack;
	  while (tmp_list)
	    {
	      tmp_paint = private->paint_stack->data;
	      gdk_region_subtract (init_region, tmp_paint->region);
	      
	      tmp_list = tmp_list->next;
	    }
	}
    }
  else
    {
      paint->x_offset = clip_box.x;
      paint->y_offset = clip_box.y;
      paint->pixmap = gdk_pixmap_new (window, clip_box.width, clip_box.height, -1);
    }

  if (!gdk_region_empty (init_region))
    gdk_window_paint_init_bg (window, paint, init_region);
  gdk_region_destroy (init_region);
  
  private->paint_stack = g_slist_prepend (private->paint_stack, paint);
#endif /* USE_BACKING_STORE */
}

void
gdk_window_end_paint (GdkWindow *window)
{
#ifdef USE_BACKING_STORE
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  GdkWindowPaint *paint;
  GdkGC *tmp_gc;
  GdkRectangle clip_box;
  gint x_offset, y_offset;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (private->paint_stack != NULL);

  paint = private->paint_stack->data;
  private->paint_stack = g_slist_delete_link (private->paint_stack, private->paint_stack);

  gdk_region_get_clipbox (paint->region, &clip_box);

  tmp_gc = gdk_gc_new (window);

  _gdk_windowing_window_get_offsets (window, &x_offset, &y_offset);

  gdk_gc_set_clip_region (tmp_gc, paint->region);
  gdk_gc_set_clip_origin (tmp_gc, -x_offset, -y_offset);

  _gdk_windowing_window_class.draw_drawable (window, tmp_gc, paint->pixmap,
					     clip_box.x - paint->x_offset,
					     clip_box.y - paint->y_offset,
					     clip_box.x - x_offset, clip_box.y - y_offset,
					     clip_box.width, clip_box.height);
  gdk_gc_unref (tmp_gc);

  if (private->paint_stack)
    {
      GSList *tmp_list = private->paint_stack;
      while (tmp_list)
	{
	  GdkWindowPaint *tmp_paint = tmp_list->data;
	  gdk_region_subtract (tmp_paint->region, paint->region);
	  
	  tmp_list = tmp_list->next;
	}
    }
  else
    gdk_drawable_unref (paint->pixmap);

  gdk_region_destroy (paint->region);
  g_free (paint);
#endif /* USE_BACKING_STORE */
}

static void
gdk_window_get_offsets (GdkWindow *window,
			gint      *x_offset,
			gint      *y_offset)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      *x_offset = paint->x_offset;
      *y_offset = paint->y_offset;
    }
  else
    _gdk_windowing_window_get_offsets (window, x_offset, y_offset);
}

#define OFFSET_GC(gc)                                         \
    gint x_offset, y_offset; 				      \
    gint old_clip_x = ((GdkGCPrivate *)gc)->clip_x_origin;    \
    gint old_clip_y = ((GdkGCPrivate *)gc)->clip_y_origin;    \
    gint old_ts_x = ((GdkGCPrivate *)gc)->ts_x_origin;        \
    gint old_ts_y = ((GdkGCPrivate *)gc)->ts_y_origin;        \
    gdk_window_get_offsets (drawable, &x_offset, &y_offset);  \
    if (x_offset != 0 || y_offset != 0)             	      \
      {                                                       \
        gdk_gc_set_clip_origin (gc, old_clip_x - x_offset,    \
	  	                old_clip_y - y_offset);       \
        gdk_gc_set_ts_origin (gc, old_ts_x - x_offset,        \
	  	              old_ts_y - y_offset);           \
      }

#define RESTORE_GC(gc)                                      \
    if (x_offset != 0 || y_offset != 0)       		    \
     {                                                      \
       gdk_gc_set_clip_origin (gc, old_clip_x, old_clip_y); \
       gdk_gc_set_ts_origin (gc, old_ts_x, old_ts_y);       \
     }

static void
gdk_window_draw_destroy   (GdkDrawable     *drawable)
{
  _gdk_windowing_window_class.destroy (drawable);
}

static GdkGC *
gdk_window_draw_create_gc (GdkDrawable     *drawable,
			   GdkGCValues     *values,
			   GdkGCValuesMask  mask)
{
  return _gdk_windowing_window_class.create_gc (drawable, values, mask);
}

static void
gdk_window_draw_rectangle (GdkDrawable *drawable,
			   GdkGC       *gc,
			   gint         filled,
			   gint         x,
			   gint         y,
			   gint         width,
			   gint         height)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)drawable;
  OFFSET_GC (gc);
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_rectangle (paint->pixmap, gc, filled,
			  x - x_offset, y - y_offset, width, height);
    }
  else
    _gdk_windowing_window_class.draw_rectangle (drawable, gc, filled,
						x - x_offset, y - y_offset, width, height);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_arc (GdkDrawable *drawable,
		     GdkGC       *gc,
		     gint         filled,
		     gint         x,
		     gint         y,
		     gint         width,
		     gint         height,
		     gint         angle1,
		     gint         angle2)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)drawable;
  OFFSET_GC (gc);

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_arc (paint->pixmap, gc, filled,
		    x - x_offset, y_offset,
		    width, height, angle1, angle2);
    }
  else
    _gdk_windowing_window_class.draw_arc (drawable, gc, filled,
					  x - x_offset, y - y_offset,
					  width, height, angle1, angle2);
  RESTORE_GC (gc);
}

static void
gdk_window_draw_polygon (GdkDrawable *drawable,
			 GdkGC       *gc,
			 gint         filled,
			 GdkPoint    *points,
			 gint         npoints)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)drawable;
  GdkPoint *new_points;
  
  OFFSET_GC (gc);

  if (x_offset != 0 || y_offset != 0)
    {
      int i;
      
      new_points = g_new (GdkPoint, npoints);
      for (i=0; i<npoints; i++)
	{
	  new_points[i].x = points[i].x - x_offset;
	  new_points[i].y = points[i].y - y_offset;
	}
    }
  else
    new_points = points;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_polygon (paint->pixmap, gc, filled, new_points, npoints);

    }
  else
    _gdk_windowing_window_class.draw_polygon (drawable, gc, filled, new_points, npoints);

  if (new_points != points)
    g_free (new_points);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_text (GdkDrawable *drawable,
		      GdkFont     *font,
		      GdkGC       *gc,
		      gint         x,
		      gint         y,
		      const gchar *text,
		      gint         text_length)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)drawable;
  OFFSET_GC (gc);

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_text (paint->pixmap, font, gc, 
		     x - x_offset, y - y_offset, text, text_length);

    }
  else
    _gdk_windowing_window_class.draw_text (drawable, font, gc,
					   x - x_offset, y - y_offset, text, text_length);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_text_wc (GdkDrawable    *drawable,
			 GdkFont        *font,
			 GdkGC          *gc,
			 gint            x,
			 gint            y,
			 const GdkWChar *text,
			 gint            text_length)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)drawable;
  OFFSET_GC (gc);

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_text_wc (paint->pixmap, font, gc, 
			x - x_offset, y - y_offset, text, text_length);
    }
  else
    _gdk_windowing_window_class.draw_text_wc (drawable, font, gc,
					      x - x_offset, y - y_offset, text, text_length);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_drawable (GdkDrawable *drawable,
			  GdkGC       *gc,
			  GdkPixmap   *src,
			  gint         xsrc,
			  gint         ysrc,
			  gint         xdest,
			  gint         ydest,
			  gint         width,
			  gint         height)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)drawable;
  OFFSET_GC (gc);

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_drawable (paint->pixmap, gc, src, xsrc, ysrc,
			 xdest - x_offset, ydest - y_offset, width, height);

    }
  else
    _gdk_windowing_window_class.draw_drawable (drawable, gc, src, xsrc, ysrc,
					       xdest - x_offset, ydest - y_offset,
					       width, height);
  RESTORE_GC (gc);
}

static void
gdk_window_draw_points (GdkDrawable *drawable,
			GdkGC       *gc,
			GdkPoint    *points,
			gint         npoints)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)drawable;
  GdkPoint *new_points;
  
  OFFSET_GC (gc);

  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_points = g_new (GdkPoint, npoints);
      for (i=0; i<npoints; i++)
	{
	  new_points[i].x = points[i].x - x_offset;
	  new_points[i].y = points[i].y - y_offset;
	}
    }
  else
    new_points = points;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_points (paint->pixmap, gc, new_points, npoints);
    }
  else
    _gdk_windowing_window_class.draw_points (drawable, gc, points, npoints);

  if (new_points != points)
    g_free (new_points);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_segments (GdkDrawable *drawable,
			  GdkGC       *gc,
			  GdkSegment  *segs,
			  gint         nsegs)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)drawable;
  GdkSegment *new_segs;

  OFFSET_GC (gc);

  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_segs = g_new (GdkSegment, nsegs);
      for (i=0; i<nsegs; i++)
	{
	  new_segs[i].x1 = segs[i].x1 - x_offset;
	  new_segs[i].y1 = segs[i].y1 - y_offset;
	  new_segs[i].x2 = segs[i].x2 - x_offset;
	  new_segs[i].y2 = segs[i].y2 - y_offset;
	}
    }
  else
    new_segs = segs;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_segments (paint->pixmap, gc, new_segs, nsegs);
    }
  else
    _gdk_windowing_window_class.draw_segments (drawable, gc, new_segs, nsegs);

  if (new_segs != segs)
    g_free (new_segs);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_lines (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)drawable;
  GdkPoint *new_points;

  OFFSET_GC (gc);

  if (x_offset != 0 || y_offset != 0)
    {
      gint i;

      new_points = g_new (GdkPoint, npoints);
      for (i=0; i<npoints; i++)
	{
	  new_points[i].x = points[i].x - x_offset;
	  new_points[i].y = points[i].y - y_offset;
	}
    }
  else
    new_points = points;

  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_lines (paint->pixmap, gc, new_points, npoints);
    }
  else
    _gdk_windowing_window_class.draw_lines (drawable, gc, new_points, npoints);

  if (new_points != points)
    g_free (new_points);

  RESTORE_GC (gc);
}

/* Fixme - this is just like gdk_window_paint_init_bg */
static void
gdk_window_clear_backing_rect (GdkWindow *window,
			       gint       x,
			       gint       y,
			       gint       width,
			       gint       height)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  GdkWindowPaint *paint = private->paint_stack->data;
  GdkGC *tmp_gc;

  tmp_gc = gdk_window_get_bg_gc (window, paint);
  gdk_draw_rectangle (paint->pixmap, tmp_gc, TRUE,
		      x - paint->x_offset, y - paint->y_offset, width, height);
  gdk_gc_unref (tmp_gc);
}

void
gdk_window_clear_area (GdkWindow *window,
		       gint       x,
		       gint       y,
		       gint       width,
		       gint       height)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (private->paint_stack)
    gdk_window_clear_backing_rect (window, x, y, width, height);
  else
    _gdk_windowing_window_clear_area (window, x, y, width, height);
}

void
gdk_window_clear_area_e (GdkWindow *window,
		         gint       x,
		         gint       y,
		         gint       width,
		         gint       height)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (private->paint_stack)
    gdk_window_clear_backing_rect (window, x, y, width, height);

  _gdk_windowing_window_clear_area_e (window, x, y, width, height);
}

void
_gdk_window_draw_image (GdkDrawable *drawable,
			GdkGC       *gc,
			GdkImage    *image,
			gint         xsrc,
			gint         ysrc,
			gint         xdest,
			gint         ydest,
			gint         width,
			gint         height)
{
  GdkImagePrivate *image_private = (GdkImagePrivate*) image;
  GdkWindowPrivate *private = (GdkWindowPrivate *)drawable;

  OFFSET_GC (gc);
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      image_private->klass->image_put (image, paint->pixmap, gc, xsrc, ysrc,
				       xdest - x_offset, ydest - y_offset,
				       width, height);

    }
  else
    image_private->klass->image_put (image, drawable, gc, xsrc, ysrc,
				     xdest - x_offset, ydest - y_offset,
				     width, height);

  RESTORE_GC (gc);
}

/* Code for dirty-region queueing
 */

static GSList *update_windows = NULL;
static guint update_idle = 0;

#define GDK_PRIORITY_REDRAW     (G_PRIORITY_HIGH_IDLE + 20)

static void
gdk_window_process_updates_internal (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  gboolean save_region = FALSE;
  
  if (gdk_event_func)
    {
      GdkEvent event;
      GdkRectangle window_rect;

      window_rect.x = 0;
      window_rect.y = 0;
      window_rect.width = private->drawable.width;
      window_rect.height = private->drawable.height;

      save_region = _gdk_windowing_window_queue_antiexpose (window, private->update_area);
      
      event.expose.type = GDK_EXPOSE;
      event.expose.window = gdk_window_ref ((GdkWindow *)private);
      event.expose.count = 0;
      
      gdk_region_get_clipbox (private->update_area, &event.expose.area);
      if (gdk_rectangle_intersect (&event.expose.area, &window_rect, &event.expose.area))
	{
	  (*gdk_event_func) (&event, gdk_event_data);
	}
    }
  
  if (!save_region)
    gdk_region_destroy (private->update_area);
  private->update_area = NULL;
}

void
gdk_window_process_all_updates (void)
{
  GSList *old_update_windows = update_windows;
  GSList *tmp_list = update_windows;

  if (update_idle)
    g_source_remove (update_idle);
  
  update_windows = NULL;
  update_idle = 0;

  while (tmp_list)
    {
      gdk_window_process_updates_internal (tmp_list->data);
      tmp_list = tmp_list->next;
    }

  g_slist_free (old_update_windows);

  gdk_flush();
}

static gboolean
gdk_window_update_idle (gpointer data)
{
  gdk_window_process_all_updates ();
  
  return FALSE;
}

void
gdk_window_process_updates (GdkWindow *window,
			    gboolean   update_children)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (private->update_area)
    {
      gdk_window_process_updates_internal (window);
      update_windows = g_slist_remove (update_windows, window);
    }

  if (update_children)
    {
      GList *tmp_list = private->children;
      while (tmp_list)
	{
	  gdk_window_process_updates (tmp_list->data, TRUE);
	  tmp_list = tmp_list->next;
	}
    }
}

void
gdk_window_invalidate_rect   (GdkWindow    *window,
			      GdkRectangle *rect,
			      gboolean      invalidate_children)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (private->update_area)
    {
      gdk_region_union_with_rect (private->update_area, rect);
    }
  else
    {
      update_windows = g_slist_prepend (update_windows, window);
      private->update_area = gdk_region_rectangle (rect);

      if (!private->update_freeze_count && !update_idle)
	update_idle = g_idle_add_full (GDK_PRIORITY_REDRAW,
				       gdk_window_update_idle, NULL, NULL);
    }


  if (invalidate_children)
    {
      GList *tmp_list;
      GdkRectangle child_rect, new_rect;

      tmp_list = private->children;
      while (tmp_list)
	{
	  GdkWindowPrivate *child = tmp_list->data;
	  tmp_list = tmp_list->next;

	  /* FIXME: this is a HACK to figure out if the child is
	   *        input-only.
	   */
	  if (child->drawable.colormap)
	    {
	      child_rect.x = child->x;
	      child_rect.y = child->y;
	      child_rect.width = child->drawable.width;
	      child_rect.height = child->drawable.height;
	      
	      if (gdk_rectangle_intersect (rect, &child_rect, &new_rect))
		{
		  new_rect.x -= child_rect.x;
		  new_rect.y -= child_rect.y;
		  
		  gdk_window_invalidate_rect ((GdkWindow *)child, &new_rect, TRUE);
		}
	    }
	}
    }
}

void
gdk_window_invalidate_region (GdkWindow *window,
			      GdkRegion *region,
			      gboolean   invalidate_children)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (private->input_only)
    return;
  
  if (private->update_area)
    {
      gdk_region_union (private->update_area, region);
    }
  else
    {
      update_windows = g_slist_prepend (update_windows, window);
      private->update_area = gdk_region_copy (region);

      if (!private->update_freeze_count && !update_idle)
	update_idle = g_idle_add_full (GDK_PRIORITY_REDRAW,
				       gdk_window_update_idle, NULL, NULL);
    }

  if (invalidate_children)
    {
      GList *tmp_list;
      GdkRectangle child_rect;
      GdkRegion *child_region;

      tmp_list = private->children;
      while (tmp_list)
	{
	  GdkWindowPrivate *child = tmp_list->data;
	  tmp_list = tmp_list->next;

	  if (child->input_only)
	    {
	      child_rect.x = child->x;
	      child_rect.y = child->y;
	      child_rect.width = child->drawable.width;
	      child_rect.height = child->drawable.height;
	      
	      child_region = gdk_region_rectangle (&child_rect);
	      gdk_region_intersect (child_region, region);
	      
	      if (!gdk_region_empty (child_region))
		{
		  gdk_region_offset (child_region, - child_rect.x, - child_rect.y);
		  gdk_window_invalidate_region ((GdkWindow *)child, child_region, TRUE);
		}
	      
	      gdk_region_destroy (child_region);
	    }
	}
    }
}

GdkRegion *
gdk_window_get_update_area (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;
  GdkRegion *tmp_region;

  g_return_val_if_fail (window != NULL, NULL);
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (private->update_area)
    {
      tmp_region = private->update_area;
      private->update_area = NULL;

      update_windows = g_slist_remove (update_windows, window);
      
      return tmp_region;
    }
  else
    return NULL;
}

void
gdk_window_freeze_updates (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  private->update_freeze_count++;
}

void
gdk_window_thaw_updates (GdkWindow *window)
{
  GdkWindowPrivate *private = (GdkWindowPrivate *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (private->update_freeze_count > 0);

  private->update_freeze_count--;
  if (!private->update_freeze_count && private->update_area && !update_idle)
    update_idle = g_idle_add_full (GDK_PRIORITY_REDRAW,
				   gdk_window_update_idle, NULL, NULL);
}

