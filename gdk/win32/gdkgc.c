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
#include "gdkpixmap.h"
#include "gdkprivate.h"
#include "gdkx.h"

GdkGC*
gdk_gc_new (GdkWindow *window)
{
  return gdk_gc_new_with_values (window, NULL, 0);
}

GdkGC*
gdk_gc_new_with_values (GdkWindow	*window,
			GdkGCValues	*values,
			GdkGCValuesMask	 values_mask)
{
  GdkGC *gc;
  GdkGCPrivate *private;
  static GdkColor black;
  static GdkColor white;
  static gboolean beenhere = FALSE;

  if (!beenhere)
    {
      gdk_color_black (gdk_colormap_get_system (), &black);
      gdk_color_white (gdk_colormap_get_system (), &white);
      beenhere = TRUE;
    }

  g_return_val_if_fail (window != NULL, NULL);

  if (GDK_DRAWABLE_DESTROYED (window))
    return NULL;

  private = g_new (GdkGCPrivate, 1);
  gc = (GdkGC*) private;

  private->ref_count = 1;
  private->rop2 = R2_COPYPEN;
  private->fill_style = GDK_SOLID;
  private->values_mask = values_mask;
  private->values_mask |= GDK_GC_FUNCTION | GDK_GC_FILL;

  GDK_NOTE (MISC, g_print ("gdk_gc_new: {"));

  if (values_mask & GDK_GC_FOREGROUND)
    {
      private->foreground = values->foreground;
    }
  else
    private->foreground = black;
  
  if (values_mask & GDK_GC_BACKGROUND)
    {
      private->background = values->background;
    }
  else
    private->background = white;

  if ((values_mask & GDK_GC_FONT) && (values->font->type == GDK_FONT_FONT
				      || values->font->type == GDK_FONT_FONTSET))
    {
      private->font = values->font;
      gdk_font_ref (private->font);
      GDK_NOTE (MISC, g_print (" font=%#x", private->font));
    }
  else
    private->font = NULL;

  if (values_mask & GDK_GC_FUNCTION)
    {
      switch (values->function)
	{
	case GDK_COPY:
	  private->rop2 = R2_COPYPEN; break;
	case GDK_INVERT:
	  private->rop2 = R2_NOT; break;
	case GDK_XOR:
	  private->rop2 = R2_XORPEN; break;
	case GDK_CLEAR:
	  private->rop2 = R2_BLACK; break;
	case GDK_AND:
	  private->rop2 = R2_MASKPEN; break;
	case GDK_AND_REVERSE:
	  private->rop2 = R2_MASKPENNOT; break;
	case GDK_AND_INVERT:
	  private->rop2 = R2_MASKNOTPEN; break;
	case GDK_NOOP:
	  private->rop2 = R2_NOP; break;
	case GDK_OR:
	  private->rop2 = R2_MERGEPEN; break;
	case GDK_EQUIV:
	  private->rop2 = R2_NOTXORPEN; break;
	case GDK_OR_REVERSE:
	  private->rop2 = R2_MERGEPENNOT; break;
	case GDK_COPY_INVERT:
	  private->rop2 = R2_NOTCOPYPEN; break;
	case GDK_OR_INVERT:
	  private->rop2 = R2_MERGENOTPEN; break;
	case GDK_NAND:
	  private->rop2 = R2_NOTMASKPEN; break;
	case GDK_SET:
	  private->rop2 = R2_WHITE; break;
	}
      GDK_NOTE (MISC, g_print (" function=%d", private->rop2));
    }

  if (values_mask & GDK_GC_FILL)
    {
      private->fill_style = values->fill;
      GDK_NOTE (MISC, g_print (" fill=%d", private->fill_style));
    }

  if (values_mask & GDK_GC_TILE)
    {
      private->tile = values->tile;
      gdk_pixmap_ref (private->tile);
      GDK_NOTE (MISC, g_print (" tile=%#x", GDK_DRAWABLE_XID (private->tile)));
    }
  else
    private->tile = NULL;

  if (values_mask & GDK_GC_STIPPLE)
    {
      private->stipple = values->stipple;
      gdk_pixmap_ref (private->stipple);
      GDK_NOTE (MISC, g_print (" stipple=%#x", GDK_DRAWABLE_XID (private->stipple)));
    }
  else
    private->stipple = NULL;

  if (values_mask & GDK_GC_CLIP_MASK)
    {
      private->clip_region =
	BitmapToRegion ((HBITMAP) GDK_DRAWABLE_XID (values->clip_mask));
      GDK_NOTE (MISC, g_print (" clip=%#x", private->clip_region));
    }
  else
    private->clip_region = NULL;

  if (values_mask & GDK_GC_SUBWINDOW)
    {
      private->subwindow_mode = values->subwindow_mode;
      GDK_NOTE (MISC, g_print (" subw=%d", private->subwindow_mode));
    }

  if (values_mask & GDK_GC_TS_X_ORIGIN)
    {
      private->ts_x_origin = values->ts_x_origin;
      GDK_NOTE (MISC, g_print (" ts_x=%d", private->ts_x_origin));
    }

  if (values_mask & GDK_GC_TS_Y_ORIGIN)
    {
      private->ts_y_origin = values->ts_y_origin;
      GDK_NOTE (MISC, g_print (" ts_y=%d", private->ts_y_origin));
    }

  if (values_mask & GDK_GC_CLIP_X_ORIGIN)
    {
      private->clip_x_origin = values->clip_x_origin;
      GDK_NOTE (MISC, g_print (" clip_x=%d", private->clip_x_origin));
    }

  if (values_mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      private->clip_y_origin = values->clip_y_origin; 
      GDK_NOTE (MISC, g_print (" clip_y=%d", private->clip_y_origin));
   }
 
  if (values_mask & GDK_GC_EXPOSURES)
    {
      private->graphics_exposures = values->graphics_exposures;
      GDK_NOTE (MISC, g_print (" exp=%d", private->graphics_exposures));
    }

  private->pen_style = PS_GEOMETRIC;
  private->pen_width = 1;

  if (values_mask & (GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE))
    {
      if (values_mask & GDK_GC_LINE_WIDTH)
	{
	  private->pen_width = values->line_width;
	  GDK_NOTE (MISC, g_print (" pw=%d", private->pen_width));
	}
      if (values_mask & GDK_GC_LINE_STYLE)
	{
	  switch (values->line_style)
	    {
	    case GDK_LINE_SOLID:
	      private->pen_style |= PS_SOLID; break;
	    case GDK_LINE_ON_OFF_DASH:
	    case GDK_LINE_DOUBLE_DASH: /* ??? */
	      private->pen_style |= PS_DASH; break;
	    }
	  GDK_NOTE (MISC, g_print (" ps=%#x", private->pen_style));
	}
    }

  if (values_mask & GDK_GC_CAP_STYLE)
    {
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:	/* ??? */
	case GDK_CAP_BUTT:
	  private->pen_style |= PS_ENDCAP_FLAT;	break;
	case GDK_CAP_ROUND:
	  private->pen_style |= PS_ENDCAP_ROUND; break;
	case GDK_CAP_PROJECTING:
	  private->pen_style |= PS_ENDCAP_SQUARE; break;
	}
      GDK_NOTE (MISC, g_print (" ps=%#x", private->pen_style));
    }

  if (values_mask & GDK_GC_JOIN_STYLE)
    {
      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  private->pen_style |= PS_JOIN_MITER;
	  break;
	case GDK_JOIN_ROUND:
	  private->pen_style |= PS_JOIN_ROUND;
	  break;
	case GDK_JOIN_BEVEL:
	  private->pen_style |= PS_JOIN_BEVEL;
	  break;
	}
      GDK_NOTE (MISC, g_print (" ps=%#x", private->pen_style));
    }

  private->hwnd = NULL;
  private->xgc = NULL;

  GDK_NOTE (MISC, g_print ("} = %p\n", private));

  return gc;
}

