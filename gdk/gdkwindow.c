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
static GdkGC *gdk_window_create_gc      (GdkDrawable     *drawable,
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
static void   gdk_window_draw_glyphs    (GdkDrawable      *drawable,
					 GdkGC            *gc,
					 PangoFont        *font,
					 gint              x,
					 gint              y,
					 PangoGlyphString *glyphs);

static void   gdk_window_draw_image     (GdkDrawable     *drawable,
                                         GdkGC           *gc,
                                         GdkImage        *image,
                                         gint             xsrc,
                                         gint             ysrc,
                                         gint             xdest,
                                         gint             ydest,
                                         gint             width,
                                         gint             height);


static void   gdk_window_real_get_size  (GdkDrawable     *drawable,
                                         gint            *width,
                                         gint            *height);

static GdkVisual*   gdk_window_real_get_visual   (GdkDrawable *drawable);
static gint         gdk_window_real_get_depth    (GdkDrawable *drawable);
static void         gdk_window_real_set_colormap (GdkDrawable *drawable,
                                             GdkColormap *cmap);
static GdkColormap* gdk_window_real_get_colormap (GdkDrawable *drawable);
     
static void gdk_window_free_paint_stack (GdkWindow *window);

static void gdk_window_init       (GdkWindowObject      *window);
static void gdk_window_class_init (GdkWindowObjectClass *klass);
static void gdk_window_finalize   (GObject              *object);

static gpointer parent_class = NULL;

GType
gdk_window_object_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkWindowObjectClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_window_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkWindowObject),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_window_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE,
                                            "GdkWindow",
                                            &object_info);
    }
  
  return object_type;
}

static void
gdk_window_init (GdkWindowObject *window)
{
  /* 0-initialization is good for all other fields. */

  window->window_type = GDK_WINDOW_CHILD;

  window->impl = g_object_new (_gdk_window_impl_get_type (), NULL);
}

static void
gdk_window_class_init (GdkWindowObjectClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_window_finalize;

  drawable_class->create_gc = gdk_window_create_gc;
  drawable_class->draw_rectangle = gdk_window_draw_rectangle;
  drawable_class->draw_arc = gdk_window_draw_arc;
  drawable_class->draw_polygon = gdk_window_draw_polygon;
  drawable_class->draw_text = gdk_window_draw_text;
  drawable_class->draw_text_wc = gdk_window_draw_text_wc;
  drawable_class->draw_drawable = gdk_window_draw_drawable;
  drawable_class->draw_points = gdk_window_draw_points;
  drawable_class->draw_segments = gdk_window_draw_segments;
  drawable_class->draw_lines = gdk_window_draw_lines;
  drawable_class->draw_glyphs = gdk_window_draw_glyphs;
  drawable_class->draw_image = gdk_window_draw_image;
  drawable_class->get_depth = gdk_window_real_get_depth;
  drawable_class->get_size = gdk_window_real_get_size;
  drawable_class->set_colormap = gdk_window_real_set_colormap;
  drawable_class->get_colormap = gdk_window_real_get_colormap;
  drawable_class->get_visual = gdk_window_real_get_visual;
}

