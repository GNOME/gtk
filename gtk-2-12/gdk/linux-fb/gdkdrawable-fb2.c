/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Elliot Lee
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
#include "gdkprivate-fb.h"
#include "mi.h"
#include <string.h>
#include <gdkregion-generic.h>

#include <pango/pangoft2.h>
#include <freetype/ftglyph.h>

#ifndef g_alloca
#define g_alloca alloca
#endif

void                gdk_fb_draw_rectangle     (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       gboolean          filled,
					       gint              x,
					       gint              y,
					       gint              width,
					       gint              height);
static void         gdk_fb_draw_arc           (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       gboolean          filled,
					       gint              x,
					       gint              y,
					       gint              width,
					       gint              height,
					       gint              angle1,
					       gint              angle2);
static void         gdk_fb_draw_polygon       (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       gboolean          filled,
					       GdkPoint         *points,
					       gint              npoints);
static void         gdk_fb_draw_text          (GdkDrawable      *drawable,
					       GdkFont          *font,
					       GdkGC            *gc,
					       gint              x,
					       gint              y,
					       const gchar      *text,
					       gint              text_length);
static void         gdk_fb_draw_text_wc       (GdkDrawable      *drawable,
					       GdkFont          *font,
					       GdkGC            *gc,
					       gint              x,
					       gint              y,
					       const GdkWChar   *text,
					       gint              text_length);
static void         gdk_fb_draw_glyphs        (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       PangoFont        *font,
					       gint              x,
					       gint              y,
					       PangoGlyphString *glyphs);
void                gdk_fb_draw_drawable      (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       GdkPixmap        *src,
					       gint              xsrc,
					       gint              ysrc,
					       gint              xdest,
					       gint              ydest,
					       gint              width,
					       gint              height);
static void         gdk_fb_draw_image         (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       GdkImage         *image,
					       gint              xsrc,
					       gint              ysrc,
					       gint              xdest,
					       gint              ydest,
					       gint              width,
					       gint              height);
static void         gdk_fb_draw_points        (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       GdkPoint         *points,
					       gint              npoints);
static void         gdk_fb_draw_segments      (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       GdkSegment       *segs,
					       gint              nsegs);
static void         gdk_fb_draw_lines         (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       GdkPoint         *points,
					       gint              npoints);
static GdkColormap* gdk_fb_get_colormap       (GdkDrawable      *drawable);
static void         gdk_fb_set_colormap       (GdkDrawable      *drawable,
					       GdkColormap      *colormap);
static gint         gdk_fb_get_depth          (GdkDrawable      *drawable);
static GdkScreen*   gdk_fb_get_screen         (GdkDrawable      *drawable);
static GdkVisual*   gdk_fb_get_visual         (GdkDrawable      *drawable);
static void         gdk_fb_drawable_finalize  (GObject *object);

#ifdef ENABLE_SHADOW_FB
static void         gdk_shadow_fb_draw_rectangle     (GdkDrawable      *drawable,
						      GdkGC            *gc,
						      gboolean          filled,
						      gint              x,
						      gint              y,
						      gint              width,
						      gint              height);
static void         gdk_shadow_fb_draw_arc           (GdkDrawable      *drawable,
						      GdkGC            *gc,
						      gboolean          filled,
						      gint              x,
						      gint              y,
						      gint              width,
						      gint              height,
						      gint              angle1,
						      gint              angle2);
static void         gdk_shadow_fb_draw_polygon       (GdkDrawable      *drawable,
						      GdkGC            *gc,
						      gboolean          filled,
						      GdkPoint         *points,
						      gint              npoints);
static void         gdk_shadow_fb_draw_text          (GdkDrawable      *drawable,
						      GdkFont          *font,
						      GdkGC            *gc,
						      gint              x,
						      gint              y,
						      const gchar      *text,
						      gint              text_length);
static void         gdk_shadow_fb_draw_text_wc       (GdkDrawable      *drawable,
						      GdkFont          *font,
						      GdkGC            *gc,
						      gint              x,
						      gint              y,
						      const GdkWChar   *text,
						      gint              text_length);
static void         gdk_shadow_fb_draw_drawable      (GdkDrawable      *drawable,
						      GdkGC            *gc,
						      GdkPixmap        *src,
						      gint              xsrc,
						      gint              ysrc,
						      gint              xdest,
						      gint              ydest,
						      gint              width,
						      gint              height);
static void         gdk_shadow_fb_draw_image         (GdkDrawable      *drawable,
						      GdkGC            *gc,
						      GdkImage         *image,
						      gint              xsrc,
						      gint              ysrc,
						      gint              xdest,
						      gint              ydest,
						      gint              width,
						      gint              height);
static void         gdk_shadow_fb_draw_points        (GdkDrawable      *drawable,
						      GdkGC            *gc,
						      GdkPoint         *points,
						      gint              npoints);
static void         gdk_shadow_fb_draw_segments      (GdkDrawable      *drawable,
						      GdkGC            *gc,
						      GdkSegment       *segs,
						      gint              nsegs);
static void         gdk_shadow_fb_draw_lines         (GdkDrawable      *drawable,
						      GdkGC            *gc,
						      GdkPoint         *points,
						      gint              npoints);
#endif


static gpointer parent_class = NULL;

static void
gdk_fb_get_size (GdkDrawable *d, gint *width, gint *height)
{
  if (width)
    *width = GDK_DRAWABLE_FBDATA (d)->width;
  if (height)
    *height = GDK_DRAWABLE_FBDATA (d)->height;
}

