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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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

#include "config.h"

#include <string.h>

#include "gdkgc.h"
#include "gdkfont.h"
#include "gdkpixmap.h"
#include "gdkregion-generic.h"
#include "gdkinternals.h"
#include "gdkprivate-win32.h"

static void gdk_win32_gc_destroy    (GdkGC           *gc);
static void gdk_win32_gc_get_values (GdkGC           *gc,
				     GdkGCValues     *values);
static void gdk_win32_gc_set_values (GdkGC           *gc,
				     GdkGCValues     *values,
				     GdkGCValuesMask  values_mask);
static void gdk_win32_gc_set_dashes (GdkGC           *gc,
				     gint             dash_offset,
				     gchar            dash_list[],
				     gint             n);

static GdkGCClass gdk_win32_gc_class = {
  gdk_win32_gc_destroy,
  gdk_win32_gc_get_values,
  gdk_win32_gc_set_values,
  gdk_win32_gc_set_dashes
};

static void
gdk_win32_gc_values_to_win32values (GdkGCValues    *values,
				    GdkGCValuesMask mask,
				    GdkGC          *gc)
{				    
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc);
  char *s = "";
  gint sw, sh;

  GDK_NOTE (MISC, g_print ("{"));

  if (mask & GDK_GC_FOREGROUND)
    {
      data->foreground = values->foreground.pixel;
      data->values_mask |= GDK_GC_FOREGROUND;
      GDK_NOTE (MISC, (g_print ("fg=%.06x", data->foreground),
		       s = ","));
    }
  
  if (mask & GDK_GC_BACKGROUND)
    {
      data->background = values->background.pixel;
      data->values_mask |= GDK_GC_BACKGROUND;
      GDK_NOTE (MISC, (g_print ("%sbg=%.06x", s, data->background),
		       s = ","));
    }

  if ((mask & GDK_GC_FONT) && (values->font->type == GDK_FONT_FONT
			       || values->font->type == GDK_FONT_FONTSET))
    {
      if (data->font != NULL)
	gdk_font_unref (data->font);
      data->font = values->font;
      if (data->font != NULL)
	{
	  gchar *xlfd;

	  gdk_font_ref (data->font);
	  data->values_mask |= GDK_GC_FONT;
	  GDK_NOTE (MISC, (xlfd = gdk_font_full_name_get (data->font),
			   g_print ("%sfont=%s", s, xlfd),
			   s = ",",
			   gdk_font_full_name_free (xlfd)));
	}
      else
	{
	  data->values_mask &= ~GDK_GC_FONT;
	  GDK_NOTE (MISC, (g_print ("%sfont=NULL"),
			   s = ","));
	}
    }

  if (mask & GDK_GC_FUNCTION)
    {
      GDK_NOTE (MISC, (g_print ("%srop2=", s),
		       s = ","));
      switch (values->function)
	{
	case GDK_COPY:
	  data->rop2 = R2_COPYPEN;
	  GDK_NOTE (MISC, g_print ("COPYPEN"));
	  break;
	case GDK_INVERT:
	  data->rop2 = R2_NOT;
	  GDK_NOTE (MISC, g_print ("NOT"));
	  break;
	case GDK_XOR:
	  data->rop2 = R2_XORPEN;
	  GDK_NOTE (MISC, g_print ("XORPEN"));
	  break;
	case GDK_CLEAR:
	  data->rop2 = R2_BLACK;
	  GDK_NOTE (MISC, g_print ("BLACK"));
	  break;
	case GDK_AND:
	  data->rop2 = R2_MASKPEN;
	  GDK_NOTE (MISC, g_print ("MASKPEN"));
	  break;
	case GDK_AND_REVERSE:
	  data->rop2 = R2_MASKPENNOT;
	  GDK_NOTE (MISC, g_print ("MASKPENNOT"));
	  break;
	case GDK_AND_INVERT:
	  data->rop2 = R2_MASKNOTPEN;
	  GDK_NOTE (MISC, g_print ("MASKNOTPEN"));
	  break;
	case GDK_NOOP:
	  data->rop2 = R2_NOP;
	  GDK_NOTE (MISC, g_print ("NOP"));
	  break;
	case GDK_OR:
	  data->rop2 = R2_MERGEPEN;
	  GDK_NOTE (MISC, g_print ("MERGEPEN"));
	  break;
	case GDK_EQUIV:
	  data->rop2 = R2_NOTXORPEN;
	  GDK_NOTE (MISC, g_print ("NOTXORPEN"));
	  break;
	case GDK_OR_REVERSE:
	  data->rop2 = R2_MERGEPENNOT;
	  GDK_NOTE (MISC, g_print ("MERGEPENNOT"));
	  break;
	case GDK_COPY_INVERT:
	  data->rop2 = R2_NOTCOPYPEN;
	  GDK_NOTE (MISC, g_print ("NOTCOPYPEN"));
	  break;
	case GDK_OR_INVERT:
	  data->rop2 = R2_MERGENOTPEN;
	  GDK_NOTE (MISC, g_print ("MERGENOTPEN"));
	  break;
	case GDK_NAND:
	  data->rop2 = R2_NOTMASKPEN;
	  GDK_NOTE (MISC, g_print ("NOTMASKPEN"));
	  break;
	case GDK_NOR:
	  data->rop2 = R2_NOTMERGEPEN;
	  GDK_NOTE (MISC, g_print ("NOTMERGEPEN"));
	  break;
	case GDK_SET:
	  data->rop2 = R2_WHITE;
	  GDK_NOTE (MISC, g_print ("WHITE"));
	  break;
	}
      data->values_mask |= GDK_GC_FUNCTION;
    }

  if (mask & GDK_GC_FILL)
    {
      data->fill_style = values->fill;
      data->values_mask |= GDK_GC_FILL;
      GDK_NOTE (MISC, (g_print ("%sfill=%d", s, data->fill_style),
		       s = ","));
    }

  if (mask & GDK_GC_TILE)
    {
      if (data->tile != NULL)
	gdk_drawable_unref (data->tile);
      data->tile = values->tile;
      if (data->tile != NULL)
	{
	  gdk_drawable_ref (data->tile);
	  data->values_mask |= GDK_GC_TILE;
	  GDK_NOTE (MISC, (g_print ("%stile=%#x", s,
				    GDK_DRAWABLE_XID (data->tile)),
			   s = ","));
	}
      else
	{
	  data->values_mask &= ~GDK_GC_TILE;
	  GDK_NOTE (MISC, (g_print ("%stile=NULL", s),
			   s = ","));
	}
    }

  if (mask & GDK_GC_STIPPLE)
    {
      if (data->stipple != NULL)
	gdk_drawable_unref (data->stipple);
      data->stipple = values->stipple;
      if (data->stipple != NULL)
	{
	  gdk_drawable_get_size (data->stipple, &sw, &sh);

	  if (sw != 8 || sh != 8)
	    {
	      /* It seems that it *must* be 8x8, at least on my machine. 
	       * Thus, tile an 8x8 bitmap with the stipple in case it is
	       * smaller, or simply use just the top left 8x8 in case it is
	       * larger.
	       */
	      gchar dummy[8];
	      GdkPixmap *bm = gdk_bitmap_create_from_data (NULL, dummy, 8, 8);
	      GdkGC *gc = gdk_gc_new (bm);
	      gint i, j;

	      i = 0;
	      while (i < 8)
		{
		  j = 0;
		  while (j < 8)
		    {
		      gdk_draw_drawable (bm, gc, data->stipple, 0, 0, i, j, sw, sh);
		      j += sh;
		    }
		  i += sw;
		}
	      data->stipple = bm;
	      gdk_gc_unref (gc);
	    }
	  else
	    gdk_drawable_ref (data->stipple);
	  data->values_mask |= GDK_GC_STIPPLE;
	  GDK_NOTE (MISC, (g_print ("%sstipple=%#x", s,
				    GDK_DRAWABLE_XID (data->stipple)),
			   s = ","));
	}
      else
	{
	  data->values_mask &= ~GDK_GC_STIPPLE;
	  GDK_NOTE (MISC, (g_print ("%sstipple=NULL", s),
			   s = ","));
	}
    }

  if (mask & GDK_GC_CLIP_MASK)
    {
      if (data->clip_region != NULL)
	{
	  gdk_region_destroy (data->clip_region);
	  data->clip_region = NULL;
	}
      if (data->hcliprgn != NULL)
	DeleteObject (data->hcliprgn);

      if (values->clip_mask != NULL)
	{
	  data->hcliprgn =
	    BitmapToRegion ((HBITMAP) GDK_DRAWABLE_XID (values->clip_mask));
	  data->values_mask |= GDK_GC_CLIP_MASK;
	  OffsetRgn (data->hcliprgn,
		     ((GdkGCPrivate *) gc)->clip_x_origin,
		     ((GdkGCPrivate *) gc)->clip_y_origin);
	}
      else
	{
	  data->hcliprgn = NULL;
	  data->values_mask &= ~GDK_GC_CLIP_MASK;
	}
      GDK_NOTE (MISC, (g_print ("%sclip=%#x", s, data->hcliprgn),
		       s = ","));
    }

  if (mask & GDK_GC_SUBWINDOW)
    {
      data->subwindow_mode = values->subwindow_mode;
      data->values_mask |= GDK_GC_SUBWINDOW;
      GDK_NOTE (MISC, (g_print ("%ssubw=%d", s, data->subwindow_mode),
		       s = ","));
    }

  if (mask & GDK_GC_TS_X_ORIGIN)
    {
      data->values_mask |= GDK_GC_TS_X_ORIGIN;
      GDK_NOTE (MISC, (g_print ("%sts_x=%d", s, values->ts_x_origin),
		       s = ","));
    }

  if (mask & GDK_GC_TS_Y_ORIGIN)
    {
      data->values_mask |= GDK_GC_TS_Y_ORIGIN;
      GDK_NOTE (MISC, (g_print ("%sts_y=%d", s, values->ts_y_origin),
		       s = ","));
    }

  if (mask & GDK_GC_CLIP_X_ORIGIN)
    {
      data->values_mask |= GDK_GC_CLIP_X_ORIGIN;
      GDK_NOTE (MISC, (g_print ("%sclip_x=%d", s, values->clip_x_origin),
		       s = ","));
    }

  if (mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      data->values_mask |= GDK_GC_CLIP_Y_ORIGIN;
      GDK_NOTE (MISC, (g_print ("%sclip_y=%d", s, values->clip_y_origin),
		       s = ","));
   }
 
  if (mask & GDK_GC_EXPOSURES)
    {
      data->graphics_exposures = values->graphics_exposures;
      data->values_mask |= GDK_GC_EXPOSURES;
      GDK_NOTE (MISC, (g_print ("%sexp=%d", s, data->graphics_exposures),
		       s = ","));
    }

  if (mask & GDK_GC_LINE_WIDTH)
    {
      data->pen_width = values->line_width;
      data->values_mask |= GDK_GC_LINE_WIDTH;
      GDK_NOTE (MISC, (g_print ("%spw=%d", s, data->pen_width),
		       s = ","));
    }

  if (mask & GDK_GC_LINE_STYLE)
    {
      data->pen_style &= ~(PS_STYLE_MASK);
      GDK_NOTE (MISC, (g_print ("%sps|=", s),
		       s = ","));
      switch (values->line_style)
	{
	case GDK_LINE_SOLID:
	  GDK_NOTE (MISC, g_print ("LINE_SOLID"));
	  data->pen_style |= PS_SOLID;
	  break;
	case GDK_LINE_ON_OFF_DASH:
	case GDK_LINE_DOUBLE_DASH: /* ??? */
	  GDK_NOTE (MISC, g_print ("DASH"));
	  data->pen_style |= PS_DASH;
	  break;
	}
      data->values_mask |= GDK_GC_LINE_STYLE;
    }

  if (mask & GDK_GC_CAP_STYLE)
    {
      data->pen_style &= ~(PS_ENDCAP_MASK);
      GDK_NOTE (MISC, (g_print ("%sps|=", s),
		       s = ","));
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:	/* ??? */
	case GDK_CAP_BUTT:
	  GDK_NOTE (MISC, g_print ("ENDCAP_FLAT"));
	  data->pen_style |= PS_ENDCAP_FLAT;
	  break;
	case GDK_CAP_ROUND:
	  GDK_NOTE (MISC, g_print ("ENDCAP_ROUND"));
	  data->pen_style |= PS_ENDCAP_ROUND;
	  break;
	case GDK_CAP_PROJECTING:
	  GDK_NOTE (MISC, g_print ("ENDCAP_SQUARE"));
	  data->pen_style |= PS_ENDCAP_SQUARE;
	  break;
	}
      data->values_mask |= GDK_GC_CAP_STYLE;
    }

  if (mask & GDK_GC_JOIN_STYLE)
    {
      data->pen_style &= ~(PS_JOIN_MASK);
      GDK_NOTE (MISC, (g_print ("%sps|=", s),
		       s = ","));
      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  GDK_NOTE (MISC, g_print ("JOIN_MITER"));
	  data->pen_style |= PS_JOIN_MITER;
	  break;
	case GDK_JOIN_ROUND:
	  GDK_NOTE (MISC, g_print ("JOIN_ROUND"));
	  data->pen_style |= PS_JOIN_ROUND;
	  break;
	case GDK_JOIN_BEVEL:
	  GDK_NOTE (MISC, g_print ("JOIN_BEVEL"));
	  data->pen_style |= PS_JOIN_BEVEL;
	  break;
	}
      data->values_mask |= GDK_GC_JOIN_STYLE;
    }
  GDK_NOTE (MISC, g_print ("}\n"));
}

