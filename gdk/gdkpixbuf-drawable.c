/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/* GdkPixbuf library - convert X drawable information to RGB
 *
 * Copyright (C) 1999 Michael Zucchi
 *
 * Authors: Michael Zucchi <zucchi@zedzone.mmc.com.au>
 *          Cody Russell <bratsche@dfw.net>
 * 	    Federico Mena-Quintero <federico@gimp.org>
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
#include <string.h>
#include "gdk-pixbuf-private.h"

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define LITTLE
#endif
#define d(x)



static guint32 mask_table[] = {
	0x00000000, 0x00000001, 0x00000003, 0x00000007,
	0x0000000f, 0x0000001f, 0x0000003f, 0x0000007f,
	0x000000ff, 0x000001ff, 0x000003ff, 0x000007ff,
	0x00000fff, 0x00001fff, 0x00003fff, 0x00007fff,
	0x0000ffff, 0x0001ffff, 0x0003ffff, 0x0007ffff,
	0x000fffff, 0x001fffff, 0x003fffff, 0x007fffff,
	0x00ffffff, 0x01ffffff, 0x03ffffff, 0x07ffffff,
	0x0fffffff, 0x1fffffff, 0x3fffffff, 0x7fffffff,
	0xffffffff
};



/*
  convert 1 bits-pixel data
  no alpha
*/
static void
rgb1 (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	guint8 *s;
	register guint8 data;
	guint8 *o;
	guint8 *srow = image->mem, *orow = pixels;

	d (printf ("1 bits/pixel\n"));

	/* convert upto 8 pixels/time */
	/* its probably not worth trying to make this run very fast, who uses
	   1 bit displays anymore? */
	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;

		for (xx = 0; xx < width; xx ++) {
			data = srow[xx >> 3] >> (7 - (xx & 7)) & 1;
			*o++ = colormap->colors[data].red;
			*o++ = colormap->colors[data].green;
			*o++ = colormap->colors[data].blue;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 1 bits/pixel data
  with alpha
*/
static void
rgb1a (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	guint8 *s;
	register guint8 data;
	guint8 *o;
	guint8 *srow = image->mem, *orow = pixels;
	guint32 remap[2];

	d (printf ("1 bits/pixel\n"));

	/* convert upto 8 pixels/time */
	/* its probably not worth trying to make this run very fast, who uses
	   1 bit displays anymore? */
	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (xx = 0; xx < 2; xx++) {
#ifdef LITTLE
		remap[xx] = 0xff000000
			| colormap->colors[xx].blue << 16
			| colormap->colors[xx].green << 8
			| colormap->colors[xx].red;
#else
		remap[xx] = 0xff
			| colormap->colors[xx].red << 24
			| colormap->colors[xx].green << 16
			| colormap->colors[xx].blue << 8;
#endif
	}

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;

		for (xx = 0; xx < width; xx ++) {
			data = srow[xx >> 3] >> (7 - (xx & 7)) & 1;
			*o++ = remap[data];
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 8 bits/pixel data
  no alpha
*/
static void
rgb8 (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	guint32 mask;
	register guint32 data;
	guint8 *srow = image->mem, *orow = pixels;
	register guint8 *s;
	register guint8 *o;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	d (printf ("8 bit, no alpha output\n"));

	mask = mask_table[image->depth];

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			data = *s++ & mask;
			*o++ = colormap->colors[data].red;
			*o++ = colormap->colors[data].green;
			*o++ = colormap->colors[data].blue;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 8 bits/pixel data
  with alpha
*/
static void
rgb8a (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	guint32 mask;
	register guint32 data;
	guint32 remap[256];
	register guint8 *s;	/* read 2 pixels at once */
	register guint32 *o;
	guint8 *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	d (printf ("8 bit, with alpha output\n"));

	mask = mask_table[image->depth];

	for (xx = 0; xx < colormap->size; xx++) {
#ifdef LITTLE
		remap[xx] = 0xff000000
			| colormap->colors[xx].blue << 16
			| colormap->colors[xx].green << 8
			| colormap->colors[xx].red;
#else
		remap[xx] = 0xff
			| colormap->colors[xx].red << 24
			| colormap->colors[xx].green << 16
			| colormap->colors[xx].blue << 8;
#endif
	}

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = (guint32 *) orow;
		for (xx = 0; xx < width; xx ++) {
			data = *s++ & mask;
			*o++ = remap[data];
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 16 bits/pixel data
  no alpha
  data in lsb format
*/
static void
rgb565lsb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register guint32 *s;	/* read 2 pixels at once */
#else
	register guint8 *s;	/* read 2 pixels at once */
#endif
	register guint16 *o;
	guint8 *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (guint32 *) srow;
#else
		s = (guint32 *) srow;
#endif
		o = (guint16 *) orow;
		for (xx = 1; xx < width; xx += 2) {
			register guint32 data;
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0xf800) >> 8 | (data & 0x7e0) << 5;
			*o++ = (data & 0x1f) << 3 | (data & 0xf8000000) >> 16;
			*o++ = ((data & 0x7e00000) >> 19) | (data & 0x1f0000) >> 5;
#else
			/* swap endianness first */
			data = s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
			s += 4;
			*o++ = (data & 0xf800) | (data & 0x7e0) >> 3;
			*o++ = (data & 0x1f) << 11 | (data & 0xf8000000) >> 24;
			*o++ = ((data & 0x7e00000) >> 11) | (data & 0x1f0000) >> 13;
#endif
		}
		/* check for last remaining pixel */
		if (width & 1) {
			register guint16 data;
#ifdef LITTLE
			data = *((short *) s);
#else
			data = *((short *) s);
			data = ((data >> 8) & 0xff) | ((data & 0xff) << 8);
#endif
			((char *) o)[0] = (data >> 8) & 0xf8;
			((char *) o)[1] = (data >> 3) & 0xfc;
			((char *) o)[2] = (data << 3) & 0xf8;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 16 bits/pixel data
  no alpha
  data in msb format
*/
static void
rgb565msb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register guint8 *s;	/* need to swap data order */
#else
	register guint32 *s;	/* read 2 pixels at once */
#endif
	register guint16 *o;
	guint8 *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = srow;
#else
		s = (guint32 *) srow;
#endif
		o = (guint16 *) orow;
		for (xx = 1; xx < width; xx += 2) {
			register guint32 data;
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
			s += 4;
			*o++ = (data & 0xf800) >> 8 | (data & 0x7e0) << 5;
			*o++ = (data & 0x1f) << 3 | (data & 0xf8000000) >> 16;
			*o++ = ((data & 0x7e00000) >> 19) | (data & 0x1f0000) >> 5;
#else
			data = *s++;
			*o++ = (data & 0xf800) | (data & 0x7e0) >> 3;
			*o++ = (data & 0x1f) << 11 | (data & 0xf8000000) >> 24;
			*o++ = ((data & 0x7e00000) >> 11) | (data & 0x1f0000) >> 13;
#endif
		}
		/* check for last remaining pixel */
		if (width & 1) {
			register guint16 data;
#ifdef LITTLE
			data = *((short *) s);
			data = ((data >> 8) & 0xff) | ((data & 0xff) << 8);
#else
			data = *((short *) s);
#endif
			((char *) o)[0] = (data >> 8) & 0xf8;
			((char *) o)[1] = (data >> 3) & 0xfc;
			((char *) o)[2] = (data << 3) & 0xf8;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 16 bits/pixel data
  with alpha
  data in lsb format
*/
static void
rgb565alsb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register guint16 *s;	/* read 1 pixels at once */
#else
	register guint8 *s;
#endif
	register guint32 *o;

	guint8 *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (guint16 *) srow;
#else
		s = (guint8 *) srow;
#endif
		o = (guint32 *) orow;
		for (xx = 0; xx < width; xx ++) {
			register guint32 data;
			/*  rrrrrggg gggbbbbb -> rrrrr000 gggggg00 bbbbb000 aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbb000 gggggg00 rrrrr000 */
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0xf800) >> 8 | (data & 0x7e0) << 5
				| (data & 0x1f) << 19 | 0xff000000;
#else
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0xf800) << 16 | (data & 0x7e0) << 13
				| (data & 0x1f) <<  11 | 0xff;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 16 bits/pixel data
  with alpha
  data in msb format
*/
static void
rgb565amsb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register guint8 *s;
#else
	register guint16 *s;	/* read 1 pixels at once */
#endif
	register guint32 *o;

	guint8 *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = (guint32 *) orow;
		for (xx = 0; xx < width; xx ++) {
			register guint32 data;
			/*  rrrrrggg gggbbbbb -> rrrrr000 gggggg00 bbbbb000 aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbb000 gggggg00 rrrrr000 */
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0xf800) >> 8 | (data & 0x7e0) << 5
				| (data & 0x1f) << 19 | 0xff000000;
#else
			data = *s++;
			*o++ = (data & 0xf800) << 16 | (data & 0x7e0) << 13
				| (data & 0x1f) <<  11 | 0xff;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 15 bits/pixel data
  no alpha
  data in lsb format
*/
static void
rgb555lsb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register guint32 *s;	/* read 2 pixels at once */
#else
	register guint8 *s;	/* read 2 pixels at once */
#endif
	register guint16 *o;
	guint8 *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (guint32 *) srow;
#else
		s = srow;
#endif
		o = (guint16 *) orow;
		for (xx = 1; xx < width; xx += 2) {
			register guint32 data;
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x3e0) << 6;
			*o++ = (data & 0x1f) << 3 | (data & 0x7c000000) >> 15;
			*o++ = ((data & 0x3e00000) >> 18) | (data & 0x1f0000) >> 5;
#else
			/* swap endianness first */
			data = s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
			s += 4;
			*o++ = (data & 0x7c00) << 1 | (data & 0x3e0) >> 2;
			*o++ = (data & 0x1f) << 11 | (data & 0x7c000000) >> 23;
			*o++ = ((data & 0x3e00000) >> 10) | (data & 0x1f0000) >> 13;
#endif
		}
		/* check for last remaining pixel */
		if (width & 1) {
			register guint16 data;
#ifdef LITTLE
			data = *((short *) s);
#else
			data = *((short *) s);
			data = ((data >> 8) & 0xff) | ((data & 0xff) << 8);
#endif
			((char *) o)[0] = (data & 0x7c0) >> 7;
			((char *) o)[1] = (data & 0x3e0) >> 2;
			((char *) o)[2] = (data & 0x1f) << 3;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 15 bits/pixel data
  no alpha
  data in msb format
*/
static void
rgb555msb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register guint8 *s;	/* read 2 pixels at once */
#else
	register guint32 *s;	/* read 2 pixels at once */
#endif
	register guint16 *o;
	guint8 *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = (guint16 *) orow;
		for (xx = 1; xx < width; xx += 2) {
			register guint32 data;
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1] << 8 | s[2] << 16 | s[3] << 24;
			s += 4;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x3e0) << 6;
			*o++ = (data & 0x1f) << 3 | (data & 0x7c000000) >> 15;
			*o++ = ((data & 0x3e00000) >> 18) | (data & 0x1f0000) >> 5;
#else
			data = *s++;
			*o++ = (data & 0x7c00) << 1 | (data & 0x3e0) >> 2;
			*o++ = (data & 0x1f) << 11 | (data & 0x7c000000) >> 23;
			*o++ = ((data & 0x3e00000) >> 10) | (data & 0x1f0000) >> 13;
#endif
		}
		/* check for last remaining pixel */
		if (width & 1) {
			register guint16 data;
#ifdef LITTLE
			data = *((short *) s);
			data = ((data >> 8) & 0xff) | ((data & 0xff) << 8);
#else
			data = *((short *) s);
#endif
			((char *) o)[0] = (data & 0x7c0) >> 7;
			((char *) o)[1] = (data & 0x3e0) >> 2;
			((char *) o)[2] = (data & 0x1f) << 3;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 15 bits/pixel data
  with alpha
  data in lsb format
*/
static void
rgb555alsb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register guint16 *s;	/* read 1 pixels at once */
#else
	register guint8 *s;
#endif
	register guint32 *o;

	guint8 *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (guint16 *) srow;
#else
		s = srow;
#endif
		o = (guint32 *) orow;
		for (xx = 0; xx < width; xx++) {
			register guint32 data;
			/*  rrrrrggg gggbbbbb -> rrrrr000 gggggg00 bbbbb000 aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbb000 gggggg00 rrrrr000 */
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x3e0) << 6
				| (data & 0x1f) << 19 | 0xff000000;
#else
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0x7c00) << 17 | (data & 0x3e0) << 14
				| (data & 0x1f) <<  11 | 0xff;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  convert 15 bits/pixel data
  with alpha
  data in msb format
*/
static void
rgb555amsb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register guint16 *s;	/* read 1 pixels at once */
#else
	register guint8 *s;
#endif
	register guint32 *o;

	guint8 *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (guint16 *) srow;
#else
		s = srow;
#endif
		o = (guint32 *) orow;
		for (xx = 0; xx < width; xx++) {
			register guint32 data;
			/*  rrrrrggg gggbbbbb -> rrrrr000 gggggg00 bbbbb000 aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbb000 gggggg00 rrrrr000 */
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0x7c00) >>7 | (data & 0x3e0) << 6
				| (data & 0x1f) << 19 | 0xff000000;
#else
			data = *s++;
			*o++ = (data & 0x7c00) << 17 | (data & 0x3e0) << 14
				| (data & 0x1f) <<  11 | 0xff;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}


static void
rgb888alsb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	guint8 *s;	/* for byte order swapping */
	guint8 *o;
	guint8 *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	d (printf ("32 bits/pixel with alpha\n"));

	/* lsb data */
	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			*o++ = s[2];
			*o++ = s[1];
			*o++ = s[0];
			*o++ = 0xff;
			s += 4;
		}
		srow += bpl;
		orow += rowstride;
	}
}

static void
rgb888lsb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	guint8 *srow = image->mem, *orow = pixels;
	guint8 *o, *s;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	d (printf ("32 bit, lsb, no alpha\n"));

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			*o++ = s[2];
			*o++ = s[1];
			*o++ = s[0];
			s += 4;
		}
		srow += bpl;
		orow += rowstride;
	}
}

