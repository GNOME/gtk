/* GdkPixbuf library - SUNRAS image loader
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Arjan van de Ven <arjan@fenrus.demon.nl>
 *          Federico Mena-Quintero <federico@gimp.org>
 *
 * Based on io-gif.c, io-tiff.c and io-png.c
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
	* "Indexed" (incl grayscale) sunras files don't work
	* 1 bpp sunrasfiles don't work

*/

#include <config.h>
#include <stdio.h>
#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-io.h"



/* 
   Header structure for sunras files.
   All values are in big-endian order on disk
 */

struct rasterfile {
	guint magic;
	guint width;
	guint height;
	guint depth;
	guint length;
	guint type;
	guint maptype;
	guint maplength;
};

/* 
	This does a byte-order swap. Does glib have something like
	be32_to_cpu() ??
*/

static unsigned int be32_to_cpu(guint i)
{
	unsigned int i2;
	i2 =
	    ((i & 255) << 24) | (((i >> 8) & 255) << 16) |
	    (((i >> 16) & 255) << 8) | ((i >> 24) & 255);
	return i2;
}

/* 
	Destroy notification function for the libart pixbuf 
*/

static void free_buffer(gpointer user_data, gpointer data)
{
	free(data);
}

/*

OneLineBGR_buf/file does what it says: Reads one line from file or buffer.
Note: It also changes BGR pixelorder to RGB as libart currently
doesn't support ART_PIX_BGR.

*/
static void OneLineBGR_buf(guchar * buffer, guint Width, guchar * pixels,
			   guint bpp)
{
	guint X;

	memcpy(pixels, buffer, (size_t) (Width * bpp));
	X = 0;
	while (X < Width) {
		guchar Blue;
		Blue = pixels[X * bpp];
		pixels[X * bpp] = pixels[X * bpp + 2];
		pixels[X * bpp + 2] = Blue;
		X++;
	}
}

static void OneLineBGR_file(FILE * f, guint Width, guchar * pixels,
			    guint bpp)
{
	size_t result;
	guint X;
	guchar DummyByte;

	result = fread(pixels, 1, (size_t) (Width * bpp), f);

	g_assert(result == (size_t) (Width * bpp));
	if (((Width * bpp) & 1) != 0)	/*  Not 16 bit aligned */
		(void) fread(&DummyByte, 1, 1, f);
	X = 0;
	while (X < Width) {
		guchar Blue;
		Blue = pixels[X * bpp];
		pixels[X * bpp] = pixels[X * bpp + 2];
		pixels[X * bpp + 2] = Blue;
		X++;
	}
}

static void OneLineMapped_file(FILE * f, guint Width, guchar * pixels,
			    guint bpp, guchar *Map)
{
	size_t result;
	guint X;
	guchar DummyByte;
	guchar *buffer;
	
	
	buffer =g_malloc((size_t) (Width * bpp));
	
	g_assert(buffer!=NULL);

	result = fread(buffer, 1, (size_t) (Width * bpp), f);

	g_assert(result == (size_t) (Width * bpp));
	
	if (((Width * bpp) & 1) != 0)	/*  Not 16 bit aligned */
		(void) fread(&DummyByte, 1, 1, f);
	X = 0;
	while (X < Width) {
		pixels[X * 3]   = Map[buffer[X]*3];
		pixels[X * 3+1] = Map[buffer[X]*3];
		pixels[X * 3+2] = Map[buffer[X]*3];

		X++;
	}
	g_free(buffer);
}

static void OneLineMapped_buf(guchar *buffer, guint Width, guchar * pixels,
			    guint bpp,guchar *Map)
{
	size_t result;
	guint X;
	
	X = 0;
	while (X < Width) {
		pixels[X * 3]   = Map[buffer[X]*3];
		pixels[X * 3+1] = Map[buffer[X]*3+1];
		pixels[X * 3+2] = Map[buffer[X]*3+2];

		X++;
	}
}

