/*
 * Copyright (C) 2012 Canonical Ltd
 *
 * This  library is free  software; you can  redistribute it and/or
 * modify it  under  the terms  of the  GNU Lesser  General  Public
 * License  as published  by the Free  Software  Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed  in the hope that it will be useful,
 * but  WITHOUT ANY WARRANTY; without even  the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License  along  with  this library;  if not,  write to  the Free
 * Software Foundation, Inc., 51  Franklin St, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 * Authored by Andrea Cimitan <andrea.cimitan@canonical.com>
 * Original code from Mirco Mueller <mirco.mueller@canonical.com>
 *
 */

#include "gtkcairoblurprivate.h"

#include <math.h>

/*
 * Notes:
 *   based on exponential-blur algorithm by Jani Huhtanen
 */
static inline void
_blurinner (guchar* pixel,
            gint   *zR,
            gint   *zG,
            gint   *zB,
            gint   *zA,
            gint    alpha,
            gint    aprec,
            gint    zprec)
{
  gint R;
  gint G;
  gint B;
  guchar A;

  R = *pixel;
  G = *(pixel + 1);
  B = *(pixel + 2);
  A = *(pixel + 3);

  *zR += (alpha * ((R << zprec) - *zR)) >> aprec;
  *zG += (alpha * ((G << zprec) - *zG)) >> aprec;
  *zB += (alpha * ((B << zprec) - *zB)) >> aprec;
  *zA += (alpha * ((A << zprec) - *zA)) >> aprec;

  *pixel       = *zR >> zprec;
  *(pixel + 1) = *zG >> zprec;
  *(pixel + 2) = *zB >> zprec;
  *(pixel + 3) = *zA >> zprec;
} 

static inline void
_blurrow (guchar* pixels,
          gint    width,
          gint    height,
          gint    rowstride,
          gint    channels,
          gint    line,
          gint    alpha,
          gint    aprec,
          gint    zprec)
{
  gint    zR;
  gint    zG;
  gint    zB;
  gint    zA;
  gint    index;
  guchar* scanline;

  scanline = &pixels[line * rowstride];

  zR = *scanline << zprec;
  zG = *(scanline + 1) << zprec;
  zB = *(scanline + 2) << zprec;
  zA = *(scanline + 3) << zprec;

  for (index = 0; index < width; index ++)
    _blurinner (&scanline[index * channels],
                &zR,
                &zG,
                &zB,
                &zA,
                alpha,
                aprec,
                zprec);

  for (index = width - 2; index >= 0; index--)
    _blurinner (&scanline[index * channels],
                &zR,
                &zG,
                &zB,
                &zA,
                alpha,
                aprec,
                zprec);
}

static inline void
_blurcol (guchar* pixels,
          gint    width,
          gint    height,
          gint    rowstride,
          gint    channels,
          gint    x,
          gint    alpha,
          gint    aprec,
          gint    zprec)
{
  gint zR;
  gint zG;
  gint zB;
  gint zA;
  gint index;
  guchar* ptr;

  ptr = pixels;
  
  ptr += x * channels;

  zR = *((guchar*) ptr    ) << zprec;
  zG = *((guchar*) ptr + 1) << zprec;
  zB = *((guchar*) ptr + 2) << zprec;
  zA = *((guchar*) ptr + 3) << zprec;

  for (index = 0; index < height; index++)
    _blurinner (&ptr[index * rowstride],
                &zR,
                &zG,
                &zB,
                &zA,
                alpha,
                aprec,
                zprec);

  for (index = height - 2; index >= 0; index--)
    _blurinner (&ptr[index * rowstride],
                &zR,
                &zG,
                &zB,
                &zA,
                alpha,
                aprec,
                zprec);
}

/*
 * _expblur:
 * @pixels: image data
 * @width: image width
 * @height: image height
 * @rowstride: image rowstride
 * @channels: image channels
 * @radius: kernel radius
 * @aprec: precision of alpha parameter in fixed-point format 0.aprec
 * @zprec: precision of state parameters zR,zG,zB and zA in fp format 8.zprec
 *
 * Performs an in-place blur of image data 'pixels'
 * with kernel of approximate radius 'radius'.
 *
 * Blurs with two sided exponential impulse response.
 *
 */
static void
_expblur (guchar* pixels,
          gint    width,
          gint    height,
          gint    rowstride,
          gint    channels,
          double  radius,
          gint    aprec,
          gint    zprec)
{
  gint alpha;
  int row, col;

  /* Calculate the alpha such that 90% of 
   * the kernel is within the radius.
   * (Kernel extends to infinity) */
  alpha = (gint) ((1 << aprec) * (1.0f - expf (-2.3f / (radius + 1.f))));

  for (row = 0; row < height; row++)
    _blurrow (pixels,
              width,
              height,
              rowstride,
              channels,
              row,
              alpha,
              aprec,
              zprec);

  for(col = 0; col < width; col++)
    _blurcol (pixels,
              width,
              height,
              rowstride,
              channels,
              col,
              alpha,
              aprec,
              zprec);
}


/*
 * _gtk_cairo_blur_surface:
 * @surface: a cairo image surface.
 * @radius: the blur radius.
 *
 * Blurs the cairo image surface at the given radius.
 */
void
_gtk_cairo_blur_surface (cairo_surface_t* surface,
                         double           radius)
{
  cairo_format_t format;

  g_return_if_fail (surface != NULL);
  g_return_if_fail (cairo_surface_get_type (surface) == CAIRO_SURFACE_TYPE_IMAGE);

  format = cairo_image_surface_get_format (surface);
  g_return_if_fail (format == CAIRO_FORMAT_RGB24 ||
                    format == CAIRO_FORMAT_ARGB32);

  if (radius == 0)
    return;

  /* Before we mess with the surface execute any pending drawing. */
  cairo_surface_flush (surface);

  _expblur (cairo_image_surface_get_data (surface),
            cairo_image_surface_get_width (surface),
            cairo_image_surface_get_height (surface),
            cairo_image_surface_get_stride (surface),
            4,
            radius,
            16,
            7);

  /* Inform cairo we altered the surfaces contents. */
  cairo_surface_mark_dirty (surface);
}

/**
 * _gtk_cairo_blur_compute_pixels:
 * @radius: the radius to compute the pixels for
 *
 * Computes the number of pixels necessary to extend an image in one
 * direction to hold the image with shadow.
 *
 * This is just the number of pixels added by the blur radius, shadow
 * offset and spread are not included.
 * 
 * Much of this, the 3 * sqrt(2 * pi) / 4, is the known value for
 * approximating a Gaussian using box blurs.  This yields quite a good
 * approximation for a Gaussian.  Then we multiply this by 1.5 since our
 * code wants the radius of the entire triple-box-blur kernel instead of
 * the diameter of an individual box blur.  For more details, see:
 * http://www.w3.org/TR/SVG11/filters.html#feGaussianBlurElement
 * https://bugzilla.mozilla.org/show_bug.cgi?id=590039#c19
 */
#define GAUSSIAN_SCALE_FACTOR ((3.0 * sqrt(2 * G_PI) / 4) * 1.5)

int
_gtk_cairo_blur_compute_pixels (double radius)
{
  return floor (radius * GAUSSIAN_SCALE_FACTOR + 0.5);
}