void
gdk_gc_destroy (GdkGC *gc)
{
  gdk_gc_unref (gc);
}

GdkGC *
gdk_gc_ref (GdkGC *gc)
{
  GdkGCPrivate *private = (GdkGCPrivate*) gc;

  g_return_val_if_fail (gc != NULL, NULL);
  private->ref_count += 1;

  return gc;
}

void
gdk_gc_unref (GdkGC *gc)
{
  GdkGCPrivate *private = (GdkGCPrivate*) gc;
  
  g_return_if_fail (gc != NULL);
  
  if (private->ref_count > 1)
    private->ref_count -= 1;
  else
    {
      if (private->values_mask & GDK_GC_FONT)
	gdk_font_unref (private->font);

      if (private->values_mask & GDK_GC_TILE)
	gdk_pixmap_unref (private->tile);
      
      if (private->values_mask & GDK_GC_STIPPLE)
	gdk_pixmap_unref (private->stipple);
      
      if (private->values_mask & GDK_GC_CLIP_MASK)
	DeleteObject (private->clip_region);

      g_free (gc);
    }
}

void
gdk_gc_get_values (GdkGC       *gc,
		   GdkGCValues *values)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);
  g_return_if_fail (values != NULL);

  private = (GdkGCPrivate*) gc;

  values->foreground = private->foreground;
  values->background = private->background;
  values->font = private->font;

  switch (private->rop2)
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

  values->fill = private->fill_style;

  values->tile = private->tile;
  values->stipple = private->stipple;
  if (private->clip_region != NULL)
    {
      RECT rect;
      HBRUSH hbr;
      HDC hdc;
      HGDIOBJ oldbitmap;
      GdkPixmap *pixmap;

      GetRgnBox (private->clip_region, &rect);
      pixmap =
	gdk_pixmap_new (NULL, rect.right - rect.left, rect.bottom - rect.top,
			1);
      hbr = GetStockObject (WHITE_BRUSH);
      if ((hdc = CreateCompatibleDC (NULL)) == NULL)
	g_warning ("gdk_gc_get_values: CreateCompatibleDC failed");
      if ((oldbitmap =
	   SelectObject (hdc, GDK_DRAWABLE_XID (pixmap))) == NULL)
	g_warning ("gdk_gc_get_values: SelectObject #1 failed");
      hbr = GetStockObject (BLACK_BRUSH);
      if (!FillRect (hdc, &rect, hbr))
	g_warning ("gdk_gc_get_values: FillRect failed");
      hbr = GetStockObject (WHITE_BRUSH);
      if (!FillRgn (hdc, private->clip_region, hbr))
	g_warning ("gdk_gc_get_values: FillRgn failed");
      if (SelectObject (hdc, oldbitmap) == NULL)
	g_warning ("gdk_gc_get_values: SelectObject #2 failed");
      DeleteDC (hdc);
      values->clip_mask = pixmap;
    }
  else
    values->clip_mask = NULL;
  values->subwindow_mode = private->subwindow_mode;
  values->ts_x_origin = private->ts_x_origin;
  values->ts_y_origin = private->ts_y_origin;
  values->clip_x_origin = private->clip_x_origin;
  values->clip_y_origin = private->clip_y_origin;
  values->graphics_exposures = private->graphics_exposures;
  values->line_width = private->pen_width;
  
  if (private->pen_style & PS_SOLID)
    values->line_style = GDK_LINE_SOLID;
  else if (private->pen_style & PS_DASH)
    values->line_style = GDK_LINE_ON_OFF_DASH;
  else
    values->line_style = GDK_LINE_SOLID;

  /* PS_ENDCAP_ROUND is zero */
  if (private->pen_style & PS_ENDCAP_FLAT)
    values->cap_style = GDK_CAP_BUTT;
  else if (private->pen_style & PS_ENDCAP_SQUARE)
    values->cap_style = GDK_CAP_PROJECTING;
  else
    values->cap_style = GDK_CAP_ROUND;
    
  /* PS_JOIN_ROUND is zero */
  if (private->pen_style & PS_JOIN_MITER)
    values->join_style = GDK_JOIN_MITER;
  else if (private->pen_style & PS_JOIN_BEVEL)
    values->join_style = GDK_JOIN_BEVEL;
  else
    values->join_style = GDK_JOIN_ROUND;
}

