#include "gdkprivate-fb.h"
#include "mi.h"

/* #define USE_FTGRAYS */
#define USE_AA
#include <freetype/ftglyph.h>

#include <endian.h>
#ifndef __BYTE_ORDER
#error "endian.h needs to #define __BYTE_ORDER"
#endif

#ifndef g_alloca
#define g_alloca alloca
#endif

static void gdk_fb_drawable_set_pixel(GdkDrawable *drawable, GdkGC *gc, int x, int y, GdkColor *spot, gboolean abs_coords);

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
static void gdk_fb_draw_glyphs(GdkDrawable      *drawable,
			       GdkGC	           *gc,
			       PangoFont        *font,
			       gint              x,
			       gint              y,
			       PangoGlyphString *glyphs);
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
  gdk_fb_draw_lines,
  gdk_fb_draw_glyphs
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
gdk_fb_fill_span(GdkDrawable *drawable, GdkGC *gc, GdkSegment *cur, GdkColor *color, GdkVisual *visual)
{
  int curx, cury;
  GdkColor spot = *color;

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
	  GdkFBDrawingContext *dc, dc_data;
	  dc = &dc_data;

	  gdk_fb_drawing_context_init(dc, drawable, gc, FALSE, TRUE);

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

		  gdk_fb_draw_drawable_3(drawable, gc, ts,
					 dc,
					 draww, drawh,
					 relx, rely,
					 xstep, ystep);
		}
	    }

	  gdk_fb_drawing_context_finalize(dc);

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

      for(cury = cur->y1; cury < cur->y2; cury++)
	{
	  for(curx = cur->x1; curx < cur->x2; curx++)
	    {
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
		      spot = GDK_GC_FBDATA(gc)->values.foreground;
		    }
		  else if(solid_stipple)
		    {
		      spot = GDK_GC_FBDATA(gc)->values.background;
		    }
		  else
		    continue;
		}

	      gdk_fb_drawable_set_pixel(drawable, gc, curx, cury, &spot, TRUE);
	    }
	}
    }
  else
    {
      guchar *mem = GDK_DRAWABLE_FBDATA(drawable)->mem, *ptr;
      guint rowstride = GDK_DRAWABLE_FBDATA(drawable)->rowstride;
      int n;

      switch(GDK_DRAWABLE_P(drawable)->depth)
	{
	case 1:
	  {
	    int fromx = MIN((cur->x1+7)&(~7), cur->x2);
	    int begn = fromx - cur->x1, begoff = cur->x1 % 8, endn;
	    guchar begmask, endmask;
	    int body_end = cur->x2 & ~7;
	    int body_len = (body_end - fromx)/8;

	    begmask = ((1 << (begn + begoff)) - 1)
	      & ~((1 << (begoff)) - 1);
	    endn = cur->x2 - body_end;
	    endmask = (1 << endn) - 1;

	    for(cury = cur->y1; cury < cur->y2; cury++)
	      {
		ptr = mem + cury*rowstride + (cur->x1 >> 3);

		if(spot.pixel)
		  *ptr |= begmask;
		else
		  *ptr &= ~begmask;

		curx = fromx;

		if(curx < cur->x2)
		  {
		    ptr = mem + cury*rowstride + (curx >> 3);
		    memset(ptr, spot.pixel?0xFF:0, body_len);

		    if(endn)
		      {
			ptr = mem + cury*rowstride + (body_end >> 3);
			if(spot.pixel)
			  *ptr |= endmask;
			else
			  *ptr &= ~endmask;
		      }
		  }
	      }
	  }
	  break;
	case 8:
	  for(cury = cur->y1; cury < cur->y2; cury++)
	    {
	      ptr = mem + cury*rowstride + cur->x1;
	      memset(ptr, spot.pixel, cur->x2 - cur->x1);
	    }
	  break;
	case 16:
	  {
	    int i;
	    n = cur->x2 - cur->x1;
	    for(cury = cur->y1; cury < cur->y2; cury++)
	      {
		guint16 *p16 = (guint16 *)(mem + cury * rowstride + cur->x1*2);
		for(i = 0; i < n; i++)
		  *(p16++) = spot.pixel;
	      }
	  }
	  break;
	case 24:
	  {
	    guchar redval = spot.red>>8, greenval=spot.green>>8, blueval=spot.blue>>8;
	    guchar *firstline, *ptr_end;

	    if((cur->y2 - cur->y1) <= 0)
	      break;

	    n = (cur->x2 - cur->x1)*3;

	    firstline = ptr = mem + cur->y1 * rowstride + cur->x1*3;
	    ptr_end = ptr+n;
	    while(ptr < ptr_end)
	      {
		ptr[gdk_display->red_byte] = redval;
		ptr[gdk_display->green_byte] = greenval;
		ptr[gdk_display->blue_byte] = blueval;
		ptr += 3;
	      }
	    for(cury = cur->y1 + 1, ptr = mem + cury * rowstride + cur->x1*3; cury < cur->y2; cury++, ptr += rowstride)
	      {
		memcpy(ptr, firstline, n);
	      }
	  }
	  break;
	case 32:
	  {
	    int i;
	    n = cur->x2 - cur->x1;
	    for(cury = cur->y1; cury < cur->y2; cury++)
	      {
		guint32 *p32 = (guint32 *)(mem + cury * rowstride + cur->x1*4);
		for(i = 0; i < n; i++)
		  *(p32++) = spot.pixel;
	      }
	  }
	  break;
	default:
	  g_assert_not_reached();
#if 0
	  for(cury = cur->y1; cury < cur->y2; cury++)
	    {
	      for(curx = cur->x1; curx < cur->x2; curx++)
		{
		  gdk_fb_drawable_set_pixel(drawable, gc, curx, cury, &spot, TRUE);
		}
	    }
#endif
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
  GdkColor color;
  GdkRegion *real_clip_region, *tmpreg;
  GdkRectangle draw_rect, cursor_rect;
  GdkVisual *visual = gdk_visual_get_system();
  gboolean handle_cursor = FALSE;

  if(GDK_IS_WINDOW(drawable) && !GDK_WINDOW_P(drawable)->mapped)
    return;
  if(GDK_IS_WINDOW(drawable) && GDK_WINDOW_P(drawable)->input_only)
    g_error("Drawing on the evil input-only!");

  if(gc && (GDK_GC_FBDATA(gc)->values_mask | GDK_GC_FOREGROUND))
    color = GDK_GC_FBDATA(gc)->values.foreground;
  else if(GDK_IS_WINDOW(drawable))
    color = GDK_WINDOW_P(drawable)->bg_color;
  else
    gdk_color_black(GDK_DRAWABLE_P(drawable)->colormap, &color);

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
	    {
	      cur = tmpreg->rects[j];
	      gdk_fb_fill_span(drawable, gc, &cur, &color, visual);
	    }
	  gdk_region_destroy(tmpreg);
	  break;
	case GDK_OVERLAP_RECTANGLE_IN:
	  gdk_fb_fill_span(drawable, gc, &cur, &color, visual);
	  break;
	default:
	  break;
	}
    }

  gdk_region_destroy(real_clip_region);
  if(handle_cursor)
    gdk_fb_cursor_unhide();
}

