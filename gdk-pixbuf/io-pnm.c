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
#include <ctype.h>
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
	guint                    maxval;
	guint                    rowstride;
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

static void explode_bitmap_into_buf (PnmLoaderContext *context);
static void explode_gray_into_buf (PnmLoaderContext *context);

/* Destroy notification function for the libart pixbuf */
static void
free_buffer (gpointer user_data, gpointer data)
{
	free (data);
}


/* explode bitmap data into rgb components         */
/* we need to know what the row so we can          */
/* do sub-byte expansion (since 1 byte = 8 pixels) */
/* context->dptr MUST point at first byte in incoming data  */
/* which corresponds to first pixel of row y       */
static void
explode_bitmap_into_buf (PnmLoaderContext *context)
{
	gint j;
	guchar *from, *to, data;
	gint bit;
	guchar *dptr;
	gint wid, x, y;

	g_return_if_fail (context != NULL);
	g_return_if_fail (context->dptr != NULL);

	/* I'm no clever bit-hacker so I'm sure this can be optimized */
	dptr = context->dptr;
	y    = context->output_row;
	wid  = context->width;

	from = dptr + (wid - 1)/8;
	to   = dptr + (wid - 1) * 3;
/*	bit  = 7 - (((y+1)*wid-1) % 8); */
	bit  = 7 - ((wid-1) % 8); 

	/* get first byte and align properly */
	data = from[0];
	for (j = 0; j < bit; j++, data >>= 1);

	for (x = wid-1; x >= 0; x--) {

/*		g_print ("%c",  (data & 1) ? '*' : ' '); */

		to[0] = to[1] = to[2] = (data & 1) ? 0x00 : 0xff;

		to -= 3;
		bit++;

		if (bit > 7) {
			from--;
			data = from[0];
			bit = 0;
		} else {
			data >>= 1;
		}
	}

/*	g_print ("\n"); */
}

/* explode gray image row into rgb components in pixbuf */
static void
explode_gray_into_buf (PnmLoaderContext *context)
{
	gint j;
	guchar *from, *to;
	guint w;

	g_return_if_fail (context != NULL);
	g_return_if_fail (context->dptr != NULL);

	/* Expand grey->colour.  Expand from the end of the
	 * memory down, so we can use the same buffer.
	 */
	w = context->width;
	from = context->dptr + w - 1;
	to = context->dptr + (w - 1) * 3;
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
	
	g_return_val_if_fail (inbuf != NULL, NULL);
	g_return_val_if_fail (inbuf->next_byte != NULL, NULL);
	
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

	g_return_val_if_fail (inbuf != NULL, NULL);
	g_return_val_if_fail (inbuf->next_byte != NULL, NULL);
	
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

	g_return_val_if_fail (inbuf != NULL, -1);
	g_return_val_if_fail (inbuf->next_byte != NULL, -1);
	g_return_val_if_fail (value != NULL, -1);

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
	
	context->width  = w;
	context->height = h;
	
	switch (type) {
	case PNM_FORMAT_PPM:
	case PNM_FORMAT_PPM_RAW:
	case PNM_FORMAT_PGM:
	case PNM_FORMAT_PGM_RAW:
		if ((rc = read_next_number (inbuf, &context->maxval)) < 0) {
			inbuf->next_byte = old_next_byte;
			inbuf->bytes_left = old_bytes_left;
			return PNM_SUSPEND;
		}
		break;
	default:
		break;
	}

	return PNM_OK;
}


static gint
pnm_read_raw_scanline (PnmLoaderContext *context)
{
	guint  numpix;
	guint  numbytes, offset;
	PnmIOBuffer *inbuf;

	g_return_val_if_fail (context != NULL, PNM_FATAL_ERR);

/*G_BREAKPOINT(); */

	inbuf = &context->inbuf;

	switch (context->type) {
	case PNM_FORMAT_PBM_RAW:
		numpix = inbuf->bytes_left * 8;
		break;
	case PNM_FORMAT_PGM_RAW:
		numpix = inbuf->bytes_left;
		break;
	case PNM_FORMAT_PPM_RAW:
		numpix = inbuf->bytes_left/3;
		break;
	default:
		g_warning ("io-pnm.c: Illegal raw pnm type!\n");
		return PNM_FATAL_ERR;
		break;
	}
	
	numpix = MIN (numpix, context->width - context->output_col);

	if (numpix == 0)
		return PNM_SUSPEND;
	
	context->dptr = context->pixels + 
		context->output_row * context->rowstride;
	
	switch (context->type) {
	case PNM_FORMAT_PBM_RAW:
		numbytes = numpix/8 + ((numpix % 8) ? 1 : 0);
		offset = context->output_col/8;
		break;
	case PNM_FORMAT_PGM_RAW:
		numbytes = numpix;
		offset = context->output_col;
		break;
	case PNM_FORMAT_PPM_RAW:
		numbytes = numpix*3;
		offset = context->output_col*3;
		break;
	default:
		g_warning ("io-pnm.c: Illegal raw pnm type!\n");
		break;
	}
	
	memcpy (context->dptr + offset,	inbuf->next_byte, numbytes);

	inbuf->next_byte  += numbytes;
	inbuf->bytes_left -= numbytes;

	context->output_col += numpix;
	if (context->output_col == context->width) {
		if ( context->type == PNM_FORMAT_PBM_RAW )
			explode_bitmap_into_buf(context);
		else if ( context->type == PNM_FORMAT_PGM_RAW )
			explode_gray_into_buf (context);
		
		context->output_col = 0;
		context->output_row++;
		
	} else {
		return PNM_SUSPEND;
	}

	return PNM_OK;
}


