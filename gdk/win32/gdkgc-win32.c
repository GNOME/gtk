/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2004 Tor Lillqvist
 * Copyright (C) 2000-2004 Hans Breuer
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

#define LINE_ATTRIBUTES (GDK_GC_LINE_WIDTH|GDK_GC_LINE_STYLE| \
			 GDK_GC_CAP_STYLE|GDK_GC_JOIN_STYLE)

#include <config.h>
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
  
  if (win32_gc->hcliprgn != NULL)
    DeleteObject (win32_gc->hcliprgn);
  
  if (win32_gc->values_mask & GDK_GC_FONT)
    gdk_font_unref (win32_gc->font);
  
  if (win32_gc->values_mask & GDK_GC_TILE)
    g_object_unref (win32_gc->tile);
  
  if (win32_gc->values_mask & GDK_GC_STIPPLE)
    g_object_unref (win32_gc->stipple);

  if (win32_gc->pen_dashes)
    g_free (win32_gc->pen_dashes);
  
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
	  gdk_font_ref (win32_gc->font);
	  win32_gc->values_mask |= GDK_GC_FONT;
	  GDK_NOTE (GC, (g_print ("%sfont=%p", s, win32_gc->font),
			 s = ","));
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
      GDK_NOTE (GC, (g_print ("%sfill=%s", s,
			      _gdk_win32_fill_style_to_string (win32_gc->fill_style)),
		     s = ","));
    }

  if (mask & GDK_GC_TILE)
    {
      if (win32_gc->tile != NULL)
	g_object_unref (win32_gc->tile);
      win32_gc->tile = values->tile;
      if (win32_gc->tile != NULL)
	{
	  g_object_ref (win32_gc->tile);
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
	g_object_unref (win32_gc->stipple);
      win32_gc->stipple = values->stipple;
      if (win32_gc->stipple != NULL)
	{
	  gdk_drawable_get_size (win32_gc->stipple, &sw, &sh);

#if 0 /* HB: this size limitation is disabled to make radio and check
       * buttons work. I got the impression from the API docs, that
       * it shouldn't be necessary at all, but win9x would do the clipping
       */
	  if (   (sw != 8 || sh != 8)
	      && !G_WIN32_IS_NT_BASED ()) /* HB: the MSDN says it's a Win95 limitation */
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
	    g_object_ref (win32_gc->stipple);
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
      if (win32_gc->hcliprgn != NULL)
	DeleteObject (win32_gc->hcliprgn);

      if (values->clip_mask != NULL)
	{
	  win32_gc->hcliprgn = _gdk_win32_bitmap_to_hrgn (values->clip_mask);
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
	  if (win32_gc->pen_dashes)
	    {
	      g_free (win32_gc->pen_dashes);
	      win32_gc->pen_dashes = NULL;
	      win32_gc->pen_num_dashes = 0;
	    }
          win32_gc->pen_style &= ~(PS_STYLE_MASK);
	  win32_gc->pen_style |= PS_SOLID;
	  win32_gc->pen_double_dash = FALSE;
	  break;
	case GDK_LINE_ON_OFF_DASH:
	case GDK_LINE_DOUBLE_DASH: 
	  if (!win32_gc->pen_dashes)
	    {
            /* setting to PS_DASH probably isn't correct. If I understand the
             * xlib docs correctly it should influence the handling of
             * line endings ? --hb
             */
            win32_gc->pen_style &= ~(PS_STYLE_MASK);
	    win32_gc->pen_style |= PS_DASH;
          }
	  win32_gc->pen_double_dash = values->line_style == GDK_LINE_DOUBLE_DASH;
	  break;
	}
      GDK_NOTE (GC, (g_print ("%sps|=PS_STYLE_%s", s, _gdk_win32_psstyle_to_string (win32_gc->pen_style)),
		     s = ","));
      win32_gc->values_mask |= GDK_GC_LINE_STYLE;
    }

  if (mask & GDK_GC_CAP_STYLE)
    {
      win32_gc->pen_style &= ~(PS_ENDCAP_MASK);
      switch (values->cap_style)
	{
	case GDK_CAP_NOT_LAST:	/* ??? */
	case GDK_CAP_BUTT:
	  win32_gc->pen_style |= PS_ENDCAP_FLAT;
	  break;
	case GDK_CAP_ROUND:
	  win32_gc->pen_style |= PS_ENDCAP_ROUND;
	  break;
	case GDK_CAP_PROJECTING:
	  win32_gc->pen_style |= PS_ENDCAP_SQUARE;
	  break;
	}
      GDK_NOTE (GC, (g_print ("%sps|=PS_ENDCAP_%s", s, _gdk_win32_psendcap_to_string (win32_gc->pen_style)),
		     s = ","));
      win32_gc->values_mask |= GDK_GC_CAP_STYLE;
    }

  if (mask & GDK_GC_JOIN_STYLE)
    {
      win32_gc->pen_style &= ~(PS_JOIN_MASK);
      switch (values->join_style)
	{
	case GDK_JOIN_MITER:
	  win32_gc->pen_style |= PS_JOIN_MITER;
	  break;
	case GDK_JOIN_ROUND:
	  win32_gc->pen_style |= PS_JOIN_ROUND;
	  break;
	case GDK_JOIN_BEVEL:
	  win32_gc->pen_style |= PS_JOIN_BEVEL;
	  break;
	}
      GDK_NOTE (GC, (g_print ("%sps|=PS_JOIN_%s", s, _gdk_win32_psjoin_to_string (win32_gc->pen_style)),
		     s = ","));
      win32_gc->values_mask |= GDK_GC_JOIN_STYLE;
    }
  GDK_NOTE (GC, g_print ("} mask=(%s)", _gdk_win32_gcvalues_mask_to_string (win32_gc->values_mask)));
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
  win32_gc->subwindow_mode = GDK_CLIP_BY_CHILDREN;
  win32_gc->graphics_exposures = TRUE;
  win32_gc->pen_width = 0;
  win32_gc->pen_style = PS_GEOMETRIC|PS_ENDCAP_FLAT|PS_JOIN_MITER;
  win32_gc->pen_dashes = NULL;
  win32_gc->pen_num_dashes = 0;
  win32_gc->pen_dash_offset = 0;
  win32_gc->pen_double_dash = FALSE;
  win32_gc->pen_hbrbg = NULL;

  win32_gc->values_mask = GDK_GC_FUNCTION | GDK_GC_FILL;

  GDK_NOTE (GC, g_print ("_gdk_win32_gc_new: %p: ", win32_gc));
  gdk_win32_gc_values_to_win32values (values, mask, win32_gc);
  GDK_NOTE (GC, g_print ("\n"));

  win32_gc->hdc = NULL;
  win32_gc->hwnd = NULL;

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
    values->line_style = win32_gc->pen_double_dash ? GDK_LINE_DOUBLE_DASH :
						     GDK_LINE_ON_OFF_DASH;
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

  GDK_NOTE (GC, g_print ("gdk_win32_gc_set_values: %p: ", GDK_GC_WIN32 (gc)));
  gdk_win32_gc_values_to_win32values (values, mask, GDK_GC_WIN32 (gc));
  GDK_NOTE (GC, g_print ("\n"));
}

static void
gdk_win32_gc_set_dashes (GdkGC *gc,
			 gint	dash_offset,
			 gint8  dash_list[],
			 gint   n)
{
  GdkGCWin32 *win32_gc;
  int i;

  g_return_if_fail (GDK_IS_GC (gc));
  g_return_if_fail (dash_list != NULL);

  win32_gc = GDK_GC_WIN32 (gc);

  /* mark as set, see gdk_win32_gc_values_to_win32values () for the reason */
  win32_gc->values_mask |= GDK_GC_LINE_STYLE;

  win32_gc->pen_style &= ~(PS_STYLE_MASK);

  win32_gc->pen_style |= (PS_GEOMETRIC | PS_USERSTYLE);
  win32_gc->pen_num_dashes = n;
  if (win32_gc->pen_dashes != NULL)
    g_free (win32_gc->pen_dashes);
  win32_gc->pen_dashes = g_new (DWORD, n);
  for (i = 0; i < n; i++)
    win32_gc->pen_dashes[i] = dash_list[i];
  win32_gc->pen_dash_offset = dash_offset;
}

void
gdk_gc_set_clip_rectangle (GdkGC	*gc,
			   GdkRectangle *rectangle)
{
  GdkGCWin32 *win32_gc;

  g_return_if_fail (GDK_IS_GC (gc));

  win32_gc = GDK_GC_WIN32 (gc);

  if (win32_gc->hcliprgn)
    DeleteObject (win32_gc->hcliprgn);

  if (rectangle)
    {
      GDK_NOTE (GC, g_print ("gdk_gc_set_clip_rectangle: %p: %s\n",
			     win32_gc,
			     _gdk_win32_gdkrectangle_to_string (rectangle)));
      win32_gc->hcliprgn = CreateRectRgn (rectangle->x, rectangle->y,
					  rectangle->x + rectangle->width,
					  rectangle->y + rectangle->height);
      win32_gc->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (GC, g_print ("gdk_gc_set_clip_rectangle: NULL\n"));

      win32_gc->hcliprgn = NULL;
      win32_gc->values_mask &= ~GDK_GC_CLIP_MASK;
    }

  gc->clip_x_origin = 0;
  gc->clip_y_origin = 0;
  
  win32_gc->values_mask &= ~(GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN);
} 

void
gdk_gc_set_clip_region (GdkGC	  *gc,
			GdkRegion *region)
{
  GdkGCWin32 *win32_gc;

  g_return_if_fail (GDK_IS_GC (gc));

  win32_gc = GDK_GC_WIN32 (gc);

  if (win32_gc->hcliprgn)
    DeleteObject (win32_gc->hcliprgn);

  if (region)
    {
      GDK_NOTE (GC, g_print ("gdk_gc_set_clip_region: %p: %s\n",
			     win32_gc,
			     _gdk_win32_gdkregion_to_string (region)));

      win32_gc->hcliprgn = _gdk_win32_gdkregion_to_hrgn (region, 0, 0);
      win32_gc->values_mask |= GDK_GC_CLIP_MASK;
    }
  else
    {
      GDK_NOTE (GC, g_print ("gdk_gc_set_clip_region: NULL\n"));

      win32_gc->hcliprgn = NULL;
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

  GDK_NOTE (GC, g_print ("gdk_gc_copy: %p := %p\n", dst_win32_gc, src_win32_gc));

  if (dst_gc->colormap)
    g_object_unref (dst_gc->colormap);

  if (dst_win32_gc->hcliprgn != NULL)
    DeleteObject (dst_win32_gc->hcliprgn);

  if (dst_win32_gc->font != NULL)
    gdk_font_unref (dst_win32_gc->font);

  if (dst_win32_gc->tile != NULL)
    g_object_unref (dst_win32_gc->tile);

  if (dst_win32_gc->stipple != NULL)
    g_object_unref (dst_win32_gc->stipple);

  if (dst_win32_gc->pen_dashes)
    g_free (dst_win32_gc->pen_dashes);
  
  dst_gc->clip_x_origin = src_gc->clip_x_origin;
  dst_gc->clip_y_origin = src_gc->clip_y_origin;
  dst_gc->ts_x_origin = src_gc->ts_x_origin;
  dst_gc->ts_y_origin = src_gc->ts_y_origin;
  dst_gc->colormap = src_gc->colormap;
  if (dst_gc->colormap)
    g_object_ref (dst_gc->colormap);

  dst_win32_gc->hcliprgn = src_win32_gc->hcliprgn;
  if (dst_win32_gc->hcliprgn)
    {
      /* create a new region, to copy to */
      dst_win32_gc->hcliprgn = CreateRectRgn (0,0,1,1);
      /* overwrite from source */
      CombineRgn (dst_win32_gc->hcliprgn, src_win32_gc->hcliprgn,
		  NULL, RGN_COPY);
    }

  dst_win32_gc->values_mask = src_win32_gc->values_mask; 
  dst_win32_gc->foreground = src_win32_gc->foreground;
  dst_win32_gc->background = src_win32_gc->background;
  dst_win32_gc->font = src_win32_gc->font;
  if (dst_win32_gc->font != NULL)
    gdk_font_ref (dst_win32_gc->font);

  dst_win32_gc->rop2 = src_win32_gc->rop2;
  dst_win32_gc->fill_style = src_win32_gc->fill_style;
  dst_win32_gc->tile = src_win32_gc->tile;
  if (dst_win32_gc->tile != NULL)
    g_object_ref (dst_win32_gc->tile);

  dst_win32_gc->stipple = src_win32_gc->stipple;
  if (dst_win32_gc->stipple != NULL)
    g_object_ref (dst_win32_gc->stipple);

  dst_win32_gc->subwindow_mode = src_win32_gc->subwindow_mode;
  dst_win32_gc->graphics_exposures = src_win32_gc->graphics_exposures;
  dst_win32_gc->pen_width = src_win32_gc->pen_width;
  dst_win32_gc->pen_style = src_win32_gc->pen_style;
  dst_win32_gc->pen_dashes = src_win32_gc->pen_dashes;
  if (dst_win32_gc->pen_dashes)
    dst_win32_gc->pen_dashes = g_memdup (src_win32_gc->pen_dashes, 
                                         sizeof (DWORD) * src_win32_gc->pen_num_dashes);
  dst_win32_gc->pen_num_dashes = src_win32_gc->pen_num_dashes;
  dst_win32_gc->pen_dash_offset = src_win32_gc->pen_dash_offset;
  dst_win32_gc->pen_double_dash = src_win32_gc->pen_double_dash;


  dst_win32_gc->hdc = NULL;
  dst_win32_gc->saved_dc = FALSE;
  dst_win32_gc->hwnd = NULL;
  dst_win32_gc->holdpal = NULL;
  dst_win32_gc->pen_hbrbg = NULL;
}

GdkScreen *  
gdk_gc_get_screen (GdkGC *gc)
{
  g_return_val_if_fail (GDK_IS_GC_WIN32 (gc), NULL);
  
  return _gdk_screen;
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

gboolean
predraw (GdkGC       *gc,
	 GdkColormap *colormap)
{
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;
  GdkColormapPrivateWin32 *colormap_private;
  gint k;
  gboolean ok = TRUE;

  if (colormap &&
      (colormap->visual->type == GDK_VISUAL_PSEUDO_COLOR ||
       colormap->visual->type == GDK_VISUAL_STATIC_COLOR))
    {
      colormap_private = GDK_WIN32_COLORMAP_DATA (colormap);

      g_assert (colormap_private != NULL);

      if (!(win32_gc->holdpal = SelectPalette (win32_gc->hdc, colormap_private->hpal, FALSE)))
	WIN32_GDI_FAILED ("SelectPalette"), ok = FALSE;
      else if ((k = RealizePalette (win32_gc->hdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette"), ok = FALSE;
      else if (k > 0)
	GDK_NOTE (COLORMAP, g_print ("predraw: realized %p: %d colors\n",
				     colormap_private->hpal, k));
    }

  return ok;
}

/**
 * gdk_win32_hdc_get:
 * @drawable: destination #GdkDrawable
 * @gc: #GdkGC to use for drawing on @drawable
 * @usage: mask indicating what properties needs to be set up
 *
 * Allocates a Windows device context handle (HDC) for drawing into
 * @drawable, and sets it up appropriately according to @usage.
 *
 * Each #GdkGC can at one time have only one HDC associated with it.
 *
 * The following flags in @mask are handled:
 *
 * If %GDK_GC_FOREGROUND is set in @mask, a solid brush of the
 * foreground color in @gc is selected into the HDC. The text color of
 * the HDC is also set. If the @drawable has a palette (256-color
 * mode), the palette is selected and realized.
 *
 * If any of the line attribute flags (%GDK_GC_LINE_WIDTH,
 * %GDK_GC_LINE_STYLE, %GDK_GC_CAP_STYLE and %GDK_GC_JOIN_STYLE) is
 * set in @mask, a solid pen of the foreground color and appropriate
 * width and stule is created and selected into the HDC. Note that the
 * dash properties are not completely implemented.
 *
 * If the %GDK_GC_FONT flag is set, the background mix mode is set to
 * %TRANSPARENT. and the text alignment is set to
 * %TA_BASELINE|%TA_LEFT. Note that no font gets selected into the HDC
 * by this function.
 *
 * Some things are done regardless of @mask: If the function in @gc is
 * any other than %GDK_COPY, the raster operation of the HDC is
 * set. If @gc has a clip mask, the clip region of the HDC is set.
 *
 * Note that the fill style, tile, stipple, and tile and stipple
 * origins in the @gc are ignored by this function. (In general, tiles
 * and stipples can't be implemented directly on Win32; you need to do
 * multiple pass drawing and blitting to implement tiles or
 * stipples. GDK does just that when you call the GDK drawing
 * functions with a GC that asks for tiles or stipples.)
 *
 * When the HDC is no longer used, it should be released by calling
 * <function>gdk_win32_hdc_release()</function> with the same
 * parameters.
 *
 * If you modify the HDC by calling <function>SelectObject</function>
 * you should undo those modifications before calling
 * <function>gdk_win32_hdc_release()</function>.
 *
 * Return value: The HDC.
 **/
HDC
gdk_win32_hdc_get (GdkDrawable    *drawable,
		   GdkGC          *gc,
		   GdkGCValuesMask usage)
{
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;
  GdkDrawableImplWin32 *impl = NULL;
  gboolean ok = TRUE;
  COLORREF fg = RGB (0, 0, 0), bg = RGB (255, 255, 255);
  LOGBRUSH logbrush;
  HPEN hpen;
  HBRUSH hbr;

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

  if (ok && (usage & (GDK_GC_FOREGROUND | GDK_GC_BACKGROUND)))
      ok = predraw (gc, impl->colormap);

  if (ok && (usage & GDK_GC_FOREGROUND))
    {
      fg = _gdk_win32_colormap_color (impl->colormap, win32_gc->foreground);
      if ((hbr = CreateSolidBrush (fg)) == NULL)
	WIN32_GDI_FAILED ("CreateSolidBrush"), ok = FALSE;

      if (ok && SelectObject (win32_gc->hdc, hbr) == NULL)
	WIN32_GDI_FAILED ("SelectObject"), ok = FALSE;

      if (ok && SetTextColor (win32_gc->hdc, fg) == CLR_INVALID)
	WIN32_GDI_FAILED ("SetTextColor"), ok = FALSE;
    }

  if (ok && (usage & LINE_ATTRIBUTES))
    {
      /* For drawing GDK_LINE_DOUBLE_DASH */
      if ((usage & GDK_GC_BACKGROUND) && win32_gc->pen_double_dash)
        {
          bg = _gdk_win32_colormap_color (impl->colormap, win32_gc->background);
          if ((win32_gc->pen_hbrbg = CreateSolidBrush (bg)) == NULL)
	    WIN32_GDI_FAILED ("CreateSolidBrush"), ok = FALSE;
        }

      if (ok)
        {
	  /* Create and select pen */
	  logbrush.lbStyle = BS_SOLID;
	  logbrush.lbColor = fg;
	  logbrush.lbHatch = 0;
	  
	  if (win32_gc->pen_num_dashes > 0 && !G_WIN32_IS_NT_BASED ())
	    {
	      /* The Win9x GDI is rather limited so we either draw dashed
	       * lines ourselves (only horizontal and vertical) or let them be
	       * drawn solid to avoid implementing a whole line renderer.
	       */
	      if ((hpen = ExtCreatePen (
					(win32_gc->pen_style & ~(PS_STYLE_MASK)) | PS_SOLID,
					MAX (win32_gc->pen_width, 1),
					&logbrush, 
					0, NULL)) == NULL)
		WIN32_GDI_FAILED ("ExtCreatePen"), ok = FALSE;
	    }
	  else
	    {
	      if ((hpen = ExtCreatePen (win32_gc->pen_style,
					MAX (win32_gc->pen_width, 1),
					&logbrush, 
					win32_gc->pen_num_dashes,
					win32_gc->pen_dashes)) == NULL)
		WIN32_GDI_FAILED ("ExtCreatePen"), ok = FALSE;
	    }
	  
	  if (ok && SelectObject (win32_gc->hdc, hpen) == NULL)
	    WIN32_GDI_FAILED ("SelectObject"), ok = FALSE;
	}
    }

  if (ok && (usage & GDK_GC_FONT))
    {
      if (SetBkMode (win32_gc->hdc, TRANSPARENT) == 0)
	WIN32_GDI_FAILED ("SetBkMode"), ok = FALSE;
  
      if (ok && SetTextAlign (win32_gc->hdc, TA_BASELINE|TA_LEFT|TA_NOUPDATECP) == GDI_ERROR)
	WIN32_GDI_FAILED ("SetTextAlign"), ok = FALSE;
    }
  
  if (ok && win32_gc->rop2 != R2_COPYPEN)
    if (SetROP2 (win32_gc->hdc, win32_gc->rop2) == 0)
      WIN32_GDI_FAILED ("SetROP2"), ok = FALSE;

  if (ok &&
      (win32_gc->values_mask & GDK_GC_CLIP_MASK) &&
      win32_gc->hcliprgn != NULL)
    {
      if (SelectClipRgn (win32_gc->hdc, win32_gc->hcliprgn) == ERROR)
	WIN32_API_FAILED ("SelectClipRgn"), ok = FALSE;

      if (ok && win32_gc->values_mask & (GDK_GC_CLIP_X_ORIGIN | GDK_GC_CLIP_Y_ORIGIN) &&
	  OffsetClipRgn (win32_gc->hdc,
	    win32_gc->values_mask & GDK_GC_CLIP_X_ORIGIN ? gc->clip_x_origin : 0,
	    win32_gc->values_mask & GDK_GC_CLIP_Y_ORIGIN ? gc->clip_y_origin : 0) == ERROR)
	WIN32_API_FAILED ("OffsetClipRgn"), ok = FALSE;
    }

  GDK_NOTE (GC, (g_print ("gdk_win32_hdc_get: %p (%s): ",
			  win32_gc, _gdk_win32_gcvalues_mask_to_string (usage)),
		 _gdk_win32_print_dc (win32_gc->hdc)));

  return win32_gc->hdc;
}

/**
 * gdk_win32_hdc_release:
 * @drawable: destination #GdkDrawable
 * @gc: #GdkGC to use for drawing on @drawable
 * @usage: mask indicating what properties were set up
 *
 * This function deallocates the Windows device context allocated by
 * <funcion>gdk_win32_hdc_get()</function>. It should be called with
 * the same parameters.
 **/
void
gdk_win32_hdc_release (GdkDrawable    *drawable,
		       GdkGC          *gc,
		       GdkGCValuesMask usage)
{
  GdkGCWin32 *win32_gc = (GdkGCWin32 *) gc;
  GdkDrawableImplWin32 *impl = NULL;
  HGDIOBJ hpen = NULL;
  HGDIOBJ hbr = NULL;

  GDK_NOTE (GC, g_print ("gdk_win32_hdc_release: %p: %p (%s)\n",
			 win32_gc, win32_gc->hdc,
			 _gdk_win32_gcvalues_mask_to_string (usage)));

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

  if (usage & LINE_ATTRIBUTES)
    if ((hpen = GetCurrentObject (win32_gc->hdc, OBJ_PEN)) == NULL)
      WIN32_GDI_FAILED ("GetCurrentObject");
  
  if (usage & GDK_GC_FOREGROUND)
    if ((hbr = GetCurrentObject (win32_gc->hdc, OBJ_BRUSH)) == NULL)
      WIN32_GDI_FAILED ("GetCurrentObject");

  GDI_CALL (RestoreDC, (win32_gc->hdc, win32_gc->saved_dc));

  if (GDK_IS_PIXMAP_IMPL_WIN32 (impl))
    GDI_CALL (DeleteDC, (win32_gc->hdc));
  else
    GDI_CALL (ReleaseDC, (win32_gc->hwnd, win32_gc->hdc));

  if (hpen != NULL)
    GDI_CALL (DeleteObject, (hpen));
  
  if (hbr != NULL)
    GDI_CALL (DeleteObject, (hbr));

  if (win32_gc->pen_hbrbg != NULL)
    GDI_CALL (DeleteObject, (win32_gc->pen_hbrbg));

  win32_gc->hdc = NULL;
}

/* This function originally from Jean-Edouard Lachand-Robert, and
 * available at www.codeguru.com. Simplified for our needs, not sure
 * how much of the original code left any longer. Now handles just
 * one-bit deep bitmaps (in Window parlance, ie those that GDK calls
 * bitmaps (and not pixmaps), with zero pixels being transparent.
 */

/* _gdk_win32_bitmap_to_hrgn : Create a region from the
 * "non-transparent" pixels of a bitmap.
 */

HRGN
_gdk_win32_bitmap_to_hrgn (GdkPixmap *pixmap)
{
  HRGN hRgn = NULL;
  HRGN h;
  DWORD maxRects;
  RGNDATA *pData;
  guchar *bits;
  gint width, height, bpl;
  guchar *p;
  gint x, y;

  g_assert (GDK_PIXMAP_OBJECT(pixmap)->depth == 1);

  bits = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl)->bits;
  width = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl)->width;
  height = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl)->height;
  bpl = ((width - 1)/32 + 1)*4;

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

  for (y = 0; y < height; y++)
    {
      /* Scan each bitmap row from left to right*/
      p = (guchar *) bits + y * bpl;
      for (x = 0; x < width; x++)
	{
	  /* Search for a continuous range of "non transparent pixels"*/
	  gint x0 = x;
	  while (x < width)
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

HRGN
_gdk_win32_gdkregion_to_hrgn (GdkRegion *region,
			      gint       x_origin,
			      gint       y_origin)
{
  HRGN hrgn;
  RGNDATA *rgndata;
  RECT *rect;
  GdkRegionBox *boxes = region->rects;
  guint nbytes =
    sizeof (RGNDATAHEADER) + (sizeof (RECT) * region->numRects);
  int i;

  rgndata = g_malloc (nbytes);
  rgndata->rdh.dwSize = sizeof (RGNDATAHEADER);
  rgndata->rdh.iType = RDH_RECTANGLES;
  rgndata->rdh.nCount = rgndata->rdh.nRgnSize = 0;
  SetRect (&rgndata->rdh.rcBound,
	   G_MAXSHORT, G_MAXSHORT, G_MINSHORT, G_MINSHORT);

  for (i = 0; i < region->numRects; i++)
    {
      rect = ((RECT *) rgndata->Buffer) + rgndata->rdh.nCount++;

      rect->left = CLAMP (boxes[i].x1 + x_origin,
			  G_MINSHORT, G_MAXSHORT);
      rect->right = CLAMP (boxes[i].x2 + x_origin,
			   G_MINSHORT, G_MAXSHORT);
      rect->top = CLAMP (boxes[i].y1 + y_origin,
			 G_MINSHORT, G_MAXSHORT);
      rect->bottom = CLAMP (boxes[i].y2 + y_origin,
			    G_MINSHORT, G_MAXSHORT);

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
    WIN32_API_FAILED ("ExtCreateRegion");

  g_free (rgndata);

  return (hrgn);
}

void
_gdk_windowing_gc_get_foreground (GdkGC    *gc,
				  GdkColor *color)
{
  GdkGCWin32 *win32_gc;
  GdkColormap *cmap;
  
  g_return_if_fail (GDK_IS_GC_WIN32 (gc));

  win32_gc = GDK_GC_WIN32 (gc);

  color->pixel = win32_gc->foreground;
  cmap = gdk_gc_get_colormap (gc);

  if (cmap)
    gdk_colormap_query_color (cmap, win32_gc->foreground, color);
  else
    g_warning ("No colormap in _gdk_windowing_gc_get_foreground");
}
