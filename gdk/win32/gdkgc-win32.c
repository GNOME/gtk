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

GdkGC*
_gdk_win32_gc_new (GdkDrawable	  *drawable,
		   GdkGCValues	  *values,
		   GdkGCValuesMask values_mask)
{
  GdkGC *gc;
  GdkGCPrivate *private;
  GdkGCWin32Data *data;
  static GdkColor black;
  static GdkColor white;
  static gboolean beenhere = FALSE;

  if (!beenhere)
    {
      gdk_color_black (gdk_colormap_get_system (), &black);
      gdk_color_white (gdk_colormap_get_system (), &white);
      beenhere = TRUE;
    }

  gc = gdk_gc_alloc ();
  private = (GdkGCPrivate *)gc;

  private->klass = &gdk_win32_gc_class;
  private->klass_data = data = g_new (GdkGCWin32Data, 1);
    
  data->rop2 = R2_COPYPEN;
  data->fill_style = GDK_SOLID;
  data->values_mask = values_mask;
  data->values_mask |= GDK_GC_FUNCTION | GDK_GC_FILL;

  GDK_NOTE (MISC, g_print ("_gdk_win32_gc_new: {"));

  if (values_mask & GDK_GC_FOREGROUND)
    {
      data->foreground = values->foreground;
    }
  else
    data->foreground = black;
  
  if (values_mask & GDK_GC_BACKGROUND)
    {
      data->background = values->background;
    }
  else
    data->background = white;

  if ((values_mask & GDK_GC_FONT) && (values->font->type == GDK_FONT_FONT
				      || values->font->type == GDK_FONT_FONTSET))
    {
      data->font = values->font;
      gdk_font_ref (data->font);
      GDK_NOTE (MISC, g_print (" font"));
    }
  else
    data->font = NULL;

  if (values_mask & GDK_GC_FUNCTION)
    {
      switch (values->function)
	{
	case GDK_COPY:
	  data->rop2 = R2_COPYPEN; break;
	case GDK_INVERT:
	  data->rop2 = R2_NOT; break;
	case GDK_XOR:
	  data->rop2 = R2_XORPEN; break;
	case GDK_CLEAR:
	  data->rop2 = R2_BLACK; break;
	case GDK_AND:
	  data->rop2 = R2_MASKPEN; break;
	case GDK_AND_REVERSE:
	  data->rop2 = R2_MASKPENNOT; break;
	case GDK_AND_INVERT:
	  data->rop2 = R2_MASKNOTPEN; break;
	case GDK_NOOP:
	  data->rop2 = R2_NOP; break;
	case GDK_OR:
	  data->rop2 = R2_MERGEPEN; break;
	case GDK_EQUIV:
	  data->rop2 = R2_NOTXORPEN; break;
	case GDK_OR_REVERSE:
	  data->rop2 = R2_MERGEPENNOT; break;
	case GDK_COPY_INVERT:
	  data->rop2 = R2_NOTCOPYPEN; break;
	case GDK_OR_INVERT:
	  data->rop2 = R2_MERGENOTPEN; break;
	case GDK_NAND:
	  data->rop2 = R2_NOTMASKPEN; break;
	case GDK_SET:
	  data->rop2 = R2_WHITE; break;
	}
      GDK_NOTE (MISC, g_print (" function=%d", data->rop2));
    }

  if (values_mask & GDK_GC_FILL)
    {
      data->fill_style = values->fill;
      GDK_NOTE (MISC, g_print (" fill=%d", data->fill_style));
    }

  if (values_mask & GDK_GC_TILE)
    {
      data->tile = values->tile;
      gdk_drawable_ref (data->tile);
      GDK_NOTE (MISC, g_print (" tile=%#x", GDK_DRAWABLE_XID (data->tile)));
    }
  else
    data->tile = NULL;

  if (values_mask & GDK_GC_STIPPLE)
    {
      data->stipple = values->stipple;
      gdk_drawable_ref (data->stipple);
      GDK_NOTE (MISC, g_print (" stipple=%#x", GDK_DRAWABLE_XID (data->stipple)));
    }
  else
    data->stipple = NULL;

  if (values_mask & GDK_GC_CLIP_MASK)
    {
      data->clip_region =
	BitmapToRegion ((HBITMAP) GDK_DRAWABLE_XID (values->clip_mask));
      GDK_NOTE (MISC, g_print (" clip=%#x", data->clip_region));
    }
  else
    data->clip_region = NULL;

  if (values_mask & GDK_GC_SUBWINDOW)
    {
      data->subwindow_mode = values->subwindow_mode;
      GDK_NOTE (MISC, g_print (" subw=%d", data->subwindow_mode));
    }

  if (values_mask & GDK_GC_TS_X_ORIGIN)
    {
      data->ts_x_origin = values->ts_x_origin;
      GDK_NOTE (MISC, g_print (" ts_x=%d", data->ts_x_origin));
    }

  if (values_mask & GDK_GC_TS_Y_ORIGIN)
    {
      data->ts_y_origin = values->ts_y_origin;
      GDK_NOTE (MISC, g_print (" ts_y=%d", data->ts_y_origin));
    }

  if (values_mask & GDK_GC_CLIP_X_ORIGIN)
    {
      data->clip_x_origin = values->clip_x_origin;
      GDK_NOTE (MISC, g_print (" clip_x=%d", data->clip_x_origin));
    }

  if (values_mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      data->clip_y_origin = values->clip_y_origin; 
      GDK_NOTE (MISC, g_print (" clip_y=%d", data->clip_y_origin));
   }
 
  if (values_mask & GDK_GC_EXPOSURES)
    {
      data->graphics_exposures = values->graphics_exposures;
      GDK_NOTE (MISC, g_print (" exp=%d", data->graphics_exposures));
    }

  data->pen_style = PS_GEOMETRIC;
  data->pen_width = 1;

  if (values_mask & (GDK_GC_LINE_WIDTH | GDK_GC_LINE_STYLE))
    {
      if (values_mask & GDK_GC_LINE_WIDTH)
	{
	  data->pen_width = values->line_width;
	  GDK_NOTE (MISC, g_print (" pw=%d", data->pen_width));
	}
      if (values_mask & GDK_GC_LINE_STYLE)
	{
	  switch (values->line_style)
	    {
	    case GDK_LINE_SOLID:
	      data->pen_style |= PS_SOLID; break;
	    case GDK_LINE_ON_OFF_DASH:
	    case GDK_LINE_DOUBLE_DASH: /* ??? */
	      data->pen_style |= PS_DASH; break;
	    }
	  GDK_NOTE (MISC, g_print (" ps=%#x", data->pen_style));
	}
    }

  if (values_mask & GDK_GC_CAP_STYLE)
    {
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:	/* ??? */
	case GDK_CAP_BUTT:
	  data->pen_style |= PS_ENDCAP_FLAT;	break;
	case GDK_CAP_ROUND:
	  data->pen_style |= PS_ENDCAP_ROUND; break;
	case GDK_CAP_PROJECTING:
	  data->pen_style |= PS_ENDCAP_SQUARE; break;
	}
      GDK_NOTE (MISC, g_print (" ps=%#x", data->pen_style));
    }

  if (values_mask & GDK_GC_JOIN_STYLE)
    {
      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  data->pen_style |= PS_JOIN_MITER;
	  break;
	case GDK_JOIN_ROUND:
	  data->pen_style |= PS_JOIN_ROUND;
	  break;
	case GDK_JOIN_BEVEL:
	  data->pen_style |= PS_JOIN_BEVEL;
	  break;
	}
      GDK_NOTE (MISC, g_print (" ps=%#x", data->pen_style));
    }

  data->hwnd = NULL;
  data->xgc = NULL;

  GDK_NOTE (MISC, g_print ("} = %p\n", gc));

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

  values->foreground = data->foreground;
  values->background = data->background;
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
  if (data->clip_region != NULL)
    {
      RECT rect;
      HBRUSH hbr;
      HDC hdc;
      HGDIOBJ oldbitmap;
      GdkPixmap *pixmap;

      GetRgnBox (data->clip_region, &rect);
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
      if (!FillRgn (hdc, data->clip_region, hbr))
	g_warning ("gdk_gc_get_values: FillRgn failed");
      if (SelectObject (hdc, oldbitmap) == NULL)
	g_warning ("gdk_gc_get_values: SelectObject #2 failed");
      DeleteDC (hdc);
      values->clip_mask = pixmap;
    }
  else
    values->clip_mask = NULL;
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
			 GdkGCValuesMask  values_mask)
{
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc);
  gchar *xlfd;

  GDK_NOTE (MISC, g_print ("gdk_win32_gc_set_values: {"));

  if (values_mask & GDK_GC_FOREGROUND)
    {
      GDK_NOTE (MISC, g_print ("fg = %s ",
			       gdk_color_to_string (&values->foreground)));
      data->foreground = values->foreground;
      data->values_mask |= GDK_GC_FOREGROUND;
    }
  
  if (values_mask & GDK_GC_BACKGROUND)
    {
      GDK_NOTE (MISC, g_print ("bg = %s ",
			       gdk_color_to_string (&values->foreground)));
      data->background = values->background;
      data->values_mask |= GDK_GC_BACKGROUND;
    }

  if (values_mask & GDK_GC_FONT)
    {
      if (data->font != NULL)
	gdk_font_unref (data->font);
      data->font = values->font;
      if (data->font != NULL)
	{
	  GDK_NOTE (MISC, (xlfd = gdk_font_xlfd_create (data->font),
			   g_print ("font = %s ", xlfd),
			   gdk_font_xlfd_free (xlfd)));
	  gdk_font_ref (data->font);
	  data->values_mask |= GDK_GC_FONT;
	}
      else
	{
	  GDK_NOTE (MISC, g_print ("font = NULL "));
	  data->values_mask &= ~GDK_GC_FONT;
	}
    }

  if (values_mask & GDK_GC_FUNCTION)
    {
      GDK_NOTE (MISC, g_print ("fun = %d ", values->function));

      switch (values->function)
	{
	case GDK_COPY:
	  data->rop2 = R2_COPYPEN; break;
	case GDK_INVERT:
	  data->rop2 = R2_NOT; break;
	case GDK_XOR:
	  data->rop2 = R2_XORPEN; break;
	case GDK_CLEAR:
	  data->rop2 = R2_BLACK; break;
	case GDK_AND:
	  data->rop2 = R2_MASKPEN; break;
	case GDK_AND_REVERSE:
	  data->rop2 = R2_MASKPENNOT; break;
	case GDK_AND_INVERT:
	  data->rop2 = R2_MASKNOTPEN; break;
	case GDK_NOOP:
	  data->rop2 = R2_NOP; break;
	case GDK_OR:
	  data->rop2 = R2_MERGEPEN; break;
	case GDK_EQUIV:
	  data->rop2 = R2_NOTXORPEN; break;
	case GDK_OR_REVERSE:
	  data->rop2 = R2_MERGEPENNOT; break;
	case GDK_COPY_INVERT:
	  data->rop2 = R2_NOTCOPYPEN; break;
	case GDK_OR_INVERT:
	  data->rop2 = R2_MERGENOTPEN; break;
	case GDK_NAND:
	  data->rop2 = R2_NOTMASKPEN; break;
	case GDK_SET:
	  data->rop2 = R2_WHITE; break;
	}
      data->values_mask |= GDK_GC_FUNCTION;
    }

  if (values_mask & GDK_GC_FILL)
    {
      GDK_NOTE (MISC, g_print ("fill = %d ", values->fill));
      data->fill_style = values->fill;
      data->values_mask |= GDK_GC_FILL;
    }

  if (values_mask & GDK_GC_TILE)
    {
      if (data->tile != NULL)
	gdk_drawable_unref (data->tile);
      data->tile = values->tile;
      if (data->tile != NULL)
	{
	  GDK_NOTE (MISC, g_print ("tile = %#x ",
				   GDK_DRAWABLE_XID (values->tile)));
	  gdk_drawable_ref (data->tile);
	  data->values_mask |= GDK_GC_TILE;
	}
      else
	{
	  GDK_NOTE (MISC, g_print ("tile = NULL "));
	  data->values_mask &= ~GDK_GC_TILE;
	}
    }

  if (values_mask & GDK_GC_STIPPLE)
    {
      if (data->stipple != NULL)
	gdk_drawable_unref (data->stipple);
      data->stipple = values->stipple;
      if (data->stipple != NULL)
	{
	  GDK_NOTE (MISC, g_print ("stipple = %#x ",
				   GDK_DRAWABLE_XID (values->stipple)));
	  gdk_drawable_ref (data->stipple);
	  data->values_mask |= GDK_GC_STIPPLE;
	}
      else
	{
	  GDK_NOTE (MISC, g_print ("stipple = NULL "));
	  data->values_mask &= ~GDK_GC_STIPPLE;
	}
    }

  if (values_mask & GDK_GC_CLIP_MASK)
    {
      if (data->clip_region != NULL)
	if (!DeleteObject (data->clip_region))
	  g_warning ("gdk_win32_gc_set_values: DeleteObject failed");
      if (values->clip_mask != NULL)
	{
	  data->clip_region =
	    BitmapToRegion (GDK_DRAWABLE_XID (values->clip_mask));
	  data->values_mask |= GDK_GC_CLIP_MASK;
	}
      else
	{
	  data->clip_region = NULL;
	  data->values_mask &= ~GDK_GC_CLIP_MASK;
	}
    }

  if (values_mask & GDK_GC_SUBWINDOW)
    {
      data->values_mask |= GDK_GC_SUBWINDOW;
    }

  if (values_mask & GDK_GC_TS_X_ORIGIN)
    {
      data->ts_x_origin = values->ts_x_origin;
      data->values_mask |= GDK_GC_TS_X_ORIGIN;
    }
  
  if (values_mask & GDK_GC_TS_Y_ORIGIN)
    {
      data->ts_y_origin = values->ts_y_origin;
      data->values_mask |= GDK_GC_TS_Y_ORIGIN;
    }
  
  if (values_mask & GDK_GC_CLIP_X_ORIGIN)
    {
      data->clip_x_origin = values->clip_x_origin;
      data->values_mask |= GDK_GC_CLIP_X_ORIGIN;
    }
  
  if (values_mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      data->clip_y_origin = values->clip_y_origin;
      data->values_mask |= GDK_GC_CLIP_Y_ORIGIN;
    }
  
  if (values_mask & GDK_GC_EXPOSURES)
    {
      data->values_mask |= GDK_GC_EXPOSURES;
    }

  if (values_mask & GDK_GC_LINE_WIDTH)
    {
      data->pen_width = values->line_width;
      data->values_mask |= GDK_GC_LINE_WIDTH;
    }

  if (values_mask & GDK_GC_LINE_STYLE)
    {
      data->pen_style &= ~(PS_STYLE_MASK);
      switch (values->line_style)
	{
	case GDK_LINE_SOLID:
	  data->pen_style |= PS_SOLID; break;
	case GDK_LINE_ON_OFF_DASH:
	case GDK_LINE_DOUBLE_DASH: /* ??? */
	  data->pen_style |= PS_DASH; break;
	}
      data->values_mask |= GDK_GC_LINE_STYLE;
    }
  
  if (values_mask & GDK_GC_CAP_STYLE)
    {
      data->pen_style &= ~(PS_ENDCAP_MASK);
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:
	  /* ??? */
	  break;
	case GDK_CAP_BUTT:
	  data->pen_style |= PS_ENDCAP_FLAT; break;
	case GDK_CAP_ROUND:
	  data->pen_style |= PS_ENDCAP_ROUND; break;
	case GDK_CAP_PROJECTING:
	  data->pen_style |= PS_ENDCAP_SQUARE; break;
	}
      data->values_mask |= GDK_GC_CAP_STYLE;
    }

  if (values_mask & GDK_GC_JOIN_STYLE)
    {
      data->pen_style &= ~(PS_JOIN_MASK);

      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  data->pen_style |= PS_JOIN_MITER; break;
	case GDK_JOIN_ROUND:
	  data->pen_style |= PS_JOIN_ROUND; break;
	case GDK_JOIN_BEVEL:
	  data->pen_style |= PS_JOIN_BEVEL; break;
	}
      data->values_mask |= GDK_GC_JOIN_STYLE;
    }
}

