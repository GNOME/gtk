/* GdkPixbuf library - SUNRAS image loader
 *
 * Copyright (C) 1999 The Free Software Foundation
 *
 * Authors: Arjan van de Ven <arjan@fenrus.demon.nl>
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

#include <config.h>
#include <stdio.h>
#include <glib.h>
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

unsigned int ByteOrder(unsigned int i)
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

OneLineBGR does what it says: Reads one line from file.
Note: It also changes BGR pixelorder to RGB as libart currently
doesn't support ART_PIX_BGR.

*/
static OneLineBGR(FILE * f, guint Width, guchar * pixels, gint bpp)
{
	gint result, X;
	guchar DummyByte;

	result = fread(pixels, 1, Width * bpp, f);

	g_assert(result == Width * bpp);
	if (((Width * bpp) & 7) != 0)	/*  Not 16 bit aligned */
		fread(&DummyByte, 1, 1, f);
	X = 0;
	while (X < Width) {
		guchar Blue;
		Blue = pixels[X * bpp];
		pixels[X * bpp] = pixels[X * bpp + 2];
		pixels[X * bpp + 2] = Blue;
		X++;
	}
}

/* Shared library entry point */
GdkPixbuf *image_load(FILE * f)
{
	gint i, bpp, Y;
	guchar *pixels;
	struct rasterfile Header;

	i = fread(&Header, 1, sizeof(Header), f);
	g_assert(i == 32);

	/* Correct the byteorder of the header here */
	Header.width = ByteOrder(Header.width);
	Header.height = ByteOrder(Header.height);
	Header.depth = ByteOrder(Header.depth);
	Header.length = ByteOrder(Header.length);
	Header.type = ByteOrder(Header.type);
	Header.maptype = ByteOrder(Header.maptype);
	Header.maplength = ByteOrder(Header.maplength);


	bpp = 0;
	if (Header.depth == 32)
		bpp = 4;
	else
		bpp = 3;

	g_assert(bpp != 0);	/* Only 24 and 32 bpp for now */

	pixels = (guchar *) malloc(Header.width * Header.height * bpp);
	if (!pixels) {
		return NULL;
	}

	/* 

	   Loop through the file, one line at a time. 
	   Only BGR-style files are handled right now.

	 */
	Y = 0;
	while (Y < Header.height) {
		OneLineBGR(f, Header.width,
			   &pixels[Y * Header.width * bpp], bpp);
		Y++;
	}


	if (bpp == 4)
		return gdk_pixbuf_new_from_data(pixels, ART_PIX_RGB, TRUE,
						Header.width,
						Header.height,
						Header.width * bpp,
						free_buffer, NULL);
	else
		return gdk_pixbuf_new_from_data(pixels, ART_PIX_RGB, FALSE,
						Header.width,
						Header.height,
						Header.width * bpp,
						free_buffer, NULL);
}
