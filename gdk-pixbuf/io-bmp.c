/* GdkPixbuf library - Windows Bitmap image loader
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Arjan van de Ven <arjan@fenrus.demon.nl>
 *          Federico Mena-Quintero <federico@gimp.org>
 *
 * Based on io-ras.c
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

#include <config.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <string.h>
#include "gdk-pixbuf-private.h"
#include "gdk-pixbuf-io.h"



#if 0
/* If these structures were unpacked, they would define the two headers of the
 * BMP file.  After them comes the palette, and then the image data.
 *
 * We do not use these structures; we just keep them here for reference.
 */
struct BitmapFileHeader {
	guint16 magic;
	guint32 file_size;
	guint32 reserved;
	guint32 data_offset;
};

struct BitmapInfoHeader {
	guint32 header_size;
	guint32 width;
	guint32 height;
	guint16 planes;
	guint16 bpp;
	guint32 compression;
	guint32 data_size;
	guint32 x_ppm;
	guint32 y_ppm;
	guint32 n_colors;
	guint32 n_important_colors;
};
#endif

/* Compression values */

#define BI_RGB 0
#define BI_RLE8 1
#define BI_RLE4 2
#define BI_BITFIELDS 3

/* State machine */
typedef enum {
	READ_STATE_HEADERS,	/* Reading the bitmap file header and bitmap info header */
	READ_STATE_PALETTE,	/* Reading the palette */
	READ_STATE_BITMASKS,	/* Reading the bitmasks for BI_BITFIELDS */
	READ_STATE_DATA,	/* Reading the actual image data */
	READ_STATE_ERROR,	/* An error occurred; further data will be ignored */
	READ_STATE_DONE		/* Done reading the image; further data will be ignored */
} ReadState;

/*

DumpBIH printf's the values in a BitmapInfoHeader to the screen, for
debugging purposes.

*/
#if DUMPBIH
static void DumpBIH(unsigned char *BIH)
{
	printf("biSize      = %i \n",
	       (int) (BIH[3] << 24) + (BIH[2] << 16) + (BIH[1] << 8) +
	       (BIH[0]));
	printf("biWidth     = %i \n",
	       (int) (BIH[7] << 24) + (BIH[6] << 16) + (BIH[5] << 8) +
	       (BIH[4]));
	printf("biHeight    = %i \n",
	       (int) (BIH[11] << 24) + (BIH[10] << 16) + (BIH[9] << 8) +
	       (BIH[8]));
	printf("biPlanes    = %i \n", (int) (BIH[13] << 8) + (BIH[12]));
	printf("biBitCount  = %i \n", (int) (BIH[15] << 8) + (BIH[14]));
	printf("biCompress  = %i \n",
	       (int) (BIH[19] << 24) + (BIH[18] << 16) + (BIH[17] << 8) +
	       (BIH[16]));
	printf("biSizeImage = %i \n",
	       (int) (BIH[23] << 24) + (BIH[22] << 16) + (BIH[21] << 8) +
	       (BIH[20]));
	printf("biXPels     = %i \n",
	       (int) (BIH[27] << 24) + (BIH[26] << 16) + (BIH[25] << 8) +
	       (BIH[24]));
	printf("biYPels     = %i \n",
	       (int) (BIH[31] << 24) + (BIH[30] << 16) + (BIH[29] << 8) +
	       (BIH[28]));
	printf("biClrUsed   = %i \n",
	       (int) (BIH[35] << 24) + (BIH[34] << 16) + (BIH[33] << 8) +
	       (BIH[32]));
	printf("biClrImprtnt= %i \n",
	       (int) (BIH[39] << 24) + (BIH[38] << 16) + (BIH[37] << 8) +
	       (BIH[36]));
}
#endif
/* struct headerpair contains the decoded width/height/depth info for
   the current bitmap */

struct headerpair {
	guint32 size;
	gint32 width;
	gint32 height;
	guint depth;
	guint Negative;		/* Negative = 1 -> top down BMP,
				   Negative = 0 -> bottom up BMP */
};

/* Data needed for the "state" during decompression */
struct bmp_compression_state {
	gint phase;
	gint RunCount;

	guchar *linebuff;
	gint linebuffsize;	/* these two counts in nibbles */
	gint linebuffdone;
};

