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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
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
				     gint8            dash_list[],
				     gint             n);

static void gdk_gc_win32_class_init (GdkGCWin32Class *klass);
static void gdk_gc_win32_finalize   (GObject         *object);

static gpointer parent_class = NULL;

GType
gdk_gc_win32_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkGCWin32Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_gc_win32_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkGCWin32),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (GDK_TYPE_GC,
                                            "GdkGCWin32",
                                            &object_info);
    }
  
  return object_type;
}

static void
gdk_gc_win32_class_init (GdkGCWin32Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkGCClass *gc_class = GDK_GC_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_gc_win32_finalize;

  gc_class->get_values = gdk_win32_gc_get_values;
  gc_class->set_values = gdk_win32_gc_set_values;
  gc_class->set_dashes = gdk_win32_gc_set_dashes;
}

static void
gdk_gc_win32_finalize (GObject *object)
{
  GdkGCWin32 *win32_gc = GDK_GC_WIN32 (object);
  
  if (win32_gc->clip_region)
    gdk_region_destroy (win32_gc->clip_region);
  
  if (win32_gc->values_mask & GDK_GC_FONT)
    gdk_font_unref (win32_gc->font);
  
  if (win32_gc->values_mask & GDK_GC_TILE)
    gdk_drawable_unref (win32_gc->tile);
  
  if (win32_gc->values_mask & GDK_GC_STIPPLE)
    gdk_drawable_unref (win32_gc->stipple);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_win32_gc_values_to_win32values (GdkGCValues    *values,
				    GdkGCValuesMask mask,
				    GdkGCWin32     *win32_gc)
{				    
  char *s = "";
  gint sw, sh;

  GDK_NOTE (MISC, g_print ("{"));

  if (mask & GDK_GC_FOREGROUND)
    {
      win32_gc->foreground = values->foreground.pixel;
      win32_gc->values_mask |= GDK_GC_FOREGROUND;
      GDK_NOTE (MISC, (g_print ("fg=%.06x", win32_gc->foreground),
		       s = ","));
    }
  
  if (mask & GDK_GC_BACKGROUND)
    {
      win32_gc->background = values->background.pixel;
      win32_gc->values_mask |= GDK_GC_BACKGROUND;
      GDK_NOTE (MISC, (g_print ("%sbg=%.06x", s, win32_gc->background),
		       s = ","));
    }

  if ((mask & GDK_GC_FONT) && (values->font->type == GDK_FONT_FONT
			       || values->font->type == GDK_FONT_FONTSET))
    {
      if (win32_gc->font != NULL)
	gdk_font_unref (win32_gc->font);
      win32_gc->font = values->font;
      if (win32_gc->font != NULL)
	{
	  gchar *xlfd;

	  gdk_font_ref (win32_gc->font);
	  win32_gc->values_mask |= GDK_GC_FONT;
	  GDK_NOTE (MISC, (xlfd = gdk_font_full_name_get (win32_gc->font),
			   g_print ("%sfont=%s", s, xlfd),
			   s = ",",
			   gdk_font_full_name_free (xlfd)));
	}
      else
	{
	  win32_gc->values_mask &= ~GDK_GC_FONT;
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
	  win32_gc->rop2 = R2_COPYPEN;
	  GDK_NOTE (MISC, g_print ("COPYPEN"));
	  break;
	case GDK_INVERT:
	  win32_gc->rop2 = R2_NOT;
	  GDK_NOTE (MISC, g_print ("NOT"));
	  break;
	case GDK_XOR:
	  win32_gc->rop2 = R2_XORPEN;
	  GDK_NOTE (MISC, g_print ("XORPEN"));
	  break;
	case GDK_CLEAR:
	  win32_gc->rop2 = R2_BLACK;
	  GDK_NOTE (MISC, g_print ("BLACK"));
	  break;
	case GDK_AND:
	  win32_gc->rop2 = R2_MASKPEN;
	  GDK_NOTE (MISC, g_print ("MASKPEN"));
	  break;
	case GDK_AND_REVERSE:
	  win32_gc->rop2 = R2_MASKPENNOT;
	  GDK_NOTE (MISC, g_print ("MASKPENNOT"));
	  break;
	case GDK_AND_INVERT:
	  win32_gc->rop2 = R2_MASKNOTPEN;
	  GDK_NOTE (MISC, g_print ("MASKNOTPEN"));
	  break;
	case GDK_NOOP:
	  win32_gc->rop2 = R2_NOP;
	  GDK_NOTE (MISC, g_print ("NOP"));
	  break;
	case GDK_OR:
	  win32_gc->rop2 = R2_MERGEPEN;
	  GDK_NOTE (MISC, g_print ("MERGEPEN"));
	  break;
	case GDK_EQUIV:
	  win32_gc->rop2 = R2_NOTXORPEN;
	  GDK_NOTE (MISC, g_print ("NOTXORPEN"));
	  break;
	case GDK_OR_REVERSE:
	  win32_gc->rop2 = R2_MERGEPENNOT;
	  GDK_NOTE (MISC, g_print ("MERGEPENNOT"));
	  break;
	case GDK_COPY_INVERT:
	  win32_gc->rop2 = R2_NOTCOPYPEN;
	  GDK_NOTE (MISC, g_print ("NOTCOPYPEN"));
	  break;
	case GDK_OR_INVERT:
	  win32_gc->rop2 = R2_MERGENOTPEN;
	  GDK_NOTE (MISC, g_print ("MERGENOTPEN"));
	  break;
	case GDK_NAND:
	  win32_gc->rop2 = R2_NOTMASKPEN;
	  GDK_NOTE (MISC, g_print ("NOTMASKPEN"));
	  break;
	case GDK_NOR:
	  win32_gc->rop2 = R2_NOTMERGEPEN;
	  GDK_NOTE (MISC, g_print ("NOTMERGEPEN"));
	  break;
	case GDK_SET:
	  win32_gc->rop2 = R2_WHITE;
	  GDK_NOTE (MISC, g_print ("WHITE"));
	  break;
	}
      win32_gc->values_mask |= GDK_GC_FUNCTION;
    }

  if (mask & GDK_GC_FILL)
    {
      win32_gc->fill_style = values->fill;
      win32_gc->values_mask |= GDK_GC_FILL;
      GDK_NOTE (MISC, (g_print ("%sfill=%d", s, win32_gc->fill_style),
		       s = ","));
    }

  if (mask & GDK_GC_TILE)
    {
      if (win32_gc->tile != NULL)
	gdk_drawable_unref (win32_gc->tile);
      win32_gc->tile = values->tile;
      if (win32_gc->tile != NULL)
	{
	  gdk_drawable_ref (win32_gc->tile);
	  win32_gc->values_mask |= GDK_GC_TILE;
	  GDK_NOTE (MISC, (g_print ("%stile=%#x", s,
				    GDK_PIXMAP_HBITMAP (win32_gc->tile)),
			   s = ","));
	}
      else
	{
	  win32_gc->values_mask &= ~GDK_GC_TILE;
	  GDK_NOTE (MISC, (g_print ("%stile=NULL", s),
			   s = ","));
	}
    }

  if (mask & GDK_GC_STIPPLE)
    {
      if (win32_gc->stipple != NULL)
	gdk_drawable_unref (win32_gc->stipple);
      win32_gc->stipple = values->stipple;
      if (win32_gc->stipple != NULL)
	{
	  gdk_drawable_get_size (win32_gc->stipple, &sw, &sh);

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
		      gdk_draw_drawable (bm, gc, win32_gc->stipple, 0, 0, i, j, sw, sh);
		      j += sh;
		    }
		  i += sw;
		}
	      win32_gc->stipple = bm;
	      gdk_gc_unref (gc);
	    }
	  else
	    gdk_drawable_ref (win32_gc->stipple);
	  win32_gc->values_mask |= GDK_GC_STIPPLE;
	  GDK_NOTE (MISC, (g_print ("%sstipple=%#x", s,
				    GDK_PIXMAP_HBITMAP (win32_gc->stipple)),
			   s = ","));
	}
      else
	{
	  win32_gc->values_mask &= ~GDK_GC_STIPPLE;
	  GDK_NOTE (MISC, (g_print ("%sstipple=NULL", s),
			   s = ","));
	}
    }

  if (mask & GDK_GC_CLIP_MASK)
    {
      if (win32_gc->clip_region != NULL)
	{
	  gdk_region_destroy (win32_gc->clip_region);
	  win32_gc->clip_region = NULL;
	}
      if (win32_gc->hcliprgn != NULL)
	DeleteObject (win32_gc->hcliprgn);

      if (values->clip_mask != NULL)
	{
	  win32_gc->hcliprgn =
	    BitmapToRegion ((HBITMAP) GDK_PIXMAP_HBITMAP (values->clip_mask));
	  win32_gc->values_mask |= GDK_GC_CLIP_MASK;
	  OffsetRgn (win32_gc->hcliprgn,
		     win32_gc->parent_instance.clip_x_origin,
		     win32_gc->parent_instance.clip_y_origin);
	}
      else
	{
	  win32_gc->hcliprgn = NULL;
	  win32_gc->values_mask &= ~GDK_GC_CLIP_MASK;
	}
      GDK_NOTE (MISC, (g_print ("%sclip=%#x", s, win32_gc->hcliprgn),
		       s = ","));
    }

  if (mask & GDK_GC_SUBWINDOW)
    {
      win32_gc->subwindow_mode = values->subwindow_mode;
      win32_gc->values_mask |= GDK_GC_SUBWINDOW;
      GDK_NOTE (MISC, (g_print ("%ssubw=%d", s, win32_gc->subwindow_mode),
		       s = ","));
    }

  if (mask & GDK_GC_TS_X_ORIGIN)
    {
      win32_gc->values_mask |= GDK_GC_TS_X_ORIGIN;
      GDK_NOTE (MISC, (g_print ("%sts_x=%d", s, values->ts_x_origin),
		       s = ","));
    }

  if (mask & GDK_GC_TS_Y_ORIGIN)
    {
      win32_gc->values_mask |= GDK_GC_TS_Y_ORIGIN;
      GDK_NOTE (MISC, (g_print ("%sts_y=%d", s, values->ts_y_origin),
		       s = ","));
    }

  if (mask & GDK_GC_CLIP_X_ORIGIN)
    {
      win32_gc->values_mask |= GDK_GC_CLIP_X_ORIGIN;
      GDK_NOTE (MISC, (g_print ("%sclip_x=%d", s, values->clip_x_origin),
		       s = ","));
    }

  if (mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      win32_gc->values_mask |= GDK_GC_CLIP_Y_ORIGIN;
      GDK_NOTE (MISC, (g_print ("%sclip_y=%d", s, values->clip_y_origin),
		       s = ","));
   }
 
  if (mask & GDK_GC_EXPOSURES)
    {
      win32_gc->graphics_exposures = values->graphics_exposures;
      win32_gc->values_mask |= GDK_GC_EXPOSURES;
      GDK_NOTE (MISC, (g_print ("%sexp=%d", s, win32_gc->graphics_exposures),
		       s = ","));
    }

  if (mask & GDK_GC_LINE_WIDTH)
    {
      win32_gc->pen_width = values->line_width;
      win32_gc->values_mask |= GDK_GC_LINE_WIDTH;
      GDK_NOTE (MISC, (g_print ("%spw=%d", s, win32_gc->pen_width),
		       s = ","));
    }

  if (mask & GDK_GC_LINE_STYLE)
    {
      win32_gc->pen_style &= ~(PS_STYLE_MASK);
      GDK_NOTE (MISC, (g_print ("%sps|=", s),
		       s = ","));
      switch (values->line_style)
	{
	case GDK_LINE_SOLID:
	  GDK_NOTE (MISC, g_print ("LINE_SOLID"));
	  win32_gc->pen_style |= PS_SOLID;
	  break;
	case GDK_LINE_ON_OFF_DASH:
	case GDK_LINE_DOUBLE_DASH: /* ??? */
	  GDK_NOTE (MISC, g_print ("DASH"));
	  win32_gc->pen_style |= PS_DASH;
	  break;
	}
      win32_gc->values_mask |= GDK_GC_LINE_STYLE;
    }

  if (mask & GDK_GC_CAP_STYLE)
    {
      win32_gc->pen_style &= ~(PS_ENDCAP_MASK);
      GDK_NOTE (MISC, (g_print ("%sps|=", s),
		       s = ","));
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:	/* ??? */
	case GDK_CAP_BUTT:
	  GDK_NOTE (MISC, g_print ("ENDCAP_FLAT"));
	  win32_gc->pen_style |= PS_ENDCAP_FLAT;
	  break;
	case GDK_CAP_ROUND:
	  GDK_NOTE (MISC, g_print ("ENDCAP_ROUND"));
	  win32_gc->pen_style |= PS_ENDCAP_ROUND;
	  break;
	case GDK_CAP_PROJECTING:
	  GDK_NOTE (MISC, g_print ("ENDCAP_SQUARE"));
	  win32_gc->pen_style |= PS_ENDCAP_SQUARE;
	  break;
	}
      win32_gc->values_mask |= GDK_GC_CAP_STYLE;
    }

  if (mask & GDK_GC_JOIN_STYLE)
    {
      win32_gc->pen_style &= ~(PS_JOIN_MASK);
      GDK_NOTE (MISC, (g_print ("%sps|=", s),
		       s = ","));
      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  GDK_NOTE (MISC, g_print ("JOIN_MITER"));
	  win32_gc->pen_style |= PS_JOIN_MITER;
	  break;
	case GDK_JOIN_ROUND:
	  GDK_NOTE (MISC, g_print ("JOIN_ROUND"));
	  win32_gc->pen_style |= PS_JOIN_ROUND;
	  break;
	case GDK_JOIN_BEVEL:
	  GDK_NOTE (MISC, g_print ("JOIN_BEVEL"));
	  win32_gc->pen_style |= PS_JOIN_BEVEL;
	  break;
	}
      win32_gc->values_mask |= GDK_GC_JOIN_STYLE;
    }
  GDK_NOTE (MISC, g_print ("}\n"));
}