static void
gdk_win32_gc_set_dashes (GdkGC *gc,
			 gint	  dash_offset,
			 gchar  dash_list[],
			 gint   n)
{
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc);

  /* XXX ??? */

  data->pen_style &= ~(PS_STYLE_MASK);
  data->pen_style |= PS_DASH;
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
      g_warning ("gdk_gc_set_clip_rectangle: DeleteObject failed");
  if (rectangle)
    {
      GDK_NOTE (MISC,
		g_print ("gdk_gc_set_clip_rectangle: (%d) %dx%d@+%d+%d\n",
			 data,
			 rectangle->width, rectangle->height,
			 rectangle->x, rectangle->y));
      if ((data->clip_region =
	   CreateRectRgn (rectangle->x, rectangle->y,
			  rectangle->x + rectangle->width,
			  rectangle->y + rectangle->height)) == NULL)
	g_warning ("gdk_gc_set_clip_rectangle: CreateRectRgn failed");

      data->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_rectangle: (%d) NULL\n",
			       data));
      data->clip_region = NULL;
      data->values_mask &= ~GDK_GC_CLIP_MASK;
    }
    data->values_mask &= ~(GDK_GC_CLIP_X_ORIGIN |GDK_GC_CLIP_Y_ORIGIN);
} 

void
gdk_gc_set_clip_region (GdkGC		 *gc,
			GdkRegion	 *region)
{
  GdkGCWin32Data *data;

  g_return_if_fail (gc != NULL);

  data = GDK_GC_WIN32DATA (gc);

  GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_region: (%d) %s\n",
			   data, (region != NULL ? "xxx" : "None")));

  if (data->clip_region != NULL)
    if (!DeleteObject (data->clip_region))
      g_warning ("gdk_gc_set_clip_region: DeleteObject failed");
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

  if (dst_data->font)
    gdk_font_unref (dst_data->font);
  if (dst_data->tile)
    gdk_drawable_unref (dst_data->tile);
  if (dst_data->stipple)
    gdk_drawable_unref (dst_data->stipple);
  
  *dst_data = *src_data;
  
  if (dst_data->font)
    gdk_font_ref (dst_data->font);
  if (dst_data->tile)
    gdk_drawable_ref (dst_data->tile);
  if (dst_data->stipple)
    gdk_drawable_ref (dst_data->stipple);
}