void
gdk_gc_set_foreground (GdkGC	*gc,
		       GdkColor *color)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);
  g_return_if_fail (color != NULL);

  private = (GdkGCPrivate*) gc;
  {
    GDK_NOTE (MISC, g_print ("gdk_gc_set_foreground: (%d) %s\n",
			     private, gdk_color_to_string (color)));
    private->foreground = *color;
    private->values_mask |= GDK_GC_FOREGROUND;
  }
}

void
gdk_gc_set_background (GdkGC	*gc,
		       GdkColor *color)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);
  g_return_if_fail (color != NULL);

  private = (GdkGCPrivate*) gc;

  GDK_NOTE (MISC, g_print ("gdk_gc_set_backround: (%d) %s\n",
			   private, gdk_color_to_string(color)));
  private->background = *color;
  private->values_mask |= GDK_GC_BACKGROUND;
}

void
gdk_gc_set_font (GdkGC	 *gc,
		 GdkFont *font)
{
  GdkGCPrivate *gc_private;
  GdkFontPrivate *font_private;
  gchar *xlfd;

  g_return_if_fail (gc != NULL);
  g_return_if_fail (font != NULL);

  if (font->type == GDK_FONT_FONT
      || font->type == GDK_FONT_FONTSET)
    {
      gc_private = (GdkGCPrivate*) gc;
      
      GDK_NOTE (MISC, (xlfd = gdk_font_xlfd_create (font),
		       g_print ("gdk_gc_set_font: (%d) %s\n",
				gc_private, xlfd),
		       gdk_font_xlfd_free (xlfd)));

      if (gc_private->font != NULL)
	gdk_font_unref (gc_private->font);
      gc_private->font = font;
      gdk_font_ref (gc_private->font);
      gc_private->values_mask |= GDK_GC_FONT;
    }
}

