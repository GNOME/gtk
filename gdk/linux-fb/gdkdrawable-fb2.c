#include "gdkprivate-fb.h"
#include "mi.h"
#include <t1lib.h>

#ifndef g_alloca
#define g_alloca alloca
#endif

static void gdk_fb_drawable_destroy   (GdkDrawable     *drawable);
void gdk_fb_draw_rectangle (GdkDrawable    *drawable,
			    GdkGC          *gc,
			    gint            filled,
			    gint            x,
			    gint            y,
			    gint            width,
			    gint            height);
static void gdk_fb_draw_arc       (GdkDrawable    *drawable,
				   GdkGC          *gc,
				   gint            filled,
				   gint            x,
				   gint            y,
				   gint            width,
				   gint            height,
				   gint            angle1,
				   gint            angle2);
static void gdk_fb_draw_polygon   (GdkDrawable    *drawable,
				   GdkGC          *gc,
				   gint            filled,
				   GdkPoint       *points,
				   gint            npoints);
static void gdk_fb_draw_text      (GdkDrawable    *drawable,
				   GdkFont        *font,
				   GdkGC          *gc,
				   gint            x,
				   gint            y,
				   const gchar    *text,
				   gint            text_length);
static void gdk_fb_draw_text_wc   (GdkDrawable    *drawable,
				   GdkFont        *font,
				   GdkGC          *gc,
				   gint            x,
				   gint            y,
				   const GdkWChar *text,
				   gint            text_length);
void gdk_fb_draw_drawable  (GdkDrawable    *drawable,
			    GdkGC          *gc,
			    GdkPixmap      *src,
			    gint            xsrc,
			    gint            ysrc,
			    gint            xdest,
			    gint            ydest,
			    gint            width,
			    gint            height);
static void gdk_fb_draw_points    (GdkDrawable    *drawable,
				   GdkGC          *gc,
				   GdkPoint       *points,
				   gint            npoints);
static void gdk_fb_draw_segments  (GdkDrawable    *drawable,
				   GdkGC          *gc,
				   GdkSegment     *segs,
				   gint            nsegs);
static void gdk_fb_draw_lines     (GdkDrawable    *drawable,
				   GdkGC          *gc,
				   GdkPoint       *points,
				   gint            npoints);

GdkDrawableClass _gdk_fb_drawable_class = {
  gdk_fb_drawable_destroy,
  (gpointer)_gdk_fb_gc_new,
  gdk_fb_draw_rectangle,
  gdk_fb_draw_arc,
  gdk_fb_draw_polygon,
  gdk_fb_draw_text,
  gdk_fb_draw_text_wc,
  gdk_fb_draw_drawable,
  gdk_fb_draw_points,
  gdk_fb_draw_segments,
  gdk_fb_draw_lines
};

/*****************************************************
 * FB specific implementations of generic functions *
 *****************************************************/

GdkColormap*
gdk_drawable_get_colormap (GdkDrawable *drawable)
{
  GdkColormap *retval = GDK_DRAWABLE_P(drawable)->colormap;

  if(!retval)
    retval = gdk_colormap_get_system();

  return retval;
}

void
gdk_drawable_set_colormap (GdkDrawable *drawable,
			   GdkColormap *colormap)
{
  GdkColormap *old_cmap;
  old_cmap = GDK_DRAWABLE_P(drawable)->colormap;
  GDK_DRAWABLE_P(drawable)->colormap = gdk_colormap_ref(colormap);
  gdk_colormap_unref(old_cmap);
}

/* Drawing
 */
static void 
gdk_fb_drawable_destroy (GdkDrawable *drawable)
{
}

