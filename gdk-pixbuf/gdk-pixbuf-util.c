/* GdkPixbuf library - Utilities and miscellaneous convenience functions
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Federico Mena-Quintero <federico@gimp.org>
 *          Cody Russell  <bratsche@dfw.net>
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
#include "gdk-pixbuf.h"



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
gdk_pixbuf_add_alpha (GdkPixbuf *pixbuf, gboolean substitute_color, guchar r, guchar g, guchar b)
{
	ArtPixBuf *apb;
	ArtPixBuf *new_apb;
	GdkPixbuf *new_pixbuf;
	int x, y;

	g_return_val_if_fail (pixbuf != NULL, NULL);

	apb = pixbuf->art_pixbuf;
	g_return_val_if_fail (apb->format == ART_PIX_RGB, NULL);
	g_return_val_if_fail (apb->n_channels == 3 || apb->n_channels == 4, NULL);
	g_return_val_if_fail (apb->bits_per_sample == 8, NULL);

	if (apb->has_alpha) {
		new_apb = art_pixbuf_duplicate (apb);
		if (!new_apb)
			return NULL;

		return gdk_pixbuf_new_from_art_pixbuf (new_apb);
	}

	new_pixbuf = gdk_pixbuf_new (ART_PIX_RGB, TRUE, 8, apb->width, apb->height);
	if (!new_pixbuf)
		return NULL;

	new_apb = new_pixbuf->art_pixbuf;

	for (y = 0; y < apb->height; y++) {
		guchar *src, *dest;
		guchar tr, tg, tb;

		src = apb->pixels + y * apb->rowstride;
		dest = new_apb->pixels + y * new_apb->rowstride;

		for (x = 0; x < apb->width; x++) {
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
 * @src_pixbuf: The pixbuf to be copied.
 * @src_x: The X coordinate of the upper left corner of the area to copy.
 * @src_y: The Y coordinate of the upper left corner of the area to copy.
 * @width: The width of the area to copy.
 * @height: The height of the area to copy.
 * @dest_pixbuf: The pixbuf to store the copy in.
 * @dest_x: X coordinate for the upper left corner of the rectangle to draw to in @dest_pixbuf.
 * @dest_y: Y coordinate for the upper left corner of the rectangle to draw to in @dest_pixbuf.
 *
 * Takes a rectangle area beginning at (@src_x, @src_y) @width pixels wide
 * and @height pixels high from @src_pixbuf and copy it into @dest_pixbuf
 * at (@dest_x, @dest_y). @dest_pixbuf must already be created and must be
 * large enough to hold the requested area.
 *
 * Return value: void
 **/
void gdk_pixbuf_copy_area(GdkPixbuf *src_pixbuf,
			  gint src_x, gint src_y,
			  gint width, gint height,
			  GdkPixbuf *dest_pixbuf,
			  gint dest_x, gint dest_y)
{
	gint src_width, src_height, dest_width, dest_height;

	/* Ensure that we have a source pixbuf, and that the requested
	 * area is not larger than that pixbuf.
	 */
	g_return_if_fail(src_pixbuf != NULL);

	src_width = gdk_pixbuf_get_width(src_pixbuf);
	src_height = gdk_pixbuf_get_height(src_pixbuf);

	g_return_if_fail(src_x >= 0 && width <= src_width);
	g_return_if_fail(src_y >= 0 && height <= src_height);

	/* Ensure that we have a destination pixbuf, and that the
	 * requested area is not larger than that pixbuf.
	 */
	g_return_if_fail(dest_pixbuf != NULL);

	dest_width = gdk_pixbuf_get_width(dest_pixbuf);
	dest_height = gdk_pixbuf_get_height(dest_pixbuf);

	g_return_if_fail(dest_x >= 0 && width <= dest_width);
	g_return_if_fail(dest_y >= 0 && height <= dest_height);

	/* Scale 1:1 the source pixbuf into the destination pixbuf. */
	gdk_pixbuf_scale(src_pixbuf,
			 dest_pixbuf,
			 dest_x, dest_y,
			 width, height,
			 (double)(dest_x - src_x),
			 (double)(dest_y - src_y),
			 1., 1., ART_FILTER_NEAREST);
}
