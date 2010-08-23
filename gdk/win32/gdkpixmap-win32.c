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

#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "gdkpixmap.h"
#include "gdkdisplay.h"
#include "gdkscreen.h"

#include "gdkprivate-win32.h"

static void gdk_pixmap_impl_win32_get_size   (GdkDrawable        *drawable,
					      gint               *width,
					      gint               *height);

static void gdk_pixmap_impl_win32_init       (GdkPixmapImplWin32      *pixmap);
static void gdk_pixmap_impl_win32_class_init (GdkPixmapImplWin32Class *klass);
static void gdk_pixmap_impl_win32_finalize   (GObject                 *object);

static gpointer parent_class = NULL;

GType
_gdk_pixmap_impl_win32_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkPixmapImplWin32Class),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_pixmap_impl_win32_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkPixmapImplWin32),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_pixmap_impl_win32_init,
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_WIN32,
                                            "GdkPixmapImplWin32",
                                            &object_info, 0);
    }
  
  return object_type;
}

GType
_gdk_pixmap_impl_get_type (void)
{
  return _gdk_pixmap_impl_win32_get_type ();
}

static void
gdk_pixmap_impl_win32_init (GdkPixmapImplWin32 *impl)
{
  impl->width = 1;
  impl->height = 1;
}

static void
gdk_pixmap_impl_win32_class_init (GdkPixmapImplWin32Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_pixmap_impl_win32_finalize;

  drawable_class->get_size = gdk_pixmap_impl_win32_get_size;
}

