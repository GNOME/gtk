/* GdkPixbuf library - JPEG image loader
 *
 * Copyright (C) 1999 Michael Zucchi
 * Copyright (C) 1999 The Free Software Foundation
 * 
 * Progressive loading code Copyright (C) 1999 Red Hat, Inc.
 *
 * Authors: Michael Zucchi <zucchi@zedzone.mmc.com.au>
 *          Federico Mena-Quintero <federico@gimp.org>
 *          Michael Fulbright <drmike@redhat.com>
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


/*
  Progressive file loading notes (10/29/199) <drmike@redhat.com>...

  These are issues I know of and will be dealing with shortly:

    -  Currently does not handle progressive jpegs - this
       requires a change in the way image_load_increment () calls
       libjpeg. Progressive jpegs are rarer but I will add this
       support asap.

    - gray images are not properly loaded

    - error handling is not as good as it should be

 */


#include <config.h>
#include <stdio.h>
#include <setjmp.h>
#include <jpeglib.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"



/* we are a "source manager" as far as libjpeg is concerned */
typedef struct {
	struct jpeg_source_mgr pub;   /* public fields */

	JOCTET * buffer;              /* start of buffer */
	gboolean start_of_file;       /* have we gotten any data yet? */
	long  skip_next;              /* number of bytes to skip next read */
	
} my_source_mgr;

typedef my_source_mgr * my_src_ptr;

/* error handler data */
struct error_handler_data {
	struct jpeg_error_mgr pub;
	sigjmp_buf setjmp_buffer;
};

/* progressive loader context */
typedef struct {
	ModulePreparedNotifyFunc notify_func;
	gpointer                 notify_user_data;

	GdkPixbuf                *pixbuf;
	guchar                   *dptr;   /* current position in pixbuf */

	gboolean                 did_prescan;  /* are we in image data yet? */
	gboolean                 got_header;  /* have we loaded jpeg header? */
	gboolean                 src_initialized;/* TRUE when jpeg lib initialized */
	struct jpeg_decompress_struct cinfo;
	struct error_handler_data     jerr;
} JpegProgContext;

#define JPEG_PROG_BUF_SIZE 4096

GdkPixbuf *image_load (FILE *f);
gpointer image_begin_load (ModulePreparedNotifyFunc func, gpointer user_data);
void image_stop_load (gpointer context);
gboolean image_load_increment(gpointer context, guchar *buf, guint size);


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


/* explode gray image data from jpeg library into rgb components in pixbuf */
static void
explode_gray_into_buf (struct jpeg_decompress_struct *cinfo,
		       guchar **lines) 
{
	gint i, j;
	guint w;

	g_return_if_fail (cinfo != NULL);
	g_return_if_fail (cinfo->output_components == 1);

	/* Expand grey->colour.  Expand from the end of the
	 * memory down, so we can use the same buffer.
	 */
	w = cinfo->image_width;
	for (i = cinfo->rec_outbuf_height - 1; i >= 0; i--) {
		guchar *from, *to;
		
		from = lines[i] + w - 1;
		to = lines[i] + (w - 1) * 3;
		for (j = w - 1; j >= 0; j--) {
			to[0] = from[0];
			to[1] = from[0];
			to[2] = from[0];
			to -= 3;
			from--;
		}
	}
}

/* Shared library entry point */
GdkPixbuf *
image_load (FILE *f)
{
	int w, h, i;
	guchar *pixels = NULL;
	guchar *dptr;
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

		if (cinfo.output_components == 1)
			explode_gray_into_buf (&cinfo, lines);
	}

	jpeg_finish_decompress (&cinfo);
	jpeg_destroy_decompress (&cinfo);

	return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, FALSE,
					 w, h, w * 3,
					 free_buffer, NULL);

	return pixbuf;
}


/**** Progressive image loading handling *****/

/* these routines required because we are acting as a source manager for */
/* libjpeg. */
static void
init_source (j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;

	src->start_of_file = TRUE;
	src->skip_next = 0;
}