/* Progressive loading */

struct bmp_progressive_state {
	ModulePreparedNotifyFunc prepared_func;
	ModuleUpdatedNotifyFunc updated_func;
	gpointer user_data;

	ReadState read_state;

	guint LineWidth;
	guint Lines;		/* # of finished lines */

	guchar *buff;
	gint BufferSize;
	gint BufferDone;

	guchar (*Colormap)[3];

	gint Type;		/*
				   32 = RGB + alpha
				   24 = RGB
				   16 = RGB
				   4  = 4 bpp colormapped
				   8  = 8 bpp colormapped
				   1  = 1 bit bitonal
				 */
	gint Compressed;
	struct bmp_compression_state compr;


	struct headerpair Header;	/* Decoded (BE->CPU) header */

	/* Bit masks, shift amounts, and significant bits for BI_BITFIELDS coding */
	int r_mask, r_shift, r_bits;
	int g_mask, g_shift, g_bits;
	int b_mask, b_shift, b_bits;

	GdkPixbuf *pixbuf;	/* Our "target" */
};

static gpointer
gdk_pixbuf__bmp_image_begin_load(ModulePreparedNotifyFunc prepared_func,
				 ModuleUpdatedNotifyFunc updated_func,
                                 gpointer user_data,
                                 GError **error);

static gboolean gdk_pixbuf__bmp_image_stop_load(gpointer data, GError **error);
static gboolean gdk_pixbuf__bmp_image_load_increment(gpointer data,
                                                     const guchar * buf,
                                                     guint size,
                                                     GError **error);



/* Shared library entry point --> This should be removed when
   generic_image_load enters gdk-pixbuf-io. */
static GdkPixbuf *gdk_pixbuf__bmp_image_load(FILE * f, GError **error)
{
	guchar membuf[4096];
	size_t length;
	struct bmp_progressive_state *State;

	GdkPixbuf *pb;

	State =
	    gdk_pixbuf__bmp_image_begin_load(NULL, NULL, NULL,
                                             error);

        if (State == NULL)
          return NULL;

	while (feof(f) == 0) {
		length = fread(membuf, 1, sizeof (membuf), f);
		if (length > 0)
                  if (!gdk_pixbuf__bmp_image_load_increment(State,
                                                            membuf,
                                                            length,
                                                            error)) {
                          gdk_pixbuf__bmp_image_stop_load (State, NULL);
                          return NULL;
                  }

	}
	if (State->pixbuf != NULL)
		g_object_ref(State->pixbuf);

	pb = State->pixbuf;

	gdk_pixbuf__bmp_image_stop_load(State, NULL);
	return pb;
}

