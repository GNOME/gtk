/* GdkPixbuf library - PNM image loader
 *
 * Copyright (C) 1999 Red Hat, Inc.
 *
 * Authors: Michael Fulbright <drmike@redhat.com>
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
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <jpeglib.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"




#define PNM_BUF_SIZE 4096

#define PNM_SUSPEND    0
#define PNM_OK         1
#define PNM_FATAL_ERR  -1

typedef enum {
	PNM_FORMAT_PGM,
	PNM_FORMAT_PGM_RAW,
	PNM_FORMAT_PPM,
	PNM_FORMAT_PPM_RAW,
	PNM_FORMAT_PBM,
	PNM_FORMAT_PBM_RAW
} PnmFormat;

typedef struct {
	guchar    buffer[PNM_BUF_SIZE];
	guchar    *next_byte;
	guint     bytes_left;
} PnmIOBuffer;

typedef struct {
	ModuleUpdatedNotifyFunc  updated_func;
	ModulePreparedNotifyFunc prepared_func;
	gpointer                 user_data;
	
	GdkPixbuf                *pixbuf;
	guchar                   *pixels; /* incoming pixel data buffer */
	guchar                   *dptr;   /* current position in pixbuf */

	PnmIOBuffer              inbuf;

	guint                    width;
	guint                    height;
	PnmFormat                type;

	guint                    output_row;  /* last row to be completed */
	guint                    output_col;
	gboolean                 did_prescan;  /* are we in image data yet? */
	gboolean                 got_header;  /* have we loaded jpeg header? */
} PnmLoaderContext;

GdkPixbuf *image_load (FILE *f);
gpointer image_begin_load (ModulePreparedNotifyFunc func, 
			   ModuleUpdatedNotifyFunc func2, gpointer user_data);
void image_stop_load (gpointer context);
gboolean image_load_increment(gpointer context, guchar *buf, guint size);


/* Destroy notification function for the libart pixbuf */
static void
free_buffer (gpointer user_data, gpointer data)
{
	free (data);
}


/* explode bitmap data into rgb components */
static void
explode_bitmap_into_buf (guchar *dptr, guint w)
{
	guchar *p;
	guchar data;
	gint   i;
	
	for (p = dptr; (p-dptr) < w; p++) {
		data = *p;

		for (i=0; i<8; i++, data >> 1)
			*(p+i) = (data & 1) ? 0xff : 0x00;
	}
}

/* explode gray image row into rgb components in pixbuf */
static void
explode_gray_into_buf (guchar *dptr, guint w)
{
	gint i, j;
	guchar *from, *to;

	g_return_if_fail (dptr != NULL);

	/* Expand grey->colour.  Expand from the end of the
	 * memory down, so we can use the same buffer.
	 */
	from = dptr + w - 1;
	to = dptr + (w - 1) * 3;
	for (j = w - 1; j >= 0; j--) {
		to[0] = from[0];
		to[1] = from[0];
		to[2] = from[0];
		to -= 3;
		from--;
	}
}

/* skip over whitespace in file from current pos.                      */
/* also skips comments                                                 */
/* returns pointer to first non-whitespace char hit or, or NULL if     */
/* we ran out of data w/o hitting a whitespace                         */
/* internal pointer in inbuf isnt moved ahead in this case             */
static guchar *
skip_ahead_whitespace (PnmIOBuffer *inbuf)
{
	gboolean in_comment;
	guchar *ptr;
	guint num_left;
	
	g_return_if_fail (inbuf != NULL);
	g_return_if_fail (inbuf->next_byte != NULL);
	
	in_comment = FALSE;
	num_left = inbuf->bytes_left;
	ptr      = inbuf->next_byte;
	while (num_left > 0) {
		if (in_comment) {
			if (*ptr == '\n')
				in_comment = FALSE;
		} else if (*ptr == '#') {
			in_comment = TRUE;
		} else if (!isspace (*ptr)) {
			inbuf->bytes_left -= (ptr-inbuf->next_byte);
			inbuf->next_byte   = ptr;
			return ptr;
		}
		ptr ++;
		num_left--;
	}
	return NULL;
}