static void
term_source (j_decompress_ptr cinfo)
{
	/* XXXX - probably should scream something has happened */
}


/* for progressive loading (called "I/O Suspension" by libjpeg docs) */
/* we do nothing except return "FALSE"                               */
static boolean
fill_input_buffer (j_decompress_ptr cinfo)
{
	return FALSE;
}


static void
skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;
	long   num_can_do;

	/* move as far as we can into current buffer */
	/* then set skip_next to catch the rest      */
	if (num_bytes > 0) {
		num_can_do = MIN (src->pub.bytes_in_buffer, num_bytes);
		src->pub.next_input_byte += (size_t) num_can_do;
		src->pub.bytes_in_buffer -= (size_t) num_can_do;

		src->skip_next = num_bytes - num_can_do;
	}
}

 
/* 
 * func - called when we have pixmap created (but no image data)
 * user_data - passed as arg 1 to func
 * return context (opaque to user)
 */

gpointer
image_begin_load (ModulePreparedNotifyFunc func, gpointer user_data)
{
	JpegProgContext *context;
	my_source_mgr   *src;

	context = g_new (JpegProgContext, 1);
	context->notify_func = func;
	context->notify_user_data = user_data;
	context->pixbuf = NULL;
	context->got_header = FALSE;
	context->did_prescan = FALSE;
	context->src_initialized = FALSE;

	/* create libjpeg structures */
	jpeg_create_decompress (&context->cinfo);

	context->cinfo.src = (struct jpeg_source_mgr *) g_new (my_source_mgr, 1);
	src = (my_src_ptr) context->cinfo.src;
	src->buffer = g_malloc (JPEG_PROG_BUF_SIZE);

	context->cinfo.err = jpeg_std_error (&context->jerr.pub);

	src = (my_src_ptr) context->cinfo.src;
	src->pub.init_source = init_source;
	src->pub.fill_input_buffer = fill_input_buffer;
	src->pub.skip_input_data = skip_input_data;
	src->pub.resync_to_restart = jpeg_resync_to_restart;
	src->pub.term_source = term_source;
	src->pub.bytes_in_buffer = 0;
	src->pub.next_input_byte = NULL;

	return (gpointer) context;
}

/*
 * context - returned from image_begin_load
 *
 * free context, unref gdk_pixbuf
 */
void
image_stop_load (gpointer data)
{
	JpegProgContext *context = (JpegProgContext *) data;
	g_return_if_fail (context != NULL);

	if (context->pixbuf)
		gdk_pixbuf_unref (context->pixbuf);

	if (context->cinfo.src) {
		my_src_ptr src = (my_src_ptr) context->cinfo.src;
		
		if (src->buffer)
			g_free (src->buffer);
		g_free (src);
	}

	jpeg_finish_decompress(&context->cinfo);
	jpeg_destroy_decompress(&context->cinfo);

	g_free (context);
}




/*
 * context - from image_begin_load
 * buf - new image data
 * size - length of new image data
 *
 * append image data onto inrecrementally built output image
 */