static gboolean DecodeHeader(unsigned char *BFH, unsigned char *BIH,
                             struct bmp_progressive_state *State,
                             GError **error)
{
        /* FIXME this is totally unrobust against bogus image data. */

	if (State->BufferSize < GUINT32_FROM_LE (* (guint32 *) &BIH[0]) + 14) {
		State->BufferSize = GUINT32_FROM_LE (* (guint32 *) &BIH[0]) + 14;
		State->buff = g_realloc (State->buff, State->BufferSize);
		return TRUE;
	}

#if DUMPBIH
	DumpBIH(BIH);
#endif

	State->Header.size = GUINT32_FROM_LE (* (guint32 *) &BIH[0]);
	if (State->Header.size == 40) {
		State->Header.width = GINT32_FROM_LE (* (gint32 *) &BIH[4]);
		State->Header.height = GINT32_FROM_LE (* (gint32 *) &BIH[8]);
		State->Header.depth = GUINT16_FROM_LE (* (guint16 *) &BIH[14]);
		State->Compressed = GUINT32_FROM_LE (* (guint32 *) &BIH[16]);
	} else if (State->Header.size == 12) {
		State->Header.width = GUINT16_FROM_LE (* (guint16 *) &BIH[4]);
		State->Header.height = GUINT16_FROM_LE (* (guint16 *) &BIH[6]);
		State->Header.depth = GUINT16_FROM_LE (* (guint16 *) &BIH[10]);
		State->Compressed = BI_RGB;
	} else {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			     _("BMP image has unsupported header size"));
		State->read_state = READ_STATE_ERROR;
		return FALSE;
	}

	State->Type = State->Header.depth;	/* This may be less trivial someday */

	/* Negative heights indicates bottom-down pixelorder */
	if (State->Header.height < 0) {
		State->Header.height = -State->Header.height;
		State->Header.Negative = 1;
	}
	if (State->Header.width < 0) {
		State->Header.width = -State->Header.width;
		State->Header.Negative = 0;
	}

	if (State->Header.width == 0 || State->Header.height == 0 ||
	    (State->Compressed == BI_RLE4 && State->Type != 4)    ||
	    (State->Compressed == BI_RLE8 && State->Type != 8)	  ||
	    (State->Compressed == BI_BITFIELDS && !(State->Type == 16 || State->Type == 32)) ||
	    State->Compressed > BI_BITFIELDS) {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			     _("BMP image has bogus header data"));
		State->read_state = READ_STATE_ERROR;
		return FALSE;
	}

	if (State->Type == 32)
		State->LineWidth = State->Header.width * 4;
	else if (State->Type == 24)
		State->LineWidth = State->Header.width * 3;
	else if (State->Type == 16)
		State->LineWidth = State->Header.width * 2;
	else if (State->Type == 8)
		State->LineWidth = State->Header.width * 1;
	else if (State->Type == 4)
		State->LineWidth = (State->Header.width + 1) / 2;
	else if (State->Type == 1) {
		State->LineWidth = State->Header.width / 8;
		if ((State->Header.width & 7) != 0)
			State->LineWidth++;
	} else {
		g_set_error (error,
			     GDK_PIXBUF_ERROR,
			     GDK_PIXBUF_ERROR_CORRUPT_IMAGE,
			     _("BMP image has bogus header data"));
		State->read_state = READ_STATE_ERROR;
		return FALSE;
	}

	/* Pad to a 32 bit boundary */
	if (((State->LineWidth % 4) > 0)
	    && (State->Compressed == BI_RGB || State->Compressed == BI_BITFIELDS))
		State->LineWidth = (State->LineWidth / 4) * 4 + 4;

	if (State->pixbuf == NULL) {
		if (State->Type == 32)
			State->pixbuf =
			    gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8,
					   (gint) State->Header.width,
					   (gint) State->Header.height);
		else
			State->pixbuf =
			    gdk_pixbuf_new(GDK_COLORSPACE_RGB, FALSE, 8,
					   (gint) State->Header.width,
					   (gint) State->Header.height);

                if (State->pixbuf == NULL) {
                        g_set_error (error,
                                     GDK_PIXBUF_ERROR,
                                     GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY,
                                     _("Not enough memory to load bitmap image"));
			State->read_state = READ_STATE_ERROR;
                        return FALSE;
                }

		if (State->prepared_func != NULL)
			/* Notify the client that we are ready to go */
			(*State->prepared_func) (State->pixbuf, NULL, State->user_data);

	}

	if (!(State->Compressed == BI_RGB || State->Compressed == BI_BITFIELDS)) {
		State->compr.linebuffdone = 0;
		State->compr.linebuffsize = State->Header.width;
		if (State->Type == 8)
			State->compr.linebuffsize *= 2;
		State->compr.linebuff = g_malloc ((State->compr.linebuffsize + 1) / 2);
	}

	State->BufferDone = 0;
	if (State->Type <= 8) {
		State->read_state = READ_STATE_PALETTE;
		State->BufferSize = GUINT32_FROM_LE (* (guint32 *) &BFH[10]) - 14 - State->Header.size;
	} else if (State->Compressed == BI_RGB) {
		State->read_state = READ_STATE_DATA;
		State->BufferSize = State->LineWidth;
	} else if (State->Compressed == BI_BITFIELDS) {
		State->read_state = READ_STATE_BITMASKS;
		State->BufferSize = 12;
	} else
		g_assert_not_reached ();

	State->buff = g_realloc (State->buff, State->BufferSize);

        return TRUE;
}