static GdkRegion *
gdk_fb_clip_region(GdkDrawable *drawable, GdkGC *gc, gboolean do_clipping)
{
  GdkRectangle draw_rect;
  GdkRegion *real_clip_region, *tmpreg;

  g_assert(!GDK_IS_WINDOW(drawable) || !GDK_WINDOW_P(drawable)->input_only);

  draw_rect.x = GDK_DRAWABLE_FBDATA(drawable)->llim_x;
  draw_rect.y = GDK_DRAWABLE_FBDATA(drawable)->llim_y;
  draw_rect.width = GDK_DRAWABLE_FBDATA(drawable)->lim_x - draw_rect.x;
  draw_rect.height = GDK_DRAWABLE_FBDATA(drawable)->lim_y - draw_rect.y;
  real_clip_region = gdk_region_rectangle(&draw_rect);

  if(do_clipping && GDK_IS_WINDOW(drawable) && GDK_WINDOW_P(drawable)->mapped && !GDK_WINDOW_P(drawable)->input_only)
    {
      GdkWindow *parentwin, *lastwin;

      for(parentwin = lastwin = ((GdkWindow *)drawable);
	  parentwin; lastwin = parentwin, parentwin = GDK_WINDOW_P(parentwin)->parent)
	{
	  GList *cur;

	  for(cur = GDK_WINDOW_P(parentwin)->children; cur && cur->data != lastwin; cur = cur->next)
	    {
	      if(!GDK_WINDOW_P(cur->data)->mapped || GDK_WINDOW_P(cur->data)->input_only)
		continue;

	      draw_rect.x = GDK_DRAWABLE_FBDATA(cur->data)->llim_x;
	      draw_rect.y = GDK_DRAWABLE_FBDATA(cur->data)->llim_y;
	      draw_rect.width = GDK_DRAWABLE_FBDATA(cur->data)->lim_x - draw_rect.x;
	      draw_rect.height = GDK_DRAWABLE_FBDATA(cur->data)->lim_y - draw_rect.y;

	      tmpreg = gdk_region_rectangle(&draw_rect);
	      gdk_region_subtract(real_clip_region, tmpreg);
	      gdk_region_destroy(tmpreg);
	    }
	}
    }

  if(gc)
    {
      if(GDK_GC_FBDATA(gc)->clip_region)
	{
	  tmpreg = gdk_region_copy(GDK_GC_FBDATA(gc)->clip_region);
	  gdk_region_offset(tmpreg, GDK_DRAWABLE_FBDATA(drawable)->abs_x + GDK_GC_P(gc)->clip_x_origin,
			    GDK_DRAWABLE_FBDATA(drawable)->abs_y + GDK_GC_P(gc)->clip_y_origin);
	  gdk_region_intersect(real_clip_region, tmpreg);
	  gdk_region_destroy(tmpreg);
	}

      if(GDK_GC_FBDATA(gc)->values.clip_mask)
	{
	  GdkDrawable *cmask = GDK_GC_FBDATA(gc)->values.clip_mask;

	  g_assert(GDK_DRAWABLE_P(cmask)->depth == 1);
	  g_assert(GDK_DRAWABLE_FBDATA(cmask)->abs_x == 0
		   && GDK_DRAWABLE_FBDATA(cmask)->abs_y == 0);

	  draw_rect.x = GDK_DRAWABLE_FBDATA(drawable)->abs_x + GDK_DRAWABLE_FBDATA(cmask)->llim_x + GDK_GC_FBDATA(gc)->values.clip_x_origin;
	  draw_rect.y = GDK_DRAWABLE_FBDATA(drawable)->abs_y + GDK_DRAWABLE_FBDATA(cmask)->llim_y + GDK_GC_FBDATA(gc)->values.clip_y_origin;
	  draw_rect.width = GDK_DRAWABLE_P(cmask)->width;
	  draw_rect.height = GDK_DRAWABLE_P(cmask)->height;

	  tmpreg = gdk_region_rectangle(&draw_rect);
	  gdk_region_intersect(real_clip_region, tmpreg);
	  gdk_region_destroy(tmpreg);
	}
    }

  return real_clip_region;
}

