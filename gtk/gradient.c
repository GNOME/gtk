/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */

/* Metacity gradient rendering */

/* 
 * Copyright (C) 2001 Havoc Pennington, 99% copied from wrlib in
 * WindowMaker, Copyright (C) 1997-2000 Dan Pascu and Alfredo Kojima
 * Copyright (C) 2005 Elijah Newren
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.  */

#include "gradient.h"
#include "util.h"
#include <string.h>

/* This is all Alfredo's and Dan's usual very nice WindowMaker code,
 * slightly GTK-ized
 */
static GdkPixbuf* meta_gradient_create_horizontal       (int             width,
                                                         int             height,
                                                         const GdkColor *from,
                                                         const GdkColor *to);
static GdkPixbuf* meta_gradient_create_vertical         (int             width,
                                                         int             height,
                                                         const GdkColor *from,
                                                         const GdkColor *to);
static GdkPixbuf* meta_gradient_create_diagonal         (int             width,
                                                         int             height,
                                                         const GdkColor *from,
                                                         const GdkColor *to);
static GdkPixbuf* meta_gradient_create_multi_horizontal (int             width,
                                                         int             height,
                                                         const GdkColor *colors,
                                                         int             count);
static GdkPixbuf* meta_gradient_create_multi_vertical   (int             width,
                                                         int             height,
                                                         const GdkColor *colors,
                                                         int             count);
static GdkPixbuf* meta_gradient_create_multi_diagonal   (int             width,
                                                         int             height,
                                                         const GdkColor *colors,
                                                         int             count);


/* Used as the destroy notification function for gdk_pixbuf_new() */
static void
free_buffer (guchar *pixels, gpointer data)
{
  g_free (pixels);
}

static GdkPixbuf*
blank_pixbuf (int width, int height, gboolean no_padding)
{
  guchar *buf;
  int rowstride;

  g_return_val_if_fail (width > 0, NULL);
  g_return_val_if_fail (height > 0, NULL);

  if (no_padding)
    rowstride = width * 3;
  else
    /* Always align rows to 32-bit boundaries */  
    rowstride = 4 * ((3 * width + 3) / 4);

  buf = g_try_malloc (height * rowstride);
  if (!buf)
    return NULL;

  return gdk_pixbuf_new_from_data (buf, GDK_COLORSPACE_RGB,
                                   FALSE, 8,
                                   width, height, rowstride,
                                   free_buffer, NULL);
}

GdkPixbuf*
meta_gradient_create_simple (int              width,
                             int              height,
                             const GdkColor  *from,
                             const GdkColor  *to,
                             MetaGradientType style)
{
  switch (style)
    {
    case META_GRADIENT_HORIZONTAL:
      return meta_gradient_create_horizontal (width, height,
                                              from, to);
    case META_GRADIENT_VERTICAL:
      return meta_gradient_create_vertical (width, height,
                                            from, to);

    case META_GRADIENT_DIAGONAL:
      return meta_gradient_create_diagonal (width, height,
                                            from, to);
    case META_GRADIENT_LAST:
      break;
    }
  g_assert_not_reached ();
  return NULL;
}

GdkPixbuf*
meta_gradient_create_multi (int              width,
                            int              height,
                            const GdkColor  *colors,
                            int              n_colors,
                            MetaGradientType style)
{

  if (n_colors > 2)
    {
      switch (style)
        {
        case META_GRADIENT_HORIZONTAL:
          return meta_gradient_create_multi_horizontal (width, height, colors, n_colors);
        case META_GRADIENT_VERTICAL:
          return meta_gradient_create_multi_vertical (width, height, colors, n_colors);
        case META_GRADIENT_DIAGONAL:
          return meta_gradient_create_multi_diagonal (width, height, colors, n_colors);
        case META_GRADIENT_LAST:
          g_assert_not_reached ();
          break;
        }
    }
  else if (n_colors > 1)
    {
      return meta_gradient_create_simple (width, height, &colors[0], &colors[1],
                                          style);
    }
  else if (n_colors > 0)
    {
      return meta_gradient_create_simple (width, height, &colors[0], &colors[0],
                                          style);
    }
  g_assert_not_reached ();
  return NULL;
}

