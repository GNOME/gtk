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
#include <string.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-private.h"

static void gdk_pixbuf_class_init  (GdkPixbufClass *klass);
static void gdk_pixbuf_finalize    (GObject        *object);



static gpointer parent_class;

GType
gdk_pixbuf_get_type (void)
{
        static GType object_type = 0;

        if (!object_type) {
                static const GTypeInfo object_info = {
                        sizeof (GdkPixbufClass),
                        (GBaseInitFunc) NULL,
                        (GBaseFinalizeFunc) NULL,
                        (GClassInitFunc) gdk_pixbuf_class_init,
                        NULL,           /* class_finalize */
                        NULL,           /* class_data */
                        sizeof (GdkPixbuf),
                        0,              /* n_preallocs */
                        (GInstanceInitFunc) NULL,
                };
      
                object_type = g_type_register_static (G_TYPE_OBJECT,
                                                      "GdkPixbuf",
                                                      &object_info);
        }
  
        return object_type;
}

static void
gdk_pixbuf_class_init (GdkPixbufClass *klass)
{
        GObjectClass *object_class = G_OBJECT_CLASS (klass);
        
        parent_class = g_type_class_peek_parent (klass);
        
        object_class->finalize = gdk_pixbuf_finalize;
}

static void
gdk_pixbuf_finalize (GObject *object)
{
        GdkPixbuf *pixbuf = GDK_PIXBUF (object);
        
        if (pixbuf->destroy_fn)
                (* pixbuf->destroy_fn) (pixbuf->pixels, pixbuf->destroy_fn_data);
        
        G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gdk_pixbuf_ref:
 * @pixbuf: A pixbuf.
 *
 * Adds a reference to a pixbuf. Deprecated; use g_object_ref().
 *
 * Return value: The same as the @pixbuf argument.
 **/
GdkPixbuf *
gdk_pixbuf_ref (GdkPixbuf *pixbuf)
{
        return (GdkPixbuf *) g_object_ref (G_OBJECT(pixbuf));
}

/**
 * gdk_pixbuf_unref:
 * @pixbuf: A pixbuf.
 *
 * Removes a reference from a pixbuf. Deprecated; use
 * g_object_unref().
 *
 **/
void
gdk_pixbuf_unref (GdkPixbuf *pixbuf)
{
        g_object_unref (G_OBJECT (pixbuf));
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
        g_type_init ();
}

void
gdk_pixbuf_postinit (gpointer app, gpointer modinfo)
{
}

void
gdk_pixbuf_init (void)
{
        gdk_pixbuf_preinit (NULL, NULL);
        gdk_pixbuf_postinit (NULL, NULL);
}
