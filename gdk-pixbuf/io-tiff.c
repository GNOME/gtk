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

/* Following code (almost) blatantly ripped from Imlib */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <tiffio.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"



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
	TIFF *tiff;
	guchar *pixels = NULL;
	guchar *tmppix;
	gint w, h, x, y, num_pixs, fd;
	uint32 *rast, *tmp_rast;

	fd = fileno (f);
	tiff = TIFFFdOpen (fd, "libpixbuf-tiff", "r");

	if (!tiff)
		return NULL;

	TIFFGetField (tiff, TIFFTAG_IMAGEWIDTH, &w);
	TIFFGetField (tiff, TIFFTAG_IMAGELENGTH, &h);
	num_pixs = w * h;

	/* Yes, it needs to be _TIFFMalloc... */
	rast = (uint32 *) _TIFFmalloc (num_pixs * sizeof (uint32));

	if (!rast) {
		TIFFClose (tiff);
		return NULL;
	}

	if (TIFFReadRGBAImage (tiff, w, h, rast, 0)) {
		pixels = malloc (num_pixs * 4);
		if (!pixels) {
			_TIFFfree (rast);
			TIFFClose (tiff);
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
				tmppix[0] = TIFFGetR (*tmp_rast);
				tmppix[1] = TIFFGetG (*tmp_rast);
				tmppix[2] = TIFFGetB (*tmp_rast);
				tmppix[3] = TIFFGetA (*tmp_rast);
				tmp_rast++;
				tmppix += 4;
			}
		}
	}
	_TIFFfree (rast);
	TIFFClose (tiff);

	return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, TRUE,
					 w, h, w * 4,
					 free_buffer, NULL);
}

/* Progressive loader */

/*
 * Tiff loading progressively cannot be done.  We write it to a file, then load
 * the file when it's done.  
 */

typedef struct _TiffData TiffData;
struct _TiffData
{
	ModulePreparedNotifyFunc *func;
	gpointer user_data;

	FILE *file;
};

gpointer
image_begin_load (ModulePreparedNotifyFunc *func, gpointer user_data)
{
	TiffData *context;
	gint fd;
	char template[21];
	
	context = g_new (TiffData, 1);
	context->func = func;
	context->user_data = user_data;

	strncpy (template, "/tmp/temp-tiffXXXXXX", strlen ("/tmp/temp-tiffXXXXXX"));
	fd = mkstemp (template);
	g_print ("fd=%d\n", fd);
	context->file = fdopen (fd, "w");
	if (context->file == NULL)
		g_print ("it's null\n");
	else
		g_print ("it's not null\n");
	return context;
}

void
image_stop_load (gpointer data)
{
	TiffData *context = (TiffData*) data;
	fclose (context->file);
/*	unlink (context->file);*/
	g_free ((TiffData *) context);
}

gboolean
image_load_increment (gpointer data, guchar *buf, guint size)
{
	TiffData *context = (TiffData *) data;

	g_assert (context->file != NULL);
	fwrite (buf, sizeof (guchar), size, context->file);
	return TRUE;
}