/* Shared library entry point */
GdkPixbuf *image_load(FILE * f)
{
	guint bpp;
	guint Y;
	size_t i;
	guchar *pixels;
	guchar ColorMap[768];
	struct rasterfile Header;

	i = fread(&Header, 1, sizeof(Header), f);
	g_assert(i == 32);
	
	/* Fill default colormap */
	for (Y=0;Y<256;Y++) {
		ColorMap[Y*3+0]=Y;
		ColorMap[Y*3+1]=Y;
		ColorMap[Y*3+2]=Y;
	}
	/* Correct the byteorder of the header here */
	Header.width = be32_to_cpu(Header.width);
	Header.height = be32_to_cpu(Header.height);
	Header.depth = be32_to_cpu(Header.depth);
	Header.length = be32_to_cpu(Header.length);
	Header.type = be32_to_cpu(Header.type);
	Header.maptype = be32_to_cpu(Header.maptype);
	Header.maplength = be32_to_cpu(Header.maplength);


	bpp = 0;
	if (Header.depth == 32)
		bpp = 4;
	if (Header.depth == 24)
		bpp = 3;
	if (Header.depth == 8)
		bpp = 1;
		
	printf("mapl %i \n",Header.maplength);
	if (Header.maplength>0)
		i = fread(ColorMap,1,Header.maplength,f);
				

	g_assert(bpp != 0);	/* Only 24 and 32 bpp for now */

	if (bpp==4)
		pixels = (guchar *) g_malloc(Header.width * Header.height * 4);
	else
		pixels = (guchar *) g_malloc(Header.width * Header.height * 3);
	if (!pixels) {
		return NULL;
	}

	/* 

	   Loop through the file, one line at a time. 
	   Only BGR-style files are handled right now.

	 */
	Y = 0;
	while (Y < Header.height) {
		if (bpp>1) 
			OneLineBGR_file(f, Header.width,
				&pixels[Y * Header.width * bpp], bpp);
		else
			OneLineMapped_file(f, Header.width,
				&pixels[Y * Header.width * 3], bpp,&ColorMap[0]);
		Y++;
	}


	if (bpp == 4)
		return gdk_pixbuf_new_from_data(pixels, ART_PIX_RGB, TRUE,
						(gint) Header.width,
						(gint) Header.height,
						(gint) (Header.width *
							4), free_buffer,
						NULL);
	else
		return gdk_pixbuf_new_from_data(pixels, ART_PIX_RGB, FALSE,
						(gint) Header.width,
						(gint) Header.height,
						(gint) (Header.width *
							3), free_buffer,
						NULL);
}


/* Progressive loading */

struct ras_progressive_state {
	ModulePreparedNotifyFunc prepared_func;
	ModuleUpdatedNotifyFunc updated_func;
	gpointer user_data;

	GdkPixbuf *pixbuf;	/* Our "target" */
	guchar *PixelData;	/* pointer to the next line */
	struct rasterfile Header;
	guint bpp;
	guint BytesPerLine;
	guchar *linebuf;
	guchar *ColorMap;
	guint BytesInLB;
	guint LinesDone;



	gboolean have_header;
};

/* 
 * func - called when we have pixmap created (but no image data)
 * user_data - passed as arg 1 to func
 * return context (opaque to user)
 */

gpointer
image_begin_load(ModulePreparedNotifyFunc prepared_func,
		 ModuleUpdatedNotifyFunc updated_func, gpointer user_data)
{
	struct ras_progressive_state *context;

	context = g_new0(struct ras_progressive_state, 1);

	context->prepared_func = prepared_func;
	context->updated_func = updated_func;
	context->user_data = user_data;
	context->pixbuf = NULL;
	context->linebuf = NULL;
	context->bpp = 0;
	context->have_header = 0;
	context->BytesInLB = 0;
	context->BytesPerLine = 0;
	context->LinesDone = 0;
	context->ColorMap = NULL;

	return (gpointer) context;
}

