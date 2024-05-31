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
                          output->values[1] / 100,
                          output->values[2] / 100,
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

/* vim:set foldmethod=marker expandtab: */