static void
rgb888amsb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	guint8 *srow = image->mem, *orow = pixels;
#ifdef LITTLE
	guint32 *o;
	guint32 *s;
#else
	guint8 *s;	/* for byte order swapping */
	guint8 *o;
#endif

	d (printf ("32 bit, msb, with alpha\n"));

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	/* msb data */
	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (guint32 *) srow;
		o = (guint32 *) orow;
#else
		s = srow;
		o = orow;
#endif
		for (xx = 0; xx < width; xx++) {
#ifdef LITTLE
			*o++ = s[1];
			*o++ = s[2];
			*o++ = s[3];
			*o++ = 0xff;
			s += 4;
#else
			*o++ = (*s << 8) | 0xff; /* untested */
			s++;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}

static void
rgb888msb (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	guint8 *srow = image->mem, *orow = pixels;
	guint8 *s;
	guint8 *o;

	d (printf ("32 bit, msb, no alpha\n"));

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			*o++ = s[1];
			*o++ = s[2];
			*o++ = s[3];
			s += 4;
		}
		srow += bpl;
		orow += rowstride;
	}
}

/*
  This should work correctly with any display/any endianness, but will probably
  run quite slow
*/
static void
convert_real_slow (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *cmap, int alpha)
{
	int xx, yy;
	int width, height;
	int bpl;
	guint8 *srow = image->mem, *orow = pixels;
	guint8 *s;
	guint8 *o;
	guint32 pixel;
	GdkVisual *v;

	width = image->width;
	height = image->height;
	bpl = image->bpl;
	v = gdk_colormap_get_visual(cmap);

	d(printf("rgb  mask/shift/prec = %x:%x:%x %d:%d:%d  %d:%d:%d\n",
		 v->red_mask, v->green_mask, v->blue_mask,
		 v->red_shift, v->green_shift, v->blue_shift,
		 v->red_prec, v->green_prec, v->blue_prec));

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			pixel = gdk_image_get_pixel(image, xx, yy);
			switch (v->type) {
				/* I assume this is right for static & greyscale's too? */
			case GDK_VISUAL_STATIC_GRAY:
			case GDK_VISUAL_GRAYSCALE:
			case GDK_VISUAL_STATIC_COLOR:
			case GDK_VISUAL_PSEUDO_COLOR:
				*o++ = cmap->colors[pixel].red;
				*o++ = cmap->colors[pixel].green;
				*o++ = cmap->colors[pixel].blue;
				break;
			case GDK_VISUAL_TRUE_COLOR:
				/* this is odd because it must sometimes shift left (otherwise
				   i'd just shift >> *_shift - 8 + *_prec), so this logic
				   should work for all bit sizes/shifts/etc */
				*o++ = ((pixel & v->red_mask) << (32 - v->red_shift - v->red_prec)) >> 24;
				*o++ = ((pixel & v->green_mask) << (32 - v->green_shift - v->green_prec)) >> 24;
				*o++ = ((pixel & v->blue_mask) << (32 - v->blue_shift - v->blue_prec)) >> 24;
				break;
			case GDK_VISUAL_DIRECT_COLOR:
				*o++ = cmap->colors[((pixel & v->red_mask) << (32 - v->red_shift - v->red_prec)) >> 24].red;
				*o++ = cmap->colors[((pixel & v->green_mask) << (32 - v->green_shift - v->green_prec)) >> 24].green;
				*o++ = cmap->colors[((pixel & v->blue_mask) << (32 - v->blue_shift - v->blue_prec)) >> 24].blue;
				break;
			}
			if (alpha)
				*o++ = 0xff;
		}
		srow += bpl;
		orow += rowstride;
	}
}