static void
gdk_fb_fill_span(GdkDrawable *drawable, GdkGC *gc, GdkSegment *cur, guint pixel, GdkVisual *visual)
{
  int curx, cury;
  guchar *mem = GDK_DRAWABLE_FBDATA(drawable)->mem;
  guint rowstride = GDK_DRAWABLE_FBDATA(drawable)->rowstride;
  guint depth = GDK_DRAWABLE_P(drawable)->depth;

  if(gc
     && (GDK_GC_FBDATA(gc)->values.clip_mask
	 || GDK_GC_FBDATA(gc)->values.tile
	 || GDK_GC_FBDATA(gc)->values.stipple))
    {
      int clipxoff, clipyoff; /* Amounts to add to curx & cury to get x & y in clip mask */
      int tsxoff, tsyoff;
      GdkDrawable *cmask;
      guchar *clipmem;
      guint mask_rowstride;
      GdkPixmap *ts = NULL;
      gboolean solid_stipple;

      cmask = GDK_GC_FBDATA(gc)->values.clip_mask;
      if(cmask)
	{
	  clipmem = GDK_DRAWABLE_FBDATA(cmask)->mem;
	  clipxoff = GDK_DRAWABLE_FBDATA(cmask)->abs_x - GDK_GC_FBDATA(gc)->values.clip_x_origin - GDK_DRAWABLE_FBDATA(drawable)->abs_x;
	  clipyoff = GDK_DRAWABLE_FBDATA(cmask)->abs_y - GDK_GC_FBDATA(gc)->values.clip_y_origin - GDK_DRAWABLE_FBDATA(drawable)->abs_y;
	  mask_rowstride = GDK_DRAWABLE_FBDATA(cmask)->rowstride;
	}

      if(GDK_GC_FBDATA(gc)->values.fill == GDK_TILED
	 && GDK_GC_FBDATA(gc)->values.tile)
	{
	  gint xstep, ystep;
	  gint relx, rely;

	  ts = GDK_GC_FBDATA(gc)->values.tile;
	  for(cury = cur->y1; cury < cur->y2; cury += ystep)
	    {
	      int drawh;
	      
	      rely = cury - GDK_DRAWABLE_FBDATA(drawable)->abs_y;
	      drawh = (rely + GDK_GC_FBDATA(gc)->values.ts_y_origin) % GDK_DRAWABLE_P(ts)->height;
	      if(drawh < 0)
		drawh += GDK_DRAWABLE_P(ts)->height;

	      ystep = MIN(GDK_DRAWABLE_P(ts)->height - drawh, cur->y2 - rely);

	      for(curx = cur->x1; curx < cur->x2; curx += xstep)
		{
		  int draww;

		  relx = curx - GDK_DRAWABLE_FBDATA(drawable)->abs_x;

		  draww = (relx + GDK_GC_FBDATA(gc)->values.ts_x_origin) % GDK_DRAWABLE_P(ts)->width;
		  if(draww < 0)
		    draww += GDK_DRAWABLE_P(ts)->width;

		  xstep = MIN(GDK_DRAWABLE_P(ts)->width - draww, cur->x2 - relx);

		  gdk_fb_draw_drawable_2(drawable, gc, ts,
					 draww, drawh,
					 relx, rely,
					 xstep, ystep, FALSE, TRUE);
		}
	    }

	  return;
	}
      else if((GDK_GC_FBDATA(gc)->values.fill == GDK_STIPPLED
	      || GDK_GC_FBDATA(gc)->values.fill == GDK_OPAQUE_STIPPLED)
	      && GDK_GC_FBDATA(gc)->values.stipple)
	{
	  ts = GDK_GC_FBDATA(gc)->values.stipple;
	  tsxoff = GDK_DRAWABLE_FBDATA(ts)->abs_x - GDK_GC_FBDATA(gc)->values.ts_x_origin - GDK_DRAWABLE_FBDATA(drawable)->abs_x;
	  tsyoff = GDK_DRAWABLE_FBDATA(ts)->abs_y - GDK_GC_FBDATA(gc)->values.ts_y_origin - GDK_DRAWABLE_FBDATA(drawable)->abs_y;
	  solid_stipple = (GDK_GC_FBDATA(gc)->values.fill == GDK_OPAQUE_STIPPLED);
	}

      switch(depth)
	{
	case 1:
	  g_assert(!ts);
	  for(cury = cur->y1; cury < cur->y2; cury++)
	    {
	      for(curx = cur->x1; curx < cur->x2; curx++)
		{
		  guchar *ptr = mem + (cury * rowstride) + (curx >> 3);
		  int maskx = curx+clipxoff, masky = cury + clipyoff;
		  guchar foo;

		  if(cmask)
		    {
		      foo = clipmem[masky*mask_rowstride + (maskx >> 3)];

		      if(!(foo & (1 << (maskx % 8))))
			continue;
		    }

		  *ptr |= (1 << (curx % 8));
		}
	    }
	  break;
	case 8:
	  for(cury = cur->y1; cury < cur->y2; cury++)
	    {
	      for(curx = cur->x1; curx < cur->x2; curx++)
		{
		  guchar *ptr = mem + (cury * rowstride) + curx;
		  int maskx = curx+clipxoff, masky = cury + clipyoff;
		  guchar foo;

		  if(cmask)
		    {
		      foo = clipmem[masky*mask_rowstride + (maskx >> 3)];

		      if(!(foo & (1 << (maskx % 8))))
			continue;
		    }

		  if(ts)
		    {
		      int wid = GDK_DRAWABLE_P(ts)->width, hih = GDK_DRAWABLE_P(ts)->height;
		      maskx = (curx+tsxoff)%wid;
		      masky = (cury+tsyoff)%hih;
		      if(maskx < 0)
			maskx += wid;
		      if(masky < 0)
			masky += hih;

		      foo = GDK_DRAWABLE_FBDATA(ts)->mem[(maskx >> 3) + GDK_DRAWABLE_FBDATA(ts)->rowstride*masky];
		      if(foo & (1 << (maskx % 8)))
			{
			  pixel = GDK_GC_FBDATA(gc)->values.foreground.pixel;
			}
		      else if(solid_stipple)
			{
			  pixel = GDK_GC_FBDATA(gc)->values.background.pixel;
			}
		      else
			continue;
		    }

		  *ptr = pixel;
		}
	    }
	  break;

	case 16:
	case 24:
	case 32:
	  for(cury = cur->y1; cury < cur->y2; cury++)
	    {
	      for(curx = cur->x1; curx < cur->x2; curx++)
		{
		  guint *ptr2 = (guint *)(mem + (cury * rowstride) + (curx * (depth >> 3)));
		  int maskx = curx+clipxoff, masky = cury + clipyoff;
		  guchar foo;

		  if(cmask)
		    {
		      foo = clipmem[masky*mask_rowstride + (maskx >> 3)];

		      if(!(foo & (1 << (maskx % 8))))
			continue;
		    }

		  if(ts)
		    {
		      int wid = GDK_DRAWABLE_P(ts)->width, hih = GDK_DRAWABLE_P(ts)->height;

		      maskx = (curx+tsxoff)%wid;
		      masky = (cury+tsyoff)%hih;
		      if(maskx < 0)
			maskx += wid;
		      if(masky < 0)
			masky += hih;

		      foo = GDK_DRAWABLE_FBDATA(ts)->mem[(maskx >> 3) + GDK_DRAWABLE_FBDATA(ts)->rowstride*masky];
		      if(foo & (1 << (maskx % 8)))
			{
			  pixel = GDK_GC_FBDATA(gc)->values.foreground.pixel;
			}
		      else if(solid_stipple)
			{
			  pixel = GDK_GC_FBDATA(gc)->values.background.pixel;
			}
		      else
			continue;
		    }

		  *ptr2 = (*ptr2 & ~(visual->red_mask|visual->green_mask|visual->blue_mask)) | pixel;
		}
	    }
	  break;
	}
    }
  else
    {
      switch(depth)
	{
	case 1:
	  for(cury = cur->y1; cury < cur->y2; cury++)
	    {
	      for(curx = cur->x1; curx < cur->x2; curx++)
		{
		  guchar *ptr = mem + (cury * rowstride) + (curx >> 3);

		  if(pixel)
		    *ptr |= (1 << (curx % 8));
		  else
		    *ptr &= ~(1 << (curx % 8));
		}
	    }
	  break;

	case 8:
	  for(cury = cur->y1; cury < cur->y2; cury++)
	    {
	      guchar *ptr = mem + (cury * rowstride) + cur->x1;
	      memset(ptr, pixel, cur->x2 - cur->x1);
	    }
	  break;

	case 16:
	case 24:
	case 32:
	  for(cury = cur->y1; cury < cur->y2; cury++)
	    {
	      for(curx = cur->x1; curx < cur->x2; curx++)
		{
		  guint *ptr2 = (guint *)(mem + (cury * rowstride) + (curx * (depth >> 3)));

		  *ptr2 = (*ptr2 & ~(visual->red_mask|visual->green_mask|visual->blue_mask)) | pixel;
		}
	    }
	  break;
	}
    }
}