/* reads into buffer until we hit whitespace in file from current pos,     */
/* return NULL if ran out of data                                          */
/* advances inbuf if successful                                            */
static guchar *
read_til_whitespace (PnmIOBuffer *inbuf, guchar *buf, guint size)
{
	guchar *p;
	guchar *ptr;
	guint num_left;

	g_return_if_fail (inbuf != NULL);
	g_return_if_fail (inbuf->next_byte != NULL);
	
	p = buf;
	num_left = inbuf->bytes_left;
	ptr      = inbuf->next_byte;
	while (num_left > 0 && (p-buf)+1 < size) {
		if (isspace (*ptr)) {
			*p = '\0';
			inbuf->bytes_left = num_left;
			inbuf->next_byte  = ptr;
			return ptr;
		} else {
			*p = *ptr;
			p++;
			ptr++;
			num_left--;
		}
	}
	return NULL;
}

/* read next number from buffer  */
/* -1 if failed, 0 if successful */
static gint
read_next_number (PnmIOBuffer *inbuf, guint *value)
{
	guchar *tmpptr;
	guchar *old_next_byte;
	gchar  *errptr;
	guint  old_bytes_left;
	guchar buf[128];

	g_return_if_fail (inbuf != NULL);
	g_return_if_fail (inbuf->next_byte != NULL);
	g_return_if_fail (value != NULL);

	old_next_byte  = inbuf->next_byte;
	old_bytes_left = inbuf->bytes_left;

	if ((tmpptr = skip_ahead_whitespace (inbuf)) == NULL)
		return -1;

	if ((tmpptr = read_til_whitespace (inbuf, buf, 128)) == NULL) {
		inbuf->next_byte  = old_next_byte;
		inbuf->bytes_left = old_bytes_left;
		return -1;
	}
	
	*value = strtol (buf, &errptr, 10);

	if (*errptr != '\0') {
		inbuf->next_byte  = old_next_byte;
		inbuf->bytes_left = old_bytes_left;
		return -1;
	}

	return 0;
}

/* returns PNM_OK, PNM_SUSPEND, or PNM_FATAL_ERR */
static gint
pnm_read_header (PnmLoaderContext *context)
{
	guchar *old_next_byte;
	guint  old_bytes_left;
	PnmIOBuffer *inbuf;
	guint  maxval;
	guint  w, h;
	gint rc;
	PnmFormat  type;

	g_return_val_if_fail (context != NULL, PNM_FATAL_ERR);

	inbuf = &context->inbuf;
	old_bytes_left = inbuf->bytes_left;
	old_next_byte  = inbuf->next_byte;
	
	/* file must start with a 'P' followed by a numeral */
	/* so loop till we get enough data to determine type*/
	if (inbuf->bytes_left < 2)
		return PNM_SUSPEND;
	
	if (*inbuf->next_byte != 'P')
		return PNM_FATAL_ERR;
	
	switch (*(inbuf->next_byte+1)) {
	case '1':
		type = PNM_FORMAT_PBM;
		break;
	case '2':
		type = PNM_FORMAT_PGM;
		break;
	case '3':
		type = PNM_FORMAT_PPM;
		break;
	case '4':
		type = PNM_FORMAT_PBM_RAW;
		break;
	case '5':
		type = PNM_FORMAT_PGM_RAW;
		break;
	case '6':
		type = PNM_FORMAT_PPM_RAW;
		break;
	default:
		return PNM_FATAL_ERR;
	}
	
	g_print ("File format is %d\n", type);

	context->type = type;

	inbuf->next_byte  += 2;
	inbuf->bytes_left -= 2;
	
	/* now read remainder of header */
	if ((rc = read_next_number (inbuf, &w))) {
		inbuf->next_byte = old_next_byte;
		inbuf->bytes_left = old_bytes_left;
		return PNM_SUSPEND;
	}
	
	if ((rc = read_next_number (inbuf, &h))) {
		inbuf->next_byte = old_next_byte;
		inbuf->bytes_left = old_bytes_left;
		return PNM_SUSPEND;
	}
	
	g_print ("Dimensions are %d x %d\n", w, h);

	context->width  = w;
	context->height = h;
	
	switch (type) {
	case PNM_FORMAT_PPM:
	case PNM_FORMAT_PPM_RAW:
	case PNM_FORMAT_PGM:
	case PNM_FORMAT_PGM_RAW:
		if ((rc = read_next_number (inbuf, &maxval)) < 0) {
			inbuf->next_byte = old_next_byte;
			inbuf->bytes_left = old_bytes_left;
			return PNM_SUSPEND;
		}
		
		g_print ("Maximum component value is %d\n", maxval);
		break;
	default:
		break;
	}

	return 0;
}