GdkGC*
_gdk_win32_gc_new (GdkDrawable	  *drawable,
		   GdkGCValues	  *values,
		   GdkGCValuesMask mask)
{
  GdkGC *gc;
  GdkGCPrivate *private;
  GdkGCWin32Data *data;

  gc = gdk_gc_alloc ();
  private = (GdkGCPrivate *)gc;

  private->klass = &gdk_win32_gc_class;
  private->klass_data = data = g_new (GdkGCWin32Data, 1);
    
  data->clip_region = NULL;

  /* Use the same default values as X11 does, even if they don't make
   * sense per se. But apps always set fg and bg anyway.
   */
  data->foreground = 0;
  data->background = 1;
  data->font = NULL;
  data->rop2 = R2_COPYPEN;
  data->fill_style = GDK_SOLID;
  data->tile = NULL;
  data->stipple = NULL;
  data->pen_style = PS_GEOMETRIC|PS_ENDCAP_FLAT|PS_JOIN_MITER;
  data->pen_width = 0;

  data->values_mask = GDK_GC_FUNCTION | GDK_GC_FILL;

  GDK_NOTE (MISC, g_print ("_gdk_win32_gc_new: "));
  gdk_win32_gc_values_to_win32values (values, mask, gc);

  data->hwnd = NULL;
  data->xgc = NULL;
  data->hcliprgn = NULL;

  GDK_NOTE (MISC, g_print (" = %p\n", gc));

  return gc;
}

