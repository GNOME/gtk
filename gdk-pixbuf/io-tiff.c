/* GdkPixbuf library - TIFF image loader
 *
 * Copyright (C) 1999 Mark Crichton
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Mark Crichton <crichton@gimp.org>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Jonathan Blandford <jrb@redhat.com>
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

/* Following code (almost) blatantly ripped from Imlib */

#include <config.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <tiffio.h>
#include <errno.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"

#ifdef G_OS_WIN32
#include <fcntl.h>
#define O_RDWR _O_RDWR
#endif


typedef struct _TiffData TiffData;
struct _TiffData
{
	ModulePreparedNotifyFunc prepare_func;
	ModuleUpdatedNotifyFunc update_func;
	gpointer user_data;

	gchar *tempname;
	FILE *file;
	gboolean all_okay;
};



static GdkPixbuf *
gdk_pixbuf__tiff_image_load_real (FILE *f, TiffData *context, GError **error)
{
	TIFF *tiff;
	guchar *pixels = NULL;
	guchar *tmppix;
	gint w, h, x, y, num_pixs, fd;
	uint32 *rast, *tmp_rast;
	GdkPixbuf *pixbuf;
	
	fd = fileno (f);
	tiff = TIFFFdOpen (fd, "libpixbuf-tiff", "r");

	if (!tiff) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
                             _("Failed to open TIFF image"));
		return NULL;
        }
                
	TIFFGetField (tiff, TIFFTAG_IMAGEWIDTH, &w);
	TIFFGetField (tiff, TIFFTAG_IMAGELENGTH, &h);
	num_pixs = w * h;
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, w, h);

        if (!pixbuf) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                             _("Insufficient memory to open TIFF file"));
		TIFFClose (tiff);
		return NULL;
        }
        
	if (context)
		(* context->prepare_func) (pixbuf, context->user_data);

	/* Yes, it needs to be _TIFFMalloc... */
	rast = (uint32 *) _TIFFmalloc (num_pixs * sizeof (uint32));

	if (!rast) {
                g_set_error (error,
                             GDK_PIXBUF_ERROR,
                             GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                             _("Insufficient memory to open TIFF file"));
		TIFFClose (tiff);
		return NULL;
	}

	if (TIFFReadRGBAImage (tiff, w, h, rast, 0)) {
		pixels = gdk_pixbuf_get_pixels (pixbuf);

                g_assert (pixels);
                
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

	if (context) {
		(* context->update_func) (pixbuf, 0, 0, w, h, context->user_data);
		gdk_pixbuf_unref (pixbuf);
	}

	return pixbuf;
}



/* Static loader */

static GdkPixbuf *
gdk_pixbuf__tiff_image_load (FILE *f, GError **error)
{
	return gdk_pixbuf__tiff_image_load_real (f, NULL, error);
}



/* Progressive loader */
/*
 * Tiff loading progressively cannot be done.  We write it to a file, then load
 * the file when it's done.  It's not pretty.
 */


static gpointer
gdk_pixbuf__tiff_image_begin_load (ModulePreparedNotifyFunc prepare_func,
				   ModuleUpdatedNotifyFunc update_func,
				   ModuleFrameDoneNotifyFunc frame_done_func,
				   ModuleAnimationDoneNotifyFunc anim_done_func,
				   gpointer user_data,
                                   GError **error)
{
	TiffData *context;
	gint fd;

	context = g_new (TiffData, 1);
	context->prepare_func = prepare_func;
	context->update_func = update_func;
	context->user_data = user_data;
	context->all_okay = TRUE;
	fd = g_file_open_tmp ("gdkpixbuf-tif-tmp.XXXXXX", &context->tempname,
			      NULL);
	if (fd < 0) {
		g_free (context);
		return NULL;
	}

	context->file = fdopen (fd, "w");
	if (context->file == NULL) {
                g_free (context->tempname);
		g_free (context);
		return NULL;
	}

	return context;
}

static gboolean
gdk_pixbuf__tiff_image_stop_load (gpointer data,
                                  GError **error)
{
	TiffData *context = (TiffData*) data;
        gboolean retval = FALSE;
        
	g_return_val_if_fail (data != NULL, TRUE);

	fflush (context->file);
	rewind (context->file);
	if (context->all_okay) {
                GdkPixbuf *pixbuf;
		pixbuf = gdk_pixbuf__tiff_image_load_real (context->file, context, error);
                if (pixbuf != NULL)
                        retval = TRUE;
        }

	fclose (context->file);
	unlink (context->tempname);
	g_free (context->tempname);
	g_free ((TiffData *) context);

        return TRUE;
}

static gboolean
gdk_pixbuf__tiff_image_load_increment (gpointer data, const guchar *buf,
                                       guint size, GError **error)
{
	TiffData *context = (TiffData *) data;

	g_return_val_if_fail (data != NULL, FALSE);

	if (fwrite (buf, sizeof (guchar), size, context->file) != size) {
		context->all_okay = FALSE;
                g_set_error (error,
                             G_FILE_ERROR,
                             g_file_error_from_errno (errno),
                             _("Failed to write to temporary file when loading TIFF image"));
		return FALSE;
	}

	return TRUE;
}

void
gdk_pixbuf__tiff_fill_vtable (GdkPixbufModule *module)
{
  module->load = gdk_pixbuf__tiff_image_load;
  module->begin_load = gdk_pixbuf__tiff_image_begin_load;
  module->stop_load = gdk_pixbuf__tiff_image_stop_load;
  module->load_increment = gdk_pixbuf__tiff_image_load_increment;
}

