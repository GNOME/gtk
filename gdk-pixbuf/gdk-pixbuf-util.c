/* GdkPixbuf library - Utilities and miscellaneous convenience functions
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
#include "gdk-pixbuf/gdk-pixbuf.h"



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