/* Interwoven essentially means we have two vertical gradients,
 * cut into horizontal strips of the given thickness, and then the strips
 * are alternated. I'm not sure what it's good for, just copied since
 * WindowMaker had it.
 */
GdkPixbuf*
meta_gradient_create_interwoven (int            width,
                                 int            height,
                                 const GdkColor colors1[2],
                                 int            thickness1,
                                 const GdkColor colors2[2],
                                 int            thickness2)
{
  
  int i, j, k, l, ll;
  long r1, g1, b1, dr1, dg1, db1;
  long r2, g2, b2, dr2, dg2, db2;
  GdkPixbuf *pixbuf;
  unsigned char *ptr;
  unsigned char *pixels;
  int rowstride;
  
  pixbuf = blank_pixbuf (width, height, FALSE);
  if (pixbuf == NULL)
    return NULL;
    
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  
  r1 = colors1[0].red<<8;
  g1 = colors1[0].green<<8;
  b1 = colors1[0].blue<<8;

  r2 = colors2[0].red<<8;
  g2 = colors2[0].green<<8;
  b2 = colors2[0].blue<<8;

  dr1 = ((colors1[1].red-colors1[0].red)<<8)/(int)height;
  dg1 = ((colors1[1].green-colors1[0].green)<<8)/(int)height;
  db1 = ((colors1[1].blue-colors1[0].blue)<<8)/(int)height;

  dr2 = ((colors2[1].red-colors2[0].red)<<8)/(int)height;
  dg2 = ((colors2[1].green-colors2[0].green)<<8)/(int)height;
  db2 = ((colors2[1].blue-colors2[0].blue)<<8)/(int)height;

  for (i=0,k=0,l=0,ll=thickness1; i<height; i++)
    {
      ptr = pixels + i * rowstride;
      
      if (k == 0)
        {
          ptr[0] = (unsigned char) (r1>>16);
          ptr[1] = (unsigned char) (g1>>16);
          ptr[2] = (unsigned char) (b1>>16);
        }
      else
        {
          ptr[0] = (unsigned char) (r2>>16);
          ptr[1] = (unsigned char) (g2>>16);
          ptr[2] = (unsigned char) (b2>>16);
        }

      for (j=1; j <= width/2; j *= 2)
        memcpy (&(ptr[j*3]), ptr, j*3);
      memcpy (&(ptr[j*3]), ptr, (width - j)*3);

      if (++l == ll)
        {
          if (k == 0)
            {
              k = 1;
              ll = thickness2;
            }
          else
            {
              k = 0;
              ll = thickness1;
            }
          l = 0;
        }
      r1+=dr1;
      g1+=dg1;
      b1+=db1;
	
      r2+=dr2;
      g2+=dg2;
      b2+=db2;
    }

  return pixbuf;
}

/*
 *----------------------------------------------------------------------
 * meta_gradient_create_horizontal--
 * 	Renders a horizontal linear gradient of the specified size in the
 * GdkPixbuf format with a border of the specified type. 
 * 
 * Returns:
 * 	A 24bit GdkPixbuf with the gradient (no alpha channel).
 * 
 * Side effects:
 * 	None
 *---------------------------------------------------------------------- 
 */