static void
gdk_window_finalize (GObject *object)
{
  GdkWindow *window = GDK_WINDOW (object);
  GdkWindowObject *obj = (GdkWindowObject *) object;
  
  if (!GDK_WINDOW_DESTROYED (window))
    {
      if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	{
	  g_warning ("losing last reference to undestroyed window\n");
	  _gdk_window_destroy (window, FALSE);
	}
      else
	/* We use TRUE here, to keep us from actually calling
	 * XDestroyWindow() on the window
	 */
	_gdk_window_destroy (window, TRUE);
    }

  g_object_unref (G_OBJECT (obj->impl));
  obj->impl = NULL;
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * _gdk_window_destroy_hierarchy:
 * @window: a #GdkWindow
 * @recursing: If TRUE, then this is being called because a parent
 *            was destroyed. This generally means that the call to the windowing system
 *            to destroy the window can be omitted, since it will be destroyed as a result
 *            of the parent being destroyed. Unless @foreign_destroy
 *            
 * foreign_destroy: If TRUE, the window or a parent was destroyed by some external 
 *            agency. The window has already been destroyed and no windowing
 *            system calls should be made. (This may never happen for some
 *            windowing systems.)
 *
 * Internal function to destroy a window. Like gdk_window_destroy(), but does not
 * drop the reference count created by gdk_window_new().
 **/
static void
_gdk_window_destroy_hierarchy (GdkWindow *window,
			       gboolean   recursing,
			       gboolean   foreign_destroy)
{
  GdkWindowObject *private;
  GdkWindowObject *temp_private;
  GdkWindow *temp_window;
  GList *children;
  GList *tmp;
  
  g_return_if_fail (window != NULL);
  
  private = (GdkWindowObject*) window;
  
  switch (GDK_WINDOW_TYPE (window))
    {
    case GDK_WINDOW_TOPLEVEL:
    case GDK_WINDOW_CHILD:
    case GDK_WINDOW_DIALOG:
    case GDK_WINDOW_TEMP:
    case GDK_WINDOW_FOREIGN:
      if (!GDK_WINDOW_DESTROYED (window))
	{
	  private->mapped = FALSE;
	  private->destroyed = TRUE;
	  
	  _gdk_windowing_window_destroy (window, recursing, foreign_destroy);

	  if (private->parent)
	    {
	      GdkWindowObject *parent_private = (GdkWindowObject *)private->parent;
	      if (parent_private->children)
		parent_private->children = g_list_remove (parent_private->children, window);
	    }

	  _gdk_window_clear_update_area (window);
	  gdk_window_free_paint_stack (window);
	  
	  if (private->bg_pixmap &&
              private->bg_pixmap != GDK_PARENT_RELATIVE_BG &&
              private->bg_pixmap != GDK_NO_BG)
	    {
	      gdk_pixmap_unref (private->bg_pixmap);
	      private->bg_pixmap = NULL;
	    }
	  
	  if (GDK_WINDOW_TYPE (window) != GDK_WINDOW_FOREIGN)
	    {
	      children = tmp = private->children;
	      private->children = NULL;
	      
	      while (tmp)
		{
		  temp_window = tmp->data;
		  tmp = tmp->next;
		  
		  temp_private = (GdkWindowObject*) temp_window;
		  if (temp_private)
		    _gdk_window_destroy_hierarchy (temp_window, TRUE, foreign_destroy);
		}
	      
	      g_list_free (children);
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

          gdk_drawable_set_colormap (GDK_DRAWABLE (window), NULL);
	}
      break;
      
    case GDK_WINDOW_ROOT:
      g_error ("attempted to destroy root window");
      break;
    }
}

/**
 * _gdk_window_destroy:
 * @window: a #GdkWindow
 * foreign_destroy: If TRUE, the window or a parent was destroyed by some external 
 *            agency. The window has already been destroyed and no windowing
 *            system calls should be made. (This may never happen for some
 *            windowing systems.)
 *
 * Internal function to destroy a window. Like gdk_window_destroy(), but does not
 * drop the reference count created by gdk_window_new().
 **/
void
_gdk_window_destroy (GdkWindow *window,
		     gboolean   foreign_destroy)
{
  _gdk_window_destroy_hierarchy (window, FALSE, foreign_destroy);
}

void
gdk_window_destroy (GdkWindow *window)
{
  _gdk_window_destroy_hierarchy (window, FALSE, FALSE);
  gdk_drawable_unref (window);
}

void
gdk_window_set_user_data (GdkWindow *window,
			  gpointer   user_data)
{
  g_return_if_fail (window != NULL);
  
  ((GdkWindowObject*)window)->user_data = user_data;
}

void
gdk_window_get_user_data (GdkWindow *window,
			  gpointer  *data)
{
  g_return_if_fail (window != NULL);
  
  *data = ((GdkWindowObject*)window)->user_data;
}

GdkWindowType
gdk_window_get_window_type (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), (GdkWindowType) -1);
  
  return GDK_WINDOW_TYPE (window);
}

void
gdk_window_get_position (GdkWindow *window,
			 gint      *x,
			 gint      *y)
{
  GdkWindowObject *obj;
  
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  obj = (GdkWindowObject*) window;
  
  if (x)
    *x = obj->x;
  if (y)
    *y = obj->y;
}

GdkWindow*
gdk_window_get_parent (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);
  
  return (GdkWindow*) ((GdkWindowObject*) window)->parent;
}

GdkWindow*
gdk_window_get_toplevel (GdkWindow *window)
{
  GdkWindowObject *obj;
  
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  obj = (GdkWindowObject *)window;
  while (GDK_WINDOW_TYPE (obj) == GDK_WINDOW_CHILD)
    obj = (GdkWindowObject *)obj->parent;
  
  return GDK_WINDOW (obj);
}

