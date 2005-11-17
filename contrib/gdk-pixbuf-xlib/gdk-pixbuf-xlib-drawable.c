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

/* Ported to Xlib by John Harper <john@dcs.warwick.ac.uk> */


#include <config.h>
#include <stdio.h>
#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf-private.h>
#include "gdk-pixbuf-xlib-private.h"
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define LITTLE
#endif
#define d(x)



static const guint32 mask_table[] = {
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


/* color handling */

typedef struct xlib_colormap_struct xlib_colormap;
struct xlib_colormap_struct {
	int size;
	XColor *colors;
	Visual *visual;
	Colormap colormap;
};

static xlib_colormap *
xlib_get_colormap (Colormap id, Visual *visual)
{
	int i;
	xlib_colormap *xc = g_new (xlib_colormap, 1);

	xc->size = visual->map_entries;
	xc->colors = g_new (XColor, xc->size);
	xc->visual = visual;
	xc->colormap = id;

	for (i = 0; i < xc->size; i++) {
		xc->colors[i].pixel = i;
		xc->colors[i].flags = DoRed | DoGreen | DoBlue;
	}

	XQueryColors (gdk_pixbuf_dpy, xc->colormap, xc->colors, xc->size);

	return xc;
}

static void
xlib_colormap_free (xlib_colormap *xc)
{
	g_free (xc->colors);
	g_free (xc);
}

/* from gdkvisual.c */
static void
visual_decompose_mask (gulong  mask,
		       gint   *shift,
		       gint   *prec)
{
	*shift = 0;
	*prec = 0;

	while (!(mask & 0x1)) {
		(*shift)++;
		mask >>= 1;
	}

	while (mask & 0x1) {
		(*prec)++;
		mask >>= 1;
	}
}

static gboolean x_error;

static int
handle_x_error (Display *dpy, XErrorEvent *ev)
{
	x_error = TRUE;
	return 0;
}

static gboolean
drawable_is_pixmap (Drawable d)
{
	/* copied from Imlib */

	XErrorHandler errh;
	XWindowAttributes wa;
	gboolean is_pixmap;

	errh = XSetErrorHandler (handle_x_error);
	x_error = FALSE;
	XGetWindowAttributes (gdk_pixbuf_dpy, d, &wa);
	XSync (gdk_pixbuf_dpy, False);
	is_pixmap = x_error;
	XSetErrorHandler (errh);

	return is_pixmap;
}



/*
  convert 1 bits-pixel data
  no alpha
*/
static void
rgb1 (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	guint8 *s;
	register guint8 data;
	guint8 *o;
	guint8 *srow = image->data, *orow = pixels;

	d (printf ("1 bits/pixel\n"));

	/* convert upto 8 pixels/time */
	/* its probably not worth trying to make this run very fast, who uses
	   1 bit displays anymore? */
	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
rgb1a (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	guint8 *s;
	register guint8 data;
	guint8 *o;
	guint8 *srow = image->data, *orow = pixels;
	guint32 remap[2];

	d (printf ("1 bits/pixel\n"));

	/* convert upto 8 pixels/time */
	/* its probably not worth trying to make this run very fast, who uses
	   1 bit displays anymore? */
	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
rgb8 (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	guint32 mask;
	register guint32 data;
	guint8 *srow = image->data, *orow = pixels;
	register guint8 *s;
	register guint8 *o;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
rgb8a (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;
	guint32 mask;
	register guint32 data;
	guint32 remap[256];
	register guint8 *s;	/* read 2 pixels at once */
	register guint32 *o;
	guint8 *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
rgb565lsb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
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
	guint8 *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
			*o++ = (data & 0xf800) >> 8 | (data & 0xe000) >> 13
				| (data & 0x7e0) << 5 | (data & 0x600) >> 1;
			*o++ = (data & 0x1f) << 3 | (data & 0x1c) >> 2
				| (data & 0xf8000000) >> 16 | (data & 0xe0000000) >> 21;
			*o++ = (data & 0x7e00000) >> 19 | (data & 0x6000000) >> 25
				| (data & 0x1f0000) >> 5 | (data & 0x1c0000) >> 10;
#else
			/* swap endianness first */
			data = s[1] | s[0] << 8 | s[3] << 16 | s[2] << 24;
			s += 4;
			*o++ = (data & 0xf800) | (data & 0xe000) >> 5
				| (data & 0x7e0) >> 3 | (data & 0x600) >> 9;
			*o++ = (data & 0x1f) << 11 | (data & 0x1c) << 6
				| (data & 0xf8000000) >> 24 | (data & 0xe0000000) >> 29;
			*o++ = (data & 0x7e00000) >> 11 | (data & 0x6000000) >> 17
				| (data & 0x1f0000) >> 13 | (data & 0x1c0000) >> 18;
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
			((char *) o)[0] = ((data >> 8) & 0xf8) | ((data >> 13) & 0x7);
			((char *) o)[1] = ((data >> 3) & 0xfc) | ((data >> 9) & 0x3);
			((char *) o)[2] = ((data << 3) & 0xf8) | ((data >> 2) & 0x7);
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
rgb565msb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
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
	guint8 *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
			data = s[1] | s[0] << 8 | s[3] << 16 | s[2] << 24;
			s += 4;
			*o++ = (data & 0xf800) >> 8 | (data & 0xe000) >> 13
				| (data & 0x7e0) << 5 | (data & 0x600) >> 1;
			*o++ = (data & 0x1f) << 3 | (data & 0x1c) >> 2
				| (data & 0xf8000000) >> 16 | (data & 0xe0000000) >> 21;
			*o++ = (data & 0x7e00000) >> 19 | (data & 0x6000000) >> 25
				| (data & 0x1f0000) >> 5 | (data & 0x1c0000) >> 10;
#else
			data = *s++;
			*o++ = (data & 0xf800) | (data & 0xe000) >> 5
				| (data & 0x7e0) >> 3 | (data & 0x600) >> 9;
			*o++ = (data & 0x1f) << 11 | (data & 0x1c) << 6
				| (data & 0xf8000000) >> 24 | (data & 0xe0000000) >> 29;
			*o++ = (data & 0x7e00000) >> 11 | (data & 0x6000000) >> 17
				| (data & 0x1f0000) >> 13 | (data & 0x1c0000) >> 18;
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
			((char *) o)[0] = ((data >> 8) & 0xf8) | ((data >> 13) & 0x7);
			((char *) o)[1] = ((data >> 3) & 0xfc) | ((data >> 9) & 0x3);
			((char *) o)[2] = ((data << 3) & 0xf8) | ((data >> 2) & 0x7);
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
rgb565alsb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
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

	guint8 *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (guint16 *) srow;
#else
		s = (guint8 *) srow;
#endif
		o = (guint32 *) orow;
		for (xx = 0; xx < width; xx ++) {
			register guint32 data;
			/*  rrrrrggg gggbbbbb -> rrrrrRRR ggggggGG bbbbbBBB aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbbBBB ggggggGG rrrrrRRR */
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0xf800) >> 8 | (data & 0xe000) >> 13
				| (data & 0x7e0) << 5 | (data & 0x600) >> 1
				| (data & 0x1f) << 19 | (data & 0x1c) << 14
				| 0xff000000;
#else
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0xf800) << 16 | (data & 0xe000) << 11
				| (data & 0x7e0) << 13 | (data & 0x600) << 7
				| (data & 0x1f) << 11 | (data & 0x1c) << 6
				| 0xff;
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
rgb565amsb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
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

	guint8 *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = srow;
#else
		s = (guint16 *) srow;
#endif
		o = (guint32 *) orow;
		for (xx = 0; xx < width; xx ++) {
			register guint32 data;
			/*  rrrrrggg gggbbbbb -> rrrrrRRR gggggg00 bbbbbBBB aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbbBBB gggggg00 rrrrrRRR */
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0xf800) >> 8 | (data & 0xe000) >> 13
				| (data & 0x7e0) << 5 | (data & 0x600) >> 1
				| (data & 0x1f) << 19 | (data & 0x1c) << 14
				| 0xff000000;
#else
			data = *s++;
			*o++ = (data & 0xf800) << 16 | (data & 0xe000) << 11
				| (data & 0x7e0) << 13 | (data & 0x600) << 7
				| (data & 0x1f) << 11 | (data & 0x1c) << 6
				| 0xff;
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
rgb555lsb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
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
	guint8 *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
			*o++ = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12
				| (data & 0x3e0) << 6 | (data & 0x380) << 1;
			*o++ = (data & 0x1f) << 3 | (data & 0x1c) >> 2
				| (data & 0x7c000000) >> 15 | (data & 0x70000000) >> 20;
			*o++ = (data & 0x3e00000) >> 18 | (data & 0x3800000) >> 23
				| (data & 0x1f0000) >> 5 | (data & 0x1c0000) >> 10;
#else
			/* swap endianness first */
			data = s[1] | s[0] << 8 | s[3] << 16 | s[2] << 24;
			s += 4;
			*o++ = (data & 0x7c00) << 1 | (data & 0x7000) >> 4
				| (data & 0x3e0) >> 2 | (data & 0x380) >> 7;
			*o++ = (data & 0x1f) << 11 | (data & 0x1c) << 6
				| (data & 0x7c000000) >> 23 | (data & 0x70000000) >> 28;
			*o++ = (data & 0x3e00000) >> 10 | (data & 0x3800000) >> 15
				| (data & 0x1f0000) >> 13 | (data & 0x1c0000) >> 18;
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
			((char *) o)[0] = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12;
			((char *) o)[1] = (data & 0x3e0) >> 2 | (data & 0x380) >> 7;
			((char *) o)[2] = (data & 0x1f) << 3 | (data & 0x1c) >> 2;
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
rgb555msb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
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
	guint8 *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
			data = s[1] | s[0] << 8 | s[3] << 16 | s[2] << 24;
			s += 4;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12
				| (data & 0x3e0) << 6 | (data & 0x380) << 1;
			*o++ = (data & 0x1f) << 3 | (data & 0x1c) >> 2
				| (data & 0x7c000000) >> 15 | (data & 0x70000000) >> 20;
			*o++ = (data & 0x3e00000) >> 18 | (data & 0x3800000) >> 23
				| (data & 0x1f0000) >> 5 | (data & 0x1c0000) >> 10;
#else
			data = *s++;
			*o++ = (data & 0x7c00) << 1 | (data & 0x7000) >> 4
				| (data & 0x3e0) >> 2 | (data & 0x380) >> 7;
			*o++ = (data & 0x1f) << 11 | (data & 0x1c) << 6
				| (data & 0x7c000000) >> 23 | (data & 0x70000000) >> 28;
			*o++ = (data & 0x3e00000) >> 10 | (data & 0x3800000) >> 15
				| (data & 0x1f0000) >> 13 | (data & 0x1c0000) >> 18;
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
			((char *) o)[0] = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12;
			((char *) o)[1] = (data & 0x3e0) >> 2 | (data & 0x380) >> 7;
			((char *) o)[2] = (data & 0x1f) << 3 | (data & 0x1c) >> 2;
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
rgb555alsb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
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

	guint8 *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (guint16 *) srow;
#else
		s = srow;
#endif
		o = (guint32 *) orow;
		for (xx = 0; xx < width; xx++) {
			register guint32 data;
			/*  rrrrrggg gggbbbbb -> rrrrrRRR gggggGGG bbbbbBBB aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbbBBB gggggGGG rrrrrRRR */
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12
				| (data & 0x3e0) << 6 | (data & 0x380) << 1
				| (data & 0x1f) << 19 | (data & 0x1c) << 14
				| 0xff000000;
#else
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0x7c00) << 17 | (data & 0x7000) << 12
				| (data & 0x3e0) << 14 | (data & 0x380) << 9
				| (data & 0x1f) << 11 | (data & 0x1c) << 6
				| 0xff;
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
rgb555amsb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
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

	guint8 *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (guint16 *) srow;
#else
		s = srow;
#endif
		o = (guint32 *) orow;
		for (xx = 0; xx < width; xx++) {
			register guint32 data;
			/*  rrrrrggg gggbbbbb -> rrrrrRRR gggggGGG bbbbbBBB aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbbBBB gggggGGG rrrrrRRR */
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1] << 8;
			s += 2;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x7000) >> 12
				| (data & 0x3e0) << 6 | (data & 0x380) << 1
				| (data & 0x1f) << 19 | (data & 0x1c) << 14
				| 0xff000000;
#else
			data = *s++;
			*o++ = (data & 0x7c00) << 17 | (data & 0x7000) << 12
				| (data & 0x3e0) << 14 | (data & 0x380) << 9
				| (data & 0x1f) << 11 | (data & 0x1c) << 6
				| 0xff;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}


static void
rgb888alsb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	guint8 *s;	/* for byte order swapping */
	guint8 *o;
	guint8 *srow = image->data, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
rgb888lsb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	guint8 *srow = image->data, *orow = pixels;
	guint8 *o, *s;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
rgb888amsb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	guint8 *srow = image->data, *orow = pixels;
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
	bpl = image->bytes_per_line;

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
rgb888msb (XImage *image, guchar *pixels, int rowstride, xlib_colormap *colormap)
{
	int xx, yy;
	int width, height;
	int bpl;

	guint8 *srow = image->data, *orow = pixels;
	guint8 *s;
	guint8 *o;

	d (printf ("32 bit, msb, no alpha\n"));

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;

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
convert_real_slow (XImage *image, guchar *pixels, int rowstride, xlib_colormap *cmap, int alpha)
{
	int xx, yy;
	int width, height;
	int bpl;
	guint8 *srow = image->data, *orow = pixels;
	guint8 *s;
	guint8 *o;
	guint32 pixel;
	Visual *v;
	guint8 component;
	int i;
	int red_shift, red_prec, green_shift, green_prec, blue_shift, blue_prec;

	width = image->width;
	height = image->height;
	bpl = image->bytes_per_line;
	v = cmap->visual;

	visual_decompose_mask (v->red_mask, &red_shift, &red_prec);
	visual_decompose_mask (v->green_mask, &green_shift, &green_prec);
	visual_decompose_mask (v->blue_mask, &blue_shift, &blue_prec);

	d(printf("rgb  mask/shift/prec = %x:%x:%x %d:%d:%d  %d:%d:%d\n",
		 v->red_mask, v->green_mask, v->blue_mask,
		 red_shift, green_shift, blue_shift,
		 red_prec, green_prec, blue_prec));

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		for (xx = 0; xx < width; xx++) {
			pixel = XGetPixel (image, xx, yy);
			switch (v->class) {
				/* I assume this is right for static & greyscale's too? */
			case StaticGray:
			case GrayScale:
			case StaticColor:
			case PseudoColor:
				*o++ = cmap->colors[pixel].red;
				*o++ = cmap->colors[pixel].green;
				*o++ = cmap->colors[pixel].blue;
				break;
			case TrueColor:
				/* This is odd because it must sometimes shift left (otherwise
				   I'd just shift >> (*_shift - 8 + *_prec + <0-7>). This logic
				   should work for all bit sizes/shifts/etc. */
				component = 0;
				for (i = 24; i < 32; i += red_prec)
					component |= ((pixel & v->red_mask) << (32 - red_shift - red_prec)) >> i;
				*o++ = component;
				component = 0;
				for (i = 24; i < 32; i += green_prec)
					component |= ((pixel & v->green_mask) << (32 - green_shift - green_prec)) >> i;
				*o++ = component;
				component = 0;
				for (i = 24; i < 32; i += blue_prec)
					component |= ((pixel & v->blue_mask) << (32 - blue_shift - blue_prec)) >> i;
				*o++ = component;
				break;
			case DirectColor:
				*o++ = cmap->colors[((pixel & v->red_mask) << (32 - red_shift - red_prec)) >> 24].red;
				*o++ = cmap->colors[((pixel & v->green_mask) << (32 - green_shift - green_prec)) >> 24].green;
				*o++ = cmap->colors[((pixel & v->blue_mask) << (32 - blue_shift - blue_prec)) >> 24].blue;
				break;
			}
			if (alpha)
				*o++ = 0xff;
		}
		srow += bpl;
		orow += rowstride;
	}
}

typedef void (* cfunc) (XImage *image, guchar *pixels, int rowstride, xlib_colormap *cmap);

static const cfunc convert_map[] = {
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
rgbconvert (XImage *image, guchar *pixels, int rowstride, int alpha, xlib_colormap *cmap)
{
	int index = (image->byte_order == MSBFirst) | (alpha != 0) << 1;
	int bank=5;		/* default fallback converter */
	Visual *v = cmap->visual;

	d(printf("masks = %x:%x:%x\n", v->red_mask, v->green_mask, v->blue_mask));
	d(printf("image depth = %d, bpp = %d\n", image->depth, image->bits_per_pixel));

	switch (v->class) {
				/* I assume this is right for static & greyscale's too? */
	case StaticGray:
	case GrayScale:
	case StaticColor:
	case PseudoColor:
		switch (image->bits_per_pixel) {
		case 1:
			bank = 0;
			break;
		case 8:
			bank = 1;
			break;
		}
		break;
	case TrueColor:
		switch (image->depth) {
		case 15:
			if (v->red_mask == 0x7c00 && v->green_mask == 0x3e0 && v->blue_mask == 0x1f
			    && image->bits_per_pixel == 16)
				bank = 2;
			break;
		case 16:
			if (v->red_mask == 0xf800 && v->green_mask == 0x7e0 && v->blue_mask == 0x1f
			    && image->bits_per_pixel == 16)
				bank = 3;
			break;
		case 24:
		case 32:
			if (v->red_mask == 0xff0000 && v->green_mask == 0xff00 && v->blue_mask == 0xff
			    && image->bits_per_pixel == 32)
				bank = 4;
			break;
		}
		break;
	case DirectColor:
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

static gboolean
xlib_window_is_viewable (Window w)
{
	XWindowAttributes wa;

	while (w != 0) {
		Window parent, root, *children;
		int nchildren;

		XGetWindowAttributes (gdk_pixbuf_dpy, w, &wa);
		if (wa.map_state != IsViewable)
			return FALSE;

		if (!XQueryTree (gdk_pixbuf_dpy, w, &root,
				 &parent, &children, &nchildren))
			return FALSE;

		if (nchildren > 0)
			XFree (children);

		if ((parent == root) || (w == root))
			return TRUE;

		w = parent;
	}

	return FALSE;
}

static gint
xlib_window_get_origin (Window w, gint *x, gint *y)
{
	Window child;
	return XTranslateCoordinates (gdk_pixbuf_dpy, w,
				      RootWindow (gdk_pixbuf_dpy,
						  gdk_pixbuf_screen),
				      0, 0, x, y, &child);
}

/* Exported functions */

/**
 * gdk_pixbuf_xlib_get_from_drawable:
 * @dest: Destination pixbuf, or NULL if a new pixbuf should be created.
 * @src: Source drawable.
 * @cmap: A colormap if @src is a pixmap.  If it is a window, this argument will
 * be ignored.
 * @visual: A visual if @src is a pixmap.  If it is a window, this argument will
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
gdk_pixbuf_xlib_get_from_drawable (GdkPixbuf *dest,
				   Drawable src,
				   Colormap cmap, Visual *visual,
				   int src_x, int src_y,
				   int dest_x, int dest_y,
				   int width, int height)
{
	int src_width, src_height;
	XImage *image;
	int rowstride, bpp, alpha;
	XWindowAttributes wa;
	xlib_colormap *x_cmap;
	gboolean is_pixmap;

	/* General sanity checks */

	g_return_val_if_fail (src != 0, NULL);

	is_pixmap = drawable_is_pixmap (src);

	if (is_pixmap) {
		g_return_val_if_fail (cmap != 0, NULL);
		g_return_val_if_fail (visual != NULL, NULL);
	}
	else
		g_return_val_if_fail (xlib_window_is_viewable (src), NULL);

	if (!dest)
		g_return_val_if_fail (dest_x == 0 && dest_y == 0, NULL);
	else {
		g_return_val_if_fail (dest->colorspace == GDK_COLORSPACE_RGB, NULL);
		g_return_val_if_fail (dest->n_channels == 3
				      || dest->n_channels == 4, NULL);
		g_return_val_if_fail (dest->bits_per_sample == 8, NULL);
	}

	/* Coordinate sanity checks */

	if (!is_pixmap) {
	    XGetWindowAttributes (gdk_pixbuf_dpy, src, &wa);
	    src_width = wa.width;
	    src_height = wa.height;
	} else {
	    Window root;
	    int tx, ty, bwidth, depth;
	    XGetGeometry (gdk_pixbuf_dpy, src, &root, &tx, &ty,
			  &src_width, &src_height, &bwidth, &depth);
	}

	g_return_val_if_fail (src_x >= 0 && src_y >= 0, NULL);
	g_return_val_if_fail (src_x + width <= src_width
			      && src_y + height <= src_height, NULL);

	if (dest) {
		g_return_val_if_fail (dest_x >= 0 && dest_y >= 0, NULL);
		g_return_val_if_fail (dest_x + width <= dest->width, NULL);
		g_return_val_if_fail (dest_y + height <= dest->height, NULL);
	}

	if (!is_pixmap) {
		int ret;
		int src_xorigin, src_yorigin;
		int screen_width, screen_height;
		int screen_srcx, screen_srcy;

		ret = xlib_window_get_origin (src, &src_xorigin, &src_yorigin);
		g_return_val_if_fail (ret != FALSE, NULL);

		screen_width = DisplayWidth (gdk_pixbuf_dpy, gdk_pixbuf_screen);
		screen_height = DisplayHeight (gdk_pixbuf_dpy, gdk_pixbuf_screen);

		screen_srcx = src_xorigin + src_x;
		screen_srcy = src_yorigin + src_y;

		g_return_val_if_fail (screen_srcx >= 0 && screen_srcy >= 0, NULL);
		g_return_val_if_fail (screen_srcx + width <= screen_width, NULL);
		g_return_val_if_fail (screen_srcy + height <= screen_height, NULL);
	}

	/* Get Image in ZPixmap format (packed bits). */
	image = XGetImage (gdk_pixbuf_dpy, src, src_x, src_y,
			   width, height, AllPlanes, ZPixmap);
	g_return_val_if_fail (image != NULL, NULL);

	/* Create the pixbuf if needed */
	if (!dest) {
		dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB,
				       FALSE, 8, width, height);
		if (!dest) {
			XDestroyImage (image);
			return NULL;
		}
	}

	/* Get the colormap if needed */
	if (!is_pixmap)
	{
		cmap = wa.colormap;
		visual = wa.visual;
	}

	x_cmap = xlib_get_colormap (cmap, visual);

	alpha = dest->has_alpha;
	rowstride = dest->rowstride;
	bpp = alpha ? 4 : 3;

	/* we offset into the image data based on the position we are retrieving from */
	rgbconvert (image, dest->pixels +
		    (dest_y * rowstride) + (dest_x * bpp),
		    rowstride,
		    alpha,
		    x_cmap);

	xlib_colormap_free (x_cmap);
	XDestroyImage (image);

	return dest;
}
