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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
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

#include "gdk.h"		/* For gdk_error_trap_* / gdk_flush_* */
#include "gdkimage.h"
#include "gdkprivate.h"
#include "gdkwin32.h"

static void gdk_win32_image_destroy (GdkImage    *image);
static void gdk_image_put  (GdkImage    *image,
			    GdkDrawable *drawable,
			    GdkGC       *gc,
			    gint         xsrc,
			    gint         ysrc,
			    gint         xdest,
			    gint         ydest,
			    gint         width,
			    gint         height);

static GdkImageClass image_class = {
  gdk_win32_image_destroy,
  gdk_image_put
};

static GList *image_list = NULL;

void
gdk_image_exit (void)
{
  GdkImage *image;

  while (image_list)
    {
      image = image_list->data;
      gdk_win32_image_destroy (image);
    }
}

GdkImagePrivateWin32 *
gdk_win32_image_alloc (void)
{
  GdkImagePrivateWin32 *private;

  private = g_new (GdkImagePrivateWin32, 1);
  private->base.ref_count = 1;
  private->base.klass = &image_class;

  return private;
}

GdkImage *
gdk_image_new_bitmap (GdkVisual *visual,
		      gpointer   data,
		      gint       width,
		      gint       height)
/*
 * Desc: create a new bitmap image
 */
{
  GdkPixmap *pixmap;
  GdkImage *image;
  GdkImagePrivateWin32 *private;
  gint data_bpl = (width - 1)/8 + 1;
  gint i;

  pixmap = gdk_pixmap_new (NULL, width, height, 1);

  if (pixmap == NULL)
    return NULL;

  image = GDK_DRAWABLE_WIN32DATA (pixmap)->image;
  private = (GdkImagePrivateWin32 *) image;

  GDK_NOTE (IMAGE, g_print ("gdk_image_new_bitmap: %dx%d\n", width, height));
  
  if (data_bpl != image->bpl)
    {
      for (i = 0; i < height; i++)
	memmove (image->mem + i*image->bpl, ((guchar *) data) + i*data_bpl,
		 data_bpl);
    }
  else
    memmove (image->mem, data, data_bpl*height);

  return image;
}

void
gdk_image_init (void)
{
}

GdkImage*
gdk_image_new (GdkImageType  type,
	       GdkVisual    *visual,
	       gint          width,
	       gint          height)
{
  GdkPixmap *pixmap = gdk_win32_pixmap_new (NULL, visual, width, height, visual->depth);

  if (!pixmap)
    return NULL;

  return GDK_DRAWABLE_WIN32DATA (pixmap)->image;
}