static void
gdk_drawable_impl_fb_class_init (GdkDrawableFBClass *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_fb_drawable_finalize;
  
  drawable_class->create_gc = _gdk_fb_gc_new;
  
#ifdef ENABLE_SHADOW_FB
  drawable_class->draw_rectangle = gdk_shadow_fb_draw_rectangle;
  drawable_class->draw_arc = gdk_shadow_fb_draw_arc;
  drawable_class->draw_polygon = gdk_shadow_fb_draw_polygon;
  drawable_class->draw_text = gdk_shadow_fb_draw_text;
  drawable_class->draw_text_wc = gdk_shadow_fb_draw_text_wc;
  drawable_class->draw_drawable = gdk_shadow_fb_draw_drawable;
  drawable_class->draw_points = gdk_shadow_fb_draw_points;
  drawable_class->draw_segments = gdk_shadow_fb_draw_segments;
  drawable_class->draw_lines = gdk_shadow_fb_draw_lines;
  drawable_class->draw_image = gdk_shadow_fb_draw_image;
#else
  drawable_class->draw_rectangle = gdk_fb_draw_rectangle;
  drawable_class->draw_arc = gdk_fb_draw_arc;
  drawable_class->draw_polygon = gdk_fb_draw_polygon;
  drawable_class->draw_text = gdk_fb_draw_text;
  drawable_class->draw_text_wc = gdk_fb_draw_text_wc;
  drawable_class->draw_drawable = gdk_fb_draw_drawable;
  drawable_class->draw_points = gdk_fb_draw_points;
  drawable_class->draw_segments = gdk_fb_draw_segments;
  drawable_class->draw_lines = gdk_fb_draw_lines;
  drawable_class->draw_image = gdk_fb_draw_image;
#endif
  
  drawable_class->set_colormap = gdk_fb_set_colormap;
  drawable_class->get_colormap = gdk_fb_get_colormap;
  
  drawable_class->get_size = gdk_fb_get_size;
 
  drawable_class->get_depth = gdk_fb_get_depth;
  drawable_class->get_screen = gdk_fb_get_screen;
  drawable_class->get_visual = gdk_fb_get_visual;
  
  drawable_class->_copy_to_image = _gdk_fb_copy_to_image;
}

static void
gdk_fb_drawable_finalize (GObject *object)
{
  gdk_drawable_set_colormap (GDK_DRAWABLE (object), NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}


GType
gdk_drawable_impl_fb_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDrawableFBClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drawable_impl_fb_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDrawableFBData),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE,
                                            "GdkDrawableFB",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

/*****************************************************
 * FB specific implementations of generic functions *
 *****************************************************/

static GdkColormap*
gdk_fb_get_colormap (GdkDrawable *drawable)
{
  GdkColormap *retval = GDK_DRAWABLE_FBDATA (drawable)->colormap;

  if (!retval)
    retval = gdk_colormap_get_system ();

  return retval;
}

static void
gdk_fb_set_colormap (GdkDrawable *drawable,
		     GdkColormap *colormap)
{
  GdkDrawableFBData *private;

  private = GDK_DRAWABLE_FBDATA (drawable);

  if (private->colormap == colormap)
    return;

  if (private->colormap)
    gdk_colormap_unref (private->colormap);
  private->colormap = colormap;
  if (private->colormap)
    gdk_colormap_ref (private->colormap);
}

/* Calculates the real clipping region for a drawable, taking into account
 * other windows, gc clip region and gc clip mask.
 */
GdkRegion *
gdk_fb_clip_region (GdkDrawable *drawable,
		    GdkGC *gc,
		    gboolean do_clipping,
		    gboolean do_children,
		    gboolean full_shapes)
{
  GdkRectangle draw_rect;
  GdkRegion *real_clip_region, *tmpreg, *shape;
  gboolean skipit = FALSE;
  GdkDrawableFBData *private;
  GdkWindowObject *parent;

  GDK_CHECK_IMPL (drawable);
  
  private = GDK_DRAWABLE_FBDATA (drawable);
  
  g_assert(!GDK_IS_WINDOW (private->wrapper) ||
	   !GDK_WINDOW_P (private->wrapper)->input_only);
  
  draw_rect.x = private->llim_x;
  draw_rect.y = private->llim_y;
  if (!GDK_IS_WINDOW (private) ||
      GDK_WINDOW_IS_MAPPED (private->wrapper))
    {
      draw_rect.width = private->lim_x - draw_rect.x;
      draw_rect.height = private->lim_y - draw_rect.y;
    }
  else
    {
      draw_rect.width = draw_rect.height = 0;
      skipit = TRUE;
    }
  real_clip_region = gdk_region_rectangle (&draw_rect);
  if (skipit)
    return real_clip_region;

  if (GDK_IS_WINDOW (private->wrapper))
    {
      parent = GDK_WINDOW_P (private->wrapper);
      while (parent != (GdkWindowObject *)_gdk_parent_root)
	{
	  if (full_shapes)
	    {
	      shape = gdk_fb_window_get_abs_shape (GDK_DRAWABLE (parent));
	      if (shape)
		{
		  gdk_region_intersect (real_clip_region, shape);
		  gdk_region_destroy (shape);
		}
	    }
	  else
	    {
	      gint dx, dy;
	      shape = gdk_fb_window_peek_shape (GDK_DRAWABLE (parent), &dx, &dy);
	      if (shape)
		{
		  GdkRectangle rect;
		  GdkRegion *reg;
		  
		  gdk_region_get_clipbox (shape, &rect);
		  
		  rect.x += GDK_DRAWABLE_IMPL_FBDATA (parent)->abs_x + dx;
		  rect.y += GDK_DRAWABLE_IMPL_FBDATA (parent)->abs_y + dy;
		  
		  reg = gdk_region_rectangle(&rect);
		  gdk_region_intersect (real_clip_region, reg);
		  gdk_region_destroy (reg);
		}
	      
	    }
	  parent = parent->parent;
	}
    }

  if (gc && GDK_GC_FBDATA(gc)->values.subwindow_mode == GDK_INCLUDE_INFERIORS)
    do_children = FALSE;

  if (do_clipping &&
      GDK_IS_WINDOW (private->wrapper) &&
      GDK_WINDOW_IS_MAPPED (private->wrapper) &&
      !GDK_WINDOW_P (private->wrapper)->input_only)
    {
      GdkWindow *parentwin, *lastwin;
      GdkDrawableFBData *impl_private;

      lastwin = private->wrapper;
      if (do_children)
	parentwin = lastwin;
      else
	parentwin = (GdkWindow *)GDK_WINDOW_P (lastwin)->parent;

      /* Remove the areas of all overlapping windows above parentwin in the hiearachy */
      for (; parentwin; lastwin = parentwin, parentwin = (GdkWindow *)GDK_WINDOW_P (parentwin)->parent)
	{
	  GList *cur;

	  for (cur = GDK_WINDOW_P (parentwin)->children; cur && cur->data != lastwin; cur = cur->next)
	    {
	      if (!GDK_WINDOW_IS_MAPPED (cur->data) || GDK_WINDOW_P (cur->data)->input_only)
		continue;

	      impl_private = GDK_DRAWABLE_IMPL_FBDATA(cur->data);

	      /* This shortcut is really necessary for performance when there are a lot of windows */
	      if (impl_private->llim_x >= real_clip_region->extents.x2 ||
		  impl_private->lim_x <= real_clip_region->extents.x1 ||
		  impl_private->llim_y >= real_clip_region->extents.y2 ||
		  impl_private->lim_y <= real_clip_region->extents.y1)
		continue;
	      
	      draw_rect.x = impl_private->llim_x;
	      draw_rect.y = impl_private->llim_y;
	      draw_rect.width = impl_private->lim_x - draw_rect.x;
	      draw_rect.height = impl_private->lim_y - draw_rect.y;

	      tmpreg = gdk_region_rectangle (&draw_rect);

	      shape = gdk_fb_window_get_abs_shape (impl_private->wrapper);
	      if (shape)
		{
		  gdk_region_intersect (tmpreg, shape);
		  gdk_region_destroy (shape);
		}
	      
	      gdk_region_subtract (real_clip_region, tmpreg);
	      gdk_region_destroy (tmpreg);
	    }
	}
    }

  if (gc)
    {
      GdkRegion *gc = _gdk_gc_get_clip_region (gc);
      
      if (clip_region)
	{
	  tmpreg = gdk_region_copy (clip_region);
	  gdk_region_offset (tmpreg, private->abs_x + GDK_GC_P (gc)->clip_x_origin,
			     private->abs_y + GDK_GC_P (gc)->clip_y_origin);
	  gdk_region_intersect (real_clip_region, tmpreg);
	  gdk_region_destroy (tmpreg);
	}

      if (GDK_GC_FBDATA (gc)->values.clip_mask)
	{
	  GdkDrawable *cmask = GDK_GC_FBDATA (gc)->values.clip_mask;
	  GdkDrawableFBData *cmask_private;

	  cmask_private = GDK_DRAWABLE_IMPL_FBDATA (cmask);

	  g_assert (cmask_private->depth == 1);
	  g_assert (cmask_private->abs_x == 0 &&
		    cmask_private->abs_y == 0);

	  draw_rect.x = private->abs_x +
	                cmask_private->llim_x +
	                GDK_GC_FBDATA (gc)->values.clip_x_origin;
	  
	  draw_rect.y = private->abs_y +
	                cmask_private->llim_y +
	                GDK_GC_FBDATA (gc)->values.clip_y_origin;
	  
	  draw_rect.width = cmask_private->width;
	  draw_rect.height = cmask_private->height;

	  tmpreg = gdk_region_rectangle (&draw_rect);
	  gdk_region_intersect (real_clip_region, tmpreg);
	  gdk_region_destroy (tmpreg);
	  /*
	  if (!real_clip_region->numRects)
	    g_warning ("Empty clip region");
	  */
	}
    }

  return real_clip_region;
}