void
gdk_fb_fill_spans(GdkDrawable *drawable,
		  GdkGC *gc,
		  GdkRectangle *rects, int nrects)
{
  int i;
  guint pixel;
  GdkRegion *real_clip_region, *tmpreg;
  GdkRectangle draw_rect, cursor_rect;
  GdkVisual *visual = gdk_visual_get_system();
  gboolean handle_cursor = FALSE;

  if(GDK_IS_WINDOW(drawable) && !GDK_WINDOW_P(drawable)->mapped)
    return;
  if(GDK_IS_WINDOW(drawable) && GDK_WINDOW_P(drawable)->input_only)
    g_error("Drawing on the evil input-only!");

  if(gc)
    pixel = GDK_GC_FBDATA(gc)->values.foreground.pixel;
  else if(GDK_IS_WINDOW(drawable))
    pixel = GDK_WINDOW_P(drawable)->bg_color.pixel;
  else
    pixel = 0;

  real_clip_region = gdk_fb_clip_region(drawable, gc, TRUE);

  gdk_fb_get_cursor_rect(&cursor_rect);
  if(GDK_DRAWABLE_FBDATA(drawable)->mem == GDK_DRAWABLE_FBDATA(gdk_parent_root)->mem
     && cursor_rect.x >= 0
     && gdk_region_rect_in(real_clip_region, &cursor_rect) != GDK_OVERLAP_RECTANGLE_OUT)
    {
      handle_cursor = TRUE;
      gdk_fb_cursor_hide();
    }
    
  for(i = 0; i < nrects; i++)
    {
      GdkSegment cur;
      int j;

      cur.x1 = rects[i].x;
      cur.y1 = rects[i].y;
      cur.x2 = cur.x1 + rects[i].width;
      cur.y2 = cur.y1 + rects[i].height;
      g_assert(cur.x2 >= cur.x1);
      g_assert(cur.y2 >= cur.y1);

      cur.x1 += GDK_DRAWABLE_FBDATA(drawable)->abs_x;
      cur.x1 = MAX(cur.x1, GDK_DRAWABLE_FBDATA(drawable)->llim_x);

      cur.x2 += GDK_DRAWABLE_FBDATA(drawable)->abs_x;
      cur.x2 = MIN(cur.x2, GDK_DRAWABLE_FBDATA(drawable)->lim_x);
      cur.x1 = MIN(cur.x1, cur.x2);

      cur.y1 += GDK_DRAWABLE_FBDATA(drawable)->abs_y;
      cur.y1 = MAX(cur.y1, GDK_DRAWABLE_FBDATA(drawable)->llim_y);

      cur.y2 += GDK_DRAWABLE_FBDATA(drawable)->abs_y;
      cur.y2 = MIN(cur.y2, GDK_DRAWABLE_FBDATA(drawable)->lim_y);
      cur.y1 = MIN(cur.y1, cur.y2);

      draw_rect.x = cur.x1;
      draw_rect.y = cur.y1;
      draw_rect.width = cur.x2 - cur.x1;
      draw_rect.height = cur.y2 - cur.y1;

      switch(gdk_region_rect_in(real_clip_region, &draw_rect))
	{
	case GDK_OVERLAP_RECTANGLE_PART:
	  tmpreg = gdk_region_rectangle(&draw_rect);
	  gdk_region_intersect(tmpreg, real_clip_region);
	  for(j = 0; j < tmpreg->numRects; j++)
	    gdk_fb_fill_span(drawable, gc, &tmpreg->rects[j], pixel, visual);
	  gdk_region_destroy(tmpreg);
	  break;
	case GDK_OVERLAP_RECTANGLE_IN:
	  gdk_fb_fill_span(drawable, gc, &cur, pixel, visual);
	  break;
	default:
	  break;
	}
    }

  gdk_region_destroy(real_clip_region);
  if(handle_cursor)
    gdk_fb_cursor_unhide();
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
  GdkRegion *real_clip_region, *tmpreg;
  GdkRectangle rect, cursor_rect;
  int i;
  int src_x_off, src_y_off;
  int clipxoff, clipyoff;
  guchar *mem = GDK_DRAWABLE_FBDATA(drawable)->mem, *srcmem = GDK_DRAWABLE_FBDATA(src)->mem;
  guchar *clipmem;
  guint rowstride = GDK_DRAWABLE_FBDATA(drawable)->rowstride, src_rowstride = GDK_DRAWABLE_FBDATA(src)->rowstride;
  guint clip_rowstride;
  GdkDrawableFBData *fbd;
  gboolean handle_cursor = FALSE;
  GdkPixmap *bgpm = NULL;
  GdkWindow *bg_relto = drawable;

  if(GDK_IS_WINDOW(drawable) && !GDK_WINDOW_P(drawable)->mapped)
    return;
  if(GDK_IS_WINDOW(drawable) && GDK_WINDOW_P(drawable)->input_only)
    g_error("Drawing on the evil input-only!");

  if(GDK_IS_WINDOW(drawable))
    {
      bgpm = GDK_WINDOW_P(drawable)->bg_pixmap;
      if(bgpm == GDK_PARENT_RELATIVE_BG)
	{
	  for(; bgpm == GDK_PARENT_RELATIVE_BG && bg_relto; bg_relto = GDK_WINDOW_P(bg_relto)->parent)
	    bgpm = GDK_WINDOW_P(bg_relto)->bg_pixmap;
	}

      if(bgpm == GDK_NO_BG)
	bgpm = NULL;

      if(bgpm)
	g_assert(GDK_DRAWABLE_P(bgpm)->depth == 8);
    }

  if(drawable == src)
    {
      GdkDrawableFBData *fbd = GDK_DRAWABLE_FBDATA(src);
      /* One lame hack deserves another ;-) */
      srcmem = g_alloca(fbd->rowstride * fbd->lim_y);
      memcpy(srcmem, mem, fbd->rowstride * fbd->lim_y);
    }

  real_clip_region = gdk_fb_clip_region(drawable, gc, do_clipping);
  rect.x = xdest + GDK_DRAWABLE_FBDATA(drawable)->abs_x;
  rect.y = ydest + GDK_DRAWABLE_FBDATA(drawable)->abs_y;
  rect.width = width;
  rect.height = height;
  tmpreg = gdk_region_rectangle(&rect);
  gdk_region_intersect(real_clip_region, tmpreg);
  gdk_region_destroy(tmpreg);

  rect.x = xdest + GDK_DRAWABLE_FBDATA(drawable)->abs_x;
  rect.y = ydest + GDK_DRAWABLE_FBDATA(drawable)->abs_y;
  rect.width = MAX(GDK_DRAWABLE_P(src)->width - xsrc, 0);
  rect.height = MAX(GDK_DRAWABLE_P(src)->height - ysrc, 0);
  if(!rect.width || !rect.height)
    goto out;
  tmpreg = gdk_region_rectangle(&rect);
  gdk_region_intersect(real_clip_region, tmpreg);
  gdk_region_destroy(tmpreg);

  src_x_off = (GDK_DRAWABLE_FBDATA(src)->abs_x + xsrc) - (GDK_DRAWABLE_FBDATA(drawable)->abs_x + xdest);
  src_y_off = (GDK_DRAWABLE_FBDATA(src)->abs_y + ysrc) - (GDK_DRAWABLE_FBDATA(drawable)->abs_y + ydest);
  clipxoff = - GDK_DRAWABLE_FBDATA(drawable)->abs_x;
  clipyoff = - GDK_DRAWABLE_FBDATA(drawable)->abs_y;
  if(gc)
    {
      clipxoff -= GDK_GC_FBDATA(gc)->values.clip_x_origin;
      clipyoff -= GDK_GC_FBDATA(gc)->values.clip_y_origin;

      if(GDK_GC_FBDATA(gc)->values.clip_mask)
	{
	  fbd = GDK_DRAWABLE_FBDATA(GDK_GC_FBDATA(gc)->values.clip_mask);
	  clipmem = GDK_DRAWABLE_FBDATA(GDK_GC_FBDATA(gc)->values.clip_mask)->mem;
	  clip_rowstride = GDK_DRAWABLE_FBDATA(GDK_GC_FBDATA(gc)->values.clip_mask)->rowstride;
	}
    }

  gdk_fb_get_cursor_rect(&cursor_rect);
  if(do_clipping
     && GDK_DRAWABLE_FBDATA(drawable)->mem == GDK_DRAWABLE_FBDATA(gdk_parent_root)->mem
     && cursor_rect.x >= 0
     && gdk_region_rect_in(real_clip_region, &cursor_rect) != GDK_OVERLAP_RECTANGLE_OUT)
    {
      handle_cursor = TRUE;
      gdk_fb_cursor_hide();
    }

  for(i = 0; i < real_clip_region->numRects; i++)
    {
      GdkRegionBox *cur = &real_clip_region->rects[i];
      int cur_y;

      if(GDK_DRAWABLE_P(src)->depth == GDK_DRAWABLE_P(drawable)->depth
	 && GDK_DRAWABLE_P(src)->depth > 1
	 && (!gc || !GDK_GC_FBDATA(gc)->values.clip_mask))
	{
	  guint depth = GDK_DRAWABLE_P(src)->depth;

	  for(cur_y = cur->y1; cur_y < cur->y2; cur_y++)
	    {
	      memcpy(mem + (cur_y * rowstride) + cur->x1*(depth>>3),
		     srcmem + ((cur_y + src_y_off)*src_rowstride) + (cur->x1 + src_x_off)*(depth>>3),
		     (cur->x2 - cur->x1)*(depth>>3));
	    }
	}
      else
	{
	  int cur_x;

	  for(cur_y = cur->y1; cur_y < cur->y2; cur_y++)
	    {
	      for(cur_x = cur->x1; cur_x < cur->x2; cur_x++)
		{
		  guint pixel;
		  guint16 *p16;
		  guint32 *p32;

		  if(gc && GDK_GC_FBDATA(gc)->values.clip_mask)
		    {
		      int maskx = cur_x+clipxoff, masky = cur_y + clipyoff;
		      guchar foo;

		      if(maskx < 0 || masky < 0)
			continue;

		      foo = clipmem[masky*clip_rowstride + (maskx >> 3)];

		      if(!(foo & (1 << (maskx % 8))))
			continue;
		    }

		  switch(GDK_DRAWABLE_P(src)->depth)
		    {
		    case 1:
		      {
			guchar foo = srcmem[((cur_x + src_x_off) >> 3) + ((cur_y + src_y_off)*src_rowstride)];

			if(foo & (1 << ((cur_x + src_x_off) % 8))) {
			  pixel = GDK_GC_FBDATA(gc)->values.foreground.pixel;
			} else if(draw_bg) {
			  if(bgpm)
			    {
			      int bgx, bgy;

			      bgx = (cur_x - GDK_DRAWABLE_FBDATA(bg_relto)->abs_x) % GDK_DRAWABLE_P(bgpm)->width;
			      bgy = (cur_y - GDK_DRAWABLE_FBDATA(bg_relto)->abs_y) % GDK_DRAWABLE_P(bgpm)->height;

			      pixel = GDK_DRAWABLE_FBDATA(bgpm)->mem[bgx + bgy * GDK_DRAWABLE_FBDATA(bgpm)->rowstride];
			    }
			  else
			    pixel = GDK_GC_FBDATA(gc)->values.background.pixel;
			} else
			  continue;
		      }
		      break;
		    case 8:
		      pixel = srcmem[(cur_x + src_x_off) + ((cur_y + src_y_off)*src_rowstride)];
		      break;
		    case 16:
		      pixel = *(guint16 *)(srcmem + (cur_x + src_x_off)*2 + ((cur_y + src_y_off)*src_rowstride));
		      break;
		    case 24:
		      pixel = 0x00FFFFFF & *(guint32 *)(srcmem + (cur_x + src_x_off)*3 + ((cur_y + src_y_off)*src_rowstride));
		      break;
		    case 32:
		      pixel = *(guint32 *)(srcmem + (cur_x + src_x_off)*4 + ((cur_y + src_y_off)*src_rowstride));
		      break;
		    default:
		      g_assert_not_reached();
		      break;
		    }

		  switch(GDK_DRAWABLE_P(drawable)->depth)
		    {
		    case 1:
		      {
			guchar *foo = mem + (cur_y*src_rowstride) + (cur_x >> 3);

			if(pixel == GDK_GC_FBDATA(gc)->values.foreground.pixel)
			  *foo |= (1 << (cur_x % 8));
			else
			  *foo &= ~(1 << (cur_x % 8));
		      }
		      break;
		    case 8:
		      mem[cur_x + cur_y*rowstride] = pixel;
		      break;
		    case 16:
		      p16 = (guint16 *)&mem[cur_x*2 + cur_y*rowstride];
		      *p16 = pixel;
		      break;
		    case 24:
		      p32 = (guint32 *)&mem[cur_x*3 + cur_y*rowstride];
		      *p32 = (*p32 & 0xFF000000) | pixel;
		      break;
		    case 32:
		      p32 = (guint32 *)&mem[cur_x*4 + cur_y*rowstride];
		      *p32 = pixel;
		      break;
		    default:
		      g_assert_not_reached();
		      break;
		    }
		}
	    }
	}
    }

 out:
  gdk_region_destroy(real_clip_region);

  if(handle_cursor)
    gdk_fb_cursor_unhide();
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
  gdk_fb_draw_drawable_2(drawable, gc, src, xsrc, ysrc, xdest, ydest, width, height, TRUE, TRUE);
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
  GLYPH *g;
  GdkDrawablePrivate tmp_pixmap;
  GdkDrawableFBData fbd;

  if(GDK_IS_WINDOW(drawable) && !GDK_WINDOW_P(drawable)->mapped)
    return;

  y -= font->ascent; /* y is relative to baseline, we want it relative to top left corner */

  g = T1_SetString(GDK_FONT_FB(font)->t1_font_id, (char *)text, text_length, 0, T1_KERNING, GDK_FONT_FB(font)->size,
		   NULL);
  g_assert(g);

  tmp_pixmap.window_type = GDK_DRAWABLE_PIXMAP;
  tmp_pixmap.width = (g->metrics.rightSideBearing - g->metrics.leftSideBearing);
  tmp_pixmap.height = (g->metrics.ascent - g->metrics.descent);
  tmp_pixmap.depth = 1;

  fbd.mem = g->bits;
  fbd.abs_x = fbd.abs_y = fbd.llim_x = fbd.llim_y = 0;
  fbd.lim_x = tmp_pixmap.width;
  fbd.lim_y = tmp_pixmap.height;
  fbd.rowstride = (tmp_pixmap.width + 7) / 8;
  tmp_pixmap.klass_data = &fbd;
  tmp_pixmap.klass = &_gdk_fb_drawable_class;

  gdk_fb_draw_drawable_2(drawable, gc, (GdkPixmap *)&tmp_pixmap, 0, 0, x, y, tmp_pixmap.width, tmp_pixmap.height, FALSE, TRUE);
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
  char *realbuf;
  int i;

  if(GDK_IS_WINDOW(drawable) && (!GDK_WINDOW_P(drawable)->mapped || GDK_WINDOW_P(drawable)->input_only))
    return;

  /* A hack, a hack, a pretty little hack */
  realbuf = alloca(text_length + 1);
  for(i = 0; i < text_length; i++)
    realbuf[i] = text[i];
  realbuf[i] = 0;

  gdk_fb_draw_text(drawable, font, gc, x, y, realbuf, text_length);
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
  GdkRectangle rect;

  if(filled)
    {
#if 0
      static volatile int print_rect = 0;

      if(print_rect)
	{
	  fprintf(debug_out, "[%d, %d] +[%d, %d]\n", x, y, width, height);
	  if(y < 0)
	    G_BREAKPOINT();
	}
#endif

      rect.x = x;
      rect.y = y;
      rect.width = width;
      rect.height = height;
      gdk_fb_fill_spans(drawable, gc, &rect, 1);
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
      gdk_fb_draw_lines(drawable, gc, pts, 5);
    }
}