static void
gdk_win32_gc_destroy (GdkGC *gc)
{
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc);

  if (data->clip_region)
    gdk_region_destroy (data->clip_region);
  
  if (data->values_mask & GDK_GC_FONT)
    gdk_font_unref (data->font);
  
  if (data->values_mask & GDK_GC_TILE)
    gdk_drawable_unref (data->tile);
  
  if (data->values_mask & GDK_GC_STIPPLE)
    gdk_drawable_unref (data->stipple);
  
  g_free (GDK_GC_WIN32DATA (gc));
}

static void
gdk_win32_gc_get_values (GdkGC       *gc,
			 GdkGCValues *values)
{
  GdkGCPrivate *private = (GdkGCPrivate *) gc;
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc);

  values->foreground.pixel = data->foreground;
  values->background.pixel = data->background;
  values->font = data->font;

  switch (data->rop2)
    {
    case R2_COPYPEN:
      values->function = GDK_COPY; break;
    case R2_NOT:
      values->function = GDK_INVERT; break;
    case R2_XORPEN:
      values->function = GDK_XOR; break;
    case R2_BLACK:
      values->function = GDK_CLEAR; break;
    case R2_MASKPEN:
      values->function = GDK_AND; break;
    case R2_MASKPENNOT:
      values->function = GDK_AND_REVERSE; break;
    case R2_MASKNOTPEN:
      values->function = GDK_AND_INVERT; break;
    case R2_NOP:
      values->function = GDK_NOOP; break;
    case R2_MERGEPEN:
      values->function = GDK_OR; break;
    case R2_NOTXORPEN:
      values->function = GDK_EQUIV; break;
    case R2_MERGEPENNOT:
      values->function = GDK_OR_REVERSE; break;
    case R2_NOTCOPYPEN:
      values->function = GDK_COPY_INVERT; break;
    case R2_MERGENOTPEN:
      values->function = GDK_OR_INVERT; break;
    case R2_NOTMASKPEN:
      values->function = GDK_NAND; break;
    case R2_NOTMERGEPEN:
      values->function = GDK_NOR; break;
    case R2_WHITE:
      values->function = GDK_SET; break;
    }

  values->fill = data->fill_style;

  values->tile = data->tile;
  values->stipple = data->stipple;

  /* Also the X11 backend always returns a NULL clip_mask */
  values->clip_mask = NULL;

  values->subwindow_mode = data->subwindow_mode;
  values->ts_x_origin = private->ts_x_origin;
  values->ts_y_origin = private->ts_y_origin;
  values->clip_x_origin = private->clip_x_origin;
  values->clip_y_origin = private->clip_y_origin;
  values->graphics_exposures = data->graphics_exposures;
  values->line_width = data->pen_width;
  
  if (data->pen_style & PS_SOLID)
    values->line_style = GDK_LINE_SOLID;
  else if (data->pen_style & PS_DASH)
    values->line_style = GDK_LINE_ON_OFF_DASH;
  else
    values->line_style = GDK_LINE_SOLID;

  /* PS_ENDCAP_ROUND is zero */
  if (data->pen_style & PS_ENDCAP_FLAT)
    values->cap_style = GDK_CAP_BUTT;
  else if (data->pen_style & PS_ENDCAP_SQUARE)
    values->cap_style = GDK_CAP_PROJECTING;
  else
    values->cap_style = GDK_CAP_ROUND;
    
  /* PS_JOIN_ROUND is zero */
  if (data->pen_style & PS_JOIN_MITER)
    values->join_style = GDK_JOIN_MITER;
  else if (data->pen_style & PS_JOIN_BEVEL)
    values->join_style = GDK_JOIN_BEVEL;
  else
    values->join_style = GDK_JOIN_ROUND;
}

static void
gdk_win32_gc_set_values (GdkGC           *gc,
			 GdkGCValues     *values,
			 GdkGCValuesMask  mask)
{
  GDK_NOTE (MISC, g_print ("gdk_win32_gc_set_values: "));
  gdk_win32_gc_values_to_win32values (values, mask, gc);
}

