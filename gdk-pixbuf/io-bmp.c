/*
 * io-bmp.c: GdkPixBuf I/O for BMP files.
 *
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

#include <config.h>
#include <stdio.h>
#include <glib.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"
#include "io-bmp.h"

/* Loosely based off the BMP loader from The GIMP, hence it's complexity */

/* Shared library entry point */
GdkPixBuf *image_load(FILE * f)
{
    GdkPixBuf *pixbuf;
    art_u8 *pixels;

    /* Ok, now stuff the GdkPixBuf with goodies */

    pixbuf = g_new(GdkPixBuf, 1);

    if (is_trans)
	pixbuf->art_pixbuf = art_pixbuf_new_rgba(pixels, w, h, (w * 4));
    else
	pixbuf->art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, (w * 3));

    /* Ok, I'm anal...shoot me */
    if (!(pixbuf->art_pixbuf))
	return NULL;
    pixbuf->ref_count = 1;
    pixbuf->unref_func = NULL;

    return pixbuf;
}