static void DecodeColormap (guchar *buff,
			    struct bmp_progressive_state *State,
			    GError **error)
{
	gint i;

	g_assert (State->read_state == READ_STATE_PALETTE);

	State->Colormap = g_malloc ((1 << State->Header.depth) * sizeof (*State->Colormap));

	for (i = 0; i < (1 << State->Header.depth); i++)
	{
		State->Colormap[i][0] = buff[i * (State->Header.size == 12 ? 3 : 4)];
		State->Colormap[i][1] = buff[i * (State->Header.size == 12 ? 3 : 4) + 1];
		State->Colormap[i][2] = buff[i * (State->Header.size == 12 ? 3 : 4) + 2];
	}

	State->read_state = READ_STATE_DATA;

	State->BufferDone = 0;
	if (!(State->Compressed == BI_RGB || State->Compressed == BI_BITFIELDS))
		State->BufferSize = 2;
	else
		State->BufferSize = State->LineWidth;

	State->buff = g_realloc (State->buff, State->BufferSize);
}

/* Finds the lowest set bit and the number of set bits */
static void
find_bits (int n, int *lowest, int *n_set)
{
	int i;

	*n_set = 0;

	for (i = 31; i >= 0; i--)
		if (n & (1 << i)) {
			*lowest = i;
			(*n_set)++;
		}
}

/* Decodes the 3 shorts that follow for the bitmasks for BI_BITFIELDS coding */
static void
decode_bitmasks (struct bmp_progressive_state *State, guchar *buf)
{
	State->r_mask = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
	buf += 4;

	State->g_mask = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
	buf += 4;

	State->b_mask = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);

	find_bits (State->r_mask, &State->r_shift, &State->r_bits);
	find_bits (State->g_mask, &State->g_shift, &State->g_bits);
	find_bits (State->b_mask, &State->b_shift, &State->b_bits);

	if (State->r_bits == 0 || State->g_bits == 0 || State->b_bits == 0) {
		State->r_mask = 0x7c00;
		State->r_shift = 10;
		State->g_mask = 0x03e0;
		State->g_shift = 5;
		State->b_mask = 0x001f;
		State->b_shift = 0;

		State->r_bits = State->g_bits = State->b_bits = 5;
	}

	State->read_state = READ_STATE_DATA;
	State->BufferDone = 0;
	State->BufferSize = State->LineWidth;
	State->buff = g_realloc (State->buff, State->BufferSize);
}

/*
 * func - called when we have pixmap created (but no image data)
 * user_data - passed as arg 1 to func
 * return context (opaque to user)
 */

static gpointer
gdk_pixbuf__bmp_image_begin_load(ModulePreparedNotifyFunc prepared_func,
				 ModuleUpdatedNotifyFunc updated_func,
                                 gpointer user_data,
                                 GError **error)
{
	struct bmp_progressive_state *context;

	context = g_new0(struct bmp_progressive_state, 1);
	context->prepared_func = prepared_func;
	context->updated_func = updated_func;
	context->user_data = user_data;

	context->read_state = READ_STATE_HEADERS;

	context->BufferSize = 26;
	context->buff = g_malloc(26);
	context->BufferDone = 0;
	/* 14 for the BitmapFileHeader, 12 for the BitmapImageHeader */

	context->Colormap = NULL;

	context->Lines = 0;

	context->Type = 0;

	memset(&context->Header, 0, sizeof(struct headerpair));
	memset(&context->compr, 0, sizeof(struct bmp_compression_state));


	context->pixbuf = NULL;


	return (gpointer) context;
}

/*
 * context - returned from image_begin_load
 *
 * free context, unref gdk_pixbuf
 */
static gboolean gdk_pixbuf__bmp_image_stop_load(gpointer data, GError **error)
{
	struct bmp_progressive_state *context =
	    (struct bmp_progressive_state *) data;

        /* FIXME this thing needs to report errors if
         * we have unused image data
         */

	g_return_val_if_fail(context != NULL, TRUE);

	if (context->compr.linebuff != NULL)
		g_free(context->compr.linebuff);

	if (context->Colormap != NULL)
		g_free(context->Colormap);

	if (context->pixbuf)
		g_object_unref(context->pixbuf);

	g_free(context->buff);
	g_free(context);

        return TRUE;
}