GdkGC*
_gdk_win32_gc_new (GdkDrawable	  *drawable,
		   GdkGCValues	  *values,
		   GdkGCValuesMask mask)
{
  GdkGC *gc;
  GdkGCWin32 *win32_gc;

  /* NOTICE that the drawable here has to be the impl drawable,
   * not the publically-visible drawables.
   */
  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable), NULL);

  gc = g_object_new (gdk_gc_win32_get_type (), NULL);
  win32_gc = GDK_GC_WIN32 (gc);

  win32_gc->hdc = NULL;
  win32_gc->clip_region = NULL;
  win32_gc->hcliprgn = NULL;

  /* Use the same default values as X11 does, even if they don't make
   * sense per se. But apps always set fg and bg anyway.
   */
  win32_gc->foreground = 0;
  win32_gc->background = 1;
  win32_gc->font = NULL;
  win32_gc->rop2 = R2_COPYPEN;
  win32_gc->fill_style = GDK_SOLID;
  win32_gc->tile = NULL;
  win32_gc->stipple = NULL;
  win32_gc->pen_style = PS_GEOMETRIC|PS_ENDCAP_FLAT|PS_JOIN_MITER;
  win32_gc->pen_width = 0;

  win32_gc->values_mask = GDK_GC_FUNCTION | GDK_GC_FILL;

  GDK_NOTE (MISC, g_print ("_gdk_win32_gc_new: "));
  gdk_win32_gc_values_to_win32values (values, mask, win32_gc);

  win32_gc->hwnd = NULL;

  GDK_NOTE (MISC, g_print (" = %p\n", gc));

  return gc;
}

