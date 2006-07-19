/* gdkpixmap-quartz.c
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

#include "gdkpixmap.h"
#include "gdkprivate-quartz.h"

static gpointer parent_class;

static void
gdk_pixmap_impl_quartz_init (GdkPixmapImplQuartz *impl)
{
}

static void
gdk_pixmap_impl_quartz_get_size (GdkDrawable *drawable,
				gint        *width,
				gint        *height)
{
  if (width)
    *width = GDK_PIXMAP_IMPL_QUARTZ (drawable)->width;
  if (height)
    *height = GDK_PIXMAP_IMPL_QUARTZ (drawable)->height;
}

static void
gdk_pixmap_impl_quartz_finalize (GObject *object)
{
  GdkPixmapImplQuartz *impl = GDK_PIXMAP_IMPL_QUARTZ (object);

  CGImageRelease (impl->image);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gdk_pixmap_impl_quartz_class_init (GdkPixmapImplQuartzClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkDrawableClass *drawable_class = GDK_DRAWABLE_CLASS (klass);
  
  parent_class = g_type_class_peek_parent (klass);

  object_class->finalize = gdk_pixmap_impl_quartz_finalize;

  drawable_class->get_size = gdk_pixmap_impl_quartz_get_size;
}

GType
_gdk_pixmap_impl_quartz_get_type (void)
{
  static GType object_type = 0;

  if (!object_type)
    {
      static const GTypeInfo object_info =
      {
        sizeof (GdkPixmapImplQuartzClass),
        (GBaseInitFunc) NULL,
        (GBaseFinalizeFunc) NULL,
        (GClassInitFunc) gdk_pixmap_impl_quartz_class_init,
        NULL,           /* class_finalize */
        NULL,           /* class_data */
        sizeof (GdkPixmapImplQuartz),
        0,              /* n_preallocs */
        (GInstanceInitFunc) gdk_pixmap_impl_quartz_init
      };
      
      object_type = g_type_register_static (GDK_TYPE_DRAWABLE_IMPL_QUARTZ,
                                            "GdkPixmapImplQuartz",
                                            &object_info,
					    0);
    }
  
  return object_type;
}

GType
_gdk_pixmap_impl_get_type (void)
{
  return _gdk_pixmap_impl_quartz_get_type ();
}

static void
data_provider_release (void *info, const void *data, size_t size)
{
  g_free (info);
}

GdkPixmap*
gdk_pixmap_new (GdkDrawable *drawable,
		gint         width,
		gint         height,
		gint         depth)
{
  GdkPixmap *pixmap;
  GdkDrawableImplQuartz *draw_impl;
  GdkPixmapImplQuartz *pix_impl;
  gint window_depth;
  CGColorSpaceRef colorspace;
  CGDataProviderRef data_provider;
  CGImageAlphaInfo alpha_info;
  gint bytes_per_row, bits_per_pixel;

  g_return_val_if_fail (drawable == NULL || GDK_IS_DRAWABLE (drawable), NULL);
  g_return_val_if_fail ((drawable != NULL) || (depth != -1), NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);

  if (GDK_IS_WINDOW (drawable) && GDK_WINDOW_DESTROYED (drawable))
    return NULL;

  if (!drawable)
    drawable = gdk_screen_get_root_window (gdk_screen_get_default ());

  window_depth = gdk_drawable_get_depth (GDK_DRAWABLE (drawable));

  if (depth == -1)
    depth = window_depth;

  switch (depth)
    {
    case 24:
      alpha_info = kCGImageAlphaNoneSkipLast;
      bytes_per_row = width * 4;
      bits_per_pixel = 32;
      colorspace = CGColorSpaceCreateDeviceRGB ();
      break;
    case 32:
      alpha_info = kCGImageAlphaPremultipliedFirst;
      bytes_per_row = width * 4;
      bits_per_pixel = 32;
      colorspace = CGColorSpaceCreateDeviceRGB ();
      break;
    case 1:
      alpha_info = kCGImageAlphaNone;
      bytes_per_row = width;
      bits_per_pixel = 8;
      colorspace = CGColorSpaceCreateDeviceGray ();
      break;
    default:
      g_warning ("Unsupported bit depth %d\n", depth);
      return NULL;
    }

  pixmap = g_object_new (gdk_pixmap_get_type (), NULL);
  draw_impl = GDK_DRAWABLE_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (pixmap)->impl);
  pix_impl = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (pixmap)->impl);
  draw_impl->wrapper = GDK_DRAWABLE (pixmap);

  g_assert (depth == 24 || depth == 32 || depth == 1);

  pix_impl->data = g_malloc (height * bytes_per_row);
  data_provider = CGDataProviderCreateWithData (pix_impl->data, pix_impl->data, 
						height * bytes_per_row, data_provider_release);
  pix_impl->image = CGImageCreate (width, height, 8, bits_per_pixel, 
				   bytes_per_row, colorspace,
				   alpha_info,
				   data_provider, NULL, FALSE, 
				   kCGRenderingIntentDefault);
  CGDataProviderRelease (data_provider);
  CGColorSpaceRelease (colorspace);

  pix_impl->width = width;
  pix_impl->height = height;
  GDK_PIXMAP_OBJECT (pixmap)->depth = depth;

  if (depth == window_depth) {
    GdkColormap *colormap = gdk_drawable_get_colormap (drawable);

    if (colormap)
      gdk_drawable_set_colormap (pixmap, colormap);
  }

  return pixmap;
}