struct GdkSpanHelper
{
  GdkDrawable *drawable;
  GdkGC *gc;
  GdkColor color;
};

static void
gdk_fb_fill_span_helper(GdkSpan *span,
			gpointer data)
{
  struct GdkSpanHelper *info = (struct GdkSpanHelper *)data;
  GdkGC * gc = info->gc;
  
  (GDK_GC_FBDATA (gc)->fill_span) (info->drawable, gc, span, &info->color);
}

void
gdk_fb_fill_spans (GdkDrawable *real_drawable,
		   GdkGC *gc,
		   GdkSpan *spans,
		   int nspans,
		   gboolean sorted)
{
  int i;
  struct GdkSpanHelper info;
  GdkRegion *real_clip_region;
  gboolean handle_cursor = FALSE;
  GdkDrawable *drawable;
  GdkDrawableFBData *private;
  
  drawable = real_drawable;
  private = GDK_DRAWABLE_FBDATA (drawable);

  g_assert (gc);

  if (GDK_IS_WINDOW (private->wrapper) && !GDK_WINDOW_IS_MAPPED (private->wrapper))
    return;
  if (GDK_IS_WINDOW (private->wrapper) && GDK_WINDOW_P (private->wrapper)->input_only)
    g_error ("Drawing on the evil input-only!");

  info.drawable = drawable;
  info.gc = gc;
  
  if (GDK_GC_FBDATA (gc)->values_mask & GDK_GC_FOREGROUND)
    info.color = GDK_GC_FBDATA (gc)->values.foreground;
  else if (GDK_IS_WINDOW (private->wrapper))
    info.color = GDK_WINDOW_P (private->wrapper)->bg_color;
  else
    gdk_color_black (private->colormap, &info.color);

  real_clip_region = gdk_fb_clip_region (drawable, gc, TRUE, GDK_GC_FBDATA (gc)->values.function != GDK_INVERT, TRUE);

  if (private->mem == GDK_DRAWABLE_IMPL_FBDATA (_gdk_parent_root)->mem &&
      gdk_fb_cursor_region_need_hide (real_clip_region))
    {
      handle_cursor = TRUE;
      gdk_fb_cursor_hide ();
    }

  for (i = 0; i < nspans; i++)
    {
      GdkSpan *cur;
      
      cur = &spans[i];
      
      cur->x += private->abs_x;
      cur->y += private->abs_y;

      if ( (cur->y < private->llim_y) || (cur->y >= private->lim_y))
	cur->width = 0;
      
      if (cur->x < private->llim_x)
	{
	  cur->width -= private->llim_x - cur->x;
	  cur->x = private->llim_x;
	}
      
      if (cur->x + cur->width > private->lim_x)
	{
	  cur->width = private->lim_x - cur->x;
	}

      if (cur->width <= 0)
	cur->width = 0;
    }

  gdk_region_spans_intersect_foreach (real_clip_region,
				      spans,
				      nspans,
				      sorted,
				      gdk_fb_fill_span_helper,
				      &info);

  gdk_region_destroy (real_clip_region);
  if (handle_cursor)
    gdk_fb_cursor_unhide ();
}