static void
gdk_win32_gc_get_values (GdkGC       *gc,
			 GdkGCValues *values)
{
  GdkGCWin32 *win32_gc = GDK_GC_WIN32 (gc);

  values->foreground.pixel = win32_gc->foreground;
  values->background.pixel = win32_gc->background;
  values->font = win32_gc->font;

  switch (win32_gc->rop2)
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

  values->fill = win32_gc->fill_style;

  values->tile = win32_gc->tile;
  values->stipple = win32_gc->stipple;

  /* Also the X11 backend always returns a NULL clip_mask */
  values->clip_mask = NULL;

  values->subwindow_mode = win32_gc->subwindow_mode;
  values->ts_x_origin = win32_gc->parent_instance.ts_x_origin;
  values->ts_y_origin = win32_gc->parent_instance.ts_y_origin;
  values->clip_x_origin = win32_gc->parent_instance.clip_x_origin;
  values->clip_y_origin = win32_gc->parent_instance.clip_y_origin;
  values->graphics_exposures = win32_gc->graphics_exposures;
  values->line_width = win32_gc->pen_width;
  
  if (win32_gc->pen_style & PS_SOLID)
    values->line_style = GDK_LINE_SOLID;
  else if (win32_gc->pen_style & PS_DASH)
    values->line_style = GDK_LINE_ON_OFF_DASH;
  else
    values->line_style = GDK_LINE_SOLID;

  /* PS_ENDCAP_ROUND is zero */
  if (win32_gc->pen_style & PS_ENDCAP_FLAT)
    values->cap_style = GDK_CAP_BUTT;
  else if (win32_gc->pen_style & PS_ENDCAP_SQUARE)
    values->cap_style = GDK_CAP_PROJECTING;
  else
    values->cap_style = GDK_CAP_ROUND;
    
  /* PS_JOIN_ROUND is zero */
  if (win32_gc->pen_style & PS_JOIN_MITER)
    values->join_style = GDK_JOIN_MITER;
  else if (win32_gc->pen_style & PS_JOIN_BEVEL)
    values->join_style = GDK_JOIN_BEVEL;
  else
    values->join_style = GDK_JOIN_ROUND;
}