static void gdk_fb_draw_points    (GdkDrawable    *drawable,
				   GdkGC          *gc,
				   GdkPoint       *points,
				   gint            npoints)
{
  GdkRectangle *rects = alloca(npoints * sizeof(GdkRectangle));
  int i;

  for(i = 0; i < npoints; i++)
    {
      rects[i].x = points[i].x;
      rects[i].y = points[i].y;
      rects[i].width = rects[i].height = 1;
    }

  gdk_fb_fill_spans(drawable, gc, rects, npoints);
}

static void gdk_fb_draw_arc       (GdkDrawable    *drawable,
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

  if(filled)
    miPolyFillArc(drawable, gc, 1, &arc);
  else
    miPolyArc(drawable, gc, 1, &arc);
}

static void gdk_fb_draw_polygon   (GdkDrawable    *drawable,
				   GdkGC          *gc,
				   gint            filled,
				   GdkPoint       *points,
				   gint            npoints)
{
  if(filled)
    miFillPolygon(drawable, gc, 0, 0, npoints, points);
  else
    {
      GdkPoint *realpts = alloca(sizeof(GdkPoint) * (npoints + 1));

      memcpy(realpts, points, sizeof(GdkPoint) * npoints);
      realpts[npoints] = points[0];
      gdk_fb_draw_lines(drawable, gc, points, npoints);
    }
}

static void gdk_fb_draw_lines     (GdkDrawable    *drawable,
				   GdkGC          *gc,
				   GdkPoint       *points,
				   gint            npoints)
{
  if(GDK_GC_FBDATA(gc)->values.line_width > 0)
    miWideLine(drawable, gc, 0, npoints, points);
  else
    miZeroLine(drawable, gc, 0, npoints, points);
}

static void gdk_fb_draw_segments  (GdkDrawable    *drawable,
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

      gdk_fb_draw_lines(drawable, gc, pts, 2);
    }
}

void
gdk_fb_drawable_clear(GdkDrawable *d)
{
  _gdk_windowing_window_clear_area(d, 0, 0, GDK_DRAWABLE_P(d)->width, GDK_DRAWABLE_P(d)->height);
}