static void
gdk_win32_gc_set_dashes (GdkGC *gc,
			 gint	  dash_offset,
			 gchar  dash_list[],
			 gint   n)
{
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc);

  data->pen_style &= ~(PS_STYLE_MASK);
  data->pen_style |= PS_DASH;

  /* 
   * Set the extended line style. This could be done by 
   * PS_USERSTYLE and ExtCreatePen; but ONLY on WinNT, 
   * so let's make a guess (based on the implementation 
   * in DIA). On Win9x this does only work for lines
   * with width one ...
   *
   * More workarounds for Win9x descibed at:
   * http://www.codeguru.com/gdi/dashed.shtml
   */
  if (!IS_WIN_NT (windows_version) && data->pen_width > 1)
    {
      GDK_NOTE (MISC, g_print ("gdk_win32_gc_set_dashes: not fully supported\n"));
      data->pen_style |= PS_SOLID;
      return;
    }
  
  /* data->pen_style = PS_COSMETIC; ??? */
  if (2 == n)
    {
      if ((dash_list[0] == dash_list[1]) && (dash_list[0] > 2))
        {
          data->pen_style |= PS_DASH;
          GDK_NOTE (MISC, g_print("gdk_win32_gc_set_dashes: PS_DASH (%d,%d)\n", 
                                  dash_list[0], dash_list[1]));
        }
      else
        {
          data->pen_style |= PS_DOT;
          GDK_NOTE (MISC, g_print("gdk_win32_gc_set_dashes: PS_DOT (%d,%d)\n", 
                                  dash_list[0], dash_list[1]));
        }
    }
  else if (4 == n)
    {
      data->pen_style |= PS_DASHDOT; 
      GDK_NOTE (MISC, g_print("gdk_win32_gc_set_dashes: PS_DASHDOT (%d,%d,%d,%d)\n", 
                              dash_list[0], dash_list[1],
                              dash_list[2], dash_list[3]));
    }
  else if (6 == n)
    {
      data->pen_style |= PS_DASHDOTDOT; 
      GDK_NOTE (MISC, g_print("gdk_win32_gc_set_dashes: PS_DASHDOTDOT (%d,%d,%d,%d,%d,%d)\n", 
                              dash_list[0], dash_list[1],
                              dash_list[2], dash_list[3],
                              dash_list[4], dash_list[5]));
    }
  else
    {
      data->pen_style |= PS_DASH;
      GDK_NOTE (MISC, g_print("gdk_win32_gc_set_dashes: no guess for %d dashes\n", n));
    }
}

void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkGCWin32Data *data;
   
  g_return_if_fail (gc != NULL);

  data = GDK_GC_WIN32DATA (gc);

  if (data->clip_region)
    gdk_region_destroy (data->clip_region);

  if (rectangle)
    {
      GDK_NOTE (MISC,
		g_print ("gdk_gc_set_clip_rectangle: (%d) %dx%d@+%d+%d\n",
			 data,
			 rectangle->width, rectangle->height,
			 rectangle->x, rectangle->y));
      data->clip_region = gdk_region_rectangle (rectangle);
      data->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_rectangle: (%d) NULL\n",
			       data));
      data->clip_region = NULL;
      data->values_mask &= ~GDK_GC_CLIP_MASK;
    }
    data->values_mask &= ~(GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN);
} 

void
gdk_gc_set_clip_region (GdkGC		 *gc,
			GdkRegion	 *region)
{
  GdkGCPrivate *private = (GdkGCPrivate *) gc;
  GdkGCWin32Data *data;

  g_return_if_fail (gc != NULL);

  data = GDK_GC_WIN32DATA (gc);

  if (data->clip_region)
    gdk_region_destroy (data->clip_region);

  if (region)
    {
      GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_region: %d %dx%d+%d+%d\n",
			       data,
			       region->extents.x2 - region->extents.x1,
			       region->extents.y2 - region->extents.y1,
			       region->extents.x1, region->extents.y1));
      data->clip_region = gdk_region_copy (region);
      data->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_region: %d NULL\n",
			       data));
      data->clip_region = NULL;
      data->values_mask &= ~GDK_GC_CLIP_MASK;
    }

  private->clip_x_origin = 0;
  private->clip_y_origin = 0;

  data->values_mask &= ~(GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN);
}

void
gdk_gc_copy (GdkGC *dst_gc, GdkGC *src_gc)
{
  GdkGCWin32Data *dst_data = GDK_GC_WIN32DATA (dst_gc);
  GdkGCWin32Data *src_data = GDK_GC_WIN32DATA (src_gc);
  DWORD nbytes;
  LPRGNDATA rgn;

  if (dst_data->font != NULL)
    gdk_font_unref (dst_data->font);
  if (dst_data->tile != NULL)
    gdk_drawable_unref (dst_data->tile);
  if (dst_data->stipple != NULL)
    gdk_drawable_unref (dst_data->stipple);
  if (dst_data->clip_region != NULL)
    gdk_region_destroy (dst_data->clip_region);
  
  *dst_data = *src_data;

  if (dst_data->clip_region != NULL)
    dst_data->clip_region = gdk_region_copy (dst_data->clip_region);
  if (dst_data->font != NULL)
    gdk_font_ref (dst_data->font);
  if (dst_data->tile != NULL)
    gdk_drawable_ref (dst_data->tile);
  if (dst_data->stipple != NULL)
    gdk_drawable_ref (dst_data->stipple);
}

static guint bitmask[9] = { 0, 1, 3, 7, 15, 31, 63, 127, 255 };

COLORREF
gdk_colormap_color (GdkColormapPrivateWin32 *colormap_private,
		    gulong                   pixel)
{
  GdkVisual *visual;
  guchar r, g, b;

  if (colormap_private == NULL || colormap_private->xcolormap->rc_palette)
    return PALETTEINDEX (pixel);
  else
    {
      visual = colormap_private->base.visual;
      r = (pixel & visual->red_mask) >> visual->red_shift;
      r = (r * 255) / bitmask[visual->red_prec];
      g = (pixel & visual->green_mask) >> visual->green_shift;
      g = (g * 255) / bitmask[visual->green_prec];
      b = (pixel & visual->blue_mask) >> visual->blue_shift;
      b = (b * 255) / bitmask[visual->blue_prec];

      return RGB (r, g, b);
    }
}