void
gdk_fb_drawing_context_init (GdkFBDrawingContext *dc,
			     GdkDrawable *drawable,
			     GdkGC *gc,
			     gboolean draw_bg,
			     gboolean do_clipping)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  dc->mem = private->mem;
  dc->rowstride = private->rowstride;
  dc->handle_cursor = FALSE;
  dc->bgpm = NULL;
  dc->bg_relto = private->wrapper;
  dc->draw_bg = draw_bg;

  GDK_CHECK_IMPL (drawable);

  if (GDK_IS_WINDOW (private->wrapper))
    {
      dc->bgpm = GDK_WINDOW_P (private->wrapper)->bg_pixmap;
      if (dc->bgpm == GDK_PARENT_RELATIVE_BG)
	{
	  for (; dc->bgpm == GDK_PARENT_RELATIVE_BG && dc->bg_relto; dc->bg_relto = (GdkWindow *)GDK_WINDOW_P (dc->bg_relto)->parent)
	    dc->bgpm = GDK_WINDOW_P (dc->bg_relto)->bg_pixmap;
	}

      if (dc->bgpm == GDK_NO_BG)
	dc->bgpm = NULL;
    }
  dc->clipxoff = - private->abs_x;
  dc->clipyoff = - private->abs_y;

  dc->real_clip_region = gdk_fb_clip_region (drawable, gc, do_clipping, TRUE, TRUE);

  if (gc)
    {
      dc->clipxoff -= GDK_GC_FBDATA (gc)->values.clip_x_origin;
      dc->clipyoff -= GDK_GC_FBDATA (gc)->values.clip_y_origin;

      if (GDK_GC_FBDATA (gc)->values.clip_mask)
	{
	  dc->clipmem = GDK_DRAWABLE_IMPL_FBDATA (GDK_GC_FBDATA (gc)->values.clip_mask)->mem;
	  dc->clip_rowstride = GDK_DRAWABLE_IMPL_FBDATA (GDK_GC_FBDATA (gc)->values.clip_mask)->rowstride;
	}
    }

  if (do_clipping &&
      private->mem == GDK_DRAWABLE_IMPL_FBDATA (_gdk_parent_root)->mem &&
      gdk_fb_cursor_region_need_hide (dc->real_clip_region))
    {
      dc->handle_cursor = TRUE;
      gdk_fb_cursor_hide ();
    }
}

void
gdk_fb_drawing_context_finalize (GdkFBDrawingContext *dc)
{
  gdk_region_destroy (dc->real_clip_region);

  if (dc->handle_cursor)
    gdk_fb_cursor_unhide ();
}

void
gdk_fb_draw_drawable_2 (GdkDrawable *drawable,
			GdkGC       *gc,
			GdkPixmap   *src,
			gint         xsrc,
			gint         ysrc,
			gint         xdest,
			gint         ydest,
			gint         width,
			gint         height,
			gboolean     draw_bg,
			gboolean     do_clipping)
{
  GdkFBDrawingContext *dc, dc_data;
  dc = &dc_data;

  GDK_CHECK_IMPL (src);
  GDK_CHECK_IMPL (drawable);
  
  gdk_fb_drawing_context_init (dc, drawable, gc, draw_bg, do_clipping);
  gdk_fb_draw_drawable_3 (drawable, gc, src, dc, xsrc, ysrc, xdest, ydest, width, height);
  gdk_fb_drawing_context_finalize (dc);
}

void
gdk_fb_draw_drawable_3 (GdkDrawable *drawable,
			GdkGC       *gc,
			GdkPixmap   *src,
			GdkFBDrawingContext *dc,
			gint         xsrc,
			gint         ysrc,
			gint         xdest,
			gint         ydest,
			gint         width,
			gint         height)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  GdkDrawableFBData *src_private = GDK_DRAWABLE_FBDATA (src);
  GdkRectangle rect;
  int src_x_off, src_y_off;
  GdkRegion *tmpreg, *real_clip_region;
  int i;
  int draw_direction = 1;
  gdk_fb_draw_drawable_func *draw_func = NULL;
  GdkGCFBData *gc_private;

  g_assert (gc);

  GDK_CHECK_IMPL (src);
  GDK_CHECK_IMPL (drawable);

  if (GDK_IS_WINDOW (private->wrapper))
    {
      if (!GDK_WINDOW_IS_MAPPED (private->wrapper))
	return;
      if (GDK_WINDOW_P (private->wrapper)->input_only)
	g_error ("Drawing on the evil input-only!");
    }

  gc_private = GDK_GC_FBDATA (gc);
  
  if (drawable == src)
    {
      GdkRegionBox srcb, destb;
      srcb.x1 = xsrc;
      srcb.y1 = ysrc;
      srcb.x2 = xsrc + width;
      srcb.y2 = ysrc + height;
      destb.x1 = xdest;
      destb.y1 = ydest;
      destb.x2 = xdest + width;
      destb.y2 = ydest + height;

      if (EXTENTCHECK (&srcb, &destb) && ((ydest > ysrc) || ((ydest == ysrc) && (xdest > xsrc))))
	draw_direction = -1;
    }

  switch (src_private->depth)
    {
    case 1:
      draw_func = gc_private->draw_drawable[GDK_FB_SRC_BPP_1];
      break;
    case 8:
      draw_func = gc_private->draw_drawable[GDK_FB_SRC_BPP_8];
      break;
    case 16:
      draw_func = gc_private->draw_drawable[GDK_FB_SRC_BPP_16];
      break;
    case 24:
      draw_func = gc_private->draw_drawable[GDK_FB_SRC_BPP_24];
      break;
    case 32:
      draw_func = gc_private->draw_drawable[GDK_FB_SRC_BPP_32];
      break;
    case 77:
      draw_func = gc_private->draw_drawable[GDK_FB_SRC_BPP_7_AA_GRAYVAL];
      break;
    case 78:
      draw_func = gc_private->draw_drawable[GDK_FB_SRC_BPP_8_AA_GRAYVAL];
      break;
    default:
      g_assert_not_reached ();
      break;
    }
  
  /* Do some magic to avoid creating extra regions unnecessarily */
  tmpreg = dc->real_clip_region;

  rect.x = xdest + private->abs_x;
  rect.y = ydest + private->abs_y;
  rect.width = width;
  rect.height = height;
  real_clip_region = gdk_region_rectangle (&rect);
  gdk_region_intersect (real_clip_region, tmpreg);

  rect.x = xdest + private->abs_x;
  rect.y = ydest + private->abs_y;
  rect.width = MAX (src_private->width - xsrc, 0);
  rect.height = MAX (src_private->height - ysrc, 0);
  if (!rect.width || !rect.height)
    goto out;
  tmpreg = gdk_region_rectangle (&rect);
  gdk_region_intersect (real_clip_region, tmpreg);
  gdk_region_destroy (tmpreg);

  src_x_off = (src_private->abs_x + xsrc) - (private->abs_x + xdest);
  src_y_off = (src_private->abs_y + ysrc) - (private->abs_y + ydest);

  for (i = (draw_direction>0)?0:real_clip_region->numRects-1; i >= 0 && i < real_clip_region->numRects; i+=draw_direction)
    {
      GdkRegionBox *cur = &real_clip_region->rects[i];

      (*draw_func) (drawable,
		    gc,
		    src,
		    dc,
		    cur->y1,
		    cur->y2,
		    cur->x1,
		    cur->x2,
		    src_x_off,
		    src_y_off,
		    draw_direction);
    }

 out:
  gdk_region_destroy (real_clip_region);
}