HDC
gdk_gc_predraw (GdkDrawable  *drawable,
		GdkGCPrivate *gc_private)
{
  GdkDrawablePrivate *drawable_private = (GdkDrawablePrivate *) drawable;
  GdkColormapPrivateWin32 *colormap_private =
    (GdkColormapPrivateWin32 *) drawable_private->colormap;
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc_private);
  GdkVisual *visual;
  COLORREF bg;
  COLORREF fg;
  LOGBRUSH logbrush;
  HPEN hpen;
  HBRUSH hbr;
  guchar r, g, b;
  static guint mask[9] = { 0, 1, 3, 7, 15, 31, 63, 127, 255 };

  g_assert (data->xgc == NULL);

  if (GDK_DRAWABLE_TYPE (drawable) == GDK_DRAWABLE_PIXMAP)
    {
      if ((data->xgc = CreateCompatibleDC (NULL)) == NULL)
	g_warning ("gdk_gc_predraw: CreateCompatibleDC failed");

      if ((data->saved_dc = SaveDC (data->xgc)) == 0)
	g_warning ("gdk_gc_predraw: SaveDC #1 failed");
      
      if (SelectObject (data->xgc, GDK_DRAWABLE_XID (drawable)) == NULL)
	g_warning ("gdk_gc_predraw: SelectObject #1 failed");
    }
  else
    {
      if ((data->xgc = GetDC (GDK_DRAWABLE_XID (drawable))) == NULL)
	g_warning ("gdk_gc_predraw: GetDC failed");
      
      if ((data->saved_dc = SaveDC (data->xgc)) == 0)
	g_warning ("gdk_gc_predraw: SaveDC #2 failed");
    }
  
  data->hwnd = GDK_DRAWABLE_XID (drawable);
  
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
      SelectPalette (data->xgc, hpal, FALSE);
      RealizePalette (data->xgc);
      fg = PALETTEINDEX (data->foreground.pixel);
    }
  else if (colormap_private->xcolormap->rc_palette)
    {
      int k;
      if (SelectPalette (data->xgc,
			 colormap_private->xcolormap->palette, FALSE) == NULL)
	g_warning ("gdk_gc_predraw: SelectPalette failed");
      if (TRUE || colormap_private->xcolormap->stale)
	{
	  if ((k = RealizePalette (data->xgc)) == GDI_ERROR)
	    g_warning ("gdk_gc_predraw: RealizePalette failed");
	  colormap_private->xcolormap->stale = FALSE;
	}
#if 0
      g_print ("Selected palette %#x for gc %#x, realized %d colors\n",
	       colormap_private->xcolormap->palette, data->xgc, k);
#endif
      fg = PALETTEINDEX (data->foreground.pixel);
    }
  else
    {
      visual = colormap_private->visual;
      r = (data->foreground.pixel & visual->red_mask) >> visual->red_shift;
      r = (r * 255) / mask[visual->red_prec];
      g = (data->foreground.pixel & visual->green_mask) >> visual->green_shift;
      g = (g * 255) / mask[visual->green_prec];
      b = (data->foreground.pixel & visual->blue_mask) >> visual->blue_shift;
      b = (b * 255) / mask[visual->blue_prec];

      fg = GetNearestColor (data->xgc, RGB (r, g, b));
    }
  logbrush.lbStyle = BS_SOLID;
  logbrush.lbColor = fg;
  if ((hpen = ExtCreatePen (data->pen_style, data->pen_width,
			    &logbrush, 0, NULL)) == NULL)
    g_warning ("gdk_gc_predraw: CreatePen failed");
  
  if (SelectObject (data->xgc, hpen) == NULL)
    g_warning ("gdk_gc_predraw: SelectObject #2 failed");

  if (SetTextColor (data->xgc, fg) == CLR_INVALID)
    g_warning ("gdk_gc_predraw: SetTextColor failed");

