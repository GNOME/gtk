/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2000 Alexander Larsson
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
#include <string.h>
#include <signal.h>
#include <sys/time.h>

/*
 * Reading pixel values from a generic drawable.
 */

static GetPixelRet
gdk_fb_drawable_get_color (GdkDrawable *drawable,
			   GdkGC *gc,
			   int x,
			   int y,
			   GdkColor *spot)
{
  GetPixelRet retval = GPR_NONE;
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;

  switch (private->depth)
    {
    case 1:
      {
	guchar foo = mem[(x >> 3) + y * rowstride];
	if (foo & (1 << (x % 8)))
	  *spot = GDK_GC_FBDATA (gc)->values.foreground;
	else
	  {
	    retval = GPR_USED_BG;
	    *spot = GDK_GC_FBDATA (gc)->values.background;
	  }
      }
      break;
    case 71:
      if (mem[x + y * rowstride])
	*spot = GDK_GC_FBDATA (gc)->values.foreground;
      else
	*spot = GDK_GC_FBDATA (gc)->values.background;
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
      *spot = private->colormap->colors[spot->pixel];
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
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
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

/*************************************
 * gc->get_color() implementations
 *************************************/

static GetPixelRet
gdk_fb_get_color_1 (GdkDrawable *drawable,
		    GdkGC *gc,
		    int x,
		    int y,
		    GdkColor *color)
{
  GetPixelRet retval = GPR_NONE;
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;
  guchar foo;

  g_assert (private->depth == GDK_GC_FBDATA (gc)->depth);
  
  foo = mem[(x >> 3) + y * rowstride];
  if (foo & (1 << (x % 8)))
    *color = GDK_GC_FBDATA (gc)->values.foreground;
  else
    {
      retval = GPR_USED_BG;

      *color = GDK_GC_FBDATA (gc)->values.background;
    }
  
  return retval;
}

static GetPixelRet
gdk_fb_get_color_8 (GdkDrawable *drawable,
		    GdkGC *gc,
		    int x,
		    int y,
		    GdkColor *color)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;
  gint pixel;

  g_assert (private->depth == GDK_GC_FBDATA (gc)->depth);
  
  pixel = mem[x + y * rowstride];
  *color = private->colormap->colors[pixel];

  return GPR_NONE;
}

static GetPixelRet
gdk_fb_get_color_16 (GdkDrawable *drawable,
		     GdkGC *gc,
		     int x,
		     int y,
		     GdkColor *color)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;
  guint16 val16;

  g_assert (private->depth == GDK_GC_FBDATA (gc)->depth);

  val16 = *((guint16 *)&mem[x*2 + y*rowstride]);

  color->red = (((1<<gdk_display->modeinfo.red.length) - 1) & (val16 >> gdk_display->modeinfo.red.offset)) << (16 - gdk_display->modeinfo.red.length);
  color->green = (((1<<gdk_display->modeinfo.green.length) - 1) & (val16 >> gdk_display->modeinfo.green.offset)) << (16 - gdk_display->modeinfo.green.length);
  color->blue = (((1<<gdk_display->modeinfo.blue.length) - 1) & (val16 >> gdk_display->modeinfo.blue.offset)) << (16 - gdk_display->modeinfo.blue.length);

  color->pixel = val16;

  return GPR_NONE;
}

static GetPixelRet
gdk_fb_get_color_24 (GdkDrawable *drawable,
		     GdkGC *gc,
		     int x,
		     int y,
		     GdkColor *color)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;
  guchar *smem;

  g_assert (private->depth == GDK_GC_FBDATA (gc)->depth);

  smem = &mem[x*3 + y*rowstride];
  color->red = smem[gdk_display->red_byte] << 8;
  color->green = smem[gdk_display->green_byte] << 8;
  color->blue = smem[gdk_display->blue_byte] << 8;
#if (G_BYTE_ORDER == G_BIG_ENDIAN)
  color->pixel = (smem[0]<<16)|(smem[1]<<8)|smem[2];
#else
  color->pixel = smem[0]|(smem[1]<<8)|(smem[2]<<16);
#endif

  return GPR_NONE;
}

static GetPixelRet
gdk_fb_get_color_32 (GdkDrawable *drawable,
		     GdkGC *gc,
		     int x,
		     int y,
		     GdkColor *color)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;
  guchar *smem;

  g_assert (private->depth == GDK_GC_FBDATA (gc)->depth);
  
  smem = &mem[x*4 + y*rowstride];
  color->red = smem[gdk_display->red_byte] << 8;
  color->green = smem[gdk_display->green_byte] << 8;
  color->blue = smem[gdk_display->blue_byte] << 8;
  color->pixel = *(guint32 *)smem;

  return GPR_NONE;
}

/*************************************
 * gc->set_pixel() implementations
 *************************************/

static void
gdk_fb_set_pixel_1(GdkDrawable *drawable,
		   GdkGC *gc,
		   int x,
		   int y,
		   gulong pixel)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;
  guchar *ptr;

  if (!_gdk_fb_is_active_vt)
    return;

  g_assert (private->depth == GDK_GC_FBDATA (gc)->depth);
  
  ptr = mem + (y*rowstride) + (x >> 3);

  if (pixel)
    *ptr |= (1 << (x % 8));
  else
    *ptr &= ~(1 << (x % 8));
}

