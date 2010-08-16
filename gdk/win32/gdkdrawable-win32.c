/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 * Copyright (C) 1998-2004 Tor Lillqvist
 * Copyright (C) 2001-2005 Hans Breuer
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

#include "config.h"
#include <math.h>
#include <stdio.h>
#include <glib.h>

#include <pango/pangowin32.h>
#include <cairo-win32.h>

#include "gdkscreen.h" /* gdk_screen_get_default() */
#include "gdkprivate-win32.h"

#define ROP3_D 0x00AA0029
#define ROP3_DSna 0x00220326
#define ROP3_DSPDxax 0x00E20746

#define LINE_ATTRIBUTES (GDK_GC_LINE_WIDTH|GDK_GC_LINE_STYLE| \
			 GDK_GC_CAP_STYLE|GDK_GC_JOIN_STYLE)

#define MUST_RENDER_DASHES_MANUALLY(gcwin32)			\
  (gcwin32->line_style == GDK_LINE_DOUBLE_DASH ||		\
   (gcwin32->line_style == GDK_LINE_ON_OFF_DASH && gcwin32->pen_dash_offset))

static cairo_surface_t *gdk_win32_ref_cairo_surface (GdkDrawable *drawable);
     
static void gdk_win32_set_colormap   (GdkDrawable    *drawable,
				      GdkColormap    *colormap);

static GdkColormap* gdk_win32_get_colormap   (GdkDrawable    *drawable);

static gint         gdk_win32_get_depth      (GdkDrawable    *drawable);

static GdkScreen *  gdk_win32_get_screen     (GdkDrawable    *drawable);

static GdkVisual*   gdk_win32_get_visual     (GdkDrawable    *drawable);

static void gdk_drawable_impl_win32_finalize   (GObject *object);

static const cairo_user_data_key_t gdk_win32_cairo_key;

G_DEFINE_TYPE (GdkDrawableImplWin32,  _gdk_drawable_impl_win32, GDK_TYPE_DRAWABLE)


static void
_gdk_drawable_impl_win32_class_init (GdkDrawableImplWin32Class *klass)
{
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_drawable_impl_win32_finalize;

  drawable_class->create_gc = _gdk_win32_gc_new;
  
  drawable_class->ref_cairo_surface = gdk_win32_ref_cairo_surface;
  
  drawable_class->set_colormap = gdk_win32_set_colormap;
  drawable_class->get_colormap = gdk_win32_get_colormap;

  drawable_class->get_depth = gdk_win32_get_depth;
  drawable_class->get_screen = gdk_win32_get_screen;
  drawable_class->get_visual = gdk_win32_get_visual;
}

static void
_gdk_drawable_impl_win32_init (GdkDrawableImplWin32 *impl)
{
}

static void
gdk_drawable_impl_win32_finalize (GObject *object)
{
  gdk_drawable_set_colormap (GDK_DRAWABLE (object), NULL);

  G_OBJECT_CLASS (_gdk_drawable_impl_win32_parent_class)->finalize (object);
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
    g_object_unref (impl->colormap);
  impl->colormap = colormap;
  if (impl->colormap)
    g_object_ref (impl->colormap);
}

/* Drawing
 */

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

static int
rop2_to_patblt_rop (int rop2)
{
  switch (rop2)
    {
#define CASE(rop2,patblt_rop) case R2_##rop2: return patblt_rop
      CASE (COPYPEN, PATCOPY);
      CASE (XORPEN, PATINVERT);
      CASE (NOT, DSTINVERT);
      CASE (BLACK, BLACKNESS);
      CASE (WHITE, WHITENESS);
#undef CASE
    default:
      g_warning ("Unhandled rop2 in GC to be used in PatBlt: %#x", rop2);
      return PATCOPY;
    }
}

static inline int
align_with_dash_offset (int a, DWORD *dashes, int num_dashes, GdkGCWin32 *gcwin32)
{
  int	   n = 0;
  int    len_sum = 0;
  /* 
   * We can't simply add the dashoffset, it can be an arbitrary larger
   * or smaller value not even between x1 and x2. It just says use the
   * dash pattern aligned to the offset. So ensure x1 is smaller _x1
   * and we start with the appropriate dash.
   */
  for (n = 0; n < num_dashes; n++)
    len_sum += dashes[n];
  if (   len_sum > 0 /* pathological api usage? */
      && gcwin32->pen_dash_offset > a)
    a -= (((gcwin32->pen_dash_offset/len_sum - a/len_sum) + 1) * len_sum);
  else
    a = gcwin32->pen_dash_offset;

  return a;
}
 
