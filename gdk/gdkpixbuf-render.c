/* GdkPixbuf library - Rendering functions
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Author: Federico Mena-Quintero <federico@gimp.org>
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

#include <config.h>
#include <gdk/gdk.h>
#include <libart_lgpl/art_rect.h>
#include "gdk-pixbuf.h"



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
gdk_pixbuf_render_threshold_alpha (GdkPixbuf *pixbuf, GdkBitmap *bitmap,
				   int src_x, int src_y,
				   int dest_x, int dest_y,
				   int width, int height,
				   int alpha_threshold)
{
	ArtPixBuf *apb;
	GdkGC *gc;
	GdkColor color;
	int x, y;
	guchar *p;
	int start, start_status;
	int status;

	g_return_if_fail (pixbuf != NULL);
	apb = pixbuf->art_pixbuf;

	g_return_if_fail (apb->format == ART_PIX_RGB);
	g_return_if_fail (apb->n_channels == 3 || apb->n_channels == 4);
	g_return_if_fail (apb->bits_per_sample == 8);

	g_return_if_fail (bitmap != NULL);
	g_return_if_fail (src_x >= 0 && src_x + width <= apb->width);
	g_return_if_fail (src_y >= 0 && src_y + height <= apb->height);

	g_return_if_fail (alpha_threshold >= 0 && alpha_threshold <= 255);

	gc = gdk_gc_new (bitmap);

	if (!apb->has_alpha) {
		color.pixel = (alpha_threshold == 255) ? 0 : 1;
		gdk_gc_set_foreground (gc, &color);
		gdk_draw_rectangle (bitmap, gc, TRUE, dest_x, dest_y, width, height);
		gdk_gc_unref (gc);
		return;
	}

	color.pixel = 0;
	gdk_gc_set_foreground (gc, &color);
	gdk_draw_rectangle (bitmap, gc, TRUE, dest_x, dest_y, width, height);

	for (y = 0; y < height; y++) {
		p = (apb->pixels + (y + src_y) * apb->rowstride + src_x * apb->n_channels
		     + apb->n_channels - 1);

		start = 0;
		start_status = *p < alpha_threshold;

		for (x = 0; x < width; x++) {
			status = *p < alpha_threshold;

			if (status != start_status) {
				color.pixel = start_status ? 0 : 1;
				gdk_gc_set_foreground (gc, &color);
				gdk_draw_line (bitmap, gc,
					       start + dest_x, y + dest_y,
					       x - 1 + dest_x, y + dest_y);

				start = x;
				start_status = status;
			}

			p += apb->n_channels;
		}

		color.pixel = start_status ? 0 : 1;
		gdk_gc_set_foreground (gc, &color);
		gdk_draw_line (bitmap, gc,
			       start + dest_x, y + dest_y,
			       x - 1 + dest_x, y + dest_y);
	}

	gdk_gc_unref (gc);
}



/* Creates a buffer by stripping the alpha channel of a pixbuf */
static guchar *
remove_alpha (ArtPixBuf *apb, int x, int y, int width, int height, int *rowstride)
{
	guchar *buf;
	int xx, yy;
	guchar *src, *dest;

	g_assert (apb->n_channels == 4);
	g_assert (apb->has_alpha);
	g_assert (x >= 0 && x + width <= apb->width);
	g_assert (y >= 0 && y + height <= apb->height);

	*rowstride = 4 * ((width * 3 + 3) / 4);

	buf = g_new (guchar, *rowstride * height);

	for (yy = 0; yy < height; yy++) {
		src = apb->pixels + apb->rowstride * (yy + y) + x * apb->n_channels;
		dest = buf + *rowstride * yy;

		for (xx = 0; xx < width; xx++) {
			*dest++ = *src++;
			*dest++ = *src++;
			*dest++ = *src++;
			src++;
		}
	}

	return buf;
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
 * For an explanation of dither offsets, see the GdkRGB documentation. In brief, the
 * dither offset is important when scrolling (so you can redraw half an image but keep the
 * dithering "lined up" between the part you drew first and the part you drew previously).
 * For unscrolled images, the offset can always be 0.
 **/
void
gdk_pixbuf_render_to_drawable (GdkPixbuf *pixbuf,
			       GdkDrawable *drawable, GdkGC *gc,
			       int src_x, int src_y,
			       int dest_x, int dest_y,
			       int width, int height,
			       GdkRgbDither dither,
			       int x_dither, int y_dither)
{
	ArtPixBuf *apb;
	guchar *buf;
	int rowstride;

	g_return_if_fail (pixbuf != NULL);
	apb = pixbuf->art_pixbuf;
	
	g_return_if_fail (apb->format == ART_PIX_RGB);
	g_return_if_fail (apb->n_channels == 3 || apb->n_channels == 4);
	g_return_if_fail (apb->bits_per_sample == 8);

	g_return_if_fail (drawable != NULL);
	g_return_if_fail (gc != NULL);

	g_return_if_fail (src_x >= 0 && src_x + width <= apb->width);
	g_return_if_fail (src_y >= 0 && src_y + height <= apb->height);

	/* This will have to be modified once libart supports other image types.
	 * Also, GdkRGB does not have gdk_draw_rgb_32_image_dithalign(), so we
	 * have to pack the buffer first.
	 */

	if (apb->has_alpha)
		buf = remove_alpha (apb, src_x, src_y, width, height, &rowstride);
	else {
		buf = apb->pixels + src_y * apb->rowstride + src_x * 3;
		rowstride = apb->rowstride;
	}

	gdk_draw_rgb_image_dithalign (drawable, gc,
				      dest_x, dest_y,
				      width, height,
				      dither,
				      buf, rowstride,
				      x_dither, y_dither);

	if (apb->has_alpha)
		g_free (buf);
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
 * This function has a performance penalty; it makes two synchronous
 * round trips to the X server (creating a bitmask and a GC), and it
 * draws the contents of the bitmask. If performance is crucial,
 * consider handling alpha yourself and using
 * gdk_pixbuf_render_to_drawable(). On the other hand it's more convenient
 * than gdk_pixbuf_render_to_drawable() because it handles the alpha channel.
 **/
void
gdk_pixbuf_render_to_drawable_alpha (GdkPixbuf *pixbuf, GdkDrawable *drawable,
				     int src_x, int src_y,
				     int dest_x, int dest_y,
				     int width, int height,
				     GdkPixbufAlphaMode alpha_mode,
				     int alpha_threshold,
				     GdkRgbDither dither,
				     int x_dither, int y_dither)
{
	ArtPixBuf *apb;
	GdkBitmap *bitmap;
	GdkGC *gc;

	g_return_if_fail (pixbuf != NULL);
	apb = pixbuf->art_pixbuf;

	g_return_if_fail (apb->format == ART_PIX_RGB);
	g_return_if_fail (apb->n_channels == 3 || apb->n_channels == 4);
	g_return_if_fail (apb->bits_per_sample == 8);

	g_return_if_fail (drawable != NULL);
	g_return_if_fail (src_x >= 0 && src_x + width <= apb->width);
	g_return_if_fail (src_y >= 0 && src_y + height <= apb->height);

	gc = gdk_gc_new (drawable);

	if (apb->has_alpha) {
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
		gdk_bitmap_unref (bitmap);
	}

	gdk_pixbuf_render_to_drawable (pixbuf, drawable, gc,
				       src_x, src_y,
				       dest_x, dest_y,
				       width, height,
				       dither,
				       x_dither, y_dither);

	gdk_gc_unref (gc);
}