void
gdk_gc_set_function (GdkGC	 *gc,
		     GdkFunction  function)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  GDK_NOTE (MISC, g_print ("gdk_gc_set_function: (%d) %d\n", private, function));

  switch (function)
    {
    case GDK_COPY:
      private->rop2 = R2_COPYPEN; break;
    case GDK_INVERT:
      private->rop2 = R2_NOT; break;
    case GDK_XOR:
      private->rop2 = R2_XORPEN; break;
    case GDK_CLEAR:
      private->rop2 = R2_BLACK; break;
    case GDK_AND:
      private->rop2 = R2_MASKPEN; break;
    case GDK_AND_REVERSE:
      private->rop2 = R2_MASKPENNOT; break;
    case GDK_AND_INVERT:
      private->rop2 = R2_MASKNOTPEN; break;
    case GDK_NOOP:
      private->rop2 = R2_NOP; break;
    case GDK_OR:
      private->rop2 = R2_MERGEPEN; break;
    case GDK_EQUIV:
      private->rop2 = R2_NOTXORPEN; break;
    case GDK_OR_REVERSE:
      private->rop2 = R2_MERGEPENNOT; break;
    case GDK_COPY_INVERT:
      private->rop2 = R2_NOTCOPYPEN; break;
    case GDK_OR_INVERT:
      private->rop2 = R2_MERGENOTPEN; break;
    case GDK_NAND:
      private->rop2 = R2_NOTMASKPEN; break;
    case GDK_SET:
      private->rop2 = R2_WHITE; break;
    }
  private->values_mask |= GDK_GC_FUNCTION;
}

void
gdk_gc_set_fill (GdkGC	 *gc,
		 GdkFill  fill)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  private->fill_style = fill;
  private->values_mask |= GDK_GC_FILL;
}

void
gdk_gc_set_tile (GdkGC	   *gc,
		 GdkPixmap *tile)
{
  GdkGCPrivate *private;
  HBITMAP pixmap;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  pixmap = NULL;

  if (tile)
    pixmap = GDK_DRAWABLE_XID (tile);

  if (private->tile != NULL)
    gdk_pixmap_unref (private->tile);
  private->tile = tile;
  if (tile)
    gdk_pixmap_ref (tile);
  if (pixmap != NULL)
    private->values_mask |= GDK_GC_TILE;
  else
    private->values_mask &= ~GDK_GC_TILE;
}

void
gdk_gc_set_stipple (GdkGC     *gc,
		    GdkPixmap *stipple)
{
  GdkGCPrivate *private;
  HBITMAP pixmap;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  pixmap = NULL;

  if (stipple)
    pixmap = GDK_DRAWABLE_XID (stipple);

  if (private->stipple != NULL)
    gdk_pixmap_unref (private->stipple);
  private->stipple = stipple;
  if (stipple)
    gdk_pixmap_ref (stipple);
  if (pixmap != NULL)
    private->values_mask |= GDK_GC_STIPPLE;
  else
    private->values_mask &= ~GDK_GC_STIPPLE;
}