static gint
pnm_read_ascii_scanline (PnmLoaderContext *context)
{
	guint  offset;
	gint   rc;
	guint  value, numval, i;
	guchar data;
	guchar mask;
	guchar *old_next_byte, *dptr;
	guint  old_bytes_left;
	PnmIOBuffer *inbuf;

	g_return_val_if_fail (context != NULL, PNM_FATAL_ERR);

	inbuf = &context->inbuf;

	context->dptr = context->pixels + 
		context->output_row * context->rowstride;

	switch (context->type) {
	case PNM_FORMAT_PBM:
		numval = MIN (8, context->width - context->output_col);
		offset = context->output_col/8;
		break;
	case PNM_FORMAT_PGM:
		numval = 1;
		offset = context->output_col;
		break;
	case PNM_FORMAT_PPM:
		numval = 3;
		offset = context->output_col*3;
		break;
		
	default:
		g_warning ("Can't happen\n");
		return PNM_FATAL_ERR;
	}

	dptr = context->dptr + offset;

	while (TRUE) {
		if (context->type == PNM_FORMAT_PBM) {
			mask = 0x80;
			data = 0;
			numval = MIN (8, context->width - context->output_col);
		}

		old_next_byte  = inbuf->next_byte;
		old_bytes_left = inbuf->bytes_left;
		
		for (i=0; i<numval; i++) {
			if ((rc = read_next_number (inbuf, &value))) {
				inbuf->next_byte  = old_next_byte;
				inbuf->bytes_left = old_bytes_left;
				return PNM_SUSPEND;
			}
			switch (context->type) {
			case PNM_FORMAT_PBM:
				if (value)
					data |= mask;
				mask >>= 1;
				
				break;
			case PNM_FORMAT_PGM:
				*dptr++ = (guchar)(255.0*((double)value/(double)context->maxval));
				break;
			case PNM_FORMAT_PPM:
				*dptr++ = (guchar)(255.0*((double)value/(double)context->maxval));
				break;
			default:
				g_warning ("io-pnm.c: Illegal raw pnm type!\n");
				break;
			}
		}
		
		if (context->type == PNM_FORMAT_PBM) {
			*dptr++ = data;
			context->output_col += 8;
		} else {
			context->output_col++;
		}

		if (context->output_col == context->width) {
			if ( context->type == PNM_FORMAT_PBM )
				explode_bitmap_into_buf(context);
			else if ( context->type == PNM_FORMAT_PGM )
				explode_gray_into_buf (context);

			context->output_col = 0;
			context->output_row++;
			break;
		}

	}

	return PNM_OK;
}

/* returns 1 if a scanline was converted,  0 means we ran out of data */
static gint
pnm_read_scanline (PnmLoaderContext *context)
{
	gint   rc;

	g_return_val_if_fail (context != NULL, PNM_FATAL_ERR);

	/* read in image data */
	/* for raw formats this is trivial */
	switch (context->type) {
	case PNM_FORMAT_PBM_RAW:
	case PNM_FORMAT_PGM_RAW:
	case PNM_FORMAT_PPM_RAW:
		rc = pnm_read_raw_scanline (context);
		if (rc == PNM_SUSPEND)
			return rc;
		break;

	case PNM_FORMAT_PBM:
	case PNM_FORMAT_PGM:
	case PNM_FORMAT_PPM:
		rc = pnm_read_ascii_scanline (context);
		if (rc == PNM_SUSPEND)
			return rc;
		break;

	default:
		g_warning ("Cannot load these image types (yet)\n");
		return PNM_FATAL_ERR;
	}

	return PNM_OK;
}