typedef enum { GPR_USED_BG, GPR_AA_GRAYVAL, GPR_NONE, GPR_ERR_BOUNDS } GetPixelRet;
static GetPixelRet
gdk_fb_drawable_get_pixel(GdkDrawable *drawable, GdkGC *gc, int x, int y, GdkColor *spot,
			  gboolean abs_coords, GdkDrawable *bg_relto, GdkDrawable *bgpm)
{
  GetPixelRet retval = GPR_NONE;
  guchar *mem = GDK_DRAWABLE_FBDATA(drawable)->mem;
  guint rowstride = GDK_DRAWABLE_FBDATA(drawable)->rowstride;

  if(!abs_coords)
    {
      x += GDK_DRAWABLE_FBDATA(drawable)->abs_x;
      y += GDK_DRAWABLE_FBDATA(drawable)->abs_y;
    }

  switch(GDK_DRAWABLE_P(drawable)->depth)
    {
    case 1:
      {
	guchar foo = mem[(x >> 3) + y * rowstride];
	if(foo & (1 << (x % 8)))
	  *spot = GDK_GC_FBDATA(gc)->values.foreground;
	else
	  {
	    retval = GPR_USED_BG;

	    if(bgpm)
	      {
		int bgx, bgy;

		bgx = (x - GDK_DRAWABLE_FBDATA(bg_relto)->abs_x) % GDK_DRAWABLE_P(bgpm)->width;
		bgy = (y - GDK_DRAWABLE_FBDATA(bg_relto)->abs_y) % GDK_DRAWABLE_P(bgpm)->height;

		gdk_fb_drawable_get_pixel(bgpm, gc, bgx, bgy, spot, FALSE, NULL, NULL);
		retval = GPR_USED_BG;
	      }
	    else
	      {
		*spot = GDK_GC_FBDATA(gc)->values.background;
	      }
	  }
      }
      break;
    case 71:
      if(mem[x + y * rowstride])
	*spot = GDK_GC_FBDATA(gc)->values.foreground;
      else
	*spot = GDK_GC_FBDATA(gc)->values.background;
      break;
    case 77:
      retval = GPR_AA_GRAYVAL;
      spot->pixel = mem[x + y * rowstride] << 1;
      spot->red = spot->green = spot->blue = spot->pixel << 8;
      break;
    case 78: /* AA mode */
      retval = GPR_AA_GRAYVAL;
      spot->pixel = mem[x + y * rowstride];
      spot->red = spot->green = spot->blue = spot->pixel << 8;
      break;
    case 8:
      spot->pixel = mem[x + y * rowstride];
      *spot = GDK_DRAWABLE_P(drawable)->colormap->colors[spot->pixel];
      break;
    case 16:
      {
	guint16 val16 = *((guint16 *)&mem[x*2 + y*rowstride]);

	spot->red = (((1<<gdk_display->modeinfo.red.length) - 1) & (val16 >> gdk_display->modeinfo.red.offset)) << (16 - gdk_display->modeinfo.red.length);
	spot->green = (((1<<gdk_display->modeinfo.green.length) - 1) & (val16 >> gdk_display->modeinfo.green.offset)) << (16 - gdk_display->modeinfo.green.length);
	spot->blue = (((1<<gdk_display->modeinfo.blue.length) - 1) & (val16 >> gdk_display->modeinfo.blue.offset)) << (16 - gdk_display->modeinfo.blue.length);

	spot->pixel = val16;
      }
      break;
    case 24:
      {
	guchar *smem = &mem[x*3 + y*rowstride];
	spot->red = smem[gdk_display->red_byte] << 8;
	spot->green = smem[gdk_display->green_byte] << 8;
	spot->blue = smem[gdk_display->blue_byte] << 8;
#if (__BYTE_ORDER == __BIG_ENDIAN)
	spot->pixel = (smem[0]<<16)|(smem[1]<<8)|smem[2];
#else
	spot->pixel = smem[0]|(smem[1]<<8)|(smem[2]<<16);
#endif
      }
      break;
    case 32:
      {
	guchar *smem = &mem[x*4 + y*rowstride];
	spot->red = smem[gdk_display->red_byte] << 8;
	spot->green = smem[gdk_display->green_byte] << 8;
	spot->blue = smem[gdk_display->blue_byte] << 8;
	spot->pixel = *(guint32 *)smem;
      }
      break;
    }

  return retval;
}

