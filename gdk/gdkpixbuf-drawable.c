/*
 * Creates an GdkPixbuf from a Drawable
 *
 * Authors:
 *   Cody Russell <bratsche@dfw.net>
 *   Michael Zucchi  <zucchi@zedzone.mmc.com.au>
 */

#include <config.h>
#include <stdio.h>
#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include "gdk-pixbuf.h"
#include "gdk-pixbuf-drawable.h"


#if (G_BYTE_ORDER == G_LITTLE_ENDIAN)
#define LITTLE
#endif
#define d(x)

unsigned long mask_table[] = {
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

static GdkPixbuf *
gdk_pixbuf_from_drawable_core (GdkPixmap * window, gint x, gint y, gint width,
			       gint height, gint with_alpha)
{
  GdkImage *image;
  ArtPixBuf *art_pixbuf;
  GdkColormap *colormap;
  art_u8 *buff;
  art_u8 *pixels;
  gulong pixel;
  gint rowstride;
  art_u8 r, g, b;
  gint xx, yy;
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

  switch (image->bpp)
    {
    case 1:
      {
	unsigned char *s;
	register unsigned char bits;
	register unsigned char data;
	unsigned char *o;
	unsigned char *srow = image->mem, *orow = pixels;
	int i, base;

	d (printf ("1 bits/pixel\n"));

	/* convert upto 8 pixels/time */
	colormap = gdk_rgb_get_cmap ();
	for (yy = 0; yy < height; yy++)
	  {
	    s = srow;
	    o = orow;

	    for (xx = 0; xx < width; xx += 8)
	      {
		bits = *s++;

		if (xx + 8 >= width)
		  base = 7 - (xx + 8 - width);
		else
		  base = 0;

		for (i = 7; i >= base; i--)
		  {
		    data = (bits & (0x80 >> i)) & 1;
		    *o++ = colormap->colors[data].red;
		    *o++ = colormap->colors[data].green;
		    *o++ = colormap->colors[data].blue;
		  }
	      }

	    srow += bpl;
	    orow += rowstride;
	  }
	break;
      }

    case 8:
      {
	unsigned char *s;
	unsigned long mask;
	register unsigned long data;
	unsigned char *o;
	unsigned char *srow = image->mem, *orow = pixels;

	d (printf ("8 bits/pixel\n"));

	colormap = gdk_rgb_get_cmap ();
	mask = mask_table[image->depth];

	for (yy = 0; yy < height; yy++)
	  {
	    s = srow;
	    o = orow;
	    for (xx = 0; xx < width; xx++)
	      {
		data = *s++ & mask;
		*o++ = colormap->colors[data].red;
		*o++ = colormap->colors[data].green;
		*o++ = colormap->colors[data].blue;
	      }
	    srow += bpl;
	    orow += rowstride;
	  }
	break;
      }

      /*#define SIMPLE_16  this is really quite slow ! */
    case 16:
      {
#ifdef SIMPLE_16
	unsigned short *s;
#else
	unsigned long *s;	/* read 2 pixels at once */
#endif
	register unsigned long data;
	unsigned short *o;
	unsigned char *srow = image->mem, *orow = pixels;

	printf ("16 bits/pixel\n");

	for (yy = 0; yy < height; yy++)
	  {
	    s = srow;
	    o = orow;

	    if (image->byte_order == GDK_LSB_FIRST)
	      {
#ifdef SIMPLE_16
		for (xx = 0; xx < width; xx++)
		  {
		    data = *s++;
		    *o++ = (data >> 8) & 0xf8;
		    *o++ = (data >> 3) & 0xfc;
		    *o++ = (data << 3) & 0xf8;
		  }
#else
		switch (image->depth)
		  {
		  case 15:
		    /* starts at 1, so we dont over-flow the conversion */
		    for (xx = 0; xx < width; xx += 2)
		      {
			/* read/convert 2 pixels at a time */

			/* little endian, lsb data */
#ifdef LITTLE
			/*7c 3e 1f */
			data = *s++;
			*o++ = (data & 0x7c00) >> 7 | (data & 0x3e0) << 6;
			*o++ = (data & 0x1f) << 3 | (data & 0x7c000000) >> 15;
			*o++ =
			  ((data & 0x3e00000) >> 18) | (data & 0x1f0000) >> 5;
#else
			/* big endian, lsb data */
			/* swap endianness first */
			data = (unsigned long) ((unsigned char *) s)[3] << 24
			  | (unsigned long) ((unsigned char *) s)[2] << 16
			  | (unsigned long) ((unsigned char *) s)[1] << 8
			  | (unsigned long) ((unsigned char *) s)[0];
			s++;
			*o++ = (data & 0x7c00) << 1 | (data & 0x3e0) >> 2;
			*o++ =
			  (data & 0x1f) << 11 | (data & 0x7c000000) >> 23;
			*o++ =
			  ((data & 0x3e00000) >> 10) | (data & 0x1f0000) >>
			  13;
#endif
		      }
		    /* check for last remaining pixel */
		    if (width & 1)
		      {
			register unsigned short data;
#ifdef LITTLE
			data = *((short *) s);
#else
			data = *((short *) s);
			data = ((data >> 8) & 0xff) | ((data & 0xff) << 8);
			data &= mask;
#endif
			((char *) o)[0] = (data & 0x7c0) >> 7;
			((char *) o)[1] = (data & 0x3e0) >> 2;
			((char *) o)[2] = (data & 0x1f) << 3;
		      }
		    break;
		  case 16:
		    for (xx = 0; xx < width; xx += 2)
		      {
			/* read/convert 2 pixels at a time */

			/* little endian, lsb data */
#ifdef LITTLE
			data = *s++;
			*o++ = (data & 0xf800) >> 8 | (data & 0x7e0) << 5;
			*o++ = (data & 0x1f) << 3 | (data & 0xf8000000) >> 16;
			*o++ =
			  ((data & 0x7e00000) >> 19) | (data & 0x1f0000) >> 5;
#else
			/* big endian, lsb data */
			/* swap endianness first */
			data = (unsigned long) ((unsigned char *) s)[3] << 24
			  | (unsigned long) ((unsigned char *) s)[2] << 16
			  | (unsigned long) ((unsigned char *) s)[1] << 8
			  | (unsigned long) ((unsigned char *) s)[0];
			s++;
			*o++ = (data & 0xf800) | (data & 0x7e0) >> 3;
			*o++ =
			  (data & 0x1f) << 11 | (data & 0xf8000000) >> 24;
			*o++ =
			  ((data & 0x7e00000) >> 11) | (data & 0x1f0000) >>
			  13;
#endif
		      }
		    /* check for last remaining pixel */
		    if (width & 1)
		      {
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
		    break;
		  }
#endif /* SIMPLE_16 */
	      }
	    else
	      {
		switch (image->depth)
		  {
		  case 15:
		    for (xx = 0; xx < width; xx += 2)
		      {
			/* read/convert 2 pixels at a time */
#ifdef LITTLE
			/* little endian, msb data */
			/* swap endianness first */
			data = (unsigned long) ((unsigned char *) s)[0] << 24
			  | (unsigned long) ((unsigned char *) s)[1] << 16
			  | (unsigned long) ((unsigned char *) s)[2] << 8
			  | (unsigned long) ((unsigned char *) s)[3];
			s++;
			/*7c 3e 1f */
			*o++ = (data & 0x7c00) >> 7 | (data & 0x3e0) << 6;
			*o++ = (data & 0x1f) << 3 | (data & 0x7c000000) >> 15;
			*o++ =
			  ((data & 0x3e00000) >> 18) | (data & 0x1f0000) >> 5;
#else
			/* big endian, lsb data */
			data = *s++;
			*o++ = (data & 0x7c00) << 1 | (data & 0x3e0) >> 2;
			*o++ =
			  (data & 0x1f) << 11 | (data & 0x7c000000) >> 23;
			*o++ =
			  ((data & 0x3e00000) >> 10) | (data & 0x1f0000) >>
			  13;
#endif
		      }
		    /* check for last remaining pixel */
		    if (width & 1)
		      {
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
		    break;
		  case 16:
		    for (xx = 0; xx < width; xx += 2)
		      {
			/* read/convert 2 pixels at a time */
#ifdef LITTLE
			/* little endian, msb data */
			/* swap endianness first */
			data = (unsigned long) ((unsigned char *) s)[0] << 24
			  | (unsigned long) ((unsigned char *) s)[1] << 16
			  | (unsigned long) ((unsigned char *) s)[2] << 8
			  | (unsigned long) ((unsigned char *) s)[3];
			s++;
			*o++ = (data & 0xf800) >> 8 | (data & 0x7e0) << 5;
			*o++ = (data & 0x1f) << 3 | (data & 0xf8000000) >> 16;
			*o++ =
			  ((data & 0x7e00000) >> 19) | (data & 0x1f0000) >> 5;
#else
			/* big endian, lsb data */
			data = *s++;
			*o++ = (data & 0xf800) | (data & 0x7e0) >> 3;
			*o++ =
			  (data & 0x1f) << 11 | (data & 0xf8000000) >> 24;
			*o++ =
			  ((data & 0x7e00000) >> 11) | (data & 0x1f0000) >>
			  13;
#endif
		      }
		    /* check for last remaining pixel */
		    if (width & 1)
		      {
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
		    break;
		  }
	      }
	    srow += bpl;
	    orow += rowstride;
	  }
	break;
      }
      /*#define HAIRY_32  this is slowest implementation */
#define SIMPLE_32		/* this is fastest implementation */
    case 32:
      {
#ifdef SIMPLE_32
	unsigned char *s;	/* read 1 pixels at once */
#else
	unsigned long *s;	/* read 1 pixels at once */
#endif
#ifdef HAIRY_32
	register unsigned long data2;
	unsigned short *o;
#else
	unsigned char *o;
#endif
	register unsigned long data;
	unsigned char *srow = image->mem, *orow = pixels;

	printf ("32 bits/pixel\n");

	for (yy = 0; yy < height; yy++)
	  {
	    s = srow;
	    o = orow;

	    if (image->byte_order == GDK_LSB_FIRST)
	      {
#ifdef HAIRY_32
		for (xx = 0; xx < width; xx += 2)
		  {
#else
		for (xx = 0; xx < width; xx++)
		  {
#endif
		    /* read/convert 2 pixels at a time */

		    /* little endian, lsb data */
#ifdef SIMPLE_32
		    *o++ = s[2];
		    *o++ = s[1];
		    *o++ = s[0];
		    s += 4;
#else

#ifdef LITTLE
#ifdef HAIRY_32
		    data = *s++;
		    data2 = *s++;
#else
		    data = *s++;
#endif
#else
		    /* big endian, lsb data */
		    /* swap endianness first */
		    data = (unsigned long) ((unsigned char *) s)[3] << 24
		      | (unsigned long) ((unsigned char *) s)[2] << 16
		      | (unsigned long) ((unsigned char *) s)[1] << 8
		      | (unsigned long) ((unsigned char *) s)[0];
		    s++;
#endif
#ifdef HAIRY_32
		    *o++ = (data & 0xff0000) >> 16 | (data & 0xff00);
		    *o++ = (data & 0xff) | (data2 & 0xff0000) >> 8;
		    *o++ = ((data2 & 0xff00) >> 8) | (data2 & 0xff) << 8;
#else
		    /* FIXME: endianness conversion can be done here .. doh! */
		    *o++ = data & 0xff0000 >> 16;
		    *o++ = data & 0x00ff00 >> 8;
		    *o++ = data & 0x0000ff;
#endif

#endif /* SIMPLE_32 */
		  }
	      }
	    else
	      {
		for (xx = 0; xx < width; xx++)
		  {
		    /* read/convert 2 pixels at a time */
#ifdef LITTLE
		    /* little endian, msb data */
		    /* swap endianness first */
#if 1				/* which is faster?? */
		    data = (unsigned long) ((unsigned char *) s)[0] << 24
		      | (unsigned long) ((unsigned char *) s)[1] << 16
		      | (unsigned long) ((unsigned char *) s)[2] << 8
		      | (unsigned long) ((unsigned char *) s)[3];
		    s++;
#else /* this is probably slower */
		    data = *s++;
		    data = ((data >> 16) & 0xffff) | ((data & 0xffff) << 16);
		    data =
		      ((data & 0xff00ff00) >> 8) | ((data & 0xff00ff) << 8);
#endif
#else
		    /* big endian, lsb data */
		    data = *s++;
#endif
		    /* FIXME: endianness conversion can be done here .. doh! */
		    *o++ = data & 0xff0000 >> 16;
		    *o++ = data & 0x00ff00 >> 8;
		    *o++ = data & 0x0000ff;
		  }
	      }
	    srow += bpl;
	    orow += rowstride;
	  }
	break;
      }
    default:
      g_warning ("gdk_pixbuf_from_drawable: Unsupported image format\n");
    }

  gdk_image_destroy (image);

  art_pixbuf =
    with_alpha ? art_pixbuf_new_rgba (buff, width, height,
				      rowstride) : art_pixbuf_new_rgb (buff,
								       width,
								       height,
								       rowstride);

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