void
gdk_fb_draw_drawable (GdkDrawable *drawable,
		      GdkGC       *gc,
		      GdkPixmap   *src,
		      gint         xsrc,
		      gint         ysrc,
		      gint         xdest,
		      gint         ydest,
		      gint         width,
		      gint         height)
{
  GdkPixmap   *src_impl;

  if (GDK_IS_DRAWABLE_IMPL_FBDATA (src))
    src_impl = src;
  else
    src_impl = GDK_DRAWABLE_IMPL (src);

  GDK_CHECK_IMPL (drawable);
  
  gdk_fb_draw_drawable_2 (drawable, gc, src_impl , xsrc, ysrc, xdest, ydest, width, height, TRUE, TRUE);
}

#ifdef EMULATE_GDKFONT
static void
gdk_fb_draw_text(GdkDrawable    *drawable,
		 GdkFont        *font,
		 GdkGC          *gc,
		 gint            x,
		 gint            y,
		 const gchar    *text,
		 gint            text_length)
{
  GdkFontPrivateFB *private;
  GdkDrawableFBData *drawable_private;
  guchar *utf8, *utf8_end;
  PangoGlyphString *glyphs = pango_glyph_string_new ();
  PangoEngineShape *shaper, *last_shaper;
  PangoAnalysis analysis;
  guchar *p, *start;
  gint x_offset;
  int i;
  
  g_return_if_fail (font != NULL);
  g_return_if_fail (text != NULL);

  private = (GdkFontPrivateFB*) font;
  drawable_private = GDK_DRAWABLE_FBDATA (drawable);

  utf8 = alloca (text_length*2);

  /* Convert latin-1 to utf8 */
  p = utf8;
  for (i = 0; i < text_length; i++)
    {
      if (text[i]==0)
	*p++ = 1; /* Hack to handle embedded nulls */
      else
	{
	  if(((guchar)text[i])<128)
	    *p++ = text[i];
	  else
	    {
	      *p++ = ((((guchar)text[i])>>6) & 0x3f) | 0xC0;
	      *p++ = (((guchar)text[i]) & 0x3f) | 0x80;
	    }
	}
    }
  utf8_end = p;

  last_shaper = NULL;
  shaper = NULL;

  x_offset = 0;
  p = start = utf8;
  while (p < utf8_end)
    {
      gunichar wc = g_utf8_get_char (p);
      p = g_utf8_next_char (p);
      shaper = pango_font_find_shaper (private->pango_font, pango_language_from_string ("fr"), wc);
      if (shaper != last_shaper)
	{
	  analysis.shape_engine = shaper;
	  analysis.lang_engine = NULL;
	  analysis.font = private->pango_font;
	  analysis.level = 0;
	  
	  pango_shape (start, p - start, &analysis, glyphs);

	  gdk_draw_glyphs (drawable_private->wrapper,
			   gc, private->pango_font,
			   x + PANGO_PIXELS (x_offset), y,
			   glyphs);
	  
	  for (i = 0; i < glyphs->num_glyphs; i++)
	    x_offset += glyphs->glyphs[i].geometry.width;
	  
	  start = p;
	}

      last_shaper = shaper;
    }

  if (p > start)
    {
      analysis.shape_engine = shaper;
      analysis.lang_engine = NULL;
      analysis.font = private->pango_font;
      analysis.level = 0;
      
      pango_shape (start, p - start, &analysis, glyphs);
      
      gdk_draw_glyphs (drawable_private->wrapper,
		       gc, private->pango_font,
		       x + PANGO_PIXELS (x_offset), y,
		       glyphs);
    }
  
  pango_glyph_string_free (glyphs);
      
  return;
}

#else
static void
gdk_fb_draw_text(GdkDrawable    *drawable,
		 GdkFont        *font,
		 GdkGC          *gc,
		 gint            x,
		 gint            y,
		 const gchar    *text,
		 gint            text_length)
{
  g_warning ("gdk_fb_draw_text NYI");
}
#endif

static void
gdk_fb_draw_text_wc (GdkDrawable    *drawable,
		     GdkFont	     *font,
		     GdkGC	     *gc,
		     gint	      x,
		     gint	      y,
		     const GdkWChar *text,
		     gint	      text_length)
{
  g_warning ("gdk_fb_draw_text_wc NYI");
}