static void
gdk_fb_drawable_set_pixel(GdkDrawable *drawable, GdkGC *gc, int x, int y, GdkColor *spot,
			  gboolean abs_coords)
{
  guchar *mem = GDK_DRAWABLE_FBDATA(drawable)->mem;
  guint rowstride = GDK_DRAWABLE_FBDATA(drawable)->rowstride;

  if(!abs_coords)
    {
      x += GDK_DRAWABLE_FBDATA(drawable)->abs_x;
      y += GDK_DRAWABLE_FBDATA(drawable)->abs_y;
    }

  switch(GDK_DRAWABLE_P(drawable)->depth)
    {
    case 1:
      {
	guchar *foo = mem + (y*rowstride) + (x >> 3);

	if(spot->pixel)
	  *foo |= (1 << (x % 8));
	else
	  *foo &= ~(1 << (x % 8));
      }
      break;
    case 8:
      mem[x + y*rowstride] = spot->pixel;
      break;
    case 16:
      {
	guint16 *p16 = (guint16 *)&mem[x*2 + y*rowstride];
	*p16 = spot->pixel;
      }
      break;
    case 24:
      {
	guchar *smem = &mem[x*3 + y*rowstride];
	smem[gdk_display->red_byte] = spot->red >> 8;
	smem[gdk_display->green_byte] = spot->green >> 8;
	smem[gdk_display->blue_byte] = spot->blue >> 8;
      };
      break;
    case 32:
      {
	guint32 *smem = (guint32 *)&mem[x*4 + y*rowstride];
	*smem = spot->pixel;
      }
      break;
    default:
      g_assert_not_reached();
      break;
    }

}