typedef void (* cfunc) (GdkImage *image, guchar *pixels, int rowstride, GdkColormap *cmap);

static cfunc convert_map[] = {
	rgb1,rgb1,rgb1a,rgb1a,
	rgb8,rgb8,rgb8a,rgb8a,
	rgb555lsb,rgb555msb,rgb555alsb,rgb555amsb,
	rgb565lsb,rgb565msb,rgb565alsb,rgb565amsb,
	rgb888lsb,rgb888msb,rgb888alsb,rgb888amsb
};

/*
  perform actual conversion

  If we can, try and use the optimised code versions, but as a default
  fallback, and always for direct colour, use the generic/slow but complete
  conversion function.
*/
static void
rgbconvert (GdkImage *image, guchar *pixels, int rowstride, int alpha, GdkColormap *cmap)
{
	int index = (image->byte_order == GDK_MSB_FIRST) | (alpha != 0) << 1;
	int bank=5;		/* default fallback converter */
	GdkVisual *v = gdk_colormap_get_visual(cmap);

	d(printf("masks = %x:%x:%x\n", v->red_mask, v->green_mask, v->blue_mask));
	d(printf("image depth = %d, bpp = %d\n", image->depth, image->bpp));

	switch (v->type) {
				/* I assume this is right for static & greyscale's too? */
	case GDK_VISUAL_STATIC_GRAY:
	case GDK_VISUAL_GRAYSCALE:
	case GDK_VISUAL_STATIC_COLOR:
	case GDK_VISUAL_PSEUDO_COLOR:
		switch (image->bpp) {
		case 1:
			bank = 0;
			break;
		case 8:
			bank = 1;
			break;
		}
		break;
	case GDK_VISUAL_TRUE_COLOR:
		switch (image->depth) {
		case 15:
			if (v->red_mask == 0x7c00 && v->green_mask == 0x3e0 && v->blue_mask == 0x1f
			    && image->bpp == 16)
				bank = 2;
			break;
		case 16:
			if (v->red_mask == 0xf800 && v->green_mask == 0x7e0 && v->blue_mask == 0x1f
			    && image->bpp == 16)
				bank = 3;
			break;
		case 24:
		case 32:
			if (v->red_mask == 0xff0000 && v->green_mask == 0xff00 && v->blue_mask == 0xff
			    && image->bpp == 32)
				bank = 4;
			break;
		}
		break;
	case GDK_VISUAL_DIRECT_COLOR:
		/* always use the slow version */
		break;
	}

	d(printf("converting using conversion function in bank %d\n", bank));

	if (bank==5) {
		convert_real_slow(image, pixels, rowstride, cmap, alpha);
	} else {
		index |= bank << 2;
		(* convert_map[index]) (image, pixels, rowstride, cmap);
	}
}


