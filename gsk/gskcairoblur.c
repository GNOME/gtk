/* GSK - The GIMP Toolkit
 *
 * Copyright (C) 2014 Red Hat
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
 *
 * Written by:
 *     Jasper St. Pierre <jstpierre@mecheye.net>
 *     Owen Taylor <otaylor@redhat.com>
 */

#include "config.h"

#include "gskcairoblurprivate.h"

#include "gskblurnodeprivate.h"

#include "gdkcairoprivate.h"

#include <math.h>
#include <string.h>

#define GAUSSIAN_SCALE_FACTOR ((3.0 * sqrt(2 * G_PI) / 4))

/*<private>
 * gsk_cairo_blur_compute_pixels:
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
int
gsk_cairo_blur_compute_pixels (double radius)
{
  return floor (radius * GAUSSIAN_SCALE_FACTOR * 1.5 + 0.5);
}

static gboolean
needs_blur (float        radius,
            GskBlurFlags blur_flags)
{
  /* Neither blurring horizontal nor vertical means no blurring at all. */
  if ((blur_flags & (GSK_BLUR_X | GSK_BLUR_Y)) == 0)
    return FALSE;

  /* The code doesn't actually do any blurring for radius 1, as it
   * ends up with box filter size 1 */
  if (radius <= 1.0)
    return FALSE;

  return TRUE;
}

static void
apply_current_scale (cairo_t *cr,
                     double  *x,
                     double  *y)
{
  double x_scale, y_scale;
  cairo_matrix_t matrix;

  cairo_get_matrix (cr, &matrix);

  if (matrix.xx != 0 || matrix.yx != 0)
    {
      x_scale = sqrt (matrix.xx * matrix.xx + matrix.yx * matrix.yx);
      y_scale = (matrix.xx * matrix.yy - matrix.yx * matrix.xy) / x_scale;
    }
  else if (matrix.xy != 0 || matrix.yy != 0)
    {
      y_scale = sqrt (matrix.xy * matrix.xy + matrix.yy * matrix.yy);
      x_scale = (matrix.xx * matrix.yy - matrix.yx * matrix.xy) / y_scale;
    }
  else
    {
      return;
    }

  if (x)
    *x *= fabs (x_scale);
  if (y)
    *y *= fabs (y_scale);
}

static const cairo_user_data_key_t original_cr_key = { 0, };

cairo_t *
gsk_cairo_blur_start_drawing (cairo_t         *cr,
                              float            radius,
                              GskBlurFlags     blur_flags)
{
  double clip_x1, clip_x2, clip_y1, clip_y2, clip_width, clip_height;
  cairo_surface_t *surface;
  cairo_t *blur_cr;
  double clip_radius;
  double x_scale, y_scale;
  gboolean blur_x = (blur_flags & GSK_BLUR_X) != 0;
  gboolean blur_y = (blur_flags & GSK_BLUR_Y) != 0;

  if (!needs_blur (radius, blur_flags))
    return cr;

  cairo_clip_extents (cr, &clip_x1, &clip_y1, &clip_x2, &clip_y2);
  clip_width = clip_x2 - clip_x1;
  clip_height = clip_y2 - clip_y1;

  clip_radius = gsk_cairo_blur_compute_pixels (radius);

  x_scale = y_scale = 1;
  cairo_surface_get_device_scale (cairo_get_target (cr), &x_scale, &y_scale);
  apply_current_scale (cr, &x_scale, &y_scale);

  clip_width = x_scale * (clip_width + (blur_x ? 2 * clip_radius : 0));
  clip_height = y_scale * (clip_height + (blur_y ? 2 * clip_radius : 0));

  if (blur_flags & GSK_BLUR_REPEAT)
    {
      if (!blur_x)
        clip_width = 1;
      if (!blur_y)
        clip_height = 1;
    }

  /* Create a larger surface to center the blur. */
  surface = cairo_surface_create_similar_image (cairo_get_target (cr),
                                                CAIRO_FORMAT_A8,
                                                clip_width,
                                                clip_height);
  cairo_surface_set_device_scale (surface, x_scale, y_scale);
  cairo_surface_set_device_offset (surface,
                                    x_scale * ((blur_x ? clip_radius : 0) - clip_x1),
                                    y_scale * ((blur_y ? clip_radius : 0) - clip_y1));

  blur_cr = cairo_create (surface);
  cairo_set_user_data (blur_cr, &original_cr_key, cairo_reference (cr), (cairo_destroy_func_t) cairo_destroy);

  if (cairo_has_current_point (cr))
    {
      double x, y;

      cairo_get_current_point (cr, &x, &y);
      cairo_move_to (blur_cr, x, y);
    }

  return blur_cr;
}

static void
mask_surface_repeat (cairo_t         *cr,
                     cairo_surface_t *surface)
{
  cairo_pattern_t *pattern;

  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_REPEAT);

  cairo_mask (cr, pattern);

  cairo_pattern_destroy (pattern);
}

cairo_t *
gsk_cairo_blur_finish_drawing (cairo_t         *cr,
                               GdkColorState   *ccs,
                               float            radius,
                               const GdkColor  *color,
                               GskBlurFlags     blur_flags)
{
  cairo_t *original_cr;
  cairo_surface_t *surface;
  double xscale, yscale;

  if (!needs_blur (radius, blur_flags))
    return cr;

  original_cr = cairo_get_user_data (cr, &original_cr_key);

  /* Blur the surface. */
  surface = cairo_get_target (cr);

  cairo_surface_get_device_scale (cairo_get_target (cr), &xscale, &yscale);

  gsk_blur_image_surface (surface, &GRAPHENE_SIZE_INIT (xscale * radius, yscale * radius), 3);

  gdk_cairo_set_source_color (original_cr, ccs, color);
  if (blur_flags & GSK_BLUR_REPEAT)
    mask_surface_repeat (original_cr, surface);
  else
    cairo_mask_surface (original_cr, surface, 0, 0);

  cairo_destroy (cr);

  cairo_surface_destroy (surface);

  return original_cr;
}