static void
gdk_fb_set_pixel_8(GdkDrawable *drawable,
		   GdkGC *gc,
		   int x,
		   int y,
		   gulong pixel)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;

  if (!_gdk_fb_is_active_vt)
    return;

  g_assert (private->depth == GDK_GC_FBDATA (gc)->depth);

  mem[x + y*rowstride] = pixel;
}

static void
gdk_fb_set_pixel_16(GdkDrawable *drawable,
		    GdkGC *gc,
		    int x,
		    int y,
		    gulong pixel)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;
  guint16 *ptr;

  if (!_gdk_fb_is_active_vt)
    return;

  g_assert (private->depth == GDK_GC_FBDATA (gc)->depth);
  
  ptr = (guint16 *)&mem[x*2 + y*rowstride];
  *ptr = pixel;
}

static void
gdk_fb_set_pixel_24(GdkDrawable *drawable,
		    GdkGC *gc,
		    int x,
		    int y,
		    gulong pixel)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;
  guchar *smem;
  
  if (!_gdk_fb_is_active_vt)
    return;

  g_assert (private->depth == GDK_GC_FBDATA (gc)->depth);
  
  smem = &mem[x*3 + y*rowstride];
  smem[0] = pixel & 0xff;
  smem[1] = (pixel >> 8) & 0xff;
  smem[2] = (pixel >> 16) & 0xff;
}

static void
gdk_fb_set_pixel_32(GdkDrawable *drawable,
		    GdkGC *gc,
		    int x,
		    int y,
		    gulong pixel)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  guchar *mem = private->mem;
  guint rowstride = private->rowstride;
  guint32 *smem;

  if (!_gdk_fb_is_active_vt)
    return;

  g_assert (private->depth == GDK_GC_FBDATA (gc)->depth);
  
  smem = (guint32 *)&mem[x*4 + y*rowstride];
  *smem = pixel;
}


/*************************************
 * gc->fill_span() implementations
 *************************************/

static void
gdk_fb_fill_span_generic (GdkDrawable *drawable,
			  GdkGC       *gc,
			  GdkSpan     *span,
			  GdkColor    *color)
{
  int curx;
  GdkColor spot = *color;
  GdkGCFBData *gc_private;
  GdkDrawableFBData *private;
  gint left, right, y;
  int clipxoff, clipyoff; /* Amounts to add to curx & cury to get x & y in clip mask */
  int tsxoff, tsyoff;
  GdkDrawable *cmask;
  guchar *clipmem;
  guint mask_rowstride;
  GdkPixmap *ts = NULL;
  GdkDrawableFBData *ts_private;
  gboolean solid_stipple;
  GdkFunction func;

  if (!_gdk_fb_is_active_vt)
    return;

  private = GDK_DRAWABLE_FBDATA (drawable);
  gc_private = GDK_GC_FBDATA (gc);

  g_assert (gc);

  y = span->y;
  left = span->x;
  right = span->x + span->width;
  
  func = gc_private->values.function;
    
  cmask = gc_private->values.clip_mask;
  clipxoff = clipyoff = tsxoff = tsyoff = 0;
  mask_rowstride = 0;
  solid_stipple = FALSE;
  clipmem = NULL;
  
  if (cmask)
    {
      GdkDrawableFBData *cmask_private;
      
      cmask_private = GDK_DRAWABLE_IMPL_FBDATA (cmask);
      
      clipmem = cmask_private->mem;
      clipxoff = cmask_private->abs_x - gc_private->values.clip_x_origin - private->abs_x;
      clipyoff = cmask_private->abs_y - gc_private->values.clip_y_origin - private->abs_y;
      mask_rowstride = cmask_private->rowstride;
    }
  
  if (gc_private->values.fill == GDK_TILED &&
      gc_private->values.tile)
    {
      gint xstep;
      gint relx, rely;
      int drawh;
      GdkFBDrawingContext *dc, dc_data;

      dc = &dc_data;
      
      gdk_fb_drawing_context_init (dc, drawable, gc, FALSE, TRUE);
      
      ts = gc_private->values.tile;
      ts_private = GDK_DRAWABLE_IMPL_FBDATA (ts);
      
      rely = y - private->abs_y;
      drawh = (rely - gc_private->values.ts_y_origin) % ts_private->height;
      if (drawh < 0)
	drawh += ts_private->height;
      
      for (curx = left; curx < right; curx += xstep)
	{
	  int draww;
	  
	  relx = curx - private->abs_x;
	  
	  draww = (relx - gc_private->values.ts_x_origin) % ts_private->width;
	  if (draww < 0)
	    draww += ts_private->width;
	  
	  xstep = MIN (ts_private->width - draww, right - relx);
	  
	  gdk_fb_draw_drawable_3 (drawable, gc, GDK_DRAWABLE_IMPL (ts),
				  dc,
				  draww, drawh,
				  relx, rely,
				  xstep, 1);
	}
      
      gdk_fb_drawing_context_finalize (dc);
      
      return;
    }
  else if ((gc_private->values.fill == GDK_STIPPLED ||
	    gc_private->values.fill == GDK_OPAQUE_STIPPLED) &&
	   gc_private->values.stipple)
    {
      ts = gc_private->values.stipple;
      tsxoff = - GDK_DRAWABLE_IMPL_FBDATA (ts)->abs_x - gc_private->values.ts_x_origin - private->abs_x;
      tsyoff = - GDK_DRAWABLE_IMPL_FBDATA (ts)->abs_y - gc_private->values.ts_y_origin - private->abs_y;
      solid_stipple = (gc_private->values.fill == GDK_OPAQUE_STIPPLED);
    }
  
  for (curx = left; curx < right; curx++)
    {
      int maskx = curx+clipxoff, masky = y + clipyoff;
      guchar foo;
      
      if (cmask)
	{
	  foo = clipmem[masky*mask_rowstride + (maskx >> 3)];
	  
	  if (!(foo & (1 << (maskx % 8))))
	    continue;
	}
      
      if (func == GDK_INVERT)
	{
	  (gc_private->get_color) (drawable, gc, curx, y, &spot);
	  spot.pixel = ~spot.pixel;
	  spot.red = ~spot.red;
	  spot.green = ~spot.green;
	  spot.blue = ~spot.blue;
	}
      else if (func == GDK_XOR)
	{
	  (gc_private->get_color) (drawable, gc, curx, y, &spot);
	  spot.pixel ^= gc_private->values.foreground.pixel;
	}
      else if (func != GDK_COPY)
	{
	  g_warning ("Unsupported GdkFunction %d\n", func);
	}
      else if (ts)
	{
	  int wid, hih;
	  
	  ts_private = GDK_DRAWABLE_IMPL_FBDATA (ts);
	  
	  wid = ts_private->width;
	  hih = ts_private->height;
	  
	  maskx = (curx+tsxoff)%wid;
	  masky = (y+tsyoff)%hih;
	  if (maskx < 0)
	    maskx += wid;
	  if (masky < 0)
	    masky += hih;
	  
	  foo = ts_private->mem[(maskx >> 3) + ts_private->rowstride*masky];
	  if (foo & (1 << (maskx % 8)))
	    {
	      spot = gc_private->values.foreground;
	    }
	  else if (solid_stipple)
	    {
	      spot = gc_private->values.background;
	    }
	  else
	    continue;
	}
      
      (gc_private->set_pixel) (drawable, gc, curx, y, spot.pixel);
    }
}

