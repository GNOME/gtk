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
#include <sys/mman.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "gdk-pixbuf/gdk-pixbuf.h"
#include "gdk-pixbuf/gdk-pixbuf-io.h"



struct headerpair {
	guint width;
	guint height;
	guint depth;
};

struct BitmapFileHeader {
	gushort	bfType;
	guint	bfSize;
	guint	reserverd;
	guint	bfOffbits;
};

struct BitmapInfoHeader {
	guint	biSize;
	guint	biWidth;
	guint	biHeight;
	gushort	biPlanes;
	gushort	biBitCount;
	guint	biCompression;
	guint 	biSizeImage;
	guint	biXPelsPerMeter;
	guint	biYPelsPerMeter;
	guint	biClrUsed;
	guint	biClrImportant;
};

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


	struct headerpair Header;	/* Decoded (BE->CPU) header */
	
	struct BitmapFileHeader  BFH;
	struct BitmapInfoHeader	 BIH;
	


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
		image_load_increment(State, membuf, length);
	} 
	g_free(membuf);
	if (State->pixbuf != NULL)
		gdk_pixbuf_ref(State->pixbuf);

	pb = State->pixbuf;

	image_stop_load(State);
	return State->pixbuf;
}

static void DecodeHeader(unsigned char *BFH,unsigned char *BIH,
		      struct bmp_progressive_state *State)
{
	State->Header.width = (BIH[7]<<24)+(BIH[6]<<16)+(BIH[5]<<8)+(BIH[4]);
	State->Header.height = (BIH[11]<<24)+(BIH[10]<<16)+(BIH[9]<<8)+(BIH[8]);
	State->Header.depth = (BIH[15]<<8)+(BIH[14]);;

	State->Type = State->Header.depth;	/* This may be less trivial someday */
	State->HeaderSize = 
			((BFH[13]<<24)+(BFH[12]<<16)+(BFH[11]<<8)+(BFH[10]));			   

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
	if ((State->LineWidth%4)>0)
		State->LineWidth=(State->LineWidth%4)*4+4;
	

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
	context->HeaderBuf = g_malloc(14 +
				      40 + 768);	
				      			/* 768 for the colormap */
	context->HeaderDone = 0;

	context->LineWidth = 0;
	context->LineBuf = NULL;
	context->LineDone = 0;
	context->Lines = 0;

	context->Type = 0;

	memset(&context->Header, 0, sizeof(struct headerpair));


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
	Pixels = context->pixbuf->art_pixbuf->pixels +
	    context->pixbuf->art_pixbuf->rowstride * (context->Header.height-context->Lines);
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
	Pixels = context->pixbuf->art_pixbuf->pixels +
	    context->pixbuf->art_pixbuf->rowstride * (context->Header.height-context->Lines);
	while (X < context->Header.width) {
		/* The joys of having a BGR byteorder */
		Pixels[X * 3 + 0] =
		    context->HeaderBuf[4*context->LineBuf[X] + 56];
		Pixels[X * 3 + 1] =
		    context->HeaderBuf[4*context->LineBuf[X] + 55];
		Pixels[X * 3 + 2] =
		    context->HeaderBuf[4*context->LineBuf[X] + 54];
		X++;
	}
}

static void OneLine1(struct bmp_progressive_state *context)
{
	gint X;
	guchar *Pixels;

	X = 0;
	Pixels = context->pixbuf->art_pixbuf->pixels +
	    context->pixbuf->art_pixbuf->rowstride * (context->Header.height-context->Lines);
	while (X < context->Header.width) {
		int Bit;
		
		Bit = (context->LineBuf[X/8])>>(7-(X&7));
		Bit = Bit & 1;
		/* The joys of having a BGR byteorder */
		Pixels[X * 3 + 0] =
		    context->HeaderBuf[Bit + 32];
		Pixels[X * 3 + 1] =
		    context->HeaderBuf[Bit + 2 + 32];
		Pixels[X * 3 + 2] =
		    context->HeaderBuf[Bit + 4 + 32];
		X++;
	}
}


static void OneLine(struct bmp_progressive_state *context)
{
	if (context->Type == 24)
		OneLine24(context);
	if (context->Type == 8)
		OneLine8(context);
	if (context->Type == 1)
		OneLine1(context);

	context->LineDone = 0;
	if (context->Lines > context->Header.height)
		return;
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

		if (context->HeaderDone >= 14+40)
			DecodeHeader(context->HeaderBuf,context->HeaderBuf+14,
				  context);


	}

	return TRUE;
}