void
gdk_gc_set_ts_origin (GdkGC *gc,
		      gint   x,
		      gint   y)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  private->ts_x_origin = x;
  private->ts_y_origin = y;
  private->values_mask |= GDK_GC_TS_X_ORIGIN |GDK_GC_TS_Y_ORIGIN;
}

void
gdk_gc_set_clip_origin (GdkGC *gc,
			gint   x,
			gint   y)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_origin: (%d) +%d+%d\n",
			   private, x, y));

  private->clip_x_origin = x;
  private->clip_y_origin = y;
  private->values_mask |= GDK_GC_CLIP_X_ORIGIN |GDK_GC_CLIP_Y_ORIGIN;
}

void
gdk_gc_set_clip_mask (GdkGC	*gc,
		      GdkBitmap *mask)
{
  GdkGCPrivate *private;
  HBITMAP xmask;
  
  g_return_if_fail (gc != NULL);
  
  if (mask)
    {
      if (GDK_DRAWABLE_DESTROYED (mask))
	return;
      xmask = GDK_DRAWABLE_XID (mask);
    }
  else
    xmask = NULL;

  private = (GdkGCPrivate*) gc;

  GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_mask: (%d) %#x\n", private, xmask));

  if (private->clip_region != NULL)
    if (!DeleteObject (private->clip_region))
      g_warning ("gdk_gc_set_clip_mask: DeleteObject failed");
  if (xmask != NULL)
    {
      private->clip_region = BitmapToRegion (xmask);
      {
	RECT rect;
	GetRgnBox (private->clip_region, &rect);
	GDK_NOTE (MISC, g_print ("...box = %dx%d@+%d+%d\n",
				 rect.right - rect.left, rect.bottom - rect.top,
				 rect.left, rect.top));
      }
#if 0
      /* Test code that sets clip region to whole of mask */
      {
	BITMAP bm;
	GetObject (xmask, sizeof (bm), &bm);
	private->clip_region = CreateRectRgn (0, 0, bm.bmWidth, bm.bmHeight);
      }
#endif
      private->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      private->values_mask &= ~GDK_GC_CLIP_MASK;
      private->clip_region = NULL;
    }
}

void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkGCPrivate *private;
   
  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  if (private->clip_region != NULL)
    if (!DeleteObject (private->clip_region))
      g_warning ("gdk_gc_set_clip_rectangle: DeleteObject failed");
  if (rectangle)
    {
      GDK_NOTE (MISC,
		g_print ("gdk_gc_set_clip_rectangle: (%d) %dx%d@+%d+%d\n",
			 private,
			 rectangle->width, rectangle->height,
			 rectangle->x, rectangle->y));
      if ((private->clip_region =
	   CreateRectRgn (rectangle->x, rectangle->y,
			  rectangle->x + rectangle->width,
			  rectangle->y + rectangle->height)) == NULL)
	g_warning ("gdk_gc_set_clip_rectangle: CreateRectRgn failed");

      private->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_rectangle: (%d) None\n",
			       private));
      private->clip_region = NULL;
      private->values_mask &= ~GDK_GC_CLIP_MASK;
    }
    private->values_mask &= ~(GDK_GC_CLIP_X_ORIGIN |GDK_GC_CLIP_Y_ORIGIN);
} 

void
gdk_gc_set_clip_region (GdkGC		 *gc,
			GdkRegion	 *region)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_region: (%d) %s\n",
			   private, (region != NULL ? "xxx" : "None")));

  if (private->clip_region != NULL)
    if (!DeleteObject (private->clip_region))
      g_warning ("gdk_gc_set_clip_region: DeleteObject failed");
  if (region)
    {
      GdkRegionPrivate *region_private;

      region_private = (GdkRegionPrivate*) region;
      private->clip_region = CreateRectRgn (1, 1, 0, 0);
      CombineRgn (private->clip_region, region_private->xregion, NULL, RGN_COPY);
      private->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      private->clip_region = NULL;
      private->values_mask &= ~GDK_GC_CLIP_MASK;
    }
}

void
gdk_gc_set_subwindow (GdkGC	       *gc,
		      GdkSubwindowMode	mode)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  private->subwindow_mode = mode;
  private->values_mask |= GDK_GC_SUBWINDOW;
}

void
gdk_gc_set_exposures (GdkGC *gc,
		      gint   exposures)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  private->graphics_exposures = exposures;
  private->values_mask |= GDK_GC_EXPOSURES;;
}

