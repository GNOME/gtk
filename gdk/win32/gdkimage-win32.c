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

#include "gdkimage.h"
#include "gdkpixmap.h"
#include "gdkprivate-win32.h"

static GList *image_list = NULL;
static gpointer parent_class = NULL;

static void gdk_win32_image_destroy (GdkImage      *image);
static void gdk_image_init          (GdkImage      *image);
static void gdk_image_class_init    (GdkImageClass *klass);
static void gdk_image_finalize      (GObject       *object);

GType
gdk_image_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkImageClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_image_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkImage),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_image_init,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkImage",
                                            &object_info, 0);
    }
  
  return object_type;
}

static void
gdk_image_init (GdkImage *image)
{
  image->windowing_data = g_new0 (GdkImagePrivateWin32, 1);
}

static void
gdk_image_class_init (GdkImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_image_finalize;
}

static void
gdk_image_finalize (GObject *object)
{
  GdkImage *image = GDK_IMAGE (object);

  gdk_win32_image_destroy (image);
  
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

void
_gdk_image_exit (void)
{
  GdkImage *image;

  while (image_list)
    {
      image = image_list->data;
      gdk_win32_image_destroy (image);
    }
}

GdkImage *
gdk_image_new_bitmap (GdkVisual *visual,
		      gpointer   data,
		      gint       w,
		      gint       h)
/*
 * Desc: create a new bitmap image
 */
{
  Visual *xvisual;
  GdkImage *image;
  GdkImagePrivateWin32 *private;
  struct {
    BITMAPINFOHEADER bmiHeader;
    union {
      WORD bmiIndices[2];
      RGBQUAD bmiColors[2];
    } u;
  } bmi;
  char *bits;
  int bpl = (w-1)/8 + 1;
  int bpl32 = ((w-1)/32 + 1)*4;

  image = g_object_new (gdk_image_get_type (), NULL);
  private = IMAGE_PRIVATE_DATA (image);

  image->type = GDK_IMAGE_SHARED;
  image->visual = visual;
  image->width = w;
  image->height = h;
  image->depth = 1;
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;

  GDK_NOTE (MISC, g_print ("gdk_image_new_bitmap: %dx%d\n", w, h));
  
  bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 1;
  bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage = 0;
  bmi.bmiHeader.biXPelsPerMeter =
    bmi.bmiHeader.biYPelsPerMeter = 0;
  bmi.bmiHeader.biClrUsed = 0;
  bmi.bmiHeader.biClrImportant = 0;
  
  bmi.u.bmiColors[0].rgbBlue = 
    bmi.u.bmiColors[0].rgbGreen = 
    bmi.u.bmiColors[0].rgbRed = 0x00;
  bmi.u.bmiColors[0].rgbReserved = 0x00;

  bmi.u.bmiColors[1].rgbBlue = 
    bmi.u.bmiColors[1].rgbGreen = 
    bmi.u.bmiColors[1].rgbRed = 0xFF;
  bmi.u.bmiColors[1].rgbReserved = 0x00;
  
  private->hbitmap = CreateDIBSection (gdk_display_hdc, (BITMAPINFO *) &bmi,
				       DIB_RGB_COLORS, &bits, NULL, 0);
  if (bpl != bpl32)
    {
      /* Win32 expects scanlines in DIBs to be 32 bit aligned */
      int i;
      for (i = 0; i < h; i++)
	memmove (bits + i*bpl32, ((char *) data) + i*bpl, bpl);
    }
  else
    memmove (bits, data, bpl*h);
  image->mem = bits;
  image->bpl = bpl32;
  image->byte_order = GDK_MSB_FIRST;

  image->bits_per_pixel = 1;
  image->bpp = 1;

  /* free data right now, in contrast to the X11 version we have made
   * our own copy. Use free, it was malloc()ed.
   */
  free (data);
  return(image);
} /* gdk_image_new_bitmap() */

void
_gdk_windowing_image_init (void)
{
  /* Nothing needed AFAIK */
}

GdkImage*
gdk_image_new (GdkImageType  type,
	       GdkVisual    *visual,
	       gint          width,
	       gint          height)
{
  GdkImage *image;
  GdkImagePrivateWin32 *private;
  Visual *xvisual;
  struct {
    BITMAPINFOHEADER bmiHeader;
    union {
      WORD bmiIndices[256];
      DWORD bmiMasks[3];
      RGBQUAD bmiColors[256];
    } u;
  } bmi;
  UINT iUsage;
  int i;

  if (type == GDK_IMAGE_FASTEST || type == GDK_IMAGE_NORMAL)
    type = GDK_IMAGE_SHARED;

  GDK_NOTE (MISC, g_print ("gdk_image_new: %dx%d %s\n",
			   width, height,
			   (type == GDK_IMAGE_SHARED ? "shared" :
			    "???")));
  
  image = g_object_new (gdk_image_get_type (), NULL);
  private = IMAGE_PRIVATE_DATA (image);

  image->type = type;
  image->visual = visual;
  image->width = width;
  image->height = height;
  image->depth = visual->depth;
  
  xvisual = ((GdkVisualPrivate*) visual)->xvisual;
  
  bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;
  bmi.bmiHeader.biPlanes = 1;
  if (image->depth == 15)
    bmi.bmiHeader.biBitCount = 16;
  else
    bmi.bmiHeader.biBitCount = image->depth;
  if (image->depth == 16)
    bmi.bmiHeader.biCompression = BI_BITFIELDS;
  else
    bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage = 0;
  bmi.bmiHeader.biXPelsPerMeter =
    bmi.bmiHeader.biYPelsPerMeter = 0;
  bmi.bmiHeader.biClrUsed = 0;
  bmi.bmiHeader.biClrImportant = 0;

  if (image->visual->type == GDK_VISUAL_PSEUDO_COLOR)
    {
      iUsage = DIB_PAL_COLORS;
      for (i = 0; i < 256; i++)
	bmi.u.bmiIndices[i] = i;
    }
  else
    {
      iUsage = DIB_RGB_COLORS;
      if (image->depth == 1)
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
      else if (image->depth == 16)
	{
	  bmi.u.bmiMasks[0] = visual->red_mask;
	  bmi.u.bmiMasks[1] = visual->green_mask;
	  bmi.u.bmiMasks[2] = visual->blue_mask;
	}
    }

  private->hbitmap = CreateDIBSection (gdk_display_hdc, (BITMAPINFO *) &bmi,
				       iUsage, &image->mem, NULL, 0);

  if (private->hbitmap == NULL)
    {
      WIN32_GDI_FAILED ("CreateDIBSection");
      g_free (image);
      return NULL;
    }

  switch (image->depth)
    {
    case 1:
    case 8:
      image->bpp = 1;
      break;
    case 15:
    case 16:
      image->bpp = 2;
      break;
    case 24:
      image->bpp = 3;
      break;
    case 32:
      image->bpp = 4;
      break;
    default:
      g_warning ("gdk_image_new: depth = %d", image->depth);
      g_assert_not_reached ();
    }
  image->bits_per_pixel = image->depth;
  image->byte_order = GDK_LSB_FIRST;
  if (image->depth == 1)
    image->bpl = ((width-1)/32 + 1)*4;
  else
    image->bpl = ((width*image->bpp - 1)/4 + 1)*4;

  GDK_NOTE (MISC, g_print ("... = %#x mem = %p, bpl = %d\n",
			   (guint) private->hbitmap, image->mem, image->bpl));

  return image;
}

GdkImage*
_gdk_win32_get_image (GdkDrawable *drawable,
		      gint         x,
		      gint         y,
		      gint         width,
		      gint         height)
{
  GdkImage *image;
  GdkImagePrivateWin32 *private;
  GdkDrawableImplWin32 *impl;
  HDC hdc, memdc;
  struct {
    BITMAPINFOHEADER bmiHeader;
    union {
      WORD bmiIndices[256];
      DWORD bmiMasks[3];
      RGBQUAD bmiColors[256];
    } u;
  } bmi;
  HGDIOBJ oldbitmap1 = NULL, oldbitmap2;
  UINT iUsage;
  BITMAP bm;
  int i;

  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_WIN32 (drawable), NULL);

  GDK_NOTE (MISC, g_print ("_gdk_win32_get_image: %#x %dx%d@+%d+%d\n",
			   (guint) GDK_DRAWABLE_IMPL_WIN32 (drawable)->handle,
			   width, height, x, y));

  impl = GDK_DRAWABLE_IMPL_WIN32 (drawable);

  image = g_object_new (gdk_image_get_type (), NULL);
  private = IMAGE_PRIVATE_DATA (image);

  image->type = GDK_IMAGE_SHARED;
  image->visual = gdk_drawable_get_visual (drawable);
  image->width = width;
  image->height = height;

  /* This function is called both to blit from a window and from
   * a pixmap.
   */
  if (GDK_IS_PIXMAP_IMPL_WIN32 (drawable))
    {
      if ((hdc = CreateCompatibleDC (NULL)) == NULL)
	{
	  WIN32_GDI_FAILED ("CreateCompatibleDC");
	  g_free (image);
	  return NULL;
	}
      if ((oldbitmap1 = SelectObject (hdc, impl->handle)) == NULL)
	{
	  WIN32_GDI_FAILED ("SelectObject");
	  DeleteDC (hdc);
	  g_free (image);
	  return NULL;
	}
      GetObject (impl->handle, sizeof (BITMAP), &bm);
      GDK_NOTE (MISC,
		g_print ("gdk_image_get: bmWidth:%ld bmHeight:%ld bmWidthBytes:%ld bmBitsPixel:%d\n",
			 bm.bmWidth, bm.bmHeight, bm.bmWidthBytes, bm.bmBitsPixel));
      image->depth = bm.bmBitsPixel;
      if (image->depth <= 8)
	{
	  iUsage = DIB_PAL_COLORS;
	  for (i = 0; i < 256; i++)
	    bmi.u.bmiIndices[i] = i;
	}
      else
	iUsage = DIB_RGB_COLORS;
    }
  else
    {
      if ((hdc = GetDC (impl->handle)) == NULL)
	{
	  WIN32_GDI_FAILED ("GetDC");
	  g_free (image);
	  return NULL;
	}
      image->depth = gdk_visual_get_system ()->depth;
      if (image->visual->type == GDK_VISUAL_PSEUDO_COLOR)
	{
	  iUsage = DIB_PAL_COLORS;
	  for (i = 0; i < 256; i++)
	    bmi.u.bmiIndices[i] = i;
	}
      else
	iUsage = DIB_RGB_COLORS;
    }

  if ((memdc = CreateCompatibleDC (hdc)) == NULL)
    {
      WIN32_GDI_FAILED ("CreateCompatibleDC");
      if (GDK_IS_PIXMAP_IMPL_WIN32 (drawable))
	{
	  SelectObject (hdc, oldbitmap1);
	  if (!DeleteDC (hdc))
	    WIN32_GDI_FAILED ("DeleteDC");
	}
      else
	{
	  if (!ReleaseDC (impl->handle, hdc))
	    WIN32_GDI_FAILED ("ReleaseDC");
	}
      g_free (image);
      return NULL;
    }

  bmi.bmiHeader.biSize = sizeof (BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = image->depth;
  if (image->depth == 16)
    {
      bmi.bmiHeader.biCompression = BI_BITFIELDS;
      if (image->visual == NULL)
	{
	  /* XXX ??? Is it always this if depth==16 and a pixmap? Guess so. */
	  bmi.u.bmiMasks[0] = 0xf800;
	  bmi.u.bmiMasks[1] = 0x07e0;
	  bmi.u.bmiMasks[2] = 0x001f;
	}
      else
	{
	  bmi.u.bmiMasks[0] = image->visual->red_mask;
	  bmi.u.bmiMasks[1] = image->visual->green_mask;
	  bmi.u.bmiMasks[2] = image->visual->blue_mask;
	}
    }
  else
    bmi.bmiHeader.biCompression = BI_RGB;
  bmi.bmiHeader.biSizeImage = 0;
  bmi.bmiHeader.biXPelsPerMeter =
    bmi.bmiHeader.biYPelsPerMeter = 0;
  bmi.bmiHeader.biClrUsed = 0;
  bmi.bmiHeader.biClrImportant = 0;

  if ((private->hbitmap = CreateDIBSection (hdc, (BITMAPINFO *) &bmi, iUsage,
					    &image->mem, NULL, 0)) == NULL)
    {
      WIN32_GDI_FAILED ("CreateDIBSection");
      DeleteDC (memdc);
      if (GDK_IS_PIXMAP (drawable))
	{
	  SelectObject (hdc, oldbitmap1);
	  DeleteDC (hdc);
	}
      else
	{
	  ReleaseDC (impl->handle, hdc);
	}
      g_free (image);
      return NULL;
    }

  if ((oldbitmap2 = SelectObject (memdc, private->hbitmap)) == NULL)
    {
      WIN32_GDI_FAILED ("SelectObject");
      DeleteObject (private->hbitmap);
      DeleteDC (memdc);
      if (GDK_IS_PIXMAP (drawable))
	{
	  SelectObject (hdc, oldbitmap1);
	  DeleteDC (hdc);
	}
      else
	{
	  ReleaseDC (impl->handle, hdc);
	}
      g_free (image);
      return NULL;
    }

  if (!BitBlt (memdc, 0, 0, width, height, hdc, x, y, SRCCOPY))
    {
      WIN32_GDI_FAILED ("BitBlt");
      SelectObject (memdc, oldbitmap2);
      DeleteObject (private->hbitmap);
      DeleteDC (memdc);
      if (GDK_IS_PIXMAP (drawable))
	{
	  SelectObject (hdc, oldbitmap1);
	  DeleteDC (hdc);
	}
      else
	{
	  ReleaseDC (impl->handle, hdc);
	}
      g_free (image);
      return NULL;
    }

  if (SelectObject (memdc, oldbitmap2) == NULL)
    WIN32_GDI_FAILED ("SelectObject");

  if (!DeleteDC (memdc))
    WIN32_GDI_FAILED ("DeleteDC");

  if (GDK_IS_PIXMAP_IMPL_WIN32 (drawable))
    {
      SelectObject (hdc, oldbitmap1);
      DeleteDC (hdc);
    }
  else
    {
      ReleaseDC (impl->handle, hdc);
    }

  switch (image->depth)
    {
    case 1:
    case 8:
      image->bpp = 1;
      break;
    case 15:
    case 16:
      image->bpp = 2;
      break;
    case 24:
      image->bpp = 3;
      break;
    case 32:
      image->bpp = 4;
      break;
    default:
      g_warning ("_gdk_win32_get_image: image->depth = %d", image->depth);
      g_assert_not_reached ();
    }
  image->bits_per_pixel = image->depth;
  image->byte_order = GDK_LSB_FIRST;
  if (image->depth == 1)
    image->bpl = ((width - 1)/32 + 1)*4;
  else
    image->bpl = ((width*image->bpp - 1)/4 + 1)*4;

  GDK_NOTE (MISC, g_print ("... = %#x mem = %p, bpl = %d\n",
			   (guint) private->hbitmap, image->mem, image->bpl));

  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint      x,
		     gint      y)
{
  guint32 pixel = 0;

  g_return_val_if_fail (GDK_IS_IMAGE (image), 0);

  if (!(x >= 0 && x < image->width && y >= 0 && y < image->height))
      return 0;

  if (image->depth == 1)
    pixel = (((char *) image->mem)[y * image->bpl + (x >> 3)] & (1 << (7 - (x & 0x7)))) != 0;
  else
    {
      guchar *pixelp = (guchar *) image->mem + y * image->bpl + x * image->bpp;
      
      switch (image->bpp)
	{
	case 1:
	  pixel = *pixelp;
	  break;

	/* Windows is always LSB, no need to check image->byte_order. */
	case 2:
	  pixel = pixelp[0] | (pixelp[1] << 8);
	  break;

	case 3:
	  pixel = pixelp[0] | (pixelp[1] << 8) | (pixelp[2] << 16);
	  break;

	case 4:
	  pixel = pixelp[0] | (pixelp[1] << 8) | (pixelp[2] << 16);
	  break;

	default:
	  g_warning ("gdk_image_get_pixel(): bpp = %d", image->bpp);
	  g_assert_not_reached ();
	}
    }

  return pixel;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint       x,
		     gint       y,
		     guint32    pixel)
{
  g_return_if_fail (image != NULL);

  if  (!(x >= 0 && x < image->width && y >= 0 && y < image->height))
    return;

  if (image->depth == 1)
    if (pixel & 1)
      ((guchar *) image->mem)[y * image->bpl + (x >> 3)] |= (1 << (7 - (x & 0x7)));
    else
      ((guchar *) image->mem)[y * image->bpl + (x >> 3)] &= ~(1 << (7 - (x & 0x7)));
  else
    {
      guchar *pixelp = (guchar *) image->mem + y * image->bpl + x * image->bpp;
      
      /* Windows is always LSB, no need to check image->byte_order. */
      switch (image->bpp)
	{
	case 4:
	  pixelp[3] = 0;
	case 3:
	  pixelp[2] = ((pixel >> 16) & 0xFF);
	case 2:
	  pixelp[1] = ((pixel >> 8) & 0xFF);
	case 1:
	  pixelp[0] = (pixel & 0xFF);
	}
    }
}

static void
gdk_win32_image_destroy (GdkImage *image)
{
  GdkImagePrivateWin32 *private;

  g_return_if_fail (GDK_IS_IMAGE (image));

  private = IMAGE_PRIVATE_DATA (image);

  if (private == NULL) /* This means that _gdk_image_exit() destroyed the
                        * image already, and now we're called a second
                        * time from _finalize()
                        */
    return;
  
  GDK_NOTE (MISC, g_print ("gdk_win32_image_destroy: %#x\n",
			   (guint) private->hbitmap));
  
  switch (image->type)
    {
    case GDK_IMAGE_SHARED:
      if (!DeleteObject (private->hbitmap))
	WIN32_GDI_FAILED ("DeleteObject");
      break;

    default:
      g_assert_not_reached ();
    }

  g_free (private);
  image->windowing_data = NULL;
}

