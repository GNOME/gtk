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

#define USE_GENERIC_DRAW

#include <math.h>
#include <stdio.h>
#include <glib.h>

#include <pango/pangowin32.h>

#include "gdkscreen.h" /* gdk_screen_get_default() */
#include "gdkregion-generic.h"
#include "gdkprivate-win32.h"

#define ROP3_D 0x00AA0029
#define ROP3_DPSao 0x00EA02E9
#define ROP3_DSna 0x00220326

static void gdk_win32_draw_rectangle (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      gboolean        filled,
				      gint            x,
				      gint            y,
				      gint            width,
				      gint            height);
static void gdk_win32_draw_arc       (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      gboolean        filled,
				      gint            x,
				      gint            y,
				      gint            width,
				      gint            height,
				      gint            angle1,
				      gint            angle2);
static void gdk_win32_draw_polygon   (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      gboolean        filled,
				      GdkPoint       *points,
				      gint            npoints);
static void gdk_win32_draw_text      (GdkDrawable    *drawable,
				      GdkFont        *font,
				      GdkGC          *gc,
				      gint            x,
				      gint            y,
				      const gchar    *text,
				      gint            text_length);
static void gdk_win32_draw_text_wc   (GdkDrawable    *drawable,
				      GdkFont        *font,
				      GdkGC          *gc,
				      gint            x,
				      gint            y,
				      const GdkWChar *text,
				      gint            text_length);
static void gdk_win32_draw_drawable  (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkPixmap      *src,
				      gint            xsrc,
				      gint            ysrc,
				      gint            xdest,
				      gint            ydest,
				      gint            width,
				      gint            height);
static void gdk_win32_draw_points    (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkPoint       *points,
				      gint            npoints);
static void gdk_win32_draw_segments  (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkSegment     *segs,
				      gint            nsegs);
static void gdk_win32_draw_lines     (GdkDrawable    *drawable,
				      GdkGC          *gc,
				      GdkPoint       *points,
				      gint            npoints);
static void gdk_win32_draw_glyphs    (GdkDrawable      *drawable,
				      GdkGC            *gc,
				      PangoFont        *font,
				      gint              x,
				      gint              y,
				      PangoGlyphString *glyphs);
static void gdk_win32_draw_image     (GdkDrawable     *drawable,
				      GdkGC           *gc,
				      GdkImage        *image,
				      gint             xsrc,
				      gint             ysrc,
				      gint             xdest,
				      gint             ydest,
				      gint             width,
				      gint             height);

static void gdk_win32_set_colormap   (GdkDrawable    *drawable,
				      GdkColormap    *colormap);

static GdkColormap* gdk_win32_get_colormap   (GdkDrawable    *drawable);

static gint         gdk_win32_get_depth      (GdkDrawable    *drawable);

static GdkScreen *  gdk_win32_get_screen     (GdkDrawable    *drawable);

static GdkVisual*   gdk_win32_get_visual     (GdkDrawable    *drawable);

static void gdk_drawable_impl_win32_class_init (GdkDrawableImplWin32Class *klass);

static void gdk_drawable_impl_win32_finalize   (GObject *object);

static gpointer parent_class = NULL;

GType
gdk_drawable_impl_win32_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkDrawableImplWin32Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_drawable_impl_win32_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkDrawableImplWin32),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE,
                                            "GdkDrawableImplWin32",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_drawable_impl_win32_class_init (GdkDrawableImplWin32Class *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_drawable_impl_win32_finalize;

  drawable_class->create_gc = _gdk_win32_gc_new;
  drawable_class->draw_rectangle = gdk_win32_draw_rectangle;
  drawable_class->draw_arc = gdk_win32_draw_arc;
  drawable_class->draw_polygon = gdk_win32_draw_polygon;
  drawable_class->draw_text = gdk_win32_draw_text;
  drawable_class->draw_text_wc = gdk_win32_draw_text_wc;
  drawable_class->draw_drawable = gdk_win32_draw_drawable;
  drawable_class->draw_points = gdk_win32_draw_points;
  drawable_class->draw_segments = gdk_win32_draw_segments;
  drawable_class->draw_lines = gdk_win32_draw_lines;
  drawable_class->draw_glyphs = gdk_win32_draw_glyphs;
  drawable_class->draw_image = gdk_win32_draw_image;
  
  drawable_class->set_colormap = gdk_win32_set_colormap;
  drawable_class->get_colormap = gdk_win32_get_colormap;

  drawable_class->get_depth = gdk_win32_get_depth;
  drawable_class->get_screen = gdk_win32_get_screen;
  drawable_class->get_visual = gdk_win32_get_visual;

  drawable_class->_copy_to_image = _gdk_win32_copy_to_image;
}

static void
gdk_drawable_impl_win32_finalize (GObject *object)
{
  gdk_drawable_set_colormap (GDK_DRAWABLE (object), NULL);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/*****************************************************
 * Win32 specific implementations of generic functions *
 *****************************************************/

static GdkColormap*
gdk_win32_get_colormap (GdkDrawable *drawable)
{
  return GDK_DRAWABLE_IMPL_WIN32 (drawable)->colormap;
}

static void
gdk_win32_set_colormap (GdkDrawable *drawable,
			GdkColormap *colormap)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  if (impl->colormap == colormap)
    return;
  
  if (impl->colormap)
    gdk_colormap_unref (impl->colormap);
  impl->colormap = colormap;
  if (impl->colormap)
    gdk_colormap_ref (impl->colormap);
}

/* Drawing
 */

/*
 * Render a dashed line 'by hand' cause the Win9x GDI is 
 * too limited to do so
 */
static inline gboolean
render_line_horizontal (HDC    hdc,
                        int    x1, 
                        int    x2,
                        int    y,
                        int    pen_width,
                        DWORD *dashes,
                        int    num_dashes)
{
  int n;

  for (n = 0; x1 < x2; n++)
    {
      int len = dashes[n % num_dashes];
      if (x1 + len > x2)
        len = x2 - x1;

      if (n % 2 == 0)
        if (!GDI_CALL (PatBlt, (hdc, x1, y - pen_width / 2, 
				len, pen_width, 
				PATCOPY)))
	  return FALSE;

      x1 += dashes[n % num_dashes];
    }

  return TRUE;
}

static inline gboolean
render_line_vertical (HDC    hdc,
		      int    x, 
                      int    y1,
                      int    y2,
                      int    pen_width,
                      DWORD *dashes,
		      int    num_dashes)
{
  int n;

  for (n = 0; y1 < y2; n++)
    {
      int len = dashes[n % num_dashes];
      if (y1 + len > y2)
        len = y2 - y1;
      if (n % 2 == 0)
        if (!GDI_CALL (PatBlt, (hdc, x - pen_width / 2, y1, 
				pen_width, len, 
				PATCOPY)))
	  return FALSE;

      y1 += dashes[n % num_dashes];
    }

  return TRUE;
}

static void
draw_tiles_lowlevel (HDC  dest,
		     HDC  tile,
		     int  rop3,
		     gint dest_x,
		     gint dest_y,
		     gint tile_x_origin,
		     gint tile_y_origin,
		     gint width,
		     gint height,
		     gint tile_width,
		     gint tile_height)
{
  gint x, y;

  GDK_NOTE (MISC, g_print ("draw_tiles_lowlevel: %p +%d+%d tile=%p:%dx%d@+%d+%d %dx%d\n",
			   dest,
			   dest_x, dest_y,
			   tile, tile_width, tile_height,
			   tile_x_origin, tile_y_origin,
			   width, height));

  y = tile_y_origin % tile_height;
  if (y > 0)
    y -= tile_height;
  while (y < dest_y + height)
    {
      if (y + tile_height >= dest_y)
	{
	  x = tile_x_origin % tile_width;
	  if (x > 0)
	    x -= tile_width;
	  while (x < dest_x + width)
	    {
	      if (x + tile_width >= dest_x)
		{
		  gint src_x = MAX (0, dest_x - x);
		  gint src_y = MAX (0, dest_y - y);

		  if (!GDI_CALL (BitBlt, (dest, x + src_x, y + src_y,
					  MIN (tile_width, dest_x + width - (x + src_x)),
					  MIN (tile_height, dest_y + height - (y + src_y)),
					  tile,
					  src_x, src_y,
					  rop3)))
		    return;
		}
	      x += tile_width;
	    }
	}
      y += tile_height;
    }
}