void
gdk_fb_draw_rectangle (GdkDrawable    *drawable,
		       GdkGC          *gc,
		       gboolean        filled,
		       gint            x,
		       gint            y,
		       gint            width,
		       gint            height)
{
  GdkDrawableFBData *private;
  
  private = GDK_DRAWABLE_FBDATA (drawable);
  
  if (filled)
    {
      gboolean handle_cursor = FALSE;
      GdkRectangle tmprect;
      GdkRegion *tmpreg;
      GdkRegion *real_clip_region;
      GdkColor color;
      int i;
      
      if (GDK_GC_FBDATA (gc)->values_mask & GDK_GC_FOREGROUND)
	color = GDK_GC_FBDATA (gc)->values.foreground;
      else if (GDK_IS_WINDOW (private->wrapper))
	color = GDK_WINDOW_P (private->wrapper)->bg_color;
      else
	gdk_color_black (private->colormap, &color);
      
      real_clip_region = gdk_fb_clip_region (drawable, gc, TRUE, GDK_GC_FBDATA (gc)->values.function != GDK_INVERT, TRUE);
      
      if (private->mem == GDK_DRAWABLE_IMPL_FBDATA (_gdk_parent_root)->mem &&
	  gdk_fb_cursor_region_need_hide (real_clip_region))
	{
	  handle_cursor = TRUE;
	  gdk_fb_cursor_hide ();
	}
      
      x += private->abs_x;
      y += private->abs_y;
  
      if (x <  private->llim_x)
	{
	  width -= private->llim_x - x;
	  x = private->llim_x;
	}
      if (x + width >  private->lim_x)
	width =  private->lim_x - x;
      
      if (y <  private->llim_y)
	{
	  height -= private->llim_y - y;
	  y = private->llim_y;
	}
      if (y + height >  private->lim_y)
	height =  private->lim_y - y;

      tmprect.x = x;
      tmprect.y = y;
      tmprect.width = width;
      tmprect.height = height;
      tmpreg = gdk_region_rectangle (&tmprect);
      
      gdk_region_intersect (tmpreg, real_clip_region);
      
      for (i = 0; i < tmpreg->numRects; i++)
	{
	  GdkRectangle r;
	  r.x = tmpreg->rects[i].x1;
	  r.y = tmpreg->rects[i].y1;
	  r.width = tmpreg->rects[i].x2 - tmpreg->rects[i].x1;
	  r.height = tmpreg->rects[i].y2 - tmpreg->rects[i].y1;

	  if ((r.width > 0) && (r.height > 0))
	    (GDK_GC_FBDATA (gc)->fill_rectangle) (drawable, gc, &r, &color);
	}
      
      gdk_region_destroy (tmpreg);
      
      gdk_region_destroy (real_clip_region);
      if (handle_cursor)
	gdk_fb_cursor_unhide ();
    }
  else
    {
      GdkPoint pts[5];
      
      pts[0].x = pts[4].x = x;
      pts[0].y = pts[4].y = y;
      pts[1].x = x + width;
      pts[1].y = y;
      pts[2].x = x + width;
      pts[2].y = y + height;
      pts[3].x = x;
      pts[3].y = y + height;
      gdk_fb_draw_lines (drawable, gc, pts, 5);
    }

}

static void
gdk_fb_draw_points (GdkDrawable    *drawable,
		    GdkGC          *gc,
		    GdkPoint       *points,
		    gint            npoints)
{
  GdkSpan *spans = g_alloca (npoints * sizeof(GdkSpan));
  int i;

  for (i = 0; i < npoints; i++)
    {
      spans[i].x = points[i].x;
      spans[i].y = points[i].y;
      spans[i].width = 1;
    }

  gdk_fb_fill_spans (drawable, gc, spans, npoints, FALSE);
}

static void
gdk_fb_draw_arc (GdkDrawable    *drawable,
		 GdkGC          *gc,
		 gboolean        filled,
		 gint            x,
		 gint            y,
		 gint            width,
		 gint            height,
		 gint            angle1,
		 gint            angle2)
{
  miArc arc;

  arc.x = x;
  arc.y = y;
  arc.width = width;
  arc.height = height;
  arc.angle1 = angle1;
  arc.angle2 = angle2;

  if (filled)
    miPolyFillArc (drawable, gc, 1, &arc);
  else
    miPolyArc (drawable, gc, 1, &arc);
}

static void
gdk_fb_draw_polygon (GdkDrawable    *drawable,
		     GdkGC          *gc,
		     gboolean        filled,
		     GdkPoint       *points,
		     gint            npoints)
{
  if (filled)
    miFillPolygon (drawable, gc, 0, 0, npoints, points);
  else
    {
      gint tmp_npoints;
      gboolean free_points = FALSE;
      GdkPoint *tmp_points;

      if (points[0].x != points[npoints-1].x || points[0].y != points[npoints-1].y)
	{
	  tmp_npoints = npoints + 1;
	  tmp_points = g_new (GdkPoint, tmp_npoints);
	  free_points = TRUE;
	  memcpy (tmp_points, points, sizeof(GdkPoint) * npoints);
	  tmp_points[npoints].x = points[0].x;
	  tmp_points[npoints].y = points[0].y;
	}
      else
	{
	  tmp_npoints = npoints;
	  tmp_points = points;
	}

      gdk_fb_draw_lines (drawable, gc, tmp_points, tmp_npoints);
      
      if (free_points)
	g_free (tmp_points);
    }
}

static void
gdk_fb_draw_lines (GdkDrawable    *drawable,
		   GdkGC          *gc,
		   GdkPoint       *points,
		   gint            npoints)
{
  GdkGCFBData *private;

  private = GDK_GC_FBDATA (gc);
  if (private->values.line_width > 0)
    {
      if ((private->values.line_style != GDK_LINE_SOLID) && private->dash_list)
	miWideDash (drawable, gc, 0, npoints, points);
      else 
	miWideLine (drawable, gc, 0, npoints, points);
    }
  else
    {
      if ((private->values.line_style != GDK_LINE_SOLID) && private->dash_list)
	miZeroDashLine (drawable, gc, 0, npoints, points);
      else 
	miZeroLine (drawable, gc, 0, npoints, points);
    }
}

