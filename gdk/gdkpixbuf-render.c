/* GdkPixbuf library - Rendering functions
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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
#include <gdk/gdk.h>
#include "gdk-pixbuf-private.h"
#include "gdkpixbuf.h"



/**
 * gdk_pixbuf_render_threshold_alpha:
 * @pixbuf: A pixbuf.
 * @bitmap: Bitmap where the bilevel mask will be painted to.
 * @src_x: Source X coordinate.
 * @src_y: source Y coordinate.
 * @dest_x: Destination X coordinate.
 * @dest_y: Destination Y coordinate.
 * @width: Width of region to threshold.
 * @height: Height of region to threshold.
 * @alpha_threshold: Opacity values below this will be painted as zero; all
 * other values will be painted as one.
 *
 * Takes the opacity values in a rectangular portion of a pixbuf and thresholds
 * them to produce a bi-level alpha mask that can be used as a clipping mask for
 * a drawable.
 *
 **/
void
gdk_pixbuf_render_threshold_alpha (GdkPixbuf *pixbuf,
				   GdkBitmap *bitmap,
				   int src_x,  int src_y,
				   int dest_x, int dest_y,
				   int width,  int height,
				   int alpha_threshold)
{
  GdkGC *gc;
  GdkColor color;
  int x, y;
  guchar *p;
  int start, start_status;
  int status;

  g_return_if_fail (pixbuf != NULL);
  g_return_if_fail (pixbuf->colorspace == GDK_COLORSPACE_RGB);
  g_return_if_fail (pixbuf->n_channels == 3 || pixbuf->n_channels == 4);
  g_return_if_fail (pixbuf->bits_per_sample == 8);

  g_return_if_fail (bitmap != NULL);
  g_return_if_fail (width >= 0 && height >= 0);
  g_return_if_fail (src_x >= 0 && src_x + width <= pixbuf->width);
  g_return_if_fail (src_y >= 0 && src_y + height <= pixbuf->height);

  g_return_if_fail (alpha_threshold >= 0 && alpha_threshold <= 255);

  if (width == 0 || height == 0)
    return;

  gc = gdk_gc_new (bitmap);

  if (!pixbuf->has_alpha)
    {
      color.pixel = (alpha_threshold == 255) ? 0 : 1;
      gdk_gc_set_foreground (gc, &color);
      gdk_draw_rectangle (bitmap, gc, TRUE, dest_x, dest_y, width, height);
      gdk_gc_unref (gc);
      return;
    }

  color.pixel = 0;
  gdk_gc_set_foreground (gc, &color);
  gdk_draw_rectangle (bitmap, gc, TRUE, dest_x, dest_y, width, height);

  color.pixel = 1;
  gdk_gc_set_foreground (gc, &color);

  for (y = 0; y < height; y++)
    {
      p = (pixbuf->pixels + (y + src_y) * pixbuf->rowstride + src_x * pixbuf->n_channels
	   + pixbuf->n_channels - 1);
	    
      start = 0;
      start_status = *p < alpha_threshold;
	    
      for (x = 0; x < width; x++)
	{
	  status = *p < alpha_threshold;
	  
	  if (status != start_status)
	    {
	      if (!start_status)
		gdk_draw_line (bitmap, gc,
			       start + dest_x, y + dest_y,
			       x - 1 + dest_x, y + dest_y);
	      
	      start = x;
	      start_status = status;
	    }
	  
	  p += pixbuf->n_channels;
	}
      
      if (!start_status)
	gdk_draw_line (bitmap, gc,
		       start + dest_x, y + dest_y,
		       x - 1 + dest_x, y + dest_y);
    }
	
  gdk_gc_unref (gc);
}