static void
draw_tiles (GdkDrawable *drawable,
	    GdkGC       *gc,
	    int          rop3,
	    GdkPixmap   *tile,
	    gint         dest_x,
	    gint 	 dest_y,
	    gint 	 tile_x_origin,
	    gint 	 tile_y_origin,
	    gint 	 width,
	    gint 	 height)
{
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND;
  gint tile_width, tile_height;
  GdkGC *gc_copy;
  HDC dest_hdc, tile_hdc;

  gc_copy = gdk_gc_new (tile);
  gdk_gc_copy (gc_copy, gc);
  dest_hdc = gdk_win32_hdc_get (drawable, gc, mask);
  tile_hdc = gdk_win32_hdc_get (tile, gc_copy, mask);

  gdk_drawable_get_size (tile, &tile_width, &tile_height);

  draw_tiles_lowlevel (dest_hdc, tile_hdc, rop3,
		       dest_x, dest_y, tile_x_origin, tile_y_origin,
		       width, height, tile_width, tile_height);

  gdk_win32_hdc_release (drawable, gc, mask);
  gdk_win32_hdc_release (tile, gc_copy, mask);
  gdk_gc_unref (gc_copy);
}

static int
rop2_to_rop3 (int rop2)
{
  switch (rop2)
    {
    /* Oh, Microsoft's silly names for binary and ternary rops. */
#define CASE(rop2,rop3) case R2_##rop2: return rop3
      CASE (BLACK, BLACKNESS);
      CASE (NOTMERGEPEN, NOTSRCERASE);
      CASE (MASKNOTPEN, 0x00220326);
      CASE (NOTCOPYPEN, NOTSRCCOPY);
      CASE (MASKPENNOT, SRCERASE);
      CASE (NOT, DSTINVERT);
      CASE (XORPEN, SRCINVERT);
      CASE (NOTMASKPEN, 0x007700E6);
      CASE (MASKPEN, SRCAND);
      CASE (NOTXORPEN, 0x00990066);
      CASE (NOP, 0x00AA0029);
      CASE (MERGENOTPEN, MERGEPAINT);
      CASE (COPYPEN, SRCCOPY);
      CASE (MERGEPENNOT, 0x00DD0228);
      CASE (MERGEPEN, SRCPAINT);
      CASE (WHITE, WHITENESS);
#undef CASE
    default: return SRCCOPY;
    }
}

static void
generic_draw (GdkDrawable    *drawable,
	      GdkGC          *gc,
	      GdkGCValuesMask mask,
	      void (*function) (GdkGCWin32 *, HDC, gint, gint, va_list),
	      const GdkRegion *region,
	      ...)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  GdkGCWin32 *gcwin32 = GDK_GC_WIN32 (gc);
  HDC hdc = gdk_win32_hdc_get (drawable, gc, mask);
  va_list args;

  va_start (args, region);

  /* If tiled or stippled, draw to a temp pixmap and do blitting magic.
   */

  if (gcwin32->values_mask & GDK_GC_FILL &&
      ((gcwin32->fill_style == GDK_TILED &&
	gcwin32->values_mask & GDK_GC_TILE &&
	gcwin32->tile != NULL)
       ||
       ((gcwin32->fill_style == GDK_OPAQUE_STIPPLED ||
	 gcwin32->fill_style == GDK_STIPPLED) &&
	gcwin32->values_mask & GDK_GC_STIPPLE &&
	gcwin32->stipple != NULL)))
    {
      gint ts_x_origin = 0, ts_y_origin = 0;

      gint width = region->extents.x2 - region->extents.x1;
      gint height = region->extents.y2 - region->extents.y1;

      GdkPixmap *mask_pixmap =
	gdk_pixmap_new (drawable, width, height, 1);
      GdkPixmap *tile_pixmap =
	gdk_pixmap_new (drawable, width, height, -1);
      GdkPixmap *stipple_bitmap = NULL;
      GdkColor fg;
      
      GdkGC *mask_gc = gdk_gc_new (mask_pixmap);
      GdkGC *tile_gc = gdk_gc_new (tile_pixmap);

      HDC mask_hdc;
      HDC tile_hdc = CreateCompatibleDC (hdc);

      HGDIOBJ old_mask_hbm;
      HGDIOBJ old_tile_hbm;

      if (gcwin32->values_mask & GDK_GC_TS_X_ORIGIN)
	ts_x_origin = gc->ts_x_origin;
      if (gcwin32->values_mask & GDK_GC_TS_Y_ORIGIN)
	ts_y_origin = gc->ts_y_origin;

      ts_x_origin -= region->extents.x1;
      ts_y_origin -= region->extents.y1;

      /* Fill mask bitmap with zeros */
      gdk_gc_set_function (mask_gc, GDK_CLEAR);
      gdk_draw_rectangle (mask_pixmap, mask_gc, TRUE,
			  0, 0, width, height);

      /* Paint into mask bitmap, drawing ones */
      gdk_gc_set_function (mask_gc, GDK_COPY);
      fg.pixel = 1;
      gdk_gc_set_foreground (mask_gc, &fg);

      mask_hdc = gdk_win32_hdc_get (mask_pixmap, mask_gc, GDK_GC_FOREGROUND);
      (*function) (GDK_GC_WIN32 (mask_gc), mask_hdc,
		   region->extents.x1, region->extents.y1, args);
      gdk_win32_hdc_release (mask_pixmap, mask_gc, GDK_GC_FOREGROUND);

      if (gcwin32->fill_style == GDK_TILED)
	{
	  /* Tile pixmap with tile */
	  draw_tiles (tile_pixmap, tile_gc, SRCCOPY,
		      gcwin32->tile,
		      0, 0, ts_x_origin, ts_y_origin,
		      width, height);
	}
      else
	{
	  /* Tile with stipple */
	  GdkGC *stipple_gc;

	  stipple_bitmap = gdk_pixmap_new (NULL, width, height, 1);
	  stipple_gc = gdk_gc_new (stipple_bitmap);

	  /* Tile stipple bitmap */
	  draw_tiles (stipple_bitmap, stipple_gc, SRCCOPY,
		      gcwin32->stipple,
		      0, 0, ts_x_origin, ts_y_origin,
		      width, height);

	  if (gcwin32->fill_style == GDK_OPAQUE_STIPPLED)
	    {
	      /* Fill tile pixmap with background */
	      fg.pixel = gcwin32->background;
	      gdk_gc_set_foreground (tile_gc, &fg);
	      gdk_draw_rectangle (tile_pixmap, tile_gc, TRUE,
				  0, 0, width, height);
	    }
	}

      gdk_gc_unref (mask_gc);
      gdk_gc_unref (tile_gc);

      mask_hdc = CreateCompatibleDC (hdc);

      if ((old_mask_hbm = SelectObject (mask_hdc, GDK_PIXMAP_HBITMAP (mask_pixmap))) == NULL)
	WIN32_GDI_FAILED ("SelectObject");

      if ((old_tile_hbm = SelectObject (tile_hdc, GDK_PIXMAP_HBITMAP (tile_pixmap))) == NULL)
	WIN32_GDI_FAILED ("SelectObject");

      if (gcwin32->fill_style == GDK_STIPPLED ||
	  gcwin32->fill_style == GDK_OPAQUE_STIPPLED)
	{
	  HDC stipple_hdc = CreateCompatibleDC (hdc);
	  HGDIOBJ old_stipple_hbm = SelectObject (stipple_hdc, GDK_PIXMAP_HBITMAP (stipple_bitmap));
	  HBRUSH fg_brush = CreateSolidBrush
	    (_gdk_win32_colormap_color (impl->colormap, gcwin32->foreground));
	  HGDIOBJ old_tile_brush = SelectObject (tile_hdc, fg_brush);

	  /* Paint tile with foreround where stipple is one */
	  GDI_CALL (BitBlt, (tile_hdc, 0, 0, width, height,
			     stipple_hdc, 0, 0, ROP3_DPSao));

	  if (gcwin32->fill_style == GDK_STIPPLED)
	    {
	      /* Punch holes in mask where stipple bitmap is zero */
	      GDI_CALL (BitBlt, (mask_hdc, 0, 0, width, height,
				 stipple_hdc, 0, 0, SRCAND));
	    }

	  GDI_CALL (SelectObject, (tile_hdc, old_tile_brush));
	  GDI_CALL (DeleteObject, (fg_brush));
	  GDI_CALL (SelectObject, (stipple_hdc, old_stipple_hbm));
	  GDI_CALL (DeleteDC, (stipple_hdc));
	  gdk_drawable_unref (stipple_bitmap);
	}

      /* Tile pixmap now contains the pattern that we should paint in
       * the areas where mask is one. (It is filled with said pattern.)
       */

      if (IS_WIN_NT ())
	{
	  GDI_CALL (MaskBlt, (hdc, region->extents.x1, region->extents.y1,
			      width, height,
			      tile_hdc, 0, 0,
			      GDK_PIXMAP_HBITMAP (mask_pixmap), 0, 0,
			      MAKEROP4 (rop2_to_rop3 (gcwin32->rop2), ROP3_D)));
	}
      else
	{
	  GdkPixmap *temp1_pixmap =
	    gdk_pixmap_new (drawable, width, height, -1);
	  GdkPixmap *temp2_pixmap =
	    gdk_pixmap_new (drawable, width, height, -1);
	  HDC temp1_hdc = CreateCompatibleDC (hdc);
	  HDC temp2_hdc = CreateCompatibleDC (hdc);
	  HGDIOBJ old_temp1_hbm =
	    SelectObject (temp1_hdc, GDK_PIXMAP_HBITMAP (temp1_pixmap));
	  HGDIOBJ old_temp2_hbm =
	    SelectObject (temp2_hdc, GDK_PIXMAP_HBITMAP (temp2_pixmap));

	  /* Grab copy of dest region to temp1 */
	  GDI_CALL (BitBlt,(temp1_hdc, 0, 0, width, height,
			    hdc, region->extents.x1, region->extents.y1, SRCCOPY));

	  /* Paint tile to temp1 using correct function */
	  GDI_CALL (BitBlt, (temp1_hdc, 0, 0, width, height,
			     tile_hdc, 0, 0, rop2_to_rop3 (gcwin32->rop2)));

	  /* Mask out temp1 where function didn't paint */
	  GDI_CALL (BitBlt, (temp1_hdc, 0, 0, width, height,
			     mask_hdc, 0, 0, SRCAND));

	  /* Grab another copy of dest region to temp2 */
	  GDI_CALL (BitBlt, (temp2_hdc, 0, 0, width, height,
			     hdc, region->extents.x1, region->extents.y1, SRCCOPY));

	  /* Mask out temp2 where function did paint */
	  GDI_CALL (BitBlt, (temp2_hdc, 0, 0, width, height,
			     mask_hdc, 0, 0, ROP3_DSna));

	  /* Combine temp1 with temp2 */
	  GDI_CALL (BitBlt, (temp2_hdc, 0, 0, width, height,
			     temp1_hdc, 0, 0, SRCPAINT));

	  /* Blit back */
	  GDI_CALL (BitBlt, (hdc, region->extents.x1, region->extents.y1, width, height,
			     temp2_hdc, 0, 0, SRCCOPY));

	  /* Cleanup */
	  GDI_CALL (SelectObject, (temp1_hdc, old_temp1_hbm));
	  GDI_CALL (SelectObject, (temp2_hdc, old_temp2_hbm));
	  GDI_CALL (DeleteDC, (temp1_hdc));
	  GDI_CALL (DeleteDC, (temp2_hdc));
	  gdk_drawable_unref (temp1_pixmap);
	  gdk_drawable_unref (temp2_pixmap);
	}
      
      /* Cleanup */
      GDI_CALL (SelectObject, (mask_hdc, old_mask_hbm));
      GDI_CALL (SelectObject, (tile_hdc, old_tile_hbm));
      GDI_CALL (DeleteDC, (mask_hdc));
      GDI_CALL (DeleteDC, (tile_hdc));
      gdk_drawable_unref (mask_pixmap);
      gdk_drawable_unref (tile_pixmap);
    }
  else
    (*function) (gcwin32, hdc, 0, 0, args);

  va_end (args);
  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