void
gdk_gc_set_line_attributes (GdkGC	*gc,
			    gint	 line_width,
			    GdkLineStyle line_style,
			    GdkCapStyle	 cap_style,
			    GdkJoinStyle join_style)
{
  GdkGCPrivate *private;
  int xline_style;
  int xcap_style;
  int xjoin_style;

  g_return_if_fail (gc != NULL);

  private = (GdkGCPrivate*) gc;

  GDK_NOTE (MISC,
	    g_print ("gdk_gc_set_line_attributes: (%d) %d %s %s %s\n",
		     private, line_width,
		     (line_style == GDK_LINE_SOLID ? "SOLID" :
		      (line_style == GDK_LINE_ON_OFF_DASH ? "ON_OFF_DASH" :
		       (line_style == GDK_LINE_DOUBLE_DASH ? "DOUBLE_DASH" :
			"???"))),
		     (cap_style == GDK_CAP_BUTT ? "BUTT" :
		      (cap_style == GDK_CAP_ROUND ? "ROUND" :
		       (cap_style == GDK_CAP_PROJECTING ? "PROJECTING" :
			"???"))),
		     (join_style == GDK_JOIN_MITER ? "MITER" :
		      (join_style == GDK_JOIN_ROUND ? "ROUND" :
		       (join_style == GDK_JOIN_BEVEL ? "BEVEL" :
			"???")))));

  private->pen_width = line_width;

  /* Mask old style bits away */
  private->pen_style &= ~(PS_STYLE_MASK|PS_ENDCAP_MASK|PS_JOIN_MASK);

  /* Add new bits */
  switch (line_style)
    {
    case GDK_LINE_SOLID:
      private->pen_style |= PS_SOLID; break;
    case GDK_LINE_ON_OFF_DASH:
    case GDK_LINE_DOUBLE_DASH: /* ??? */
      private->pen_style |= PS_DASH; break;
    }

  switch (cap_style)
    {
    case GDK_CAP_NOT_LAST:
      /* ??? */
      break;
    case GDK_CAP_BUTT:
      private->pen_style |= PS_ENDCAP_FLAT; break;
    case GDK_CAP_ROUND:
      private->pen_style |= PS_ENDCAP_ROUND; break;
    case GDK_CAP_PROJECTING:
      private->pen_style |= PS_ENDCAP_SQUARE; break;
    }

  switch (join_style)
    {
    case GDK_JOIN_MITER:
      private->pen_style |= PS_JOIN_MITER;
      break;
    case GDK_JOIN_ROUND:
      private->pen_style |= PS_JOIN_ROUND;
      break;
    case GDK_JOIN_BEVEL:
      private->pen_style |= PS_JOIN_BEVEL;
      break;
    }
}

void
gdk_gc_set_dashes (GdkGC *gc,
		   gint	  dash_offset,
		   gchar  dash_list[],
		   gint   n)
{
  GdkGCPrivate *private;

  g_return_if_fail (gc != NULL);
  g_return_if_fail (dash_list != NULL);

  /* XXX ??? */

  private = (GdkGCPrivate *) gc;

  private->pen_style &= ~(PS_STYLE_MASK);
  private->pen_style |= PS_DASH;
}

void
gdk_gc_copy (GdkGC *dst_gc, GdkGC *src_gc)
{
  GdkGCPrivate *dst_private, *src_private;

  src_private = (GdkGCPrivate *) src_gc;
  dst_private = (GdkGCPrivate *) dst_gc;

  *dst_private = *src_private;
}