static void
gdk_fb_fill_span_simple_1 (GdkDrawable *drawable,
			   GdkGC       *gc,
			   GdkSpan     *span,
			   GdkColor    *color)
{
  int curx;
  GdkGCFBData *gc_private;
  GdkDrawableFBData *private;
  guchar *mem, *ptr;
  guint rowstride;
  gint left, right, y;

  if (!_gdk_fb_is_active_vt)
    return;

  private = GDK_DRAWABLE_FBDATA (drawable);
  gc_private = GDK_GC_FBDATA (gc);

  g_assert (gc);

  g_assert (!gc_private->values.clip_mask &&
	    !gc_private->values.tile &&
	    !gc_private->values.stipple &&
	    gc_private->values.function != GDK_INVERT);

  y = span->y;
  left = span->x;
  right = span->x + span->width;
  
  mem = private->mem;
  rowstride = private->rowstride;

  {
    int fromx = MIN ((left+7)&(~7), right);
    int begn = fromx - left, begoff = left % 8, endn;
    guchar begmask, endmask;
    int body_end = right & ~7;
    int body_len = (body_end - fromx)/8;
    
    begmask = ((1 << (begn + begoff)) - 1)
      & ~((1 << (begoff)) - 1);
    endn = right - body_end;
    endmask = (1 << endn) - 1;
    
    ptr = mem + y*rowstride + (left >> 3);
	
    if (color->pixel)
      *ptr |= begmask;
    else
      *ptr &= ~begmask;
    
    curx = fromx;
    
    if (curx < right)
      {
	ptr = mem + y*rowstride + (curx >> 3);
	memset (ptr, color->pixel?0xFF:0, body_len);
	
	if (endn)
	  {
	    ptr = mem + y*rowstride + (body_end >> 3);
	    if (color->pixel)
	      *ptr |= endmask;
	    else
	      *ptr &= ~endmask;
	  }
      }
  }
}

static void
gdk_fb_fill_span_simple_8 (GdkDrawable *drawable,
			   GdkGC       *gc,
			   GdkSpan     *span,
			   GdkColor    *color)
{
  GdkGCFBData *gc_private;
  GdkDrawableFBData *private;
  guchar *mem, *ptr;
  guint rowstride;

  if (!_gdk_fb_is_active_vt)
    return;

  private = GDK_DRAWABLE_FBDATA (drawable);
  gc_private = GDK_GC_FBDATA (gc);

  g_assert (gc);

  g_assert (!gc_private->values.clip_mask &&
	    !gc_private->values.tile &&
	    !gc_private->values.stipple &&
	    gc_private->values.function != GDK_INVERT);

  mem = private->mem;
  rowstride = private->rowstride;

  ptr = mem + span->y*rowstride + span->x;
  memset (ptr, color->pixel, span->width);
}