/* returns 1 if a scanline was converted,  0 means we ran out of data */
static gint
pnm_read_scanline (PnmLoaderContext *context)
{
	guint  numpix;
	guint  numbytes;
	guchar *dptr;
	PnmIOBuffer *inbuf;

	g_return_val_if_fail (context != NULL, PNM_FATAL_ERR);

	inbuf = &context->inbuf;

	/* read in image data */
	/* for raw formats this is trivial */
	switch (context->type) {
	case PNM_FORMAT_PBM_RAW:
	case PNM_FORMAT_PGM_RAW:
	case PNM_FORMAT_PPM_RAW:
		switch (context->type) {
		case PNM_FORMAT_PBM_RAW:
			numpix = inbuf->bytes_left * 8;
			break;
		case PNM_FORMAT_PGM_RAW:
			numpix = inbuf->bytes_left;
			break;
		case PNM_FORMAT_PPM_RAW:
			numpix = inbuf->bytes_left;
			break;
		}

		numpix = MIN (numpix, context->width - context->output_col);

		if (numpix == 0)
			return PNM_SUSPEND;

		switch (context->type) {
		case PNM_FORMAT_PBM_RAW:
			numbytes = numpix/8;
			break;
		case PNM_FORMAT_PGM_RAW:
			numbytes = numpix;
			break;
		case PNM_FORMAT_PPM_RAW:
			numbytes = numpix*3;
			break;
		}


		dptr = context->pixels + 
			context->output_row * context->width * 3 +
			context->output_col;

		memcpy (dptr, inbuf->next_byte, numpix);

		if ( context->type == PNM_FORMAT_PBM_RAW )
			explode_bitmap_info_buf (dptr, numpix);
		else if ( context->type == PNM_FORMAT_PGM_RAW )
			explode_gray_info_buf (dptr, numpix);
	default:
		g_print ("Cannot load these image types (yet)\n");
		return PNM_FATAL_ERR;
	}
}