static void
gdk_fb_draw_segments (GdkDrawable    *drawable,
		      GdkGC          *gc,
		      GdkSegment     *segs,
		      gint            nsegs)
{
  GdkPoint pts[2];
  int i;

  for(i = 0; i < nsegs; i++)
    {
      pts[0].x = segs[i].x1;
      pts[0].y = segs[i].y1;
      pts[1].x = segs[i].x2;
      pts[1].y = segs[i].y2;

      gdk_fb_draw_lines (drawable, gc, pts, 2);
    }
}

void
gdk_fb_drawable_clear (GdkDrawable *d)
{
  extern void
    _gdk_windowing_window_clear_area (GdkWindow *window,
				      gint       x,
				      gint       y,
				      gint       width,
				      gint       height);

  _gdk_windowing_window_clear_area (d, 0, 0, GDK_DRAWABLE_IMPL_FBDATA (d)->width, GDK_DRAWABLE_IMPL_FBDATA (d)->height);
}

static void
gdk_fb_draw_image (GdkDrawable *drawable,
		   GdkGC       *gc,
		   GdkImage    *image,
		   gint         xsrc,
		   gint         ysrc,
		   gint         xdest,
		   gint         ydest,
		   gint         width,
		   gint         height)
{
  GdkImagePrivateFB *image_private;
  GdkPixmapFBData fbd;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (image != NULL);
  g_return_if_fail (gc != NULL);

  image_private = (GdkImagePrivateFB*) image;

  g_return_if_fail (image->type == GDK_IMAGE_NORMAL);

  /* Fake its existence as a pixmap */
  memset (&fbd, 0, sizeof(fbd));

  ((GTypeInstance *)&fbd)->g_class = g_type_class_peek (_gdk_pixmap_impl_get_type ());
  
  fbd.drawable_data.mem = image->mem;
  fbd.drawable_data.rowstride = image->bpl;
  fbd.drawable_data.width = fbd.drawable_data.lim_x = image->width;
  fbd.drawable_data.height = fbd.drawable_data.lim_y = image->height;
  fbd.drawable_data.depth = image->depth;
  fbd.drawable_data.window_type = GDK_DRAWABLE_PIXMAP;
  fbd.drawable_data.colormap = gdk_colormap_get_system ();
  
  gdk_fb_draw_drawable_2 (drawable, gc, (GdkPixmap *)&fbd, xsrc, ysrc, xdest, ydest, width, height, TRUE, TRUE);
}

static gint
gdk_fb_get_depth (GdkDrawable *drawable)
{
  return GDK_DRAWABLE_FBDATA (drawable)->depth;
}

static GdkScreen*
gdk_fb_get_screen (GdkDrawable *drawable)
{
  return gdk_screen_get_default();
}

static GdkVisual*
gdk_fb_get_visual (GdkDrawable    *drawable)
{
  return gdk_visual_get_system();
}

#ifdef ENABLE_SHADOW_FB
static void
gdk_shadow_fb_draw_rectangle (GdkDrawable      *drawable,
			      GdkGC            *gc,
			      gboolean          filled,
			      gint              x,
			      gint              y,
			      gint              width,
			      gint              height)
{
  GdkDrawableFBData *private;

  gdk_fb_draw_rectangle (drawable, gc, filled, x, y, width, height);

  private = GDK_DRAWABLE_FBDATA (drawable);
  if (GDK_IS_WINDOW (private->wrapper))
    {
      gint minx, miny, maxx, maxy;
      gint extra_width;
  
      minx = x + private->abs_x;
      miny = y + private->abs_y;
      maxx = x + width + private->abs_x;
      maxy = y + height + private->abs_y;
      
      if (!filled)
	{
	  extra_width = (GDK_GC_FBDATA (gc)->values.line_width + 1) / 2;
	  
	  minx -= extra_width;
	  miny -= extra_width;
	  maxx += extra_width;
	  maxy += extra_width;
	}
      gdk_shadow_fb_update (minx, miny, maxx, maxy);
    }
}

static void
gdk_shadow_fb_draw_arc (GdkDrawable      *drawable,
			GdkGC            *gc,
			gboolean          filled,
			gint              x,
			gint              y,
			gint              width,
			gint              height,
			gint              angle1,
			gint              angle2)
{
  GdkDrawableFBData *private;
  
  gdk_fb_draw_arc (drawable, gc, filled, x, y, width, height, angle1, angle2);

  private = GDK_DRAWABLE_FBDATA (drawable);
  if (GDK_IS_WINDOW (private->wrapper))
    {
      gint minx, miny, maxx, maxy;
      gint extra_width;
  
      minx = x + private->abs_x;
      miny = y + private->abs_y;
      maxx = x + width + private->abs_x;
      maxy = y + height + private->abs_y;
      
      if (!filled)
	{
	  extra_width = (GDK_GC_FBDATA (gc)->values.line_width + 1) / 2;
	  
	  minx -= extra_width;
	  miny -= extra_width;
	  maxx += extra_width;
	  maxy += extra_width;
	}
      gdk_shadow_fb_update (minx, miny, maxx, maxy);
    }
}

static void
gdk_shadow_fb_draw_polygon (GdkDrawable      *drawable,
			    GdkGC            *gc,
			    gboolean          filled,
			    GdkPoint         *points,
			    gint              npoints)
{
  GdkDrawableFBData *private;

  gdk_fb_draw_polygon (drawable, gc, filled, points, npoints);

  private = GDK_DRAWABLE_FBDATA (drawable);
  if (GDK_IS_WINDOW (private->wrapper))
    {
      gint minx, miny, maxx, maxy;
      gint extra_width;
      int i;
  
      minx = maxx = points[0].x;
      miny = maxy = points[0].y;

      for (i = 1; i < npoints; i++)
	{
	  minx = MIN(minx, points[i].x);
	  maxx = MAX(maxx, points[i].x);
	  miny = MIN(miny, points[i].y);
	  maxy = MAX(maxy, points[i].y);
	}
      
      if (!filled)
	{
	  extra_width = (GDK_GC_FBDATA (gc)->values.line_width + 1) / 2;
	  
	  minx -= extra_width;
	  miny -= extra_width;
	  maxx += extra_width;
	  maxy += extra_width;
	}
      gdk_shadow_fb_update (minx + private->abs_x, miny + private->abs_y,
			    maxx + private->abs_x, maxy + private->abs_y);
    }
}