static void
gdk_fb_fill_span_simple_16 (GdkDrawable *drawable,
			    GdkGC       *gc,
			    GdkSpan     *span,
			    GdkColor    *color)
{
  GdkGCFBData *gc_private;
  GdkDrawableFBData *private;
  guchar *mem;
  guint rowstride;
  guint16 *p16;
  int n;
  int i;

  if (!_gdk_fb_is_active_vt)
    return;

  private = GDK_DRAWABLE_FBDATA (drawable);
  gc_private = GDK_GC_FBDATA (gc);

  g_assert (gc);

  g_assert (!gc_private->values.clip_mask &&
	    !gc_private->values.tile &&
	    !gc_private->values.stipple &&
	    gc_private->values.function != GDK_INVERT);

  mem = private->mem;
  rowstride = private->rowstride;

  n = span->width;
  p16 = (guint16 *)(mem + span->y * rowstride + span->x*2);
  for (i = 0; i < n; i++)
    *(p16++) = color->pixel;
}

static void
gdk_fb_fill_span_simple_24 (GdkDrawable *drawable,
			    GdkGC       *gc,
			    GdkSpan     *span,
			    GdkColor    *color)
{
  GdkGCFBData *gc_private;
  GdkDrawableFBData *private;
  guchar *mem, *ptr;
  guint rowstride;
  int n;
  guchar redval, greenval, blueval;
  guchar *firstline, *ptr_end;

  if (!_gdk_fb_is_active_vt)
    return;

  private = GDK_DRAWABLE_FBDATA (drawable);
  gc_private = GDK_GC_FBDATA (gc);

  g_assert (gc);

  g_assert (!gc_private->values.clip_mask &&
	    !gc_private->values.tile &&
	    !gc_private->values.stipple &&
	    gc_private->values.function != GDK_INVERT);

  mem = private->mem;
  rowstride = private->rowstride;

  redval = color->red>>8;
  greenval = color->green>>8;
  blueval = color->blue>>8;
    
  n = span->width*3;
    
  firstline = ptr = mem + span->y * rowstride + span->x*3;
  ptr_end = ptr+n;
  while (ptr < ptr_end)
    {
      ptr[gdk_display->red_byte] = redval;
      ptr[gdk_display->green_byte] = greenval;
      ptr[gdk_display->blue_byte] = blueval;
      ptr += 3;
    }
}

static void
gdk_fb_fill_span_simple_32 (GdkDrawable *drawable,
			    GdkGC       *gc,
			    GdkSpan     *span,
			    GdkColor    *color)
{
  GdkGCFBData *gc_private;
  GdkDrawableFBData *private;
  guchar *mem;
  guint rowstride;
  guint32 *p32;
  int n;
  int i;

  if (!_gdk_fb_is_active_vt)
    return;

  private = GDK_DRAWABLE_FBDATA (drawable);
  gc_private = GDK_GC_FBDATA (gc);

  g_assert (gc);

  g_assert (!gc_private->values.clip_mask &&
	    !gc_private->values.tile &&
	    !gc_private->values.stipple &&
	    gc_private->values.function != GDK_INVERT);

  mem = private->mem;
  rowstride = private->rowstride;

  n = span->width;
  p32 = (guint32 *)(mem + span->y * rowstride + span->x*4);
  for (i = 0; i < n; i++)
    *(p32++) = color->pixel;
}


/*************************************
 * gc->draw_drawable() implementations
 *************************************/