/* Shared library entry point */
GdkPixbuf *
image_load (FILE *f)
{
	guint   w, h, i;
	guchar *pixels = NULL;
	guchar *dptr;
	guchar buf[PNM_BUF_SIZE];
	guint  nbytes;
	guint  chunksize;
	gint   rc;

	PnmLoaderContext context;
	PnmIOBuffer *inbuf;


#if 0
	G_BREAKPOINT();
#endif

	/* pretend to be doing progressive loading */
	context.updated_func = context.prepared_func = NULL;
	context.user_data = NULL;
	context.inbuf.bytes_left = 0;
	context.inbuf.next_byte  = NULL;
	context.pixels = context.pixbuf = NULL;
	context.got_header = context.did_prescan = FALSE;

	inbuf = &context.inbuf;

	while (TRUE) {
		guint  num_to_read;

		/* keep buffer as full as possible */
		num_to_read = PNM_BUF_SIZE - inbuf->bytes_left;

		if (inbuf->next_byte != NULL)
			memmove (inbuf->buffer, inbuf->next_byte, 
				 inbuf->bytes_left);

		nbytes = fread (inbuf->buffer, 1, num_to_read, f);
		inbuf->bytes_left += nbytes;
		inbuf->next_byte   = inbuf->buffer;
		
		/* ran out of data and we haven't exited main loop */
		/* something is wrong                              */
		if (inbuf->bytes_left == 0) {
			if (context.pixels)
				g_free (context.pixels);
			g_warning ("io-pnm.c: Ran out of data...\n");
			return NULL;
		}

		/* get header if needed */
		if (!context.got_header) {
		
			rc = pnm_read_header (&context);
			if (rc == PNM_FATAL_ERR)
				return NULL;
			else if (rc == PNM_SUSPEND)
				continue;

			context.got_header = TRUE;
		}

		/* scan until we hit image data */
		if (!context.did_prescan) {
			
			g_print ("doing prescan\n");
			if (skip_ahead_whitespace (inbuf) == NULL)
				continue;

			context.did_prescan = TRUE;
			context.output_row = 0;
			context.output_col = 0;

			pixels = g_malloc (context.height * 
					   context.width * 3);
			if (!pixels)
				return NULL;

			g_print ("prescan complete\n");
		}

		/* if we got here we're reading image data */
		while (context.output_row < context.height) {

			g_print ("reading row %d ...",context.output_row+1);

			rc = pnm_read_scanline (&context);

			if (rc == PNM_SUSPEND) {
				g_print ("suspended\n");
				break;
			} else if (rc == PNM_FATAL_ERR) {
				if (context.pixels)
					g_free (context.pixels);
				g_warning ("io-pnm.c: error reading rows..\n");
				return NULL;
			}
			g_print ("completed\n");
		}

		if (context.output_row < context.height)
			continue;
		else
			break;
	}

		
#if 0
	dptr = buf;
	
	switch (type) {
	case PNM_FORMAT_PPM:
	case PNM_FORMAT_PGM:
		if ((val = skip_ahead_whitespace (f)) < 0)
			return NULL;
		buf[0] = val;
		if (read_til_whitespace (f, buf+1, PNM_BUF_SIZE) < 0)
			return NULL;
		
		maxval = strtol (buf, &errptr, 10);
		
		g_print ("Maximum component value is %d\n", maxval);
		if (*errptr != '\0')
			return NULL;
		break;
	default:
		break;
	}
	
	fflush(stdout);
	
	/* read in image data */
	/* for raw formats this is trivial */
	switch (type) {
	case PNM_FORMAT_PBM_RAW:
		chunksize = w/8;
	case PNM_FORMAT_PGM_RAW:
		chunksize = w;
	case PNM_FORMAT_PPM_RAW:
		chunksize = 3*w;
		
		dptr = pixels;
		while (!feof (f)) {
			nbytes = fread (dptr, 1, chunksize, f);
			if (nbytes < chunksize && nbytes != 0) {
				g_free (pixels);
				return NULL;
			}
			
			if ( type == PNM_FORMAT_PBM_RAW )
				explode_bitmap_info_buf (dptr, w);
			else if ( type == PNM_FORMAT_PGM_RAW )
				explode_gray_info_buf (dptr);
			
			dptr += 3*w;
		}

		/* did we get the entire image? */
		if ((dptr - pixels) != 3*w*(h+1)) {
			g_free (pixels);
			return NULL;
		}
			
		break;
	default:
		g_print ("Couldnt load image data (yet)\n");
		fflush(stdout);
		return NULL;
	}

	return gdk_pixbuf_new_from_data (pixels, ART_PIX_RGB, FALSE,
					 w, h, w * 3,
					 free_buffer, NULL);
#endif

}

#if 0
/**** Progressive image loading handling *****/

