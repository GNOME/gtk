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
#include <unistd.h>



typedef struct _TiffData TiffData;
struct _TiffData
{
	ModulePreparedNotifyFunc func;
	gpointer user_data;

	gchar *tempname;
	FILE *file;
	gboolean all_okay;
};



GdkPixbuf *
image_load_real (FILE *f, TiffData *context)
{
	TIFF *tiff;
	guchar *pixels = NULL;
	guchar *tmppix;
	gint w, h, x, y, num_pixs, fd;
	uint32 *rast, *tmp_rast;
	GdkPixbuf *pixbuf;
	
	fd = fileno (f);
	tiff = TIFFFdOpen (fd, "libpixbuf-tiff", "r");

	if (!tiff)
		return NULL;

	TIFFGetField (tiff, TIFFTAG_IMAGEWIDTH, &w);
	TIFFGetField (tiff, TIFFTAG_IMAGELENGTH, &h);
	num_pixs = w * h;
	pixbuf = gdk_pixbuf_new (ART_PIX_RGB, TRUE, 8, w, h);

	if (context)
		(* context->func) (pixbuf, context->user_data);

	/* Yes, it needs to be _TIFFMalloc... */
	rast = (uint32 *) _TIFFmalloc (num_pixs * sizeof (uint32));

	if (!rast) {
		TIFFClose (tiff);
		return NULL;
	}

	if (TIFFReadRGBAImage (tiff, w, h, rast, 0)) {
		pixels = gdk_pixbuf_get_pixels (pixbuf);
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

	if (context)
		gdk_pixbuf_unref (pixbuf);

	return pixbuf;
}



/* Static loader */

GdkPixbuf *
image_load (FILE *f)
{
	return image_load_real (f, NULL);
}



/* Progressive loader */
/*
 * Tiff loading progressively cannot be done.  We write it to a file, then load
 * the file when it's done.  It's not pretty.
 */

gpointer
image_begin_load (ModulePreparedNotifyFunc func, gpointer user_data)
{
	TiffData *context;
	gint fd;

	context = g_new (TiffData, 1);
	context->func = func;
	context->user_data = user_data;
	context->all_okay = TRUE;
	context->tempname = g_strdup ("/tmp/gdkpixbuf-tif-tmp.XXXXXX");
	fd = mkstemp (context->tempname);
	if (fd < 0) {
		g_free (context);
		return NULL;
	}

	context->file = fdopen (fd, "w");
	if (context->file == NULL) {
		g_free (context);
		return NULL;
	}

	return context;
}

void
image_stop_load (gpointer data)
{
	TiffData *context = (TiffData*) data;

	g_return_if_fail (data != NULL);

	fflush (context->file);
	rewind (context->file);
	if (context->all_okay)
		image_load_real (context->file, context);

	fclose (context->file);
	unlink (context->tempname);
	g_free ((TiffData *) context);
}

gboolean
image_load_increment (gpointer data, guchar *buf, guint size)
{
	TiffData *context = (TiffData *) data;

	g_return_val_if_fail (data != NULL, FALSE);

	if (fwrite (buf, sizeof (guchar), size, context->file) != size) {
		context->all_okay = FALSE;
		return FALSE;
	}

	return TRUE;
}