static void
gdk_win32_gc_set_values (GdkGC           *gc,
			 GdkGCValues     *values,
			 GdkGCValuesMask  mask)
{
  g_return_if_fail (GDK_IS_GC (gc));

  GDK_NOTE (MISC, g_print ("gdk_win32_gc_set_values: "));

  gdk_win32_gc_values_to_win32values (values, mask, GDK_GC_WIN32 (gc));
}

static void
gdk_win32_gc_set_dashes (GdkGC *gc,
			 gint	dash_offset,
			 gint8  dash_list[],
			 gint   n)
{
  GdkGCWin32 *win32_gc;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (dash_list != NULL);

  win32_gc = GDK_GC_WIN32 (gc);

  win32_gc->pen_style &= ~(PS_STYLE_MASK);
  win32_gc->pen_style |= PS_DASH;

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
  if (!IS_WIN_NT () && win32_gc->pen_width > 1)
    {
      GDK_NOTE (MISC, g_print ("gdk_win32_gc_set_dashes: not fully supported\n"));
      win32_gc->pen_style |= PS_SOLID;
      return;
    }
  
  /* win32_gc->pen_style = PS_COSMETIC; ??? */
  if (2 == n)
    {
      if ((dash_list[0] == dash_list[1]) && (dash_list[0] > 2))
        {
          win32_gc->pen_style |= PS_DASH;
          GDK_NOTE (MISC, g_print("gdk_win32_gc_set_dashes: PS_DASH (%d,%d)\n", 
                                  dash_list[0], dash_list[1]));
        }
      else
        {
          win32_gc->pen_style |= PS_DOT;
          GDK_NOTE (MISC, g_print("gdk_win32_gc_set_dashes: PS_DOT (%d,%d)\n", 
                                  dash_list[0], dash_list[1]));
        }
    }
  else if (4 == n)
    {
      win32_gc->pen_style |= PS_DASHDOT; 
      GDK_NOTE (MISC, g_print("gdk_win32_gc_set_dashes: PS_DASHDOT (%d,%d,%d,%d)\n", 
                              dash_list[0], dash_list[1],
                              dash_list[2], dash_list[3]));
    }
  else if (6 == n)
    {
      win32_gc->pen_style |= PS_DASHDOTDOT; 
      GDK_NOTE (MISC, g_print("gdk_win32_gc_set_dashes: PS_DASHDOTDOT (%d,%d,%d,%d,%d,%d)\n", 
                              dash_list[0], dash_list[1],
                              dash_list[2], dash_list[3],
                              dash_list[4], dash_list[5]));
    }
  else
    {
      win32_gc->pen_style |= PS_DASH;
      GDK_NOTE (MISC, g_print("gdk_win32_gc_set_dashes: no guess for %d dashes\n", n));
    }
}

