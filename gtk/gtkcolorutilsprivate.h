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

#pragma once

#include "gtkcolorutils.h"

G_BEGIN_DECLS

void gtk_rgb_to_hsl (float  red, float  green,      float  blue,
                     float *hue, float *saturation, float *lightness);
void gtk_hsl_to_rgb (float  hue, float  saturation, float  lightness,
                     float *red, float *green,      float *blue);

void gtk_rgb_to_hwb (float  red, float  green, float  blue,
                     float *hue, float *white, float *black);
void gtk_hwb_to_rgb (float  hue, float  white, float  black,
                     float *red, float *green, float *blue);

void gtk_oklab_to_oklch (float  L,  float  a, float  b,
                         float *L2, float *C, float *H);
void gtk_oklch_to_oklab (float  L,  float  C, float  H,
                         float *L2, float *a, float *b);

void gtk_oklab_to_linear_srgb (float  L,   float  a,     float  b,
                               float *red, float *green, float *blue);
void gtk_linear_srgb_to_oklab (float  red, float  green, float  blue,
                               float *L,   float *a,     float *b);

void gtk_oklab_to_rgb (float  L,   float  a,     float  b,
                       float *red, float *green, float *blue);
void gtk_rgb_to_oklab (float  red, float  green, float  blue,
                       float *L,   float *a,     float *b);

void gtk_rgb_to_linear_srgb (float  red,        float  green,        float  blue,
                             float *linear_red, float *linear_green, float *linear_blue);
void gtk_linear_srgb_to_rgb (float  linear_red, float  linear_green, float  linear_blue,
                             float *red,        float *green,        float *blue);


G_END_DECLS