static GdkPixbuf*
meta_gradient_create_horizontal (int width, int height,
                                 const GdkColor *from,
                                 const GdkColor *to)
{    
  int i;
  long r, g, b, dr, dg, db;
  GdkPixbuf *pixbuf;
  unsigned char *ptr;
  unsigned char *pixels;
  int r0, g0, b0;
  int rf, gf, bf;
  int rowstride;

  pixbuf = blank_pixbuf (width, height, FALSE);
  if (pixbuf == NULL)
    return NULL;
    
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  ptr = pixels;
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  
  r0 = (guchar) (from->red / 256.0);
  g0 = (guchar) (from->green / 256.0);
  b0 = (guchar) (from->blue / 256.0);
  rf = (guchar) (to->red / 256.0);
  gf = (guchar) (to->green / 256.0);
  bf = (guchar) (to->blue / 256.0);  
  
  r = r0 << 16;
  g = g0 << 16;
  b = b0 << 16;
    
  dr = ((rf-r0)<<16)/(int)width;
  dg = ((gf-g0)<<16)/(int)width;
  db = ((bf-b0)<<16)/(int)width;
  /* render the first line */
  for (i=0; i<width; i++)
    {
      *(ptr++) = (unsigned char)(r>>16);
      *(ptr++) = (unsigned char)(g>>16);
      *(ptr++) = (unsigned char)(b>>16);
      r += dr;
      g += dg;
      b += db;
    }

  /* copy the first line to the other lines */
  for (i=1; i<height; i++)
    {
      memcpy (&(pixels[i*rowstride]), pixels, rowstride);
    }
  return pixbuf;
}

/*
 *----------------------------------------------------------------------
 * meta_gradient_create_vertical--
 *      Renders a vertical linear gradient of the specified size in the
 * GdkPixbuf format with a border of the specified type.
 *
 * Returns:
 *      A 24bit GdkPixbuf with the gradient (no alpha channel).
 *
 * Side effects:
 *      None
 *----------------------------------------------------------------------
 */
static GdkPixbuf*
meta_gradient_create_vertical (int width, int height,
                               const GdkColor *from,
                               const GdkColor *to)
{
  int i, j;
  long r, g, b, dr, dg, db;
  GdkPixbuf *pixbuf;
  unsigned char *ptr;
  int r0, g0, b0;
  int rf, gf, bf;
  int rowstride;
  unsigned char *pixels;
  
  pixbuf = blank_pixbuf (width, height, FALSE);
  if (pixbuf == NULL)
    return NULL;
    
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  
  r0 = (guchar) (from->red / 256.0);
  g0 = (guchar) (from->green / 256.0);
  b0 = (guchar) (from->blue / 256.0);
  rf = (guchar) (to->red / 256.0);
  gf = (guchar) (to->green / 256.0);
  bf = (guchar) (to->blue / 256.0);
  
  r = r0<<16;
  g = g0<<16;
  b = b0<<16;

  dr = ((rf-r0)<<16)/(int)height;
  dg = ((gf-g0)<<16)/(int)height;
  db = ((bf-b0)<<16)/(int)height;

  for (i=0; i<height; i++)
    {
      ptr = pixels + i * rowstride;
      
      ptr[0] = (unsigned char)(r>>16);
      ptr[1] = (unsigned char)(g>>16);
      ptr[2] = (unsigned char)(b>>16);

      for (j=1; j <= width/2; j *= 2)
        memcpy (&(ptr[j*3]), ptr, j*3);
      memcpy (&(ptr[j*3]), ptr, (width - j)*3);

      r+=dr;
      g+=dg;
      b+=db;
    }
  return pixbuf;
}


/*
 *----------------------------------------------------------------------
 * meta_gradient_create_diagonal--
 *      Renders a diagonal linear gradient of the specified size in the
 * GdkPixbuf format with a border of the specified type.
 *
 * Returns:
 *      A 24bit GdkPixbuf with the gradient (no alpha channel).
 *
 * Side effects:
 *      None
 *----------------------------------------------------------------------
 */