void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkGCWin32 *win32_gc;

  g_return_if_fail (GDK_IS_GC (gc));

  win32_gc = GDK_GC_WIN32 (gc);

  if (win32_gc->clip_region)
    gdk_region_destroy (win32_gc->clip_region);

  if (rectangle)
    {
      GDK_NOTE (MISC,
		g_print ("gdk_gc_set_clip_rectangle: (%d) %dx%d@+%d+%d\n",
			 win32_gc,
			 rectangle->width, rectangle->height,
			 rectangle->x, rectangle->y));
      win32_gc->clip_region = gdk_region_rectangle (rectangle);
      win32_gc->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_rectangle: (%d) NULL\n",
			       win32_gc));
      win32_gc->clip_region = NULL;
      win32_gc->values_mask &= ~GDK_GC_CLIP_MASK;
    }
    win32_gc->values_mask &= ~(GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN);
} 

void
gdk_gc_set_clip_region (GdkGC		 *gc,
			GdkRegion	 *region)
{
  GdkGCWin32 *win32_gc;

  g_return_if_fail (GDK_IS_GC (gc));

  win32_gc = GDK_GC_WIN32 (gc);

  if (win32_gc->clip_region)
    gdk_region_destroy (win32_gc->clip_region);

  if (region)
    {
      GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_region: %d %dx%d+%d+%d\n",
			       win32_gc,
			       region->extents.x2 - region->extents.x1,
			       region->extents.y2 - region->extents.y1,
			       region->extents.x1, region->extents.y1));
      win32_gc->clip_region = gdk_region_copy (region);
      win32_gc->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (MISC, g_print ("gdk_gc_set_clip_region: %d NULL\n",
			       win32_gc));
      win32_gc->clip_region = NULL;
      win32_gc->values_mask &= ~GDK_GC_CLIP_MASK;
    }

  gc->clip_x_origin = 0;
  gc->clip_y_origin = 0;

  win32_gc->values_mask &= ~(GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN);
}