/*
The OneLineXX functions are called when 1 line worth of data is present.
OneLine24 is the 24 bpp-version.
*/
static void OneLine32(struct bmp_progressive_state *context)
{
	int i;
	guchar *pixels;
	guchar *src;

	if (!context->Header.Negative)
		pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride * (context->Header.height - context->Lines - 1));
	else
		pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride * context->Lines);

	src = context->buff;

	if (context->Compressed == BI_BITFIELDS) {
		int r_lshift, r_rshift;
		int g_lshift, g_rshift;
		int b_lshift, b_rshift;

		r_lshift = 8 - context->r_bits;
		g_lshift = 8 - context->g_bits;
		b_lshift = 8 - context->b_bits;

		r_rshift = context->r_bits - r_lshift;
		g_rshift = context->g_bits - g_lshift;
		b_rshift = context->b_bits - b_lshift;

		for (i = 0; i < context->Header.width; i++) {
			int v, r, g, b;

			v = src[0] | (src[1] << 8) | (src[2] << 16);

			r = (v & context->r_mask) >> context->r_shift;
			g = (v & context->g_mask) >> context->g_shift;
			b = (v & context->b_mask) >> context->b_shift;

			*pixels++ = (r << r_lshift) | (r >> r_rshift);
			*pixels++ = (g << g_lshift) | (g >> g_rshift);
			*pixels++ = (b << b_lshift) | (b >> b_rshift);
			*pixels++ = src[3]; /* alpha */

			src += 4;
		}
	} else
		for (i = 0; i < context->Header.width; i++) {
			*pixels++ = src[2];
			*pixels++ = src[1];
			*pixels++ = src[0];
			*pixels++ = src[3];

			src += 4;
		}
}

static void OneLine24(struct bmp_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  (context->Header.height - context->Lines - 1));
	else
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  context->Lines);
	while (X < context->Header.width) {
		Pixels[X * 3 + 0] = context->buff[X * 3 + 2];
		Pixels[X * 3 + 1] = context->buff[X * 3 + 1];
		Pixels[X * 3 + 2] = context->buff[X * 3 + 0];
		X++;
	}

}

static void OneLine16(struct bmp_progressive_state *context)
{
	int i;
	guchar *pixels;
	guchar *src;

	if (!context->Header.Negative)
		pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride * (context->Header.height - context->Lines - 1));
	else
		pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride * context->Lines);

	src = context->buff;

	if (context->Compressed == BI_BITFIELDS) {
		int r_lshift, r_rshift;
		int g_lshift, g_rshift;
		int b_lshift, b_rshift;

		r_lshift = 8 - context->r_bits;
		g_lshift = 8 - context->g_bits;
		b_lshift = 8 - context->b_bits;

		r_rshift = context->r_bits - r_lshift;
		g_rshift = context->g_bits - g_lshift;
		b_rshift = context->b_bits - b_lshift;

		for (i = 0; i < context->Header.width; i++) {
			int v, r, g, b;

			v = (int) src[0] | ((int) src[1] << 8);

			r = (v & context->r_mask) >> context->r_shift;
			g = (v & context->g_mask) >> context->g_shift;
			b = (v & context->b_mask) >> context->b_shift;

			*pixels++ = (r << r_lshift) | (r >> r_rshift);
			*pixels++ = (g << g_lshift) | (g >> g_rshift);
			*pixels++ = (b << b_lshift) | (b >> b_rshift);

			src += 2;
		}
	} else
		for (i = 0; i < context->Header.width; i++) {
			int v, r, g, b;

			v = src[0] | (src[1] << 8);

			r = (v >> 10) & 0x1f;
			g = (v >> 5) & 0x1f;
			b = v & 0x1f;

			*pixels++ = (r << 3) | (r >> 2);
			*pixels++ = (g << 3) | (g >> 2);
			*pixels++ = (b << 3) | (b >> 2);
		}
}

static void OneLine8(struct bmp_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  (context->Header.height - context->Lines - 1));
	else
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  context->Lines);
	while (X < context->Header.width) {
		Pixels[X * 3 + 0] =
		    context->Colormap[context->buff[X]][2];
		Pixels[X * 3 + 1] =
		    context->Colormap[context->buff[X]][1];
		Pixels[X * 3 + 2] =
		    context->Colormap[context->buff[X]][0];
		X++;
	}
}