draw_rectangle (GdkGCWin32 *gcwin32,
		HDC         hdc,
		gint        x_offset,
		gint        y_offset,
		va_list     args)
{
  HGDIOBJ old_pen_or_brush;
  gint filled;
  gint x;
  gint y;
  gint width;
  gint height;

  filled = va_arg (args, gint);
  x = va_arg (args, gint);
  y = va_arg (args, gint);
  width = va_arg (args, gint);
  height = va_arg (args, gint);
  
  x -= x_offset;
  y -= y_offset;

  if (!filled && gcwin32->pen_dashes && !IS_WIN_NT ())
    {
      render_line_vertical (hdc, x, y, y+height+1,
			    gcwin32->pen_width,
			    gcwin32->pen_dashes,
			    gcwin32->pen_num_dashes) &&
      render_line_horizontal (hdc, x, x+width+1, y,
			      gcwin32->pen_width,
			      gcwin32->pen_dashes,
			      gcwin32->pen_num_dashes) &&
      render_line_vertical (hdc, x+width+1, y, y+height+1,
			    gcwin32->pen_width,
			    gcwin32->pen_dashes,
			    gcwin32->pen_num_dashes) &&
      render_line_horizontal (hdc, x, x+width+1, y+height+1,
			      gcwin32->pen_width,
			      gcwin32->pen_dashes,
			      gcwin32->pen_num_dashes);
    }
  else
    {
      if (filled)
	old_pen_or_brush = SelectObject (hdc, GetStockObject (NULL_PEN));
      else
	old_pen_or_brush = SelectObject (hdc, GetStockObject (HOLLOW_BRUSH));
      if (old_pen_or_brush == NULL)
	WIN32_GDI_FAILED ("SelectObject");
      else
	GDI_CALL (Rectangle, (hdc, x, y, x+width+1, y+height+1));

      if (old_pen_or_brush != NULL)
	GDI_CALL (SelectObject, (hdc, old_pen_or_brush));
    }
}

static void
gdk_win32_draw_rectangle (GdkDrawable *drawable,
			  GdkGC       *gc,
			  gboolean     filled,
			  gint         x,
			  gint         y,
			  gint         width,
			  gint         height)
{
#ifdef USE_GENERIC_DRAW

  GdkRectangle bounds;
  GdkRegion *region;
  gint pen_width;

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_rectangle: %s (%p) %s%dx%d@+%d+%d\n",
			   _gdk_win32_drawable_description (drawable),
			   gc,
			   (filled ? "fill " : ""),
			   width, height, x, y));
    
  pen_width = GDK_GC_WIN32 (gc)->pen_width;
  if (pen_width == 0)
    pen_width = 1;

  bounds.x = x - pen_width;
  bounds.y = y - pen_width;
  bounds.width = width + 2 * pen_width;
  bounds.height = height + 2 * pen_width;
  region = gdk_region_rectangle (&bounds);

  generic_draw (drawable, gc, GDK_GC_FOREGROUND|GDK_GC_BACKGROUND,
		draw_rectangle, region, filled, x, y, width, height);

  gdk_region_destroy (region);