static void
gdk_fb_draw_drawable_generic (GdkDrawable *drawable,
			      GdkGC       *gc,
			      GdkPixmap   *src,
			      GdkFBDrawingContext *dc,
			      gint         start_y,
			      gint         end_y,
			      gint         start_x,
			      gint         end_x,
			      gint         src_x_off,
			      gint         src_y_off,
			      gint         draw_direction)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  int cur_x, cur_y;

  if (draw_direction < 0)
    {
      int tmp;
      tmp = start_y;
      start_y = end_y;
      end_y = tmp;
      start_y--;
      end_y--;
      
      tmp = start_x;
      start_x = end_x;
      end_x = tmp;
      start_x--;
      end_x--;
    }

  for (cur_y = start_y; cur_y != end_y; cur_y+=draw_direction)
    {
      for (cur_x = start_x; cur_x != end_x; cur_x+=draw_direction)
	{
	  GdkColor spot;
	  
	  if (GDK_GC_FBDATA(gc)->values.clip_mask)
	    {
	      int maskx = cur_x + dc->clipxoff, masky = cur_y + dc->clipyoff;
	      guchar foo;
	      
	      foo = dc->clipmem[masky*dc->clip_rowstride + (maskx >> 3)];
	      
	      if (!(foo & (1 << (maskx % 8))))
		continue;
	    }
	  
	  switch (gdk_fb_drawable_get_color (src, gc, cur_x + src_x_off, cur_y + src_y_off, &spot))
	    {
	    case GPR_AA_GRAYVAL:
	      {
		GdkColor realspot, fg;
		guint graylevel = spot.pixel;
		gint tmp;
		
		if (private->depth == 1)
		  {
		    if (spot.pixel > 192)
		      spot = GDK_GC_FBDATA (gc)->values.foreground;
		    else
		      spot = GDK_GC_FBDATA (gc)->values.background;
		    break;
		  }
		else
		  {
		    if (graylevel >= 254)
		      {
			spot = GDK_GC_FBDATA (gc)->values.foreground;
		      }
		    else if (graylevel <= 2)
		      {
			if (!dc->draw_bg)
			  continue;
			
			spot = GDK_GC_FBDATA (gc)->values.background;
		      }
		    else
		      {
			switch ((GDK_GC_FBDATA (gc)->get_color) (drawable, gc, cur_x, cur_y, &realspot))
			  {
			  case GPR_USED_BG:
			    {
			      int bgx, bgy;
			      
			      bgx = (cur_x - GDK_DRAWABLE_IMPL_FBDATA (dc->bg_relto)->abs_x) % GDK_DRAWABLE_IMPL_FBDATA (dc->bgpm)->width + private->abs_x;
			      bgy = (cur_y - GDK_DRAWABLE_IMPL_FBDATA (dc->bg_relto)->abs_y) % GDK_DRAWABLE_IMPL_FBDATA (dc->bgpm)->height + private->abs_y;
			      gdk_fb_drawable_get_color (dc->bgpm, gc, bgx, bgy, &realspot);
			    }
			    break;
			  case GPR_NONE:
			    break;
			  default:
			    g_assert_not_reached ();
			    break;
			  }
			
			fg = GDK_GC_FBDATA (gc)->values.foreground;

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
			switch (private->depth)
			  {
			  case 8:
			    if (!gdk_colormap_alloc_color (private->colormap, &spot, FALSE, TRUE))
			      {
				g_error ("Can't allocate AA color!");
			      }
			    break;
			  case 16:
			    spot.pixel = (spot.red >> (16 - gdk_display->modeinfo.red.length)) << gdk_display->modeinfo.red.offset;
			    spot.pixel |= (spot.green >> (16 - gdk_display->modeinfo.green.length)) << gdk_display->modeinfo.green.offset;
			    spot.pixel |= (spot.blue >> (16 - gdk_display->modeinfo.blue.length)) << gdk_display->modeinfo.blue.offset;
			    break;
			  case 24:
			  case 32:
			    spot.pixel = ((spot.red & 0xFF00) >> 8  << (gdk_display->modeinfo.red.offset))
			      | ((spot.green & 0xFF00) >> 8 << (gdk_display->modeinfo.green.offset))
			      | ((spot.blue & 0xFF00) >> 8 << (gdk_display->modeinfo.blue.offset));
			    break;
			  }
		      }
		  }
	      }
	      break;
	    case GPR_USED_BG:
	      if (!dc->draw_bg)
		continue;
	      break;
	    case GPR_NONE:
	      break;
	    default:
	      g_assert_not_reached ();
	      break;
	    }
	  
	  (GDK_GC_FBDATA (gc)->set_pixel) (drawable, gc, cur_x, cur_y, spot.pixel);
	}
    }

}

void
gdk_fb_draw_drawable_memmove (GdkDrawable *drawable,
			      GdkGC       *gc,
			      GdkPixmap   *src,
			      GdkFBDrawingContext *dc,
			      gint         start_y,
			      gint         end_y,
			      gint         start_x,
			      gint         end_x,
			      gint         src_x_off,
			      gint         src_y_off,
			      gint         draw_direction)
{
  GdkDrawableFBData *src_private = GDK_DRAWABLE_FBDATA (src);
  guint depth = src_private->depth;
  guint src_rowstride = src_private->rowstride;
  guchar *srcmem = src_private->mem;
  int linelen = (end_x - start_x)*(depth>>3);
  gint cur_y;

  if (!_gdk_fb_is_active_vt)
    return;

  if (draw_direction < 0)
    {
      int tmp;
      tmp = start_y;
      start_y = end_y;
      end_y = tmp;
      start_y--;
      end_y--;
    }

  for(cur_y = start_y; cur_y != end_y; cur_y += draw_direction)
    {
      memmove (dc->mem + (cur_y * dc->rowstride) + start_x*(depth>>3),
	       srcmem + ((cur_y + src_y_off)*src_rowstride) + (start_x + src_x_off)*(depth>>3),
	       linelen);
    }

}

