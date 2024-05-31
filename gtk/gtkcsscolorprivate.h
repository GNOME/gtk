/* GTK - The GIMP Toolkit
 * Copyright (C) 2040 Red Hat, Inc.
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

#pragma once

#include <glib.h>
#include <math.h>

G_BEGIN_DECLS

typedef enum {
  GTK_CSS_COLOR_SPACE_SRGB,
  GTK_CSS_COLOR_SPACE_SRGB_LINEAR,
  GTK_CSS_COLOR_SPACE_HSL,
  GTK_CSS_COLOR_SPACE_HWB,
  GTK_CSS_COLOR_SPACE_OKLAB,
  GTK_CSS_COLOR_SPACE_OKLCH,
} GtkCssColorSpace;

typedef struct
{
  GtkCssColorSpace color_space;
  float values[4];
  guint missing;
} GtkCssColor;

static inline gboolean
gtk_css_color_equal (const GtkCssColor *color1,
                     const GtkCssColor *color2)
{
  return color1->color_space == color2->color_space &&
         color1->missing == color2->missing &&
         memcmp (color1->values, color2->values, sizeof (float) * 4) == 0;
}

static inline gboolean
gtk_css_color_component_missing (const GtkCssColor *color,
                                 guint              idx)
{
  return (color->missing & (1 << idx)) != 0;
}

static inline float
gtk_css_color_get_component (const GtkCssColor *color,
                             guint              idx)
{
  return color->values[idx];
}

static inline void
gtk_css_color_init_with_missing (GtkCssColor      *color,
                                 GtkCssColorSpace  color_space,
                                 const float       values[4],
                                 gboolean          missing[4])
{
  color->color_space = color_space;
  for (guint i = 0; i < 4; i++)
    color->values[i] = missing[i] ? 0 : values[i];
  color->missing = missing[0] | (missing[1] << 1) | (missing[2] << 2) | (missing[3] << 3);
}

static inline void
gtk_css_color_init_from_color (GtkCssColor       *color,
                               const GtkCssColor *src)
{
  memcpy (color, src, sizeof (GtkCssColor));
}

void    gtk_css_color_init      (GtkCssColor            *color,
                                 GtkCssColorSpace        color_space,
                                 const float             values[4]);

void    gtk_css_color_convert   (const GtkCssColor      *input,
                                 GtkCssColorSpace        dest,
                                 GtkCssColor            *output);

typedef enum
{
  GTK_CSS_HUE_INTERPOLATION_SHORTER,
  GTK_CSS_HUE_INTERPOLATION_LONGER,
  GTK_CSS_HUE_INTERPOLATION_INCREASING,
  GTK_CSS_HUE_INTERPOLATION_DECREASING,
} GtkCssHueInterpolation;

void    gtk_css_color_interpolate (const GtkCssColor      *from,
                                   const GtkCssColor      *to,
                                   float                   progress,
                                   GtkCssColorSpace        in,
                                   GtkCssHueInterpolation  interp,
                                   GtkCssColor            *output);

G_END_DECLS
