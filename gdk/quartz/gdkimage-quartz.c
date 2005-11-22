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

#include <config.h>

#include "gdk.h"
#include "gdkimage.h"
#include "gdkprivate-quartz.h"

static GObjectClass *parent_class;

GdkImage *
_gdk_quartz_copy_to_image (GdkDrawable *drawable,
			   GdkImage    *image,
			   gint         src_x,
			   gint         src_y,
			   gint         dest_x,
			   gint         dest_y,
			   gint         width,
			   gint         height)
{
  /* FIXME: Implement */
  return NULL;
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