static void
gdk_pixmap_impl_win32_finalize (GObject *object)
{
  GdkPixmapImplWin32 *impl = GDK_PIXMAP_IMPL_WIN32 (object);
  GdkPixmap *wrapper = GDK_PIXMAP (GDK_DRAWABLE_IMPL_WIN32 (impl)->wrapper);

  GDK_NOTE (PIXMAP, g_print ("gdk_pixmap_impl_win32_finalize: %p\n",
			     GDK_PIXMAP_HBITMAP (wrapper)));

  _gdk_win32_drawable_finish (GDK_DRAWABLE (object));  

  GDI_CALL (DeleteObject, (GDK_PIXMAP_HBITMAP (wrapper)));

  gdk_win32_handle_table_remove (GDK_PIXMAP_HBITMAP (wrapper));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_pixmap_impl_win32_get_size (GdkDrawable *drawable,
				gint        *width,
				gint        *height)
{
  if (width)
    *width = GDK_PIXMAP_IMPL_WIN32 (drawable)->width;
  if (height)
    *height = GDK_PIXMAP_IMPL_WIN32 (drawable)->height;
}

static int
bits_for_depth (gint depth)
{
  switch (depth)
    {
    case 1:
      return 1;

    case 2:
    case 3:
    case 4:
      return 4;

    case 5:
    case 6:
    case 7:
    case 8:
      return 8;

    case 15:
    case 16:
      return 16;

    case 24:
    case 32:
      return 32;
    }
  g_assert_not_reached ();
  return 0;
}

GdkPixmap*
_gdk_pixmap_new (GdkDrawable *drawable,
		gint         width,
		gint         height,
		gint         depth)
{
  struct {
    BITMAPINFOHEADER bmiHeader;
    union {
      WORD bmiIndices[256];
      DWORD bmiMasks[3];
      RGBQUAD bmiColors[256];
    } u;
  } bmi;
  UINT iUsage;
  HDC hdc;
  HWND hwnd;
  HPALETTE holdpal = NULL;
  HBITMAP hbitmap;
  GdkPixmap *pixmap;
  GdkDrawableImplWin32 *drawable_impl;
  GdkPixmapImplWin32 *pixmap_impl;
  GdkColormap *cmap;
  guchar *bits;
  gint i;
  gint window_depth;

  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail ((drawable != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (!drawable)
    drawable = _gdk_root;

  if (GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  window_depth = gdk_drawable_get_depth (GDK_DRAWABLE (drawable));
  if (depth == -1)
    depth = window_depth;

  GDK_NOTE (PIXMAP, g_print ("gdk_pixmap_new: %dx%dx%d drawable=%p\n",
			     width, height, depth, drawable));

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  drawable_impl = GDK_DRAWABLE_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pixmap_impl = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  drawable_impl->wrapper = GDK_DRAWABLE (pixmap);
  
  pixmap_impl->is_foreign = FALSE;
  pixmap_impl->width = width;
  pixmap_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;

  if (depth == window_depth)
    {
      cmap = gdk_drawable_get_colormap (drawable);
      if (cmap)
        gdk_drawable_set_colormap (pixmap, cmap);
    }
  
  if (GDK_IS_WINDOW (drawable))
    hwnd = GDK_WINDOW_HWND (drawable);
  else
    hwnd = GetDesktopWindow ();
  if ((hdc = GetDC (hwnd)) == NULL)
    {
      WIN32_GDI_FAILED ("GetDC");
      g_object_unref ((GObject *) pixmap);
      return NULL;
    }

  bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;
  bmi.bmiHeader.biPlanes = 1;
  switch (depth)
    {
    case 1:
    case 24:
    case 32:
      bmi.bmiHeader.biBitCount = bits_for_depth (depth);
      break;

    case 4:
      bmi.bmiHeader.biBitCount = 4;
      break;
      
    case 5:
    case 6:
    case 7:
    case 8:
      bmi.bmiHeader.biBitCount = 8;
      break;
      
    case 15:
    case 16:
      bmi.bmiHeader.biBitCount = 16;
      break;

    default:
      g_warning ("gdk_win32_pixmap_new: depth = %d", depth);
      g_assert_not_reached ();
    }

  if (bmi.bmiHeader.biBitCount == 16)
    bmi.bmiHeader.biCompression = BI_BITFIELDS;
  else
    bmi.bmiHeader.biCompression = BI_RGB;

  bmi.bmiHeader.biSizeImage = 0;
  bmi.bmiHeader.biXPelsPerMeter =
    bmi.bmiHeader.biYPelsPerMeter = 0;
  bmi.bmiHeader.biClrUsed = 0;
  bmi.bmiHeader.biClrImportant = 0;

  iUsage = DIB_RGB_COLORS;
  if (depth == 1)
    {
      bmi.u.bmiColors[0].rgbBlue =
	bmi.u.bmiColors[0].rgbGreen =
	bmi.u.bmiColors[0].rgbRed = 0x00;
      bmi.u.bmiColors[0].rgbReserved = 0x00;

      bmi.u.bmiColors[1].rgbBlue =
	bmi.u.bmiColors[1].rgbGreen =
	bmi.u.bmiColors[1].rgbRed = 0xFF;
      bmi.u.bmiColors[1].rgbReserved = 0x00;
    }
  else
    {
      if (depth <= 8 && drawable_impl->colormap != NULL)
	{
	  GdkColormapPrivateWin32 *cmapp =
	    GDK_WIN32_COLORMAP_DATA (drawable_impl->colormap);
	  gint k;

	  if ((holdpal = SelectPalette (hdc, cmapp->hpal, FALSE)) == NULL)
	    WIN32_GDI_FAILED ("SelectPalette");
	  else if ((k = RealizePalette (hdc)) == GDI_ERROR)
	    WIN32_GDI_FAILED ("RealizePalette");
	  else if (k > 0)
	    GDK_NOTE (PIXMAP_OR_COLORMAP, g_print ("_gdk_win32_pixmap_new: realized %p: %d colors\n",
						   cmapp->hpal, k));

	  iUsage = DIB_PAL_COLORS;
	  for (i = 0; i < 256; i++)
	    bmi.u.bmiIndices[i] = i;
	}
      else if (bmi.bmiHeader.biBitCount == 16)
	{
	  GdkVisual *visual = gdk_visual_get_system ();

	  bmi.u.bmiMasks[0] = visual->red_mask;
	  bmi.u.bmiMasks[1] = visual->green_mask;
	  bmi.u.bmiMasks[2] = visual->blue_mask;
	}
    }

  hbitmap = CreateDIBSection (hdc, (BITMAPINFO *) &bmi,
			      iUsage, (PVOID *) &bits, NULL, 0);
  if (holdpal != NULL)
    SelectPalette (hdc, holdpal, FALSE);

  GDI_CALL (ReleaseDC, (hwnd, hdc));

  GDK_NOTE (PIXMAP, g_print ("... =%p bits=%p pixmap=%p\n", hbitmap, bits, pixmap));

  if (hbitmap == NULL)
    {
      WIN32_GDI_FAILED ("CreateDIBSection");
      g_object_unref ((GObject *) pixmap);
      return NULL;
    }

  drawable_impl->handle = hbitmap;
  pixmap_impl->bits = bits;

  gdk_win32_handle_table_insert (&GDK_PIXMAP_HBITMAP (pixmap), pixmap);

  return pixmap;
}

GdkPixmap *
gdk_pixmap_foreign_new_for_display (GdkDisplay      *display,
				    GdkNativeWindow  anid)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (display == _gdk_display, NULL);

  return gdk_pixmap_foreign_new (anid);
}

GdkPixmap *
gdk_pixmap_foreign_new_for_screen (GdkScreen       *screen,
				   GdkNativeWindow  anid,
				   gint             width,
				   gint             height,
				   gint             depth)
{
  g_return_val_if_fail (GDK_IS_SCREEN (screen), NULL);

  return gdk_pixmap_foreign_new (anid);
}

GdkPixmap*
gdk_pixmap_foreign_new (GdkNativeWindow anid)
{
  GdkPixmap *pixmap;
  GdkDrawableImplWin32 *draw_impl;
  GdkPixmapImplWin32 *pix_impl;
  HBITMAP hbitmap;
  SIZE size;

  /* Check to make sure we were passed a HBITMAP */
  g_return_val_if_fail (GetObjectType ((HGDIOBJ) anid) == OBJ_BITMAP, NULL);

  hbitmap = (HBITMAP) anid;

  /* Get information about the bitmap to fill in the structure for the
   * GDK window.
   */
  GetBitmapDimensionEx (hbitmap, &size);

  /* Allocate a new GDK pixmap */
  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_WIN32 (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);
  
  draw_impl->handle = hbitmap;
  draw_impl->colormap = NULL;
  pix_impl->width = size.cx;
  pix_impl->height = size.cy;
  pix_impl->bits = NULL;

  gdk_win32_handle_table_insert (&GDK_PIXMAP_HBITMAP (pixmap), pixmap);

  return pixmap;
}

GdkPixmap*
gdk_pixmap_lookup (GdkNativeWindow anid)
{
  return (GdkPixmap*) gdk_win32_handle_table_lookup (anid);
}

GdkPixmap*
gdk_pixmap_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (display == _gdk_display, NULL);

  return gdk_pixmap_lookup (anid);
}
