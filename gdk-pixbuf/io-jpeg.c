/* GdkPixbuf library - JPEG image loader
 *
 * Copyright (C) 1999 Michael Zucchi
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Michael Zucchi <zucchi@zedzone.mmc.com.au>
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
#include <stdio.h>
#include <setjmp.h>
#include <jpeglib.h>
#include "gdk-pixbuf.h"



/* error handler data */
struct error_handler_data {
	struct jpeg_error_mgr pub;
	sigjmp_buf setjmp_buffer;
};

static void
fatal_error_handler (j_common_ptr cinfo)
{
	/* FIXME:
	 * We should somehow signal what error occurred to the caller so the
	 * caller can handle the error message */
	struct error_handler_data *errmgr;

	errmgr = (struct error_handler_data *) cinfo->err;
	cinfo->err->output_message (cinfo);
	siglongjmp (errmgr->setjmp_buffer, 1);
	return;
}

/* Destroy notification function for the libart pixbuf */
static void
free_buffer (gpointer user_data, gpointer data)
{
	free (data);
}

/* Shared library entry point */
GdkPixbuf *
image_load (FILE *f)
{
	int w, h, i, j;
	guchar *pixels = NULL, *dptr;
	guchar *lines[4]; /* Used to expand rows, via rec_outbuf_height, from the header file:
			   * "* Usually rec_outbuf_height will be 1 or 2, at most 4."
			   */
	guchar **lptr;
	struct jpeg_decompress_struct cinfo;
	struct error_handler_data jerr;
	GdkPixbuf *pixbuf;

	/* setup error handler */
	cinfo.err = jpeg_std_error (&jerr.pub);
	jerr.pub.error_exit = fatal_error_handler;

	if (sigsetjmp (jerr.setjmp_buffer, 1)) {
		/* Whoops there was a jpeg error */
		if (pixels)
			free (pixels);

		jpeg_destroy_decompress (&cinfo);
		return NULL;
	}

	/* load header, setup */
	jpeg_create_decompress (&cinfo);
	jpeg_stdio_src (&cinfo, f);
	jpeg_read_header (&cinfo, TRUE);
	jpeg_start_decompress (&cinfo);
	cinfo.do_fancy_upsampling = FALSE;
	cinfo.do_block_smoothing = FALSE;

	w = cinfo.output_width;
	h = cinfo.output_height;

	pixels = malloc (h * w * 3);
	if (!pixels) {
		jpeg_destroy_decompress (&cinfo);
		return NULL;
	}

	dptr = pixels;

	/* decompress all the lines, a few at a time */

	while (cinfo.output_scanline < cinfo.output_height) {
		lptr = lines;
		for (i = 0; i < cinfo.rec_outbuf_height; i++) {
			*lptr++ = dptr;
			dptr += w * 3;
		}

		jpeg_read_scanlines (&cinfo, lines, cinfo.rec_outbuf_height);
		if (cinfo.output_components == 1) {
			/* Expand grey->colour.  Expand from the end of the
			 * memory down, so we can use the same buffer.
			 */
			for (i = cinfo.rec_outbuf_height - 1; i >= 0; i--) {
				guchar *from, *to;

				from = lines[i] + w - 1;
				to = lines[i] + w * 3 - 3;
				for (j = w - 1; j >= 0; j--) {
					to[0] = from[0];
					to[1] = from[0];
					to[2] = from[0];
					to -= 3;
					from--;
				}
			}
		}
	}

	jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);

	return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, FALSE,
					 w, h, w * 3,
					 free_buffer, NULL);

	return pixbuf;
}
