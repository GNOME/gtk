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

#include <config.h>

#include <endian.h>
#ifndef __BYTE_ORDER
#error "endian.h needs to #define __BYTE_ORDER"
#endif

#include <stdlib.h>
#include <sys/types.h>

#include "gdk.h"
#include "gdkimage.h"
#include "gdkprivate.h"
#include "gdkprivate-fb.h"

struct _GdkImagePrivateFB
{
  GdkImagePrivate base;
};

static void gdk_fb_image_destroy (GdkImage    *image);
static void gdk_image_put_normal  (GdkImage    *image,
				   GdkDrawable *drawable,
				   GdkGC       *gc,
				   gint         xsrc,
				   gint         ysrc,
				   gint         xdest,
				   gint         ydest,
				   gint         width,
				   gint         height);

static GdkImageClass image_class_normal = {
  gdk_fb_image_destroy,
  gdk_image_put_normal
};

GdkImage *
gdk_image_new_bitmap(GdkVisual *visual, gpointer data, gint w, gint h)
{
  GdkImage *image;
  GdkImagePrivateFB *private;
  
  private = g_new(GdkImagePrivateFB, 1);
  image = (GdkImage *) private;
  private->base.ref_count = 1;
  private->base.klass = &image_class_normal;
  image->type = GDK_IMAGE_NORMAL;
  image->visual = visual;
  image->width = w;
  image->height = h;
  image->depth = 1;

  image->byte_order = 1 /* MSBFirst */;
  image->mem = g_malloc(w * h / 8);
  image->bpp = 1;
  image->bpl = (w+7)/8;

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
  GdkImage *image;
  GdkImagePrivateFB *private;

  private = g_new (GdkImagePrivateFB, 1);
  image = (GdkImage*) private;

  private->base.ref_count = 1;
  
  image->type = 0;
  image->visual = visual;
  image->width = width;
  image->height = height;
  image->depth = visual->depth;
  
  private->base.klass = &image_class_normal;

  if (image)
    {
      image->byte_order = 0;
      image->mem = g_malloc(width * height * (image->depth >> 3));
      image->bpp = image->depth/8;
      image->bpl = (width * image->depth + 7)/8;
    }

  return image;
}