static GdkPixbuf*
meta_gradient_create_diagonal (int width, int height,
                               const GdkColor *from,
                               const GdkColor *to)
{
  GdkPixbuf *pixbuf, *tmp;
  int j;
  float a, offset;
  unsigned char *ptr;
  unsigned char *pixels;
  int rowstride;
  
  if (width == 1)
    return meta_gradient_create_vertical (width, height, from, to);
  else if (height == 1)
    return meta_gradient_create_horizontal (width, height, from, to);

  pixbuf = blank_pixbuf (width, height, FALSE);
  if (pixbuf == NULL)
    return NULL;
    
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);

  tmp = meta_gradient_create_horizontal (2*width-1, 1, from, to);
  if (!tmp)
    {
      g_object_unref (G_OBJECT (pixbuf));
      return NULL;
    }

  ptr = gdk_pixbuf_get_pixels (tmp);

  a = ((float)(width - 1))/((float)(height - 1));
  width = width * 3;

  /* copy the first line to the other lines with corresponding offset */
  for (j=0, offset=0.0; j<rowstride*height; j += rowstride)
    {
      memcpy (&(pixels[j]), &ptr[3*(int)offset], width);
      offset += a;
    }

  g_object_unref (G_OBJECT (tmp));
  return pixbuf;
}


static GdkPixbuf*
meta_gradient_create_multi_horizontal (int width, int height,
                                       const GdkColor *colors,
                                       int count)
{
  int i, j, k;
  long r, g, b, dr, dg, db;
  GdkPixbuf *pixbuf;
  unsigned char *ptr;
  unsigned char *pixels;
  int width2;  
  int rowstride;
  
  g_return_val_if_fail (count > 2, NULL);

  pixbuf = blank_pixbuf (width, height, FALSE);
  if (pixbuf == NULL)
    return NULL;
    
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  ptr = pixels;
    
  if (count > width)
    count = width;
    
  if (count > 1)
    width2 = width/(count-1);
  else
    width2 = width;
    
  k = 0;

  r = colors[0].red << 8;
  g = colors[0].green << 8;
  b = colors[0].blue << 8;

  /* render the first line */
  for (i=1; i<count; i++)
    {
      dr = ((int)(colors[i].red   - colors[i-1].red)  <<8)/(int)width2;
      dg = ((int)(colors[i].green - colors[i-1].green)<<8)/(int)width2;
      db = ((int)(colors[i].blue  - colors[i-1].blue) <<8)/(int)width2;
      for (j=0; j<width2; j++)
        {
          *ptr++ = (unsigned char)(r>>16);
          *ptr++ = (unsigned char)(g>>16);
          *ptr++ = (unsigned char)(b>>16);
          r += dr;
          g += dg;
          b += db;
          k++;
	}
      r = colors[i].red << 8;
      g = colors[i].green << 8;
      b = colors[i].blue << 8;
    }
  for (j=k; j<width; j++)
    {
      *ptr++ = (unsigned char)(r>>16);
      *ptr++ = (unsigned char)(g>>16);
      *ptr++ = (unsigned char)(b>>16);
    }
    
  /* copy the first line to the other lines */
  for (i=1; i<height; i++)
    {
      memcpy (&(pixels[i*rowstride]), pixels, rowstride);
    }
  return pixbuf;
}