static void
predraw_set_foreground (GdkGCPrivate            *gc_private,
			GdkColormapPrivateWin32 *colormap_private,
			gboolean                *ok)
{
  COLORREF fg;
  LOGBRUSH logbrush;
  HPEN hpen;
  HBRUSH hbr;
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc_private);

  if (colormap_private == NULL)
    {
      /* A 1 bit deep bitmap */
      struct
      {
	WORD palVersion;
	WORD palNumEntries;
	PALETTEENTRY palPalEntry[2];
      } logpal;
      static HPALETTE hpal = NULL;

      if (hpal == NULL)
	{
	  /* Create a b&w palette */
	  logpal.palVersion = 0x300;
	  logpal.palNumEntries = 2;
	  logpal.palPalEntry[0].peRed = 
	    logpal.palPalEntry[0].peGreen = 
	    logpal.palPalEntry[0].peBlue = 0x00;
	  logpal.palPalEntry[0].peFlags = 0x00;
	  logpal.palPalEntry[1].peRed = 
	    logpal.palPalEntry[1].peGreen = 
	    logpal.palPalEntry[1].peBlue = 0xFF;
	  logpal.palPalEntry[1].peFlags = 0x00;
	  if ((hpal = CreatePalette ((LOGPALETTE *) &logpal)) == NULL)
	    WIN32_GDI_FAILED ("CreatePalette"), *ok = FALSE;
	}
      SelectPalette (data->xgc, hpal, FALSE);
      RealizePalette (data->xgc);
      fg = PALETTEINDEX (data->foreground);
    }
  else if (colormap_private->xcolormap->rc_palette)
    {
      int k;
      if (SelectPalette (data->xgc, colormap_private->xcolormap->palette,
			 FALSE) == NULL)
	WIN32_GDI_FAILED ("SelectPalette"), *ok = FALSE;
      if (TRUE || colormap_private->xcolormap->stale)
	{
	  if ((k = RealizePalette (data->xgc)) == GDI_ERROR)
	    WIN32_GDI_FAILED ("RealizePalette"), *ok = FALSE;
	  colormap_private->xcolormap->stale = FALSE;
	}
#if 0
      g_print ("Selected palette %#x for gc %#x, realized %d colors\n",
	       colormap_private->xcolormap->palette, data->xgc, k);
#endif
    }

  fg = gdk_colormap_color (colormap_private, data->foreground);

  if (SetTextColor (data->xgc, fg) == CLR_INVALID)
    WIN32_GDI_FAILED ("SetTextColor"), *ok = FALSE;

  /* Create and select pen and brush. */

  logbrush.lbStyle = BS_SOLID;
  logbrush.lbColor = fg;

  if (*ok && (hpen = ExtCreatePen (data->pen_style,
				   (data->pen_width > 0 ? data->pen_width : 1),
				   &logbrush, 0, NULL)) == NULL)
    WIN32_GDI_FAILED ("ExtCreatePen");
  
  if (SelectObject (data->xgc, hpen) == NULL)
    WIN32_GDI_FAILED ("SelectObject"), *ok = FALSE;

  switch (data->fill_style)
    {
    case GDK_OPAQUE_STIPPLED:
      if (*ok && (hbr = CreatePatternBrush (GDK_DRAWABLE_XID (data->stipple))) == NULL)
	WIN32_GDI_FAILED ("CreatePatternBrush"), *ok = FALSE;
	
      if (*ok && !SetBrushOrgEx(data->xgc, gc_private->ts_x_origin,
				gc_private->ts_y_origin, NULL))
	WIN32_GDI_FAILED ("SetBrushOrgEx"), *ok = FALSE;
      break;

    case GDK_SOLID:
    default:
      if (*ok && (hbr = CreateSolidBrush (fg)) == NULL)
	WIN32_GDI_FAILED ("CreateSolidBrush"), *ok = FALSE;
      break;
  }
  if (*ok && SelectObject (data->xgc, hbr) == NULL)
    WIN32_GDI_FAILED ("SelectObject"), *ok = FALSE;
}  

void
predraw_set_background (GdkGCWin32Data          *data,
			GdkColormapPrivateWin32 *colormap_private,
			gboolean                *ok)
{
  COLORREF bg = gdk_colormap_color (colormap_private, data->background);

  if (SetBkColor (data->xgc, bg) == CLR_INVALID)
    WIN32_GDI_FAILED ("SetBkColor"), *ok = FALSE;
}

