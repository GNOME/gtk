/* GdkPixbuf library - Basic memory management
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Miguel de Icaza <miguel@gnu.org>
 *          Federico Mena-Quintero <federico@gimp.org>
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
#include <math.h>
#include <libart_lgpl/art_misc.h>
#include <libart_lgpl/art_affine.h>
#include <libart_lgpl/art_pixbuf.h>
#include <libart_lgpl/art_rgb_pixbuf_affine.h>
#include <libart_lgpl/art_alphagamma.h>
#include "gdk-pixbuf.h"



/* Reference counting */

/**
 * gdk_pixbuf_ref:
 * @pixbuf: A pixbuf.
 *
 * Adds a reference to a pixbuf.  It must be released afterwards using
 * gdk_pixbuf_unref().
 **/
void
gdk_pixbuf_ref (GdkPixbuf *pixbuf)
{
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (pixbuf->ref_count > 0);

	pixbuf->ref_count++;
}

/**
 * gdk_pixbuf_unref:
 * @pixbuf: A pixbuf.
 *
 * Removes a reference from a pixbuf.  It will be destroyed when the reference
 * count drops to zero.
 **/
void
gdk_pixbuf_unref (GdkPixbuf *pixbuf)
{
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (pixbuf->ref_count > 0);

	pixbuf->ref_count--;

	if (pixbuf->ref_count == 0) {
		art_pixbuf_free (pixbuf->art_pixbuf);
		pixbuf->art_pixbuf = NULL;
		g_free (pixbuf);
	}
}



/* Wrap a libart pixbuf */

/**
 * gdk_pixbuf_new_from_art_pixbuf:
 * @art_pixbuf: A libart pixbuf.
 *
 * Creates a #GdkPixbuf by wrapping a libart pixbuf.
 *
 * Return value: A newly-created #GdkPixbuf structure with a reference count of
 * 1.
 **/
GdkPixbuf *
gdk_pixbuf_new_from_art_pixbuf (ArtPixBuf *art_pixbuf)
{
	GdkPixbuf *pixbuf;

	g_return_val_if_fail (art_pixbuf != NULL, NULL);

	pixbuf = g_new (GdkPixbuf, 1);
	pixbuf->ref_count = 1;
	pixbuf->art_pixbuf = art_pixbuf;

	return pixbuf;
}

/* Destroy notification function for gdk_pixbuf_new() */
static void
free_buffer (gpointer user_data, gpointer data)
{
	free (data);
}



/* Create an empty pixbuf */

/**
 * gdk_pixbuf_new:
 * @format: Image format.
 * @has_alpha: Whether the image should have transparency information.
 * @bits_per_sample: Number of bits per color sample.
 * @width: Width of image in pixels.
 * @height: Height of image in pixels.
 *
 * Creates a new #GdkPixbuf structure and allocates a buffer for it.  The buffer
 * has an optimal rowstride.  Note that the buffer is not cleared; you will have
 * to fill it completely.
 *
 * Return value: A newly-created #GdkPixbuf with a reference count of 1, or NULL
 * if not enough memory could be allocated for the image buffer.
 **/
GdkPixbuf *
gdk_pixbuf_new (ArtPixFormat format, gboolean has_alpha, int bits_per_sample,
		int width, int height)
{
	guchar *buf;
	int channels;
	int rowstride;

	g_return_val_if_fail (format == ART_PIX_RGB, NULL);
	g_return_val_if_fail (bits_per_sample == 8, NULL);
	g_return_val_if_fail (width > 0, NULL);
	g_return_val_if_fail (height > 0, NULL);

	/* Always align rows to 32-bit boundaries */

	channels = has_alpha ? 4 : 3;
	rowstride = 4 * ((channels * width + 3) / 4);

	buf = malloc (height * rowstride);
	if (!buf)
		return NULL;

	return gdk_pixbuf_new_from_data (buf, format, has_alpha, width, height, rowstride,
					 free_buffer, NULL);
}



/* Convenience functions */

/**
 * gdk_pixbuf_get_format:
 * @pixbuf: A pixbuf.
 *
 * Queries the image format (color model) of a pixbuf.
 *
 * Return value: Image format.
 **/
ArtPixFormat
gdk_pixbuf_get_format (GdkPixbuf *pixbuf)
{
	/* Unfortunately, there's nothing else to return */
	g_return_val_if_fail (pixbuf != NULL, ART_PIX_RGB);
	g_assert (pixbuf->art_pixbuf != NULL);

	return (pixbuf->art_pixbuf->format);
}

/**
 * gdk_pixbuf_get_n_channels:
 * @pixbuf: A pixbuf.
 *
 * Queries the number of channels of a pixbuf.
 *
 * Return value: Number of channels.
 **/
int
gdk_pixbuf_get_n_channels (GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);
	g_assert (pixbuf->art_pixbuf != NULL);

	return (pixbuf->art_pixbuf->n_channels);
}

/**
 * gdk_pixbuf_get_has_alpha:
 * @pixbuf: A pixbuf.
 *
 * Queries whether a pixbuf has an alpha channel (opacity information).
 *
 * Return value: TRUE if it has an alpha channel, FALSE otherwise.
 **/
int
gdk_pixbuf_get_has_alpha (GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);
	g_assert (pixbuf->art_pixbuf != NULL);

	return (pixbuf->art_pixbuf->has_alpha);
}

/**
 * gdk_pixbuf_get_bits_per_sample:
 * @pixbuf: A pixbuf.
 *
 * Queries the number of bits per color sample in a pixbuf.
 *
 * Return value: Number of bits per color sample.
 **/
int
gdk_pixbuf_get_bits_per_sample (GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);
	g_assert (pixbuf->art_pixbuf != NULL);

	return (pixbuf->art_pixbuf->bits_per_sample);
}

/**
 * gdk_pixbuf_get_pixels:
 * @pixbuf: A pixbuf.
 *
 * Queries a pointer to the pixel data of a pixbuf.
 *
 * Return value: A pointer to the pixbuf's pixel data.
 **/
guchar *
gdk_pixbuf_get_pixels (GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, NULL);
	g_assert (pixbuf->art_pixbuf != NULL);

	return (pixbuf->art_pixbuf->pixels);
}

/**
 * gdk_pixbuf_get_width:
 * @pixbuf: A pixbuf.
 *
 * Queries the width of a pixbuf.
 *
 * Return value: Width in pixels.
 **/
int
gdk_pixbuf_get_width (GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);
	g_assert (pixbuf->art_pixbuf != NULL);

	return (pixbuf->art_pixbuf->width);
}

/**
 * gdk_pixbuf_get_height:
 * @pixbuf: A pixbuf.
 *
 * Queries the height of a pixbuf.
 *
 * Return value: Height in pixels.
 **/
int
gdk_pixbuf_get_height (GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);
	g_assert (pixbuf->art_pixbuf != NULL);

	return (pixbuf->art_pixbuf->height);
}

/**
 * gdk_pixbuf_get_rowstride:
 * @pixbuf: A pixbuf.
 *
 * Queries the rowstride of a pixbuf, or the number of bytes between rows.
 *
 * Return value: Number of bytes between rows.
 **/
int
gdk_pixbuf_get_rowstride (GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);
	g_assert (pixbuf->art_pixbuf != NULL);

	return (pixbuf->art_pixbuf->rowstride);
}
