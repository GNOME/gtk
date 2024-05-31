/* GTK - The GIMP Toolkit
 * Copyright (C) 2024 Red Hat, Inc.
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcsscolorprivate.h"
#include "gtkcolorutilsprivate.h"

/* {{{ Initialization */

void
gtk_css_color_init (GtkCssColor      *color,
                    GtkCssColorSpace  color_space,
                    const float       values[4])
{
  gboolean missing[4] = { 0, };

  /* look for powerless components */
  switch (color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_OKLAB:
      break;

    case GTK_CSS_COLOR_SPACE_HSL:
      if (fabs (values[2]) < 0.001)
        missing[0] = 1;
      break;

    case GTK_CSS_COLOR_SPACE_HWB:
      if (values[1] + values[2] > 99.999)
        missing[0] = 1;
      break;

    case GTK_CSS_COLOR_SPACE_OKLCH:
      if (fabs (values[1]) < 0.001)
        missing[2] = 1;
      break;

    default:
      g_assert_not_reached ();
    }

  gtk_css_color_init_with_missing (color, color_space, values, missing);
}

/* }}} */
/* {{{ Color conversion */

static void
convert_to_rectangular (GtkCssColor *output)
{
  float v[4];

  switch (output->color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_OKLAB:
      break;

    case GTK_CSS_COLOR_SPACE_HSL:
      gtk_hsl_to_rgb (output->values[0],
                      output->values[1] / 100,
                      output->values[2] / 100,
                      &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_SRGB, v);
      break;

    case GTK_CSS_COLOR_SPACE_HWB:
      gtk_hwb_to_rgb (output->values[0],
                      output->values[1] / 100,
                      output->values[2] / 100,
                      &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_SRGB, v);
      break;

    case GTK_CSS_COLOR_SPACE_OKLCH:
      gtk_oklch_to_oklab (output->values[0],
                          output->values[1],
                          output->values[2],
                          &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_OKLAB, v);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
convert_to_linear (GtkCssColor *output)
{
  float v[4];

  g_assert (output->color_space == GTK_CSS_COLOR_SPACE_SRGB ||
            output->color_space == GTK_CSS_COLOR_SPACE_SRGB_LINEAR ||
            output->color_space == GTK_CSS_COLOR_SPACE_OKLAB);

  if (output->color_space == GTK_CSS_COLOR_SPACE_SRGB)
    {
      gtk_rgb_to_linear_srgb (output->values[0],
                              output->values[1],
                              output->values[2],
                              &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_SRGB_LINEAR, v);
    }
}

static void
convert_from_linear (GtkCssColor      *output,
                     GtkCssColorSpace  dest)
{
  float v[4];

  g_assert (output->color_space == GTK_CSS_COLOR_SPACE_SRGB_LINEAR ||
            output->color_space == GTK_CSS_COLOR_SPACE_OKLAB);

  switch (dest)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_HSL:
    case GTK_CSS_COLOR_SPACE_HWB:
      gtk_linear_srgb_to_rgb (output->values[0],
                              output->values[1],
                              output->values[2],
                              &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_SRGB, v);
      break;

    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_OKLAB:
    case GTK_CSS_COLOR_SPACE_OKLCH:
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
convert_from_rectangular (GtkCssColor      *output,
                          GtkCssColorSpace  dest)
{
  float v[4];

  switch (dest)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_OKLAB:
      g_assert (output->color_space == dest);
      break;

    case GTK_CSS_COLOR_SPACE_HSL:
      g_assert (output->color_space == GTK_CSS_COLOR_SPACE_SRGB);
      gtk_rgb_to_hsl (output->values[0],
                      output->values[1],
                      output->values[2],
                      &v[0], &v[1], &v[2]);
      v[1] *= 100;
      v[2] *= 100;
      v[3] = output->values[3];

      gtk_css_color_init (output, dest, v);
      break;

    case GTK_CSS_COLOR_SPACE_HWB:
      g_assert (output->color_space == GTK_CSS_COLOR_SPACE_SRGB);
      gtk_rgb_to_hwb (output->values[0],
                      output->values[1],
                      output->values[2],
                      &v[0], &v[1], &v[2]);

      v[1] *= 100;
      v[2] *= 100;
      v[3] = output->values[3];

      gtk_css_color_init (output, dest, v);
      break;

    case GTK_CSS_COLOR_SPACE_OKLCH:
      g_assert (output->color_space == GTK_CSS_COLOR_SPACE_OKLAB);
      gtk_oklab_to_oklch (output->values[0],
                          output->values[1],
                          output->values[2],
                          &v[0], &v[1], &v[2]);
      v[3] = output->values[3];

      gtk_css_color_init (output, dest, v);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
convert_linear_to_linear (GtkCssColor      *output,
                          GtkCssColorSpace  dest)
{
  GtkCssColorSpace dest_linear;
  float v[4];

  if (dest == GTK_CSS_COLOR_SPACE_OKLCH)
    dest_linear = GTK_CSS_COLOR_SPACE_OKLAB;
  else
    dest_linear = GTK_CSS_COLOR_SPACE_SRGB_LINEAR;

  if (dest_linear == GTK_CSS_COLOR_SPACE_SRGB_LINEAR &&
      output->color_space == GTK_CSS_COLOR_SPACE_OKLAB)
    {
      gtk_oklab_to_linear_srgb (output->values[0],
                                output->values[1],
                                output->values[2],
                                &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, dest_linear, v);
    }
  else if (dest_linear == GTK_CSS_COLOR_SPACE_OKLAB &&
      output->color_space == GTK_CSS_COLOR_SPACE_SRGB_LINEAR)
    {
      gtk_linear_srgb_to_oklab (output->values[0],
                                output->values[1],
                                output->values[2],
                                &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, dest_linear, v);
    }

  g_assert (output->color_space == dest_linear);
}

/* See https://www.w3.org/TR/css-color-4/#color-conversion */
void
gtk_css_color_convert (const GtkCssColor *input,
                       GtkCssColorSpace   dest,
                       GtkCssColor       *output)
{
  gtk_css_color_init_from_color (output, input);

  convert_to_rectangular (output);
  convert_to_linear (output);

  /* FIXME: White point adaptation goes here */

  g_assert (output->color_space == GTK_CSS_COLOR_SPACE_SRGB_LINEAR ||
            output->color_space == GTK_CSS_COLOR_SPACE_OKLAB);

  convert_linear_to_linear (output, dest);
  convert_from_linear (output, dest);

  /* FIXME: Gamut mapping goes here */

  convert_from_rectangular (output, dest);
}

/* }}} */
/* {{{ Color interpolation */

static void
adjust_hue (float                  *h1,
            float                  *h2,
            GtkCssHueInterpolation  interp)
{

  switch (interp)
    {
    case GTK_CSS_HUE_INTERPOLATION_SHORTER:
      {
        float d = *h2 - *h1;

        if (d > 180)
          *h1 += 360;
        else if (d < -180)
          *h2 += 360;
      }
      break;

    case GTK_CSS_HUE_INTERPOLATION_LONGER:
      {
        float d = *h2 - *h1;

        if (0 < d && d < 180)
          *h1 += 360;
        else if (-180 < d && d <= 0)
          *h2 += 360;
      }
      break;

    case GTK_CSS_HUE_INTERPOLATION_INCREASING:
      if (*h2 < *h1)
        *h2 += 360;
      break;

    case GTK_CSS_HUE_INTERPOLATION_DECREASING:
      if (*h1 < *h2)
        *h1 += 360;
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
apply_hue_interpolation (GtkCssColor            *from,
                         GtkCssColor            *to,
                         GtkCssColorSpace        in,
                         GtkCssHueInterpolation  interp)
{
  switch (in)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_OKLAB:
      break;

    case GTK_CSS_COLOR_SPACE_HSL:
    case GTK_CSS_COLOR_SPACE_HWB:
      adjust_hue (&from->values[0], &to->values[0], interp);
      break;

    case GTK_CSS_COLOR_SPACE_OKLCH:
      adjust_hue (&from->values[2], &to->values[2], interp);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
normalize_hue_component (float *v)
{
  *v = fmod (*v, 360);
  if (*v < 0)
    *v += 360;
}

static void
normalize_hue (GtkCssColor *color)
{
  switch (color->color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_OKLAB:
      break;

    case GTK_CSS_COLOR_SPACE_HSL:
    case GTK_CSS_COLOR_SPACE_HWB:
      normalize_hue_component (&color->values[0]);
      break;

    case GTK_CSS_COLOR_SPACE_OKLCH:
      normalize_hue_component  (&color->values[2]);
      break;

    default:
      g_assert_not_reached ();
    }
}

static inline void
premultiply_component (GtkCssColor *color,
                       guint        i)
{
  if ((color->missing & (1 << i)) != 0)
    return;

  color->values[i] *= color->values[3];
}

static void
premultiply (GtkCssColor *color)
{
  if (color->missing & (1 << 3))
    return;

  switch (color->color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_OKLAB:
      premultiply_component (color, 0);
      premultiply_component (color, 1);
      premultiply_component (color, 2);
      break;

    case GTK_CSS_COLOR_SPACE_HSL:
    case GTK_CSS_COLOR_SPACE_HWB:
      premultiply_component (color, 1);
      premultiply_component (color, 2);
      break;

    case GTK_CSS_COLOR_SPACE_OKLCH:
      premultiply_component (color, 0);
      premultiply_component (color, 1);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
unpremultiply_component (GtkCssColor *color,
                         guint        i)
{
  if ((color->missing & (1 << i)) != 0)
    return;

  color->values[i] /= color->values[3];
}

static void
unpremultiply (GtkCssColor *color)
{
  if ((color->missing & (1 << 3)) != 0 || color->values[3] == 0)
    return;

  switch (color->color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_OKLAB:
      unpremultiply_component (color, 0);
      unpremultiply_component (color, 1);
      unpremultiply_component (color, 2);
      break;

    case GTK_CSS_COLOR_SPACE_HSL:
    case GTK_CSS_COLOR_SPACE_HWB:
      unpremultiply_component (color, 1);
      unpremultiply_component (color, 2);
      break;

    case GTK_CSS_COLOR_SPACE_OKLCH:
      unpremultiply_component (color, 0);
      unpremultiply_component (color, 1);
      break;

    default:
      g_assert_not_reached ();
    }
}

static void
collect_analogous_missing (const GtkCssColor *color,
                           GtkCssColorSpace   color_space,
                           gboolean           missing[4])
{
  /* Coords for red, green, blue, lightness, colorfulness, hue,
   * opposite a, opposite b, for each of our colorspaces
   */
  static int analogous[][8] = {
    {  0,  1,  2, -1, -1, -1, -1, -1 }, /* srgb */
    {  0,  1,  2, -1, -1, -1, -1, -1 }, /* srgb-linear */
    { -1, -1, -1,  2,  1,  0, -1, -1 }, /* hsl */
    { -1, -1, -1, -1, -1,  0, -1, -1 }, /* hwb */
    { -1, -1, -1,  0, -1, -1,  1,  2 }, /* oklab */
    { -1, -1, -1,  0,  1,  2, -1, -1 }, /* oklch */

  };

  int *src = analogous[color->color_space];
  int *dest = analogous[color_space];

  for (guint i = 0; i < 4; i++)
    missing[i] = 0;

  for (guint i = 0; i < 4; i++)
    {
      if ((color->missing & (1 << i)) == 0)
        continue;

      for (guint j = 0; j < 8; j++)
        {
          if (src[j] == i)
            {
              int idx = dest[j];

              if (idx != -1)
                missing[idx] = TRUE;

              break;
            }
        }
    }
}

/* See https://www.w3.org/TR/css-color-4/#interpolation */
void
gtk_css_color_interpolate (const GtkCssColor      *from,
                           const GtkCssColor      *to,
                           float                   progress,
                           GtkCssColorSpace        in,
                           GtkCssHueInterpolation  interp,
                           GtkCssColor            *output)
{
  GtkCssColor from1, to1;
  gboolean from_missing[4];
  gboolean to_missing[4];
  gboolean missing[4];
  float v[4];

  collect_analogous_missing (from, in, from_missing);
  collect_analogous_missing (to, in, to_missing);

  gtk_css_color_convert (from, in, &from1);
  gtk_css_color_convert (to, in, &to1);

  for (guint i = 0; i < 4; i++)
    {
      gboolean m1 = from_missing[i];
      gboolean m2 = to_missing[i];

      if (m1 && !m2)
        from1.values[i] = to1.values[1];
      else if (!m1 && m2)
        to1.values[i] = from1.values[1];

      missing[i] = from_missing[i] && to_missing[i];
    }

  apply_hue_interpolation (&from1, &to1, in, interp);

  premultiply (&from1);
  premultiply (&to1);

  v[0] = from1.values[0] * (1 - progress) + to1.values[0] * progress;
  v[1] = from1.values[1] * (1 - progress) + to1.values[1] * progress;
  v[2] = from1.values[2] * (1 - progress) + to1.values[2] * progress;
  v[3] = from1.values[3] * (1 - progress) + to1.values[3] * progress;

  gtk_css_color_init_with_missing (output, in, v, missing);

  normalize_hue (output);

  unpremultiply (output);
}

/* }}} */
/* vim:set foldmethod=marker expandtab: */