void
gdk_fb_drawing_context_init(GdkFBDrawingContext *dc,
			    GdkDrawable *drawable,
			    GdkGC *gc,
			    gboolean draw_bg,
			    gboolean do_clipping)
{
  dc->mem = GDK_DRAWABLE_FBDATA(drawable)->mem;
  dc->rowstride = GDK_DRAWABLE_FBDATA(drawable)->rowstride;
  dc->handle_cursor = FALSE;
  dc->bgpm = NULL;
  dc->bg_relto = drawable;
  dc->draw_bg = draw_bg;

  if(GDK_IS_WINDOW(drawable))
    {
      dc->bgpm = GDK_WINDOW_P(drawable)->bg_pixmap;
      if(dc->bgpm == GDK_PARENT_RELATIVE_BG)
	{
	  for(; dc->bgpm == GDK_PARENT_RELATIVE_BG && dc->bg_relto; dc->bg_relto = GDK_WINDOW_P(dc->bg_relto)->parent)
	    dc->bgpm = GDK_WINDOW_P(dc->bg_relto)->bg_pixmap;
	}

      if(dc->bgpm == GDK_NO_BG)
	dc->bgpm = NULL;
    }
  dc->clipxoff = - GDK_DRAWABLE_FBDATA(drawable)->abs_x;
  dc->clipyoff = - GDK_DRAWABLE_FBDATA(drawable)->abs_y;

  dc->real_clip_region = gdk_fb_clip_region(drawable, gc, do_clipping);

  if(gc)
    {
      dc->clipxoff -= GDK_GC_FBDATA(gc)->values.clip_x_origin;
      dc->clipyoff -= GDK_GC_FBDATA(gc)->values.clip_y_origin;

      if(GDK_GC_FBDATA(gc)->values.clip_mask)
	{
	  dc->clipmem = GDK_DRAWABLE_FBDATA(GDK_GC_FBDATA(gc)->values.clip_mask)->mem;
	  dc->clip_rowstride = GDK_DRAWABLE_FBDATA(GDK_GC_FBDATA(gc)->values.clip_mask)->rowstride;
	}
    }

  if(do_clipping)
    {
      GdkRectangle cursor_rect;

      gdk_fb_get_cursor_rect(&cursor_rect);

      if(GDK_DRAWABLE_FBDATA(drawable)->mem == GDK_DRAWABLE_FBDATA(gdk_parent_root)->mem
	 && cursor_rect.x >= 0
	 && gdk_region_rect_in(dc->real_clip_region, &cursor_rect) != GDK_OVERLAP_RECTANGLE_OUT)
	{
	  dc->handle_cursor = TRUE;
	  gdk_fb_cursor_hide();
	}
    }
}

