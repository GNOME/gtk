/* GdkPixbuf library - Image creation from in-memory buffers
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
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-i18n.h"
#include <stdlib.h>
#include <string.h>



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

	pixbuf = g_object_new (GDK_TYPE_PIXBUF, NULL);
        
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

static guint32
read_int (const guchar **p)
{
        guint32 num;

        /* Note most significant bytes are first in the byte stream */
        num =
          (*p)[3]         |
          ((*p)[2] << 8)  |
          ((*p)[1] << 16) |
          ((*p)[0] << 24);

        *p += 4;

        return num;
}

static gboolean
read_bool (const guchar **p)
{
        gboolean val = **p != 0;
        
        ++(*p);
        
        return val;
}

static GdkPixbuf*
read_raw_inline (const guchar *data,
                 gboolean      copy_pixels,
                 int           length,
                 GError      **error)
{
        GdkPixbuf *pixbuf;
        const guchar *p = data;
        guint32 rowstride, width, height, colorspace,
                n_channels, bits_per_sample;
        gboolean has_alpha;
        
        if (length >= 0 && length < 12) {
                /* Not enough buffer to hold the width/height/rowstride */
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image data is partially missing"));
                
                return NULL;
        }

        rowstride = read_int (&p);
        width = read_int (&p);
        height = read_int (&p);

        if (rowstride < width) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image has an incorrect pixel rowstride, perhaps the data was corrupted somehow."));
                return NULL; /* bad data from untrusted source. */
        }
        
        /* rowstride >= width, so we can trust width */
        
        length -= 12;

        /* There's some better way like G_MAXINT/height > rowstride
         * but I'm not sure it works, so stick to this for now.
         */
        if (((double)height) * ((double)rowstride) > (double)G_MAXINT) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image size is impossibly large, perhaps the data was corrupted somehow"));
                
                return NULL; /* overflow */
        }

        if (length >= 0 &&
            length < (height * rowstride + 13)) {
                /* Not enough buffer to hold the remaining header
                 * information plus the data.
                 */
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image data is partially missing, probably it was corrupted somehow."));
                
                return NULL;
        }

        /* Read the remaining 13 bytes of header information */
            
        has_alpha = read_bool (&p) != FALSE;
        colorspace = read_int (&p);
        n_channels = read_int (&p);
        bits_per_sample = read_int (&p);
        
        if (colorspace != GDK_COLORSPACE_RGB) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image has an unknown colorspace code (%d), perhaps the image data was corrupted"),
                             colorspace);
                return NULL;
        }

        if (bits_per_sample != 8) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image has an improper number of bits per sample (%d), perhaps the image data was corrupted"),
                             bits_per_sample);
                return NULL;
        }
        
        if (has_alpha && n_channels != 4) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image has an improper number of channels (%d), perhaps the image data was corrupted"),
                             n_channels);
                return NULL;
        }
        
        if (!has_alpha && n_channels != 3) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image has an improper number of channels (%d), perhaps the image data was corrupted"),
                             n_channels);
                return NULL;
        }
        
        if (copy_pixels) {
                guchar *pixels;
                gint dest_rowstride;
                gint row;
                
                pixbuf = gdk_pixbuf_new (colorspace,
                                         has_alpha, bits_per_sample,
                                         width, height);
                
                if (pixbuf == NULL) {
                        g_set_error (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Not enough memory to store a %d by %d image; try exiting some applications to free memory."),
                                     width, height);
                        return NULL;
                }
                
                pixels = gdk_pixbuf_get_pixels (pixbuf);
                dest_rowstride = gdk_pixbuf_get_rowstride (pixbuf);
                
                for (row = 0; row < height; row++) {
                        memcpy (pixels, p, rowstride);
                        pixels += dest_rowstride;
                        p += rowstride;
                }
        } else {
                pixbuf = gdk_pixbuf_new_from_data (p,
                                                   colorspace,
                                                   has_alpha,
                                                   bits_per_sample,
                                                   width, height,
                                                   rowstride,
                                                   NULL, NULL);
        }
        
        return pixbuf;
}

/**
 * gdk_pixbuf_new_from_inline:
 * @inline_pixbuf: An inlined GdkPixbuf
 * @copy_pixels: whether to copy the pixels out of the inline data, or to use them in-place
 * @length: length of the inline data
 * @error: return location for error
 *
 * Create a #GdkPixbuf from a custom format invented to store pixbuf
 * data in C program code. This library comes with a program called
 * "make-inline-pixbuf" that can write out a variable definition
 * containing an inlined pixbuf.  This is useful if you want to ship a
 * program with images, but don't want to depend on any external
 * files.
 * 
 * The inline data format contains the pixels in #GdkPixbuf's native
 * format.  Since the inline pixbuf is read-only static data, you
 * don't need to copy it unless you intend to write to it.
 * 
 * If you create a pixbuf from const inline data compiled into your
 * program, it's probably safe to ignore errors, since things will
 * always succeed.  For non-const inline data, you could get out of
 * memory. For untrusted inline data located at runtime, you could
 * have corrupt inline data in addition.
 * 
 * Return value: A newly-created #GdkPixbuf structure with a reference count of
 * 1, or NULL If error is set.
 **/
GdkPixbuf*
gdk_pixbuf_new_from_inline   (const guchar *inline_pixbuf,
                              gboolean      copy_pixels,
                              int           length,
                              GError      **error)
{
        const guchar *p;
        GdkPixbuf *pixbuf;
        GdkPixbufInlineFormat format;

        if (length >= 0 && length < 8) {
                /* not enough bytes to contain even the magic number
                 * and format code.
                 */
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image contained no data."));
                return NULL;
        }
        
        p = inline_pixbuf;

        if (read_int (&p) != GDK_PIXBUF_INLINE_MAGIC_NUMBER) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Image isn't in the correct format (inline GdkPixbuf format)"));
                return NULL;
        }

        format = read_int (&p);

        switch (format)
        {
        case GDK_PIXBUF_INLINE_RAW:
                pixbuf = read_raw_inline (p, copy_pixels, length - 8, error);
                break;

        default:
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_UNKNOWN_TYPE,
                             _("This version of the software is unable to read images with type code %d"),
                             format);
                return NULL;
        }

        return pixbuf;
}