static void
gdk_fb_draw_drawable_aa_24 (GdkDrawable *drawable,
			    GdkGC       *gc,
			    GdkPixmap   *src,
			    GdkFBDrawingContext *dc,
			    gint         start_y,
			    gint         end_y,
			    gint         start_x,
			    gint         end_x,
			    gint         src_x_off,
			    gint         src_y_off,
			    gint         draw_direction)
{
  GdkDrawableFBData *private = GDK_DRAWABLE_FBDATA (drawable);
  int x, y;
  GdkGCFBData *gc_private;
  guchar *dmem = private->mem;
  guint dst_rowstride = private->rowstride;
  guchar *smem = GDK_DRAWABLE_FBDATA (src)->mem;
  guint src_rowstride = GDK_DRAWABLE_FBDATA (src)->rowstride;
  guchar *dst;
  guint grayval;
  gint r, g, b, tmp;
  GdkColor fg;
  gint fg_r, fg_g, fg_b;
  
  if (!_gdk_fb_is_active_vt)
    return;

  gc_private = GDK_GC_FBDATA (gc);

  fg = GDK_GC_FBDATA (gc)->values.foreground;
  fg_r = fg.red >> 8;
  fg_g = fg.green >> 8;
  fg_b = fg.blue >> 8;

  if (draw_direction < 0)
    {
      int tmp;
      tmp = start_y;
      start_y = end_y;
      end_y = tmp;
      start_y--;
      end_y--;
      
      tmp = start_x;
      start_x = end_x;
      end_x = tmp;
      start_x--;
      end_x--;
    }

  for (y = start_y; y != end_y; y+=draw_direction)
    {
      for (x = start_x; x != end_x; x+=draw_direction)
	{
	  grayval = smem[x + src_x_off + (y + src_y_off) * src_rowstride];

	  if ((grayval <= 2) && (!dc->draw_bg))
	    continue;

	  dst = &dmem[x*3 + y*dst_rowstride];
	  
	  if (grayval >= 254)
	    {
	      dst[gdk_display->red_byte] = fg_r;
	      dst[gdk_display->green_byte] = fg_g;
	      dst[gdk_display->blue_byte] = fg_b;
	    }
	  else if (grayval <= 2)
	    {
	      dst[gdk_display->red_byte] = GDK_GC_FBDATA (gc)->values.background.red >> 8;
	      dst[gdk_display->green_byte] = GDK_GC_FBDATA (gc)->values.background.green >> 8;
	      dst[gdk_display->blue_byte] = GDK_GC_FBDATA (gc)->values.background.blue >> 8;
	    }
	  else
	    {
	      r = dst[gdk_display->red_byte];
	      tmp = (fg_r - r) * (gint)grayval;
	      r = r + ((tmp + (tmp >> 8) + 0x80) >> 8);
	      dst[gdk_display->red_byte] = r;

	      g = dst[gdk_display->green_byte];
	      tmp = (fg_g - g) * (gint)grayval;
	      g = g + ((tmp + (tmp >> 8) + 0x80) >> 8);
	      dst[gdk_display->green_byte] = g;

	      b = dst[gdk_display->blue_byte];
	      tmp = (fg_b - b) * (gint)grayval;
	      b = b + ((tmp + (tmp >> 8) + 0x80) >> 8);
	      dst[gdk_display->blue_byte] = b;
	    }
	}
    }
}

/*************************************
 * gc->fill_rectangle() implementations
 *************************************/

void
gdk_fb_fill_rectangle_generic (GdkDrawable    *drawable,
			       GdkGC          *gc,
			       GdkRectangle   *rect,
			       GdkColor       *color)
{
  GdkDrawableFBData *private;
  GdkSpan *spans;
  int i;

  if (!_gdk_fb_is_active_vt)
    return;

  private = GDK_DRAWABLE_FBDATA (drawable);

  spans = g_new (GdkSpan, rect->height);
  for (i = 0; i < rect->height; i++)
    {
      spans[i].x = rect->x - private->abs_x;
      spans[i].y = rect->y+i - private->abs_y;
      spans[i].width = rect->width;
    }
  gdk_fb_fill_spans (drawable, gc, spans, rect->height, TRUE);
  g_free (spans);
}

void
gdk_fb_fill_rectangle_simple_16 (GdkDrawable    *drawable,
				 GdkGC          *gc,
				 GdkRectangle   *rect,
				 GdkColor       *color)
{
  GdkGCFBData *gc_private;
  GdkDrawableFBData *private;
  guchar *ptr;
  guint rowstride;
  int n;
  gboolean extra;
  int i;
  guint32 pixel;
  gint height;
  
  if (!_gdk_fb_is_active_vt)
    return;

  private = GDK_DRAWABLE_FBDATA (drawable);
  gc_private = GDK_GC_FBDATA (gc);

  rowstride = private->rowstride - rect->width*2;
  ptr = (guchar *)private->mem + rect->y * private->rowstride + rect->x*2;

  extra = rect->width&1;
  n = rect->width>>1;
    
  pixel = (color->pixel << 16) | color->pixel;

  height = rect->height;
  while (height>0)
    {
      i = n;
      while (i>0)
	{
	  *(guint32 *)ptr = pixel;
	  ptr += 4;
	  i--;
	}
      if (extra)
	{
	  *(guint16 *)ptr = color->pixel;
	  ptr += 2;
	}
      ptr += rowstride;
      height--; 
    }
}

void
gdk_fb_fill_rectangle_simple_32 (GdkDrawable    *drawable,
				 GdkGC          *gc,
				 GdkRectangle   *rect,
				 GdkColor       *color)
{
  GdkGCFBData *gc_private;
  GdkDrawableFBData *private;
  guchar *ptr;
  guint rowstride;
  int n;
  int i;
  guint32 pixel;
  gint height;
  
  if (!_gdk_fb_is_active_vt)
    return;

  private = GDK_DRAWABLE_FBDATA (drawable);
  gc_private = GDK_GC_FBDATA (gc);

  rowstride = private->rowstride - rect->width*4;
  ptr = (guchar *)private->mem + rect->y * private->rowstride + rect->x*4;

  n = rect->width;
    
  pixel = color->pixel;

  height = rect->height;
  while (height>0)
    {
      i = n;
      while (i>0)
	{
	  *(guint32 *)ptr = pixel;
	  ptr += 4;
	  i--;
	}
      ptr += rowstride;
      height--; 
    }
}


/*************************************
 * GC state calculation
 *************************************/