HDC
gdk_gc_predraw (GdkDrawable    *drawable,
		GdkGCPrivate   *gc_private,
		GdkGCValuesMask usage)
{
  GdkDrawablePrivate *drawable_private = (GdkDrawablePrivate *) drawable;
  GdkColormapPrivateWin32 *colormap_private =
    (GdkColormapPrivateWin32 *) drawable_private->colormap;
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc_private);
  gboolean ok = TRUE;
  int flag;

  g_assert (data->xgc == NULL);

  data->hwnd = GDK_DRAWABLE_XID (drawable);
  
  if (GDK_DRAWABLE_TYPE (drawable) == GDK_DRAWABLE_PIXMAP)
    {
      if ((data->xgc = CreateCompatibleDC (NULL)) == NULL)
	WIN32_GDI_FAILED ("CreateCompatibleDC"), ok = FALSE;

      if (ok && (data->saved_dc = SaveDC (data->xgc)) == 0)
	WIN32_GDI_FAILED ("SaveDC"), ok = FALSE;
      
      if (ok && SelectObject (data->xgc, data->hwnd) == NULL)
	WIN32_GDI_FAILED ("SelectObject"), ok = FALSE;
    }
  else
    {
      if ((data->xgc = GetDC (data->hwnd)) == NULL)
	WIN32_GDI_FAILED ("GetDC");
      
      if (ok && (data->saved_dc = SaveDC (data->xgc)) == 0)
	WIN32_GDI_FAILED ("SaveDC");
    }
  
  if (ok && (usage & GDK_GC_FOREGROUND))
    predraw_set_foreground (gc_private, colormap_private, &ok);

  if (ok
      && (usage & GDK_GC_BACKGROUND)
      && (data->values_mask & GDK_GC_BACKGROUND))
    predraw_set_background (data, colormap_private, &ok);
  
  if (ok && (usage & GDK_GC_FONT))
    {
      if (SetBkMode (data->xgc, TRANSPARENT) == 0)
	WIN32_GDI_FAILED ("SetBkMode"), ok = FALSE;
  
      if (ok && SetTextAlign (data->xgc, TA_BASELINE) == GDI_ERROR)
	WIN32_GDI_FAILED ("SetTextAlign"), ok = FALSE;
    }
  
  if (ok && (data->values_mask & GDK_GC_FUNCTION))
    if (SetROP2 (data->xgc, data->rop2) == 0)
      WIN32_GDI_FAILED ("SetROP2"), ok = FALSE;

  if (data->values_mask & GDK_GC_CLIP_MASK)
    g_assert ((data->clip_region != NULL) != (data->hcliprgn != NULL));

  if (ok
      && (data->values_mask & GDK_GC_CLIP_MASK)
      && data->clip_region != NULL)
    {
      HRGN hrgn;
      RGNDATA *rgndata;
      RECT *rect;
      GdkRegionBox *boxes = data->clip_region->rects;
      guint nbytes =
	sizeof (RGNDATAHEADER) + (sizeof (RECT) * data->clip_region->numRects);
      int i;

      rgndata =	g_malloc (nbytes);
      rgndata->rdh.dwSize = sizeof (RGNDATAHEADER);
      rgndata->rdh.iType = RDH_RECTANGLES;
      rgndata->rdh.nCount = rgndata->rdh.nRgnSize = 0;
      SetRect (&rgndata->rdh.rcBound,
	       G_MAXSHORT, G_MAXSHORT, G_MINSHORT, G_MINSHORT);

      for (i = 0; i < data->clip_region->numRects; i++)
	{
	  rect = ((RECT *) rgndata->Buffer) + rgndata->rdh.nCount++;

	  rect->left = CLAMP (boxes[i].x1 + gc_private->clip_x_origin,
			      G_MINSHORT, G_MAXSHORT);
	  rect->right = CLAMP (boxes[i].x2 + gc_private->clip_x_origin,
			       G_MINSHORT, G_MAXSHORT);
	  rect->top = CLAMP (boxes[i].y1 + gc_private->clip_y_origin,
			     G_MINSHORT, G_MAXSHORT);
	  rect->bottom = CLAMP (boxes[i].y2 + gc_private->clip_y_origin,
				G_MINSHORT, G_MAXSHORT);

	  GDK_NOTE (MISC, g_print ("clip rgn box %d: %dx%d@+%d+%d\n",
				   i,
				   rect->right-rect->left,
				   rect->bottom-rect->top,
				   rect->left, rect->top));

	  if (rect->left < rgndata->rdh.rcBound.left)
	    rgndata->rdh.rcBound.left = rect->left;
	  if (rect->right > rgndata->rdh.rcBound.right)
	    rgndata->rdh.rcBound.right = rect->right;
	  if (rect->top < rgndata->rdh.rcBound.top)
	    rgndata->rdh.rcBound.top = rect->top;
	  if (rect->bottom > rgndata->rdh.rcBound.bottom)
	    rgndata->rdh.rcBound.bottom = rect->bottom;
	}
      if ((hrgn = ExtCreateRegion (NULL, nbytes, rgndata)) == NULL)
	WIN32_API_FAILED ("ExtCreateRegion"), ok = FALSE;

      if (ok && SelectClipRgn (data->xgc, hrgn) == ERROR)
	WIN32_API_FAILED ("SelectClipRgn"), ok = FALSE;

      if (hrgn != NULL)
	DeleteObject (hrgn);
    }
  else if (ok
	   && (data->values_mask & GDK_GC_CLIP_MASK)
	   && data->hcliprgn != NULL)
    {
      if (SelectClipRgn (data->xgc, data->hcliprgn) == ERROR)
	WIN32_API_FAILED ("SelectClipRgn"), ok = FALSE;
    }

  if (gdk_debug_flags & GDK_DEBUG_MISC)
    {
      HGDIOBJ obj;
      LOGBRUSH logbrush;
      EXTLOGPEN extlogpen;
      HRGN hrgn;
      RECT rect;

      g_print ("gdk_gc_predraw: %d: %#x\n", data, data->xgc);
      obj = GetCurrentObject (data->xgc, OBJ_BRUSH);
      GetObject (obj, sizeof (LOGBRUSH), &logbrush);
      g_print ("brush: style: %s color: %.06x hatch: %#x\n",
	       (logbrush.lbStyle == BS_HOLLOW ? "HOLLOW" :
		(logbrush.lbStyle == BS_PATTERN ? "PATTERN" :
		 (logbrush.lbStyle == BS_SOLID ? "SOLID" :
		  "???"))),
	       logbrush.lbColor,
	       logbrush.lbHatch);
      obj = GetCurrentObject (data->xgc, OBJ_PEN);
      GetObject (obj, sizeof (EXTLOGPEN), &extlogpen);
      g_print ("pen: type: %s style: %s endcap: %s join: %s width: %d brush: %s\n",
	       ((extlogpen.elpPenStyle & PS_TYPE_MASK) ==
		PS_GEOMETRIC ? "GEOMETRIC" : "COSMETIC"),
	       ((extlogpen.elpPenStyle & PS_STYLE_MASK) ==
		PS_NULL ? "NULL" :
		((extlogpen.elpPenStyle & PS_STYLE_MASK) ==
		 PS_SOLID ? "SOLID" : "???")),
	       ((extlogpen.elpPenStyle & PS_ENDCAP_MASK) ==
		PS_ENDCAP_FLAT ? "FLAT" :
		((extlogpen.elpPenStyle & PS_ENDCAP_MASK) ==
		 PS_ENDCAP_ROUND ? "ROUND" :
		 ((extlogpen.elpPenStyle & PS_ENDCAP_MASK) ==
		  PS_ENDCAP_SQUARE ? "ROUND" :
		  ((extlogpen.elpPenStyle & PS_ENDCAP_MASK) ==
		   PS_ENDCAP_SQUARE ? "ROUND" : "???")))),
	       ((extlogpen.elpPenStyle & PS_JOIN_MASK) ==
		PS_JOIN_BEVEL ? "BEVEL" :
		((extlogpen.elpPenStyle & PS_JOIN_MASK) ==
		 PS_JOIN_MITER ? "MITER" :
		 ((extlogpen.elpPenStyle & PS_JOIN_MASK) ==
		  PS_JOIN_ROUND ? "ROUND" : "???"))),
	       extlogpen.elpWidth,
	       (extlogpen.elpBrushStyle == BS_DIBPATTERN ? "DIBPATTERN" :
		(extlogpen.elpBrushStyle == BS_DIBPATTERNPT ? "DIBPATTERNPT" :
		 (extlogpen.elpBrushStyle == BS_HATCHED ? "HATCHED" :
		  (extlogpen.elpBrushStyle == BS_HOLLOW ? "HOLLOW" :
		   (extlogpen.elpBrushStyle == BS_PATTERN ? "PATTERN" :
		    (extlogpen.elpBrushStyle == BS_SOLID ? "SOLID" : "???")))))));
      hrgn = CreateRectRgn (0, 0, 0, 0);
      if ((flag = GetClipRgn (data->xgc, hrgn)) == -1)
	WIN32_API_FAILED ("GetClipRgn");
      else if (flag == 0)
	g_print ("no clip region\n");
      else if (flag == 1)
	{
	  GetRgnBox (hrgn, &rect);
	  g_print ("clip region bbox: %dx%d@+%d+%d\n",
		   rect.right - rect.left,
		   rect.bottom - rect.top,
		   rect.left, rect.top);
	}
    }

  return data->xgc;
}

