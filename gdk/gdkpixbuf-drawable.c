/* GdkPixbuf library - convert X drawable information to RGB
 *
 * Copyright (C) 1999 Michael Zucchi
 *
 * Authors: Michael Zucchi <zucchi@zedzone.mmc.com.au>
 *          Cody Russell <bratsche@dfw.net>
 * 	    Federico Mena-Quintero <federico@gimp.org>
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
#include <string.h>
#include "gdk.h"		/* For gdk_screen_width/gdk_screen_height */
#include "gdkcolor.h"
#include "gdkimage.h"
#include "gdkvisual.h"
#include "gdkwindow.h"
#include "gdkpixbuf.h"
#include "gdk-pixbuf-private.h"
#include "gdkinternals.h"

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
 * convert bitmap data to pixbuf without alpha,
 * without using a colormap 
 */
static void
bitmap1 (GdkImage    *image,
         guchar      *pixels,
         int          rowstride,
         int          x1,
         int          y1,
         int          x2,
         int          y2)
{
  int xx, yy;
  int bpl;
  register guint8 data;
  guint8 *o;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl, *orow = pixels;

  d (printf ("bitmap, no alpha\n"));

  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
      o = orow;
      
      for (xx = x1; xx < x2; xx ++)
	{
          /* top 29 bits of xx (xx >> 3) indicate the byte the bit is inside,
           * bottom 3 bits (xx & 7) indicate bit inside that byte,
           * we don't bother to canonicalize data to 1 or 0, just
           * leave the relevant bit in-place.
           */
          data = srow[xx >> 3] & (image->byte_order == GDK_MSB_FIRST ?
                                  (0x80 >> (xx & 7)) :
                                  (1 << (xx & 7)));

          if (data)
            {
              *o++ = 255;
              *o++ = 255;
              *o++ = 255;
            }
          else
            {
              *o++ = 0;
              *o++ = 0;
              *o++ = 0;
            }
	}
      srow += bpl;
      orow += rowstride;
    }
}

/*
 * convert bitmap data to pixbuf with alpha,
 * without using a colormap 
 */
static void
bitmap1a (GdkImage    *image,
          guchar      *pixels,
          int          rowstride,
          int          x1,
          int          y1,
          int          x2,
          int          y2)
{
  int xx, yy;
  int bpl;
  register guint8 data;
  guint8 *o;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl, *orow = pixels;

  d (printf ("bitmap, with alpha\n"));

  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
      o = orow;
      
      for (xx = x1; xx < x2; xx ++)
	{
          /* see comment in bitmap1() */
          data = srow[xx >> 3] & (image->byte_order == GDK_MSB_FIRST ?
                                  (0x80 >> (xx & 7)) :
                                  (1 << (xx & 7)));

          if (data)
            {
              *o++ = 255;
              *o++ = 255;
              *o++ = 255;
              *o++ = 255;
            }
          else
            {
              *o++ = 0;
              *o++ = 0;
              *o++ = 0;
              *o++ = 0;
            }
	}
      srow += bpl;
      orow += rowstride;
    }
}

/*
 * convert 1 bits-pixel data
 * no alpha
 */
static void
rgb1 (GdkImage    *image,
      guchar      *pixels,
      int          rowstride,
      int          x1,
      int          y1,
      int          x2,
      int          y2,
      GdkColormap *colormap)
{
  int xx, yy;
  int bpl;
  register guint8 data;
  guint8 *o;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl, *orow = pixels;

  d (printf ("1 bits/pixel\n"));

  /* convert upto 8 pixels/time */
  /* its probably not worth trying to make this run very fast, who uses
   * 1 bit displays anymore?
   */
  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
      o = orow;
      
      for (xx = x1; xx < x2; xx ++)
	{
          /* see comment in bitmap1() */
          data = srow[xx >> 3] & (image->byte_order == GDK_MSB_FIRST ?
                                  (0x80 >> (xx & 7)) :
                                  (1 << (xx & 7)));

	  *o++ = colormap->colors[data].red;
	  *o++ = colormap->colors[data].green;
	  *o++ = colormap->colors[data].blue;
	}
      srow += bpl;
      orow += rowstride;
    }
}