/* Exported functions */

/**
 * gdk_pixbuf_get_from_drawable:
 * @dest: Destination pixbuf, or NULL if a new pixbuf should be created.
 * @src: Source drawable.
 * @cmap: A colormap if @src is a pixmap.  If it is a window, this argument will
 * be ignored.
 * @src_x: Source X coordinate within drawable.
 * @src_y: Source Y coordinate within drawable.
 * @dest_x: Destination X coordinate in pixbuf, or 0 if @dest is NULL.
 * @dest_y: Destination Y coordinate in pixbuf, or 0 if @dest is NULL.
 * @width: Width in pixels of region to get.
 * @height: Height in pixels of region to get.
 *
 * Transfers image data from a Gdk drawable and converts it to an RGB(A)
 * representation inside a GdkPixbuf.
 *
 * If the drawable @src is a pixmap, then a suitable colormap must be specified,
 * since pixmaps are just blocks of pixel data without an associated colormap.
 * If the drawable is a window, the @cmap argument will be ignored and the
 * window's own colormap will be used instead.
 *
 * If the specified destination pixbuf @dest is #NULL, then this function will
 * create an RGB pixbuf with 8 bits per channel and no alpha, with the same size
 * specified by the @width and @height arguments.  In this case, the @dest_x and
 * @dest_y arguments must be specified as 0, otherwise the function will return
 * #NULL.  If the specified destination pixbuf is not NULL and it contains alpha
 * information, then the filled pixels will be set to full opacity.
 *
 * If the specified drawable is a pixmap, then the requested source rectangle
 * must be completely contained within the pixmap, otherwise the function will
 * return #NULL.
 *
 * If the specified drawable is a window, then it must be viewable, i.e. all of
 * its ancestors up to the root window must be mapped.  Also, the specified
 * source rectangle must be completely contained within the window and within
 * the screen.  If regions of the window are obscured by noninferior windows, the
 * contents of those regions are undefined.  The contents of regions obscured by
 * inferior windows of a different depth than that of the source window will also
 * be undefined.
 *
 * Return value: The same pixbuf as @dest if it was non-NULL, or a newly-created
 * pixbuf with a reference count of 1 if no destination pixbuf was specified; in
 * the latter case, NULL will be returned if not enough memory could be
 * allocated for the pixbuf to be created.
 **/