#else  /* !USE_GENERIC_DRAW */

  GdkGCWin32 *gcwin32 = GDK_GC_WIN32 (gc);
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_BACKGROUND;
  HDC hdc;
  HGDIOBJ old_pen_or_brush;
  POINT pts[4];
  gboolean ok = TRUE;

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_rectangle: %s (%p) %s%dx%d@+%d+%d\n",
			   _gdk_win32_drawable_description (drawable),
			   gcwin32,
			   (filled ? "fill " : ""),
			   width, height, x, y));
    
  if (filled 
      && (gcwin32->values_mask & GDK_GC_TILE)    
      && (gcwin32->tile)
      && (gcwin32->values_mask & GDK_GC_FILL)
      && (gcwin32->fill_style == GDK_TILED))
    {
      draw_tiles (drawable, gc, SRCCOPY,
		  gcwin32->tile,
		  x, y,
		  gc->ts_x_origin,
		  gc->ts_y_origin,
		  width, height);
      return;
    }

  hdc = gdk_win32_hdc_get (drawable, gc, mask);

  if (gcwin32->fill_style == GDK_OPAQUE_STIPPLED)
    {
      if (!BeginPath (hdc))
	WIN32_GDI_FAILED ("BeginPath"), ok = FALSE;

      /* Win9x doesn't support Rectangle calls in a path,
       * thus use Polyline.
       */
	  
      pts[0].x = x;
      pts[0].y = y;
      pts[1].x = x + width + 1;
      pts[1].y = y;
      pts[2].x = x + width + 1;
      pts[2].y = y + height + 1;
      pts[3].x = x;
      pts[3].y = y + height + 1;
      
      if (ok)
	MoveToEx (hdc, x, y, NULL);
      
      if (ok && !Polyline (hdc, pts, 4))
	WIN32_GDI_FAILED ("Polyline"), ok = FALSE;
	  
      if (ok && !CloseFigure (hdc))
	WIN32_GDI_FAILED ("CloseFigure"), ok = FALSE;

      if (ok && !EndPath (hdc))
	WIN32_GDI_FAILED ("EndPath"), ok = FALSE;
	  
      if (ok && !filled)
	if (!WidenPath (hdc))
	  WIN32_GDI_FAILED ("WidenPath"), ok = FALSE;
	  
      if (ok && !FillPath (hdc))
	WIN32_GDI_FAILED ("FillPath"), ok = FALSE;
    }
  else
    {
      if (!filled && gcwin32->pen_dashes && !IS_WIN_NT ())
        {
          ok = ok && render_line_vertical (hdc, x, y, y+height+1,
                                           gcwin32->pen_width,
                                           gcwin32->pen_dashes,
                                           gcwin32->pen_num_dashes);
          ok = ok && render_line_horizontal (hdc, x, x+width+1, y,
                                             gcwin32->pen_width,
                                             gcwin32->pen_dashes,
                                             gcwin32->pen_num_dashes);
          ok = ok && render_line_vertical (hdc, x+width+1, y, y+height+1,
                                           gcwin32->pen_width,
                                           gcwin32->pen_dashes,
                                           gcwin32->pen_num_dashes);
          ok = ok && render_line_horizontal (hdc, x, x+width+1, y+height+1,
                                             gcwin32->pen_width,
                                             gcwin32->pen_dashes,
                                             gcwin32->pen_num_dashes);
        }
      else
        {
          if (filled)
            old_pen_or_brush = SelectObject (hdc, GetStockObject (NULL_PEN));
          else
            old_pen_or_brush = SelectObject (hdc, GetStockObject (HOLLOW_BRUSH));
          if (old_pen_or_brush == NULL)
            WIN32_GDI_FAILED ("SelectObject");
          if (!Rectangle (hdc, x, y, x+width+1, y+height+1))
            WIN32_GDI_FAILED ("Rectangle");
          if (old_pen_or_brush)
            SelectObject (hdc, old_pen_or_brush);
        }
    }

  gdk_win32_hdc_release (drawable, gc, mask);

#endif /* !USE_GENERIC_DRAW */
}

static void
gdk_win32_draw_arc (GdkDrawable *drawable,
		    GdkGC       *gc,
		    gboolean     filled,
		    gint         x,
		    gint         y,
		    gint         width,
		    gint         height,
		    gint         angle1,
		    gint         angle2)
{
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_BACKGROUND;
  HDC hdc;
  int nXStartArc, nYStartArc, nXEndArc, nYEndArc;

  GDK_NOTE (MISC, g_print ("gdk_draw_arc: %s  %d,%d,%d,%d  %d %d\n",
			   _gdk_win32_drawable_description (drawable),
			   x, y, width, height, angle1, angle2));

  /* Seems that drawing arcs with width or height <= 2 fails, at least
   * with my TNT card.
   */
  if (width <= 2 || height <= 2 || angle2 == 0)
    return;

  hdc = gdk_win32_hdc_get (drawable, gc, mask);

  if (angle2 >= 360*64)
    {
      nXStartArc = nYStartArc = nXEndArc = nYEndArc = 0;
    }
  else if (angle2 > 0)
    {
      /* The 100. is just an arbitrary value */
      nXStartArc = x + width/2 + 100. * cos(angle1/64.*2.*G_PI/360.);
      nYStartArc = y + height/2 + -100. * sin(angle1/64.*2.*G_PI/360.);
      nXEndArc = x + width/2 + 100. * cos((angle1+angle2)/64.*2.*G_PI/360.);
      nYEndArc = y + height/2 + -100. * sin((angle1+angle2)/64.*2.*G_PI/360.);
    }
  else
    {
      nXEndArc = x + width/2 + 100. * cos(angle1/64.*2.*G_PI/360.);
      nYEndArc = y + height/2 + -100. * sin(angle1/64.*2.*G_PI/360.);
      nXStartArc = x + width/2 + 100. * cos((angle1+angle2)/64.*2.*G_PI/360.);
      nYStartArc = y + height/2 + -100. * sin((angle1+angle2)/64.*2.*G_PI/360.);
    }
  
  /* GDK_OPAQUE_STIPPLED arcs not implemented. */
  
  if (filled)
    {
      GDK_NOTE (MISC, g_print ("...Pie(hdc,%d,%d,%d,%d,%d,%d,%d,%d)\n",
			       x, y, x+width, y+height,
			       nXStartArc, nYStartArc,
			       nXEndArc, nYEndArc));
      if (!Pie (hdc, x, y, x+width, y+height,
		nXStartArc, nYStartArc, nXEndArc, nYEndArc))
	WIN32_GDI_FAILED ("Pie");
    }
  else
    {
      GDK_NOTE (MISC, g_print ("...Arc(hdc,%d,%d,%d,%d,%d,%d,%d,%d)\n",
			       x, y, x+width, y+height,
			       nXStartArc, nYStartArc,
			       nXEndArc, nYEndArc));
      if (!Arc (hdc, x, y, x+width, y+height,
		nXStartArc, nYStartArc, nXEndArc, nYEndArc))
	WIN32_GDI_FAILED ("Arc");
    }
  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
gdk_win32_draw_polygon (GdkDrawable *drawable,
			GdkGC       *gc,
			gboolean     filled,
			GdkPoint    *points,
			gint         npoints)
{
  GdkGCWin32 *gcwin32 = GDK_GC_WIN32 (gc);
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_BACKGROUND;
  HDC hdc;
  POINT *pts;
  gboolean ok = TRUE;
  int i;

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_polygon: %s (%p) %d\n",
			   _gdk_win32_drawable_description (drawable),
			   gcwin32,
			   npoints));

  if (npoints < 2)
    return;

  hdc = gdk_win32_hdc_get (drawable, gc, mask);
  pts = g_new (POINT, npoints+1);

  for (i = 0; i < npoints; i++)
    {
      pts[i].x = points[i].x;
      pts[i].y = points[i].y;
    }
  
  if (gcwin32->fill_style == GDK_OPAQUE_STIPPLED)
    {
      if (!BeginPath (hdc))
	WIN32_GDI_FAILED ("BeginPath"), ok = FALSE;

      if (ok)
	MoveToEx (hdc, points[0].x, points[0].y, NULL);

      if (pts[0].x == pts[npoints-1].x && pts[0].y == pts[npoints-1].y)
	npoints--;

      if (ok && !Polyline (hdc, pts, 4))
	WIN32_GDI_FAILED ("Polyline"), ok = FALSE;
	  
      if (ok && !CloseFigure (hdc))
	WIN32_GDI_FAILED ("CloseFigure"), ok = FALSE;

      if (ok && !EndPath (hdc))
	WIN32_GDI_FAILED ("EndPath"), ok = FALSE;
	  
      if (ok && !filled)
	if (!WidenPath (hdc))
	  WIN32_GDI_FAILED ("WidenPath"), ok = FALSE;
	  
      if (ok && !FillPath (hdc))
	WIN32_GDI_FAILED ("FillPath"), ok = FALSE;
    }
  else
    {
      if (points[0].x != points[npoints-1].x
	  || points[0].y != points[npoints-1].y) 
	{
	  pts[npoints].x = points[0].x;
	  pts[npoints].y = points[0].y;
	  npoints++;
	}
      
      if (filled)
	{
	  if (!Polygon (hdc, pts, npoints))
	    WIN32_GDI_FAILED ("Polygon");
	}
      else
	{
	  if (!Polyline (hdc, pts, npoints))
	    WIN32_GDI_FAILED ("Polyline");
	}
    }
  g_free (pts);
  gdk_win32_hdc_release (drawable, gc, mask);
}