/* Render a dashed line 'by hand'. Used for all dashes on Win9x (where
 * GDI is way too limited), and for double dashes on all Windowses.
 */
static inline gboolean
render_line_horizontal (GdkGCWin32 *gcwin32,
                        int         x1,
                        int         x2,
                        int         y)
{
  int n = 0;
  const int pen_width = MAX (gcwin32->pen_width, 1);
  const int _x1 = x1;

  g_assert (gcwin32->pen_dashes);

  x1 = align_with_dash_offset (x1, gcwin32->pen_dashes, gcwin32->pen_num_dashes, gcwin32);

  for (n = 0; x1 < x2; n++)
    {
      int len = gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
      if (x1 + len > x2)
        len = x2 - x1;

      if (n % 2 == 0 && x1 + len > _x1)
        if (!GDI_CALL (PatBlt, (gcwin32->hdc, 
				x1 < _x1 ? _x1 : x1, 
				y - pen_width / 2, 
				len, pen_width, 
				rop2_to_patblt_rop (gcwin32->rop2))))
	  return FALSE;

      x1 += gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
    }

  if (gcwin32->line_style == GDK_LINE_DOUBLE_DASH)
    {
      HBRUSH hbr;

      if ((hbr = SelectObject (gcwin32->hdc, gcwin32->pen_hbrbg)) == HGDI_ERROR)
	return FALSE;
      x1 = _x1;
      x1 += gcwin32->pen_dash_offset;
      for (n = 0; x1 < x2; n++)
	{
	  int len = gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
	  if (x1 + len > x2)
	    len = x2 - x1;

	  if (n % 2)
	    if (!GDI_CALL (PatBlt, (gcwin32->hdc, x1, y - pen_width / 2,
				    len, pen_width,
				    rop2_to_patblt_rop (gcwin32->rop2))))
	      return FALSE;

	  x1 += gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
	}
      if (SelectObject (gcwin32->hdc, hbr) == HGDI_ERROR)
	return FALSE;
    }

  return TRUE;
}

static inline gboolean
render_line_vertical (GdkGCWin32 *gcwin32,
		      int         x,
                      int         y1,
                      int         y2)
{
  int n;
  const int pen_width = MAX (gcwin32->pen_width, 1);
  const int _y1 = y1;

  g_assert (gcwin32->pen_dashes);

  y1 = align_with_dash_offset (y1, gcwin32->pen_dashes, gcwin32->pen_num_dashes, gcwin32);
  for (n = 0; y1 < y2; n++)
    {
      int len = gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
      if (y1 + len > y2)
        len = y2 - y1;
      if (n % 2 == 0 && y1 + len > _y1)
        if (!GDI_CALL (PatBlt, (gcwin32->hdc, x - pen_width / 2, 
				y1 < _y1 ? _y1 : y1, 
				pen_width, len, 
				rop2_to_patblt_rop (gcwin32->rop2))))
	  return FALSE;

      y1 += gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
    }

  if (gcwin32->line_style == GDK_LINE_DOUBLE_DASH)
    {
      HBRUSH hbr;

      if ((hbr = SelectObject (gcwin32->hdc, gcwin32->pen_hbrbg)) == HGDI_ERROR)
	return FALSE;
      y1 = _y1;
      y1 += gcwin32->pen_dash_offset;
      for (n = 0; y1 < y2; n++)
	{
	  int len = gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
	  if (y1 + len > y2)
	    len = y2 - y1;
	  if (n % 2)
	    if (!GDI_CALL (PatBlt, (gcwin32->hdc, x - pen_width / 2, y1,
				    pen_width, len,
				    rop2_to_patblt_rop (gcwin32->rop2))))
	      return FALSE;

	  y1 += gcwin32->pen_dashes[n % gcwin32->pen_num_dashes];
	}
      if (SelectObject (gcwin32->hdc, hbr) == HGDI_ERROR)
	return FALSE;
    }

  return TRUE;
}