void
_gdk_fb_gc_calc_state (GdkGC           *gc,
		       GdkGCValuesMask  changed)
{
  GdkGCFBData *gc_private;
  int i;

  gc_private = GDK_GC_FBDATA (gc);

  gc_private->fill_span = gdk_fb_fill_span_generic;
  gc_private->fill_rectangle = gdk_fb_fill_rectangle_generic;

  for (i = 0; i < GDK_NUM_FB_SRCBPP; i++)
    gc_private->draw_drawable[i] = gdk_fb_draw_drawable_generic;
  
  if (changed & _GDK_FB_GC_DEPTH)
    switch (gc_private->depth)
      {
      case 1:
	gc_private->set_pixel = gdk_fb_set_pixel_1;
	gc_private->get_color = gdk_fb_get_color_1;
	break;
      case 8:
	gc_private->set_pixel = gdk_fb_set_pixel_8;
	gc_private->get_color = gdk_fb_get_color_8;
	break;
      case 16:
	gc_private->set_pixel = gdk_fb_set_pixel_16;
	gc_private->get_color = gdk_fb_get_color_16;
	break;
      case 24:
	gc_private->set_pixel = gdk_fb_set_pixel_24;
	gc_private->get_color = gdk_fb_get_color_24;
	break;
      case 32:
	gc_private->set_pixel = gdk_fb_set_pixel_32;
	gc_private->get_color = gdk_fb_get_color_32;
	break;
      default:
	g_assert_not_reached ();
	break;
      }

  if (!gc_private->values.clip_mask)
    {
    switch (gc_private->depth)
      {
      case 8:
	gc_private->draw_drawable[GDK_FB_SRC_BPP_8] = gdk_fb_draw_drawable_memmove;
	break;
      case 16:
	gc_private->draw_drawable[GDK_FB_SRC_BPP_16] = gdk_fb_draw_drawable_memmove;
	break;
      case 24:
	gc_private->draw_drawable[GDK_FB_SRC_BPP_8_AA_GRAYVAL] = gdk_fb_draw_drawable_aa_24;
	gc_private->draw_drawable[GDK_FB_SRC_BPP_24] = gdk_fb_draw_drawable_memmove;
	break;
      case 32:
	gc_private->draw_drawable[GDK_FB_SRC_BPP_32] = gdk_fb_draw_drawable_memmove;
	break;
      }
    }
  
  if (!gc_private->values.clip_mask &&
      !gc_private->values.tile &&
      !gc_private->values.stipple &&
       gc_private->values.function == GDK_COPY)
    {
      switch (gc_private->depth)
	{
	case 1:
	  gc_private->fill_span = gdk_fb_fill_span_simple_1;
	  break;
	case 8:
	  gc_private->fill_span = gdk_fb_fill_span_simple_8;
	  break;
	case 16:
	  gc_private->fill_span = gdk_fb_fill_span_simple_16;
	  gc_private->fill_rectangle = gdk_fb_fill_rectangle_simple_16;
	  break;
	case 24:
	  gc_private->fill_span = gdk_fb_fill_span_simple_24;
	  break;
	case 32:
	  gc_private->fill_span = gdk_fb_fill_span_simple_32;
	  gc_private->fill_rectangle = gdk_fb_fill_rectangle_simple_32;
	  break;
	default:
	  g_assert_not_reached ();
	  break;
	}
    }
}

#ifdef ENABLE_SHADOW_FB
static void
gdk_shadow_fb_copy_rect_0 (gint x, gint y, gint width, gint height)
{
  guchar *dst, *src;
  gint depth;

  if (!_gdk_fb_is_active_vt)
    return;

  depth = gdk_display->modeinfo.bits_per_pixel / 8;

  dst = gdk_display->fb_mmap + x * depth + gdk_display->sinfo.line_length * y;
  src = gdk_display->fb_mem + x * depth + gdk_display->fb_stride * y;

  width = width*depth;
  while (height>0)
    {
      memcpy (dst, src, width);
      dst += gdk_display->sinfo.line_length;
      src += gdk_display->fb_stride;
      height--;
    }
}

static void
gdk_shadow_fb_copy_rect_90 (gint x, gint y, gint width, gint height)
{
  guchar *dst, *src, *pdst;
  gint depth;
  gint w;
  gint i;

  if (!_gdk_fb_is_active_vt)
    return;

  depth = gdk_display->modeinfo.bits_per_pixel / 8;

  src = gdk_display->fb_mem + x * depth + gdk_display->fb_stride * y;
  dst = gdk_display->fb_mmap + y * depth + gdk_display->sinfo.line_length * (gdk_display->fb_width - x - 1);

  while (height>0)
    {
      w = width;
      pdst = dst;
      while (w>0) {
	for (i = 0; i < depth; i++)
	  *pdst++ = *src++;
	pdst -= gdk_display->sinfo.line_length + depth;
	w--;
      }
      dst += depth;
      src += gdk_display->fb_stride - width * depth;
      height--;
    }
}

static void
gdk_shadow_fb_copy_rect_180 (gint x, gint y, gint width, gint height)
{
  guchar *dst, *src, *pdst;
  gint depth;
  gint w;
  gint i;

  if (!_gdk_fb_is_active_vt)
    return;

  depth = gdk_display->modeinfo.bits_per_pixel / 8;

  src = gdk_display->fb_mem + x * depth + gdk_display->fb_stride * y;
  dst = gdk_display->fb_mmap + (gdk_display->fb_width - x - 1) * depth + gdk_display->sinfo.line_length * (gdk_display->fb_height - y - 1) ;

  while (height>0)
    {
      w = width;
      pdst = dst;
      while (w>0) {
	for (i = 0; i < depth; i++)
	  *pdst++ = *src++;
	pdst -= 2 * depth;
	w--;
      }
      dst -= gdk_display->sinfo.line_length;
      src += gdk_display->fb_stride - width * depth;
      height--;
    }
}