GdkPixbuf *
gdk_pixbuf_get_from_drawable (GdkPixbuf *dest,
			      GdkDrawable *src, GdkColormap *cmap,
			      int src_x, int src_y,
			      int dest_x, int dest_y,
			      int width, int height)
{
	GdkWindowType window_type;
	int src_width, src_height;
	GdkImage *image;
	int rowstride, bpp, alpha;

	/* General sanity checks */

	g_return_val_if_fail (src != NULL, NULL);

	window_type = gdk_window_get_type (src);

	if (window_type == GDK_WINDOW_PIXMAP)
		g_return_val_if_fail (cmap != NULL, NULL);
	else
		/* FIXME: this is not perfect, since is_viewable() only tests
		 * recursively up the Gdk parent window tree, but stops at
		 * foreign windows or Gdk toplevels.  I.e. if a window manager
		 * unmapped one of its own windows, this won't work.
		 */
		g_return_val_if_fail (gdk_window_is_viewable (src), NULL);

	if (!dest)
		g_return_val_if_fail (dest_x == 0 && dest_y == 0, NULL);
	else {
		g_return_val_if_fail (dest->colorspace == GDK_COLORSPACE_RGB, NULL);
		g_return_val_if_fail (dest->n_channels == 3 || dest->n_channels == 4, NULL);
		g_return_val_if_fail (dest->bits_per_sample == 8, NULL);
	}

	/* Coordinate sanity checks */

	gdk_window_get_size (src, &src_width, &src_height);

	g_return_val_if_fail (src_x >= 0 && src_y >= 0, NULL);
	g_return_val_if_fail (src_x + width <= src_width && src_y + height <= src_height, NULL);

	if (dest) {
		g_return_val_if_fail (dest_x >= 0 && dest_y >= 0, NULL);
		g_return_val_if_fail (dest_x + width <= dest->width, NULL);
		g_return_val_if_fail (dest_y + height <= dest->height, NULL);
	}

	if (window_type != GDK_WINDOW_PIXMAP) {
		int ret;
		int src_xorigin, src_yorigin;
		int screen_width, screen_height;
		int screen_srcx, screen_srcy;

		ret = gdk_window_get_origin (src, &src_xorigin, &src_yorigin);
		g_return_val_if_fail (ret != FALSE, NULL);

		screen_width = gdk_screen_width ();
		screen_height = gdk_screen_height ();

		screen_srcx = src_xorigin + src_x;
		screen_srcy = src_yorigin + src_y;

		g_return_val_if_fail (screen_srcx >= 0 && screen_srcy >= 0, NULL);
		g_return_val_if_fail (screen_srcx + width <= screen_width, NULL);
		g_return_val_if_fail (screen_srcy + height <= screen_height, NULL);
	}

	/* Get Image in ZPixmap format (packed bits). */
	image = gdk_image_get (src, src_x, src_y, width, height);
	g_return_val_if_fail (image != NULL, NULL);

	/* Create the pixbuf if needed */
	if (!dest) {
		dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
		if (!dest) {
			gdk_image_destroy(image);
			return NULL;
		}
	}

	/* Get the colormap if needed */
	if (window_type != GDK_WINDOW_PIXMAP)
		cmap = gdk_window_get_colormap (src);

	alpha = dest->has_alpha;
	rowstride = dest->rowstride;
	bpp = alpha ? 4 : 3;

	/* we offset into the image data based on the position we are retrieving from */
	rgbconvert (image, dest->pixels +
		    (dest_y * rowstride) + (dest_x * bpp),
		    rowstride,
		    alpha,
		    cmap);

	gdk_image_destroy(image);

	return dest;
}