HDC
gdk_gc_predraw (GdkDrawablePrivate *drawable_private,
		GdkGCPrivate       *gc_private)
{
  GdkColormapPrivate *colormap_private =
    (GdkColormapPrivate *) drawable_private->colormap;
  COLORREF bg;
  COLORREF fg;
  LOGBRUSH logbrush;
  HPEN hpen;
  HBRUSH hbr;

  g_assert (gc_private->xgc == NULL);

  if (drawable_private->window_type == GDK_DRAWABLE_PIXMAP)
    {
      if ((gc_private->xgc = CreateCompatibleDC (NULL)) == NULL)
	g_warning ("gdk_gc_predraw: CreateCompatibleDC failed");

      if ((gc_private->saved_dc = SaveDC (gc_private->xgc)) == 0)
	g_warning ("gdk_gc_predraw: SaveDC #1 failed");
      
      if (SelectObject (gc_private->xgc, drawable_private->xwindow) == NULL)
	g_warning ("gdk_gc_predraw: SelectObject #1 failed");
    }
  else
    {
      if ((gc_private->xgc = GetDC (drawable_private->xwindow)) == NULL)
	g_warning ("gdk_gc_predraw: GetDC failed");
      
      if ((gc_private->saved_dc = SaveDC (gc_private->xgc)) == 0)
	g_warning ("gdk_gc_predraw: SaveDC #2 failed");
    }
  
  gc_private->hwnd = drawable_private->xwindow;
  
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
	    g_warning ("gdk_gc_predraw: CreatePalette failed");
	}
      SelectPalette (gc_private->xgc, hpal, FALSE);
      RealizePalette (gc_private->xgc);
      fg = PALETTEINDEX (gc_private->foreground.pixel);
    }
  else if (colormap_private != NULL
	   && colormap_private->xcolormap->rc_palette)
    {
      int k;
      if (SelectPalette (gc_private->xgc,
			 colormap_private->xcolormap->palette, FALSE) == NULL)
	g_warning ("gdk_gc_predraw: SelectPalette failed");
      if (TRUE || colormap_private->xcolormap->stale)
	{
	  if ((k = RealizePalette (gc_private->xgc)) == GDI_ERROR)
	    g_warning ("gdk_gc_predraw: RealizePalette failed");
	  colormap_private->xcolormap->stale = FALSE;
	}
#if 0
      g_print ("Selected palette %#x for gc %#x, realized %d colors\n",
	       colormap_private->xcolormap->palette, gc_private->xgc, k);
#endif
      fg = PALETTEINDEX (gc_private->foreground.pixel);
    }
  else
    {
      COLORREF foreground = RGB (gc_private->foreground.red >> 8,
				 gc_private->foreground.green >> 8,
				 gc_private->foreground.blue >> 8);
      fg = GetNearestColor (gc_private->xgc, foreground);
    }
  logbrush.lbStyle = BS_SOLID;
  logbrush.lbColor = fg;
  if ((hpen = ExtCreatePen (gc_private->pen_style, gc_private->pen_width,
			    &logbrush, 0, NULL)) == NULL)
    g_warning ("gdk_gc_predraw: CreatePen failed");
  
  if (SelectObject (gc_private->xgc, hpen) == NULL)
    g_warning ("gdk_gc_predraw: SelectObject #2 failed");

  if (SetTextColor (gc_private->xgc, fg) == CLR_INVALID)
    g_warning ("gdk_gc_predraw: SetTextColor failed");

#if 0
  switch (gc_private->fill_style)
    {
    case GDK_STIPPLED:
      {
	GdkPixmap *stipple = gc_private->stipple;
	GdkPixmapPrivate *stipple_private = (GdkPixmapPrivate *) stipple;
	HBITMAP hbm = stipple_private->xwindow;
	if (NULL == (hbr = CreatePatternBrush (hbm)))
	  g_warning ("gdk_gc_predraw: CreatePatternBrush failed");
	
#ifdef NATIVE_WIN16
	SetBrushOrg  (gc_private->xgc, gc_private->ts_x_origin,
		      gc_private->ts_y_origin);
#else
	SetBrushOrgEx(gc_private->xgc, gc_private->ts_x_origin,
		      gc_private->ts_y_origin, NULL);
#endif
      }
      break;
    case GDK_SOLID:
    default:
      if ((hbr = CreateSolidBrush (fg)) == NULL)
	g_warning ("gdk_gc_predraw: CreateSolidBrush failed");
      break;
  }
#else
  if ((hbr = CreateSolidBrush (fg)) == NULL)
    g_warning ("gdk_gc_predraw: CreateSolidBrush failed");
