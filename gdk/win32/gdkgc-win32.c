/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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
#include "gdkprivate.h"
#include "gdkwin32.h"

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
gc_values_to_win32values (GdkGCValues    *values,
			  GdkGCValuesMask mask,
			  GdkGCWin32Data *data)
{				    
  char *s = "";
  gint sw, sh;

  GDK_NOTE (GC, g_print ("{"));

  if (mask & GDK_GC_FOREGROUND)
    {
      data->foreground = values->foreground.pixel;
      data->values_mask |= GDK_GC_FOREGROUND;
      GDK_NOTE (GC, (g_print ("fg=%06lx", data->foreground),
		     s = ","));
    }
  
  if (mask & GDK_GC_BACKGROUND)
    {
      data->background = values->background.pixel;
      data->values_mask |= GDK_GC_BACKGROUND;
      GDK_NOTE (GC, (g_print ("%sbg=%06lx", s, data->background),
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
	  GDK_NOTE (GC, (xlfd = gdk_font_full_name_get (data->font),
			 g_print ("%sfont=%s", s, xlfd),
			 s = ",",
			 gdk_font_full_name_free (xlfd)));
	}
      else
	{
	  data->values_mask &= ~GDK_GC_FONT;
	  GDK_NOTE (GC, (g_print ("%sfont=NULL", s),
			   s = ","));
	}
    }

  if (mask & GDK_GC_FUNCTION)
    {
      GDK_NOTE (GC, (g_print ("%srop2=%s ",
			      s,
			      gdk_win32_function_to_string (values->function)),
		     s = ","));
      switch (values->function)
	{
	case GDK_COPY:
	  data->rop2 = R2_COPYPEN;
	  GDK_NOTE (GC, g_print ("(COPYPEN)"));
	  break;
	case GDK_INVERT:
	  data->rop2 = R2_NOT;
	  GDK_NOTE (GC, g_print ("(NOT)"));
	  break;
	case GDK_XOR:
	  data->rop2 = R2_XORPEN;
	  GDK_NOTE (GC, g_print ("(XORPEN)"));
	  break;
	case GDK_CLEAR:
	  data->rop2 = R2_BLACK;
	  GDK_NOTE (GC, g_print ("(BLACK)"));
	  break;
	case GDK_AND:
	  data->rop2 = R2_MASKPEN;
	  GDK_NOTE (GC, g_print ("(MASKPEN)"));
	  break;
	case GDK_AND_REVERSE:
	  data->rop2 = R2_MASKPENNOT;
	  GDK_NOTE (GC, g_print ("(MASKPENNOT)"));
	  break;
	case GDK_AND_INVERT:
	  data->rop2 = R2_MASKNOTPEN;
	  GDK_NOTE (GC, g_print ("(MASKNOTPEN)"));
	  break;
	case GDK_NOOP:
	  data->rop2 = R2_NOP;
	  GDK_NOTE (GC, g_print ("(NOP)"));
	  break;
	case GDK_OR:
	  data->rop2 = R2_MERGEPEN;
	  GDK_NOTE (GC, g_print ("(MERGEPEN)"));
	  break;
	case GDK_EQUIV:
	  data->rop2 = R2_NOTXORPEN;
	  GDK_NOTE (GC, g_print ("(NOTXORPEN)"));
	  break;
	case GDK_OR_REVERSE:
	  data->rop2 = R2_MERGEPENNOT;
	  GDK_NOTE (GC, g_print ("(MERGEPENNOT)"));
	  break;
	case GDK_COPY_INVERT:
	  data->rop2 = R2_NOTCOPYPEN;
	  GDK_NOTE (GC, g_print ("(NOTCOPYPEN)"));
	  break;
	case GDK_OR_INVERT:
	  data->rop2 = R2_MERGENOTPEN;
	  GDK_NOTE (GC, g_print ("(MERGENOTPEN)"));
	  break;
	case GDK_NAND:
	  data->rop2 = R2_NOTMASKPEN;
	  GDK_NOTE (GC, g_print ("(NOTMASKPEN)"));
	  break;
	case GDK_SET:
	  data->rop2 = R2_WHITE;
	  GDK_NOTE (GC, g_print ("(WHITE)"));
	  break;
	}
      GDK_NOTE (GC, g_print (" "));
      data->values_mask |= GDK_GC_FUNCTION;
    }

  if (mask & GDK_GC_FILL)
    {
      data->fill_style = values->fill;
      data->values_mask |= GDK_GC_FILL;
      GDK_NOTE (GC,
		(g_print ("%sfill=%s",
			  s,
			  gdk_win32_fill_style_to_string (data->fill_style)),
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
	  GDK_NOTE (GC, (g_print ("%stile=%p", s,
				  GDK_DRAWABLE_XID (data->tile)),
			 s = ","));
	}
      else
	{
	  data->values_mask &= ~GDK_GC_TILE;
	  GDK_NOTE (GC, (g_print ("%stile=NULL", s),
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
	  GDK_NOTE (GC, (g_print ("%sstipple=%p", s,
				  GDK_DRAWABLE_XID (data->stipple)),
			 s = ","));
	}
      else
	{
	  data->values_mask &= ~GDK_GC_STIPPLE;
	  GDK_NOTE (GC, (g_print ("%sstipple=NULL", s),
			 s = ","));
	}
    }

  if (mask & GDK_GC_CLIP_MASK)
    {
      if (data->clip_region != NULL)
	if (!DeleteObject (data->clip_region))
	  WIN32_GDI_FAILED ("DeleteObject");
      if (values->clip_mask != NULL)
	{
	  data->clip_region = 
	    gdk_win32_bitmap_to_region (values->clip_mask);
	  data->values_mask |= GDK_GC_CLIP_MASK;
	}
      else
	{
	  data->clip_region = NULL;
	  data->values_mask &= ~GDK_GC_CLIP_MASK;
	}
      GDK_NOTE (GC, (g_print ("%sclip=%p", s, data->clip_region),
		     s = ","));
    }

  if (mask & GDK_GC_SUBWINDOW)
    {
      data->subwindow_mode = values->subwindow_mode;
      data->values_mask |= GDK_GC_SUBWINDOW;
      GDK_NOTE (GC, (g_print ("%ssubw=%d", s, data->subwindow_mode),
		     s = ","));
    }

  if (mask & GDK_GC_TS_X_ORIGIN)
    {
      data->ts_x_origin = values->ts_x_origin;
      data->values_mask |= GDK_GC_TS_X_ORIGIN;
      GDK_NOTE (GC, (g_print ("%sts_x=%d", s, data->ts_x_origin),
		     s = ","));
    }

  if (mask & GDK_GC_TS_Y_ORIGIN)
    {
      data->ts_y_origin = values->ts_y_origin;
      data->values_mask |= GDK_GC_TS_Y_ORIGIN;
      GDK_NOTE (GC, (g_print ("%sts_y=%d", s, data->ts_y_origin),
		     s = ","));
    }

  if (mask & GDK_GC_CLIP_X_ORIGIN)
    {
      data->clip_x_origin = values->clip_x_origin;
      data->values_mask |= GDK_GC_CLIP_X_ORIGIN;
      GDK_NOTE (GC, (g_print ("%sclip_x=%d", s, data->clip_x_origin),
		     s = ","));
    }

  if (mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      data->clip_y_origin = values->clip_y_origin; 
      data->values_mask |= GDK_GC_CLIP_Y_ORIGIN;
      GDK_NOTE (GC, (g_print ("%sclip_y=%d", s, data->clip_y_origin),
		     s = ","));
   }
 
  if (mask & GDK_GC_EXPOSURES)
    {
      data->graphics_exposures = values->graphics_exposures;
      data->values_mask |= GDK_GC_EXPOSURES;
      GDK_NOTE (GC, (g_print ("%sexp=%d", s, data->graphics_exposures),
		     s = ","));
    }

  if (mask & GDK_GC_LINE_WIDTH)
    {
      data->pen_width = values->line_width;
      data->values_mask |= GDK_GC_LINE_WIDTH;
      GDK_NOTE (GC, (g_print ("%spw=%d", s, data->pen_width),
		     s = ","));
    }

  if (mask & GDK_GC_LINE_STYLE)
    {
      data->pen_style &= ~(PS_STYLE_MASK);
      GDK_NOTE (GC, (g_print ("%sps|=", s),
		     s = ","));
      switch (values->line_style)
	{
	case GDK_LINE_SOLID:
	  GDK_NOTE (GC, g_print ("LINE_SOLID"));
	  data->pen_style |= PS_SOLID;
	  break;
	case GDK_LINE_ON_OFF_DASH:
	case GDK_LINE_DOUBLE_DASH: /* ??? */
	  GDK_NOTE (GC, g_print ("DASH"));
	  data->pen_style |= PS_DASH;
	  break;
	}
      data->values_mask |= GDK_GC_LINE_STYLE;
    }

  if (mask & GDK_GC_CAP_STYLE)
    {
      data->pen_style &= ~(PS_ENDCAP_MASK);
      GDK_NOTE (GC, (g_print ("%sps|=", s),
		     s = ","));
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:	/* ??? */
	case GDK_CAP_BUTT:
	  GDK_NOTE (GC, g_print ("ENDCAP_FLAT"));
	  data->pen_style |= PS_ENDCAP_FLAT;
	  break;
	case GDK_CAP_ROUND:
	  GDK_NOTE (GC, g_print ("ENDCAP_ROUND"));
	  data->pen_style |= PS_ENDCAP_ROUND;
	  break;
	case GDK_CAP_PROJECTING:
	  GDK_NOTE (GC, g_print ("ENDCAP_SQUARE"));
	  data->pen_style |= PS_ENDCAP_SQUARE;
	  break;
	}
      data->values_mask |= GDK_GC_CAP_STYLE;
    }

  if (mask & GDK_GC_JOIN_STYLE)
    {
      data->pen_style &= ~(PS_JOIN_MASK);
      GDK_NOTE (GC, (g_print ("%sps|=", s),
		     s = ","));
      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  GDK_NOTE (GC, g_print ("JOIN_MITER"));
	  data->pen_style |= PS_JOIN_MITER;
	  break;
	case GDK_JOIN_ROUND:
	  GDK_NOTE (GC, g_print ("JOIN_ROUND"));
	  data->pen_style |= PS_JOIN_ROUND;
	  break;
	case GDK_JOIN_BEVEL:
	  GDK_NOTE (GC, g_print ("JOIN_BEVEL"));
	  data->pen_style |= PS_JOIN_BEVEL;
	  break;
	}
      data->values_mask |= GDK_GC_JOIN_STYLE;
    }
  GDK_NOTE (GC, g_print ("}"));
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
    
  data->foreground = 0;
  data->background = 1;
  data->font = NULL;
  data->rop2 = R2_COPYPEN;
  data->fill_style = GDK_SOLID;
  data->tile = NULL;
  data->stipple = NULL;
  data->clip_region = NULL;
  data->ts_x_origin = data->ts_y_origin =
    data->clip_x_origin = data->clip_y_origin = 0;
  data->pen_style = PS_GEOMETRIC|PS_ENDCAP_FLAT|PS_JOIN_MITER;
  data->pen_width = 0;

  data->values_mask = GDK_GC_FUNCTION | GDK_GC_FILL;

  GDK_NOTE (GC, g_print ("_gdk_win32_gc_new: "));
  gc_values_to_win32values (values, mask, data);

  data->hwnd = NULL;
  data->xgc = NULL;

  GDK_NOTE (GC, g_print (" = %p\n", gc));

  return gc;
}

static void
gdk_win32_gc_destroy (GdkGC *gc)
{
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc);

  if (data->values_mask & GDK_GC_FONT)
    gdk_font_unref (data->font);
  
  if (data->values_mask & GDK_GC_TILE)
    gdk_drawable_unref (data->tile);
  
  if (data->values_mask & GDK_GC_STIPPLE)
    gdk_drawable_unref (data->stipple);
  
  if (data->values_mask & GDK_GC_CLIP_MASK)
    DeleteObject (data->clip_region);

  g_free (GDK_GC_WIN32DATA (gc));
}

static void
gdk_win32_gc_get_values (GdkGC       *gc,
			 GdkGCValues *values)
{
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
    case R2_WHITE:
      values->function = GDK_SET; break;
    }

  values->fill = data->fill_style;

  values->tile = data->tile;
  values->stipple = data->stipple;
  values->clip_mask = NULL;

  if (data->clip_region != NULL)
    {
      RECT rect;
      HBRUSH hbr;
      HDC hdc;
      HGDIOBJ oldbitmap;
      GdkPixmap *pixmap;
      gboolean ok = TRUE;

       /* Build pixmap for clip region; if anything fails, do so w/o
	* invoking more code on failed objects, just in case this is
	* the cause of one of the rare crashes we're seeing
	*/
      GetRgnBox (data->clip_region, &rect);
      pixmap = gdk_pixmap_new (NULL, rect.right - rect.left,
			       rect.bottom - rect.top, 1);

      if ((hbr = GetStockObject (WHITE_BRUSH)) == NULL)
	WIN32_GDI_FAILED ("GetStockObject"), ok = FALSE;

      if (ok && (hdc = CreateCompatibleDC (NULL)) == NULL)
	WIN32_GDI_FAILED ("CreateCompatibleDC"), ok = FALSE;

      if (ok && (oldbitmap = SelectObject (hdc, GDK_DRAWABLE_XID (pixmap))) == NULL)
	WIN32_GDI_FAILED ("SelectObject"), ok = FALSE;

      if (ok && (hbr = GetStockObject (BLACK_BRUSH)) == NULL)
	WIN32_GDI_FAILED ("GetStockObject"), ok = FALSE;
	  
      if (ok && !FillRect (hdc, &rect, hbr))
	WIN32_GDI_FAILED ("FillRect"), ok = FALSE;

      if (ok && (hbr = GetStockObject (WHITE_BRUSH)) == NULL)
	WIN32_GDI_FAILED ("GetStockObject"), ok = FALSE;
      
      if (ok && !FillRgn (hdc, data->clip_region, hbr))
	WIN32_GDI_FAILED ("FillRgn"), ok = FALSE;

      if (hdc != NULL && oldbitmap != NULL && SelectObject (hdc, oldbitmap) == NULL)
	WIN32_GDI_FAILED ("SelectObject"), ok = FALSE;

      if (hdc != NULL)
	DeleteDC (hdc);

      if (ok)
	values->clip_mask = pixmap;
      else if (pixmap != NULL)
	gdk_drawable_unref (pixmap);
    }

  values->subwindow_mode = data->subwindow_mode;
  values->ts_x_origin = data->ts_x_origin;
  values->ts_y_origin = data->ts_y_origin;
  values->clip_x_origin = data->clip_x_origin;
  values->clip_y_origin = data->clip_y_origin;
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
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc);

  GDK_NOTE (GC, g_print ("gdk_win32_gc_set_values: "));

  gc_values_to_win32values (values, mask, data);

  GDK_NOTE (GC, g_print ("\n"));
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

  /* Set the extended line style. This could be done by PS_USERSTYLE
   * and ExtCreatePen; but ONLY on WinNT, so let's make a guess (based
   * on the implementation in DIA). On Win9x this does only work for
   * lines with width one ...
   *
   * More workarounds for Win9x descibed at:
   * http://www.codeguru.com/gdi/dashed.shtml
   */
  if (!IS_WIN_NT (windows_version) && data->pen_width > 1)
    {
      GDK_NOTE (GC, g_print ("gdk_win32_gc_set_dashes: not fully supported\n"));
      data->pen_style |= PS_SOLID;
      return;
    }
  
  data->pen_style &= ~(PS_STYLE_MASK);

  if (2 == n)
    {
      if ((dash_list[0] == dash_list[1]) && (dash_list[0] > 2))
        {
          data->pen_style |= PS_DASH;
          GDK_NOTE (GC, g_print("gdk_win32_gc_set_dashes: PS_DASH (%d,%d)\n", 
				dash_list[0], dash_list[1]));
        }
      else
        {
          data->pen_style |= PS_DOT;
          GDK_NOTE (GC, g_print("gdk_win32_gc_set_dashes: PS_DOT (%d,%d)\n", 
				dash_list[0], dash_list[1]));
        }
    }
  else if (4 == n)
    {
      data->pen_style |= PS_DASHDOT; 
      GDK_NOTE (GC, g_print("gdk_win32_gc_set_dashes: PS_DASHDOT (%d,%d,%d,%d)\n", 
			    dash_list[0], dash_list[1],
			    dash_list[2], dash_list[3]));
    }
  else if (6 == n)
    {
      data->pen_style |= PS_DASHDOTDOT; 
      GDK_NOTE (GC, g_print("gdk_win32_gc_set_dashes: PS_DASHDOTDOT (%d,%d,%d,%d,%d,%d)\n", 
			    dash_list[0], dash_list[1],
			    dash_list[2], dash_list[3],
			    dash_list[4], dash_list[5]));
    }
  else
    {
      data->pen_style |= PS_DASH;
      GDK_NOTE (GC, g_print("gdk_win32_gc_set_dashes: no guess for %d dashes\n", n));
    }
}

