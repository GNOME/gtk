/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2002 Tor Lillqvist
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

#include <string.h>

#include "gdkgc.h"
#include "gdkfont.h"
#include "gdkpixmap.h"
#include "gdkregion-generic.h"
#include "gdkprivate-win32.h"

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
_gdk_gc_win32_get_type (void)
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
                                            &object_info, 0);
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

  GDK_NOTE (GC, g_print ("{"));

  if (mask & GDK_GC_FOREGROUND)
    {
      win32_gc->foreground = values->foreground.pixel;
      win32_gc->values_mask |= GDK_GC_FOREGROUND;
      GDK_NOTE (GC, (g_print ("fg=%.06lx", win32_gc->foreground),
		     s = ","));
    }
  
  if (mask & GDK_GC_BACKGROUND)
    {
      win32_gc->background = values->background.pixel;
      win32_gc->values_mask |= GDK_GC_BACKGROUND;
      GDK_NOTE (GC, (g_print ("%sbg=%.06lx", s, win32_gc->background),
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
	  GDK_NOTE (GC, (xlfd = gdk_font_full_name_get (win32_gc->font),
			 g_print ("%sfont=%s", s, xlfd),
			 s = ",",
			 gdk_font_full_name_free (xlfd)));
	}
      else
	{
	  win32_gc->values_mask &= ~GDK_GC_FONT;
	  GDK_NOTE (GC, (g_print ("%sfont=NULL", s),
			 s = ","));
	}
    }

  if (mask & GDK_GC_FUNCTION)
    {
      GDK_NOTE (GC, (g_print ("%srop2=", s),
		     s = ","));
      switch (values->function)
	{
#define CASE(x,y) case GDK_##x: win32_gc->rop2 = R2_##y; GDK_NOTE (GC, g_print (#y)); break
	CASE (COPY, COPYPEN);
	CASE (INVERT, NOT);
	CASE (XOR, XORPEN);
	CASE (CLEAR, BLACK);
	CASE (AND, MASKPEN);
	CASE (AND_REVERSE, MASKPENNOT);
	CASE (AND_INVERT, MASKNOTPEN);
	CASE (NOOP, NOP);
	CASE (OR, MERGEPEN);
	CASE (EQUIV, NOTXORPEN);
	CASE (OR_REVERSE, MERGEPENNOT);
	CASE (COPY_INVERT, NOTCOPYPEN);
	CASE (OR_INVERT, MERGENOTPEN);
	CASE (NAND, NOTMASKPEN);
	CASE (NOR, NOTMERGEPEN);
	CASE (SET, WHITE);
#undef CASE
	}
      win32_gc->values_mask |= GDK_GC_FUNCTION;
    }

  if (mask & GDK_GC_FILL)
    {
      win32_gc->fill_style = values->fill;
      win32_gc->values_mask |= GDK_GC_FILL;
      GDK_NOTE (GC, (g_print ("%sfill=%d", s, win32_gc->fill_style),
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
	  GDK_NOTE (GC,
		    (g_print ("%stile=%p", s,
			      GDK_PIXMAP_HBITMAP (win32_gc->tile)),
		     s = ","));
	}
      else
	{
	  win32_gc->values_mask &= ~GDK_GC_TILE;
	  GDK_NOTE (GC, (g_print ("%stile=NULL", s),
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

#if 0 /* HB: this size limitation is disabled to make radio and check
       * buttons work. I got the impression from the API docs, that
       * it shouldn't be necessary at all, but win9x would do the clipping
       */
	  if (   (sw != 8 || sh != 8)
	      && !IS_WIN_NT ()) /* HB: the MSDN says it's a Win95 limitation */
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
#endif
	    gdk_drawable_ref (win32_gc->stipple);
	  win32_gc->values_mask |= GDK_GC_STIPPLE;
	  GDK_NOTE (GC,
		    (g_print ("%sstipple=%p", s,
			      GDK_PIXMAP_HBITMAP (win32_gc->stipple)),
		     s = ","));
	}
      else
	{
	  win32_gc->values_mask &= ~GDK_GC_STIPPLE;
	  GDK_NOTE (GC, (g_print ("%sstipple=NULL", s),
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
	{
	  DeleteObject (win32_gc->hcliprgn);
	  win32_gc->hcliprgn = NULL;
	}

      if (values->clip_mask != NULL)
	{
	  win32_gc->hcliprgn =
	    _gdk_win32_bitmap_to_region (values->clip_mask);
	  win32_gc->values_mask |= GDK_GC_CLIP_MASK;
	}
      else
	{
	  win32_gc->hcliprgn = NULL;
	  win32_gc->values_mask &= ~GDK_GC_CLIP_MASK;
	}
      GDK_NOTE (GC, (g_print ("%sclip=%p", s, win32_gc->hcliprgn),
		     s = ","));
    }

  if (mask & GDK_GC_SUBWINDOW)
    {
      win32_gc->subwindow_mode = values->subwindow_mode;
      win32_gc->values_mask |= GDK_GC_SUBWINDOW;
      GDK_NOTE (GC, (g_print ("%ssubw=%d", s, win32_gc->subwindow_mode),
		     s = ","));
    }

  if (mask & GDK_GC_TS_X_ORIGIN)
    {
      win32_gc->values_mask |= GDK_GC_TS_X_ORIGIN;
      GDK_NOTE (GC, (g_print ("%sts_x=%d", s, values->ts_x_origin),
		     s = ","));
    }

  if (mask & GDK_GC_TS_Y_ORIGIN)
    {
      win32_gc->values_mask |= GDK_GC_TS_Y_ORIGIN;
      GDK_NOTE (GC, (g_print ("%sts_y=%d", s, values->ts_y_origin),
		     s = ","));
    }

  if (mask & GDK_GC_CLIP_X_ORIGIN)
    {
      win32_gc->values_mask |= GDK_GC_CLIP_X_ORIGIN;
      GDK_NOTE (GC, (g_print ("%sclip_x=%d", s, values->clip_x_origin),
		     s = ","));
    }

  if (mask & GDK_GC_CLIP_Y_ORIGIN)
    {
      win32_gc->values_mask |= GDK_GC_CLIP_Y_ORIGIN;
      GDK_NOTE (GC, (g_print ("%sclip_y=%d", s, values->clip_y_origin),
		     s = ","));
    }

  if (mask & GDK_GC_EXPOSURES)
    {
      win32_gc->graphics_exposures = values->graphics_exposures;
      win32_gc->values_mask |= GDK_GC_EXPOSURES;
      GDK_NOTE (GC, (g_print ("%sexp=%d", s, win32_gc->graphics_exposures),
		     s = ","));
    }

  if (mask & GDK_GC_LINE_WIDTH)
    {
      win32_gc->pen_width = values->line_width;
      win32_gc->values_mask |= GDK_GC_LINE_WIDTH;
      GDK_NOTE (GC, (g_print ("%spw=%d", s, win32_gc->pen_width),
		     s = ","));
    }

  if (mask & GDK_GC_LINE_STYLE)
    {
      switch (values->line_style)
	{
	case GDK_LINE_SOLID:
        win32_gc->pen_style &= ~(PS_STYLE_MASK);
	  GDK_NOTE (GC, (g_print ("%sps|=LINE_SOLID", s),
			 s = ","));
	  win32_gc->pen_style |= PS_SOLID;
	  break;
	case GDK_LINE_ON_OFF_DASH:
	case GDK_LINE_DOUBLE_DASH: /* ??? */
	  /* Only set the linestyle here, if it isn't already set
	   * gdk_win32_gc_set_dashes () knows better
	   */
	  if (0 == (win32_gc->values_mask & GDK_GC_LINE_STYLE))
	    {
            win32_gc->pen_style &= ~(PS_STYLE_MASK);
	      GDK_NOTE (GC, (g_print ("%sps|=DASH", s),
			     s = ","));
	      win32_gc->pen_style |= PS_DASH;
	    }
	  break;
	}
      win32_gc->values_mask |= GDK_GC_LINE_STYLE;
    }

  if (mask & GDK_GC_CAP_STYLE)
    {
      win32_gc->pen_style &= ~(PS_ENDCAP_MASK);
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:	/* ??? */
	case GDK_CAP_BUTT:
	  GDK_NOTE (GC, (g_print ("%sps|=ENDCAP_FLAT", s),
			 s = ","));
	  win32_gc->pen_style |= PS_ENDCAP_FLAT;
	  break;
	case GDK_CAP_ROUND:
	  GDK_NOTE (GC, (g_print ("%sps|=ENDCAP_ROUND", s),
			 s = ","));
	  win32_gc->pen_style |= PS_ENDCAP_ROUND;
	  break;
	case GDK_CAP_PROJECTING:
	  GDK_NOTE (GC, (g_print ("%sps|=ENDCAP_SQUARE", s),
			 s = ","));
	  win32_gc->pen_style |= PS_ENDCAP_SQUARE;
	  break;
	}
      win32_gc->values_mask |= GDK_GC_CAP_STYLE;
    }

  if (mask & GDK_GC_JOIN_STYLE)
    {
      win32_gc->pen_style &= ~(PS_JOIN_MASK);
      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  GDK_NOTE (GC, (g_print ("%sps|=JOIN_MITER", s),
			 s = ","));
	  win32_gc->pen_style |= PS_JOIN_MITER;
	  break;
	case GDK_JOIN_ROUND:
	  GDK_NOTE (GC, (g_print ("%sps|=JOIN_ROUND", s),
			 s = ","));
	  win32_gc->pen_style |= PS_JOIN_ROUND;
	  break;
	case GDK_JOIN_BEVEL:
	  GDK_NOTE (GC, (g_print ("%sps|=JOIN_BEVEL", s),
			 s = ","));
	  win32_gc->pen_style |= PS_JOIN_BEVEL;
	  break;
	}
      win32_gc->values_mask |= GDK_GC_JOIN_STYLE;
    }
  GDK_NOTE (GC, g_print ("}\n"));
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

  gc = g_object_new (_gdk_gc_win32_get_type (), NULL);
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

  GDK_NOTE (GC, g_print ("_gdk_win32_gc_new: "));
  gdk_win32_gc_values_to_win32values (values, mask, win32_gc);

  win32_gc->hwnd = NULL;

  GDK_NOTE (GC, g_print (" = %p\n", gc));

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

  GDK_NOTE (GC, g_print ("gdk_win32_gc_set_values: "));

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

  /* mark as set, see gdk_win32_gc_values_to_win32values () for the reason */
  win32_gc->values_mask |= GDK_GC_LINE_STYLE;

  win32_gc->pen_style &= ~(PS_STYLE_MASK);

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
      GDK_NOTE (GC, g_print ("gdk_win32_gc_set_dashes: not fully supported\n"));
      win32_gc->pen_style |= PS_SOLID;
      return;
    }
  
  win32_gc->pen_style = PS_COSMETIC; /* ??? */
  switch (n)
    {
    case 2:
      if ((dash_list[0] == dash_list[1]) && (dash_list[0] > 2))
        {
          win32_gc->pen_style |= PS_DASH;
          GDK_NOTE (GC, g_print ("gdk_win32_gc_set_dashes: PS_DASH (%d,%d)\n", 
				 dash_list[0], dash_list[1]));
        }
      else
        {
          win32_gc->pen_style |= PS_DOT;
          GDK_NOTE (GC, g_print ("gdk_win32_gc_set_dashes: PS_DOT (%d,%d)\n", 
				 dash_list[0], dash_list[1]));
        }
      break;

    case 4:
      win32_gc->pen_style |= PS_DASHDOT; 
      GDK_NOTE (GC, g_print ("gdk_win32_gc_set_dashes: PS_DASHDOT (%d,%d,%d,%d)\n", 
			     dash_list[0], dash_list[1],
			     dash_list[2], dash_list[3]));
      break;

    case 6:
      win32_gc->pen_style |= PS_DASHDOTDOT; 
      GDK_NOTE (GC, g_print ("gdk_win32_gc_set_dashes: PS_DASHDOTDOT (%d,%d,%d,%d,%d,%d)\n", 
			     dash_list[0], dash_list[1],
			     dash_list[2], dash_list[3],
			     dash_list[4], dash_list[5]));
      break;

    default:
      win32_gc->pen_style |= PS_DASH;
      GDK_NOTE (GC, g_print ("gdk_win32_gc_set_dashes: no guess for %d dashes\n", n));
      break;
    }
}

void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkGCWin32 *win32_gc;

  g_return_if_fail (GDK_IS_GC (gc));

  win32_gc = GDK_GC_WIN32 (gc);

  if (win32_gc->hcliprgn)
    {
      DeleteObject (win32_gc->hcliprgn);
      win32_gc->hcliprgn = NULL;
    }

  if (win32_gc->clip_region)
    gdk_region_destroy (win32_gc->clip_region);

  if (rectangle)
    {
      GDK_NOTE (GC,
		g_print ("gdk_gc_set_clip_rectangle: %dx%d@+%d+%d\n",
			 rectangle->width, rectangle->height,
			 rectangle->x, rectangle->y));
      win32_gc->clip_region = gdk_region_rectangle (rectangle);
      win32_gc->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (GC, g_print ("gdk_gc_set_clip_rectangle: NULL\n"));

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
      GDK_NOTE (GC, g_print ("gdk_gc_set_clip_region: %dx%d+%d+%d\n",
			       region->extents.x2 - region->extents.x1,
			       region->extents.y2 - region->extents.y1,
			       region->extents.x1, region->extents.y1));

      win32_gc->clip_region = gdk_region_copy (region);
      win32_gc->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (GC, g_print ("gdk_gc_set_clip_region: NULL\n"));

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
  if (dst_win32_gc->hcliprgn != NULL)
    DeleteObject (dst_win32_gc->hcliprgn);
  
  *dst_win32_gc = *src_win32_gc;

  if (dst_win32_gc->hcliprgn)
    {
      /* create a new region, to copy to */
      dst_win32_gc->hcliprgn = CreateRectRgn (0,0,1,1);
      /* overwrite from source */
      CombineRgn (dst_win32_gc->hcliprgn, src_win32_gc->hcliprgn,
		  NULL, RGN_COPY);
    }
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
_gdk_win32_colormap_color (GdkColormap *colormap,
                           gulong       pixel)
{
  const GdkVisual *visual;
  GdkColormapPrivateWin32 *colormap_private;
  guchar r, g, b;

  if (colormap == NULL)
    return DIBINDEX (pixel & 1);

  colormap_private = GDK_WIN32_COLORMAP_DATA (colormap);

  g_assert (colormap_private != NULL);

  visual = colormap->visual;
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

static void
predraw_set_foreground (GdkGC       *gc,
			GdkColormap *colormap,
			gboolean    *ok)
{
  COLORREF fg;
  LOGBRUSH logbrush;
  HPEN hpen;
  HBRUSH hbr;
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;
  GdkColormapPrivateWin32 *colormap_private;
  gint k;

  if (colormap &&
      (colormap->visual->type == GDK_VISUAL_PSEUDO_COLOR ||
       colormap->visual->type == GDK_VISUAL_STATIC_COLOR))
    {
      colormap_private = GDK_WIN32_COLORMAP_DATA (colormap);

      g_assert (colormap_private != NULL);

      if (!(win32_gc->holdpal = SelectPalette (win32_gc->hdc, colormap_private->hpal, FALSE)))
	WIN32_GDI_FAILED ("SelectPalette"), *ok = FALSE;
      else if ((k = RealizePalette (win32_gc->hdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette"), *ok = FALSE;
      else if (k > 0)
	GDK_NOTE (COLORMAP, g_print ("predraw_set_foreground: realized %p: %d colors\n",
				     colormap_private->hpal, k));
    }

  fg = _gdk_win32_colormap_color (colormap, win32_gc->foreground);

  GDK_NOTE (GC, g_print ("predraw_set_foreground: fg=%06lx\n", fg));

  if (SetTextColor (win32_gc->hdc, fg) == CLR_INVALID)
    WIN32_GDI_FAILED ("SetTextColor"), *ok = FALSE;

  /* Create and select pen and brush. */

  logbrush.lbStyle = BS_SOLID;
  logbrush.lbColor = fg;
  logbrush.lbHatch = 0;

  if (*ok && (hpen = ExtCreatePen (win32_gc->pen_style,
				   (win32_gc->pen_width > 0 ? win32_gc->pen_width : 1),
				   &logbrush, 0, NULL)) == NULL)
    WIN32_GDI_FAILED ("ExtCreatePen"), *ok = FALSE;
  
  if (*ok && SelectObject (win32_gc->hdc, hpen) == NULL)
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
  if (*ok)
    {
      HBRUSH old_hbr = SelectObject (win32_gc->hdc, hbr);
      if (old_hbr == NULL)
	WIN32_GDI_FAILED ("SelectObject"), *ok = FALSE;
    }
}  

static void
predraw_set_background (GdkGC       *gc,
			GdkColormap *colormap,
			gboolean    *ok)
{
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;

  if (win32_gc->values_mask & GDK_GC_BACKGROUND)
    {
      COLORREF bg = _gdk_win32_colormap_color (colormap, win32_gc->background);

      if (SetBkColor (win32_gc->hdc, bg) == CLR_INVALID)
        WIN32_GDI_FAILED ("SetBkColor"), *ok = FALSE;
    }
  else
    {
      if (!SetBkMode (win32_gc->hdc, TRANSPARENT))
        WIN32_GDI_FAILED ("SetBkMode"), *ok = FALSE;
    }
}

HDC
gdk_win32_hdc_get (GdkDrawable    *drawable,
		   GdkGC          *gc,
		   GdkGCValuesMask usage)
{
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;
  GdkDrawableImplWin32 *impl = NULL;
  gboolean ok = TRUE;
  int flag;

  g_assert (win32_gc->hdc == NULL);

  if (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable))
    impl = GDK_DRAWABLE_IMPL_WIN32(drawable);
  else if (GDK_IS_WINDOW (drawable))
    impl = GDK_DRAWABLE_IMPL_WIN32 ((GDK_WINDOW_OBJECT (drawable))->impl);
  else if (GDK_IS_PIXMAP (drawable))
    impl = GDK_DRAWABLE_IMPL_WIN32 ((GDK_PIXMAP_OBJECT (drawable))->impl);
  else
    g_assert_not_reached ();

  win32_gc->hwnd = impl->handle;

  if (GDK_IS_PIXMAP_IMPL_WIN32 (impl))
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
    predraw_set_foreground (gc, impl->colormap, &ok);

  if (ok && (usage & GDK_GC_BACKGROUND))
    predraw_set_background (gc, impl->colormap, &ok);
  
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

	  GDK_NOTE (GC, g_print ("clip rgn box %d: %ldx%ld@+%ld+%ld\n",
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

      /*
       * XXX: combine the fillmode-stipple with the clip region possibly 
       *  this needs to be done with the hcliprgn below as well, but 
       *  the test case here were check boxes and radio buttons ...
       */
      if (   (win32_gc->values_mask & GDK_GC_STIPPLE)
          && (win32_gc->values_mask & GDK_GC_FILL) && (win32_gc->fill_style == GDK_STIPPLED))
        {
          HRGN hstipplergn = _gdk_win32_bitmap_to_region (win32_gc->stipple);

          if (OffsetRgn (hstipplergn, 
             win32_gc->values_mask & GDK_GC_TS_X_ORIGIN ? gc->ts_x_origin : 0,
             win32_gc->values_mask & GDK_GC_TS_Y_ORIGIN ? gc->ts_y_origin : 0) == ERROR)
            WIN32_API_FAILED ("OffsetRgn"), ok = FALSE;
          if (CombineRgn (hrgn, hrgn, hstipplergn, RGN_AND) == ERROR)
            WIN32_API_FAILED ("CombineRgn"), ok = FALSE;
          if (!DeleteObject (hstipplergn))
            WIN32_API_FAILED ("DeleteObject");
        }
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
      if (ok && !OffsetClipRgn (win32_gc->hdc,
		 win32_gc->values_mask & GDK_GC_CLIP_X_ORIGIN ? gc->clip_x_origin : 0,
		 win32_gc->values_mask & GDK_GC_CLIP_Y_ORIGIN ? gc->clip_y_origin : 0))
	WIN32_API_FAILED ("OffsetClipRgn"), ok = FALSE;
    }
#ifdef G_ENABLE_DEBUG
  if (_gdk_debug_flags & GDK_DEBUG_GC)
    {
      HGDIOBJ obj;
      LOGBRUSH logbrush;
      EXTLOGPEN extlogpen;
      HRGN hrgn;
      RECT rect;

      g_print ("gdk_win32_hdc_get: %p\n", win32_gc->hdc);
      obj = GetCurrentObject (win32_gc->hdc, OBJ_BRUSH);
      GetObject (obj, sizeof (LOGBRUSH), &logbrush);
      g_print ("brush: style: %s color: %06lx hatch: %#lx\n",
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
		((extlogpen.elpPenStyle & PS_STYLE_MASK) == PS_SOLID ? "SOLID" :
		 ((extlogpen.elpPenStyle & PS_STYLE_MASK) == PS_DOT ? "DOT" :
		  ((extlogpen.elpPenStyle & PS_STYLE_MASK) == PS_DASH ? "DASH" :
 		   "???")))),
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
	  g_print ("clip region bbox: %ldx%ld@+%ld+%ld\n",
		   rect.right - rect.left,
		   rect.bottom - rect.top,
		   rect.left, rect.top);
	}
      DeleteObject (hrgn);
    }
#endif
  return win32_gc->hdc;
}

void
gdk_win32_hdc_release (GdkDrawable    *drawable,
		       GdkGC          *gc,
		       GdkGCValuesMask usage)
{
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;
  GdkDrawableImplWin32 *impl = NULL;
  HGDIOBJ hpen = NULL;
  HGDIOBJ hbr = NULL;

  GDK_NOTE (GC, g_print ("gdk_win32_hdc_release: %p\n", win32_gc->hdc));

  if (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable))
    impl = GDK_DRAWABLE_IMPL_WIN32(drawable);
  else if (GDK_IS_WINDOW (drawable))
    impl = GDK_DRAWABLE_IMPL_WIN32 ((GDK_WINDOW_OBJECT (drawable))->impl);
  else if (GDK_IS_PIXMAP (drawable))
    impl = GDK_DRAWABLE_IMPL_WIN32 ((GDK_PIXMAP_OBJECT (drawable))->impl);
  else
    g_assert_not_reached ();

  if (win32_gc->holdpal != NULL)
    {
      gint k;
      
      if (!SelectPalette (win32_gc->hdc, win32_gc->holdpal, FALSE))
	WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (win32_gc->hdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else if (k > 0)
	GDK_NOTE (COLORMAP, g_print ("gdk_win32_hdc_release: realized %p: %d colors\n",
				     win32_gc->holdpal, k));
      win32_gc->holdpal = NULL;
    }

  if (usage & GDK_GC_FOREGROUND)
    {
      if ((hpen = GetCurrentObject (win32_gc->hdc, OBJ_PEN)) == NULL)
	WIN32_GDI_FAILED ("GetCurrentObject");

      if ((hbr = GetCurrentObject (win32_gc->hdc, OBJ_BRUSH)) == NULL)
	WIN32_GDI_FAILED ("GetCurrentObject");
    }

  if (!RestoreDC (win32_gc->hdc, win32_gc->saved_dc))
    WIN32_GDI_FAILED ("RestoreDC");

  if (GDK_IS_PIXMAP_IMPL_WIN32 (impl))
    {
      if (!DeleteDC (win32_gc->hdc))
	WIN32_GDI_FAILED ("DeleteDC");
    }
  else
    {
      if (!ReleaseDC (win32_gc->hwnd, win32_gc->hdc))
	WIN32_GDI_FAILED ("ReleaseDC");
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
 * available at www.codeguru.com. Simplified for our needs, not sure
 * how much of the original code left any longer. Now handles just
 * one-bit deep bitmaps (in Window parlance, ie those that GDK calls
 * bitmaps (and not pixmaps), with zero pixels being transparent.
 */

/* _gdk_win32_bitmap_to_region : Create a region from the
 * "non-transparent" pixels of a bitmap.
 */

HRGN
_gdk_win32_bitmap_to_region (GdkPixmap *pixmap)
{
  HRGN hRgn = NULL;
  HRGN h;
  DWORD maxRects;
  RGNDATA *pData;
  GdkImage *image;
  guchar *p;
  gint x, y;

  image = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl)->image;
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