/**
 * gdk_pixbuf_render_to_drawable:
 * @pixbuf: A pixbuf.
 * @drawable: Destination drawable.
 * @gc: GC used for rendering.
 * @src_x: Source X coordinate within pixbuf.
 * @src_y: Source Y coordinate within pixbuf.
 * @dest_x: Destination X coordinate within drawable.
 * @dest_y: Destination Y coordinate within drawable.
 * @width: Width of region to render, in pixels.
 * @height: Height of region to render, in pixels.
 * @dither: Dithering mode for GdkRGB.
 * @x_dither: X offset for dither.
 * @y_dither: Y offset for dither.
 *
 * Renders a rectangular portion of a pixbuf to a drawable while using the
 * specified GC.  This is done using GdkRGB, so the specified drawable must have
 * the GdkRGB visual and colormap.  Note that this function will ignore the
 * opacity information for images with an alpha channel; the GC must already
 * have the clipping mask set if you want transparent regions to show through.
 *
 * For an explanation of dither offsets, see the GdkRGB documentation.  In
 * brief, the dither offset is important when re-rendering partial regions of an
 * image to a rendered version of the full image, or for when the offsets to a
 * base position change, as in scrolling.  The dither matrix has to be shifted
 * for consistent visual results.  If you do not have any of these cases, the
 * dither offsets can be both zero.
 **/
void
gdk_pixbuf_render_to_drawable (GdkPixbuf   *pixbuf,
			       GdkDrawable *drawable,
			       GdkGC       *gc,
			       int src_x,    int src_y,
			       int dest_x,   int dest_y,
			       int width,    int height,
			       GdkRgbDither dither,
			       int x_dither, int y_dither)
{
  int rowstride;
  guchar *buf;

  g_return_if_fail (pixbuf != NULL);
  g_return_if_fail (pixbuf->colorspace == GDK_COLORSPACE_RGB);
  g_return_if_fail (pixbuf->n_channels == 3 || pixbuf->n_channels == 4);
  g_return_if_fail (pixbuf->bits_per_sample == 8);

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (gc != NULL);

  g_return_if_fail (width >= 0 && height >= 0);
  g_return_if_fail (src_x >= 0 && src_x + width <= pixbuf->width);
  g_return_if_fail (src_y >= 0 && src_y + height <= pixbuf->height);

  if (width == 0 || height == 0)
    return;

  /* This will have to be modified once we support other image types.
   */

  if (pixbuf->n_channels == 4)
    {
      buf = pixbuf->pixels + src_y * pixbuf->rowstride + src_x * 4;
      rowstride = pixbuf->rowstride;

      gdk_draw_rgb_32_image_dithalign (drawable, gc,
				       dest_x, dest_y,
				       width, height,
				       dither,
				       buf, rowstride,
				       x_dither, y_dither);
    }
  else				/* n_channels == 3 */
    {
      buf = pixbuf->pixels + src_y * pixbuf->rowstride + src_x * 3;
      rowstride = pixbuf->rowstride;

      gdk_draw_rgb_image_dithalign (drawable, gc,
				    dest_x, dest_y,
				    width, height,
				    dither,
				    buf, rowstride,
				    x_dither, y_dither);
    }
}



/**
 * gdk_pixbuf_render_to_drawable_alpha:
 * @pixbuf: A pixbuf.
 * @drawable: Destination drawable.
 * @src_x: Source X coordinate within pixbuf.
 * @src_y: Source Y coordinates within pixbuf.
 * @dest_x: Destination X coordinate within drawable.
 * @dest_y: Destination Y coordinate within drawable.
 * @width: Width of region to render, in pixels.
 * @height: Height of region to render, in pixels.
 * @alpha_mode: If the image does not have opacity information, this is ignored.
 * Otherwise, specifies how to handle transparency when rendering.
 * @alpha_threshold: If the image does have opacity information and @alpha_mode
 * is GDK_PIXBUF_ALPHA_BILEVEL, specifies the threshold value for opacity
 * values.
 * @dither: Dithering mode for GdkRGB.
 * @x_dither: X offset for dither.
 * @y_dither: Y offset for dither.
 *
 * Renders a rectangular portion of a pixbuf to a drawable.  This is done using
 * GdkRGB, so the specified drawable must have the GdkRGB visual and colormap.
 *
 * When used with #GDK_PIXBUF_ALPHA_BILEVEL, this function has to create a bitmap
 * out of the thresholded alpha channel of the image and, it has to set this
 * bitmap as the clipping mask for the GC used for drawing.  This can be a
 * significant performance penalty depending on the size and the complexity of
 * the alpha channel of the image.  If performance is crucial, consider handling
 * the alpha channel yourself (possibly by caching it in your application) and
 * using gdk_pixbuf_render_to_drawable() or GdkRGB directly instead.
 **/
