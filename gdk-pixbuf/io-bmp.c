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

Known bugs:
	* Compressed files don't work yet
	* bi-tonal files aren't tested 

*/

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"




struct headerpair {
	guint width;
	guint height;
	guint depth;
	guint Negative;		/* Negative = 1 -> top down BMP,  
				   Negative = 0 -> bottom up BMP */
};

/* 

These structures are actually dummies. These are according to
the "Windows API reference guide volume II" as written by Borland,
but GCC fiddles with the alignment of the internal members.

*/

struct BitmapFileHeader {
	gushort bfType;
	guint bfSize;
	guint reserverd;
	guint bfOffbits;
};

struct BitmapInfoHeader {
	guint biSize;
	guint biWidth;
	guint biHeight;
	gushort biPlanes;
	gushort biBitCount;
	guint biCompression;
	guint biSizeImage;
	guint biXPelsPerMeter;
	guint biYPelsPerMeter;
	guint biClrUsed;
	guint biClrImportant;
};

static void DumpBIH(unsigned char *BIH)
{				/* For debugging */
	printf("biSize      = %i \n",
	       (BIH[3] << 24) + (BIH[2] << 16) + (BIH[1] << 8) + (BIH[0]));
	printf("biWidth     = %i \n",
	       (BIH[7] << 24) + (BIH[6] << 16) + (BIH[5] << 8) + (BIH[4]));
	printf("biHeight    = %i \n",
	       (BIH[11] << 24) + (BIH[10] << 16) + (BIH[9] << 8) +
	       (BIH[8]));
	printf("biPlanes    = %i \n", (BIH[13] << 8) + (BIH[12]));
	printf("biBitCount  = %i \n", (BIH[15] << 8) + (BIH[14]));
	printf("biCompress  = %i \n",
	       (BIH[19] << 24) + (BIH[18] << 16) + (BIH[17] << 8) +
	       (BIH[16]));
	printf("biSizeImage = %i \n",
	       (BIH[23] << 24) + (BIH[22] << 16) + (BIH[21] << 8) +
	       (BIH[20]));
	printf("biXPels     = %i \n",
	       (BIH[27] << 24) + (BIH[26] << 16) + (BIH[25] << 8) +
	       (BIH[24]));
	printf("biYPels     = %i \n",
	       (BIH[31] << 24) + (BIH[30] << 16) + (BIH[29] << 8) +
	       (BIH[28]));
	printf("biClrUsed   = %i \n",
	       (BIH[35] << 24) + (BIH[34] << 16) + (BIH[33] << 8) +
	       (BIH[32]));
	printf("biClrImprtnt= %i \n",
	       (BIH[39] << 24) + (BIH[38] << 16) + (BIH[37] << 8) +
	       (BIH[36]));
}

/* 
	This does a byte-order swap. Does glib have something like
	be32_to_cpu() ??
*/

static unsigned int le32_to_cpu(guint i)
{
	unsigned int i2;
	return i2;
}

/* 
	Destroy notification function for the libart pixbuf 
*/

static void free_buffer(gpointer user_data, gpointer data)
{
	free(data);
}


/* Data needed for the "state" during decompression */
struct bmp_compression_state {
	gint phase;		/* 0 = clean, 
				   1 = count received
				   2 = escape received
				   3 = in "raw" run
				   4 = Relative part 1 is next
				   5 = Relative part 2 is next
				   6 = end of image -> No more input allowed
				 */
	gint RunCount;
	gint XDelta;
	gint YDelta;
};

/* Progressive loading */

struct bmp_progressive_state {
	ModulePreparedNotifyFunc prepared_func;
	ModuleUpdatedNotifyFunc updated_func;
	gpointer user_data;

	gint HeaderSize;	/* The size of the header-part (incl colormap) */
	guchar *HeaderBuf;	/* The buffer for the header (incl colormap) */
	gint HeaderDone;	/* The nr of bytes actually in HeaderBuf */

	gint LineWidth;		/* The width of a line in bytes */
	guchar *LineBuf;	/* Buffer for 1 line */
	gint LineDone;		/* # of bytes in LineBuf */
	gint Lines;		/* # of finished lines */

