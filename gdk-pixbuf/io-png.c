/* GdkPixbuf library - JPEG image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
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
#include <png.h>
#include "gdk-pixbuf.h"



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
	png_structp png_ptr;
	png_infop info_ptr, end_info;
	gint i, depth, ctype, inttype, passes, bpp;
	png_uint_32 w, h, x, y;
	png_bytepp rows;
	guchar *pixels, *temp, *rowdata;

	png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (!png_ptr)
		return NULL;

	info_ptr = png_create_info_struct (png_ptr);
	if (!info_ptr) {
		png_destroy_read_struct (&png_ptr, NULL, NULL);
		return NULL;
	}

	end_info = png_create_info_struct (png_ptr);
	if (!end_info) {
		png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
		return NULL;
	}

	if (setjmp (png_ptr->jmpbuf)) {
		png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	png_init_io (png_ptr, f);
	png_read_info (png_ptr, info_ptr);
	png_get_IHDR (png_ptr, info_ptr, &w, &h, &depth, &ctype, &inttype, NULL, NULL);

	/* Ok, we want to work with 24 bit images.
	 * However, PNG can vary depth per channel.
	 * So, we use the png_set_expand function to expand
	 * everything into a format libart expects.
	 * We also use png_set_strip_16 to reduce down to 8 bit/chan.
	 */
	if (ctype == PNG_COLOR_TYPE_PALETTE && depth <= 8)
		png_set_expand (png_ptr);

	if (ctype == PNG_COLOR_TYPE_GRAY && depth < 8)
		png_set_expand (png_ptr);

	if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS)) {
		png_set_expand (png_ptr);
		g_warning ("FIXME: We are going to crash");
	}

	if (depth == 16)
		png_set_strip_16 (png_ptr);

	/* We also have png "packing" bits into bytes if < 8 */
	if (depth < 8)
		png_set_packing (png_ptr);

	/* Lastly, if the PNG is greyscale, convert to RGB */
	if (ctype == PNG_COLOR_TYPE_GRAY || ctype == PNG_COLOR_TYPE_GRAY_ALPHA)
		png_set_gray_to_rgb (png_ptr);

	/* ...and if we're interlaced... */
	passes = png_set_interlace_handling (png_ptr);

	/* Update our info structs */
	png_read_update_info (png_ptr, info_ptr);

	/* Allocate some memory and set up row array */
	/* This "inhales vigorously"... */
	if (ctype & PNG_COLOR_MASK_ALPHA)
		bpp = 4;
	else
		bpp = 3;

	pixels = malloc (w * h * bpp);
	rows = malloc (h * sizeof (png_bytep));

	if (!pixels || !rows) {
		if (pixels)
			free (pixels);

		if (rows)
			free (rows);

		png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	/* Icky code, but it has to be done... */
	for (i = 0; i < h; i++) {
		if ((rows[i] = malloc (w * bpp)) == NULL) {
			int n;

			for (n = 0; n < i; n++)
				free (rows[i]);

			free (rows);
			free (pixels);
			png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);
			return NULL;
		}
	}

	/* And we FINALLY get here... */
	png_read_image (png_ptr, rows);
	png_destroy_read_struct (&png_ptr, &info_ptr, &end_info);

	/* Now stuff the bytes into pixels & free rows[y] */

	temp = pixels;

	for (y = 0; y < h; y++) {
		rowdata = rows[y];
		for (x = 0; x < w; x++) {
			temp[0] = rowdata[(x * bpp)];
			temp[1] = rowdata[(x * bpp) + 1];
			temp[2] = rowdata[(x * bpp) + 2];

			if (bpp == 4)
				temp[3] = rowdata[(x * bpp) + 3];

			temp += bpp;
		}
		free (rows[y]);
	}
	free (rows);

	if (ctype & PNG_COLOR_MASK_ALPHA)
		return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, TRUE,
						 w, h, w * 4,
						 free_buffer, NULL);
	else
		return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, FALSE,
						 w, h, w * 3,
						 free_buffer, NULL);
}