GdkPixmap *
gdk_bitmap_create_from_data (GdkDrawable *window,
			     const gchar *data,
			     gint         width,
			     gint         height)
{
  GdkPixmap *pixmap;
  GdkPixmapImplQuartz *impl;
  int x, y, bpl;

  g_return_val_if_fail (data != NULL, NULL);
  g_return_val_if_fail ((width != 0) && (height != 0), NULL);
  g_return_val_if_fail (window == NULL || GDK_IS_DRAWABLE (window), NULL);

  pixmap = gdk_pixmap_new (window, width, height, 1);
  impl = GDK_PIXMAP_IMPL_QUARTZ (GDK_PIXMAP_OBJECT (pixmap)->impl);

  g_assert (CGImageGetBytesPerRow (impl->image) == width);

  /* Bytes per line: Each line consumes an integer number of bytes, possibly
   * ignoring any excess bits. */
  bpl = (width + 7) / 8;
  for (y = 0; y < height; y++)
    {
      guchar *dst = impl->data + y * width;
      const gchar *src = data + (y * bpl);   
      for (x = 0; x < width; x++)
	{
	  if ((src[x / 8] >> x % 8) & 1)
	    *dst = 0xff;
	  else
	    *dst = 0;

	  dst++;
	}
    }

  return pixmap;
}

GdkPixmap*
gdk_pixmap_create_from_data (GdkDrawable    *drawable,
			     const gchar    *data,
			     gint            width,
			     gint            height,
			     gint            depth,
			     const GdkColor *fg,
			     const GdkColor *bg)
{	
  /* FIXME: Implement */
  return NULL;
}

GdkPixmap *
gdk_pixmap_foreign_new_for_display (GdkDisplay      *display,
				    GdkNativeWindow  anid)
{
  return NULL;
}

GdkPixmap*
gdk_pixmap_foreign_new (GdkNativeWindow anid)
{
   return NULL;
}

GdkPixmap *
gdk_pixmap_foreign_new_for_screen (GdkScreen       *screen,
				   GdkNativeWindow  anid,
				   gint             width,
				   gint             height,
				   gint             depth)
{
  return NULL;
}

GdkPixmap*
gdk_pixmap_lookup (GdkNativeWindow anid)
{
  return NULL;
}

GdkPixmap*
gdk_pixmap_lookup_for_display (GdkDisplay *display, GdkNativeWindow anid)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  return NULL;
}