GList*
gdk_window_get_children (GdkWindow *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return g_list_copy (GDK_WINDOW_OBJECT (window)->children);
}

GList *
gdk_window_peek_children (GdkWindow       *window)
{
  g_return_val_if_fail (GDK_IS_WINDOW (window), NULL);

  if (GDK_WINDOW_DESTROYED (window))
    return NULL;

  return GDK_WINDOW_OBJECT (window)->children;
}

void          
gdk_window_add_filter (GdkWindow     *window,
		       GdkFilterFunc  function,
		       gpointer       data)
{
  GdkWindowObject *private;
  GList *tmp_list;
  GdkEventFilter *filter;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;
  if (private && GDK_WINDOW_DESTROYED (window))
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
  GdkWindowObject *private;
  GList *tmp_list, *node;
  GdkEventFilter *filter;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  private = (GdkWindowObject*) window;
  
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
  
  tmp_list = ((GdkWindowObject *)gdk_parent_root)->children;
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
  GdkWindowObject *private = (GdkWindowObject *)window;
  
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
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  g_return_val_if_fail (window != NULL, FALSE);
  g_return_val_if_fail (GDK_IS_WINDOW (window), FALSE);
  
  while (private && 
	 (private != (GdkWindowObject *)gdk_parent_root) &&
	 (GDK_WINDOW_TYPE (private) != GDK_WINDOW_FOREIGN))
    {
      if (!private->mapped)
	return FALSE;
      
      private = (GdkWindowObject *)private->parent;
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
  GdkWindowObject *private = (GdkWindowObject *)window;

  guint gc_mask = 0;
  GdkGCValues gc_values;

  if (private->bg_pixmap == GDK_PARENT_RELATIVE_BG && private->parent)
    {
      GdkWindowPaint tmp_paint = *paint;
      tmp_paint.x_offset += private->x;
      tmp_paint.y_offset += private->y;
      
      return gdk_window_get_bg_gc (GDK_WINDOW (private->parent), &tmp_paint);
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
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkRectangle clip_box;
  GdkWindowPaint *paint;
  GdkRegion *init_region;
  GdkGC *tmp_gc;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
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
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowPaint *paint;
  GdkGC *tmp_gc;
  GdkRectangle clip_box;
  gint x_offset, y_offset;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  g_return_if_fail (private->paint_stack != NULL);

  paint = private->paint_stack->data;
  private->paint_stack = g_slist_delete_link (private->paint_stack, private->paint_stack);

  gdk_region_get_clipbox (paint->region, &clip_box);

  tmp_gc = gdk_gc_new (window);

  _gdk_windowing_window_get_offsets (window, &x_offset, &y_offset);

  gdk_gc_set_clip_region (tmp_gc, paint->region);
  gdk_gc_set_clip_origin (tmp_gc, -x_offset, -y_offset);

  gdk_draw_drawable (private->impl, tmp_gc, paint->pixmap,
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
gdk_window_free_paint_stack (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
  if (private->paint_stack)
    {
      GSList *tmp_list = private->paint_stack;

      while (tmp_list)
	{
	  GdkWindowPaint *paint = tmp_list->data;

	  if (tmp_list == private->paint_stack)
	    gdk_drawable_unref (paint->pixmap);
		  
	  gdk_region_destroy (paint->region);
	  g_free (paint);

	  tmp_list = tmp_list->next;
	}

      g_slist_free (private->paint_stack);
      private->paint_stack = NULL;
    }
}

static void
gdk_window_get_offsets (GdkWindow *window,
			gint      *x_offset,
			gint      *y_offset)
{
  GdkWindowObject *private = (GdkWindowObject *)window;
  
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
    gint old_clip_x = gc->clip_x_origin;    \
    gint old_clip_y = gc->clip_y_origin;    \
    gint old_ts_x = gc->ts_x_origin;        \
    gint old_ts_y = gc->ts_y_origin;        \
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

static GdkGC *
gdk_window_create_gc (GdkDrawable     *drawable,
                      GdkGCValues     *values,
                      GdkGCValuesMask  mask)
{
  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);
  
  if (GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  return gdk_gc_new_with_values (((GdkWindowObject *) drawable)->impl,
                                 values, mask);
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
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_rectangle (paint->pixmap, gc, filled,
			  x - x_offset, y - y_offset, width, height);
    }
  else
    gdk_draw_rectangle (private->impl, gc, filled,
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
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_arc (paint->pixmap, gc, filled,
		    x - x_offset, y_offset,
		    width, height, angle1, angle2);
    }
  else
    gdk_draw_arc (private->impl, gc, filled,
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
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkPoint *new_points;
  
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
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
    gdk_draw_polygon (private->impl, gc, filled, new_points, npoints);
  
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
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_text (paint->pixmap, font, gc, 
		     x - x_offset, y - y_offset, text, text_length);

    }
  else
    gdk_draw_text (private->impl, font, gc,
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
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_text_wc (paint->pixmap, font, gc, 
			x - x_offset, y - y_offset, text, text_length);
    }
  else
    gdk_draw_text_wc (private->impl, font, gc,
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
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_drawable (paint->pixmap, gc, src, xsrc, ysrc,
			 xdest - x_offset, ydest - y_offset, width, height);

    }
  else
    gdk_draw_drawable (private->impl, gc, src, xsrc, ysrc,
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
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkPoint *new_points;
  
  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
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
    gdk_draw_points (private->impl, gc, points, npoints);

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
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkSegment *new_segs;

  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
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
    gdk_draw_segments (private->impl, gc, new_segs, nsegs);
  
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
  GdkWindowObject *private = (GdkWindowObject *)drawable;
  GdkPoint *new_points;

  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
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
    gdk_draw_lines (private->impl, gc, new_points, npoints);

  if (new_points != points)
    g_free (new_points);

  RESTORE_GC (gc);
}

static void
gdk_window_draw_glyphs (GdkDrawable      *drawable,
			GdkGC            *gc,
			PangoFont        *font,
			gint              x,
			gint              y,
			PangoGlyphString *glyphs)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;

  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;

      gdk_draw_glyphs (paint->pixmap, gc, font, x - x_offset, y - y_offset, glyphs);
    }
  else
    gdk_draw_glyphs (private->impl, gc, font,
                     x - x_offset, y - y_offset, glyphs);

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
  GdkWindowObject *private = (GdkWindowObject *)window;
  GdkWindowPaint *paint = private->paint_stack->data;
  GdkGC *tmp_gc;

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  tmp_gc = gdk_window_get_bg_gc (window, paint);
  gdk_draw_rectangle (paint->pixmap, tmp_gc, TRUE,
		      x - paint->x_offset, y - paint->y_offset, width, height);
  gdk_gc_unref (tmp_gc);
}

void
gdk_window_clear (GdkWindow *window)
{
  gint width, height;
  
  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  gdk_drawable_get_size (GDK_DRAWABLE (window), &width, &height);
  
  gdk_window_clear_area (window, 0, 0,
                         width, height);
}

void
gdk_window_clear_area (GdkWindow *window,
		       gint       x,
		       gint       y,
		       gint       width,
		       gint       height)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

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
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  
  if (private->paint_stack)
    gdk_window_clear_backing_rect (window, x, y, width, height);

  _gdk_windowing_window_clear_area_e (window, x, y, width, height);
}

static void
gdk_window_draw_image (GdkDrawable *drawable,
                       GdkGC       *gc,
                       GdkImage    *image,
                       gint         xsrc,
                       gint         ysrc,
                       gint         xdest,
                       gint         ydest,
                       gint         width,
                       gint         height)
{
  GdkWindowObject *private = (GdkWindowObject *)drawable;

  OFFSET_GC (gc);

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  if (private->paint_stack)
    {
      GdkWindowPaint *paint = private->paint_stack->data;
      gdk_draw_image (paint->pixmap, gc, image, xsrc, ysrc,
                      xdest - x_offset, ydest - y_offset,
                      width, height);

    }
  else
    gdk_draw_image (private->impl, gc, image, xsrc, ysrc,
                    xdest - x_offset, ydest - y_offset,
                    width, height);

  RESTORE_GC (gc);
}


static void
gdk_window_real_get_size (GdkDrawable *drawable,
                          gint *width,
                          gint *height)
{
  g_return_if_fail (GDK_IS_WINDOW (drawable));

  gdk_drawable_get_size (GDK_WINDOW_OBJECT (drawable)->impl,
                         width, height);
}

static GdkVisual*
gdk_window_real_get_visual (GdkDrawable *drawable)
{
  GdkColormap *colormap;

  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);

  colormap = gdk_drawable_get_colormap (drawable);
  return colormap ? gdk_colormap_get_visual (colormap) : NULL;
}

static gint
gdk_window_real_get_depth (GdkDrawable *drawable)
{
  gint depth;
  
  g_return_val_if_fail (GDK_IS_WINDOW (drawable), 0);

  depth = ((GdkWindowObject *)GDK_WINDOW (drawable))->depth;

  if (depth == 0)
    {
      g_print ("0 depth for type %s\n", g_type_name (G_OBJECT_TYPE (drawable)));
      G_BREAKPOINT ();
    }

  return depth;
}

static void
gdk_window_real_set_colormap (GdkDrawable *drawable,
                              GdkColormap *cmap)
{
  g_return_if_fail (GDK_IS_WINDOW (drawable));  

  if (GDK_WINDOW_DESTROYED (drawable))
    return;
  
  gdk_drawable_set_colormap (((GdkWindowObject*)drawable)->impl, cmap);
}

static GdkColormap*
gdk_window_real_get_colormap (GdkDrawable *drawable)
{
  g_return_val_if_fail (GDK_IS_WINDOW (drawable), NULL);

  if (GDK_WINDOW_DESTROYED (drawable))
    return NULL;
  
  return gdk_drawable_get_colormap (((GdkWindowObject*)drawable)->impl);
}

/* Code for dirty-region queueing
 */

static GSList *update_windows = NULL;
static guint update_idle = 0;

static void
gdk_window_process_updates_internal (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window, *cur;
  gboolean save_region = FALSE;

  for(cur = private; cur; cur = cur->parent)
    {
      if(!cur->mapped)
	return;
    }

  /* If an update got queued during update processing, we can get a
   * window in the update queue that has an empty update_area.
   * just ignore it.
   */
  if (private->update_area)
    {
      GdkRegion *update_area = private->update_area;
      private->update_area = NULL;
      
      if (gdk_event_func)
	{
	  GdkEvent event;
	  GdkRectangle window_rect;
          gint width, height;

          gdk_drawable_get_size (GDK_DRAWABLE (private), &width, &height);
          
	  window_rect.x = 0;
	  window_rect.y = 0;
	  window_rect.width = width;
	  window_rect.height = height;

	  save_region = _gdk_windowing_window_queue_antiexpose (window, update_area);
      
	  event.expose.type = GDK_EXPOSE;
	  event.expose.window = gdk_window_ref (window);
	  event.expose.count = 0;
      
	  gdk_region_get_clipbox (update_area, &event.expose.area);
	  if (gdk_rectangle_intersect (&event.expose.area, &window_rect, &event.expose.area))
	    {
	      (*gdk_event_func) (&event, gdk_event_data);
	    }

	  gdk_window_unref (window);
	}
      
      if (!save_region)
	gdk_region_destroy (update_area);
    }
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
  GdkWindowObject *private = (GdkWindowObject *)window;

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
  GdkRectangle window_rect;
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (private->input_only || !private->mapped)
    return;

  if (!rect)
    {
      window_rect.x = 0;
      window_rect.y = 0;
      gdk_drawable_get_size (GDK_DRAWABLE (window),
                             &window_rect.width,
                             &window_rect.height);
      rect = &window_rect;
    }
  
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
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (GDK_WINDOW_DESTROYED (window))
    return;
  
  if (private->input_only || !private->mapped)
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
  GdkWindowObject *private = (GdkWindowObject *)window;
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

/**
 * _gdk_window_clear_update_area:
 * @window: a #GdkWindow.
 * 
 * Internal function to clear the update area for a window. This
 * is called when the window is hidden or destroyed.
 **/
void
_gdk_window_clear_update_area (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  if (private->update_area)
    {
      update_windows = g_slist_remove (update_windows, window);
      
      gdk_region_destroy (private->update_area);
      private->update_area = NULL;
    }
}

void
gdk_window_freeze_updates (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));

  private->update_freeze_count++;
}

void
gdk_window_thaw_updates (GdkWindow *window)
{
  GdkWindowObject *private = (GdkWindowObject *)window;

  g_return_if_fail (window != NULL);
  g_return_if_fail (GDK_IS_WINDOW (window));
  g_return_if_fail (private->update_freeze_count > 0);

  private->update_freeze_count--;
  if (!private->update_freeze_count && private->update_area && !update_idle)
    update_idle = g_idle_add_full (GDK_PRIORITY_REDRAW,
				   gdk_window_update_idle, NULL, NULL);
}

