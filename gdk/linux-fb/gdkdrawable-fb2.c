#include "gdkprivate-fb.h"
#include "mi.h"
#include <string.h>

#include <pango/pangoft2.h>
#include <freetype/ftglyph.h>

#ifndef g_alloca
#define g_alloca alloca
#endif

void                gdk_fb_draw_rectangle     (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       gint              filled,
					       gint              x,
					       gint              y,
					       gint              width,
					       gint              height);
static void         gdk_fb_draw_arc           (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       gint              filled,
					       gint              x,
					       gint              y,
					       gint              width,
					       gint              height,
					       gint              angle1,
					       gint              angle2);
static void         gdk_fb_draw_polygon       (GdkDrawable      *drawable,
					       GdkGC            *gc,
					       gint              filled,
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
static GdkColormap* gdk_fb_get_colormap       (GdkDrawable      *drawable);
static void         gdk_fb_set_colormap       (GdkDrawable      *drawable,
					       GdkColormap      *colormap);
static gint         gdk_fb_get_depth          (GdkDrawable      *drawable);
static GdkVisual*   gdk_fb_get_visual         (GdkDrawable      *drawable);


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

  parent_class = g_type_class_peek_parent (klass);

  drawable_class->create_gc = _gdk_fb_gc_new;
  drawable_class->draw_rectangle = gdk_fb_draw_rectangle;
  drawable_class->draw_arc = gdk_fb_draw_arc;
  drawable_class->draw_polygon = gdk_fb_draw_polygon;
  drawable_class->draw_text = gdk_fb_draw_text;
  drawable_class->draw_text_wc = gdk_fb_draw_text_wc;
  drawable_class->draw_drawable = gdk_fb_draw_drawable;
  drawable_class->draw_points = gdk_fb_draw_points;
  drawable_class->draw_segments = gdk_fb_draw_segments;
  drawable_class->draw_lines = gdk_fb_draw_lines;
  drawable_class->draw_glyphs = gdk_fb_draw_glyphs;
  drawable_class->draw_image = gdk_fb_draw_image;
  
  drawable_class->set_colormap = gdk_fb_set_colormap;
  drawable_class->get_colormap = gdk_fb_get_colormap;
  
  drawable_class->get_size = gdk_fb_get_size;
 
  drawable_class->get_depth = gdk_fb_get_depth;
  drawable_class->get_visual = gdk_fb_get_visual;
  
  drawable_class->get_image = _gdk_fb_get_image;
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
  GdkColormap *old_cmap;
  GdkDrawableFBData *private;

  private = GDK_DRAWABLE_FBDATA (drawable);
  
  old_cmap = private->colormap;
  private->colormap = gdk_colormap_ref (colormap);
  gdk_colormap_unref (old_cmap);
}

/* Calculates the real clipping region for a drawable, taking into account
 * other windows, gc clip region and gc clip mask.
 */