void
gdk_pixbuf_render_to_drawable_alpha (GdkPixbuf   *pixbuf,
				     GdkDrawable *drawable,
				     int src_x,    int src_y,
				     int dest_x,   int dest_y,
				     int width,    int height,
				     GdkPixbufAlphaMode alpha_mode,
				     int                alpha_threshold,
				     GdkRgbDither       dither,
				     int x_dither, int y_dither)
{
  GdkBitmap *bitmap = NULL;
  GdkGC *gc;

  g_return_if_fail (pixbuf != NULL);
  g_return_if_fail (pixbuf->colorspace == GDK_COLORSPACE_RGB);
  g_return_if_fail (pixbuf->n_channels == 3 || pixbuf->n_channels == 4);
  g_return_if_fail (pixbuf->bits_per_sample == 8);

  g_return_if_fail (drawable != NULL);
  g_return_if_fail (width >= 0 && height >= 0);
  g_return_if_fail (src_x >= 0 && src_x + width <= pixbuf->width);
  g_return_if_fail (src_y >= 0 && src_y + height <= pixbuf->height);

  if (width == 0 || height == 0)
    return;

  gc = gdk_gc_new (drawable);

  if (pixbuf->has_alpha)
    {
      /* Right now we only support GDK_PIXBUF_ALPHA_BILEVEL, so we
       * unconditionally create the clipping mask.
       */

      bitmap = gdk_pixmap_new (NULL, width, height, 1);
      gdk_pixbuf_render_threshold_alpha (pixbuf, bitmap,
					 src_x, src_y,
					 0, 0,
					 width, height,
					 alpha_threshold);

      gdk_gc_set_clip_mask (gc, bitmap);
      gdk_gc_set_clip_origin (gc, dest_x, dest_y);
    }

  gdk_pixbuf_render_to_drawable (pixbuf, drawable, gc,
				 src_x, src_y,
				 dest_x, dest_y,
				 width, height,
				 dither,
				 x_dither, y_dither);

  if (bitmap)
    gdk_bitmap_unref (bitmap);

  gdk_gc_unref (gc);
}

/**
 * gdk_pixbuf_render_pixmap_and_mask:
 * @pixbuf: A pixbuf.
 * @pixmap_return: Return value for the created pixmap.
 * @mask_return: Return value for the created mask.
 * @alpha_threshold: Threshold value for opacity values.
 *
 * Creates a pixmap and a mask bitmap which are returned in the @pixmap_return
 * and @mask_return arguments, respectively, and renders a pixbuf and its
 * corresponding tresholded alpha mask to them.  This is merely a convenience
 * function; applications that need to render pixbufs with dither offsets or to
 * given drawables should use gdk_pixbuf_render_to_drawable_alpha() or
 * gdk_pixbuf_render_to_drawable(), and gdk_pixbuf_render_threshold_alpha().
 *
 * If the pixbuf does not have an alpha channel, then *@mask_return will be set
 * to NULL.
 **/
void
gdk_pixbuf_render_pixmap_and_mask (GdkPixbuf  *pixbuf,
				   GdkPixmap **pixmap_return,
				   GdkBitmap **mask_return,
				   int         alpha_threshold)
{
  g_return_if_fail (pixbuf != NULL);
  
  if (pixmap_return)
    {
      GdkGC *gc;
      
      *pixmap_return = gdk_pixmap_new (NULL, pixbuf->width, pixbuf->height,
				       gdk_rgb_get_visual ()->depth);
      gc = gdk_gc_new (*pixmap_return);
      gdk_pixbuf_render_to_drawable (pixbuf, *pixmap_return, gc,
				     0, 0, 0, 0,
				     pixbuf->width, pixbuf->height,
				     GDK_RGB_DITHER_NORMAL,
				     0, 0);
      gdk_gc_unref (gc);
    }
  
  if (mask_return)
    {
      if (pixbuf->has_alpha)
	{
	  *mask_return = gdk_pixmap_new (NULL, pixbuf->width, pixbuf->height, 1);
	  gdk_pixbuf_render_threshold_alpha (pixbuf, *mask_return,
					     0, 0, 0, 0,
					     pixbuf->width, pixbuf->height,
					     alpha_threshold);
	}
      else
	*mask_return = NULL;
    }
}