void
gdk_gc_copy (GdkGC *dst_gc,
	     GdkGC *src_gc)
{
  GdkGCWin32 *dst_win32_gc;
  GdkGCWin32 *src_win32_gc;
  DWORD nbytes;
  LPRGNDATA rgn;

  g_return_if_fail (GDK_IS_GC_WIN32 (dst_gc));
  g_return_if_fail (GDK_IS_GC_WIN32 (src_gc));
  
  dst_win32_gc = GDK_GC_WIN32 (dst_gc);
  src_win32_gc = GDK_GC_WIN32 (src_gc);

  if (dst_win32_gc->font != NULL)
    gdk_font_unref (dst_win32_gc->font);
  if (dst_win32_gc->tile != NULL)
    gdk_drawable_unref (dst_win32_gc->tile);
  if (dst_win32_gc->stipple != NULL)
    gdk_drawable_unref (dst_win32_gc->stipple);
  if (dst_win32_gc->clip_region != NULL)
    gdk_region_destroy (dst_win32_gc->clip_region);
  
  *dst_win32_gc = *src_win32_gc;

  if (dst_win32_gc->clip_region != NULL)
    dst_win32_gc->clip_region = gdk_region_copy (dst_win32_gc->clip_region);
  if (dst_win32_gc->font != NULL)
    gdk_font_ref (dst_win32_gc->font);
  if (dst_win32_gc->tile != NULL)
    gdk_drawable_ref (dst_win32_gc->tile);
  if (dst_win32_gc->stipple != NULL)
    gdk_drawable_ref (dst_win32_gc->stipple);
}

static guint bitmask[9] = { 0, 1, 3, 7, 15, 31, 63, 127, 255 };

COLORREF
gdk_colormap_color (GdkColormap *colormap,
		    gulong       pixel)
{
  const GdkVisual *visual;
  GdkColormapPrivateWin32 *colormap_private;
  guchar r, g, b;

  if (colormap == NULL)
    return PALETTEINDEX (pixel);

  colormap_private = GDK_COLORMAP_PRIVATE_DATA (colormap);

  if (colormap_private->xcolormap->rc_palette)
    return PALETTEINDEX (pixel);

  visual = colormap->visual;
  r = (pixel & visual->red_mask) >> visual->red_shift;
  r = (r * 255) / bitmask[visual->red_prec];
  g = (pixel & visual->green_mask) >> visual->green_shift;
  g = (g * 255) / bitmask[visual->green_prec];
  b = (pixel & visual->blue_mask) >> visual->blue_shift;
  b = (b * 255) / bitmask[visual->blue_prec];
  
  return RGB (r, g, b);
}

static void
predraw_set_foreground (GdkGC       *gc,
			GdkColormap *colormap,
			gboolean    *ok)
{
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;
  GdkColormapPrivateWin32 *colormap_private;
  COLORREF fg;
  LOGBRUSH logbrush;
  HPEN hpen;
  HBRUSH hbr;

  if (colormap == NULL)
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
      SelectPalette (win32_gc->hdc, hpal, FALSE);
      RealizePalette (win32_gc->hdc);
      fg = PALETTEINDEX (win32_gc->foreground);
    }
  else
    {
      colormap_private = GDK_COLORMAP_PRIVATE_DATA (colormap);
      if (colormap_private->xcolormap->rc_palette)
	{
	  int k;
	  if (SelectPalette (win32_gc->hdc, colormap_private->xcolormap->palette,
			     FALSE) == NULL)
	    WIN32_GDI_FAILED ("SelectPalette"), *ok = FALSE;
	  if (TRUE || colormap_private->xcolormap->stale)
	    {
	      if ((k = RealizePalette (win32_gc->hdc)) == GDI_ERROR)
		WIN32_GDI_FAILED ("RealizePalette"), *ok = FALSE;
	      colormap_private->xcolormap->stale = FALSE;
	    }
#if 0
	  g_print ("Selected palette %#x for gc %#x, realized %d colors\n",
		   colormap_private->xcolormap->palette, win32_gc->hdc, k);
#endif
	}
    }

  fg = gdk_colormap_color (colormap, win32_gc->foreground);

  if (SetTextColor (win32_gc->hdc, fg) == CLR_INVALID)
    WIN32_GDI_FAILED ("SetTextColor"), *ok = FALSE;

  /* Create and select pen and brush. */

  logbrush.lbStyle = BS_SOLID;
  logbrush.lbColor = fg;

  if (*ok && (hpen = ExtCreatePen (win32_gc->pen_style,
				   (win32_gc->pen_width > 0 ? win32_gc->pen_width : 1),
				   &logbrush, 0, NULL)) == NULL)
    WIN32_GDI_FAILED ("ExtCreatePen");
  
  if (SelectObject (win32_gc->hdc, hpen) == NULL)
    WIN32_GDI_FAILED ("SelectObject"), *ok = FALSE;

  switch (win32_gc->fill_style)
    {
    case GDK_OPAQUE_STIPPLED:
      if (*ok && (hbr = CreatePatternBrush (GDK_PIXMAP_HBITMAP (win32_gc->stipple))) == NULL)
	WIN32_GDI_FAILED ("CreatePatternBrush"), *ok = FALSE;
	
      if (*ok && !SetBrushOrgEx(win32_gc->hdc, gc->ts_x_origin,
				gc->ts_y_origin, NULL))
	WIN32_GDI_FAILED ("SetBrushOrgEx"), *ok = FALSE;
      break;

    case GDK_SOLID:
    default:
      if (*ok && (hbr = CreateSolidBrush (fg)) == NULL)
	WIN32_GDI_FAILED ("CreateSolidBrush"), *ok = FALSE;
      break;
  }
  if (*ok && SelectObject (win32_gc->hdc, hbr) == NULL)
    WIN32_GDI_FAILED ("SelectObject"), *ok = FALSE;
}  