void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkGCWin32Data *data;
   
  g_return_if_fail (gc != NULL);

  data = GDK_GC_WIN32DATA (gc);

  if (data->clip_region != NULL)
    if (!DeleteObject (data->clip_region))
      WIN32_GDI_FAILED ("DeleteObject");
  if (rectangle)
    {
      GDK_NOTE (GC, g_print ("gdk_gc_set_clip_rectangle: (%p) %dx%d@+%d+%d\n",
			     data,
			     rectangle->width, rectangle->height,
			     rectangle->x, rectangle->y));
      if ((data->clip_region =
	   CreateRectRgn (rectangle->x, rectangle->y,
			  rectangle->x + rectangle->width,
			  rectangle->y + rectangle->height)) == NULL)
	WIN32_GDI_FAILED ("CreateRectRgn");

      data->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (GC, g_print ("gdk_gc_set_clip_rectangle: (%p) NULL\n",
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
  GdkGCWin32Data *data;

  g_return_if_fail (gc != NULL);

  data = GDK_GC_WIN32DATA (gc);

  GDK_NOTE (GC, g_print ("gdk_gc_set_clip_region: (%p) %s\n",
			 data, (region != NULL ? "xxx" : "None")));

  if (data->clip_region != NULL)
    if (!DeleteObject (data->clip_region))
      WIN32_GDI_FAILED ("DeleteObject");
  if (region)
    {
      GdkRegionPrivate *region_private;

      region_private = (GdkRegionPrivate*) region;
      data->clip_region = CreateRectRgn (1, 1, 0, 0);
      CombineRgn (data->clip_region, region_private->xregion, NULL, RGN_COPY);
      data->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      data->clip_region = NULL;
      data->values_mask &= ~GDK_GC_CLIP_MASK;
    }
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
    if (!DeleteObject (dst_data->clip_region))
      WIN32_GDI_FAILED ("DeleteObject");
  
  *dst_data = *src_data;

  if (dst_data->clip_region != NULL)
    {
      nbytes = GetRegionData (dst_data->clip_region, 0, NULL);
      rgn = g_malloc (nbytes);
      GetRegionData (dst_data->clip_region, nbytes, rgn);
      if ((dst_data->clip_region = ExtCreateRegion (NULL, nbytes, rgn)) == NULL)
	WIN32_GDI_FAILED ("ExtCreateRegion");
      g_free (rgn);
    }
  
  if (dst_data->font != NULL)
    gdk_font_ref (dst_data->font);
  if (dst_data->tile != NULL)
    gdk_drawable_ref (dst_data->tile);
  if (dst_data->stipple != NULL)
    gdk_drawable_ref (dst_data->stipple);
}
struct _GdkPenCacheEntry
{
  gboolean in_use;
  HPEN hpen;
  COLORREF fg;
  DWORD pen_style;
  gint width;
  gint use_count;
};

typedef struct _GdkPenCacheEntry GdkPenCacheEntry;

#define gdk_pen_cache_entry_count 3
#define INIT_PEN_ENTRY { FALSE, NULL, 0, 0, 0, 0 }
static GdkPenCacheEntry gdk_pen_cache[gdk_pen_cache_entry_count] = {
  INIT_PEN_ENTRY,
  INIT_PEN_ENTRY,
  INIT_PEN_ENTRY,
};

static HPEN
obtain_pen (COLORREF fg,
            DWORD pen_style,
            gint width)
{
  LOGBRUSH logbrush;
  HPEN hpen;
  GdkPenCacheEntry* unused_entry = NULL;
  int i;

  if (width < 1)
    width = 1;
    
  for (i = 0; i < gdk_pen_cache_entry_count; i++)
    {
      GdkPenCacheEntry* entry = &gdk_pen_cache[i];
      if (!entry->in_use)
        {
          if (entry->hpen != NULL && entry->fg == fg 
              && entry->pen_style == pen_style && entry->width == width)
            {
              entry->in_use = TRUE;
              entry->use_count += 2;
              return entry->hpen;
            }
          if (entry->use_count > 0)
            entry->use_count--;
          if (unused_entry == NULL || entry->use_count < unused_entry->use_count)
            unused_entry = entry;
        }
    }
    
  /* Create pen. */
  logbrush.lbStyle = BS_SOLID;
  logbrush.lbColor = fg;
  logbrush.lbHatch = 0;

  if ((hpen = ExtCreatePen (pen_style, width, &logbrush, 0, NULL)) == NULL)
    {
      WIN32_GDI_FAILED ("ExtCreatePen");
      return NULL;
    }
    
  if (unused_entry != NULL)
    {
      if (unused_entry->hpen != NULL)
        {
          if (!DeleteObject (unused_entry->hpen))
            WIN32_GDI_FAILED ("DeleteObject");
        }          
      unused_entry->hpen = hpen;
      unused_entry->fg = fg;
      unused_entry->pen_style = pen_style;
      unused_entry->width = width;
      unused_entry->in_use = TRUE;
      unused_entry->use_count += 2;
    }
    
    return hpen;
}

static void
release_pen (HPEN hpen)
{
  int i;

  if (hpen == NULL)
    return;
    
  for (i = 0; i < gdk_pen_cache_entry_count; i++)
    {
      GdkPenCacheEntry* entry = &gdk_pen_cache[i];
      if (entry->hpen == hpen)
        {
          entry->in_use = FALSE;
          return;
        }
    }

  if (!DeleteObject (hpen))
     WIN32_GDI_FAILED ("DeleteObject");
  GDK_NOTE (GC, g_print ("release_pen: deleted %p\n", hpen));
}
  
struct _GdkHDCCacheEntry
{
  BOOL in_use;
  BOOL offscreen;
  HDC hdc;
  HWND hwnd;
  HBITMAP holdbitmap;
  gboolean pen_set;
  COLORREF pen_fg;
  GdkLineStyle pen_style;
  gint pen_width;
};

typedef struct _GdkHDCCacheEntry GdkHDCCacheEntry;

#define gdk_hdc_cache_entry_count 3
#define INIT_HDC_ENTRY   { FALSE, FALSE, NULL, NULL, FALSE, 0, 0, 0 }
static GdkHDCCacheEntry gdk_hdc_cache[gdk_hdc_cache_entry_count] = {
  INIT_HDC_ENTRY,
  INIT_HDC_ENTRY,
  INIT_HDC_ENTRY,
};
static gboolean gdk_hdc_clear_idle_proc_installed = FALSE;
#undef INIT_HDC_ENTRY

void
gdk_win32_destroy_hdc (HWND hwnd,
                       HDC  hdc)
{
  HPEN hpen;
  HBRUSH hbr;
  
  if ((hpen = GetCurrentObject (hdc, OBJ_PEN)) == NULL)
    WIN32_GDI_FAILED ("GetCurrentObject");

  if ((hbr = GetCurrentObject (hdc, OBJ_BRUSH)) == NULL)
    WIN32_GDI_FAILED ("GetCurrentObject");

  if (hwnd == NULL)
    {
      if (!DeleteDC (hdc))
	WIN32_GDI_FAILED ("DeleteDC");
    }
  else
    {
      if (!ReleaseDC (hwnd, hdc))
        WIN32_GDI_FAILED ("ReleaseDC");
    }

  release_pen (hpen);
  
  if (!DeleteObject (hbr))
    WIN32_GDI_FAILED ("DeleteObject");
}

static void
clear_hdc_cache_entry (GdkHDCCacheEntry *entry)
{
  if (entry->hdc != NULL)
    {
      if (entry->holdbitmap != NULL)
	{
	  GDK_NOTE (GC, g_print ("clear_hdc_cache_entry: deselecting %p from %p\n",
				 entry->hwnd, entry->hdc));
	  if (SelectObject (entry->hdc, entry->holdbitmap) == NULL)
	    WIN32_GDI_FAILED ("SelectObject");
	}

      if (entry->offscreen)
        gdk_win32_destroy_hdc (NULL, entry->hdc);
      else
        gdk_win32_destroy_hdc (entry->hwnd, entry->hdc);
    }
  entry->in_use = FALSE;
  entry->offscreen = FALSE;
  entry->hdc = NULL;
  entry->hwnd = NULL;
  entry->holdbitmap = NULL;
  entry->pen_set = FALSE;
  entry->pen_fg = 0;
  entry->pen_style = 0;
  entry->pen_width = 0;
}

void gdk_win32_clear_hdc_cache (void)
{
  int i;

  for (i = 0; i < gdk_hdc_cache_entry_count; i++)
    clear_hdc_cache_entry (&gdk_hdc_cache[i]);
}

static gboolean
gdk_clear_hdc_cache_on_idle (gpointer arg)
{
  gdk_win32_clear_hdc_cache ();
  gdk_hdc_clear_idle_proc_installed = FALSE;
  return FALSE;
}

static GdkHDCCacheEntry*
find_free_hdc_entry (void)
{
  int i;
  GdkHDCCacheEntry* not_in_use = NULL;

  if (!gdk_hdc_clear_idle_proc_installed) 
    {
      g_idle_add_full (G_PRIORITY_HIGH, gdk_clear_hdc_cache_on_idle, NULL,
                       NULL);
      gdk_hdc_clear_idle_proc_installed = TRUE;
    }

  for (i = 0; i < gdk_hdc_cache_entry_count; i++)
    {
      if (gdk_hdc_cache[i].hdc == NULL)
        return &gdk_hdc_cache[i];
      else if (!gdk_hdc_cache[i].in_use)
        not_in_use = &gdk_hdc_cache[i];
    }

  if (not_in_use != NULL)
    {
      clear_hdc_cache_entry (not_in_use);
      return not_in_use;
    }

  return NULL;
}
 
static GdkHDCCacheEntry*  
find_hdc_entry_for_hwnd (HWND hwnd)
{
  int i;

  for (i = 0; i < gdk_hdc_cache_entry_count; i++)
    {
      if (gdk_hdc_cache[i].hwnd == hwnd)
        return &gdk_hdc_cache[i];
    }

  return NULL;
}
 
static GdkHDCCacheEntry*  
find_hdc_entry_for_hdc (HDC hdc)
{
  int i;

  for (i = 0; i < gdk_hdc_cache_entry_count; i++)
    {
      if (gdk_hdc_cache[i].hdc == hdc)
        return &gdk_hdc_cache[i];
    }

  return NULL;
}

void
gdk_win32_clear_hdc_cache_for_hwnd (HWND hwnd)
{
  GdkHDCCacheEntry *entry;

  entry = find_hdc_entry_for_hwnd (hwnd);
  if (entry != NULL)
    clear_hdc_cache_entry (entry);
}

HDC
gdk_win32_obtain_window_hdc (HWND hwnd)
{
  HDC hdc;
  GdkHDCCacheEntry *entry;

  entry = find_hdc_entry_for_hwnd (hwnd);
  if (entry != NULL && !entry->in_use)
    {
      entry->in_use = TRUE;
      return entry->hdc;
    }

  if ((entry = find_free_hdc_entry ()) == NULL)
    return NULL;

  if ((hdc = GetDC(hwnd)) == NULL)
    WIN32_GDI_FAILED("GetDC");
  else
    {
      entry->in_use = TRUE;
      entry->offscreen = FALSE;
      entry->hdc = hdc;
      entry->hwnd = hwnd;
      entry->holdbitmap = NULL;
    }

  return hdc;
}

void 
gdk_win32_release_hdc (HWND hwnd,
                       HDC  hdc)
{
  GdkHDCCacheEntry *entry;

  entry = find_hdc_entry_for_hdc (hdc);
  if (entry != NULL)
    entry->in_use = FALSE;
  else 
    gdk_win32_destroy_hdc (hwnd, hdc);
}

HDC
gdk_win32_obtain_offscreen_hdc (HWND hwnd)
{
  HDC hdc;
  HBITMAP holdbitmap;
  GdkHDCCacheEntry *entry;

  entry = find_hdc_entry_for_hwnd (hwnd);
  if (entry != NULL && !entry->in_use)
    {
      entry->in_use = TRUE;
      return entry->hdc;
    }

  entry = find_free_hdc_entry ();
  if (entry == NULL)
    return NULL;

  if ((hdc = CreateCompatibleDC (NULL)) == NULL)
    {
      WIN32_GDI_FAILED ("CreateCompatibleDC");
      return NULL;
    }
      
  if ((holdbitmap = SelectObject (hdc, hwnd)) == NULL)
    {
      WIN32_GDI_FAILED ("SelectObject");
      DeleteDC (hdc);
      return NULL;
    }
  GDK_NOTE (GC, g_print ("gdk_win32_obtain_offscreen_hdc: selected %p into %p\n",
			 hwnd, hdc));

  entry->in_use = TRUE;
  entry->offscreen = TRUE;
  entry->hdc = hdc;
  entry->hwnd = hwnd;
  entry->holdbitmap = holdbitmap;

  return hdc;
}

static void
set_hdc_pen (HDC          hdc,
	     COLORREF     fg,
	     GdkLineStyle style,
	     gint         width)
{
  HPEN hpen, old_hpen;
  GdkHDCCacheEntry *entry;

  if (width < 1)
    width = 1;
    
  entry = find_hdc_entry_for_hdc (hdc);
  if (entry != NULL && entry->pen_set && entry->pen_fg == fg
      && entry->pen_style == style && entry->pen_width == width)
    return;
    
  /* Create and select pen. */
  hpen = obtain_pen (fg, style, width);
  if (hpen == NULL)
    return;
    
  old_hpen = SelectObject (hdc, hpen);
  if (old_hpen == NULL)
    {
      WIN32_GDI_FAILED ("SelectObject");
      release_pen (hpen);
      return;
    }
  else
    {
      GDK_NOTE (GC, g_print ("set_hdc_pen: selected %p into %p\n", hpen, hdc));
      release_pen (old_hpen);
    }
    
  if (entry != NULL)
    {
      entry->pen_set = TRUE;
      entry->pen_fg = fg;
      entry->pen_style = style;
      entry->pen_width = width;
    }
}

static guint bitmask[9] = { 0, 1, 3, 7, 15, 31, 63, 127, 255 };

COLORREF
gdk_win32_colormap_color_pack (GdkColormapPrivateWin32 *colormap_private,
			       gulong                   pixel)
{
  GdkVisual *visual;
  guchar r, g, b;

  if (colormap_private == NULL)
    return DIBINDEX (pixel & 1);

  visual = colormap_private->base.visual;
  switch (visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
    case GDK_VISUAL_STATIC_COLOR:
      return PALETTEINDEX (pixel);

    case GDK_VISUAL_TRUE_COLOR:
      r = (pixel & visual->red_mask) >> visual->red_shift;
      r = (r * 255) / bitmask[visual->red_prec];
      g = (pixel & visual->green_mask) >> visual->green_shift;
      g = (g * 255) / bitmask[visual->green_prec];
      b = (pixel & visual->blue_mask) >> visual->blue_shift;
      b = (b * 255) / bitmask[visual->blue_prec];
      return RGB (r, g, b);

    default:
      g_assert_not_reached ();
      return 0;
    }
}

void
gdk_win32_colormap_color_unpack (GdkColormapPrivateWin32 *colormap_private,
				 COLORREF                 color,
				 GdkColor                *result)
{
  GdkVisual *visual;

  if (colormap_private == NULL)
    {
      result->pixel = (color & 0xFFFF);
      return;
    }

  visual = colormap_private->base.visual;
  switch (visual->type)
    {
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_PSEUDO_COLOR:
    case GDK_VISUAL_STATIC_COLOR:
      result->pixel = (color & 0xFFFF);
      return;

    case GDK_VISUAL_TRUE_COLOR:
      result->red = (GetRValue (color) * 65535) / 255;
      result->green = (GetGValue (color) * 65535) / 255;
      result->blue = (GetBValue (color) * 65535) / 255;
      result->pixel =
	(((GetRValue (color) >> (8 - visual->red_prec)) << visual->red_shift) +
	 ((GetGValue (color) >> (8 - visual->green_prec)) << visual->green_shift) +
	 ((GetBValue (color) >> (8 - visual->blue_prec)) << visual->blue_shift));
      return;

    default:
      g_assert_not_reached ();
      return;
    }
}

static void
predraw_set_foreground (GdkGCWin32Data          *data,
			GdkColormapPrivateWin32 *colormap_private)
{
  COLORREF fg;
  HBRUSH hbr;

  if (colormap_private &&
      (colormap_private->base.visual->type == GDK_VISUAL_PSEUDO_COLOR ||
       colormap_private->base.visual->type == GDK_VISUAL_STATIC_COLOR))
    {
      gint k;
      
      if (!(data->holdpal = SelectPalette (data->xgc, colormap_private->hpal, FALSE)))
	WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (data->xgc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else if (k > 0)
	GDK_NOTE (COLORMAP, g_print ("predraw_set_foreground: realized %p: %d colors\n",
				     colormap_private->hpal, k));
    }

  fg = gdk_win32_colormap_color_pack (colormap_private, data->foreground);

  GDK_NOTE (GC, g_print ("predraw_set_foreground: fg=%06lx\n", fg));

  if (SetTextColor (data->xgc, fg) == CLR_INVALID)
    WIN32_GDI_FAILED ("SetTextColor");

  set_hdc_pen (data->xgc, fg, data->pen_style, data->pen_width);
  
  switch (data->fill_style)
    {
    case GDK_OPAQUE_STIPPLED:
      if ((hbr = CreatePatternBrush (GDK_DRAWABLE_XID (data->stipple))) == NULL)
	WIN32_GDI_FAILED ("CreatePatternBrush");
	
      SetBrushOrgEx (data->xgc, data->ts_x_origin,
		     data->ts_y_origin, NULL);

      break;

    case GDK_SOLID:
    default:
      if ((hbr = CreateSolidBrush (fg)) == NULL)
	WIN32_GDI_FAILED ("CreateSolidBrush");
      break;
  }
  if (hbr != NULL)
    {
      HBRUSH old_hbr = SelectObject (data->xgc, hbr);
      if (old_hbr == NULL)
	WIN32_GDI_FAILED ("SelectObject");
      else if (!DeleteObject (old_hbr))
	WIN32_GDI_FAILED ("DeleteObject");
    }
}  

static void
predraw_set_background (GdkGCWin32Data          *data,
			GdkColormapPrivateWin32 *colormap_private)
{
  COLORREF bg;
  
  bg = gdk_win32_colormap_color_pack (colormap_private, data->background);

  GDK_NOTE (GC, g_print ("predraw_set_background: bg=%06lx\n", bg));

  if (SetBkColor (data->xgc, bg) == CLR_INVALID)
    WIN32_GDI_FAILED ("SetBkColor");
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

  g_assert (data->xgc == NULL);

  data->hwnd = GDK_DRAWABLE_XID (drawable);
  data->holdpal = NULL;
  
  if (GDK_DRAWABLE_TYPE (drawable) == GDK_DRAWABLE_PIXMAP)
    data->xgc = gdk_win32_obtain_offscreen_hdc (data->hwnd);
  else
    data->xgc = gdk_win32_obtain_window_hdc (data->hwnd);

  if (usage & GDK_GC_FOREGROUND)
    predraw_set_foreground (data, colormap_private);

  if ((usage & GDK_GC_BACKGROUND) && (data->values_mask & GDK_GC_BACKGROUND))
    predraw_set_background (data, colormap_private);
  
  if (usage & GDK_GC_FONT)
    {
      if (SetBkMode (data->xgc, TRANSPARENT) == 0)
	WIN32_GDI_FAILED ("SetBkMode");
  
      if (SetTextAlign (data->xgc, TA_BASELINE) == GDI_ERROR)
	WIN32_GDI_FAILED ("SetTextAlign");
    }
  
  if (SetROP2 (data->xgc, data->rop2) == 0)
    WIN32_GDI_FAILED ("SetROP2");

  if ((data->values_mask & GDK_GC_CLIP_MASK)
      && data->clip_region != NULL)
    {
      RECT gc_rect,  hdc_rect;
      int gc_complex, hdc_complex;

      if (data->values_mask & (GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN))
	OffsetRgn (data->clip_region,
		   data->clip_x_origin, data->clip_y_origin);
      gc_complex = GetRgnBox (data->clip_region, &gc_rect);
      hdc_complex = GetClipBox (data->xgc,  &hdc_rect);
      if (gc_complex != SIMPLEREGION || hdc_complex != SIMPLEREGION ||
	  !EqualRect(&gc_rect, &hdc_rect))
        SelectClipRgn (data->xgc, data->clip_region);
    }
  else 
    {
      if (SelectClipRgn (data->xgc, NULL) == ERROR)
        WIN32_GDI_FAILED("SelectClipRegion");
    }

  return data->xgc;
}

void
gdk_gc_postdraw (GdkDrawable    *drawable,
		 GdkGCPrivate   *gc_private,
		 GdkGCValuesMask usage)
{
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc_private);
    
  if (data->holdpal != NULL)
    {
      gint k;
      
      if (!SelectPalette (data->xgc, data->holdpal, FALSE))
	WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (data->xgc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else if (k > 0)
	GDK_NOTE (COLORMAP, g_print ("gdk_gc_postdraw: realized %p: %d colors\n",
				     data->holdpal, k));
      data->holdpal = NULL;
    }

  if (GDK_DRAWABLE_TYPE (drawable) == GDK_DRAWABLE_PIXMAP)
    gdk_win32_release_hdc (NULL, data->xgc);
  else 
    gdk_win32_release_hdc (data->hwnd, data->xgc);
      
  if ((data->values_mask & GDK_GC_CLIP_MASK)
      && data->clip_region != NULL
      && (data->values_mask & (GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN)))
    OffsetRgn (data->clip_region,
	       -data->clip_x_origin, -data->clip_y_origin);
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
 * available at www.codeguru.com. Simplified for our needs, not sure
 * how much of the original code left any longer. Now handles just
 * one-bit deep bitmaps (in Window parlance, ie those that GDK calls
 * bitmaps (and not pixmaps), with zero pixels being transparent.
 */

/* gdk_win32_bitmap_to_region : Create a region from the
 * "non-transparent" pixels of a bitmap.
 */

HRGN
gdk_win32_bitmap_to_region (GdkPixmap *pixmap)
{
  HRGN hRgn = NULL;
  HRGN h;
  DWORD maxRects;
  RGNDATA *pData;
  GdkImage *image;
  guchar *p;
  gint x, y;

  g_assert (GDK_DRAWABLE_TYPE (pixmap) == GDK_DRAWABLE_PIXMAP);
  image = GDK_DRAWABLE_WIN32DATA (pixmap)->image;
  g_assert (image->depth == 1);

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

  for (y = 0; y < image->height; y++)
    {
      /* Scan each bitmap row from left to right*/
      p = (guchar *) image->mem + y * image->bpl;
      for (x = 0; x < image->width; x++)
	{
	  /* Search for a continuous range of "non transparent pixels"*/
	  gint x0 = x;
	  while (x < image->width)
	    {
	      if ((((p[x/8])>>(7-(x%8)))&1) == 0)
		/* This pixel is "transparent"*/
		break;
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
  g_free (pData);

  return hRgn;
}

#ifdef G_ENABLE_DEBUG

gchar *
gdk_win32_cap_style_to_string (GdkCapStyle cap_style)
{
  switch (cap_style)
    {
#define CASE(x) case x: return #x + strlen ("GDK_CAP_")
    CASE (GDK_CAP_NOT_LAST);
    CASE (GDK_CAP_BUTT);
    CASE (GDK_CAP_ROUND);
    CASE (GDK_CAP_PROJECTING);
#undef CASE
    default: return ("illegal GdkCapStyle value");
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
gdk_win32_fill_style_to_string (GdkFill fill)
{
  switch (fill)
    {
#define CASE(x) case x: return #x + strlen ("GDK_")
    CASE (GDK_SOLID);
    CASE (GDK_TILED);
    CASE (GDK_STIPPLED);
    CASE (GDK_OPAQUE_STIPPLED);
#undef CASE
    default: return ("illegal GdkFill value");
    }
  /* NOTREACHED */
  return NULL;
}

gchar *
gdk_win32_function_to_string (GdkFunction function)
{
  switch (function)
    {
#define CASE(x) case x: return #x + strlen ("GDK_")
    CASE (GDK_COPY);
    CASE (GDK_INVERT);
    CASE (GDK_XOR);
    CASE (GDK_CLEAR);
    CASE (GDK_AND);
    CASE (GDK_AND_REVERSE);
    CASE (GDK_AND_INVERT);
    CASE (GDK_NOOP);
    CASE (GDK_OR);
    CASE (GDK_EQUIV);
    CASE (GDK_OR_REVERSE);
    CASE (GDK_COPY_INVERT);
    CASE (GDK_OR_INVERT);
    CASE (GDK_NAND);
    CASE (GDK_SET);
#undef CASE
    default: return ("illegal GdkFunction value");
    }
  /* NOTREACHED */
  return NULL; 
}

gchar *
gdk_win32_join_style_to_string (GdkJoinStyle join_style)
{
  switch (join_style)
    {
#define CASE(x) case x: return #x + strlen ("GDK_JOIN_")
    CASE (GDK_JOIN_MITER);
    CASE (GDK_JOIN_ROUND);
    CASE (GDK_JOIN_BEVEL);
#undef CASE
    default: return ("illegal GdkJoinStyle value");
    }
  /* NOTREACHED */
  return NULL; 
}

gchar *
gdk_win32_line_style_to_string (GdkLineStyle line_style)
{
  switch (line_style)
    {
#define CASE(x) case x: return #x + strlen ("GDK_LINE_")
    CASE(GDK_LINE_SOLID);
    CASE(GDK_LINE_ON_OFF_DASH);  
    CASE(GDK_LINE_DOUBLE_DASH);  
#undef CASE
    default: return ("illegal GdkLineStyle value");
    }
  /* NOTREACHED */
  return NULL; 
}
#endif