static void
gdk_shadow_fb_copy_rect_270 (gint x, gint y, gint width, gint height)
{
  guchar *dst, *src, *pdst;
  gint depth;
  gint w;
  gint i;

  if (!_gdk_fb_is_active_vt)
    return;

  depth = gdk_display->modeinfo.bits_per_pixel / 8;

  src = gdk_display->fb_mem + x * depth + gdk_display->fb_stride * y;
  dst = gdk_display->fb_mmap + (gdk_display->fb_height - y - 1) * depth + gdk_display->sinfo.line_length * x;

  while (height>0)
    {
      w = width;
      pdst = dst;
      while (w>0) {
	for (i = 0; i < depth; i++)
	  *pdst++ = *src++;
	pdst += gdk_display->sinfo.line_length - depth;
	w--;
      }
      dst -= depth;
      src += gdk_display->fb_stride - width * depth;
      height--;
    }
}

static void (*shadow_copy_rect[4]) (gint x, gint y, gint width, gint height);

volatile gint refresh_queued = 0;
volatile gint refresh_x1, refresh_y1;
volatile gint refresh_x2, refresh_y2;

static void
gdk_shadow_fb_refresh (int signum)
{
  gint minx, miny, maxx, maxy;

  if (!refresh_queued)
    {
      struct itimerval timeout;
      /* Stop the timer */ 
      timeout.it_value.tv_sec = 0;
      timeout.it_value.tv_usec = 0;
      timeout.it_interval.tv_sec = 0;
      timeout.it_interval.tv_usec = 0;
      setitimer (ITIMER_REAL, &timeout, NULL);
      return;
    }
 
  
  minx = refresh_x1;
  miny = refresh_y1;
  maxx = refresh_x2;
  maxy = refresh_y2;
  refresh_queued = 0;

  /* clip x */
  if (minx < 0) {
    minx = 0;
    maxx = MAX (maxx, 0);
  }
  if (maxx >= gdk_display->fb_width) {
    maxx = gdk_display->fb_width-1;
    minx = MIN (minx, maxx);
  }
  /* clip y */
  if (miny < 0) {
    miny = 0;
    maxy = MAX (maxy, 0);
  }
  if (maxy >= gdk_display->fb_height) {
    maxy = gdk_display->fb_height-1;
    miny = MIN (miny, maxy);
  }
  
  (*shadow_copy_rect[_gdk_fb_screen_angle]) (minx, miny, maxx - minx + 1, maxy - miny + 1);
}

void
gdk_shadow_fb_stop_updates (void)
{
  struct itimerval timeout;

  refresh_queued = 0;

  /* Stop the timer */ 
  timeout.it_value.tv_sec = 0;
  timeout.it_value.tv_usec = 0;
  timeout.it_interval.tv_sec = 0;
  timeout.it_interval.tv_usec = 0;
  setitimer (ITIMER_REAL, &timeout, NULL);

  refresh_queued = 0;
}

void
gdk_shadow_fb_init (void)
{
  struct sigaction action;

  action.sa_handler = gdk_shadow_fb_refresh;
  sigemptyset (&action.sa_mask);
  action.sa_flags = 0;
  
  sigaction (SIGALRM, &action, NULL);

  shadow_copy_rect[GDK_FB_0_DEGREES] = gdk_shadow_fb_copy_rect_0;
  shadow_copy_rect[GDK_FB_90_DEGREES] = gdk_shadow_fb_copy_rect_90;
  shadow_copy_rect[GDK_FB_180_DEGREES] = gdk_shadow_fb_copy_rect_180;
  shadow_copy_rect[GDK_FB_270_DEGREES] = gdk_shadow_fb_copy_rect_270;
}

/* maxx and maxy are included */
void
gdk_shadow_fb_update (gint minx, gint miny, gint maxx, gint maxy)
{
  struct itimerval timeout;

  if (gdk_display->manager_blocked)
    return;
  
  g_assert (minx <= maxx);
  g_assert (miny <= maxy);

  if (refresh_queued)
    {
      refresh_x1 = MIN (refresh_x1, minx);
      refresh_y1 = MIN (refresh_y1, miny);
      refresh_x2 = MAX (refresh_x2, maxx);
      refresh_y2 = MAX (refresh_y2, maxy);
      refresh_queued = 1;
    }
  else
    {
      refresh_x1 = minx;
      refresh_y1 = miny;
      refresh_x2 = maxx;
      refresh_y2 = maxy;
      refresh_queued = 1;

      getitimer (ITIMER_REAL, &timeout);
      if (timeout.it_value.tv_usec == 0)
	{
	  timeout.it_value.tv_sec = 0;
	  timeout.it_value.tv_usec = 20000; /* 20 ms => 50 fps */
	  timeout.it_interval.tv_sec = 0;
	  timeout.it_interval.tv_usec = 20000; /* 20 ms => 50 fps */
	  setitimer (ITIMER_REAL, &timeout, NULL);
	}
    }
  
}
#else

void
gdk_shadow_fb_stop_updates (void)
{
}

void
gdk_shadow_fb_update (gint minx, gint miny, gint maxx, gint maxy)
{
}

void
gdk_shadow_fb_init (void)
{
}

#endif