/*
 * context - returned from image_begin_load
 *
 * free context, unref gdk_pixbuf
 */
void image_stop_load(gpointer data)
{
	struct ras_progressive_state *context =
	    (struct ras_progressive_state *) data;

	g_return_if_fail(context != NULL);

	if (context->pixbuf)
		gdk_pixbuf_unref(context->pixbuf);

	if (context->linebuf)
		g_free(context->linebuf);
	if (context->ColorMap)
		g_free(context->ColorMap);
	g_free(context);
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
	struct ras_progressive_state *context =
	    (struct ras_progressive_state *) data;

	if ((context->have_header == 0) && (size < 32))
		return FALSE;

	if (context->have_header == FALSE) {
		memcpy(&context->Header, buf, 32);
		buf += 32;
		size -= 32;

		context->Header.width = be32_to_cpu(context->Header.width);
		context->Header.height =
		    be32_to_cpu(context->Header.height);
		context->Header.depth = be32_to_cpu(context->Header.depth);
		context->Header.length =
		    be32_to_cpu(context->Header.length);
		context->Header.type = be32_to_cpu(context->Header.type);
		context->Header.maptype =
		    be32_to_cpu(context->Header.maptype);
		context->Header.maplength =
		    be32_to_cpu(context->Header.maplength);

		context->bpp = 0;
		if (context->Header.depth == 32)
			context->bpp = 4;
		if (context->Header.depth == 24)
			context->bpp = 3;
		if (context->Header.depth == 8)
			context->bpp = 1;
			

		g_assert(context->bpp != 0);	/* Only 24 and 32 bpp for now */


		if (context->bpp == 4)
			context->pixbuf = gdk_pixbuf_new(ART_PIX_RGB, TRUE,
							 8,
							 (gint)
							 context->Header.
							 width,
							 (gint)
							 context->Header.
							 height);
		else
			context->pixbuf =
			    gdk_pixbuf_new(ART_PIX_RGB, FALSE, 8,
					   (gint) context->Header.width,
					   (gint) context->Header.height);

		/* Use pixbuf buffer to store decompressed data */
		g_assert(context->pixbuf != NULL);
		g_assert(context->pixbuf->art_pixbuf != NULL);
		context->PixelData = context->pixbuf->art_pixbuf->pixels;
		g_assert(context->PixelData != NULL);
		context->BytesPerLine =
		    context->Header.width * context->bpp;

		if (((context->BytesPerLine) & 1) != 0)	/*  Not 16 bit aligned */
			context->BytesPerLine++;

		context->linebuf =
		    (guchar *) g_malloc(context->BytesPerLine);
		context->have_header = TRUE;
		if (context->linebuf == NULL) {
			/* Failed to allocate memory */
			g_error("Couldn't allocate linebuf");
		}


		/* Notify the client that we are ready to go */
		(*context->prepared_func) (context->pixbuf,
					   context->user_data);
	}


	while (size > 0) {
		guint ToDo;
		ToDo = context->BytesPerLine - context->BytesInLB;
		if (ToDo > size)
			ToDo = size;
		memcpy(context->linebuf + context->BytesInLB,
		       buf, (size_t) ToDo);
		size -= ToDo;
		buf += ToDo;
		context->BytesInLB += ToDo;
		if (context->BytesInLB >= context->BytesPerLine) {
			/* linebuf if full */
			if (context->bpp>1)
				OneLineBGR_buf(context->linebuf,
				       context->Header.width,
				       context->PixelData, context->bpp);
			else				     
				OneLineMapped_buf(context->linebuf,
				       context->Header.width,
				       context->PixelData, context->bpp,
				       context->ColorMap);
			context->BytesInLB = 0;
			context->PixelData +=
			    context->pixbuf->art_pixbuf->rowstride;
			context->LinesDone++;

			(*context->updated_func) (context->pixbuf,
						  context->user_data,
						  0,
						  context->LinesDone,
						  context->Header.width,
						  context->Header.height);


		}

	}

	return TRUE;
}