static GdkPixbuf*
meta_gradient_create_multi_vertical (int width, int height,
                                     const GdkColor *colors,
                                     int count)
{
  int i, j, k;
  long r, g, b, dr, dg, db;
  GdkPixbuf *pixbuf;
  unsigned char *ptr, *tmp, *pixels;
  int height2;
  int x;
  int rowstride;
  
  g_return_val_if_fail (count > 2, NULL);

  pixbuf = blank_pixbuf (width, height, FALSE);
  if (pixbuf == NULL)
    return NULL;
    
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  ptr = pixels;
    
  if (count > height)
    count = height;
    
  if (count > 1)
    height2 = height/(count-1);
  else
    height2 = height;
    
  k = 0;

  r = colors[0].red << 8;
  g = colors[0].green << 8;
  b = colors[0].blue << 8;

  for (i=1; i<count; i++)
    {
      dr = ((int)(colors[i].red   - colors[i-1].red)  <<8)/(int)height2;
      dg = ((int)(colors[i].green - colors[i-1].green)<<8)/(int)height2;
      db = ((int)(colors[i].blue  - colors[i-1].blue) <<8)/(int)height2;

      for (j=0; j<height2; j++)
        {
          ptr[0] = (unsigned char)(r>>16);
          ptr[1] = (unsigned char)(g>>16);
          ptr[2] = (unsigned char)(b>>16);

          for (x=1; x <= width/2; x *= 2)
            memcpy (&(ptr[x*3]), ptr, x*3);
          memcpy (&(ptr[x*3]), ptr, (width - x)*3);

          ptr += rowstride;
          
          r += dr;
          g += dg;
          b += db;
          k++;
	}
      r = colors[i].red << 8;
      g = colors[i].green << 8;
      b = colors[i].blue << 8;
    }

  if (k<height)
    {
      tmp = ptr;

      ptr[0] = (unsigned char) (r>>16);
      ptr[1] = (unsigned char) (g>>16);
      ptr[2] = (unsigned char) (b>>16);

      for (x=1; x <= width/2; x *= 2)
        memcpy (&(ptr[x*3]), ptr, x*3);
      memcpy (&(ptr[x*3]), ptr, (width - x)*3);

      ptr += rowstride;
      
      for (j=k+1; j<height; j++)
        {
          memcpy (ptr, tmp, rowstride);
          ptr += rowstride;
        }
    }
    
  return pixbuf;
}


static GdkPixbuf*
meta_gradient_create_multi_diagonal (int width, int height,
                                     const GdkColor *colors,
                                     int count)
{
  GdkPixbuf *pixbuf, *tmp;
  float a, offset;
  int j;
  unsigned char *ptr;
  unsigned char *pixels;
  int rowstride;
  
  g_return_val_if_fail (count > 2, NULL);

  if (width == 1)
    return meta_gradient_create_multi_vertical (width, height, colors, count);
  else if (height == 1)
    return meta_gradient_create_multi_horizontal (width, height, colors, count);

  pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8,
                           width, height);
  if (pixbuf == NULL)
    return NULL;
    
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  
  if (count > width)
    count = width;
  if (count > height)
    count = height;

  if (count > 2)
    tmp = meta_gradient_create_multi_horizontal (2*width-1, 1, colors, count);
  else
    /* wrlib multiplies these colors by 256 before passing them in, but
     * I think it's a bug in wrlib, so changed here. I could be wrong
     * though, if we notice two-color multi diagonals not working.
     */
    tmp = meta_gradient_create_horizontal (2*width-1, 1,
                                           &colors[0], &colors[1]);

  if (!tmp)
    {
      g_object_unref (G_OBJECT (pixbuf));
      return NULL;
    }
  ptr = gdk_pixbuf_get_pixels (tmp);

  a = ((float)(width - 1))/((float)(height - 1));
  width = width * 3;

  /* copy the first line to the other lines with corresponding offset */
  for (j=0, offset=0; j<rowstride*height; j += rowstride)
    {
      memcpy (&(pixels[j]), &ptr[3*(int)offset], width);
      offset += a;
    }

  g_object_unref (G_OBJECT (tmp));
  return pixbuf;
}

static void
simple_multiply_alpha (GdkPixbuf *pixbuf,
                       guchar     alpha)
{
  guchar *pixels;
  int rowstride;
  int height;
  int row;

  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
  
  if (alpha == 255)
    return;
  
  g_assert (gdk_pixbuf_get_has_alpha (pixbuf));
  
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  row = 0;
  while (row < height)
    {
      guchar *p;
      guchar *end;

      p = pixels + row * rowstride;
      end = p + rowstride;

      while (p != end)
        {
          p += 3; /* skip RGB */

          /* multiply the two alpha channels. not sure this is right.
           * but some end cases are that if the pixbuf contains 255,
           * then it should be modified to contain "alpha"; if the
           * pixbuf contains 0, it should remain 0.
           */
          /* ((*p / 255.0) * (alpha / 255.0)) * 255; */
          *p = (guchar) (((int) *p * (int) alpha) / (int) 255);
          
          ++p; /* skip A */
        }

      ++row;
    }
}