#if 0
  switch (data->fill_style)
    {
    case GDK_STIPPLED:
      {
	GdkPixmap *stipple = data->stipple;
	GdkPixmapPrivate *stipple_private = (GdkPixmapPrivate *) stipple;
	HBITMAP hbm = GDK_DRAWABLE_XID (stipple);
	if (NULL == (hbr = CreatePatternBrush (hbm)))
	  g_warning ("gdk_gc_predraw: CreatePatternBrush failed");
	
#ifdef NATIVE_WIN16
	SetBrushOrg  (data->xgc, data->ts_x_origin,
		      data->ts_y_origin);
#else
	SetBrushOrgEx(data->xgc, data->ts_x_origin,
		      data->ts_y_origin, NULL);
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
  if (SelectObject (data->xgc, hbr) == NULL)
    g_warning ("gdk_gc_predraw: SelectObject #3 failed");

  if (data->values_mask & GDK_GC_BACKGROUND)
    {
      if (colormap_private == NULL)
	{
	  /* a bitmap */
	  bg = PALETTEINDEX (data->background.pixel);
	}
      else if (colormap_private->xcolormap->rc_palette)
	{
	  bg = PALETTEINDEX (data->background.pixel);
	}
      else
	{
	  visual = colormap_private->visual;
	  r = (data->background.pixel & visual->red_mask) >> visual->red_shift;
	  r = (r * 255) / mask[visual->red_prec];
	  g = (data->background.pixel & visual->green_mask) >> visual->green_shift;
	  g = (g * 255) / mask[visual->green_prec];
	  b = (data->background.pixel & visual->blue_mask) >> visual->blue_shift;
	  b = (b * 255) / mask[visual->green_prec];
	  
	  fg = GetNearestColor (data->xgc, RGB (r, g, b));
	}
      if (SetBkColor (data->xgc, bg) == CLR_INVALID)
	g_warning ("gdk_gc_predraw: SetBkColor failed");
    }
  
  if (SetBkMode (data->xgc, TRANSPARENT) == 0)
    g_warning ("gdk_gc_predraw: SetBkMode failed");
  
  if (SetTextAlign (data->xgc, TA_BASELINE) == GDI_ERROR)
    g_warning ("gdk_gc_predraw: SetTextAlign failed");
  
  if (data->values_mask & GDK_GC_FUNCTION)
    if (SetROP2 (data->xgc, data->rop2) == 0)
      g_warning ("gdk_gc_predraw: SetROP2 failed");

  if (data->values_mask & GDK_GC_CLIP_MASK
      && data->clip_region != NULL)
    {
      if (data->values_mask & (GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN))
	OffsetRgn (data->clip_region,
		   data->clip_x_origin, data->clip_y_origin);
      SelectClipRgn (data->xgc, data->clip_region);
    }

  return data->xgc;
}

void
gdk_gc_postdraw (GdkDrawable  *drawable,
		 GdkGCPrivate *gc_private)
{
  GdkDrawablePrivate *drawable_private = (GdkDrawablePrivate *) drawable;
  GdkColormapPrivateWin32 *colormap_private =
    (GdkColormapPrivateWin32 *) drawable_private->colormap;
  GdkGCWin32Data *data = GDK_GC_WIN32DATA (gc_private);
  HGDIOBJ hpen;
  HGDIOBJ hbr;

  if ((hpen = GetCurrentObject (data->xgc, OBJ_PEN)) == NULL)
    g_warning ("gdk_gc_postdraw: GetCurrentObject #1 failed");

  if ((hbr = GetCurrentObject (data->xgc, OBJ_BRUSH)) == NULL)
    g_warning ("gdk_gc_postdraw: GetCurrentObject #2 failed");

  if (!RestoreDC (data->xgc, data->saved_dc))
    g_warning ("gdk_gc_postdraw: RestoreDC failed");
#if 0
  if (colormap_private != NULL
      && colormap_private->xcolormap->rc_palette
      && colormap_private->xcolormap->stale)
    {
      SelectPalette (data->xgc, GetStockObject (DEFAULT_PALETTE), FALSE);
      if (!UnrealizeObject (colormap_private->xcolormap->palette))
	g_warning ("gdk_gc_postraw: UnrealizeObject failed");
    }
#endif
  if (GDK_DRAWABLE_TYPE (drawable) == GDK_DRAWABLE_PIXMAP)
    {
      if (!DeleteDC (data->xgc))
	g_warning ("gdk_gc_postdraw: DeleteDC failed");
    }
  else
    {
      ReleaseDC (data->hwnd, data->xgc);
    }

  if (hpen != NULL)
    if (!DeleteObject (hpen))
      g_warning ("gdk_gc_postdraw: DeleteObject #1 failed");
  
  if (hbr != NULL)
    if (!DeleteObject (hbr))
      g_warning ("gdk_gc_postdraw: DeleteObject #2 failed");

  data->xgc = NULL;
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