GdkImage*
gdk_image_get (GdkWindow *window,
	       gint       x,
	       gint       y,
	       gint       width,
	       gint       height)
{
  GdkImage *image;
  GdkImagePrivateFB *private;
  gint bits_per_pixel = GDK_DRAWABLE_P(gdk_parent_root)->depth;
  GdkPixmapFBData fbd;
  GdkDrawablePrivate tmp_foo;

  g_return_val_if_fail (window != NULL, NULL);

  if (GDK_DRAWABLE_DESTROYED (window))
    return NULL;

  private = g_new (GdkImagePrivateFB, 1);
  image = (GdkImage*) private;

  private->base.ref_count = 1;
  private->base.klass = &image_class_normal;

  image->type = GDK_IMAGE_NORMAL;
  image->visual = gdk_window_get_visual (window);
  image->width = width;
  image->height = height;
  image->depth = bits_per_pixel;

  if (bits_per_pixel <= 8)
    image->bpp = 1;
  else if (bits_per_pixel <= 16)
    image->bpp = 2;
  else if (bits_per_pixel <= 24)
    image->bpp = 3;
  else
    image->bpp = 4;
  image->byte_order = 1;
  image->bpl = (image->width * image->depth + 7)/8;
  image->mem = g_malloc(image->bpl * image->height);
  
  /* Fake its existence as a pixmap */
  memset(&tmp_foo, 0, sizeof(tmp_foo));
  memset(&fbd, 0, sizeof(fbd));
  tmp_foo.klass = &_gdk_fb_drawable_class;
  tmp_foo.klass_data = &fbd;
  fbd.drawable_data.mem = image->mem;
  fbd.drawable_data.rowstride = image->bpl;
  tmp_foo.width = fbd.drawable_data.lim_x = image->width;
  tmp_foo.height = fbd.drawable_data.lim_y = image->height;
  tmp_foo.depth = image->depth;
  tmp_foo.window_type = GDK_DRAWABLE_PIXMAP;

  gdk_fb_draw_drawable((GdkPixmap *)&tmp_foo, NULL, window, x, y, 0, 0, width, height);

  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint x,
		     gint y)
{
  GdkImagePrivateFB *private;

  g_return_val_if_fail (image != NULL, 0);

  private = (GdkImagePrivateFB *) image;

  switch(image->depth)
    {
    case 8:
      return ((guchar *)image->mem)[x + y * image->bpl];
      break;
    case 16:
      return *((guint16 *)&((guchar *)image->mem)[x*2 + y*image->bpl]);
      break;
    case 24:
    case 32:
      {
	guchar *smem = &(((guchar *)image->mem)[x*image->bpp + y*image->bpl]);
	return smem[0]|(smem[1]<<8)|(smem[2]<<16);
      }
      break;
    }

  return 0;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint x,
		     gint y,
		     guint32 pixel)
{
  guchar *ptr = image->mem;

  g_return_if_fail (image != NULL);

  switch(image->depth)
    {
    case 8:
      ptr[x + y * image->bpl] = pixel;
      break;
    case 16:
      {
	guint16 *p16 = (guint16 *)&ptr[x*2 + y*image->bpl];
	*p16 = pixel;
      }
      break;
    case 24:
      {
	guchar *smem = &ptr[x*3 + y*image->bpl];
	smem[0] = (pixel & 0xFF);
	smem[1] = (pixel & 0xFF00) >> 8;
	smem[2] = (pixel & 0xFF0000) >> 16;
      }
      break;
    case 32:
      {
	guint32 *smem = (guint32 *)&ptr[x*4 + y*image->bpl];
	*smem = pixel;
      }
      break;
    default:
      g_assert_not_reached();
      break;
    }
}

static void
gdk_fb_image_destroy (GdkImage *image)
{
  GdkImagePrivateFB *private;

  g_return_if_fail (image != NULL);

  private = (GdkImagePrivateFB*) image;

  g_free(image->mem); image->mem = NULL;

  g_free (image);
}

static void
gdk_image_put_normal (GdkImage    *image,
		      GdkDrawable *drawable,
		      GdkGC       *gc,
		      gint         xsrc,
		      gint         ysrc,
		      gint         xdest,
		      gint         ydest,
		      gint         width,
		      gint         height)
{
  GdkImagePrivateFB *image_private;
  GdkPixmapFBData fbd;
  GdkDrawablePrivate tmp_foo;

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (image != NULL);
  g_return_if_fail (gc != NULL);

  if (GDK_DRAWABLE_DESTROYED (drawable))
    return;

  image_private = (GdkImagePrivateFB*) image;

  g_return_if_fail (image->type == GDK_IMAGE_NORMAL);

  /* Fake its existence as a pixmap */
  memset(&tmp_foo, 0, sizeof(tmp_foo));
  memset(&fbd, 0, sizeof(fbd));
  tmp_foo.klass = &_gdk_fb_drawable_class;
  tmp_foo.klass_data = &fbd;
  fbd.drawable_data.mem = image->mem;
  fbd.drawable_data.rowstride = image->bpl;
  tmp_foo.width = fbd.drawable_data.lim_x = image->width;
  tmp_foo.height = fbd.drawable_data.lim_y = image->height;
  tmp_foo.depth = image->depth;
  tmp_foo.window_type = GDK_DRAWABLE_PIXMAP;

  gdk_fb_draw_drawable(drawable, gc, (GdkPixmap *)&tmp_foo, xsrc, ysrc, xdest, ydest, width, height);
}

void
gdk_image_exit(void)
{
}