static void
gdk_shadow_fb_draw_text (GdkDrawable      *drawable,
			 GdkFont          *font,
			 GdkGC            *gc,
			 gint              x,
			 gint              y,
			 const gchar      *text,
			 gint              text_length)
{
  gdk_fb_draw_text (drawable, font, gc, x, y, text, text_length);
}

static void
gdk_shadow_fb_draw_text_wc (GdkDrawable      *drawable,
			    GdkFont          *font,
			    GdkGC            *gc,
			    gint              x,
			    gint              y,
			    const GdkWChar   *text,
			    gint              text_length)
{
  gdk_fb_draw_text_wc (drawable, font, gc, x, y, text, text_length);
}

static void
gdk_shadow_fb_draw_drawable (GdkDrawable      *drawable,
			     GdkGC            *gc,
			     GdkPixmap        *src,
			     gint              xsrc,
			     gint              ysrc,
			     gint              xdest,
			     gint              ydest,
			     gint              width,
			     gint              height)
{
  GdkDrawableFBData *private;
  
  gdk_fb_draw_drawable (drawable, gc, src, xsrc, ysrc, xdest, ydest, width, height);
  
  private = GDK_DRAWABLE_FBDATA (drawable);
  if (GDK_IS_WINDOW (private->wrapper))
    gdk_shadow_fb_update (xdest + private->abs_x, ydest + private->abs_y,
			  xdest + private->abs_x + width, ydest + private->abs_y + height);
}

static void
gdk_shadow_fb_draw_image (GdkDrawable      *drawable,
			  GdkGC            *gc,
			  GdkImage         *image,
			  gint              xsrc,
			  gint              ysrc,
			  gint              xdest,
			  gint              ydest,
			  gint              width,
			  gint              height)
{
  GdkDrawableFBData *private;
  
  gdk_fb_draw_image (drawable, gc, image, xsrc, ysrc, xdest, ydest, width, height);
  
  private = GDK_DRAWABLE_FBDATA (drawable);
  if (GDK_IS_WINDOW (private->wrapper))
    gdk_shadow_fb_update (xdest + private->abs_x, ydest + private->abs_y,
			  xdest + private->abs_x + width, ydest + private->abs_y + height);
}

static void
gdk_shadow_fb_draw_points (GdkDrawable      *drawable,
			   GdkGC            *gc,
			   GdkPoint         *points,
			   gint              npoints)
{
  GdkDrawableFBData *private;

  gdk_fb_draw_points (drawable, gc, points, npoints);

  private = GDK_DRAWABLE_FBDATA (drawable);
  if (GDK_IS_WINDOW (private->wrapper))
    {
      gint minx, miny, maxx, maxy;
      int i;
  
      minx = maxx = points[0].x;
      miny = maxy = points[0].y;

      for (i = 1; i < npoints; i++)
	{
	  minx = MIN(minx, points[i].x);
	  maxx = MAX(maxx, points[i].x);
	  miny = MIN(miny, points[i].y);
	  maxy = MAX(maxy, points[i].y);
	}
      
      gdk_shadow_fb_update (minx + private->abs_x, miny + private->abs_y,
			    maxx + private->abs_x, maxy + private->abs_y);
    }
}

static void
gdk_shadow_fb_draw_segments (GdkDrawable      *drawable,
			     GdkGC            *gc,
			     GdkSegment       *segs,
			     gint              nsegs)
{
  GdkDrawableFBData *private;

  gdk_fb_draw_segments (drawable, gc, segs, nsegs);

  private = GDK_DRAWABLE_FBDATA (drawable);
  if (GDK_IS_WINDOW (private->wrapper))
    {
      gint minx, miny, maxx, maxy;
      gint extra_width;
      int i;
  
      minx = maxx = segs[0].x1;
      miny = maxy = segs[0].y1;

      for (i = 0; i < nsegs; i++)
	{
	  minx = MIN(minx, segs[i].x1);
	  maxx = MAX(maxx, segs[i].x1);
	  minx = MIN(minx, segs[i].x2);
	  maxx = MAX(maxx, segs[i].x2);
	  
	  miny = MIN(miny, segs[i].y1);
	  maxy = MAX(maxy, segs[i].y1);
	  miny = MIN(miny, segs[i].y2);
	  maxy = MAX(maxy, segs[i].y2);

	}
      
      extra_width = (GDK_GC_FBDATA (gc)->values.line_width + 1) / 2;
      
      minx -= extra_width;
      miny -= extra_width;
      maxx += extra_width;
      maxy += extra_width;
      
      gdk_shadow_fb_update (minx + private->abs_x, miny + private->abs_y,
			    maxx + private->abs_x, maxy + private->abs_y);
    }
}

static void
gdk_shadow_fb_draw_lines (GdkDrawable      *drawable,
			  GdkGC            *gc,
			  GdkPoint         *points,
			  gint              npoints)
{
  GdkDrawableFBData *private;

  gdk_fb_draw_lines (drawable, gc, points, npoints);

  private = GDK_DRAWABLE_FBDATA (drawable);
  if (GDK_IS_WINDOW (private->wrapper))
    {
      gint minx, miny, maxx, maxy;
      gint extra_width;
      int i;
  
      minx = maxx = points[0].x;
      miny = maxy = points[0].y;

      for (i = 1; i < npoints; i++)
	{
	  minx = MIN(minx, points[i].x);
	  maxx = MAX(maxx, points[i].x);
	  miny = MIN(miny, points[i].y);
	  maxy = MAX(maxy, points[i].y);
	}
      
      extra_width = (GDK_GC_FBDATA (gc)->values.line_width + 1) / 2;
	  
      minx -= extra_width;
      miny -= extra_width;
      maxx += extra_width;
      maxy += extra_width;
      
      gdk_shadow_fb_update (minx + private->abs_x, miny + private->abs_y,
			    maxx + private->abs_x, maxy + private->abs_y);
    }
}

#endif

gboolean
gdk_draw_rectangle_alpha_libgtk_only (GdkDrawable *drawable,
				      gint         x,
				      gint         y,
				      gint         width,
				      gint         height,
				      GdkColor    *color,
				      guint16      alpha)
{
  return FALSE;
}