	gint Type;		/*  
				   24 = RGB
				   8 = 8 bit colormapped
				   1  = 1 bit bitonal 
				 */
	gint Compressed;
	struct bmp_compression_state compr;


	struct headerpair Header;	/* Decoded (BE->CPU) header */


	GdkPixbuf *pixbuf;	/* Our "target" */
};

gpointer
image_begin_load(ModulePreparedNotifyFunc prepared_func,
		 ModuleUpdatedNotifyFunc updated_func, gpointer user_data);
void image_stop_load(gpointer data);
gboolean image_load_increment(gpointer data, guchar * buf, guint size);



/* Shared library entry point */
GdkPixbuf *image_load(FILE * f)
{
	guchar *membuf;
	size_t length;
	struct bmp_progressive_state *State;
	int fd;

	GdkPixbuf *pb;

	State = image_begin_load(NULL, NULL, NULL);
	membuf = g_malloc(4096);

	g_assert(membuf != NULL);


	while (feof(f) == 0) {
		length = fread(membuf, 1, 4096, f);
		if (length > 0)
			image_load_increment(State, membuf, length);

	}
	g_free(membuf);
	if (State->pixbuf != NULL)
		gdk_pixbuf_ref(State->pixbuf);

	pb = State->pixbuf;

	image_stop_load(State);
	return State->pixbuf;
}

static void DecodeHeader(unsigned char *BFH, unsigned char *BIH,
			 struct bmp_progressive_state *State)
{
/*	DumpBIH(BIH);*/
	State->Header.width =
	    (BIH[7] << 24) + (BIH[6] << 16) + (BIH[5] << 8) + (BIH[4]);
	State->Header.height =
	    (BIH[11] << 24) + (BIH[10] << 16) + (BIH[9] << 8) + (BIH[8]);
	State->Header.depth = (BIH[15] << 8) + (BIH[14]);;

	State->Type = State->Header.depth;	/* This may be less trivial someday */
	State->HeaderSize =
	    ((BFH[13] << 24) + (BFH[12] << 16) + (BFH[11] << 8) +
	     (BFH[10]));
	if (State->HeaderSize >= 14 + 40 + 768 + 512)
		State->HeaderSize = 14 + 40 + 768 + 512 - 1;

	if ((BIH[16] != 0) || (BIH[17] != 0) || (BIH[18] != 0)
	    || (BIH[19] != 0)) {
		State->Compressed = 1;
	}

	if (State->Header.height < 0) {
		State->Header.height = -State->Header.height;
		State->Header.Negative = 1;
	}
	if (State->Header.width < 0) {
		State->Header.width = -State->Header.width;
		State->Header.Negative = 0;
	}

	if (State->Type == 24)
		State->LineWidth = State->Header.width * 3;
	if (State->Type == 8)
		State->LineWidth = State->Header.width * 1;
	if (State->Type == 1) {
		State->LineWidth = State->Header.width / 8;
		if ((State->Header.width & 7) != 0)
			State->LineWidth++;
	}

	/* Pad to a 32 bit boundary */
	if (((State->LineWidth % 4) > 0) && (State->Compressed == 0))
		State->LineWidth = (State->LineWidth / 4) * 4 + 4;


	if (State->LineBuf == NULL)
		State->LineBuf = g_malloc(State->LineWidth);

	g_assert(State->LineBuf != NULL);


	if (State->pixbuf == NULL) {
		State->pixbuf =
		    gdk_pixbuf_new(ART_PIX_RGB, FALSE, 8,
				   (gint) State->Header.width,
				   (gint) State->Header.height);

		if (State->prepared_func != NULL)
			/* Notify the client that we are ready to go */
			(*State->prepared_func) (State->pixbuf,
						 State->user_data);

	}

}

/* 
 * func - called when we have pixmap created (but no image data)
 * user_data - passed as arg 1 to func
 * return context (opaque to user)
 */

