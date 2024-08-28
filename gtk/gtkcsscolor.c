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
#include "gdkcolorstateprivate.h"

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
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_XYZ:
    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
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
/* {{{ Utilities */

static inline void
append_color_component (GString           *string,
                        const GtkCssColor *color,
                        guint              idx)
{
  if (gtk_css_color_component_missing (color, idx))
    g_string_append (string, "none");
  else
    g_string_append_printf (string, "%g", gtk_css_color_get_component (color, idx));
}

static void
print_as_rgb (const GtkCssColor *color,
              GString           *string)
{
  GtkCssColor tmp;

  gtk_css_color_convert (color, GTK_CSS_COLOR_SPACE_SRGB, &tmp);
  if (tmp.values[3] > 0.999)
    {
      g_string_append_printf (string, "rgb(%d,%d,%d)",
                              (int)(0.5 + CLAMP (tmp.values[0], 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (tmp.values[1], 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (tmp.values[2], 0., 1.) * 255.));
    }
  else
    {
      char alpha[G_ASCII_DTOSTR_BUF_SIZE];

      g_ascii_formatd (alpha, G_ASCII_DTOSTR_BUF_SIZE, "%g", CLAMP (tmp.values[3], 0, 1));

      g_string_append_printf (string, "rgba(%d,%d,%d,%s)",
                              (int)(0.5 + CLAMP (tmp.values[0], 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (tmp.values[1], 0., 1.) * 255.),
                              (int)(0.5 + CLAMP (tmp.values[2], 0., 1.) * 255.),
                              alpha);
    }
}

GString *
gtk_css_color_print (const GtkCssColor *color,
                     gboolean           serialize_as_rgb,
                     GString           *string)
{
  GtkCssColorSpace print_color_space = color->color_space;
  GtkCssColor tmp;

  switch (color->color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_HSL:
    case GTK_CSS_COLOR_SPACE_HWB:
      if (serialize_as_rgb)
        {
          print_as_rgb (color, string);
          return string;
        }

      print_color_space = GTK_CSS_COLOR_SPACE_SRGB;
      g_string_append (string, "color(srgb ");
      break;

    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
      g_string_append (string, "color(srgb-linear ");
      break;

    case GTK_CSS_COLOR_SPACE_OKLAB:
      g_string_append (string, "oklab(");
      break;

    case GTK_CSS_COLOR_SPACE_OKLCH:
      g_string_append (string, "oklch(");
      break;

    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
      g_string_append (string, "color(display-p3 ");
      break;

    case GTK_CSS_COLOR_SPACE_XYZ:
      g_string_append (string, "color(xyz ");
      break;

    case GTK_CSS_COLOR_SPACE_REC2020:
      g_string_append (string, "color(rec2020 ");
      break;

    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
      g_string_append (string, "color(rec2100-pq ");
      break;

    default:
      g_assert_not_reached ();
    }

  if (print_color_space != color->color_space)
    gtk_css_color_convert (color, print_color_space, &tmp);
  else
    tmp = *color;

  for (guint i = 0; i < 3; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');
      append_color_component (string, &tmp, i);
    }

  if (gtk_css_color_component_missing (&tmp, 3) ||
      tmp.values[3] < 0.999)
    {
      g_string_append (string, " / ");
      append_color_component (string, &tmp, 3);
    }

  g_string_append_c (string, ')');

  return string;
}

char *
gtk_css_color_to_string (const GtkCssColor *color)
{
  return g_string_free (gtk_css_color_print (color, FALSE, g_string_new ("")), FALSE);
}

const char *
gtk_css_color_space_get_coord_name (GtkCssColorSpace color_space,
                                    guint            coord)
{
  if (coord == 3)
    return "alpha";

  switch (color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
      switch (coord)
        {
        case 0: return "r";
        case 1: return "g";
        case 2: return "b";
        default: g_assert_not_reached ();
        }
    case GTK_CSS_COLOR_SPACE_XYZ:
      switch (coord)
        {
        case 0: return "x";
        case 1: return "y";
        case 2: return "z";
        default: g_assert_not_reached ();
        }
    case GTK_CSS_COLOR_SPACE_HSL:
      switch (coord)
        {
        case 0: return "h";
        case 1: return "s";
        case 2: return "l";
        default: g_assert_not_reached ();
        }
    case GTK_CSS_COLOR_SPACE_HWB:
      switch (coord)
        {
        case 0: return "h";
        case 1: return "w";
        case 2: return "b";
        default: g_assert_not_reached ();
        }
    case GTK_CSS_COLOR_SPACE_OKLAB:
      switch (coord)
        {
        case 0: return "l";
        case 1: return "a";
        case 2: return "b";
        default: g_assert_not_reached ();
        }
    case GTK_CSS_COLOR_SPACE_OKLCH:
      switch (coord)
        {
        case 0: return "l";
        case 1: return "c";
        case 2: return "h";
        default: g_assert_not_reached ();
        }
    default:
      g_assert_not_reached ();
    }
}

void
gtk_css_color_space_get_coord_range (GtkCssColorSpace  color_space,
                                     gboolean          legacy_rgb_scale,
                                     guint             coord,
                                     float            *lower,
                                     float            *upper)
{
  if (coord == 3)
    {
      *lower = 0;
      *upper = 1;
      return;
    }

  switch (color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
      *lower = 0;
      *upper = legacy_rgb_scale ? 255 : 1;
      return;
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_XYZ:
    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
      *lower = 0;
      *upper = 1;
      return;
    case GTK_CSS_COLOR_SPACE_HSL:
    case GTK_CSS_COLOR_SPACE_HWB:
      switch (coord)
        {
        case 0: *lower = *upper = NAN; return;
        case 1:
        case 2: *lower = 0; *upper = 100; return;
        default: g_assert_not_reached ();
        }
    case GTK_CSS_COLOR_SPACE_OKLAB:
      switch (coord)
        {
        case 0: *lower = 0; *upper = 1; return;
        case 1:
        case 2: *lower = -0.4; *upper = 0.4; return;
        default: g_assert_not_reached ();
        }
    case GTK_CSS_COLOR_SPACE_OKLCH:
      switch (coord)
        {
        case 0: *lower = 0; *upper = 1; return;
        case 1: *lower = 0; *upper = 0.4; return;
        case 2: *lower = *upper = NAN; return;
        default: g_assert_not_reached ();
        }
    default:
      g_assert_not_reached ();
    }
}

static gboolean
color_space_is_polar (GtkCssColorSpace color_space)
{
  switch (color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_OKLAB:
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_XYZ:
    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
      return FALSE;

    case GTK_CSS_COLOR_SPACE_HSL:
    case GTK_CSS_COLOR_SPACE_HWB:
    case GTK_CSS_COLOR_SPACE_OKLCH:
      return TRUE;

    default:
      g_assert_not_reached ();
    }
}

/* }}} */
/* {{{ Color conversion */

static void
convert_to_rectangular (GtkCssColor *output)
{
  float v[4];
  gboolean no_missing[4] = { 0, };

  switch (output->color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
    case GTK_CSS_COLOR_SPACE_OKLAB:
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_XYZ:
    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
      break;

    case GTK_CSS_COLOR_SPACE_HSL:
      gtk_hsl_to_rgb (output->values[0],
                      output->values[1] / 100,
                      output->values[2] / 100,
                      &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init_with_missing (output, GTK_CSS_COLOR_SPACE_SRGB, v, no_missing);
      break;

    case GTK_CSS_COLOR_SPACE_HWB:
      gtk_hwb_to_rgb (output->values[0],
                      output->values[1] / 100,
                      output->values[2] / 100,
                      &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init_with_missing (output, GTK_CSS_COLOR_SPACE_SRGB, v, no_missing);
      break;

    case GTK_CSS_COLOR_SPACE_OKLCH:
      gtk_oklch_to_oklab (output->values[0],
                          output->values[1],
                          output->values[2],
                          &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init_with_missing (output, GTK_CSS_COLOR_SPACE_OKLAB, v, no_missing);
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
            output->color_space == GTK_CSS_COLOR_SPACE_OKLAB ||
            output->color_space == GTK_CSS_COLOR_SPACE_DISPLAY_P3 ||
            output->color_space == GTK_CSS_COLOR_SPACE_XYZ ||
            output->color_space == GTK_CSS_COLOR_SPACE_REC2020 ||
            output->color_space == GTK_CSS_COLOR_SPACE_REC2100_PQ);

  if (output->color_space == GTK_CSS_COLOR_SPACE_SRGB)
    {
      gtk_rgb_to_linear_srgb (output->values[0],
                              output->values[1],
                              output->values[2],
                              &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_SRGB_LINEAR, v);
    }
  else if (output->color_space == GTK_CSS_COLOR_SPACE_DISPLAY_P3)
    {
      gtk_p3_to_rgb (output->values[0],
                     output->values[1],
                     output->values[2],
                     &v[0], &v[1], &v[2]);
      gtk_rgb_to_linear_srgb (v[0], v[1], v[2],
                              &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_SRGB_LINEAR, v);
    }
  else if (output->color_space == GTK_CSS_COLOR_SPACE_XYZ)
    {
      gtk_xyz_to_linear_srgb (output->values[0],
                              output->values[1],
                              output->values[2],
                              &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_SRGB_LINEAR, v);
    }
  else if (output->color_space == GTK_CSS_COLOR_SPACE_REC2020)
    {
      gtk_rec2020_to_xyz (output->values[0],
                          output->values[1],
                          output->values[2],
                          &v[0], &v[1], &v[2]);
      gtk_xyz_to_linear_srgb (v[0], v[1], v[2],
                              &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_SRGB_LINEAR, v);
    }
  else if (output->color_space == GTK_CSS_COLOR_SPACE_REC2100_PQ)
    {
      gtk_rec2100_pq_to_rec2100_linear (output->values[0],
                                        output->values[1],
                                        output->values[2],
                                        &v[0], &v[1], &v[2]);
      gtk_rec2100_linear_to_rec2020_linear (v[0], v[1], v[2],
                                            &v[0], &v[1], &v[2]);
      gtk_rec2020_linear_to_xyz (v[0], v[1], v[2],
                                 &v[0], &v[1], &v[2]);
      gtk_xyz_to_linear_srgb (v[0], v[1], v[2],
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

    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
      gtk_linear_srgb_to_rgb (output->values[0],
                              output->values[1],
                              output->values[2],
                              &v[0], &v[1], &v[2]);
      gtk_rgb_to_p3 (v[0], v[1], v[2],
                     &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_DISPLAY_P3, v);
      break;

    case GTK_CSS_COLOR_SPACE_XYZ:
      gtk_linear_srgb_to_xyz (output->values[0],
                              output->values[1],
                              output->values[2],
                              &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_XYZ, v);
      break;

    case GTK_CSS_COLOR_SPACE_REC2020:
      gtk_linear_srgb_to_xyz (output->values[0],
                              output->values[1],
                              output->values[2],
                              &v[0], &v[1], &v[2]);
      gtk_xyz_to_rec2020 (v[0], v[1], v[2],
                          &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_REC2020, v);
      break;

    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
      gtk_linear_srgb_to_xyz (output->values[0],
                              output->values[1],
                              output->values[2],
                              &v[0], &v[1], &v[2]);
      gtk_xyz_to_rec2020_linear (v[0], v[1], v[2],
                                 &v[0], &v[1], &v[2]);
      gtk_rec2020_linear_to_rec2100_linear (v[0], v[1], v[2],
                                            &v[0], &v[1], &v[2]);
      gtk_rec2100_linear_to_rec2100_pq (v[0], v[1], v[2],
                                        &v[0], &v[1], &v[2]);
      v[3] = output->values[3];
      gtk_css_color_init (output, GTK_CSS_COLOR_SPACE_REC2100_PQ, v);
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
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_XYZ:
    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
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

  if (dest == GTK_CSS_COLOR_SPACE_OKLCH ||
      dest == GTK_CSS_COLOR_SPACE_OKLAB)
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
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_XYZ:
    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
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
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_XYZ:
    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
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
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_XYZ:
    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
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
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_XYZ:
    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
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
   * opposite a, opposite b, alpha, for each of our colorspaces,
   */
  static int analogous[][9] = {
    {  0,  1,  2, -1, -1, -1, -1, -1, 3 }, /* srgb */
    {  0,  1,  2, -1, -1, -1, -1, -1, 3 }, /* srgb-linear */
    { -1, -1, -1,  2,  1,  0, -1, -1, 3 }, /* hsl */
    { -1, -1, -1, -1, -1,  0, -1, -1, 3 }, /* hwb */
    { -1, -1, -1,  0, -1, -1,  1,  2, 3 }, /* oklab */
    { -1, -1, -1,  0,  1,  2, -1, -1, 3 }, /* oklch */
    {  0,  1,  2, -1, -1, -1, -1, -1, 3 }, /* display-p3 */
    {  0,  1,  2, -1, -1, -1, -1, -1, 3 }, /* xyz */
    {  0,  1,  2, -1, -1, -1, -1, -1, 3 }, /* rec2020 */
    {  0,  1,  2, -1, -1, -1, -1, -1, 3 }, /* rec2100-pq */
  };

  int *src = analogous[color->color_space];
  int *dest = analogous[color_space];

  for (guint i = 0; i < 4; i++)
    missing[i] = 0;

  for (guint i = 0; i < 4; i++)
    {
      if ((color->missing & (1 << i)) == 0)
        continue;

      for (guint j = 0; j < 9; j++)
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
        from1.values[i] = to1.values[i];
      else if (!m1 && m2)
        to1.values[i] = from1.values[i];

      missing[i] = from_missing[i] && to_missing[i];
    }

  from1.missing = 0;
  to1.missing = 0;

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

static gboolean
parse_hue_interpolation (GtkCssParser           *parser,
                         GtkCssHueInterpolation *interp)
{
  const GtkCssToken *token = gtk_css_parser_get_token (parser);

  if (gtk_css_token_is_ident (token, "shorter"))
    {
      gtk_css_parser_consume_token (parser);
      *interp = GTK_CSS_HUE_INTERPOLATION_SHORTER;
    }
  else if (gtk_css_token_is_ident (token, "longer"))
    {
      gtk_css_parser_consume_token (parser);
      *interp = GTK_CSS_HUE_INTERPOLATION_LONGER;
    }
  else if (gtk_css_token_is_ident (token, "increasing"))
    {
      gtk_css_parser_consume_token (parser);
      *interp = GTK_CSS_HUE_INTERPOLATION_INCREASING;
    }
  else if (gtk_css_token_is_ident (token, "decreasing"))
    {
      gtk_css_parser_consume_token (parser);
      *interp = GTK_CSS_HUE_INTERPOLATION_DECREASING;
    }
  else if (gtk_css_token_is_ident (token, "hue"))
    {
      gtk_css_parser_error_syntax (parser, "'hue' goes after the interpolation method");
      return FALSE;
    }
  else
    {
      *interp = GTK_CSS_HUE_INTERPOLATION_SHORTER;
      return TRUE;
    }

  if (!gtk_css_parser_try_ident (parser, "hue"))
    {
      gtk_css_parser_error_syntax (parser, "Expected 'hue'");
      return FALSE;
    }

  return TRUE;
}

gboolean
gtk_css_color_interpolation_method_can_parse (GtkCssParser *parser)
{
  return gtk_css_token_is_ident (gtk_css_parser_get_token (parser), "in");
}

gboolean
gtk_css_color_interpolation_method_parse (GtkCssParser           *parser,
                                          GtkCssColorSpace       *in,
                                          GtkCssHueInterpolation *interp)
{
  const GtkCssToken *token;

  if (!gtk_css_parser_try_ident (parser, "in"))
    {
      gtk_css_parser_error_syntax (parser, "Expected 'in'");
      return FALSE;
    }

  token = gtk_css_parser_get_token (parser);

  if (gtk_css_token_is_ident (token, "srgb"))
    {
      gtk_css_parser_consume_token (parser);
      *in = GTK_CSS_COLOR_SPACE_SRGB;
    }
  else if (gtk_css_token_is_ident (token, "srgb-linear"))
    {
      gtk_css_parser_consume_token (parser);
      *in = GTK_CSS_COLOR_SPACE_SRGB_LINEAR;
    }
  else if (gtk_css_token_is_ident (token, "hsl"))
    {
      gtk_css_parser_consume_token (parser);
      *in = GTK_CSS_COLOR_SPACE_HSL;
    }
  else if (gtk_css_token_is_ident (token, "hwb"))
    {
      gtk_css_parser_consume_token (parser);
      *in = GTK_CSS_COLOR_SPACE_HWB;
    }
  else if (gtk_css_token_is_ident (token, "oklab"))
    {
      gtk_css_parser_consume_token (parser);
      *in = GTK_CSS_COLOR_SPACE_OKLAB;
    }
  else if (gtk_css_token_is_ident (token, "oklch"))
    {
      gtk_css_parser_consume_token (parser);
      *in = GTK_CSS_COLOR_SPACE_OKLCH;
    }
  else if (gtk_css_token_is_ident (token, "display-p3"))
    {
      gtk_css_parser_consume_token (parser);
      *in = GTK_CSS_COLOR_SPACE_DISPLAY_P3;
    }
  else if (gtk_css_token_is_ident (token, "xyz"))
    {
      gtk_css_parser_consume_token (parser);
      *in = GTK_CSS_COLOR_SPACE_XYZ;
    }
  else if (gtk_css_token_is_ident (token, "rec2020"))
    {
      gtk_css_parser_consume_token (parser);
      *in = GTK_CSS_COLOR_SPACE_REC2020;
    }
  else if (gtk_css_token_is_ident (token, "rec2100-pq"))
    {
      gtk_css_parser_consume_token (parser);
      *in = GTK_CSS_COLOR_SPACE_REC2100_PQ;
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "Invalid color space: %s", gtk_css_token_to_string (token));
      return FALSE;
    }

  if (color_space_is_polar (*in))
    return parse_hue_interpolation (parser, interp);

  return TRUE;
}


void
gtk_css_color_interpolation_method_print (GtkCssColorSpace        in,
                                          GtkCssHueInterpolation  interp,
                                          GString                *string)
{
  g_string_append (string, "in ");

  switch (in)
  {
    case GTK_CSS_COLOR_SPACE_SRGB:
      g_string_append (string, "srgb");
      break;
    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
      g_string_append (string, "srgb-linear");
      break;
    case GTK_CSS_COLOR_SPACE_HSL:
      g_string_append (string, "hsl");
      break;
    case GTK_CSS_COLOR_SPACE_HWB:
      g_string_append (string, "hwb");
      break;
    case GTK_CSS_COLOR_SPACE_OKLCH:
      g_string_append (string, "oklch");
      break;
    case GTK_CSS_COLOR_SPACE_OKLAB:
      g_string_append (string, "oklab");
      break;
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
      g_string_append (string, "display-p3");
      break;
    case GTK_CSS_COLOR_SPACE_XYZ:
      g_string_append (string, "xyz");
      break;
    case GTK_CSS_COLOR_SPACE_REC2020:
      g_string_append (string, "rec2020");
      break;
    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
      g_string_append (string, "rec2100-pq");
      break;
    default:
      g_assert_not_reached ();
  }

  if (!color_space_is_polar (in))
    return;

  switch (interp)
  {
    case GTK_CSS_HUE_INTERPOLATION_SHORTER:
      /* shorter is the default mode, don't print it */
      break;
    case GTK_CSS_HUE_INTERPOLATION_LONGER:
      g_string_append (string, " longer hue");
      break;
    case GTK_CSS_HUE_INTERPOLATION_INCREASING:
      g_string_append (string, " increasing hue");
      break;
    case GTK_CSS_HUE_INTERPOLATION_DECREASING:
      g_string_append (string, " decreasing hue");
      break;
    default:
      g_assert_not_reached ();
  }
}

/* }}} */
/* {{{ GdkColor conversion */

void
gtk_css_color_to_color (const GtkCssColor *css,
                        GdkColor          *color)
{
  switch (css->color_space)
    {
    case GTK_CSS_COLOR_SPACE_SRGB:
      gdk_color_init (color, GDK_COLOR_STATE_SRGB, css->values);
      break;

    case GTK_CSS_COLOR_SPACE_SRGB_LINEAR:
      gdk_color_init (color, GDK_COLOR_STATE_SRGB_LINEAR, css->values);
      break;

    case GTK_CSS_COLOR_SPACE_REC2100_PQ:
      gdk_color_init (color, GDK_COLOR_STATE_REC2100_PQ, css->values);
      break;

    case GTK_CSS_COLOR_SPACE_HSL:
    case GTK_CSS_COLOR_SPACE_HWB:
    case GTK_CSS_COLOR_SPACE_OKLAB:
    case GTK_CSS_COLOR_SPACE_OKLCH:
      {
        GtkCssColor tmp;
        gtk_css_color_convert (css, GTK_CSS_COLOR_SPACE_SRGB, &tmp);
        gdk_color_init (color, GDK_COLOR_STATE_SRGB, tmp.values);
      }
      break;

    case GTK_CSS_COLOR_SPACE_REC2020:
    case GTK_CSS_COLOR_SPACE_DISPLAY_P3:
    case GTK_CSS_COLOR_SPACE_XYZ:
      {
        GtkCssColor tmp;
        gtk_css_color_convert (css, GTK_CSS_COLOR_SPACE_REC2100_PQ, &tmp);
        gdk_color_init (color, GDK_COLOR_STATE_REC2100_PQ, tmp.values);
      }
      break;

    default:
      g_assert_not_reached ();
    }
}

/* }}} */
/* vim:set foldmethod=marker expandtab: */
