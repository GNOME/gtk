/* GdkPixbuf library - Utilities and miscellaneous convenience functions
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Federico Mena-Quintero <federico@gimp.org>
 *          Cody Russell  <bratsche@dfw.net>
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
#include "gdk-pixbuf-private.h"



/**
 * gdk_pixbuf_add_alpha:
 * @pixbuf: A pixbuf.
 * @substitute_color: Whether to substitute a color for zero opacity.  If this
 * is #FALSE, then the (@r, @g, @b) arguments will be ignored.
 * @r: Red value to substitute.
 * @g: Green value to substitute.
 * @b: Blue value to substitute.
 *
 * Takes an existing pixbuf and adds an alpha channel to it.  If the original
 * pixbuf already had alpha information, then the contents of the new pixbuf are
 * exactly the same as the original's.  Otherwise, the new pixbuf will have all
 * pixels with full opacity if @substitute_color is #FALSE.  If
 * @substitute_color is #TRUE, then the color specified by (@r, @g, @b) will be
 * substituted for zero opacity.
 *
 * Return value: A newly-created pixbuf with a reference count of 1.
 **/
GdkPixbuf *
gdk_pixbuf_add_alpha (const GdkPixbuf *pixbuf,
		      gboolean substitute_color, guchar r, guchar g, guchar b)
{
	GdkPixbuf *new_pixbuf;
	int x, y;

	g_return_val_if_fail (pixbuf != NULL, NULL);
	g_return_val_if_fail (pixbuf->colorspace == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail (pixbuf->n_channels == 3 || pixbuf->n_channels == 4, NULL);
	g_return_val_if_fail (pixbuf->bits_per_sample == 8, NULL);

	if (pixbuf->has_alpha) {
		new_pixbuf = gdk_pixbuf_copy (pixbuf);
		if (!new_pixbuf)
			return NULL;

		return new_pixbuf;
	}

	new_pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, pixbuf->width, pixbuf->height);
	if (!new_pixbuf)
		return NULL;

	for (y = 0; y < pixbuf->height; y++) {
		guchar *src, *dest;
		guchar tr, tg, tb;

		src = pixbuf->pixels + y * pixbuf->rowstride;
		dest = new_pixbuf->pixels + y * new_pixbuf->rowstride;

		for (x = 0; x < pixbuf->width; x++) {
			tr = *dest++ = *src++;
			tg = *dest++ = *src++;
			tb = *dest++ = *src++;

			if (substitute_color && tr == r && tg == g && tb == b)
				*dest++ = 0;
			else
				*dest++ = 255;
		}
	}

	return new_pixbuf;
}

/**
 * gdk_pixbuf_copy_area:
 * @src_pixbuf: Source pixbuf.
 * @src_x: Source X coordinate within @src_pixbuf.
 * @src_y: Source Y coordinate within @src_pixbuf.
 * @width: Width of the area to copy.
 * @height: Height of the area to copy.
 * @dest_pixbuf: Destination pixbuf.
 * @dest_x: X coordinate within @dest_pixbuf.
 * @dest_y: Y coordinate within @dest_pixbuf.
 *
 * Copies a rectangular area from @src_pixbuf to @dest_pixbuf.  Conversion of
 * pixbuf formats is done automatically.
 **/
void
gdk_pixbuf_copy_area (const GdkPixbuf *src_pixbuf,
		      int src_x, int src_y,
		      int width, int height,
		      GdkPixbuf *dest_pixbuf,
		      int dest_x, int dest_y)
{
	g_return_if_fail (src_pixbuf != NULL);
	g_return_if_fail (dest_pixbuf != NULL);

	g_return_if_fail (src_x >= 0 && src_x + width <= src_pixbuf->width);
	g_return_if_fail (src_y >= 0 && src_y + height <= src_pixbuf->height);

	g_return_if_fail (dest_x >= 0 && dest_x + width <= dest_pixbuf->width);
	g_return_if_fail (dest_y >= 0 && dest_y + height <= dest_pixbuf->height);

	/* This will perform format conversions automatically */

	gdk_pixbuf_scale (src_pixbuf,
			  dest_pixbuf,
			  dest_x, dest_y,
			  width, height,
			  (double) (dest_x - src_x),
			  (double) (dest_y - src_y),
			  1.0, 1.0,
			  GDK_INTERP_NEAREST);
}
