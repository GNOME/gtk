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
#include <stdlib.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-private.h"



/* Reference counting */

/**
 * gdk_pixbuf_ref:
 * @pixbuf: A pixbuf.
 *
 * Adds a reference to a pixbuf.  It must be released afterwards using
 * gdk_pixbuf_unref().
 *
 * Return value: The same as the @pixbuf argument.
 **/
GdkPixbuf *
gdk_pixbuf_ref (GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, NULL);
	g_return_val_if_fail (pixbuf->ref_count > 0, NULL);

	pixbuf->ref_count++;
	return pixbuf;
}

/**
 * gdk_pixbuf_unref:
 * @pixbuf: A pixbuf.
 *
 * Removes a reference from a pixbuf.  If this is the last reference for the
 * @pixbuf, then its last unref handler function will be called; if no handler
 * has been defined, then the pixbuf will be finalized.
 *
 * See also: gdk_pixbuf_set_last_unref_handler().
 **/
void
gdk_pixbuf_unref (GdkPixbuf *pixbuf)
{
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (pixbuf->ref_count > 0);

	if (pixbuf->ref_count > 1)
		pixbuf->ref_count--;
	else if (pixbuf->last_unref_fn)
		(* pixbuf->last_unref_fn) (pixbuf, pixbuf->last_unref_fn_data);
	else
		gdk_pixbuf_finalize (pixbuf);
}

/**
 * gdk_pixbuf_set_last_unref_handler:
 * @pixbuf: A pixbuf.
 * @last_unref_fn: Handler function for the last unref.
 * @last_unref_fn_data: Closure data to pass to the last unref handler function.
 * 
 * Sets the handler function for the @pixbuf's last unref handler.  When
 * gdk_pixbuf_unref() is called on this pixbuf and it has a reference count of
 * 1, i.e. its last reference, then the last unref handler will be called.  This
 * function should determine whether to finalize the pixbuf or just continue.
 * If it wishes to finalize the pixbuf, it should do so by calling
 * gdk_pixbuf_finalize().
 *
 * See also: gdk_pixbuf_finalize().
 **/
void
gdk_pixbuf_set_last_unref_handler (GdkPixbuf *pixbuf, GdkPixbufLastUnref last_unref_fn,
				   gpointer last_unref_fn_data)
{
	g_return_if_fail (pixbuf != NULL);

	pixbuf->last_unref_fn = last_unref_fn;
	pixbuf->last_unref_fn_data = last_unref_fn_data;
}

/**
 * gdk_pixbuf_finalize:
 * @pixbuf: A pixbuf with a reference count of 1.
 * 
 * Finalizes a pixbuf by calling its destroy notification function to free the
 * pixel data and freeing the pixbuf itself.  This function is meant to be
 * called only from within a #GdkPixbufLastUnref handler function, and the
 * specified @pixbuf must have a reference count of 1, i.e. its last reference.
 **/
void
gdk_pixbuf_finalize (GdkPixbuf *pixbuf)
{
	g_return_if_fail (pixbuf != NULL);
	g_return_if_fail (pixbuf->ref_count == 1);

	if (pixbuf->destroy_fn)
		(* pixbuf->destroy_fn) (pixbuf->pixels, pixbuf->destroy_fn_data);

	g_free (pixbuf);
}



/* Used as the destroy notification function for gdk_pixbuf_new() */
static void
free_buffer (guchar *pixels, gpointer data)
{
	free (pixels);
}

/**
 * gdk_pixbuf_new:
 * @colorspace: Color space for image.
 * @has_alpha: Whether the image should have transparency information.
 * @bits_per_sample: Number of bits per color sample.
 * @width: Width of image in pixels.
 * @height: Height of image in pixels.
 *
 * Creates a new #GdkPixbuf structure and allocates a buffer for it.  The buffer
 * has an optimal rowstride.  Note that the buffer is not cleared; you will have
 * to fill it completely yourself.
 *
 * Return value: A newly-created #GdkPixbuf with a reference count of 1, or NULL
 * if not enough memory could be allocated for the image buffer.
 **/