static void
meta_gradient_add_alpha_horizontal (GdkPixbuf           *pixbuf,
                                    const unsigned char *alphas,
                                    int                  n_alphas)
{
  int i, j;
  long a, da;
  unsigned char *p;
  unsigned char *pixels;
  int width2;  
  int rowstride;
  int width, height;
  unsigned char *gradient;
  unsigned char *gradient_p;
  unsigned char *gradient_end;
  
  g_return_if_fail (n_alphas > 0);

  if (n_alphas == 1)
    {
      /* Optimize this */
      simple_multiply_alpha (pixbuf, alphas[0]);
      return;
    }
  
  width = gdk_pixbuf_get_width (pixbuf);
  height = gdk_pixbuf_get_height (pixbuf);

  gradient = g_new (unsigned char, width);
  gradient_end = gradient + width;
  
  if (n_alphas > width)
    n_alphas = width;
    
  if (n_alphas > 1)
    width2 = width / (n_alphas - 1);
  else
    width2 = width;
    
  a = alphas[0] << 8;
  gradient_p = gradient;
  
  /* render the gradient into an array */
  for (i = 1; i < n_alphas; i++)
    {
      da = (((int)(alphas[i] - (int) alphas[i-1])) << 8) / (int) width2;

      for (j = 0; j < width2; j++)
        {
          *gradient_p++ = (a >> 8);
          
          a += da;
	}

      a = alphas[i] << 8;
    }

  /* get leftover pixels */
  while (gradient_p != gradient_end)
    {
      *gradient_p++ = a >> 8;
    }
    
  /* Now for each line of the pixbuf, fill in with the gradient */
  pixels = gdk_pixbuf_get_pixels (pixbuf);
  rowstride = gdk_pixbuf_get_rowstride (pixbuf);
  
  p = pixels;
  i = 0;
  while (i < height)
    {
      unsigned char *row_end = p + rowstride;
      gradient_p = gradient;

      p += 3;
      while (gradient_p != gradient_end)
        {
          /* multiply the two alpha channels. not sure this is right.
           * but some end cases are that if the pixbuf contains 255,
           * then it should be modified to contain "alpha"; if the
           * pixbuf contains 0, it should remain 0.
           */
          /* ((*p / 255.0) * (alpha / 255.0)) * 255; */
          *p = (guchar) (((int) *p * (int) *gradient_p) / (int) 255);

          p += 4;
          ++gradient_p;
        }

      p = row_end;
      ++i;
    }
  
  g_free (gradient);
}

void
meta_gradient_add_alpha (GdkPixbuf       *pixbuf,
                         const guchar    *alphas,
                         int              n_alphas,
                         MetaGradientType type)
{
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
  g_return_if_fail (gdk_pixbuf_get_has_alpha (pixbuf));
  g_return_if_fail (n_alphas > 0);
  
  switch (type)
    {
    case META_GRADIENT_HORIZONTAL:
      meta_gradient_add_alpha_horizontal (pixbuf, alphas, n_alphas);
      break;
      
    case META_GRADIENT_VERTICAL:
      g_printerr ("metacity: vertical alpha channel gradient not implemented yet\n");
      break;
      
    case META_GRADIENT_DIAGONAL:
      g_printerr ("metacity: diagonal alpha channel gradient not implemented yet\n");
      break;
      
    case META_GRADIENT_LAST:
      g_assert_not_reached ();
      break;
    }
}