GdkImage*
gdk_image_get (GdkWindow *window,
	       gint       x,
	       gint       y,
	       gint       width,
	       gint       height)
{
  HDC hdc, memdc;
  HGDIOBJ oldbitmap2;
  BITMAP bm;
  HPALETTE holdpalw = NULL, holdpali = NULL;
  GdkVisual *visual;
  GdkPixmap *pixmap;
  GdkImage *image;
  GdkColormapPrivateWin32 *cmapp = NULL;
  gint depth;

  g_return_val_if_fail (window != NULL, NULL);

  if (GDK_DRAWABLE_DESTROYED (window))
    return NULL;

  GDK_NOTE (IMAGE, g_print ("gdk_image_get: %p %dx%d@+%d+%d\n",
			    GDK_DRAWABLE_XID (window), width, height, x, y));
  
  visual = gdk_drawable_get_visual (window);

  /* This function is called both to blit from a window and from
   * a pixmap.
   */
  if (GDK_DRAWABLE_TYPE (window) == GDK_DRAWABLE_PIXMAP)
    {
      hdc = gdk_win32_obtain_offscreen_hdc (GDK_DRAWABLE_XID (window));
      if (hdc == NULL)
	{
	  g_free (image);
	  return NULL;
	}
      if (visual != NULL &&
	  (visual->type == GDK_VISUAL_PSEUDO_COLOR ||
	   visual->type == GDK_VISUAL_STATIC_COLOR))
	cmapp = (GdkColormapPrivateWin32 *) gdk_drawable_get_colormap (window);

      GetObject (GDK_DRAWABLE_XID (window), sizeof (BITMAP), &bm);
      GDK_NOTE (IMAGE,
		g_print ("gdk_image_get: bmWidth=%ld bmHeight=%ld "
			 "bmWidthBytes=%ld bmBitsPixel=%d\n",
			 bm.bmWidth, bm.bmHeight,
			 bm.bmWidthBytes, bm.bmBitsPixel));
      depth = bm.bmBitsPixel;
    }
  else
    {
      hdc = gdk_win32_obtain_window_hdc (GDK_DRAWABLE_XID (window));
      if (hdc == NULL)
	return NULL;
      depth = gdk_visual_get_system ()->depth;
    }

  if ((memdc = CreateCompatibleDC (hdc)) == NULL)
    {
      WIN32_GDI_FAILED ("CreateCompatibleDC");
      if (GDK_DRAWABLE_TYPE (window) == GDK_DRAWABLE_PIXMAP)
        gdk_win32_release_hdc (NULL, hdc);
      else
        gdk_win32_release_hdc (GDK_DRAWABLE_XID (window), hdc);
      return NULL;
    }

  pixmap = gdk_win32_pixmap_new (NULL, visual, width, height, depth);
  if (!pixmap)
    return NULL;

  image = GDK_DRAWABLE_WIN32DATA (pixmap)->image;

  if (cmapp != NULL)
    {
      if (!(holdpalw = SelectPalette (hdc, cmapp->hpal, FALSE)))
	WIN32_GDI_FAILED ("SelectPalette");
      else
	GDK_NOTE (IMAGE, g_print ("gdk_image_get: hpal=%p hdc=%p\n",
				  cmapp->hpal, hdc));
    }
  
  if ((oldbitmap2 = SelectObject (memdc, GDK_DRAWABLE_XID (pixmap))) == NULL)
    {
      WIN32_GDI_FAILED ("SelectObject");
      gdk_pixmap_unref (pixmap);
      DeleteDC (memdc);
      if (GDK_DRAWABLE_TYPE (window) == GDK_DRAWABLE_PIXMAP)
        gdk_win32_release_hdc (NULL, hdc);
      else
        gdk_win32_release_hdc (GDK_DRAWABLE_XID (window), hdc);
      return NULL;
    }
  
  if (cmapp != NULL)
    {
      if (!(holdpali = SelectPalette (memdc, cmapp->hpal, FALSE)))
	WIN32_GDI_FAILED ("SelectPalette");
      else
	GDK_NOTE (IMAGE, g_print ("gdk_image_get: hpal=%p memdc=%p\n",
				  cmapp->hpal, memdc));
    }
  
  if (!BitBlt (memdc, 0, 0, width, height, hdc, x, y, SRCCOPY))
    {
      WIN32_GDI_FAILED ("BitBlt");
      if (holdpalw != NULL)
	if (!SelectPalette (hdc, holdpalw, FALSE))
	  WIN32_GDI_FAILED ("SelectPalette");
      if (holdpali != NULL)
	if (!SelectPalette (memdc, holdpali, FALSE))
	  WIN32_GDI_FAILED ("SelectPalette");
      SelectObject (memdc, oldbitmap2);
      gdk_pixmap_unref (pixmap);
      DeleteDC (memdc);
      if (GDK_DRAWABLE_TYPE (window) == GDK_DRAWABLE_PIXMAP)
        gdk_win32_release_hdc (NULL, hdc);
      else
        gdk_win32_release_hdc (GDK_DRAWABLE_XID (window), hdc);
      return NULL;
    }

  if (holdpalw != NULL)
    if (!SelectPalette (hdc, holdpalw, FALSE))
      WIN32_GDI_FAILED ("SelectPalette");

  if (holdpali != NULL)
    if (!SelectPalette (memdc, holdpali, FALSE))
      WIN32_GDI_FAILED ("SelectPalette");

  if (SelectObject (memdc, oldbitmap2) == NULL)
    WIN32_GDI_FAILED ("SelectObject");

  if (!DeleteDC (memdc))
    WIN32_GDI_FAILED ("DeleteDC");

  if (GDK_DRAWABLE_TYPE (window) == GDK_DRAWABLE_PIXMAP)
    gdk_win32_release_hdc (NULL, hdc);
  else
    gdk_win32_release_hdc (GDK_DRAWABLE_XID (window), hdc);

  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint      x,
		     gint      y)
{
  guint32 pixel;

  g_return_val_if_fail (image != NULL, 0);

  g_return_val_if_fail (x >= 0 && x < image->width &&
			y >= 0 && y < image->height,
			0);

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
	  g_assert_not_reached ();
	}
    }

  return pixel;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint      x,
		     gint      y,
		     guint32   pixel)
{
  g_return_if_fail (image != NULL);

  g_return_if_fail (x >= 0 && x < image->width && y >= 0 && y < image->height);

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
  g_return_if_fail (image != NULL);

  gdk_pixmap_unref (((GdkImagePrivateWin32 *) image)->pixmap);
}

static void
gdk_image_put (GdkImage    *image,
	       GdkDrawable *drawable,
	       GdkGC       *gc,
	       gint         xsrc,
	       gint         ysrc,
	       gint         xdest,
	       gint         ydest,
	       gint         width,
	       gint         height)
{
  g_return_if_fail (drawable != NULL);
  g_return_if_fail (image != NULL);
  g_return_if_fail (gc != NULL);

  if (GDK_DRAWABLE_DESTROYED (drawable))
    return;

  gdk_draw_drawable (drawable, gc, ((GdkImagePrivateWin32 *) image)->pixmap,
		     xsrc, ysrc, xdest, ydest, width, height);
}