gboolean
image_load_increment (gpointer data, guchar *buf, guint size)
{
	JpegProgContext *context = (JpegProgContext *)data;
	struct jpeg_decompress_struct *cinfo;
	my_src_ptr  src;
	guint       num_left, num_copy;
	guchar      *nextptr;

	g_return_val_if_fail (context != NULL, FALSE);
	g_return_val_if_fail (buf != NULL, FALSE);

	src = (my_src_ptr) context->cinfo.src;
	cinfo = &context->cinfo;

	/* skip over data if requested, handle unsigned int sizes cleanly */
	/* only can happen if we've already called jpeg_get_header once   */
	if (context->src_initialized && src->skip_next) {
		if (src->skip_next > size) {
			src->skip_next -= size;
			return TRUE;
		} else {
			num_left = size - src->skip_next;
			src->skip_next = 0;
		}
	} else {
		num_left = size;
	}

	while (num_left > 0) {
		/* copy as much data into buffer as possible */
		num_copy = MIN (JPEG_PROG_BUF_SIZE - src->pub.bytes_in_buffer,
				size);

		if (num_copy == 0) 
			g_assert ("Buffer overflow!\n");

		nextptr = src->buffer + src->pub.bytes_in_buffer;
		memcpy (nextptr, buf, num_copy);
		
		if (src->pub.next_input_byte == NULL ||
		    src->pub.bytes_in_buffer == 0)
			src->pub.next_input_byte = src->buffer;

		src->pub.bytes_in_buffer += num_copy;

		num_left -= num_copy;

		/* try to load jpeg header */
		if (!context->got_header) {
			int rc;

			rc = jpeg_read_header (cinfo, TRUE);
			context->src_initialized = TRUE;
			if (rc == JPEG_SUSPENDED)
				continue;

			context->got_header = TRUE;

			if (jpeg_has_multiple_scans (cinfo)) {
				g_print ("io-jpeg.c: Does not currently "
					 "support progressive jpeg files.\n");
				return FALSE;
			}

			context->pixbuf = gdk_pixbuf_new(ART_PIX_RGB, 
							 /*have_alpha*/ FALSE,
							 8, 
							 cinfo->image_width,
							 cinfo->image_height);

			if (context->pixbuf == NULL) {
				/* Failed to allocate memory */
				g_assert ("Couldn't allocate gdkpixbuf\n");
			}

			/* Use pixbuf buffer to store decompressed data */
			context->dptr = context->pixbuf->art_pixbuf->pixels;

			/* Notify the client that we are ready to go */

			if (context->notify_func)
				(* context->notify_func) (context->pixbuf,
							  context->notify_user_data);

			src->start_of_file = FALSE;
		} else if (!context->did_prescan) {
			int rc;

			/* start decompression */
			rc = jpeg_start_decompress (cinfo);
			cinfo->do_fancy_upsampling = FALSE;
			cinfo->do_block_smoothing = FALSE;

			if (rc == JPEG_SUSPENDED)
				continue;

			context->did_prescan = TRUE;
		} else {
			/* we're decompressing so feed jpeg lib scanlines */
			guchar *lines[4];
			guchar **lptr;
			guchar *rowptr, *p;
			gint   nlines, i;
			gint   start_scanline;

			/* keep going until we've done all scanlines */
			while (cinfo->output_scanline < cinfo->output_height) {
				start_scanline = cinfo->output_scanline;
				lptr = lines;
				rowptr = context->dptr;
				for (i=0; i < cinfo->rec_outbuf_height; i++) {
					*lptr++ = rowptr;
					rowptr += context->pixbuf->art_pixbuf->rowstride;;
				}

				for (p=lines[0],i=0; i< context->pixbuf->art_pixbuf->rowstride;i++, p++)
					*p = 0;
				
				nlines = jpeg_read_scanlines (cinfo, lines,
							      cinfo->rec_outbuf_height);
				if (nlines == 0)
					break;

				/* handle gray */
				if (cinfo->output_components == 1)
					explode_gray_into_buf (cinfo, lines);

				context->dptr += nlines * context->pixbuf->art_pixbuf->rowstride;
#ifdef DEBUG_JPEG_PROGRESSIVE
				
				if (start_scanline != cinfo->output_scanline)
					g_print("jpeg: Input pass=%2d, next input scanline=%3d,"
						" emitted %3d - %3d\n",
						cinfo->input_scan_number, cinfo->input_iMCU_row * 16,
						start_scanline, cinfo->output_scanline - 1);
				
				
				
				g_print ("Scanline %d of %d - ", 
					 cinfo->output_scanline,
					 cinfo->output_height);
/*			g_print ("rec_height %d -", cinfo->rec_outbuf_height); */
				g_print ("Processed %d lines - bytes left = %d\n",
					 nlines, cinfo->src->bytes_in_buffer);
#endif
			}
			/* did entire image */
			return TRUE;
		}
	}

	return TRUE;
}