static cairo_region_t *
widen_bounds (GdkRectangle *bounds,
	      gint          pen_width)
{
  if (pen_width == 0)
    pen_width = 1;

  bounds->x -= pen_width;
  bounds->y -= pen_width;
  bounds->width += 2 * pen_width;
  bounds->height += 2 * pen_width;

  return cairo_region_create_rectangle (bounds);
}

static void
blit_from_pixmap (gboolean              use_fg_bg,
		  GdkDrawableImplWin32 *dest,
		  HDC                   hdc,
		  GdkPixmapImplWin32   *src,
		  GdkGC                *gc,
		  gint         	      	xsrc,
		  gint         	      	ysrc,
		  gint         	      	xdest,
		  gint         	      	ydest,
		  gint         	      	width,
		  gint         	      	height)
{
  GdkGCWin32 *gcwin32 = GDK_GC_WIN32 (gc);
  HDC srcdc;
  HBITMAP holdbitmap;
  RGBQUAD oldtable[256], newtable[256];
  COLORREF bg, fg;

  gint newtable_size = 0, oldtable_size = 0;
  gboolean ok = TRUE;
  
  GDK_NOTE (DRAW, g_print ("blit_from_pixmap\n"));

  srcdc = _gdk_win32_drawable_acquire_dc (GDK_DRAWABLE (src));
  if (!srcdc)
    return;
  
  if (!(holdbitmap = SelectObject (srcdc, ((GdkDrawableImplWin32 *) src)->handle)))
    WIN32_GDI_FAILED ("SelectObject");
  else
    {
      if (GDK_PIXMAP_OBJECT (src->parent_instance.wrapper)->depth <= 8)
	{
	  /* Blitting from a 1, 4 or 8-bit pixmap */

	  if ((oldtable_size = GetDIBColorTable (srcdc, 0, 256, oldtable)) == 0)
	    WIN32_GDI_FAILED ("GetDIBColorTable");
	  else if (GDK_PIXMAP_OBJECT (src->parent_instance.wrapper)->depth == 1)
	    {
	      /* Blitting from an 1-bit pixmap */

	      gint bgix, fgix;
	      
	      if (use_fg_bg)
		{
		  bgix = _gdk_gc_get_bg_pixel (gc);
		  fgix = _gdk_gc_get_fg_pixel (gc);
		}
	      else
		{
		  bgix = 0;
		  fgix = 1;
		}
	      
	      if (GDK_IS_PIXMAP_IMPL_WIN32 (dest) &&
		  GDK_PIXMAP_OBJECT (dest->wrapper)->depth <= 8)
		{
		  /* Destination is also pixmap, get fg and bg from
		   * its palette. Either use the foreground and
		   * background pixel values in the GC, or 0
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
		GDK_NOTE (DRAW, g_print ("bg: %02x %02x %02x "
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
	if (!BitBlt (hdc, xdest, ydest, width, height,
		     srcdc, xsrc, ysrc, rop2_to_rop3 (gcwin32->rop2)) &&
	    GetLastError () != ERROR_INVALID_HANDLE)
	  WIN32_GDI_FAILED ("BitBlt");
      
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
  
  _gdk_win32_drawable_release_dc (GDK_DRAWABLE (src));
}

static void
blit_inside_drawable (HDC      	hdc,
		      GdkGCWin32 *gcwin32,
		      gint     	xsrc,
		      gint     	ysrc,
		      gint     	xdest,
		      gint     	ydest,
		      gint     	width,
		      gint     	height)

{
  GDK_NOTE (DRAW, g_print ("blit_inside_drawable\n"));

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

  GDK_NOTE (DRAW, g_print ("blit_from_window\n"));

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

/**
 * _gdk_win32_drawable_acquire_dc
 * @drawable: a Win32 #GdkDrawable implementation
 * 
 * Gets a DC with the given drawable selected into
 * it.
 *
 * Return value: The DC, on success. Otherwise
 *  %NULL. If this function succeeded
 *  _gdk_win32_drawable_release_dc()  must be called
 *  release the DC when you are done using it.
 **/
HDC 
_gdk_win32_drawable_acquire_dc (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  
  if (GDK_IS_WINDOW_IMPL_WIN32 (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->hdc)
    {
      if (GDK_IS_PIXMAP_IMPL_WIN32 (impl))
	{
	  impl->hdc = CreateCompatibleDC (NULL);
	  if (!impl->hdc)
	    WIN32_GDI_FAILED ("CreateCompatibleDC");
	  
	  if (impl->hdc)
	    {
	      impl->saved_dc_bitmap = SelectObject (impl->hdc,
						    impl->handle);
	      if (!impl->saved_dc_bitmap)
		{
		  WIN32_GDI_FAILED ("CreateCompatibleDC");
		  DeleteDC (impl->hdc);
		  impl->hdc = NULL;
		}
	    }
	}
      else
	{
	  impl->hdc = GetDC (impl->handle);
	  if (!impl->hdc)
	    WIN32_GDI_FAILED ("GetDC");
	}
    }

  if (impl->hdc)
    {
      impl->hdc_count++;
      return impl->hdc;
    }
  else
    {
      return NULL;
    }
}

/**
 * _gdk_win32_drawable_release_dc
 * @drawable: a Win32 #GdkDrawable implementation
 * 
 * Releases the reference count for the DC
 * from _gdk_win32_drawable_acquire_dc()
 **/
void
_gdk_win32_drawable_release_dc (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);
  
  g_return_if_fail (impl->hdc_count > 0);

  impl->hdc_count--;
  if (impl->hdc_count == 0)
    {
      if (impl->saved_dc_bitmap)
	{
	  GDI_CALL (SelectObject, (impl->hdc, impl->saved_dc_bitmap));
	  impl->saved_dc_bitmap = NULL;
	}
      
      if (impl->hdc)
	{
	  if (GDK_IS_PIXMAP_IMPL_WIN32 (impl))
	    GDI_CALL (DeleteDC, (impl->hdc));
	  else
	    GDI_CALL (ReleaseDC, (impl->handle, impl->hdc));
	  impl->hdc = NULL;
	}
    }
}

cairo_surface_t *
_gdk_windowing_create_cairo_surface (GdkDrawable *drawable,
				     gint width,
				     gint height)
{
  /* width and height are determined from the DC */
  return gdk_win32_ref_cairo_surface (drawable);
}

static void
gdk_win32_cairo_surface_destroy (void *data)
{
  GdkDrawableImplWin32 *impl = data;

  _gdk_win32_drawable_release_dc (GDK_DRAWABLE (impl));
  impl->cairo_surface = NULL;
}

static cairo_surface_t *
gdk_win32_ref_cairo_surface (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  if (GDK_IS_WINDOW_IMPL_WIN32 (drawable) &&
      GDK_WINDOW_DESTROYED (impl->wrapper))
    return NULL;

  if (!impl->cairo_surface)
    {
      HDC hdc = _gdk_win32_drawable_acquire_dc (drawable);
      if (!hdc)
	return NULL;

      impl->cairo_surface = cairo_win32_surface_create (hdc);

      cairo_surface_set_user_data (impl->cairo_surface, &gdk_win32_cairo_key,
				   drawable, gdk_win32_cairo_surface_destroy);
    }
  else
    cairo_surface_reference (impl->cairo_surface);

  return impl->cairo_surface;
}

gboolean
_gdk_windowing_set_cairo_surface_size (cairo_surface_t *surface,
				       gint width,
				       gint height)
{
  // Do nothing.  The surface size is determined by the DC
  return FALSE;
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

/**
 * _gdk_win32_drawable_finish
 * @drawable: a Win32 #GdkDrawable implementation
 * 
 * Releases any resources allocated internally for the drawable.
 * This is called when the drawable becomes unusable
 * (gdk_window_destroy() for a window, or the refcount going to
 * zero for a pixmap.)
 **/
void
_gdk_win32_drawable_finish (GdkDrawable *drawable)
{
  GdkDrawableImplWin32 *impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  if (impl->cairo_surface)
    {
      cairo_surface_finish (impl->cairo_surface);
      cairo_surface_set_user_data (impl->cairo_surface, &gdk_win32_cairo_key, NULL, NULL);
    }

  g_assert (impl->hdc_count == 0);
}