GdkRegion *
gdk_fb_clip_region (GdkDrawable *drawable,
		    GdkGC *gc,
		    gboolean do_clipping,
		    gboolean do_children)
{
  GdkRectangle draw_rect;
  GdkRegion *real_clip_region, *tmpreg;
  gboolean skipit = FALSE;
  GdkDrawableFBData *private;

  private = GDK_DRAWABLE_FBDATA (drawable);
  
  g_assert(!GDK_IS_WINDOW (private->wrapper) ||
	   !GDK_WINDOW_P (private->wrapper)->input_only);
  
  draw_rect.x = private->llim_x;
  draw_rect.y = private->llim_y;
  if (!GDK_IS_WINDOW (private) ||
      GDK_WINDOW_P (private->wrapper)->mapped)
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

  if (gc && GDK_GC_FBDATA(gc)->values.subwindow_mode == GDK_INCLUDE_INFERIORS)
    do_children = FALSE;

  if (do_clipping &&
      GDK_IS_WINDOW (private->wrapper) &&
      GDK_WINDOW_P (private->wrapper)->mapped &&
      !GDK_WINDOW_P (private->wrapper)->input_only)
    {
      GdkWindow *parentwin, *lastwin;
      GdkDrawableFBData *impl_private;

      lastwin = private->wrapper;
      if (do_children)
	parentwin = lastwin;
      else
	parentwin = (GdkWindow *)GDK_WINDOW_P (lastwin)->parent;

      for (; parentwin; lastwin = parentwin, parentwin = (GdkWindow *)GDK_WINDOW_P (parentwin)->parent)
	{
	  GList *cur;

	  for (cur = GDK_WINDOW_P (parentwin)->children; cur && cur->data != lastwin; cur = cur->next)
	    {
	      if (!GDK_WINDOW_P (cur->data)->mapped || GDK_WINDOW_P (cur->data)->input_only)
		continue;

	      impl_private = GDK_DRAWABLE_IMPL_FBDATA(cur->data);
	      draw_rect.x = impl_private->llim_x;
	      draw_rect.y = impl_private->llim_y;
	      draw_rect.width = impl_private->lim_x - draw_rect.x;
	      draw_rect.height = impl_private->lim_y - draw_rect.y;

	      tmpreg = gdk_region_rectangle (&draw_rect);

	      gdk_region_subtract (real_clip_region, tmpreg);
	      gdk_region_destroy (tmpreg);
	    }
	}
    }

  if (gc)
    {
      if (GDK_GC_FBDATA (gc)->clip_region)
	{
	  tmpreg = gdk_region_copy (GDK_GC_FBDATA (gc)->clip_region);
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
	  if (!real_clip_region->numRects)
	    g_warning ("Empty clip region");
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

  if (GDK_IS_WINDOW (private->wrapper) && !GDK_WINDOW_P (private->wrapper)->mapped)
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

  real_clip_region = gdk_fb_clip_region (drawable, gc, TRUE, GDK_GC_FBDATA (gc)->values.function != GDK_INVERT);

  if (private->mem == GDK_DRAWABLE_IMPL_FBDATA (gdk_parent_root)->mem &&
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

  dc->real_clip_region = gdk_fb_clip_region (drawable, gc, do_clipping, TRUE);

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
      private->mem == GDK_DRAWABLE_IMPL_FBDATA (gdk_parent_root)->mem &&
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

  if (GDK_IS_WINDOW (private->wrapper))
    {
      if (!GDK_WINDOW_P (private->wrapper)->mapped)
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

  
  gdk_fb_draw_drawable_2 (drawable, gc, src_impl , xsrc, ysrc, xdest, ydest, width, height, TRUE, TRUE);
}




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
		       gint            filled,
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
      
      real_clip_region = gdk_fb_clip_region (drawable, gc, TRUE, GDK_GC_FBDATA (gc)->values.function != GDK_INVERT);
      
      if (private->mem == GDK_DRAWABLE_IMPL_FBDATA (gdk_parent_root)->mem &&
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
		 gint            filled,
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
		     gint            filled,
		     GdkPoint       *points,
		     gint            npoints)
{
  if (filled)
    miFillPolygon (drawable, gc, 0, 0, npoints, points);
  else
    {
      GdkPoint *realpts = g_alloca (sizeof(GdkPoint) * (npoints + 1));

      memcpy (realpts, points, sizeof(GdkPoint) * npoints);
      realpts[npoints] = points[0];
      gdk_fb_draw_lines (drawable, gc, points, npoints);
    }
}

void
gdk_fb_draw_lines (GdkDrawable    *drawable,
		   GdkGC          *gc,
		   GdkPoint       *points,
		   gint            npoints)
{
  GdkGCFBData *private;

  private = GDK_GC_FBDATA (gc);
  if (private->values.line_width > 0)
    {
      if (private->dash_list)
	miWideDash (drawable, gc, 0, npoints, points);
      else 
	miWideLine (drawable, gc, 0, npoints, points);
    }
  else
    {
      if (private->dash_list)
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
gdk_fb_draw_glyphs (GdkDrawable      *drawable,
		    GdkGC	     *gc,
		    PangoFont        *font,
		    gint              x,
		    gint              y,
		    PangoGlyphString *glyphs)
{
  GdkFBDrawingContext fbdc;
  FT_Bitmap bitmap;
  GdkPixmapFBData pixmap;
  PangoFT2Subfont subfont_index;
  PangoGlyphInfo *gi;
  FT_Face face;
  FT_UInt glyph_index;
  int i, xpos;

  g_return_if_fail (font);

  gdk_fb_drawing_context_init (&fbdc, drawable, gc, FALSE, TRUE);

  /* Fake its existence as a pixmap */

  pixmap.drawable_data.abs_x = 0;
  pixmap.drawable_data.abs_y = 0;
  pixmap.drawable_data.depth = 78;
  
  gi = glyphs->glyphs;
  for (i = 0, xpos = 0; i < glyphs->num_glyphs; i++, gi++)
    {
      if (gi->glyph)
	{
	  glyph_index = PANGO_FT2_GLYPH_INDEX (gi->glyph);
	  subfont_index = PANGO_FT2_GLYPH_SUBFONT (gi->glyph);
	  face = pango_ft2_get_face (font, subfont_index);

	  if (face)
	    {
	      /* Draw glyph */
	      FT_Load_Glyph (face, glyph_index, FT_LOAD_DEFAULT);
	      if (face->glyph->format != ft_glyph_format_bitmap)
		FT_Render_Glyph (face->glyph, ft_render_mode_normal);

	      pixmap.drawable_data.mem = face->glyph->bitmap.buffer;
	      pixmap.drawable_data.rowstride = face->glyph->bitmap.pitch;
	      pixmap.drawable_data.width = face->glyph->bitmap.width;
	      pixmap.drawable_data.height = face->glyph->bitmap.rows;
	      
	      gdk_fb_draw_drawable_3 (drawable, gc, (GdkPixmap *)&pixmap,
				      &fbdc,
				      0, 0,
				      x + PANGO_PIXELS (xpos) + face->glyph->bitmap_left,
				      y - face->glyph->bitmap_top + 1,
				      face->glyph->bitmap.width, face->glyph->bitmap.rows);
	    }
	}
      xpos += glyphs->glyphs[i].geometry.width;
    }

  gdk_fb_drawing_context_finalize (&fbdc);
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
  return GDK_DRAWABLE_IMPL_FBDATA (drawable)->depth;
}

static GdkVisual*
gdk_fb_get_visual (GdkDrawable    *drawable)
{
  return gdk_visual_get_system();
}
