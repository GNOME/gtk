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
 * Takes the opacity values in a pixbuf and thresholds them to produce a
 * bi-level alpha mask that can be used as a clipping mask for a drawable.
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

void
gdk_pixbuf_render_to_drawable (GdkPixbuf *pixbuf, GdkDrawable *drawable,
			       int src_x, int src_y,
			       int dest_x, int dest_y,
			       int width, int height,
			       GdkPixbufAlphaMode alpha_mode,
			       int alpha_threshold,
			       GdkRgbDither dither,
			       int x_dither, int y_dither)
{
	ArtPixBuf *apb;
	ArtIRect dest_rect, req_rect, area_rect;
	GdkBitmap *bitmap;

	g_return_if_fail (pixbuf != NULL);
	apb = pixbuf->art_pixbuf;

	g_return_if_fail (apb->format == ART_PIX_RGB);
	g_return_if_fail (apb->n_channels == 3 || apb->n_channels == 4);
	g_return_if_fail (apb->bits_per_sample == 8);

	g_return_if_fail (drawable != NULL);
	g_return_if_fail (src_x >= 0 && src_x + width <= apb->width);
	g_return_if_fail (src_y >= 0 && src_y + height <= apb->height);


	bitmap = gdk_pixmap_new (NULL, width, height, 1);
}