/* these routines required because we are acting as a source manager for */
/* libjpeg. */
static void
init_source (j_decompress_ptr cinfo)
{
	my_src_ptr src = (my_src_ptr) cinfo->src;

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
image_begin_load (ModulePreparedNotifyFunc prepared_func, 
		  ModuleUpdatedNotifyFunc  updated_func,
		  gpointer user_data)
{
	JpegProgContext *context;
	my_source_mgr   *src;

	context = g_new0 (JpegProgContext, 1);
	context->prepared_func = prepared_func;
	context->updated_func  = updated_func;
	context->user_data = user_data;
	context->pixbuf = NULL;
	context->got_header = FALSE;
	context->did_prescan = FALSE;
	context->src_initialized = FALSE;

	/* create libjpeg structures */
	jpeg_create_decompress (&context->cinfo);

	context->cinfo.src = (struct jpeg_source_mgr *) g_new0 (my_source_mgr, 1);
	src = (my_src_ptr) context->cinfo.src;

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

	jpeg_finish_decompress(&context->cinfo);
	jpeg_destroy_decompress(&context->cinfo);

	if (context->cinfo.src) {
		my_src_ptr src = (my_src_ptr) context->cinfo.src;
		
		g_free (src);
	}

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
	guint       last_bytes_left;
	guint       spinguard;
	guchar *bufhd;

	g_return_val_if_fail (context != NULL, FALSE);
	g_return_val_if_fail (buf != NULL, FALSE);

	src = (my_src_ptr) context->cinfo.src;

	cinfo = &context->cinfo;

	/* XXXXXXX (drmike) - loop(s) below need to be recoded now I
         *                    have a grasp of what the flow needs to be!
         */


	/* skip over data if requested, handle unsigned int sizes cleanly */
	/* only can happen if we've already called jpeg_get_header once   */
	if (context->src_initialized && src->skip_next) {
		if (src->skip_next > size) {
			src->skip_next -= size;
			return TRUE;
		} else {
			num_left = size - src->skip_next;
			bufhd = buf + src->skip_next;
			src->skip_next = 0;
		}
	} else {
		num_left = size;
		bufhd = buf;
	}


	last_bytes_left = 0;
	spinguard = 0;
	while (src->pub.bytes_in_buffer != 0 || num_left != 0) {

		/* handle any data from caller we haven't processed yet */
		if (num_left > 0) {
			if(src->pub.bytes_in_buffer && 
			   src->pub.next_input_byte != src->buffer)
				memmove(src->buffer, src->pub.next_input_byte,
					src->pub.bytes_in_buffer);


			num_copy = MIN (JPEG_PROG_BUF_SIZE - src->pub.bytes_in_buffer,
					num_left);

/*			if (num_copy == 0) 
				g_error ("Buffer overflow!");
*/
			memcpy(src->buffer + src->pub.bytes_in_buffer, bufhd,num_copy);
			src->pub.next_input_byte = src->buffer;
			src->pub.bytes_in_buffer += num_copy;
			bufhd += num_copy;
			num_left -= num_copy;
		} else {
		/* did anything change from last pass, if not return */
			if (last_bytes_left == 0)
				last_bytes_left = src->pub.bytes_in_buffer;
			else if (src->pub.bytes_in_buffer == last_bytes_left)
				spinguard++;
			else
				last_bytes_left = src->pub.bytes_in_buffer;
		}

		/* should not go through twice and not pull bytes out of buf */
		if (spinguard > 2)
			return TRUE;

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
				g_error ("Couldn't allocate gdkpixbuf");
			}

			/* Use pixbuf buffer to store decompressed data */
			context->dptr = context->pixbuf->art_pixbuf->pixels;

			/* Notify the client that we are ready to go */
			(* context->prepared_func) (context->pixbuf,
						    context->user_data);

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

#ifdef IO_JPEG_DEBUG_GREY
				for (p=lines[0],i=0; i< context->pixbuf->art_pixbuf->rowstride;i++, p++)
					*p = 0;
				
#endif
				nlines = jpeg_read_scanlines (cinfo, lines,
							      cinfo->rec_outbuf_height);
				if (nlines == 0)
					break;

				/* handle gray */
				if (cinfo->output_components == 1)
					explode_gray_into_buf (cinfo, lines);

				context->dptr += nlines * context->pixbuf->art_pixbuf->rowstride;

				/* send updated signal */
				(* context->updated_func) (context->pixbuf,
						 context->user_data,
						 0, 
						 cinfo->output_scanline-1,
						 cinfo->image_width, 
						 nlines);

#undef DEBUG_JPEG_PROGRESSIVE
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
			if (cinfo->output_scanline >= cinfo->output_height)
				return TRUE;
			else
				continue;
		}
	}

	return TRUE;
}
#endif