gpointer
image_begin_load(ModulePreparedNotifyFunc prepared_func,
		 ModuleUpdatedNotifyFunc updated_func, gpointer user_data)
{
	struct bmp_progressive_state *context;

	context = g_new0(struct bmp_progressive_state, 1);
	context->prepared_func = prepared_func;
	context->updated_func = updated_func;
	context->user_data = user_data;

	context->HeaderSize = 54;
	context->HeaderBuf = g_malloc(14 + 40 + 768 + 512);
	/* 768 for the colormap */
	context->HeaderDone = 0;

	context->LineWidth = 0;
	context->LineBuf = NULL;
	context->LineDone = 0;
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
void image_stop_load(gpointer data)
{
	struct bmp_progressive_state *context =
	    (struct bmp_progressive_state *) data;


	g_return_if_fail(context != NULL);

	if (context->LineBuf != NULL)
		g_free(context->LineBuf);
	context->LineBuf = NULL;
	if (context->HeaderBuf != NULL)
		g_free(context->HeaderBuf);

	if (context->pixbuf)
		gdk_pixbuf_unref(context->pixbuf);

	g_free(context);
}


static void OneLine24(struct bmp_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = context->pixbuf->art_pixbuf->pixels +
		    gdk_pixbuf_get_rowstride(context->pixbuf) *
		    (context->Header.height - context->Lines - 1);
	else
		Pixels = context->pixbuf->art_pixbuf->pixels +
		    gdk_pixbuf_get_rowstride(context->pixbuf) *
		    context->Lines;
	while (X < context->Header.width) {
		Pixels[X * 3 + 0] = context->LineBuf[X * 3 + 2];
		Pixels[X * 3 + 1] = context->LineBuf[X * 3 + 1];
		Pixels[X * 3 + 2] = context->LineBuf[X * 3 + 0];
		X++;
	}

}

static void OneLine8(struct bmp_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = context->pixbuf->art_pixbuf->pixels +
		    gdk_pixbuf_get_rowstride(context->pixbuf) *
		    (context->Header.height - context->Lines - 1);
	else
		Pixels = context->pixbuf->art_pixbuf->pixels +
		    gdk_pixbuf_get_rowstride(context->pixbuf) *
		    context->Lines;
	while (X < context->Header.width) {
		/* The joys of having a BGR byteorder */
		Pixels[X * 3 + 0] =
		    context->HeaderBuf[4 * context->LineBuf[X] + 56];
		Pixels[X * 3 + 1] =
		    context->HeaderBuf[4 * context->LineBuf[X] + 55];
		Pixels[X * 3 + 2] =
		    context->HeaderBuf[4 * context->LineBuf[X] + 54];
		X++;
	}
}

static void OneLine1(struct bmp_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	if (context->Header.Negative == 0)
		Pixels = context->pixbuf->art_pixbuf->pixels +
		    gdk_pixbuf_get_rowstride(context->pixbuf) *
		    (context->Header.height - context->Lines - 1);
	else
		Pixels = context->pixbuf->art_pixbuf->pixels +
		    gdk_pixbuf_get_rowstride(context->pixbuf) *
		    context->Lines;
	while (X < context->Header.width) {
		int Bit;

		Bit = (context->LineBuf[X / 8]) >> (7 - (X & 7));
		Bit = Bit & 1;
		/* The joys of having a BGR byteorder */
		Pixels[X * 3 + 0] = context->HeaderBuf[Bit + 32];
		Pixels[X * 3 + 1] = context->HeaderBuf[Bit + 2 + 32];
		Pixels[X * 3 + 2] = context->HeaderBuf[Bit + 4 + 32];
		X++;
	}
}


static void OneLine(struct bmp_progressive_state *context)
{
	context->LineDone = 0;
	if (context->Lines >= context->Header.height)
		return;

	if (context->Type == 24)
		OneLine24(context);
	if (context->Type == 8)
		OneLine8(context);
	if (context->Type == 1)
		OneLine1(context);

	context->Lines++;

	if (context->updated_func != NULL) {
		(*context->updated_func) (context->pixbuf,
					  context->user_data,
					  0,
					  context->Lines,
					  context->Header.width,
					  context->Header.height);

	}
}

/*
 * context - from image_begin_load
 * buf - new image data
 * size - length of new image data
 *
 * append image data onto inrecrementally built output image
 */
gboolean image_load_increment(gpointer data, guchar * buf, guint size)
{
	struct bmp_progressive_state *context =
	    (struct bmp_progressive_state *) data;

	gint BytesToCopy;

	while (size > 0) {
		g_assert(context->LineDone >= 0);
		if (context->HeaderDone < context->HeaderSize) {	/* We still 
									   have headerbytes to do */
			BytesToCopy =
			    context->HeaderSize - context->HeaderDone;
			if (BytesToCopy > size)
				BytesToCopy = size;

			memcpy(context->HeaderBuf + context->HeaderDone,
			       buf, BytesToCopy);

			size -= BytesToCopy;
			buf += BytesToCopy;
			context->HeaderDone += BytesToCopy;

		} else if (context->Compressed) {
			/* Compression is done 1 at a time for now */
			switch (context->compr.phase) {
			case 0:
				if (buf[0] != 0) {	/* run count */
					context->compr.phase = 1;
					context->compr.RunCount = buf[0];
				} else {	/* Escape */
					context->compr.phase = 2;
				}
				buf++;
				size--;
				break;
			case 1:	/* Run count received.... */
				while (context->compr.RunCount > 0) {
					BytesToCopy =
					    context->LineWidth -
					    context->LineDone;
					if (BytesToCopy >
					    context->compr.
					    RunCount) BytesToCopy =
						    context->compr.
						    RunCount;
					if (BytesToCopy > 0) {
						memset(context->LineBuf +
						       context->LineDone,
						       buf[0],
						       BytesToCopy);

						context->compr.RunCount -=
						    BytesToCopy;
						context->LineDone +=
						    BytesToCopy;
					}
					if (
					    (context->LineDone >=
					     context->LineWidth)
					    && (context->LineWidth > 0)) {
						OneLine(context);
					}
				}
				context->compr.phase = 0;
				buf++;
				size--;
				break;
			case 2:	/* Escape received */
				if (buf[0] == 0) {	/* End of line */
					context->compr.phase = 0;
					if (context->LineDone > 0)
						OneLine(context);
				} else if (buf[0] == 1) {	/* End of image */
					OneLine(context);
					context->compr.phase = 6;
					size = 0;
					break;
				} else if (buf[0] == 2) {	/* Cursor displacement */
					context->compr.phase = 4;
				} else {
					context->compr.phase = 3;
					context->compr.RunCount = buf[0];
				}
				buf++;
				size--;

				break;
			case 3:
				while ((context->compr.RunCount > 0)
				       && (size > 0)) {
					BytesToCopy =
					    context->LineWidth -
					    context->LineDone;
					if (BytesToCopy >
					    context->compr.
					    RunCount) BytesToCopy =
						    context->compr.
						    RunCount;
					if (BytesToCopy > size)
						BytesToCopy = size;

					if (BytesToCopy > 0) {
						memcpy(context->LineBuf +
						       context->LineDone,
						       buf, BytesToCopy);

						context->compr.RunCount -=
						    BytesToCopy;
						buf += BytesToCopy;
						size -= BytesToCopy;
						context->LineDone +=
						    BytesToCopy;
					}
					if (
					    (context->LineDone >=
					     context->LineWidth)
					    && (context->LineWidth > 0))
						OneLine(context);
				}
				if (context->compr.RunCount <= 0)
					context->compr.phase = 0;

				break;
			case 4:
				context->compr.phase = 5;
				context->compr.XDelta = buf[0];
				buf++;
				size--;
				break;
			case 5:
				context->compr.phase = 0;
				context->compr.YDelta = buf[0];
				g_assert(0);	/* No implementatio of this yet */
				buf++;
				size--;
				break;
			case 6:
				size = 0;
			}

		} else {
			/* Pixeldata only */
			BytesToCopy =
			    context->LineWidth - context->LineDone;
			if (BytesToCopy > size)
				BytesToCopy = size;

			if (BytesToCopy > 0) {
				memcpy(context->LineBuf +
				       context->LineDone, buf,
				       BytesToCopy);

				size -= BytesToCopy;
				buf += BytesToCopy;
				context->LineDone += BytesToCopy;
			}
			if ((context->LineDone >= context->LineWidth) &&
			    (context->LineWidth > 0))
				OneLine(context);


		}

		if (context->HeaderDone >= 14 + 40)
			DecodeHeader(context->HeaderBuf,
				     context->HeaderBuf + 14, context);


	}

	return TRUE;
}