void
gdk_fb_drawing_context_finalize(GdkFBDrawingContext *dc)
{
  gdk_region_destroy(dc->real_clip_region);

  if(dc->handle_cursor)
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
  GdkFBDrawingContext *dc, dc_data;
  dc = &dc_data;

  gdk_fb_drawing_context_init(dc, drawable, gc, draw_bg, do_clipping);
  gdk_fb_draw_drawable_3(drawable, gc, src, dc, xsrc, ysrc, xdest, ydest, width, height);
  gdk_fb_drawing_context_finalize(dc);
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
  GdkRectangle rect;
  guchar *srcmem = GDK_DRAWABLE_FBDATA(src)->mem;
  int src_x_off, src_y_off;
  GdkRegion *tmpreg, *real_clip_region;
  int i;

  if(GDK_IS_WINDOW(drawable))
    {
      if(!GDK_WINDOW_P(drawable)->mapped)
	return;
      if(GDK_WINDOW_P(drawable)->input_only)
	g_error("Drawing on the evil input-only!");
    }

  if(drawable == src)
    {
      GdkDrawableFBData *fbd = GDK_DRAWABLE_FBDATA(src);

      /* One lame hack deserves another ;-) */
      srcmem = g_alloca(fbd->rowstride * fbd->lim_y);
      memcpy(srcmem, dc->mem, fbd->rowstride * fbd->lim_y);
    }

  /* Do some magic to avoid creating extra regions unnecessarily */
  tmpreg = dc->real_clip_region;

  rect.x = xdest + GDK_DRAWABLE_FBDATA(drawable)->abs_x;
  rect.y = ydest + GDK_DRAWABLE_FBDATA(drawable)->abs_y;
  rect.width = width;
  rect.height = height;
  real_clip_region = gdk_region_rectangle(&rect);
  gdk_region_intersect(real_clip_region, tmpreg);

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

  for(i = 0; i < real_clip_region->numRects; i++)
    {
      GdkRegionBox *cur = &real_clip_region->rects[i];
      int cur_y;

      if(GDK_DRAWABLE_P(src)->depth == GDK_DRAWABLE_P(drawable)->depth
	 && GDK_DRAWABLE_P(src)->depth >= 8
	 && GDK_DRAWABLE_P(src)->depth <= 32
	 && (!gc || !GDK_GC_FBDATA(gc)->values.clip_mask))
	{
	  guint depth = GDK_DRAWABLE_P(src)->depth;
	  guint src_rowstride = GDK_DRAWABLE_FBDATA(src)->rowstride;
	  int linelen = (cur->x2 - cur->x1)*(depth>>3);

	  for(cur_y = cur->y1; cur_y < cur->y2; cur_y++)
	    {
	      memcpy(dc->mem + (cur_y * dc->rowstride) + cur->x1*(depth>>3),
		     srcmem + ((cur_y + src_y_off)*src_rowstride) + (cur->x1 + src_x_off)*(depth>>3),
		     linelen);
	    }
	}
      else
	{
	  int cur_x;

	  for(cur_y = cur->y1; cur_y < cur->y2; cur_y++)
	    {
	      for(cur_x = cur->x1; cur_x < cur->x2; cur_x++)
		{
		  GdkColor spot;

		  if(gc && GDK_GC_FBDATA(gc)->values.clip_mask)
		    {
		      int maskx = cur_x+dc->clipxoff, masky = cur_y + dc->clipyoff;
		      guchar foo;

		      foo = dc->clipmem[masky*dc->clip_rowstride + (maskx >> 3)];

		      if(!(foo & (1 << (maskx % 8))))
			continue;
		    }

		  switch(gdk_fb_drawable_get_pixel(src, gc, cur_x + src_x_off, cur_y + src_y_off, &spot, TRUE, NULL, NULL))
		    {
		    case GPR_AA_GRAYVAL:
		      {
			GdkColor realspot, fg;
			guint graylevel = spot.pixel;
			gint tmp;

			if(GDK_DRAWABLE_P(drawable)->depth == 1)
			  {
			    if(spot.pixel > 192)
			      spot = GDK_GC_FBDATA(gc)->values.foreground;
			    else
			     spot = GDK_GC_FBDATA(gc)->values.background;
			    break;
			  }
			else
			  {
			    if(graylevel >= 254)
			      {
				spot = GDK_GC_FBDATA(gc)->values.foreground;
			      }
			    else if(graylevel <= 2)
			      {
				if(!dc->draw_bg)
				  continue;

				spot = GDK_GC_FBDATA(gc)->values.background;
			      }
			    else
			      {
				switch(gdk_fb_drawable_get_pixel(drawable, gc, cur_x, cur_y, &realspot, TRUE, dc->bg_relto, dc->bgpm))
				  {
				  case GPR_NONE:
				  case GPR_USED_BG:
				    break;
				  default:
				    g_assert_not_reached();
				    break;
				  }

				fg = GDK_GC_FBDATA(gc)->values.foreground;
				/* Now figure out what 'spot' should actually look like */
				fg.red >>= 8;
				fg.green >>= 8;
				fg.blue >>= 8;
				realspot.red >>= 8;
				realspot.green >>= 8;
				realspot.blue >>= 8;

				tmp = (fg.red - realspot.red) * graylevel;
				spot.red = realspot.red + ((tmp + (tmp >> 8) + 0x80) >> 8);
				spot.red <<= 8;

				tmp = (fg.green - realspot.green) * graylevel;
				spot.green = realspot.green + ((tmp + (tmp >> 8) + 0x80) >> 8);
				spot.green <<= 8;

				tmp = (fg.blue - realspot.blue) * graylevel;
				spot.blue = realspot.blue + ((tmp + (tmp >> 8) + 0x80) >> 8);
				spot.blue <<= 8;

				/* Now find the pixel for this thingie */
				switch(GDK_DRAWABLE_P(drawable)->depth)
				  {
				  case 8:
				    if(!gdk_colormap_alloc_color(GDK_DRAWABLE_P(drawable)->colormap, &spot, FALSE, TRUE))
				      {
					g_error("Can't allocate AA color!");
				      }
				    break;
				  case 16:
				    spot.pixel = (spot.red >> (16 - gdk_display->modeinfo.red.length)) << gdk_display->modeinfo.red.offset;
				    spot.pixel |= (spot.green >> (16 - gdk_display->modeinfo.green.length)) << gdk_display->modeinfo.green.offset;
				    spot.pixel |= (spot.blue >> (16 - gdk_display->modeinfo.blue.length)) << gdk_display->modeinfo.blue.offset;
				    break;
				  case 24:
				  case 32:
				    spot.pixel = ((spot.red & 0xFF00) << (gdk_display->modeinfo.red.offset - 8))
				      | ((spot.green & 0xFF00) << (gdk_display->modeinfo.green.offset - 8))
				      | ((spot.blue & 0xFF00) << (gdk_display->modeinfo.blue.offset - 8));
				    break;
				  }
			      }
			  }
		      }
		      break;
		    case GPR_USED_BG:
		      if(!dc->draw_bg)
			continue;
		      break;
		    case GPR_NONE:
		      break;
		    default:
		      g_assert_not_reached();
		      break;
		    }

		  gdk_fb_drawable_set_pixel(drawable, gc, cur_x, cur_y, &spot, GDK_DRAWABLE_P(src)->depth);
		}
	    }
	}
    }

 out:
  gdk_region_destroy(real_clip_region);
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
  g_warning("gdk_fb_draw_text NYI");
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
  g_warning("gdk_fb_draw_text_wc NYI");
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
  GdkRectangle *rects = g_alloca(npoints * sizeof(GdkRectangle));
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
      GdkPoint *realpts = g_alloca(sizeof(GdkPoint) * (npoints + 1));

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
  extern void
    _gdk_windowing_window_clear_area (GdkWindow *window,
				      gint       x,
				      gint       y,
				      gint       width,
				      gint       height);

  _gdk_windowing_window_clear_area(d, 0, 0, GDK_DRAWABLE_P(d)->width, GDK_DRAWABLE_P(d)->height);
}

