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
{
  GdkPixmap *pixmap;
  GdkImage *image;
  gint data_bpl = (width - 1)/8 + 1;
  gint i;

  pixmap = gdk_pixmap_new (NULL, width, height, 1);

  if (pixmap == NULL)
    return NULL;

  image = GDK_DRAWABLE_WIN32DATA (pixmap)->image;

  GDK_NOTE (IMAGE, g_print ("gdk_image_new_bitmap: %dx%d\n", width, height));
  
  if (data_bpl != image->bpl)
    {
      for (i = 0; i < height; i++)
	memmove ((guchar *) image->mem + i*image->bpl, ((guchar *) data) + i*data_bpl,
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
  BITMAP bm;
  GdkVisual *visual;
  GdkPixmap *pixmap;
  GdkGC *gc;
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
  if (GDK_IS_PIXMAP (window))
    {
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
      depth = gdk_visual_get_system ()->depth;
    }

  pixmap = gdk_win32_pixmap_new (NULL, visual, width, height, depth);
  if (!pixmap)
    return NULL;

  gc = gdk_gc_new (pixmap);
  gdk_win32_blit (FALSE, pixmap, gc, window, x, y, 0, 0, width, height);
  gdk_gc_unref (gc);

  return GDK_DRAWABLE_WIN32DATA (pixmap)->image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint      x,
		     gint      y)
{
  guchar *pixelp;

  g_return_val_if_fail (image != NULL, 0);
  g_return_val_if_fail (x >= 0 && x < image->width &&
			y >= 0 && y < image->height,
			0);

  GdiFlush ();
  if (image->depth == 1)
    return (((guchar *) image->mem)[y * image->bpl + (x >> 3)] & (1 << (7 - (x & 0x7)))) != 0;

  if (image->depth == 4)
    {
      pixelp = (guchar *) image->mem + y * image->bpl + (x >> 1);
      if (x&1)
	return (*pixelp) & 0x0F;

      return (*pixelp) >> 4;
    }
    
  pixelp = (guchar *) image->mem + y * image->bpl + x * image->bpp;
      
  switch (image->bpp)
    {
    case 1:
      return *pixelp;
      
      /* Windows is always LSB, no need to check image->byte_order. */

    case 2:
      return pixelp[0] | (pixelp[1] << 8);
      
    case 3:
      return pixelp[0] | (pixelp[1] << 8) | (pixelp[2] << 16);
      
    case 4:
      return pixelp[0] | (pixelp[1] << 8) | (pixelp[2] << 16);
      
    default:
      g_assert_not_reached ();
      return 0;
    }
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint      x,
		     gint      y,
		     guint32   pixel)
{
  g_return_if_fail (image != NULL);

  g_return_if_fail (x >= 0 && x < image->width && y >= 0 && y < image->height);

  GdiFlush ();
  if (image->depth == 1)
    if (pixel & 1)
      ((guchar *) image->mem)[y * image->bpl + (x >> 3)] |= (1 << (7 - (x & 0x7)));
    else
      ((guchar *) image->mem)[y * image->bpl + (x >> 3)] &= ~(1 << (7 - (x & 0x7)));
  else if (image->depth == 4)
    {
      guchar *pixelp = (guchar *) image->mem + y * image->bpl + (x >> 1);

      if (x&1)
	{
	  *pixelp &= 0xF0;
	  *pixelp |= (pixel & 0x0F);
	}
      else
	{
	  *pixelp &= 0x0F;
	  *pixelp |= (pixel << 4);
	}
    }
  else
    {
      guchar *pixelp = (guchar *) image->mem + y * image->bpl + x * image->bpp;
      
      /* Windows is always LSB, no need to check image->byte_order. */
      switch (image->bpp)
	{
	case 4:
	  pixelp[3] = 0;
	  /* Fall-through */
	case 3:
	  pixelp[2] = ((pixel >> 16) & 0xFF);
	  /* Fall-through */
	case 2:
	  pixelp[1] = ((pixel >> 8) & 0xFF);
	  /* Fall-through */
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

  gdk_win32_blit (TRUE, drawable, gc,
		  ((GdkImagePrivateWin32 *) image)->pixmap,
		  xsrc, ysrc, xdest, ydest, width, height);
}