static void OneLine4(struct bmp_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  (context->Header.height - context->Lines - 1));
	else
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  context->Lines);

	while (X < context->Header.width) {
		guchar Pix;

		Pix = context->buff[X / 2];

		Pixels[X * 3 + 0] =
		    context->Colormap[Pix >> 4][2];
		Pixels[X * 3 + 1] =
		    context->Colormap[Pix >> 4][1];
		Pixels[X * 3 + 2] =
		    context->Colormap[Pix >> 4][0];
		X++;
		if (X < context->Header.width) {
			/* Handle the other 4 bit pixel only when there is one */
			Pixels[X * 3 + 0] =
			    context->Colormap[Pix & 15][2];
			Pixels[X * 3 + 1] =
			    context->Colormap[Pix & 15][1];
			Pixels[X * 3 + 2] =
			    context->Colormap[Pix & 15][0];
			X++;
		}
	}

}

static void OneLine1(struct bmp_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  (context->Header.height - context->Lines - 1));
	else
		Pixels = (context->pixbuf->pixels +
			  context->pixbuf->rowstride *
			  context->Lines);
	while (X < context->Header.width) {
		gint Bit;

		Bit = (context->buff[X / 8]) >> (7 - (X & 7));
		Bit = Bit & 1;
		Pixels[X * 3 + 0] = context->Colormap[Bit][2];
		Pixels[X * 3 + 1] = context->Colormap[Bit][1];
		Pixels[X * 3 + 2] = context->Colormap[Bit][0];
		X++;
	}
}


static void OneLine(struct bmp_progressive_state *context)
{
	context->BufferDone = 0;
	if (context->Lines >= context->Header.height)
		return;

	if (context->Type == 32)
		OneLine32(context);
	else if (context->Type == 24)
		OneLine24(context);
	else if (context->Type == 16)
		OneLine16(context);
	else if (context->Type == 8)
		OneLine8(context);
	else if (context->Type == 4)
		OneLine4(context);
	else if (context->Type == 1)
		OneLine1(context);
	else
		g_assert_not_reached ();

	context->Lines++;

	if (context->updated_func != NULL) {
		(*context->updated_func) (context->pixbuf,
					  0,
					  context->Lines,
					  context->Header.width,
					  1,
					  context->user_data);

	}
}

static void
DoCompressed(struct bmp_progressive_state *context)
{
	gint count, pos;
	switch (context->compr.phase) {
	case 0:		/* Neutral state */
		if (context->buff[0] != 0) {	/* run count */
			context->compr.RunCount = context->buff[0];
			if (context->Type == 8)
				context->compr.RunCount *= 2;
			while (context->compr.RunCount > 0) {
				if (context->compr.linebuffdone & 1) {
					guchar *ptr = context->compr.linebuff +
					    context->compr.linebuffdone / 2;

					*ptr = (*ptr & 0xF0) | (context->buff[1] >> 4);
					context->buff[1] = (context->buff[1] << 4) |
							   (context->buff[1] >> 4);
					context->compr.linebuffdone++;
					context->compr.RunCount--;
				}

				if (context->compr.RunCount) {
					count = context->compr.linebuffsize -
					    context->compr.linebuffdone;
					if (count > context->compr.RunCount)
						count = context->compr.RunCount;

					memset (context->compr.linebuff +
						context->compr.linebuffdone / 2,
						context->buff[1],
						(count + 1) / 2);
					context->compr.RunCount -= count;
					context->compr.linebuffdone += count;
				}
				if (context->compr.linebuffdone == context->compr.linebuffsize) {
					guchar *tmp = context->buff;
					context->buff = context->compr.linebuff;
					OneLine (context);
					context->buff = tmp;

					if (context->compr.linebuffdone & 1)
						context->buff[1] = (context->buff[1] << 4) |
								   (context->buff[1] >> 4);
					context->compr.linebuffdone = 0;
				}
			}
		} else {	/* Escape */
			if (context->buff[1] == 0) {	/* End of line */
				if (context->compr.linebuffdone) {
					guchar *tmp = context->buff;
					context->buff = context->compr.linebuff;
					OneLine (context);
					context->buff = tmp;

					context->compr.linebuffdone = 0;
				}
			} else if (context->buff[1] == 1) {	/* End of image */
				if (context->compr.linebuffdone) {
					guchar *tmp = context->buff;
					context->buff = context->compr.linebuff;
					OneLine (context);
					context->buff = tmp;
				}

				context->compr.phase = 2;
			} else if (context->buff[1] == 2)	/* Cursor displacement */
				;	/* not implemented */
			else {
				context->compr.phase = 1;
				context->compr.RunCount = context->buff[1];
				if (context->Type == 8)
					context->compr.RunCount *= 2;
				context->BufferSize = (context->compr.RunCount + 3) / 4 * 2;
				context->buff = g_realloc (context->buff, context->BufferSize);
			}
		}
		context->BufferDone = 0;
		break;
	case 1:
		pos = 0;
		while (pos < context->compr.RunCount) {
			count = context->compr.linebuffsize - context->compr.linebuffdone;
			if (count > context->compr.RunCount)
				count = context->compr.RunCount;

			if ((context->compr.linebuffdone & 1) || (pos & 1)) {
				gint i, newval;
				guchar *ptr;
				for (i = 0; i < count; i++) {
					ptr = context->compr.linebuff + (i +
					      context->compr.linebuffdone) / 2;
					newval = *(context->buff + (pos + i) / 2) & (0xf0 >> (((pos + i) % 2) * 4));
					if (((pos + i) % 2) ^ ((context->compr.linebuffdone + i) % 2)) {
						if ((pos + i) % 2)
							newval <<= 4;
						else
							newval >>= 4;
					}
					*ptr = (*ptr & (0xf << (((i + context->compr.linebuffdone) % 2) * 4))) | newval;
				}
			} else {
				memmove (context->compr.linebuff +
					 context->compr.linebuffdone / 2,
					 context->buff + pos / 2,
					 (count + 1) / 2);
			}
			pos += count;
			context->compr.linebuffdone += count;
			if (context->compr.linebuffdone == context->compr.linebuffsize) {
				guchar *tmp = context->buff;
				context->buff = context->compr.linebuff;
				OneLine (context);
				context->buff = tmp;

				context->compr.linebuffdone = 0;
			}
		}
		context->compr.phase = 0;
		context->BufferSize = 2;
		context->buff = g_realloc (context->buff, context->BufferSize);
		context->BufferDone = 0;
		break;
	case 2:
		context->BufferDone = 0;
		break;
	}
}