#endif
  if (SelectObject (gc_private->xgc, hbr) == NULL)
    g_warning ("gdk_gc_predraw: SelectObject #3 failed");

  if (gc_private->values_mask & GDK_GC_BACKGROUND)
    {
      if (colormap_private == NULL)
	{
	  /* a bitmap */
	  bg = PALETTEINDEX (gc_private->background.pixel);
	}
      else if (colormap_private != NULL
	  && colormap_private->xcolormap->rc_palette)
	{
	  bg = PALETTEINDEX (gc_private->background.pixel);
	}
      else
	{
	  COLORREF background = RGB (gc_private->background.red >> 8,
				     gc_private->background.green >> 8,
				     gc_private->background.blue >> 8);
	  bg = GetNearestColor (gc_private->xgc, background);
	}
      if (SetBkColor (gc_private->xgc, bg) == CLR_INVALID)
	g_warning ("gdk_gc_predraw: SetBkColor failed");
    }
  
  if (SetBkMode (gc_private->xgc, TRANSPARENT) == 0)
    g_warning ("gdk_gc_predraw: SetBkMode failed");
  
  if (SetTextAlign (gc_private->xgc, TA_BASELINE) == GDI_ERROR)
    g_warning ("gdk_gc_predraw: SetTextAlign failed");
  
  if (gc_private->values_mask & GDK_GC_FUNCTION)
    if (SetROP2 (gc_private->xgc, gc_private->rop2) == 0)
      g_warning ("gdk_gc_predraw: SetROP2 failed");

  if (gc_private->values_mask & GDK_GC_CLIP_MASK
      && gc_private->clip_region != NULL)
    {
      if (gc_private->values_mask & (GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN))
	OffsetRgn (gc_private->clip_region,
		   gc_private->clip_x_origin, gc_private->clip_y_origin);
      SelectClipRgn (gc_private->xgc, gc_private->clip_region);
    }

  return gc_private->xgc;
}

void
gdk_gc_postdraw (GdkDrawablePrivate *drawable_private,
		 GdkGCPrivate       *gc_private)
{
  HGDIOBJ hpen;
  HGDIOBJ hbr;
  GdkColormapPrivate *colormap_private =
    (GdkColormapPrivate *) drawable_private->colormap;

  if ((hpen = GetCurrentObject (gc_private->xgc, OBJ_PEN)) == NULL)
    g_warning ("gdk_gc_postdraw: GetCurrentObject #1 failed");

  if ((hbr = GetCurrentObject (gc_private->xgc, OBJ_BRUSH)) == NULL)
    g_warning ("gdk_gc_postdraw: GetCurrentObject #2 failed");

  if (!RestoreDC (gc_private->xgc, gc_private->saved_dc))
    g_warning ("gdk_gc_postdraw: RestoreDC failed");
#if 0
  if (colormap_private != NULL
      && colormap_private->xcolormap->rc_palette
      && colormap_private->xcolormap->stale)
    {
      SelectPalette (gc_private->xgc, GetStockObject (DEFAULT_PALETTE), FALSE);
      if (!UnrealizeObject (colormap_private->xcolormap->palette))
	g_warning ("gdk_gc_postraw: UnrealizeObject failed");
    }
#endif
  if (drawable_private->window_type == GDK_DRAWABLE_PIXMAP)
    {
      if (!DeleteDC (gc_private->xgc))
	g_warning ("gdk_gc_postdraw: DeleteDC failed");
    }
  else
    {
      ReleaseDC (gc_private->hwnd, gc_private->xgc);
    }

  if (hpen != NULL)
    if (!DeleteObject (hpen))
      g_warning ("gdk_gc_postdraw: DeleteObject #1 failed");
  
  if (hbr != NULL)
    if (!DeleteObject (hbr))
      g_warning ("gdk_gc_postdraw: DeleteObject #2 failed");

  gc_private->xgc = NULL;
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
	g_warning ("BitmapToRegion: CreatePalette failed");
    }

  /* Create a memory DC inside which we will scan the bitmap content */
  hMemDC = CreateCompatibleDC (NULL);
  if (!hMemDC)
    {
      g_warning ("BitmapToRegion: CreateCompatibleDC #1 failed");
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
      g_warning ("BitmapToRegion: CreateDIBSection failed");
      DeleteDC (hMemDC);
      return NULL;
    }

  holdBmp = (HBITMAP) SelectObject (hMemDC, hbm8);

  /* Create a DC just to copy the bitmap into the memory DC*/
  hDC = CreateCompatibleDC (hMemDC);
  if (!hDC)
    {
      g_warning ("BitmapToRegion: CreateCompatibleDC #2 failed");
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
      g_warning ("BitmapToRegion: BitBlt failed");
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
