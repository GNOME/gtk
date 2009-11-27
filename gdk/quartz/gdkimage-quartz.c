/* gdkimage-quartz.c
 *
 * Copyright (C) 2005 Imendio AB
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

#include "config.h"

#include "gdk.h"
#include "gdkimage.h"
#include "gdkprivate-quartz.h"

static GObjectClass *parent_class;

GdkImage *
_gdk_quartz_image_copy_to_image (GdkDrawable *drawable,
				 GdkImage    *image,
				 gint         src_x,
				 gint         src_y,
				 gint         dest_x,
				 gint         dest_y,
				 gint         width,
				 gint         height)
{
  GdkScreen *screen;
  
  g_return_val_if_fail (GDK_IS_DRAWABLE_IMPL_QUARTZ (drawable), NULL);
  g_return_val_if_fail (image != NULL || (dest_x == 0 && dest_y == 0), NULL);

  screen = gdk_drawable_get_screen (drawable);
  if (!image)
    image = _gdk_image_new_for_depth (screen, GDK_IMAGE_FASTEST, NULL, 
				      width, height,
				      gdk_drawable_get_depth (drawable));
  
  if (GDK_IS_PIXMAP_IMPL_QUARTZ (drawable))
    {
      GdkPixmapImplQuartz *pix_impl;
      gint bytes_per_row;
      guchar *data;
      int x, y;

      pix_impl = GDK_PIXMAP_IMPL_QUARTZ (drawable);
      data = (guchar *)(pix_impl->data);

      if (src_x + width > pix_impl->width || src_y + height > pix_impl->height)
      	{
          g_warning ("Out of bounds copy-area for pixmap -> image conversion\n");
          return image;
        }

      switch (gdk_drawable_get_depth (drawable))
        {
        case 24:
          bytes_per_row = pix_impl->width * 4;
          for (y = 0; y < height; y++)
            {
              guchar *src = data + ((y + src_y) * bytes_per_row) + (src_x * 4);

              for (x = 0; x < width; x++)
                {
                  gint32 pixel;
	  
                  /* RGB24, 4 bytes per pixel, skip first. */
                  pixel = src[0] << 16 | src[1] << 8 | src[2];
                  src += 4;

                  gdk_image_put_pixel (image, dest_x + x, dest_y + y, pixel);
                }
            }
          break;

        case 32:
          bytes_per_row = pix_impl->width * 4;
          for (y = 0; y < height; y++)
            {
              guchar *src = data + ((y + src_y) * bytes_per_row) + (src_x * 4);

              for (x = 0; x < width; x++)
                {
                  gint32 pixel;
	  
                  /* ARGB32, 4 bytes per pixel. */
                  pixel = src[0] << 24 | src[1] << 16 | src[2] << 8 | src[3];
                  src += 4;

                  gdk_image_put_pixel (image, dest_x + x, dest_y + y, pixel);
                }
            }
          break;

        case 1: /* TODO: optimize */
          bytes_per_row = pix_impl->width;
          for (y = 0; y < height; y++)
            {
              guchar *src = data + ((y + src_y) * bytes_per_row) + src_x;

              for (x = 0; x < width; x++)
                {
                  gint32 pixel;
	  
                  /* 8 bits */
                  pixel = src[0];
                  src++;

                  gdk_image_put_pixel (image, dest_x + x, dest_y + y, pixel);
                }
            }
          break;

        default:
          g_warning ("Unsupported bit depth %d\n", gdk_drawable_get_depth (drawable));
          return image;
        }
    }
  else if (GDK_IS_WINDOW_IMPL_QUARTZ (drawable))
    {
      GdkQuartzView *view;
      NSBitmapImageRep *rep;
      NSRect rect;
      guchar *data;
      int x, y;
      NSSize size;

      view = GDK_WINDOW_IMPL_QUARTZ (drawable)->view;

      /* We return the image even if we can't copy to it. */
      if (![view lockFocusIfCanDraw])
        return image;

      rect = NSMakeRect (src_x, src_y, width, height);

      rep = [[NSBitmapImageRep alloc] initWithFocusedViewRect: rect];
      [view unlockFocus];
	  
      data = [rep bitmapData];
      size = [rep size];

      for (y = 0; y < size.height; y++)
	{
	  guchar *src = data + y * [rep bytesPerRow];

	  for (x = 0; x < size.width; x++)
	    {
	      gint32 pixel;

	      if (image->byte_order == GDK_LSB_FIRST)
		pixel = src[0] << 8 | src[1] << 16 |src[2] << 24;
	      else
		pixel = src[0] << 16 | src[1] << 8 |src[2];

	      src += 3;

	      gdk_image_put_pixel (image, dest_x + x, dest_y + y, pixel);
	    }
	}

      [rep release];
    }

  return image;
}

static void
gdk_image_finalize (GObject *object)
{
  GdkImage *image = GDK_IMAGE (object);

  g_free (image->mem);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_image_class_init (GdkImageClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  parent_class = g_type_class_peek_parent (klass);
  
  object_class->finalize = gdk_image_finalize;
}

GType
gdk_image_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      const GTypeInfo object_info =
      {
        sizeof (GdkImageClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_image_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkImage),
        0,              /* n_preallocs */
        (GInstanceInitFunc) NULL,
      };
      
      object_type = g_type_register_static (G_TYPE_OBJECT,
                                            "GdkImage",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

GdkImage *
gdk_image_new_bitmap (GdkVisual *visual, gpointer data, gint width, gint height)
{
  /* We don't implement this function because it's broken, deprecated and 
   * tricky to implement. */
  g_warning ("This function is unimplemented");

  return NULL;
}

GdkImage*
_gdk_image_new_for_depth (GdkScreen    *screen,
			  GdkImageType  type,
			  GdkVisual    *visual,
			  gint          width,
			  gint          height,
			  gint          depth)
{
  GdkImage *image;

  if (visual)
    depth = visual->depth;

  g_assert (depth == 24 || depth == 32);

  image = g_object_new (gdk_image_get_type (), NULL);
  image->type = type;
  image->visual = visual;
  image->width = width;
  image->height = height;
  image->depth = depth;

  image->byte_order = (G_BYTE_ORDER == G_LITTLE_ENDIAN) ? GDK_LSB_FIRST : GDK_MSB_FIRST;

  /* We only support images with bpp 4 */
  image->bpp = 4;
  image->bpl = image->width * image->bpp;
  image->bits_per_pixel = image->bpp * 8;
  
  image->mem = g_malloc (image->bpl * image->height);
  memset (image->mem, 0x00, image->bpl * image->height);

  return image;
}

guint32
gdk_image_get_pixel (GdkImage *image,
		     gint x,
		     gint y)
{
  guchar *ptr;

  g_return_val_if_fail (image != NULL, 0);
  g_return_val_if_fail (x >= 0 && x < image->width, 0);
  g_return_val_if_fail (y >= 0 && y < image->height, 0);

  ptr = image->mem + y * image->bpl + x * image->bpp;

  return *(guint32 *)ptr;
}

void
gdk_image_put_pixel (GdkImage *image,
		     gint x,
		     gint y,
		     guint32 pixel)
{
  guchar *ptr;

  ptr = image->mem + y * image->bpl + x * image->bpp;

  *(guint32 *)ptr = pixel;
}

gint
_gdk_windowing_get_bits_for_depth (GdkDisplay *display,
				   gint        depth)
{
  if (depth == 24 || depth == 32)
    return 32;
  else
    g_assert_not_reached ();

  return 0;
}