GdkPixbuf *
gdk_pixbuf_new (GdkColorspace colorspace, gboolean has_alpha, int bits_per_sample,
		int width, int height)
{
	guchar *buf;
	int channels;
	int rowstride;

	g_return_val_if_fail (colorspace == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail (bits_per_sample == 8, NULL);
	g_return_val_if_fail (width > 0, NULL);
	g_return_val_if_fail (height > 0, NULL);

	/* Always align rows to 32-bit boundaries */

	channels = has_alpha ? 4 : 3;
	rowstride = 4 * ((channels * width + 3) / 4);

	buf = malloc (height * rowstride);
	if (!buf)
		return NULL;

	return gdk_pixbuf_new_from_data (buf, colorspace, has_alpha, bits_per_sample,
					 width, height, rowstride,
					 free_buffer, NULL);
}

/**
 * gdk_pixbuf_copy:
 * @pixbuf: A pixbuf.
 * 
 * Creates a new #GdkPixbuf with a copy of the information in the specified
 * @pixbuf.
 * 
 * Return value: A newly-created pixbuf with a reference count of 1, or NULL if
 * not enough memory could be allocated.
 **/
GdkPixbuf *
gdk_pixbuf_copy (const GdkPixbuf *pixbuf)
{
	guchar *buf;
	int size;

	g_return_val_if_fail (pixbuf != NULL, NULL);

	/* Calculate a semi-exact size.  Here we copy with full rowstrides;
	 * maybe we should copy each row individually with the minimum
	 * rowstride?
	 */

	size = ((pixbuf->height - 1) * pixbuf->rowstride +
		pixbuf->width * ((pixbuf->n_channels * pixbuf->bits_per_sample + 7) / 8));

	buf = malloc (size * sizeof (guchar));
	if (!buf)
		return NULL;

	memcpy (buf, pixbuf->pixels, size);

	return gdk_pixbuf_new_from_data (buf,
					 pixbuf->colorspace, pixbuf->has_alpha,
					 pixbuf->bits_per_sample,
					 pixbuf->width, pixbuf->height,
					 pixbuf->rowstride,
					 free_buffer,
					 NULL);
}



/* Accessors */

/**
 * gdk_pixbuf_get_colorspace:
 * @pixbuf: A pixbuf.
 *
 * Queries the color space of a pixbuf.
 *
 * Return value: Color space.
 **/
GdkColorspace
gdk_pixbuf_get_colorspace (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, GDK_COLORSPACE_RGB);

	return pixbuf->colorspace;
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
gdk_pixbuf_get_n_channels (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);

	return pixbuf->n_channels;
}

/**
 * gdk_pixbuf_get_has_alpha:
 * @pixbuf: A pixbuf.
 *
 * Queries whether a pixbuf has an alpha channel (opacity information).
 *
 * Return value: TRUE if it has an alpha channel, FALSE otherwise.
 **/
gboolean
gdk_pixbuf_get_has_alpha (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);

	return pixbuf->has_alpha ? TRUE : FALSE;
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
gdk_pixbuf_get_bits_per_sample (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);

	return pixbuf->bits_per_sample;
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
gdk_pixbuf_get_pixels (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, NULL);

	return pixbuf->pixels;
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
gdk_pixbuf_get_width (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);

	return pixbuf->width;
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
gdk_pixbuf_get_height (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);

	return pixbuf->height;
}

/**
 * gdk_pixbuf_get_rowstride:
 * @pixbuf: A pixbuf.
 *
 * Queries the rowstride of a pixbuf, which is the number of bytes between rows.
 *
 * Return value: Number of bytes between rows.
 **/
int
gdk_pixbuf_get_rowstride (const GdkPixbuf *pixbuf)
{
	g_return_val_if_fail (pixbuf != NULL, -1);

	return pixbuf->rowstride;
}



/* General initialization hooks */
const guint gdk_pixbuf_major_version = GDK_PIXBUF_MAJOR;
const guint gdk_pixbuf_minor_version = GDK_PIXBUF_MINOR;
const guint gdk_pixbuf_micro_version = GDK_PIXBUF_MICRO;

const char *gdk_pixbuf_version = GDK_PIXBUF_VERSION;

void
gdk_pixbuf_preinit (gpointer app, gpointer modinfo)
{
}

void
gdk_pixbuf_postinit (gpointer app, gpointer modinfo)
{
}
