/* GdkPixbuf library - GIF image loader
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
#include <glib.h>
#include <gif_lib.h>
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
	gint fn, is_trans = FALSE;
	gint done = 0;
	gint t_color = -1;
	gint w, h, i, j;
	guchar *pixels, *tmpptr;
	GifFileType *gif;
	GifRowType *rows;
	GifRecordType rec;
	ColorMapObject *cmap;
	int intoffset[] = { 0, 4, 2, 1 };
	int intjump[] = { 8, 8, 4, 2 };

	fn = fileno (f);
	gif = DGifOpenFileHandle (fn);

	if (!gif) {
		g_warning ("DGifOpenFilehandle FAILED");
		PrintGifError ();
		return NULL;
	}

	/* Now we do the ungodly mess of loading a GIF image I used to remember
	 * when I liked this file format...  of course, I still coded in
	 * assembler then.  This comes from gdk_imlib, with some cleanups.
	 */

	do {
		if (DGifGetRecordType (gif, &rec) == GIF_ERROR) {
			PrintGifError ();
			DGifCloseFile (gif);
			return NULL;
		}

		if (rec == IMAGE_DESC_RECORD_TYPE && !done) {
			if (DGifGetImageDesc (gif) == GIF_ERROR) {
				PrintGifError ();
				DGifCloseFile (gif);
				return NULL;
			}

			/* Note the careful use of malloc/calloc vs. g_malloc;
			 * we want to fail gracefully if we run out of memory.
			 */

			w = gif->Image.Width;
			h = gif->Image.Height;
			rows = calloc (sizeof (GifRowType *), h);
			if (!rows) {
				DGifCloseFile (gif);
				return NULL;
			}

			for (i = 0; i < h; i++) {
				rows[i] = calloc (sizeof (GifPixelType), w);
				if (!rows[i]) {
					DGifCloseFile (gif);
					for (j = 0; j < h; j++)
						if (rows[j])
							free (rows[j]);

					free (rows);
					return NULL;
				}
			}

			if (gif->Image.Interlace)
				for (i = 0; i < 4; i++) {
					for (j = intoffset[i]; j < h; j += intjump[i])
						DGifGetLine (gif, rows[j], w);
				}
			else
				for (i = 0; i < h; i++)
					DGifGetLine (gif, rows[i], w);

			done = 1;
		} else if (rec == EXTENSION_RECORD_TYPE) {
			gint ext_code;
			GifByteType *ext;

			DGifGetExtension (gif, &ext_code, &ext);
			while (ext) {
				if ((ext_code == 0xf9) && (ext[1] & 1) && (t_color < 0)) {
					is_trans = TRUE;
					t_color = (gint) ext[4];
				}

				ext = NULL;
				DGifGetExtensionNext (gif, &ext);
			}
		}
	} while (rec != TERMINATE_RECORD_TYPE);

	/* Ok, we're loaded, now to convert from indexed -> RGB with alpha if necessary */

	if (is_trans)
		pixels = malloc (h * w * 4);
	else
		pixels = malloc (h * w * 3);

	tmpptr = pixels;

	if (!pixels) {
		DGifCloseFile (gif);
		for (i = 0; i < h; i++)
			if (rows[i])
				free (rows[i]);

		free (rows);
		return NULL;
	}

	cmap = (gif->Image.ColorMap ? gif->Image.ColorMap : gif->SColorMap);

	/* Unindex the data, and pack it in RGB(A) order */

	for (i = 0; i < h; i++) {
		for (j = 0; j < w; j++) {
			tmpptr[0] = cmap->Colors[rows[i][j]].Red;
			tmpptr[1] = cmap->Colors[rows[i][j]].Green;
			tmpptr[2] = cmap->Colors[rows[i][j]].Blue;

			if (is_trans) {
				if (rows[i][j] == t_color)
					tmpptr[3] = 0;
				else
					tmpptr[3] = 0xFF;

				tmpptr += 4;
			} else
				tmpptr += 3;
		}

		free (rows[i]);
	}

	free (rows);
	DGifCloseFile (gif);

	return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, is_trans,
					 w, h, is_trans ? (w * 4) : (w * 3),
					 free_buffer, NULL);
}