/*
 * context - from image_begin_load
 * buf - new image data
 * size - length of new image data
 *
 * append image data onto inrecrementally built output image
 */
static gboolean
gdk_pixbuf__bmp_image_load_increment(gpointer data,
                                     const guchar * buf,
                                     guint size,
                                     GError **error)
{
	struct bmp_progressive_state *context =
	    (struct bmp_progressive_state *) data;

	gint BytesToCopy;

	if (context->read_state == READ_STATE_DONE)
		return TRUE;
	else if (context->read_state == READ_STATE_ERROR)
		return FALSE;

	while (size > 0) {
		if (context->BufferDone < context->BufferSize) {	/* We still
									   have headerbytes to do */
			BytesToCopy =
			    context->BufferSize - context->BufferDone;
			if (BytesToCopy > size)
				BytesToCopy = size;

			memmove(context->buff + context->BufferDone,
				buf, BytesToCopy);

			size -= BytesToCopy;
			buf += BytesToCopy;
			context->BufferDone += BytesToCopy;

			if (context->BufferDone != context->BufferSize)
				break;
		}

		switch (context->read_state) {
		case READ_STATE_HEADERS:
			if (!DecodeHeader (context->buff,
					   context->buff + 14, context,
					   error))
				return FALSE;

			break;

		case READ_STATE_PALETTE:
			DecodeColormap (context->buff, context, error);
			break;

		case READ_STATE_BITMASKS:
			decode_bitmasks (context, context->buff);
			break;

		case READ_STATE_DATA:
			if (context->Compressed == BI_RGB || context->Compressed == BI_BITFIELDS)
				OneLine (context);
			else
				DoCompressed (context);

			break;

		default:
			g_assert_not_reached ();
		}
	}

	return TRUE;
}

void
gdk_pixbuf__bmp_fill_vtable (GdkPixbufModule *module)
{
  module->load = gdk_pixbuf__bmp_image_load;
  module->begin_load = gdk_pixbuf__bmp_image_begin_load;
  module->stop_load = gdk_pixbuf__bmp_image_stop_load;
  module->load_increment = gdk_pixbuf__bmp_image_load_increment;
}
