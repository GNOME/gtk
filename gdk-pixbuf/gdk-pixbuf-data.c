/* GdkPixbuf library - Image creation from in-memory buffers
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
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-private.h"



/**
 * gdk_pixbuf_new_from_data:
 * @data: Image data in 8-bit/sample packed format.
 * @colorspace: Colorspace for the image data.
 * @has_alpha: Whether the data has an opacity channel.
 * @bits_per_sample: Number of bits per sample.
 * @width: Width of the image in pixels.
 * @height: Height of the image in pixels.
 * @rowstride: Distance in bytes between rows.
 * @destroy_fn: Function used to free the data when the pixbuf's reference count
 * drops to zero, or NULL if the data should not be freed.
 * @destroy_fn_data: Closure data to pass to the destroy notification function.
 * 
 * Creates a new #GdkPixbuf out of in-memory image data.  Currently only RGB
 * images with 8 bits per sample are supported.
 * 
 * Return value: A newly-created #GdkPixbuf structure with a reference count of
 * 1.
 **/
GdkPixbuf *
gdk_pixbuf_new_from_data (const guchar *data, GdkColorspace colorspace, gboolean has_alpha,
			  int bits_per_sample, int width, int height, int rowstride,
			  GdkPixbufDestroyNotify destroy_fn, gpointer destroy_fn_data)
{
	GdkPixbuf *pixbuf;

	/* Only 8-bit/sample RGB buffers are supported for now */

	g_return_val_if_fail (data != NULL, NULL);
	g_return_val_if_fail (colorspace == GDK_COLORSPACE_RGB, NULL);
	g_return_val_if_fail (bits_per_sample == 8, NULL);
	g_return_val_if_fail (width > 0, NULL);
	g_return_val_if_fail (height > 0, NULL);

	pixbuf = GDK_PIXBUF (g_type_create_instance (GDK_TYPE_PIXBUF));
        
	pixbuf->colorspace = colorspace;
	pixbuf->n_channels = has_alpha ? 4 : 3;
	pixbuf->bits_per_sample = bits_per_sample;
	pixbuf->has_alpha = has_alpha ? TRUE : FALSE;
	pixbuf->width = width;
	pixbuf->height = height;
	pixbuf->rowstride = rowstride;
	pixbuf->pixels = (guchar *) data;
	pixbuf->destroy_fn = destroy_fn;
	pixbuf->destroy_fn_data = destroy_fn_data;

	return pixbuf;
}

int
read_int (const guchar **p)
{
        guint32 num;
        
        num = g_ntohl (* (guint32*) *p);
        
        *p += 4;
        
        return num;
}

gboolean
read_bool (const guchar **p)
{
        gboolean val = **p != 0;
        
        ++(*p);
        
        return val;
}

static void
free_buffer (guchar *pixels, gpointer data)
{
	free (pixels);
}

static GdkPixbuf*
read_raw_inline (const guchar *data, gboolean copy_pixels)
{
        GdkPixbuf *pixbuf;
        const guchar *p = data;
        
        pixbuf = GDK_PIXBUF (g_type_create_instance (GDK_TYPE_PIXBUF));

        pixbuf->rowstride = read_int (&p);
        pixbuf->width = read_int (&p);
        pixbuf->height = read_int (&p);
        pixbuf->has_alpha = read_bool (&p);
        pixbuf->colorspace = read_int (&p);
        pixbuf->n_channels = read_int (&p);
        pixbuf->bits_per_sample = read_int (&p);
  
        if (copy_pixels) {
                pixbuf->pixels = malloc (pixbuf->height * pixbuf->rowstride);

                if (pixbuf->pixels == NULL) {                        
                        g_object_unref (G_OBJECT (pixbuf));
                        return NULL;
                }

                pixbuf->destroy_fn = free_buffer;
                pixbuf->destroy_fn_data = NULL;

                memcpy (pixbuf->pixels, p, pixbuf->height * pixbuf->rowstride);
        } else {
                pixbuf->pixels = (guchar *) p;
        }

        return pixbuf;
}

/**
 * gdk_pixbuf_new_from_inline:
 * @data: An inlined GdkPixbuf
 * @copy_pixels: whether to copy the pixels out of the inline data, or to use them in-place
 *
 * Create a #GdkPixbuf from a custom format invented to store pixbuf
 * data in C program code. This library comes with a program called "image-to-inline"
 * that can write out a variable definition containing an inlined pixbuf.
 * This is useful if you want to ship a program with images, but
 * don't want to depend on any external files.
 * 
 * The inline data format contains the pixels in #GdkPixbuf's native format.
 * Since the inline pixbuf is static data, you don't really need to copy it.
 * However it's typically in read-only memory, so if you plan to modify
 * it you must copy it.
 * 
 * Return value: A newly-created #GdkPixbuf structure with a reference count of
 * 1.
 **/
GdkPixbuf*
gdk_pixbuf_new_from_inline   (const guchar *inline_pixbuf,
                              gboolean      copy_pixels)
{
        const guchar *p;
        GdkPixbuf *pixbuf;
        GdkPixbufInlineFormat format;
        
        p = inline_pixbuf;

        if (read_int (&p) != GDK_PIXBUF_INLINE_MAGIC_NUMBER) {
                g_warning ("Bad inline data; wrong magic number");
                return NULL;
        }

        format = read_int (&p);

        switch (format)
        {
        case GDK_PIXBUF_INLINE_RAW:
                pixbuf = read_raw_inline (p, copy_pixels);
                break;

        default:
                return NULL;
        }

        return pixbuf;
}