typedef struct
{
  gint x, y;
  HDC hdc;
} gdk_draw_text_arg;

static void
gdk_draw_text_handler (GdkWin32SingleFont *singlefont,
		       const wchar_t      *wcstr,
		       int                 wclen,
		       void               *arg)
{
  HGDIOBJ oldfont;
  SIZE size;
  gdk_draw_text_arg *argp = (gdk_draw_text_arg *) arg;

  if (!singlefont)
    return;

  if ((oldfont = SelectObject (argp->hdc, singlefont->hfont)) == NULL)
    {
      WIN32_GDI_FAILED ("SelectObject");
      return;
    }
  
  if (!TextOutW (argp->hdc, argp->x, argp->y, wcstr, wclen))
    WIN32_GDI_FAILED ("TextOutW");
  GetTextExtentPoint32W (argp->hdc, wcstr, wclen, &size);
  argp->x += size.cx;

  SelectObject (argp->hdc, oldfont);
}

static void
gdk_win32_draw_text (GdkDrawable *drawable,
		     GdkFont     *font,
		     GdkGC       *gc,
		     gint         x,
		     gint         y,
		     const gchar *text,
		     gint         text_length)
{
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_FONT;
  wchar_t *wcstr, wc;
  gint wlen;
  gdk_draw_text_arg arg;

  if (text_length == 0)
    return;

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  arg.x = x;
  arg.y = y;
  arg.hdc = gdk_win32_hdc_get (drawable, gc, mask);

  GDK_NOTE (MISC, g_print ("gdk_draw_text: %s (%d,%d) \"%.*s\" (len %d)\n",
			   _gdk_win32_drawable_description (drawable),
			   x, y,
			   (text_length > 10 ? 10 : text_length),
			   text, text_length));
  
  if (text_length == 1)
    {
      /* For single characters, don't try to interpret as UTF-8. */
      wc = (guchar) text[0];
      _gdk_wchar_text_handle (font, &wc, 1, gdk_draw_text_handler, &arg);
    }
  else
    {
      wcstr = g_new (wchar_t, text_length);
      if ((wlen = _gdk_utf8_to_ucs2 (wcstr, text, text_length, text_length)) == -1)
	g_warning ("gdk_win32_draw_text: _gdk_utf8_to_ucs2 failed");
      else
	_gdk_wchar_text_handle (font, wcstr, wlen, gdk_draw_text_handler, &arg);
      g_free (wcstr);
    }


  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
gdk_win32_draw_text_wc (GdkDrawable	 *drawable,
			GdkFont          *font,
			GdkGC		 *gc,
			gint		  x,
			gint		  y,
			const GdkWChar *text,
			gint		  text_length)
{
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_FONT;
  gint i;
  wchar_t *wcstr;
  gdk_draw_text_arg arg;

  if (text_length == 0)
    return;

  g_assert (font->type == GDK_FONT_FONT || font->type == GDK_FONT_FONTSET);

  arg.x = x;
  arg.y = y;
  arg.hdc = gdk_win32_hdc_get (drawable, gc, mask);

  GDK_NOTE (MISC, g_print ("gdk_draw_text_wc: %s (%d,%d) len: %d\n",
			   _gdk_win32_drawable_description (drawable),
			   x, y, text_length));
      
  if (sizeof (wchar_t) != sizeof (GdkWChar))
    {
      wcstr = g_new (wchar_t, text_length);
      for (i = 0; i < text_length; i++)
	wcstr[i] = text[i];
    }
  else
    wcstr = (wchar_t *) text;

  _gdk_wchar_text_handle (font, wcstr, text_length,
			 gdk_draw_text_handler, &arg);

  if (sizeof (wchar_t) != sizeof (GdkWChar))
    g_free (wcstr);

  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
gdk_win32_draw_drawable (GdkDrawable *drawable,
			 GdkGC       *gc,
			 GdkPixmap   *src,
			 gint         xsrc,
			 gint         ysrc,
			 gint         xdest,
			 gint         ydest,
			 gint         width,
			 gint         height)
{
  g_assert (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable));

  _gdk_win32_blit (FALSE, (GdkDrawableImplWin32 *) drawable,
		   gc, src, xsrc, ysrc,
		   xdest, ydest, width, height);
}

static void
gdk_win32_draw_points (GdkDrawable *drawable,
		       GdkGC       *gc,
		       GdkPoint    *points,
		       gint         npoints)
{
  HDC hdc;
  COLORREF fg;
  GdkGCWin32 *gcwin32 = GDK_GC_WIN32 (gc);
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  int i;

  hdc = gdk_win32_hdc_get (drawable, gc, 0);
  
  fg = _gdk_win32_colormap_color (impl->colormap, gcwin32->foreground);

  GDK_NOTE (MISC, g_print ("gdk_draw_points: %s %dx%.06x\n",
			   _gdk_win32_drawable_description (drawable),
			   npoints, (guint) fg));

  for (i = 0; i < npoints; i++)
    SetPixel (hdc, points[i].x, points[i].y, fg);

  gdk_win32_hdc_release (drawable, gc, 0);
}

static void
gdk_win32_draw_segments (GdkDrawable *drawable,
			 GdkGC       *gc,
			 GdkSegment  *segs,
			 gint         nsegs)
{
  GdkGCWin32 *gcwin32 = GDK_GC_WIN32 (gc);
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_BACKGROUND;
  HDC hdc;
  gboolean ok = TRUE;
  int i;

  GDK_NOTE (MISC, g_print ("gdk_win32_draw_segments: %s nsegs: %d\n",
			   _gdk_win32_drawable_description (drawable), nsegs));

  hdc = gdk_win32_hdc_get (drawable, gc, mask);

  if (gcwin32->fill_style == GDK_OPAQUE_STIPPLED)
    {
      if (!BeginPath (hdc))
	WIN32_GDI_FAILED ("BeginPath"), ok = FALSE;
      
      for (i = 0; ok && i < nsegs; i++)
	{
	  if (ok && !MoveToEx (hdc, segs[i].x1, segs[i].y1, NULL))
	    WIN32_GDI_FAILED ("MoveToEx"), ok = FALSE;
	  if (ok && !LineTo (hdc, segs[i].x2, segs[i].y2))
	    WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
	  
	  /* Draw end pixel */
	  if (ok && gcwin32->pen_width <= 1)
	    if (!LineTo (hdc, segs[i].x2 + 1, segs[i].y2))
	      WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
	}

      if (ok && !EndPath (hdc))
	WIN32_GDI_FAILED ("EndPath"), ok = FALSE;
	  
      if (ok && !WidenPath (hdc))
	WIN32_GDI_FAILED ("WidenPath"), ok = FALSE;
	  
      if (ok && !FillPath (hdc))
	WIN32_GDI_FAILED ("FillPath"), ok = FALSE;
    }
  else
    {
      if (gcwin32->pen_dashes && !IS_WIN_NT ())
        {
          /* code very similar to the IMHO questionable optimization 
           * below. This one draws dashed vertical/horizontal lines 
           * with the limited Win9x GDI.                        --hb
           */
          for (i = 0; ok && i < nsegs; i++)
            {
    	        if (segs[i].x1 == segs[i].x2)
                {
                  int y1, y2;

                  if (segs[i].y1 <= segs[i].y2)
                    y1 = segs[i].y1, y2 = segs[i].y2;
                  else
                    y1 = segs[i].y2, y2 = segs[i].y1;

                  ok = render_line_vertical (hdc,
                                             segs[i].x1, y1, y2,
                                             gcwin32->pen_width,
                                             gcwin32->pen_dashes,
                                             gcwin32->pen_num_dashes);
                }
              else if (segs[i].y1 == segs[i].y2)
                {
                  int x1, x2;

                  if (segs[i].x1 <= segs[i].x2)
                    x1 = segs[i].x1, x2 = segs[i].x2;
                  else
                    x1 = segs[i].x2, x2 = segs[i].x1;

                  ok = render_line_horizontal (hdc,
                                               x1, x2, segs[i].y1,
                                               gcwin32->pen_width,
                                               gcwin32->pen_dashes,
                                               gcwin32->pen_num_dashes);
                }
              else
                {
                  if (!MoveToEx (hdc, segs[i].x1, segs[i].y1, NULL))
                    WIN32_GDI_FAILED ("MoveToEx"), ok = FALSE;
                  if (ok && !LineTo (hdc, segs[i].x2, segs[i].y2))
                    WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
    	  
                  /* Draw end pixel */
                  if (ok && gcwin32->pen_width <= 1)
                    if (!LineTo (hdc, segs[i].x2 + 1, segs[i].y2))
                      WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
                }
            }
        }
      else
        {
          const gboolean maybe_patblt =
            gcwin32->rop2 == R2_COPYPEN &&
            gcwin32->pen_width <= 1 &&
            (gcwin32->pen_style & PS_STYLE_MASK) == PS_SOLID;

          for (i = 0; ok && i < nsegs; i++)
            {
              /* PatBlt() is much faster than LineTo(), says
               * jpe@archaeopteryx.com. Hmm. Use it if we have a solid
               * colour pen, then we know that the brush is also solid and
               * of the same colour.
    	         */
    	        if (maybe_patblt && segs[i].x1 == segs[i].x2)
                {
                  int y1, y2;

                  if (segs[i].y1 <= segs[i].y2)
                    y1 = segs[i].y1, y2 = segs[i].y2;
                  else
                    y1 = segs[i].y2, y2 = segs[i].y1;

                  if (!PatBlt (hdc, segs[i].x1, y1,
                               1, y2 - y1 + 1, PATCOPY))
                    WIN32_GDI_FAILED ("PatBlt"), ok = FALSE;
                }
              else if (maybe_patblt && segs[i].y1 == segs[i].y2)
                {
                  int x1, x2;

                  if (segs[i].x1 <= segs[i].x2)
                    x1 = segs[i].x1, x2 = segs[i].x2;
                  else
                    x1 = segs[i].x2, x2 = segs[i].x1;

                  if (!PatBlt (hdc, x1, segs[i].y1,
                               x2 - x1 + 1, 1, PATCOPY))
                    WIN32_GDI_FAILED ("PatBlt"), ok = FALSE;
                }
              else
                {
                  if (!MoveToEx (hdc, segs[i].x1, segs[i].y1, NULL))
                    WIN32_GDI_FAILED ("MoveToEx"), ok = FALSE;
                  if (ok && !LineTo (hdc, segs[i].x2, segs[i].y2))
                    WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
    	  
                  /* Draw end pixel */
                  if (ok && gcwin32->pen_width <= 1)
                    if (!LineTo (hdc, segs[i].x2 + 1, segs[i].y2))
                      WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
                }
            }
    	  }
    }
  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
gdk_win32_draw_lines (GdkDrawable *drawable,
		      GdkGC       *gc,
		      GdkPoint    *points,
		      gint         npoints)
{
  GdkGCWin32 *gcwin32 = GDK_GC_WIN32 (gc);
  const GdkGCValuesMask mask = GDK_GC_FOREGROUND|GDK_GC_BACKGROUND;
  HDC hdc;
  POINT *pts;
  int i;
  gboolean ok = TRUE;

  if (npoints < 2)
    return;

  hdc = gdk_win32_hdc_get (drawable, gc, mask);

  if (gcwin32->pen_dashes && !IS_WIN_NT ())
    {
      for (i = 0; i < npoints - 1; i++)
        {
	  if (points[i].x == points[i+1].x)
	    {
	      int y1, y2;
	      if (points[i].y > points[i+1].y)
	        y1 = points[i+1].y, y2 = points[i].y;
	      else
	        y1 = points[i].y, y2 = points[i+1].y;

	      render_line_vertical (hdc, points[i].x, y1, y2,
	                            gcwin32->pen_width,
	                            gcwin32->pen_dashes,
	                            gcwin32->pen_num_dashes);
	    }
	  else if (points[i].y == points[i+1].y)
	    {
	      int x1, x2;
	      if (points[i].x > points[i+1].x)
	        x1 = points[i+1].x, x2 = points[i].x;
	      else
	        x1 = points[i].x, x2 = points[i+1].x;

	      render_line_horizontal (hdc, x1, x2, points[i].y,
	                              gcwin32->pen_width,
	                              gcwin32->pen_dashes,
	                              gcwin32->pen_num_dashes);
	    }
	  else
	    {
	      if (!MoveToEx (hdc, points[i].x, points[i].y, NULL))
		WIN32_GDI_FAILED ("MoveToEx"), ok = FALSE;
	      if (ok && !LineTo (hdc, points[i+1].x, points[i+1].y))
		WIN32_GDI_FAILED ("LineTo"), ok = FALSE;
	    }
	}
    }
  else
    {
      pts = g_new (POINT, npoints);

      for (i = 0; i < npoints; i++)
	{
	  pts[i].x = points[i].x;
	  pts[i].y = points[i].y;
	}
  
      if (!Polyline (hdc, pts, npoints))
	WIN32_GDI_FAILED ("Polyline"), ok = FALSE;
  
      g_free (pts);
  
      /* Draw end pixel */
      if (ok && gcwin32->pen_width <= 1)
	{
	  MoveToEx (hdc, points[npoints-1].x, points[npoints-1].y, NULL);
	  if (!LineTo (hdc, points[npoints-1].x + 1, points[npoints-1].y))
	    WIN32_GDI_FAILED ("LineTo");
	}
    }

  gdk_win32_hdc_release (drawable, gc, mask);
}

static void
draw_glyphs (GdkGCWin32 *gcwin32,
	     HDC         hdc,
	     gint        x_offset,
	     gint        y_offset,
	     va_list     args)
{
  PangoFont *font;
  gint x;
  gint y;
  PangoGlyphString *glyphs;

  font = va_arg (args, PangoFont *);
  x = va_arg (args, gint);
  y = va_arg (args, gint);
  glyphs = va_arg (args, PangoGlyphString *);

  x -= x_offset;
  y -= y_offset;

  /* HB: Maybe there should be a GDK_GC_PANGO flag for hdc_get */
  /* default write mode is transparent (leave background) */
  GDI_CALL (SetBkMode, (hdc, TRANSPARENT));

  if (GDI_ERROR == SetTextAlign (hdc, TA_LEFT|TA_BASELINE|TA_NOUPDATECP))
    WIN32_GDI_FAILED ("SetTextAlign");

  pango_win32_render (hdc, font, glyphs, x, y);
}

static void
gdk_win32_draw_glyphs (GdkDrawable      *drawable,
		       GdkGC            *gc,
		       PangoFont        *font,
		       gint              x,
		       gint              y,
		       PangoGlyphString *glyphs)
{
#ifdef USE_GENERIC_DRAW

  GdkRectangle bounds;
  GdkRegion *region;
  PangoRectangle ink_rect;

  pango_glyph_string_extents (glyphs, font, &ink_rect, NULL);

  bounds.x = x + PANGO_PIXELS (ink_rect.x) - 1;
  bounds.y = y + PANGO_PIXELS (ink_rect.y) - 1;
  bounds.width = PANGO_PIXELS (ink_rect.width) + 2;
  bounds.height = PANGO_PIXELS (ink_rect.height) + 2;
  region = gdk_region_rectangle (&bounds);
  
  generic_draw (drawable, gc, GDK_GC_FOREGROUND,
		draw_glyphs, region, font, x, y, glyphs);

  gdk_region_destroy (region);

#else

  const GdkGCValuesMask mask = GDK_GC_FOREGROUND;
  HDC hdc;

  hdc = gdk_win32_hdc_get (drawable, gc, mask);

  /* HB: Maybe there should be a GDK_GC_PANGO flag for hdc_get */
  /* default write mode is transparent (leave background) */
  if (SetBkMode (hdc, TRANSPARENT) == 0)
    WIN32_GDI_FAILED ("SetBkMode");

  if (GDI_ERROR == SetTextAlign (hdc, TA_LEFT|TA_BASELINE|TA_NOUPDATECP))
    WIN32_GDI_FAILED ("SetTextAlign");

  pango_win32_render (hdc, font, glyphs, x, y);

  gdk_win32_hdc_release (drawable, gc, mask);

#endif
}

static void
blit_from_pixmap (gboolean              use_fg_bg,
		  GdkDrawableImplWin32 *dest,
		  HDC                   hdc,
		  GdkPixmapImplWin32   *src,
		  GdkGCWin32           *gcwin32,
		  gint         	      	xsrc,
		  gint         	      	ysrc,
		  gint         	      	xdest,
		  gint         	      	ydest,
		  gint         	      	width,
		  gint         	      	height)
{
  HDC srcdc;
  HBITMAP holdbitmap;
  RGBQUAD oldtable[256], newtable[256];
  COLORREF bg, fg;

  gint newtable_size = 0, oldtable_size = 0;
  gboolean ok = TRUE;
  
  GDK_NOTE (MISC, g_print ("blit_from_pixmap\n"));

  if (!(srcdc = CreateCompatibleDC (NULL)))
    {
      WIN32_GDI_FAILED ("CreateCompatibleDC");
      return;
    }
      
  if (!(holdbitmap = SelectObject (srcdc, ((GdkDrawableImplWin32 *) src)->handle)))
    WIN32_GDI_FAILED ("SelectObject");
  else
    {
      if (src->image->depth <= 8)
	{
	  /* Blitting from a 1, 4 or 8-bit pixmap */

	  if ((oldtable_size = GetDIBColorTable (srcdc, 0, 256, oldtable)) == 0)
	    WIN32_GDI_FAILED ("GetDIBColorTable");
	  else if (src->image->depth == 1)
	    {
	      /* Blitting from an 1-bit pixmap */

	      gint bgix, fgix;
	      
	      if (use_fg_bg)
		{
		  bgix = gcwin32->background;
		  fgix = gcwin32->foreground;
		}
	      else
		{
		  bgix = 0;
		  fgix = 1;
		}

	      if (GDK_IS_PIXMAP_IMPL_WIN32 (dest) &&
		  ((GdkPixmapImplWin32 *) dest)->image->depth <= 8)
		{
		  /* Destination is also pixmap, get fg and bg from
		   * its palette. Either use the foreground and
		   * background pixel values in the GC (only in the
		   * case of gdk_image_put(), cf. XPutImage()), or 0
		   * and 1 to index the palette.
		   */
		  if (!GDI_CALL (GetDIBColorTable, (hdc, bgix, 1, newtable)) ||
		      !GDI_CALL (GetDIBColorTable, (hdc, fgix, 1, newtable+1)))
		    ok = FALSE;
		}
	      else
		{
		  /* Destination is a window, get fg and bg from its
		   * colormap
		   */

		  bg = _gdk_win32_colormap_color (dest->colormap, bgix);
		  fg = _gdk_win32_colormap_color (dest->colormap, fgix);
		  newtable[0].rgbBlue = GetBValue (bg);
		  newtable[0].rgbGreen = GetGValue (bg);
		  newtable[0].rgbRed = GetRValue (bg);
		  newtable[0].rgbReserved = 0;
		  newtable[1].rgbBlue = GetBValue (fg);
		  newtable[1].rgbGreen = GetGValue (fg);
		  newtable[1].rgbRed = GetRValue (fg);
		  newtable[1].rgbReserved = 0;
		}
	      if (ok)
		GDK_NOTE (MISC, g_print ("bg: %02x %02x %02x "
					 "fg: %02x %02x %02x\n",
					 newtable[0].rgbRed,
					 newtable[0].rgbGreen,
					 newtable[0].rgbBlue,
					 newtable[1].rgbRed,
					 newtable[1].rgbGreen,
					 newtable[1].rgbBlue));
	      newtable_size = 2;
	    }
	  else if (GDK_IS_PIXMAP_IMPL_WIN32 (dest))
	    {
	      /* Destination is pixmap, get its color table */
	      
	      if ((newtable_size = GetDIBColorTable (hdc, 0, 256, newtable)) == 0)
		WIN32_GDI_FAILED ("GetDIBColorTable"), ok = FALSE;
	    }
	  
	  /* If blitting between pixmaps, set source's color table */
	  if (ok && newtable_size > 0)
	    {
	      GDK_NOTE (MISC_OR_COLORMAP,
			g_print ("blit_from_pixmap: set color table"
				 " hdc=%p count=%d\n",
				 srcdc, newtable_size));
	      if (!GDI_CALL (SetDIBColorTable, (srcdc, 0, newtable_size, newtable)))
		ok = FALSE;
	    }
	}
      
      if (ok)
	GDI_CALL (BitBlt, (hdc, xdest, ydest, width, height,
			   srcdc, xsrc, ysrc, rop2_to_rop3 (gcwin32->rop2)));
      
      /* Restore source's color table if necessary */
      if (ok && newtable_size > 0 && oldtable_size > 0)
	{
	  GDK_NOTE (MISC_OR_COLORMAP,
		    g_print ("blit_from_pixmap: reset color table"
			     " hdc=%p count=%d\n",
			     srcdc, oldtable_size));
	  GDI_CALL (SetDIBColorTable, (srcdc, 0, oldtable_size, oldtable));
	}
      
      GDI_CALL (SelectObject, (srcdc, holdbitmap));
    }
  GDI_CALL (DeleteDC, (srcdc));
}

static void
blit_inside_window (HDC      	hdc,
		    GdkGCWin32 *gcwin32,
		    gint     	xsrc,
		    gint     	ysrc,
		    gint     	xdest,
		    gint     	ydest,
		    gint     	width,
		    gint     	height)

{
  GDK_NOTE (MISC, g_print ("blit_inside_window\n"));

  GDI_CALL (BitBlt, (hdc, xdest, ydest, width, height,
		     hdc, xsrc, ysrc, rop2_to_rop3 (gcwin32->rop2)));
}

static void
blit_from_window (HDC                   hdc,
		  GdkGCWin32           *gcwin32,
		  GdkDrawableImplWin32 *src,
		  gint         	      	xsrc,
		  gint         	      	ysrc,
		  gint         	      	xdest,
		  gint         	      	ydest,
		  gint         	      	width,
		  gint         	      	height)
{
  HDC srcdc;
  HPALETTE holdpal = NULL;
  GdkColormap *cmap = gdk_colormap_get_system ();

  GDK_NOTE (MISC, g_print ("blit_from_window\n"));

  if ((srcdc = GetDC (src->handle)) == NULL)
    {
      WIN32_GDI_FAILED ("GetDC");
      return;
    }

  if (cmap->visual->type == GDK_VISUAL_PSEUDO_COLOR ||
      cmap->visual->type == GDK_VISUAL_STATIC_COLOR)
    {
      gint k;
      
      if (!(holdpal = SelectPalette (srcdc, GDK_WIN32_COLORMAP_DATA (cmap)->hpal, FALSE)))
	WIN32_GDI_FAILED ("SelectPalette");
      else if ((k = RealizePalette (srcdc)) == GDI_ERROR)
	WIN32_GDI_FAILED ("RealizePalette");
      else if (k > 0)
	GDK_NOTE (MISC_OR_COLORMAP,
		  g_print ("blit_from_window: realized %d\n", k));
    }
  
  GDI_CALL (BitBlt, (hdc, xdest, ydest, width, height,
		     srcdc, xsrc, ysrc, rop2_to_rop3 (gcwin32->rop2)));
  
  if (holdpal != NULL)
    GDI_CALL (SelectPalette, (srcdc, holdpal, FALSE));
  
  GDI_CALL (ReleaseDC, (src->handle, srcdc));
}

void
_gdk_win32_blit (gboolean              use_fg_bg,
		 GdkDrawableImplWin32 *drawable,
		 GdkGC       	      *gc,
		 GdkDrawable 	      *src,
		 gint        	       xsrc,
		 gint        	       ysrc,
		 gint        	       xdest,
		 gint        	       ydest,
		 gint        	       width,
		 gint        	       height)
{
  HDC hdc;
  HRGN src_rgn, draw_rgn, outside_rgn;
  RECT r;
  GdkDrawableImplWin32 *draw_impl;
  GdkDrawableImplWin32 *src_impl = NULL;
  gint src_width, src_height;
  
  GDK_NOTE (MISC, g_print ("_gdk_win32_blit: src:%s %dx%d@+%d+%d\n"
			   "                 dst:%s @+%d+%d use_fg_bg=%d\n",
			   _gdk_win32_drawable_description (src),
			   width, height, xsrc, ysrc,
			   _gdk_win32_drawable_description ((GdkDrawable *) drawable),
			   xdest, ydest,
			   use_fg_bg));

  draw_impl = (GdkDrawableImplWin32 *) drawable;

  if (GDK_IS_DRAWABLE_IMPL_WIN32 (src))
    src_impl = (GdkDrawableImplWin32 *) src;
  else if (GDK_IS_WINDOW (src))
    src_impl = (GdkDrawableImplWin32 *) GDK_WINDOW_OBJECT (src)->impl;
  else if (GDK_IS_PIXMAP (src))
    src_impl = (GdkDrawableImplWin32 *) GDK_PIXMAP_OBJECT (src)->impl;
  else
    g_assert_not_reached ();

  hdc = gdk_win32_hdc_get ((GdkDrawable *) drawable, gc, GDK_GC_FOREGROUND);

  gdk_drawable_get_size (src, &src_width, &src_height);

  if ((src_rgn = CreateRectRgn (0, 0, src_width + 1, src_height + 1)) == NULL)
    WIN32_GDI_FAILED ("CreateRectRgn");
  else if ((draw_rgn = CreateRectRgn (xsrc, ysrc,
				      xsrc + width + 1,
				      ysrc + height + 1)) == NULL)
    WIN32_GDI_FAILED ("CreateRectRgn");
  else
    {
      if (GDK_IS_WINDOW_IMPL_WIN32 (draw_impl))
	{
	  int comb;
	  
	  /* If we are drawing on a window, calculate the region that is
	   * outside the source pixmap, and invalidate that, causing it to
	   * be cleared. Not completely sure whether this is always needed. XXX
	   */
	  SetRectEmpty (&r);
	  outside_rgn = CreateRectRgnIndirect (&r);
	  
	  if ((comb = CombineRgn (outside_rgn,
				  draw_rgn, src_rgn,
				  RGN_DIFF)) == ERROR)
	    WIN32_GDI_FAILED ("CombineRgn");
	  else if (comb != NULLREGION)
	    {
	      OffsetRgn (outside_rgn, xdest, ydest);
	      GDK_NOTE (MISC, (GetRgnBox (outside_rgn, &r),
			       g_print ("...calling InvalidateRgn, "
					"bbox: %ldx%ld@+%ld+%ld\n",
					r.right - r.left - 1, r.bottom - r.top - 1,
					r.left, r.top)));
	      InvalidateRgn (draw_impl->handle, outside_rgn, TRUE);
	    }
	  GDI_CALL (DeleteObject, (outside_rgn));
	}

#if 1 /* Don't know if this is necessary XXX */
      if (CombineRgn (draw_rgn, draw_rgn, src_rgn, RGN_AND) == COMPLEXREGION)
	g_warning ("gdk_win32_blit: CombineRgn returned a COMPLEXREGION");
      
      GetRgnBox (draw_rgn, &r);
      if (r.left != xsrc || r.top != ysrc ||
	  r.right != xsrc + width + 1 || r.bottom != ysrc + height + 1)
	{
	  xdest += r.left - xsrc;
	  xsrc = r.left;
	  ydest += r.top - ysrc;
	  ysrc = r.top;
	  width = r.right - xsrc - 1;
	  height = r.bottom - ysrc - 1;
	  
	  GDK_NOTE (MISC, g_print ("... restricted to src: %dx%d@+%d+%d, "
				   "dest: @+%d+%d\n",
				   width, height, xsrc, ysrc,
				   xdest, ydest));
	}
#endif

      GDI_CALL (DeleteObject, (src_rgn));
      GDI_CALL (DeleteObject, (draw_rgn));
    }

  if (GDK_IS_PIXMAP_IMPL_WIN32 (src_impl))
    blit_from_pixmap (use_fg_bg, draw_impl, hdc,
		      (GdkPixmapImplWin32 *) src_impl, GDK_GC_WIN32 (gc),
		      xsrc, ysrc, xdest, ydest, width, height);
  else if (draw_impl->handle == src_impl->handle)
    blit_inside_window (hdc, GDK_GC_WIN32 (gc), xsrc, ysrc, xdest, ydest, width, height);
  else
    blit_from_window (hdc, GDK_GC_WIN32 (gc), src_impl, xsrc, ysrc, xdest, ydest, width, height);
  gdk_win32_hdc_release ((GdkDrawable *) drawable, gc, GDK_GC_FOREGROUND);
}

static void
gdk_win32_draw_image (GdkDrawable     *drawable,
		      GdkGC           *gc,
		      GdkImage        *image,
		      gint             xsrc,
		      gint             ysrc,
		      gint             xdest,
		      gint             ydest,
		      gint             width,
		      gint             height)
{
  g_assert (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable));

  _gdk_win32_blit (TRUE, (GdkDrawableImplWin32 *) drawable,
		   gc, (GdkPixmap *) image->windowing_data,
		   xsrc, ysrc, xdest, ydest, width, height);
}

static gint
gdk_win32_get_depth (GdkDrawable *drawable)
{
  /* This is a bit bogus but I'm not sure the other way is better */

  return gdk_drawable_get_depth (GDK_DRAWABLE_IMPL_WIN32 (drawable)->wrapper);
}

static GdkScreen*
gdk_win32_get_screen (GdkDrawable *drawable)
{
  return gdk_screen_get_default ();
}
 
static GdkVisual*
gdk_win32_get_visual (GdkDrawable *drawable)
{
  return gdk_drawable_get_visual (GDK_DRAWABLE_IMPL_WIN32 (drawable)->wrapper);
}

HGDIOBJ
gdk_win32_drawable_get_handle (GdkDrawable *drawable)
{
  return GDK_DRAWABLE_HANDLE (drawable);
}