void
gdk_gc_postdraw (GdkDrawable    *drawable,
		 GdkGCPrivate   *gc_private,
		 GdkGCValuesMask usage)
{
  GdkDrawablePrivate *drawable_private = (GdkDrawablePrivate *) drawable;
  GdkColormapPrivateWin32 *colormap_private =
    (GdkColormapPrivateWin32 *) drawable_private->colormap;
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc_private);
  HGDIOBJ hpen = NULL;
  HGDIOBJ hbr = NULL;

  if (usage & GDK_GC_FOREGROUND)
    {
      if ((hpen = GetCurrentObject (data->xgc, OBJ_PEN)) == NULL)
	WIN32_GDI_FAILED ("GetCurrentObject");

      if ((hbr = GetCurrentObject (data->xgc, OBJ_BRUSH)) == NULL)
	WIN32_GDI_FAILED ("GetCurrentObject");
    }

  if (!RestoreDC (data->xgc, data->saved_dc))
    WIN32_GDI_FAILED ("RestoreDC");
#if 0
  if (colormap_private != NULL
      && colormap_private->xcolormap->rc_palette
      && colormap_private->xcolormap->stale)
    {
      SelectPalette (data->xgc, GetStockObject (DEFAULT_PALETTE), FALSE);
      if (!UnrealizeObject (colormap_private->xcolormap->palette))
	WIN32_GDI_FAILED ("UnrealizeObject");
    }
#endif
  if (GDK_DRAWABLE_TYPE (drawable) == GDK_DRAWABLE_PIXMAP)
    {
      if (!DeleteDC (data->xgc))
	WIN32_GDI_FAILED ("DeleteDC");
    }
  else
    {
      ReleaseDC (data->hwnd, data->xgc);
    }

  if (hpen != NULL)
    if (!DeleteObject (hpen))
      WIN32_GDI_FAILED ("DeleteObject");
  
  if (hbr != NULL)
    if (!DeleteObject (hbr))
      WIN32_GDI_FAILED ("DeleteObject");

  data->xgc = NULL;
}

HDC
gdk_win32_hdc_get (GdkDrawable     *drawable,
		   GdkGC           *gc,
		   GdkGCValuesMask usage)
{
  return gdk_gc_predraw (drawable, (GdkGCPrivate *) gc, usage);
}

void
gdk_win32_hdc_release (GdkDrawable     *drawable,
		       GdkGC           *gc,
		       GdkGCValuesMask usage)
{
  gdk_gc_postdraw (drawable, (GdkGCPrivate *) gc, usage);
}

/* This function originally from Jean-Edouard Lachand-Robert, and
 * available at www.codeguru.com. Simplified for our needs, now
 * handles just one-bit deep bitmaps (in Window parlance, ie those
 * that GDK calls bitmaps (and not pixmaps), with zero pixels being
 * transparent.
 */

/*
 *  BitmapToRegion :  Create a region from the "non-transparent" pixels of
 *  a bitmap
 *  Author :      Jean-Edouard Lachand-Robert
 *  (http://www.geocities.com/Paris/LeftBank/1160/resume.htm), June 1998.
 */