/* Shared library entry point */
GdkPixbuf *
image_load (FILE *f)
{
	guint  nbytes;
	gint   rc;

	PnmLoaderContext context;
	PnmIOBuffer *inbuf;

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

		if (inbuf->next_byte != NULL && inbuf->bytes_left > 0)
			memmove (inbuf->buffer, inbuf->next_byte, 
				 inbuf->bytes_left);

		nbytes = fread (inbuf->buffer+inbuf->bytes_left, 
				1, num_to_read, f);
		inbuf->bytes_left += nbytes;
		inbuf->next_byte   = inbuf->buffer;

		/* ran out of data and we haven't exited main loop */
		if (inbuf->bytes_left == 0) {
			if (context.pixbuf)
				gdk_pixbuf_unref (context.pixbuf);
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

			if (skip_ahead_whitespace (inbuf) == NULL)
				continue;

			context.did_prescan = TRUE;
			context.output_row = 0;
			context.output_col = 0;

			context.rowstride = context.width * 3;
			context.pixels = g_malloc (context.height * 
						   context.width  * 3);
			if (!context.pixels) {
				/* Failed to allocate memory */
				g_error ("Couldn't allocate pixel buf");
			}
		}

		/* if we got here we're reading image data */
		while (context.output_row < context.height) {

			rc = pnm_read_scanline (&context);

			if (rc == PNM_SUSPEND) {
				break;
			} else if (rc == PNM_FATAL_ERR) {
				if (context.pixbuf)
					gdk_pixbuf_unref (context.pixbuf);
				g_warning ("io-pnm.c: error reading rows..\n");
				return NULL;
			}
		}

		if (context.output_row < context.height)
			continue;
		else
			break;
	}

	return gdk_pixbuf_new_from_data (context.pixels, ART_PIX_RGB, FALSE,
					 context.width, context.height, 
					 context.width * 3, free_buffer, NULL);

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
	PnmLoaderContext *context;

	context = g_new0 (PnmLoaderContext, 1);
	context->prepared_func = prepared_func;
	context->updated_func  = updated_func;
	context->user_data = user_data;
	context->pixbuf = context->pixels = NULL;
	context->got_header = FALSE;
	context->did_prescan = FALSE;

	context->inbuf.bytes_left = 0;
	context->inbuf.next_byte  = NULL;

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
	PnmLoaderContext *context = (PnmLoaderContext *) data;

	g_return_if_fail (context != NULL);

	if (context->pixbuf)
		gdk_pixbuf_unref (context->pixbuf);

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
	PnmLoaderContext *context = (PnmLoaderContext *)data;
	PnmIOBuffer      *inbuf;

	guchar *old_next_byte;
	guint  old_bytes_left;
	guchar *bufhd;
	guint  num_left, spinguard;
	gint   rc;

	g_return_val_if_fail (context != NULL, FALSE);
	g_return_val_if_fail (buf != NULL, FALSE);

	bufhd = buf;
	inbuf = &context->inbuf;
	old_bytes_left = inbuf->bytes_left;
	old_next_byte  = inbuf->next_byte;

	num_left = size;
	spinguard = 0;
	while (TRUE) {
		guint num_to_copy;

		/* keep buffer as full as possible */
		num_to_copy = MIN (PNM_BUF_SIZE - inbuf->bytes_left, num_left);
		
		if (num_to_copy == 0)
			spinguard++;

		if (spinguard > 1)
			return TRUE;

		if (inbuf->next_byte != NULL && inbuf->bytes_left > 0)
			memmove (inbuf->buffer, inbuf->next_byte, 
				 inbuf->bytes_left);

		memcpy (inbuf->buffer + inbuf->bytes_left, bufhd, num_to_copy);
		bufhd += num_to_copy;
		inbuf->bytes_left += num_to_copy;
		inbuf->next_byte   = inbuf->buffer;
		num_left -= num_to_copy;
		
		/* ran out of data and we haven't exited main loop */
		if (inbuf->bytes_left == 0)
			return TRUE;

		/* get header if needed */
		if (!context->got_header) {
			rc = pnm_read_header (context);
			if (rc == PNM_FATAL_ERR)
				return FALSE;
			else if (rc == PNM_SUSPEND)
				continue;

			context->got_header = TRUE;
		}

		/* scan until we hit image data */
		if (!context->did_prescan) {
			if (skip_ahead_whitespace (inbuf) == NULL)
				continue;

			context->did_prescan = TRUE;
			context->output_row = 0;
			context->output_col = 0;

			context->pixbuf = gdk_pixbuf_new(ART_PIX_RGB, 
							 /*have_alpha*/ FALSE,
							 8, 
							 context->width,
							 context->height);

			if (context->pixbuf == NULL) {
				/* Failed to allocate memory */
				g_error ("Couldn't allocate gdkpixbuf");
			}

			context->pixels = context->pixbuf->art_pixbuf->pixels;
			context->rowstride = context->pixbuf->art_pixbuf->rowstride;

			/* Notify the client that we are ready to go */
			(* context->prepared_func) (context->pixbuf,
						    context->user_data);

		}

		/* if we got here we're reading image data */
		while (context->output_row < context->height) {
			rc = pnm_read_scanline (context);

			if (rc == PNM_SUSPEND) {
				break;
			} else if (rc == PNM_FATAL_ERR) {
				if (context->pixbuf)
					gdk_pixbuf_unref (context->pixbuf);
				g_warning ("io-pnm.c: error reading rows..\n");
				return FALSE;
			} else if (rc == PNM_OK) {

				/* send updated signal */
				(* context->updated_func) (context->pixbuf,
						 context->user_data,
						 0, 
						 context->output_row-1,
						 context->width, 
						 1);
			}
		}

		if (context->output_row < context->height)
			continue;
		else
			break;
	}

	return TRUE;
}
