/*
 * io-gif.c: GdkPixBuf I/O for GIF files.
 * ...second verse, same as the first...
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
#include <gif_lib.h>

/* Shared library entry point */
GdkPixBuf *image_load(FILE * f)
{
    gint fn, is_trans = FALSE;
    gint done = 0;
    gint t_color = -1;
    gint w, h, i, j;
    art_u8 *pixels, *tmpptr;
    GifFileType *gif;
    GifRowType *rows;
    GifRecordType rec;
    ColorMapObject *cmap;
    int intoffset[] =
    {0, 4, 2, 1};
    int intjump[] =
    {8, 8, 4, 2};

    GdkPixBuf *pixbuf;

    g_return_val_if_fail(f != NULL, NULL);

    fn = fileno(f);
/*    lseek(fn, 0, 0);*/
    gif = DGifOpenFileHandle(fn);

    if (!gif) {
	g_error("DGifOpenFilehandle FAILED");
	PrintGifError();
	return NULL;
    }
    /* Now we do the ungodly mess of loading a GIF image
     * I used to remember when I liked this file format...
     * of course, I still coded in assembler then.
     * This comes from gdk_imlib, with some cleanups.
     */

    do {
	if (DGifGetRecordType(gif, &rec) == GIF_ERROR) {
	    PrintGifError();
	    rec = TERMINATE_RECORD_TYPE;
	}
	if ((rec == IMAGE_DESC_RECORD_TYPE) && (!done)) {
	    if (DGifGetImageDesc(gif) == GIF_ERROR) {
		PrintGifError();
		rec = TERMINATE_RECORD_TYPE;
	    }
	    w = gif->Image.Width;
	    h = gif->Image.Height;
	    rows = g_malloc0(h * sizeof(GifRowType *));
	    if (!rows) {
		DGifCloseFile(gif);
		return NULL;
	    }
	    for (i = 0; i < h; i++) {
		rows[i] = g_malloc0(w * sizeof(GifPixelType));
		if (!rows[i]) {
		    DGifCloseFile(gif);
		    for (i = 0; i < h; i++)
			if (rows[i])
			    g_free(rows[i]);
		    free(rows);
		    return NULL;
		}
	    }
	    if (gif->Image.Interlace) {
		for (i = 0; i < 4; i++) {
		    for (j = intoffset[i]; j < h; j += intjump[i])
			DGifGetLine(gif, rows[j], w);
		}
	    } else {
		for (i = 0; i < h; i++)
		    DGifGetLine(gif, rows[i], w);
	    }
	    done = 1;
	} else if (rec == EXTENSION_RECORD_TYPE) {
	    gint ext_code;
	    GifByteType *ext;

	    DGifGetExtension(gif, &ext_code, &ext);
	    while (ext) {
		if ((ext_code == 0xf9) &&
		    (ext[1] & 1) && (t_color < 0)) {
		    is_trans = TRUE;
		    t_color = (gint) ext[4];
		}
		ext = NULL;
		DGifGetExtensionNext(gif, &ext);
	    }
	}
    }
    while (rec != TERMINATE_RECORD_TYPE);

    /* Ok, we're loaded, now to convert from indexed -> RGB
     * with alpha if necessary
     */

    if (is_trans)
	pixels = art_alloc(h * w * 4);
    else
	pixels = art_alloc(h * w * 3);
    tmpptr = pixels;

    if (!pixels)
	return NULL;

    /* The meat of the transformation */
    /* Get the right palette */
    cmap = (gif->Image.ColorMap ? gif->Image.ColorMap : gif->SColorMap);

    /* Unindex the data, and pack it in RGB(A) order.
     * Note for transparent GIFs, the alpha is set to 0
     * for the transparent color, and 0xFF for everything else.
     * I think that's right...
     */

    for (i = 0; i < h; i++) {
	for (j = 0; j < w; j++) {
	    tmpptr[0] = cmap->Colors[rows[i][j]].Red;
	    tmpptr[1] = cmap->Colors[rows[i][j]].Green;
	    tmpptr[2] = cmap->Colors[rows[i][j]].Blue;
	    if (is_trans && (rows[i][j] == t_color))
		tmpptr[3] = 0;
	    else
		tmpptr[3] = 0xFF;
	    tmpptr += (is_trans ? 4 : 3);
	}
    }

    /* Ok, now stuff the GdkPixBuf with goodies */

    pixbuf = g_new(GdkPixBuf, 1);

    if (is_trans)
	pixbuf->art_pixbuf = art_pixbuf_new_rgba(pixels, w, h, (w * 4));
    else
	pixbuf->art_pixbuf = art_pixbuf_new_rgb(pixels, w, h, (w * 3));

    /* Ok, I'm anal...shoot me */
    if (!(pixbuf->art_pixbuf))
	return NULL;
    pixbuf->ref_count = 0;
    pixbuf->unref_func = NULL;

    return pixbuf;
}

image_save() {}