HRGN
BitmapToRegion (HBITMAP hBmp)
{
  HRGN hRgn = NULL;
  HDC hMemDC;
  BITMAP bm;

  struct
  {
    BITMAPINFOHEADER bmiHeader;
#if 1
    WORD bmiColors[2];
#else
    RGBQUAD bmiColors[2];
#endif
  } bmi;
  VOID *pbits8; 
  HBITMAP hbm8;
  struct
  {
    WORD palVersion;
    WORD palNumEntries;
    PALETTEENTRY palPalEntry[2];
  } logpal;
  static HPALETTE bwPalette = NULL;

  HBITMAP holdBmp;
  HDC hDC;

  BITMAP bm8;
  HBITMAP holdBmp2;
  DWORD maxRects;
  RGNDATA *pData;
  BYTE *p8;
  int x, y;
  HRGN h;

  /* Create a B&W palette */
  if (bwPalette == NULL)
    {
      /* Create a b&w palette */
      logpal.palVersion = 0x300;
      logpal.palNumEntries = 2;
      logpal.palPalEntry[0].peRed = 
	logpal.palPalEntry[0].peGreen = 
	logpal.palPalEntry[0].peBlue = 0;
      logpal.palPalEntry[0].peFlags = 0;
      logpal.palPalEntry[1].peRed = 
	logpal.palPalEntry[1].peGreen = 
	logpal.palPalEntry[1].peBlue = 0xFF;
      logpal.palPalEntry[1].peFlags = 0;
      if ((bwPalette = CreatePalette ((LOGPALETTE *) &logpal)) == NULL)
	WIN32_GDI_FAILED ("CreatePalette");
    }

  /* Create a memory DC inside which we will scan the bitmap content */
  hMemDC = CreateCompatibleDC (NULL);
  if (!hMemDC)
    {
      WIN32_GDI_FAILED ("CreateCompatibleDC");
      return NULL;
    }

  SelectPalette (hMemDC, bwPalette, FALSE);
  RealizePalette (hMemDC);

  /* Get bitmap size */
  GetObject(hBmp, sizeof(bm), &bm);
  
  /* Create a 8 bits depth bitmap and select it into the memory DC */
  bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = bm.bmWidth;
  bmi.bmiHeader.biHeight = bm.bmHeight;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 8;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage = 0;
  bmi.bmiHeader.biXPelsPerMeter = 0;
  bmi.bmiHeader.biYPelsPerMeter = 0;
  bmi.bmiHeader.biClrUsed = 2;
  bmi.bmiHeader.biClrImportant = 2;
#if 1
  bmi.bmiColors[0] = 0;
  bmi.bmiColors[1] = 1;
  hbm8 = CreateDIBSection (hMemDC, (BITMAPINFO *)&bmi,
			    DIB_PAL_COLORS, &pbits8, NULL, 0);
#else
  bmi.bmiColors[0].rgbBlue =
    bmi.bmiColors[0].rgbGreen =
    bmi.bmiColors[0].rgbRed = 0x00;
  bmi.bmiColors[0].rgbReserved = 0x00;

  bmi.bmiColors[1].rgbBlue =
    bmi.bmiColors[1].rgbGreen =
    bmi.bmiColors[1].rgbRed = 0xFF;
  bmi.bmiColors[0].rgbReserved = 0x00;

  hbm8 = CreateDIBSection (hMemDC, (BITMAPINFO *)&bmi,
			    DIB_RGB_COLORS, &pbits8, NULL, 0);
#endif
  if (!hbm8)
    {
      WIN32_GDI_FAILED ("CreateDIBSection");
      DeleteDC (hMemDC);
      return NULL;
    }

  holdBmp = (HBITMAP) SelectObject (hMemDC, hbm8);

  /* Create a DC just to copy the bitmap into the memory DC*/
  hDC = CreateCompatibleDC (hMemDC);
  if (!hDC)
    {
      WIN32_GDI_FAILED ("CreateCompatibleDC");
      SelectObject (hMemDC, holdBmp);
      DeleteObject (hbm8);
      DeleteDC (hMemDC);
      return NULL;
    }

  /* Get how many bytes per row we have for the bitmap bits */
  GetObject (hbm8, sizeof (bm8), &bm8);

  /* Hans Breuer found a fix to the long-standing erroneous behaviour
   * on NT 4.0: There seems to be a bug in Win NT 4.0 GDI: scanlines
   * in bitmaps are dword aligned on both Win95 and NT. In the case of
   * a bitmap with 22 bytes worth of width, GetObject above returns
   * with bmWidth == 22. On Win95 bmWidthBytes == 24, as it should be,
   * but on NT is it 22. We need to correct this here.
   */
  bm8.bmWidthBytes = (((bm8.bmWidthBytes-1)/4)+1)*4; /* dword aligned!! */

  /* Copy the bitmap into the memory DC*/
  holdBmp2 = (HBITMAP) SelectObject (hDC, hBmp);

  if (!BitBlt (hMemDC, 0, 0, bm.bmWidth, bm.bmHeight, hDC, 0, 0, SRCCOPY))
    {
      WIN32_GDI_FAILED ("BitBlt");
      SelectObject (hDC, holdBmp2);
      SelectObject (hMemDC, holdBmp);
      DeleteObject (hbm8);
      DeleteDC (hMemDC);
      return NULL;
    }
  SelectObject (hDC, holdBmp2);
  DeleteDC (hDC);

  /* For better performances, we will use the ExtCreateRegion()
   * function to create the region. This function take a RGNDATA
   * structure on entry. We will add rectangles by amount of
   * ALLOC_UNIT number in this structure.
   */
  #define ALLOC_UNIT  100
  maxRects = ALLOC_UNIT;

  pData = g_malloc (sizeof (RGNDATAHEADER) + (sizeof (RECT) * maxRects));
  pData->rdh.dwSize = sizeof (RGNDATAHEADER);
  pData->rdh.iType = RDH_RECTANGLES;
  pData->rdh.nCount = pData->rdh.nRgnSize = 0;
  SetRect (&pData->rdh.rcBound, MAXLONG, MAXLONG, 0, 0);

  /* Scan each bitmap from bottom to top (the bitmap is inverted vertically)*/
  p8 = (BYTE *) pbits8 + (bm8.bmHeight - 1) * bm8.bmWidthBytes;
  for (y = 0; y < bm.bmHeight; y++)
    {
      /* Scan each bitmap row from left to right*/
      for (x = 0; x < bm.bmWidth; x++)
	{
	  /* Search for a continuous range of "non transparent pixels"*/
	  int x0 = x;
	  BYTE *p = p8 + x;
	  while (x < bm.bmWidth)
	    {
	      if (*p == 0)
		/* This pixel is "transparent"*/
		break;
	      p++;
	      x++;
	    }
	  
	  if (x > x0)
	    {
	      RECT *pr;
	      /* Add the pixels (x0, y) to (x, y+1) as a new rectangle
	       * in the region
	       */
	      if (pData->rdh.nCount >= maxRects)
		{
		  maxRects += ALLOC_UNIT;
		  pData = g_realloc (pData, sizeof(RGNDATAHEADER)
				     + (sizeof(RECT) * maxRects));
		}
	      pr = (RECT *) &pData->Buffer;
	      SetRect (&pr[pData->rdh.nCount], x0, y, x, y+1);
	      if (x0 < pData->rdh.rcBound.left)
		pData->rdh.rcBound.left = x0;
	      if (y < pData->rdh.rcBound.top)
		pData->rdh.rcBound.top = y;
	      if (x > pData->rdh.rcBound.right)
		pData->rdh.rcBound.right = x;
	      if (y+1 > pData->rdh.rcBound.bottom)
		pData->rdh.rcBound.bottom = y+1;
	      pData->rdh.nCount++;
	      
	      /* On Windows98, ExtCreateRegion() may fail if the
	       * number of rectangles is too large (ie: >
	       * 4000). Therefore, we have to create the region by
	       * multiple steps.
	       */
	      if (pData->rdh.nCount == 2000)
		{
		  HRGN h = ExtCreateRegion (NULL, sizeof(RGNDATAHEADER) + (sizeof(RECT) * maxRects), pData);
		  if (hRgn)
		    {
		      CombineRgn(hRgn, hRgn, h, RGN_OR);
		      DeleteObject(h);
		    }
		  else
		    hRgn = h;
		  pData->rdh.nCount = 0;
		  SetRect (&pData->rdh.rcBound, MAXLONG, MAXLONG, 0, 0);
		}
	    }
	}
      
      /* Go to next row (remember, the bitmap is inverted vertically)*/
      p8 -= bm8.bmWidthBytes;
    }
  
  /* Create or extend the region with the remaining rectangles*/
  h = ExtCreateRegion (NULL, sizeof (RGNDATAHEADER)
		       + (sizeof (RECT) * maxRects), pData);
  if (hRgn)
    {
      CombineRgn (hRgn, hRgn, h, RGN_OR);
      DeleteObject (h);
    }
  else
    hRgn = h;

  /* Clean up*/
  SelectObject(hMemDC, holdBmp);
  DeleteObject (hbm8);
  DeleteDC (hMemDC);

  return hRgn;
}
