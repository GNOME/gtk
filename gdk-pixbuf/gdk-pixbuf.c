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
 * Creates a &GdkPixbuf by wrapping a libart pixbuf.
 *
 * Return value: A newly-created &GdkPixbuf structure with a reference count of
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

/**
 * gdk_pixbuf_new:
 * @art_pixbuf: A libart pixbuf.
 *
 * Creates a &GdkPixbuf; magically sets the ArtPixFormat, rowstride, and creates
 * the buffer. Use gdk_pixbuf_new_from_data() to do things manually.
 *
 * Return value: A newly-created &GdkPixbuf structure with a reference count of
 * 1. Somewhat oddly, returns NULL if it can't allocate the buffer; this unusual
 * behavior is needed because images can be very large.
 **/
GdkPixbuf *
gdk_pixbuf_new (gboolean has_alpha, int width, int height)
{
        GdkPixbuf *pixbuf;

	g_return_val_if_fail (width > 0, NULL);
        g_return_val_if_fail (height > 0, NULL);

	pixbuf = g_new (GdkPixbuf, 1);
	pixbuf->ref_count = 1;
        
        if (has_alpha) {
                art_u8* pixels;
                int rowstride;

                /* FIXME, pick an optimal stride */                
                rowstride = 4*width;

                pixels = art_alloc(rowstride*height);

                if (pixels == NULL) {
                        g_free(pixbuf);
                        return NULL;
                }
                
                pixbuf->art_pixbuf = art_pixbuf_new_rgba(pixels, width, height, rowstride);
        } else {
                art_u8* pixels;
                int rowstride;

                /* FIXME, pick an optimal stride */
                rowstride = 3*width;

                pixels = art_alloc(rowstride*height);

                if (pixels == NULL) {
                        g_free(pixbuf);
                        return NULL;
                }

                pixbuf->art_pixbuf = art_pixbuf_new_rgb(pixels, width, height, rowstride);
        }
                
	return pixbuf;


}