/*
 * convert 1 bits/pixel data
 * with alpha
 */
static void
rgb1a (GdkImage    *image,
       guchar      *pixels,
       int          rowstride,
       int          x1,
       int          y1,
       int          x2,
       int          y2,
       GdkColormap *colormap)
{
  int xx, yy;
  int bpl;
  register guint8 data;
  guint8 *o;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl, *orow = pixels;
  
  d (printf ("1 bits/pixel\n"));

  /* convert upto 8 pixels/time */
  /* its probably not worth trying to make this run very fast, who uses
   * 1 bit displays anymore? */
  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
      o = orow;
      
      for (xx = x1; xx < x2; xx ++)
	{
          /* see comment in bitmap1() */
          data = srow[xx >> 3] & (image->byte_order == GDK_MSB_FIRST ?
                                  (0x80 >> (xx & 7)) :
                                  (1 << (xx & 7)));

          *o++ = colormap->colors[data].red;
	  *o++ = colormap->colors[data].green;
	  *o++ = colormap->colors[data].blue;
	  *o++ = 255;
	}
      srow += bpl;
      orow += rowstride;
    }
}

/*
 * convert 8 bits/pixel data
 * no alpha
 */
static void
rgb8 (GdkImage    *image,
      guchar      *pixels,
      int          rowstride,
      int          x1,
      int          y1,
      int          x2,
      int          y2,
      GdkColormap *colormap)
{
  int xx, yy;
  int bpl;
  guint32 mask;
  register guint32 data;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;
  register guint8 *s;
  register guint8 *o;

  bpl = image->bpl;

  d (printf ("8 bit, no alpha output\n"));

  mask = mask_table[image->depth];

  for (yy = y1; yy < y2; yy++)
    {
      s = srow;
      o = orow;
      for (xx = x1; xx < x2; xx++)
        {
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
 * convert 8 bits/pixel data
 * with alpha
 */
static void
rgb8a (GdkImage    *image,
       guchar      *pixels,
       int          rowstride,
       int          x1,
       int          y1,
       int          x2,
       int          y2,
       GdkColormap *colormap)
{
  int xx, yy;
  int bpl;
  guint32 mask;
  register guint32 data;
  guint32 remap[256];
  register guint8 *s;	/* read 2 pixels at once */
  register guint32 *o;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;

  bpl = image->bpl;

  d (printf ("8 bit, with alpha output\n"));

  mask = mask_table[image->depth];

  for (xx = x1; xx < colormap->size; xx++)
    {
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

  for (yy = y1; yy < y2; yy++)
    {
      s = srow;
      o = (guint32 *) orow;
      for (xx = x1; xx < x2; xx ++)
	{
	  data = *s++ & mask;
	  *o++ = remap[data];
	}
      srow += bpl;
      orow += rowstride;
    }
}

/*
 * convert 16 bits/pixel data
 * no alpha
 * data in lsb format
 */
static void
rgb565lsb (GdkImage    *image,
	   guchar      *pixels,
	   int          rowstride,
           int          x1,
           int          y1,
           int          x2,
           int          y2,
	   GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

#ifdef LITTLE
  register guint32 *s;	/* read 2 pixels at once */
#else
  register guint8 *s;	/* read 2 pixels at once */
#endif
  register guint16 *o;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;
  
  bpl = image->bpl;
  
  for (yy = y1; yy < y2; yy++)
    {
#ifdef LITTLE
      s = (guint32 *) srow;
#else
      s = srow;
#endif
      o = (guint16 *) orow;

      /* check for first pixel odd */
      xx = x1;
      if (xx & 1)
	{
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
	  o = (guint16 *) (((char *) o) + 3);
          ++xx;
	}

      g_assert (!(xx & 1));

      /* if x2 is even, then the -1 does nothing to number of
       * loop iterations, if x2 is odd then the -1 reduces
       * iterations by one
       */
      for (; xx < (x2 - 1); xx += 2)
	{
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
      if (x2 & 1)
	{
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
 * convert 16 bits/pixel data
 * no alpha
 * data in msb format
 */
static void
rgb565msb (GdkImage    *image,
	   guchar      *pixels,
	   int          rowstride,
           int          x1,
           int          y1,
           int          x2,
           int          y2,
	   GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

#ifdef LITTLE
  register guint8 *s;	/* need to swap data order */
#else
  register guint32 *s;	/* read 2 pixels at once */
#endif
  register guint16 *o;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;

  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
#ifdef LITTLE
      s = srow;
#else
      s = (guint32 *) srow;
#endif
      o = (guint16 *) orow;

      xx = x1;
      
      /* check for first pixel odd */
      if (xx & 1)
	{
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

	  o = (guint16 *) (((char *) o) + 3);
          ++xx;
	}

      g_assert (!(xx & 1));

      /* if x2 is even, then the -1 does nothing to number of
       * loop iterations, if x2 is odd then the -1 reduces
       * iterations by one
       */
      for (; xx < (x2 - 1); xx += 2)
	{
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
      if (x2 & 1)
	{
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
 * convert 16 bits/pixel data
 * with alpha
 * data in lsb format
 */
static void
rgb565alsb (GdkImage    *image,
	    guchar      *pixels,
	    int          rowstride,
            int          x1,
            int          y1,
            int          x2,
            int          y2,
	    GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

#ifdef LITTLE
  register guint16 *s;	/* read 1 pixels at once */
#else
  register guint8 *s;
#endif
  register guint32 *o;

  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;

  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
#ifdef LITTLE
      s = (guint16 *) srow;
#else
      s = (guint8 *) srow;
#endif
      o = (guint32 *) orow;
      for (xx = x1; xx < x2; xx ++)
	{
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
 * convert 16 bits/pixel data
 * with alpha
 * data in msb format
 */
static void
rgb565amsb (GdkImage    *image,
	    guchar      *pixels,
	    int          rowstride,
            int          x1,
            int          y1,
            int          x2,
            int          y2,
	    GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

#ifdef LITTLE
  register guint8 *s;
#else
  register guint16 *s;	/* read 1 pixels at once */
#endif
  register guint32 *o;

  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;

  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
      s = srow;
      o = (guint32 *) orow;
      for (xx = x1; xx < x2; xx ++)
	{
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
 * convert 15 bits/pixel data
 * no alpha
 * data in lsb format
 */
static void
rgb555lsb (GdkImage     *image,
	   guchar       *pixels,
	   int           rowstride,
           int          x1,
           int          y1,
           int          x2,
           int          y2,
	   GdkColormap  *colormap)
{
  int xx, yy;
  int bpl;

#ifdef LITTLE
  register guint32 *s;	/* read 2 pixels at once */
#else
  register guint8 *s;	/* read 2 pixels at once */
#endif
  register guint16 *o;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;

  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
#ifdef LITTLE
      s = (guint32 *) srow;
#else
      s = srow;
#endif
      o = (guint16 *) orow;

      xx = x1;
      
      /* check for first odd pixel */
      if (xx & 1)
	{
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
	  o = (guint16 *) (((char *) o) + 3);
          ++xx;
	}

      g_assert (!(xx & 1));

      /* if x2 is even, then the -1 does nothing to number of
       * loop iterations, if x2 is odd then the -1 reduces
       * iterations by one
       */
      for (; xx < (x2 - 1); xx += 2)
	{
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
      if (x2 & 1)
	{
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
 * convert 15 bits/pixel data
 * no alpha
 * data in msb format
 */
static void
rgb555msb (GdkImage    *image,
	   guchar      *pixels,
	   int          rowstride,
           int          x1,
           int          y1,
           int          x2,
           int          y2,
	   GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

#ifdef LITTLE
  register guint8 *s;	/* read 2 pixels at once */
#else
  register guint32 *s;	/* read 2 pixels at once */
#endif
  register guint16 *o;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;

  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
      s = srow;
      o = (guint16 *) orow;

      xx = x1;
      /* See if first pixel is odd */
      if (xx & 1)
	{
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

	  o = (guint16 *) (((char *) o) + 3);
          ++xx;
	}

      g_assert (!(xx & 1));

      /* if x2 is even, then the -1 does nothing to number of
       * loop iterations, if x2 is odd then the -1 reduces
       * iterations by one
       */
      for (; xx < (x2 - 1); xx += 2)
	{
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
      if (x2 & 1)
	{
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
 * convert 15 bits/pixel data
 * with alpha
 * data in lsb format
 */
static void
rgb555alsb (GdkImage    *image,
	    guchar      *pixels,
	    int          rowstride,
            int          x1,
            int          y1,
            int          x2,
            int          y2,
	    GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

#ifdef LITTLE
  register guint16 *s;	/* read 1 pixels at once */
#else
  register guint8 *s;
#endif
  register guint32 *o;

  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;

  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
#ifdef LITTLE
      s = (guint16 *) srow;
#else
      s = srow;
#endif
      o = (guint32 *) orow;
      for (xx = x1; xx < x2; xx++)
	{
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
 * convert 15 bits/pixel data
 * with alpha
 * data in msb format
 */
static void
rgb555amsb (GdkImage    *image,
	    guchar      *pixels,
	    int          rowstride,
            int          x1,
            int          y1,
            int          x2,
            int          y2,
	    GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

#ifdef LITTLE
  register guint16 *s;	/* read 1 pixels at once */
#else
  register guint8 *s;
#endif
  register guint32 *o;

  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;

  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
#ifdef LITTLE
      s = (guint16 *) srow;
#else
      s = srow;
#endif
      o = (guint32 *) orow;
      for (xx = x1; xx < x2; xx++)
	{
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
rgb888alsb (GdkImage    *image,
	    guchar      *pixels,
	    int          rowstride,
            int          x1,
            int          y1,
            int          x2,
            int          y2,
	    GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

  guint8 *s;	/* for byte order swapping */
  guint8 *o;
  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;

  bpl = image->bpl;

  d (printf ("32 bits/pixel with alpha\n"));

  /* lsb data */
  for (yy = y1; yy < y2; yy++)
    {
      s = srow;
      o = orow;
      for (xx = x1; xx < x2; xx++)
	{
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
rgb888lsb (GdkImage    *image,
	   guchar      *pixels,
	   int          rowstride,
           int          x1,
           int          y1,
           int          x2,
           int          y2,
	   GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;
  guint8 *o, *s;

  bpl = image->bpl;

  d (printf ("32 bit, lsb, no alpha\n"));

  for (yy = y1; yy < y2; yy++)
    {
      s = srow;
      o = orow;
      for (xx = x1; xx < x2; xx++)
	{
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
rgb888amsb (GdkImage    *image,
	    guchar      *pixels,
	    int          rowstride,
            int          x1,
            int          y1,
            int          x2,
            int          y2,
	    GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;
#ifdef LITTLE
  guint32 *o;
  guint32 *s;
#else
  guint8 *s;	/* for byte order swapping */
  guint8 *o;
#endif

  d (printf ("32 bit, msb, with alpha\n"));

  bpl = image->bpl;

  /* msb data */
  for (yy = y1; yy < y2; yy++)
    {
#ifdef LITTLE
      s = (guint32 *) srow;
      o = (guint32 *) orow;
#else
      s = srow;
      o = orow;
#endif
      for (xx = x1; xx < x2; xx++)
	{
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
rgb888msb (GdkImage    *image,
	   guchar      *pixels,
	   int          rowstride,
           int          x1,
           int          y1,
           int          x2,
           int          y2,
	   GdkColormap *colormap)
{
  int xx, yy;
  int bpl;

  guint8 *srow = (guint8*)image->mem + y1 * image->bpl + x1 * image->bpp, *orow = pixels;
  guint8 *s;
  guint8 *o;

  d (printf ("32 bit, msb, no alpha\n"));

  bpl = image->bpl;

  for (yy = y1; yy < y2; yy++)
    {
      s = srow;
      o = orow;
      for (xx = x1; xx < x2; xx++)
	{
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
 * This should work correctly with any display/any endianness, but will probably
 * run quite slow
 */
static void
convert_real_slow (GdkImage    *image,
		   guchar      *pixels,
		   int          rowstride,
                   int          x1,
                   int          y1,
                   int          x2,
                   int          y2,
		   GdkColormap *cmap,
		   gboolean     alpha)
{
  int xx, yy;
  int bpl;
  guint8 *orow = pixels;
  guint8 *o;
  guint32 pixel;
  GdkVisual *v;
  guint8 component;
  int i;

  bpl = image->bpl;
  v = gdk_colormap_get_visual(cmap);

  d(printf("rgb  mask/shift/prec = %x:%x:%x %d:%d:%d  %d:%d:%d\n",
	   v->red_mask, v->green_mask, v->blue_mask,
	   v->red_shift, v->green_shift, v->blue_shift,
	   v->red_prec, v->green_prec, v->blue_prec));

  for (yy = y1; yy < y2; yy++)
    {
      o = orow;
      for (xx = x1; xx < x2; xx++)
	{
	  pixel = gdk_image_get_pixel(image, xx, yy);
	  switch (v->type)
	    {
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
				/* This is odd because it must sometimes shift left (otherwise
				 * I'd just shift >> (*_shift - 8 + *_prec + <0-7>). This logic
				 * should work for all bit sizes/shifts/etc.
				 */
	      component = 0;
	      for (i = 24; i < 32; i += v->red_prec)
		component |= ((pixel & v->red_mask) << (32 - v->red_shift - v->red_prec)) >> i;
	      *o++ = component;
	      component = 0;
	      for (i = 24; i < 32; i += v->green_prec)
		component |= ((pixel & v->green_mask) << (32 - v->green_shift - v->green_prec)) >> i;
	      *o++ = component;
	      component = 0;
	      for (i = 24; i < 32; i += v->blue_prec)
		component |= ((pixel & v->blue_mask) << (32 - v->blue_shift - v->blue_prec)) >> i;
	      *o++ = component;
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
      orow += rowstride;
    }
}

typedef void (* cfunc) (GdkImage    *image,
                        guchar      *pixels,
                        int          rowstride,
                        int          x1,
                        int          y1,
                        int          x2,
                        int          y2,
                        GdkColormap *cmap);

static cfunc convert_map[] = {
  rgb1,rgb1,rgb1a,rgb1a,
  rgb8,rgb8,rgb8a,rgb8a,
  rgb555lsb,rgb555msb,rgb555alsb,rgb555amsb,
  rgb565lsb,rgb565msb,rgb565alsb,rgb565amsb,
  rgb888lsb,rgb888msb,rgb888alsb,rgb888amsb
};

/*
 * perform actual conversion
 *
 *  If we can, try and use the optimised code versions, but as a default
 * fallback, and always for direct colour, use the generic/slow but complete
 * conversion function.
 */
static void
rgbconvert (GdkImage    *image,
	    guchar      *pixels,
	    int          rowstride,
	    gboolean     alpha,
            int          x,
            int          y,
            int          width,
            int          height,
	    GdkColormap *cmap)
{
  int index;
  int bank;
  GdkVisual *v;

  g_assert ((x + width) <= image->width);
  g_assert ((y + height) <= image->height);
  
  if (cmap == NULL)
    {
      /* Only allowed for bitmaps */
      g_return_if_fail (image->depth == 1);
      
      if (alpha)
        bitmap1a (image, pixels, rowstride,
                  x, y, x + width, y + height);
      else
        bitmap1 (image, pixels, rowstride,
                  x, y, x + width, y + height);
      
      return;
    }
  
  v = gdk_colormap_get_visual (cmap);
  bank = 5; /* default fallback converter */
  index = (image->byte_order == GDK_MSB_FIRST) | (alpha != 0) << 1;
  
  d(printf("masks = %x:%x:%x\n", v->red_mask, v->green_mask, v->blue_mask));
  d(printf("image depth = %d, bits per pixel = %d\n", image->depth, image->bits_per_pixel));
  
  switch (v->type)
    {
				/* I assume this is right for static & greyscale's too? */
    case GDK_VISUAL_STATIC_GRAY:
    case GDK_VISUAL_GRAYSCALE:
    case GDK_VISUAL_STATIC_COLOR:
    case GDK_VISUAL_PSEUDO_COLOR:
      switch (image->bits_per_pixel)
	{
	case 1:
	  bank = 0;
	  break;
	case 8:
	  if (image->depth == 8)
	    bank = 1;
	  break;
	}
      break;
    case GDK_VISUAL_TRUE_COLOR:
      switch (image->depth)
	{
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
    case GDK_VISUAL_DIRECT_COLOR:
      /* always use the slow version */
      break;
    }

  d (g_print ("converting using conversion function in bank %d\n", bank));

  if (bank == 5)
    {
      convert_real_slow (image, pixels, rowstride,
                         x, y, x + width, y + height,                         
                         cmap, alpha);
    }
  else
    {
      index |= bank << 2;
      d (g_print ("converting with index %d\n", index));
      (* convert_map[index]) (image, pixels, rowstride,
                              x, y, x + width, y + height,
                              cmap);
    }
}


/* Exported functions */

/**
 * gdk_pixbuf_get_from_drawable:
 * @dest: Destination pixbuf, or %NULL if a new pixbuf should be created.
 * @src: Source drawable.
 * @cmap: A colormap if @src doesn't have one set.
 * @src_x: Source X coordinate within drawable.
 * @src_y: Source Y coordinate within drawable.
 * @dest_x: Destination X coordinate in pixbuf, or 0 if @dest is NULL.
 * @dest_y: Destination Y coordinate in pixbuf, or 0 if @dest is NULL.
 * @width: Width in pixels of region to get.
 * @height: Height in pixels of region to get.
 *
 * Transfers image data from a #GdkDrawable and converts it to an RGB(A)
 * representation inside a #GdkPixbuf. In other words, copies
 * image data from a server-side drawable to a client-side RGB(A) buffer.
 * This allows you to efficiently read individual pixels on the client side.
 * 
 * If the drawable @src has no colormap (gdk_drawable_get_colormap()
 * returns %NULL), then a suitable colormap must be specified.
 * Typically a #GdkWindow or a pixmap created by passing a #GdkWindow
 * to gdk_pixmap_new() will already have a colormap associated with
 * it.  If the drawable has a colormap, the @cmap argument will be
 * ignored.  If the drawable is a bitmap (1 bit per pixel pixmap),
 * then a colormap is not required; pixels with a value of 1 are
 * assumed to be white, and pixels with a value of 0 are assumed to be
 * black. For taking screenshots, gdk_colormap_get_system() returns
 * the correct colormap to use.
 *
 * If the specified destination pixbuf @dest is %NULL, then this
 * function will create an RGB pixbuf with 8 bits per channel and no
 * alpha, with the same size specified by the @width and @height
 * arguments.  In this case, the @dest_x and @dest_y arguments must be
 * specified as 0.  If the specified destination pixbuf is not %NULL
 * and it contains alpha information, then the filled pixels will be
 * set to full opacity (alpha = 255).
 *
 * If the specified drawable is a pixmap, then the requested source
 * rectangle must be completely contained within the pixmap, otherwise
 * the function will return %NULL. For pixmaps only (not for windows)
 * passing -1 for width or height is allowed to mean the full width
 * or height of the pixmap.
 *
 * If the specified drawable is a window, and the window is off the
 * screen, then there is no image data in the obscured/offscreen
 * regions to be placed in the pixbuf. The contents of portions of the
 * pixbuf corresponding to the offscreen region are undefined.
 *
 * If the window you're obtaining data from is partially obscured by
 * other windows, then the contents of the pixbuf areas corresponding
 * to the obscured regions are undefined.
 * 
 * If the target drawable is not mapped (typically because it's
 * iconified/minimized or not on the current workspace), then %NULL
 * will be returned.
 *
 * If memory can't be allocated for the return value, %NULL will be returned
 * instead.
 *
 * (In short, there are several ways this function can fail, and if it fails
 *  it returns %NULL; so check the return value.)
 *
 * This function calls gdk_drawable_get_image() internally and
 * converts the resulting image to a #GdkPixbuf, so the
 * documentation for gdk_drawable_get_image() may also be relevant.
 * 
 * Return value: The same pixbuf as @dest if it was non-%NULL, or a newly-created
 * pixbuf with a reference count of 1 if no destination pixbuf was specified, or %NULL on error
 **/
GdkPixbuf *
gdk_pixbuf_get_from_drawable (GdkPixbuf   *dest,
			      GdkDrawable *src,
			      GdkColormap *cmap,
			      int src_x,  int src_y,
			      int dest_x, int dest_y,
			      int width,  int height)
{
  int src_width, src_height;
  GdkImage *image;
  int depth;
  int x0, y0;
  
  /* General sanity checks */

  g_return_val_if_fail (src != NULL, NULL);

  if (GDK_IS_WINDOW (src))
    /* FIXME: this is not perfect, since is_viewable() only tests
     * recursively up the Gdk parent window tree, but stops at
     * foreign windows or Gdk toplevels.  I.e. if a window manager
     * unmapped one of its own windows, this won't work.
     */
    g_return_val_if_fail (gdk_window_is_viewable (src), NULL);

  if (!dest)
    g_return_val_if_fail (dest_x == 0 && dest_y == 0, NULL);
  else
    {
      g_return_val_if_fail (dest->colorspace == GDK_COLORSPACE_RGB, NULL);
      g_return_val_if_fail (dest->n_channels == 3 || dest->n_channels == 4, NULL);
      g_return_val_if_fail (dest->bits_per_sample == 8, NULL);
    }

  /* Create the pixbuf if needed */
  if (!dest)
    {
      dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
      if (dest == NULL)
        return NULL;
    }

  if (cmap == NULL)
    cmap = gdk_drawable_get_colormap (src);

  depth = gdk_drawable_get_depth (src);
  
  if (depth != 1 && cmap == NULL)
    {
      g_warning ("%s: Source drawable has no colormap; either pass "
                 "in a colormap, or set the colormap on the drawable "
                 "with gdk_drawable_set_colormap()", G_STRLOC);
      return NULL;
    }
  
  /* Coordinate sanity checks */
  
  if (GDK_IS_PIXMAP (src))
    {
      gdk_drawable_get_size (src, &src_width, &src_height);
      if (width < 0)
        width = src_width;
      if (height < 0)
        height = src_height;
      
      g_return_val_if_fail (src_x >= 0 && src_y >= 0, NULL);
      g_return_val_if_fail (src_x + width <= src_width && src_y + height <= src_height, NULL);
    }

  if (dest)
    {
      g_return_val_if_fail (dest_x >= 0 && dest_y >= 0, NULL);
      g_return_val_if_fail (dest_x + width <= dest->width, NULL);
      g_return_val_if_fail (dest_y + height <= dest->height, NULL);
    }

  for (y0 = 0; y0 < height; y0 += GDK_SCRATCH_IMAGE_HEIGHT)
    {
      gint height1 = MIN (height - y0, GDK_SCRATCH_IMAGE_HEIGHT);
      for (x0 = 0; x0 < width; x0 += GDK_SCRATCH_IMAGE_WIDTH)
	{
	  gint xs0, ys0;
	  
	  gint width1 = MIN (width - x0, GDK_SCRATCH_IMAGE_WIDTH);
	  
	  image = _gdk_image_get_scratch (width1, height1, depth, &xs0, &ys0);

	  _gdk_drawable_copy_to_image (src, image,
				       src_x + x0, src_y + y0,
				       xs0, ys0, width1, height1);

	  gdk_pixbuf_get_from_image (dest, image, cmap,
				     xs0, ys0, dest_x + x0, dest_y + y0,
				     width1, height1);
	}
    }
  
  return dest;
}
        
/**
 * gdk_pixbuf_get_from_image:
 * @dest: Destination pixbuf, or %NULL if a new pixbuf should be created.
 * @src: Source #GdkImage.
 * @cmap: A colormap, or %NULL to use the one for @src
 * @src_x: Source X coordinate within drawable.
 * @src_y: Source Y coordinate within drawable.
 * @dest_x: Destination X coordinate in pixbuf, or 0 if @dest is NULL.
 * @dest_y: Destination Y coordinate in pixbuf, or 0 if @dest is NULL.
 * @width: Width in pixels of region to get.
 * @height: Height in pixels of region to get.
 * 
 * Same as gdk_pixbuf_get_from_drawable() but gets the pixbuf from
 * an image.
 * 
 * Return value: @dest, newly-created pixbuf if @dest was %NULL, %NULL on error
 **/
GdkPixbuf*
gdk_pixbuf_get_from_image (GdkPixbuf   *dest,
                           GdkImage    *src,
                           GdkColormap *cmap,
                           int          src_x,
                           int          src_y,
                           int          dest_x,
                           int          dest_y,
                           int          width,
                           int          height)
{
  int rowstride, bpp, alpha;
  
  /* General sanity checks */

  g_return_val_if_fail (GDK_IS_IMAGE (src), NULL);

  if (!dest)
    g_return_val_if_fail (dest_x == 0 && dest_y == 0, NULL);
  else
    {
      g_return_val_if_fail (dest->colorspace == GDK_COLORSPACE_RGB, NULL);
      g_return_val_if_fail (dest->n_channels == 3 || dest->n_channels == 4, NULL);
      g_return_val_if_fail (dest->bits_per_sample == 8, NULL);
    }

  if (cmap == NULL)
    cmap = gdk_image_get_colormap (src);
  
  if (src->depth != 1 && cmap == NULL)
    {
      g_warning ("%s: Source image has no colormap; either pass "
                 "in a colormap, or set the colormap on the image "
                 "with gdk_image_set_colormap()", G_STRLOC);
      return NULL;
    }
  
  /* Coordinate sanity checks */

  g_return_val_if_fail (src_x >= 0 && src_y >= 0, NULL);
  g_return_val_if_fail (src_x + width <= src->width && src_y + height <= src->height, NULL);

  if (dest)
    {
      g_return_val_if_fail (dest_x >= 0 && dest_y >= 0, NULL);
      g_return_val_if_fail (dest_x + width <= dest->width, NULL);
      g_return_val_if_fail (dest_y + height <= dest->height, NULL);
    }

  /* Create the pixbuf if needed */
  if (!dest)
    {
      dest = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
      if (dest == NULL)
        return NULL;
    }

  alpha = dest->has_alpha;
  rowstride = dest->rowstride;
  bpp = alpha ? 4 : 3;

  /* we offset into the image data based on the position we are
   * retrieving from
   */
  rgbconvert (src, dest->pixels +
	      (dest_y * rowstride) + (dest_x * bpp),
	      rowstride,
	      alpha,
              src_x, src_y,
              width,
              height,
	      cmap);
  
  return dest;
}
