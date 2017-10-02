/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Benjamin Otte <otte@gnome.org>
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "reftest-compare.h"

static void
get_surface_size (cairo_surface_t *surface,
                  int             *width,
                  int             *height)
{
  GdkRectangle area;
  cairo_t *cr;

  cr = cairo_create (surface);
  if (!gdk_cairo_get_clip_rectangle (cr, &area))
    {
      g_assert_not_reached ();
    }

  g_assert (area.x == 0 && area.y == 0);
  g_assert (area.width > 0 && area.height > 0);

  *width = area.width;
  *height = area.height;
}

static cairo_surface_t *
coerce_surface_for_comparison (cairo_surface_t *surface,
                               int              width,
                               int              height)
{
  cairo_surface_t *coerced;
  cairo_t *cr;

  coerced = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        width,
                                        height);
  cr = cairo_create (coerced);
  
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
  cairo_paint (cr);

  cairo_destroy (cr);
  cairo_surface_destroy (surface);

  g_assert (cairo_surface_status (coerced) == CAIRO_STATUS_SUCCESS);

  return coerced;
}

/* Compares two CAIRO_FORMAT_ARGB32 buffers, returning NULL if the
 * buffers are equal or a surface containing a diff between the two
 * surfaces.
 *
 * This function should be rewritten to compare all formats supported by
 * cairo_format_t instead of taking a mask as a parameter.
 *
 * This function is originally from cairo:test/buffer-diff.c.
 * Copyright © 2004 Richard D. Worth
 */
static cairo_surface_t *
buffer_diff_core (const guchar *buf_a,
                  int           stride_a,
        	  const guchar *buf_b,
                  int           stride_b,
        	  int		width,
        	  int		height)
{
  int x, y;
  guchar *buf_diff = NULL;
  int stride_diff = 0;
  cairo_surface_t *diff = NULL;

  for (y = 0; y < height; y++)
    {
      const guint32 *row_a = (const guint32 *) (buf_a + y * stride_a);
      const guint32 *row_b = (const guint32 *) (buf_b + y * stride_b);
      guint32 *row = (guint32 *) (buf_diff + y * stride_diff);

      for (x = 0; x < width; x++)
        {
          int channel;
          guint32 diff_pixel = 0;

          /* check if the pixels are the same */
          if (row_a[x] == row_b[x])
            continue;
        
          if (diff == NULL)
            {
              diff = cairo_image_surface_create (CAIRO_FORMAT_RGB24,
                                                 width,
                                                 height);
              g_assert (cairo_surface_status (diff) == CAIRO_STATUS_SUCCESS);
              buf_diff = cairo_image_surface_get_data (diff);
              stride_diff = cairo_image_surface_get_stride (diff);
              row = (guint32 *) (buf_diff + y * stride_diff);
            }

          /* calculate a difference value for all 4 channels */
          for (channel = 0; channel < 4; channel++)
            {
              int value_a = (row_a[x] >> (channel*8)) & 0xff;
              int value_b = (row_b[x] >> (channel*8)) & 0xff;
              guint diff;

              diff = ABS (value_a - value_b);
              diff *= 4;  /* emphasize */
              if (diff)
                diff += 128; /* make sure it's visible */
              if (diff > 255)
                diff = 255;
              diff_pixel |= diff << (channel*8);
            }

          if ((diff_pixel & 0x00ffffff) == 0)
            {
              /* alpha only difference, convert to luminance */
              guint8 alpha = diff_pixel >> 24;
              diff_pixel = alpha * 0x010101;
            }
          
          row[x] = diff_pixel;
      }
  }

  return diff;
}

cairo_surface_t *
reftest_compare_surfaces (cairo_surface_t *surface1,
                          cairo_surface_t *surface2)
{
  int w1, h1, w2, h2, w, h;
  cairo_surface_t *diff;
  
  get_surface_size (surface1, &w1, &h1);
  get_surface_size (surface2, &w2, &h2);
  w = MAX (w1, w2);
  h = MAX (h1, h2);
  surface1 = coerce_surface_for_comparison (surface1, w, h);
  surface2 = coerce_surface_for_comparison (surface2, w, h);

  diff = buffer_diff_core (cairo_image_surface_get_data (surface1),
                           cairo_image_surface_get_stride (surface1),
                           cairo_image_surface_get_data (surface2),
                           cairo_image_surface_get_stride (surface2),
                           w, h);

  return diff;
}

