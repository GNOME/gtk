/*
 * Creates an GdkPixbuf from a Drawable
 *
 * Authors:
 *   Cody Russell <bratsche@dfw.net>
 *   Michael Zucchi  <zucchi@zedzone.mmc.com.au>
 *
 * This is licensed software, see the file COPYING for
 * details.
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include "gdk-pixbuf/gdk-pixbuf.h"
#include "gdk-pixbuf/gdk-pixbuf-drawable.h"


#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define LITTLE
#endif
#define d(x)

static unsigned long mask_table[] = {
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
static void rgb1(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;
	unsigned char *s;
	register unsigned char data;
	unsigned char *o;
	unsigned char *srow = image->mem, *orow = pixels;
	GdkColormap *colormap;	

	d (printf ("1 bits/pixel\n"));

	/* convert upto 8 pixels/time */
	/* its probably not worth trying to make this run very fast, who uses
	   1 bit displays anymore? */
	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	colormap = gdk_rgb_get_cmap ();

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		
		for (xx = 0; xx < width; xx ++) {
			data = srow[xx>>3] >> (7 - (xx & 7)) & 1;
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
static void rgb1a(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;
	unsigned char *s;
	register unsigned char data;
	unsigned char *o;
	unsigned char *srow = image->mem, *orow = pixels;
	unsigned long remap[2];
	GdkColormap *colormap;	

	d (printf ("1 bits/pixel\n"));

	/* convert upto 8 pixels/time */
	/* its probably not worth trying to make this run very fast, who uses
	   1 bit displays anymore? */
	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	colormap = gdk_rgb_get_cmap ();

	for (xx=0;xx<2;xx++) {
#ifdef LITTLE
		remap[xx] = 0xff000000
			| colormap->colors[xx].blue<<16
			| colormap->colors[xx].green<<8
			| colormap->colors[xx].red;
#else
		remap[xx] = 0xff
			| colormap->colors[xx].red<<24
			| colormap->colors[xx].green<<16
			| colormap->colors[xx].red<<8;
#endif
	}

	for (yy = 0; yy < height; yy++) {
		s = srow;
		o = orow;
		
		for (xx = 0; xx < width; xx ++) {
			data = srow[xx>>3] >> (7 - (xx & 7)) & 1;
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
static void rgb8(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;
	unsigned long mask;
	register unsigned long data;
	unsigned char *srow = image->mem, *orow = pixels;
	register unsigned char *s;
	register unsigned char *o;
	GdkColormap *colormap;	
	
	width = image->width;
	height = image->height;
	bpl = image->bpl;

	d(printf("8 bit, no alpha output\n"));
	
	colormap = gdk_rgb_get_cmap ();
	mask = mask_table[image->depth];

	for (yy = 0;yy < height; yy++) {
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
static void rgb8a(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;
	unsigned long mask;
	register unsigned long data;
	GdkColormap *colormap;
	unsigned long remap[256];	
	register unsigned char *s;	/* read 2 pixels at once */
	register unsigned long *o;
	unsigned char *srow = image->mem, *orow = pixels;
	
	width = image->width;
	height = image->height;
	bpl = image->bpl;

	d(printf("8 bit, with alpha output\n"));
	
	colormap = gdk_rgb_get_cmap ();
	mask = mask_table[image->depth];
	
	for (xx=0;xx<colormap->size;xx++) {
#ifdef LITTLE
		remap[xx] = 0xff000000
			| colormap->colors[xx].blue<<16
			| colormap->colors[xx].green<<8
			| colormap->colors[xx].red;
#else
		remap[xx] = 0xff
			| colormap->colors[xx].red<<24
			| colormap->colors[xx].green<<16
			| colormap->colors[xx].red<<8;
#endif
	}
	
	for (yy = 0;yy < height; yy++) {
		s = srow;
		o = (unsigned long *)orow;
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
static void rgb565lsb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;
	
#ifdef LITTLE
	register unsigned long *s;	/* read 2 pixels at once */
#else
	register unsigned char *s;	/* read 2 pixels at once */
#endif
	register unsigned short *o;
	unsigned char *srow = image->mem, *orow = pixels;
	
	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	for (yy = 0;yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned long *)srow;
#else
		s = (unsigned char *)srow;
#endif
		o = (unsigned short *)orow;
		for (xx = 1; xx < width; xx += 2) {
			register unsigned long data;
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0xf800) >> 8 | (data & 0x7e0) << 5;
			*o++ = (data & 0x1f) << 3 | (data & 0xf8000000) >> 16;
			*o++ = ((data & 0x7e00000) >> 19) | (data & 0x1f0000) >> 5;
#else
			/* swap endianness first */
			data = s[0] | s[1]<<8 | s[2]<<16 | s[3]<<24;
			s+=4;
			*o++ = (data & 0xf800) | (data & 0x7e0) >> 3;
			*o++ = (data & 0x1f) << 11 | (data & 0xf8000000) >> 24;
			*o++ = ((data & 0x7e00000) >> 11) | (data & 0x1f0000) >> 13;
#endif
		}
		/* check for last remaining pixel */
		if (width & 1) {
			register unsigned short data;
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
static void rgb565msb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;
	
#ifdef LITTLE
	register unsigned char *s;	/* need to swap data order */
#else
	register unsigned long *s;	/* read 2 pixels at once */
#endif
	register unsigned short *o;
	unsigned char *srow = image->mem, *orow = pixels;
	
	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	for (yy = 0;yy < height; yy++) {
#ifdef LITTLE
		s = srow;
#else
		s = (unsigned long *)srow;
#endif
		o = (unsigned short *)orow;
		for (xx = 1; xx < width; xx += 2) {
			register unsigned long data;
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1]<<8 | s[2]<<16 | s[3]<<24;
			s+=4;
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
			register unsigned short data;
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
static void rgb565alsb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned short *s;	/* read 1 pixels at once */
#else
	register unsigned char *s;
#endif
	register unsigned long *o;
	
	unsigned char *srow = image->mem, *orow = pixels;
	
	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	for (yy = 0;yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned short *)srow;
#else
		s = (unsigned char *)srow;
#endif
		o = (unsigned long *)orow;
		for (xx = 0; xx < width; xx ++) {
			register unsigned long data;
			/*  rrrrrggg gggbbbbb -> rrrrr000 gggggg00 bbbbb000 aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbb000 gggggg00 rrrrr000 */
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0xf800) >>8 | (data & 0x7e0) << 5
				| (data & 0x1f) << 19 | 0xff000000;
#else
			/* swap endianness first */
			data = s[0] | s[1]<<8;
			s+=2;
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
static void rgb565amsb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned char *s;
#else
	register unsigned short *s;	/* read 1 pixels at once */
#endif
	register unsigned long *o;
	
	unsigned char *srow = image->mem, *orow = pixels;
	
	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	for (yy = 0;yy < height; yy++) {
		s = srow;
		o = (unsigned long *)orow;
		for (xx = 0; xx < width; xx ++) {
			register unsigned long data;
			/*  rrrrrggg gggbbbbb -> rrrrr000 gggggg00 bbbbb000 aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbb000 gggggg00 rrrrr000 */
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1]<<8;
			s+=2;
			*o++ = (data & 0xf800) >>8 | (data & 0x7e0) << 5
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
static void rgb555lsb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;
	
#ifdef LITTLE
	register unsigned long *s;	/* read 2 pixels at once */
#else
	register unsigned char *s;	/* read 2 pixels at once */
#endif
	register unsigned short *o;
	unsigned char *srow = image->mem, *orow = pixels;
	
	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	for (yy = 0;yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned long *)srow;
#else
		s = srow;
#endif
		o = (unsigned short *)orow;
		for (xx = 1; xx < width; xx += 2) {
			register unsigned long data;
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x3e0) << 6;
			*o++ = (data & 0x1f) << 3 | (data & 0x7c000000) >> 15;
			*o++ = ((data & 0x3e00000) >> 18) | (data & 0x1f0000) >> 5;
#else
			/* swap endianness first */
			data = s[0] | s[1]<<8 | s[2]<<16 | s[3]<<24;
			s+=4;
			*o++ = (data & 0x7c00) << 1 | (data & 0x3e0) >> 2;
			*o++ = (data & 0x1f) << 11 | (data & 0x7c000000) >> 23;
			*o++ = ((data & 0x3e00000) >> 10) | (data & 0x1f0000) >> 13;
#endif
		}
		/* check for last remaining pixel */
		if (width & 1) {
			register unsigned short data;
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
static void rgb555msb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;
	
#ifdef LITTLE
	register unsigned char *s;	/* read 2 pixels at once */
#else
	register unsigned long *s;	/* read 2 pixels at once */
#endif
	register unsigned short *o;
	unsigned char *srow = image->mem, *orow = pixels;
	
	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	for (yy = 0;yy < height; yy++) {
		s = srow;
		o = (unsigned short *)orow;
		for (xx = 1; xx < width; xx += 2) {
			register unsigned long data;
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1]<<8 | s[2]<<16 | s[3]<<24;
			s+=4;
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
			register unsigned short data;
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
static void rgb555alsb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned short *s;	/* read 1 pixels at once */
#else
	register unsigned char *s;
#endif
	register unsigned long *o;
	
	unsigned char *srow = image->mem, *orow = pixels;
	
	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	for (yy = 0;yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned short *)srow;
#else
		s = srow;
#endif
		o = (unsigned long *)orow;
		for (xx = 0; xx < width; xx++) {
			register unsigned long data;
			/*  rrrrrggg gggbbbbb -> rrrrr000 gggggg00 bbbbb000 aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbb000 gggggg00 rrrrr000 */
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0x7c00) >>7 | (data & 0x3e0) << 6
				| (data & 0x1f) << 19 | 0xff000000;
#else
			/* swap endianness first */
			data = s[0] | s[1]<<8;
			s+=2;
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
static void rgb555amsb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;

#ifdef LITTLE
	register unsigned short *s;	/* read 1 pixels at once */
#else
	register unsigned char *s;
#endif
	register unsigned long *o;
	
	unsigned char *srow = image->mem, *orow = pixels;
	
	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	for (yy = 0;yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned short *)srow;
#else
		s = srow;
#endif
		o = (unsigned long *)orow;
		for (xx = 0; xx < width; xx++) {
			register unsigned long data;
			/*  rrrrrggg gggbbbbb -> rrrrr000 gggggg00 bbbbb000 aaaaaaaa */
			/*  little endian: aaaaaaaa bbbbb000 gggggg00 rrrrr000 */
#ifdef LITTLE
			/* swap endianness first */
			data = s[0] | s[1]<<8;
			s+=2;
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


static void rgb888alsb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;

	unsigned char *s;	/* for byte order swapping */
	unsigned char *o;
	unsigned char *srow = image->mem, *orow = pixels;

	width = image->width;
	height = image->height;
	bpl = image->bpl;
	
	d(printf ("32 bits/pixel with alpha\n"));
	
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

static void rgb888lsb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;

	unsigned char *srow = image->mem, *orow = pixels;
	unsigned char *o, *s;

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	d(printf("32 bit, lsb, no alpha\n"));

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

static void rgb888amsb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;

	unsigned char *srow = image->mem, *orow = pixels;
#ifdef LITTLE
	unsigned long *o;
	unsigned long *s;
#else
	unsigned char *s;	/* for byte order swapping */
	unsigned char *o;
#endif

	d(printf("32 bit, msb, with alpha\n"));

	width = image->width;
	height = image->height;
	bpl = image->bpl;

	/* msb data */
	for (yy = 0; yy < height; yy++) {
#ifdef LITTLE
		s = (unsigned long *)srow;
		o = (unsigned long *)orow;
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
			*o++ = (*s <<8 ) | 0xff; /* untested */
			s++;
#endif
		}
		srow += bpl;
		orow += rowstride;
	}
}

static void rgb888msb(GdkImage *image, art_u8 *pixels, int rowstride)
{
	int xx, yy;
	int width, height;
	int bpl;

	unsigned char *srow = image->mem, *orow = pixels;
	unsigned char *s;
	unsigned char *o;

	d(printf("32 bit, msb, no alpha\n"));

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

typedef void (*cfunc) (GdkImage *image, art_u8 *pixels, int rowstride);

static cfunc convert_map[] = {
	rgb1,rgb1,rgb1a,rgb1a,
	rgb8,rgb8,rgb8a,rgb8a,
	rgb555lsb,rgb555msb,rgb555alsb,rgb555amsb,
	rgb565lsb,rgb565msb,rgb565alsb,rgb565amsb,
	rgb888lsb,rgb888msb,rgb888alsb,rgb888amsb
};

/*
  perform actual conversion
*/
static void rgbconvert(GdkImage *image, art_u8 *pixels, int rowstride, int alpha)
{
	int index = (image->byte_order == GDK_MSB_FIRST)
		| (alpha!=0) << 1;
	int bank=0;

	switch (image->depth) {
	case 1:	bank = 0; break;
	case 8:	bank = 1; break;
	case 15: bank = 2; break;
	case 16: bank = 3; break;
	case 24:
	case 32: bank = 4; break;
	}
	index |= bank<<2;
	convert_map[index](image, pixels, rowstride);
}

static GdkPixbuf *
gdk_pixbuf_from_drawable_core (GdkPixmap * window, gint x, gint y, gint width,
			       gint height, gint with_alpha)
{
	GdkImage *image;
	ArtPixBuf *art_pixbuf;
	art_u8 *buff;
	art_u8 *pixels;
	gint rowstride;
	gint fatness;
	gint screen_width, screen_height;
	gint window_width, window_height, window_x, window_y;
	int bpl;
	
	g_return_val_if_fail (window != NULL, NULL);
	
	/* always returns image in ZPixmap format ... */
	image = gdk_image_get (window, x, y, width, height);
	
	fatness = with_alpha ? 4 : 3;
	rowstride = width * fatness;
	
	buff = art_alloc (rowstride * height);
	pixels = buff;

#if 0
	printf ("bpp = %d\n", image->bpp);
	printf ("depth = %d\n", image->depth);
	printf ("byte order = %d\n", image->byte_order);
	printf ("bytes/line = %d\n", image->bpl);
#endif
	
	bpl = image->bpl;
	
	rgbconvert(image, pixels, rowstride, with_alpha);
	gdk_image_destroy (image);
	
	if (with_alpha)
		art_pixbuf = art_pixbuf_new_rgba (buff, width, height, rowstride);
	else
		art_pixbuf = art_pixbuf_new_rgb (buff, width, height, rowstride);
	
	return gdk_pixbuf_new_from_art_pixbuf (art_pixbuf);
}

/* Public functions */
 
GdkPixbuf *
gdk_pixbuf_rgb_from_drawable (GdkWindow * window, gint x, gint y, gint width,
			      gint height)
{
  return gdk_pixbuf_from_drawable_core (window, x, y, width, height, 0);
}

GdkPixbuf *
gdk_pixbuf_rgba_from_drawable (GdkWindow * window, gint x, gint y, gint width,
			       gint height)
{
  return gdk_pixbuf_from_drawable_core (window, x, y, width, height, 1);
}

/*
 * Local variables:
 * c-basic-offset: 8
 * End:
 */