void
predraw_set_background (GdkGC       *gc,
			GdkColormap *colormap,
			gboolean    *ok)
{
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;
  COLORREF bg = gdk_colormap_color (colormap, win32_gc->background);

  if (SetBkColor (win32_gc->hdc, bg) == CLR_INVALID)
    WIN32_GDI_FAILED ("SetBkColor"), *ok = FALSE;
}

HDC
gdk_win32_hdc_get (GdkDrawable    *drawable,
		   GdkGC          *gc,
		   GdkGCValuesMask usage)
{
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;
  GdkColormapPrivateWin32 *colormap_private =
    (GdkColormapPrivateWin32 *) GDK_DRAWABLE_IMPL_WIN32 (drawable)->colormap;
  gboolean ok = TRUE;
  int flag;

  g_assert (win32_gc->hdc == NULL);

  win32_gc->hwnd = GDK_DRAWABLE_IMPL_WIN32(drawable)->handle;

  if (GDK_IS_PIXMAP_IMPL_WIN32 (drawable))
    {
      if ((win32_gc->hdc = CreateCompatibleDC (NULL)) == NULL)
	WIN32_GDI_FAILED ("CreateCompatibleDC"), ok = FALSE;

      if (ok && (win32_gc->saved_dc = SaveDC (win32_gc->hdc)) == 0)
	WIN32_GDI_FAILED ("SaveDC"), ok = FALSE;
      
      if (ok && SelectObject (win32_gc->hdc, win32_gc->hwnd) == NULL)
	WIN32_GDI_FAILED ("SelectObject"), ok = FALSE;
    }
  else
    {
      if ((win32_gc->hdc = GetDC (win32_gc->hwnd)) == NULL)
	WIN32_GDI_FAILED ("GetDC");
      
      if (ok && (win32_gc->saved_dc = SaveDC (win32_gc->hdc)) == 0)
	WIN32_GDI_FAILED ("SaveDC");
    }
  
  if (ok && (usage & GDK_GC_FOREGROUND))
    predraw_set_foreground (gc, GDK_DRAWABLE_IMPL_WIN32 (drawable)->colormap, &ok);

  if (ok
      && (usage & GDK_GC_BACKGROUND)
      && (win32_gc->values_mask & GDK_GC_BACKGROUND))
    predraw_set_background (gc, GDK_DRAWABLE_IMPL_WIN32 (drawable)->colormap, &ok);
  
  if (ok && (usage & GDK_GC_FONT))
    {
      if (SetBkMode (win32_gc->hdc, TRANSPARENT) == 0)
	WIN32_GDI_FAILED ("SetBkMode"), ok = FALSE;
  
      if (ok && SetTextAlign (win32_gc->hdc, TA_BASELINE) == GDI_ERROR)
	WIN32_GDI_FAILED ("SetTextAlign"), ok = FALSE;
    }
  
  if (ok && (win32_gc->values_mask & GDK_GC_FUNCTION))
    if (SetROP2 (win32_gc->hdc, win32_gc->rop2) == 0)
      WIN32_GDI_FAILED ("SetROP2"), ok = FALSE;

  if (win32_gc->values_mask & GDK_GC_CLIP_MASK)
    g_assert ((win32_gc->clip_region != NULL) != (win32_gc->hcliprgn != NULL));

  if (ok
      && (win32_gc->values_mask & GDK_GC_CLIP_MASK)
      && win32_gc->clip_region != NULL)
    {
      HRGN hrgn;
      RGNDATA *rgndata;
      RECT *rect;
      GdkRegionBox *boxes = win32_gc->clip_region->rects;
      guint nbytes =
	sizeof (RGNDATAHEADER) + (sizeof (RECT) * win32_gc->clip_region->numRects);
      int i;

      rgndata =	g_malloc (nbytes);
      rgndata->rdh.dwSize = sizeof (RGNDATAHEADER);
      rgndata->rdh.iType = RDH_RECTANGLES;
      rgndata->rdh.nCount = rgndata->rdh.nRgnSize = 0;
      SetRect (&rgndata->rdh.rcBound,
	       G_MAXSHORT, G_MAXSHORT, G_MINSHORT, G_MINSHORT);

      for (i = 0; i < win32_gc->clip_region->numRects; i++)
	{
	  rect = ((RECT *) rgndata->Buffer) + rgndata->rdh.nCount++;

	  rect->left = CLAMP (boxes[i].x1 + gc->clip_x_origin,
			      G_MINSHORT, G_MAXSHORT);
	  rect->right = CLAMP (boxes[i].x2 + gc->clip_x_origin,
			       G_MINSHORT, G_MAXSHORT);
	  rect->top = CLAMP (boxes[i].y1 + gc->clip_y_origin,
			     G_MINSHORT, G_MAXSHORT);
	  rect->bottom = CLAMP (boxes[i].y2 + gc->clip_y_origin,
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

      if (ok && SelectClipRgn (win32_gc->hdc, hrgn) == ERROR)
	WIN32_API_FAILED ("SelectClipRgn"), ok = FALSE;

      if (hrgn != NULL)
	DeleteObject (hrgn);
    }
  else if (ok
	   && (win32_gc->values_mask & GDK_GC_CLIP_MASK)
	   && win32_gc->hcliprgn != NULL)
    {
      if (SelectClipRgn (win32_gc->hdc, win32_gc->hcliprgn) == ERROR)
	WIN32_API_FAILED ("SelectClipRgn"), ok = FALSE;
    }

  if (gdk_debug_flags & GDK_DEBUG_MISC)
    {
      HGDIOBJ obj;
      LOGBRUSH logbrush;
      EXTLOGPEN extlogpen;
      HRGN hrgn;
      RECT rect;

      g_print ("gdk_win32_hdc_get: %d: %#x\n", win32_gc, win32_gc->hdc);
      obj = GetCurrentObject (win32_gc->hdc, OBJ_BRUSH);
      GetObject (obj, sizeof (LOGBRUSH), &logbrush);
      g_print ("brush: style: %s color: %.06x hatch: %#x\n",
	       (logbrush.lbStyle == BS_HOLLOW ? "HOLLOW" :
		(logbrush.lbStyle == BS_PATTERN ? "PATTERN" :
		 (logbrush.lbStyle == BS_SOLID ? "SOLID" :
		  "???"))),
	       logbrush.lbColor,
	       logbrush.lbHatch);
      obj = GetCurrentObject (win32_gc->hdc, OBJ_PEN);
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
      if ((flag = GetClipRgn (win32_gc->hdc, hrgn)) == -1)
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

  return win32_gc->hdc;
}

void
gdk_win32_hdc_release (GdkDrawable    *drawable,
		       GdkGC          *gc,
		       GdkGCValuesMask usage)
{
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;
  HGDIOBJ hpen = NULL;
  HGDIOBJ hbr = NULL;

  if (usage & GDK_GC_FOREGROUND)
    {
      if ((hpen = GetCurrentObject (win32_gc->hdc, OBJ_PEN)) == NULL)
	WIN32_GDI_FAILED ("GetCurrentObject");

      if ((hbr = GetCurrentObject (win32_gc->hdc, OBJ_BRUSH)) == NULL)
	WIN32_GDI_FAILED ("GetCurrentObject");
    }

  if (!RestoreDC (win32_gc->hdc, win32_gc->saved_dc))
    WIN32_GDI_FAILED ("RestoreDC");
#if 0
  if (colormap_private != NULL
      && colormap_private->xcolormap->rc_palette
      && colormap_private->xcolormap->stale)
    {
      SelectPalette (win32_gc->hdc, GetStockObject (DEFAULT_PALETTE), FALSE);
      if (!UnrealizeObject (colormap_private->xcolormap->palette))
	WIN32_GDI_FAILED ("UnrealizeObject");
    }
#endif
  if (GDK_IS_PIXMAP_IMPL_WIN32 (drawable))
    {
      if (!DeleteDC (win32_gc->hdc))
	WIN32_GDI_FAILED ("DeleteDC");
    }
  else
    {
      ReleaseDC (win32_gc->hwnd, win32_gc->hdc);
    }

  if (hpen != NULL)
    if (!DeleteObject (hpen))
      WIN32_GDI_FAILED ("DeleteObject");
  
  if (hbr != NULL)
    if (!DeleteObject (hbr))
      WIN32_GDI_FAILED ("DeleteObject");

  win32_gc->hdc = NULL;
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
