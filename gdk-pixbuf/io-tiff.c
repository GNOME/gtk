/*
 * io-tiff.c: GdkPixBuf I/O for TIFF files.
 * Copyright (C) 1999 Mark Crichton
 * Author: Mark Crichton <crichton@gimp.org>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Cambridge, MA 02139, USA.
 *
 */

/* Following code (almost) blatantly ripped from Imlib */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <tiffio.h>

#include "gdk-pixbuf.h"
/*#include "gdk-pixbuf-io.h" */

GdkPixBuf *image_load(FILE * f)
{
    GdkPixBuf *pixbuf;
    TIFF *tiff;
    art_u8 *pixels, *tmppix;
    gint w, h, x, y, num_pixs, fd;
    uint32 *rast, *tmp_rast;

    g_return_val_if_fail(f != NULL, NULL);

    fd = fileno(f);
    tiff = TIFFFdOpen(fd, "libpixbuf-tiff", "r");

    if (!tiff)
	return NULL;

    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &w);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &h);
    num_pixs = w * h;

    /* Yes, it needs to be _TIFFMalloc... */
    rast = (uint32 *) _TIFFmalloc(num_pixs * sizeof(uint32));

    if (!rast) {
	TIFFClose(tiff);
	return NULL;
    }

    if (TIFFReadRGBAImage(tiff, w, h, rast, 0)) {
	pixels = art_alloc(num_pixs * 4);
	if (!pixels) {
	    _TIFFfree(rast);
	    TIFFClose(tiff);
	    return NULL;
	}
	tmppix = pixels;

	for (y = 0; y < h; y++) {
	    /* Unexplainable...are tiffs backwards? */
	    /* Also looking at the GIMP plugin, this
	     * whole reading thing can be a bit more
	     * robust.
	     */
	    tmp_rast = rast + ((h - y - 1) * w);
	    for (x = 0; x < w; x++) {
		tmppix[0] = TIFFGetR(*tmp_rast);
		tmppix[1] = TIFFGetG(*tmp_rast);
		tmppix[2] = TIFFGetB(*tmp_rast);
		tmppix[3] = TIFFGetA(*tmp_rast);
		tmp_rast++;
		tmppix += 4;
	    }
	}
    }
    _TIFFfree(rast);
    TIFFClose(tiff);

    /* Return the GdkPixBuf */
    pixbuf = g_new(GdkPixBuf, 1);
    pixbuf->art_pixbuf = art_pixbuf_new_rgba(pixels, w, h, (w * 4));

    /* Ok, I'm anal...shoot me */
    if (!(pixbuf->art_pixbuf))
	return NULL;
    pixbuf->ref_count = 1;
    pixbuf->unref_func = NULL;

    return pixbuf;
}