extern FT_Library gdk_fb_ft_lib;

extern void pango_fb_font_set_size(PangoFont *font);

static void gdk_fb_draw_glyphs(GdkDrawable      *drawable,
			       GdkGC	        *gc,
			       PangoFont        *font,
			       gint              x,
			       gint              y,
			       PangoGlyphString *glyphs)
{
  int i;
  GdkPixmapFBData fbd;
  GdkDrawablePrivate tmp_foo;
  FT_Face ftf;
  int xpos;
  GdkFBDrawingContext fbdc;

  g_return_if_fail(font);

  gdk_fb_drawing_context_init(&fbdc, drawable, gc, FALSE, TRUE);

  ftf = PANGO_FB_FONT(font)->ftf;

  /* Fake its existence as a pixmap */
  memset(&tmp_foo, 0, sizeof(tmp_foo));
  memset(&fbd, 0, sizeof(fbd));
  tmp_foo.klass = &_gdk_fb_drawable_class;
  tmp_foo.klass_data = &fbd;
  tmp_foo.window_type = GDK_DRAWABLE_PIXMAP;

  pango_fb_font_set_size(font);
  for(i = xpos = 0; i < glyphs->num_glyphs; i++)
    {
      FT_GlyphSlot g;
      FT_Bitmap *renderme;
      int this_wid;

      FT_Load_Glyph(ftf, glyphs->glyphs[i].glyph, FT_LOAD_DEFAULT);
      g = ftf->glyph;

      if(g->format != ft_glyph_format_bitmap)
	{
	  FT_BitmapGlyph bgy;
	  int bdepth;
#ifdef USE_AA
#ifdef USE_FTGRAYS
	  bdepth = 256;
#else
	  bdepth = 128;
#endif
#else
	  bdepth = 0;
#endif

	  if(FT_Get_Glyph_Bitmap(ftf, glyphs->glyphs[i].glyph, 0, bdepth, NULL, &bgy))
	    continue;

	  renderme = &bgy->bitmap;
	}
      else
	renderme = &g->bitmap;

      fbd.drawable_data.mem = renderme->buffer;
      fbd.drawable_data.rowstride = renderme->pitch;
      tmp_foo.width = fbd.drawable_data.lim_x = renderme->width;
      tmp_foo.height = fbd.drawable_data.lim_y = renderme->rows;

      switch(renderme->pixel_mode)
	{
	case ft_pixel_mode_mono:
	  tmp_foo.depth = 1;
	  break;
	case ft_pixel_mode_grays:
#if defined(USE_FTGRAYS)
	  tmp_foo.depth = 78;
#else
	  tmp_foo.depth = 77;
#endif
	  break;
	default:
	  g_assert_not_reached();
	  break;
	}

      this_wid = (xpos + glyphs->glyphs[i].geometry.width)/PANGO_SCALE;
      gdk_fb_draw_drawable_3(drawable, gc, (GdkDrawable *)&tmp_foo,
			     &fbdc,
			     0, 0,
			     x + (xpos + glyphs->glyphs[i].geometry.x_offset)/PANGO_SCALE,
			     y + glyphs->glyphs[i].geometry.y_offset / PANGO_SCALE
			     + ((-ftf->glyph->metrics.horiBearingY) >> 6),
			     this_wid, renderme->rows);

      xpos += glyphs->glyphs[i].geometry.width;
    }

  gdk_fb_drawing_context_finalize(&fbdc);
}
