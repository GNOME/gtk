/* GDK - The GIMP Drawing Kit
 *
 * Copyright (C) 2021 Benjamin Otte
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

#include <gdk/gdktypes.h>
#include <gdk/gdkcolorstate.h>
#include <gdk/gdkrgba.h>
#include <gdk/gdkmemoryformatprivate.h>


typedef struct _GdkColor GdkColor;

/* The interpretation of the first 3 components depends on the color state.
 * values[3] is always alpha.
 */
struct _GdkColor
{
  GdkColorState *color_state;
  union {
    float values[4];
    struct {
      float r;
      float g;
      float b;
      float a;
    };
    struct {
      float red;
      float green;
      float blue;
      float alpha;
    };
  };
};

G_STATIC_ASSERT (G_STRUCT_OFFSET (GdkColor, r) == G_STRUCT_OFFSET (GdkColor, red));
G_STATIC_ASSERT (G_STRUCT_OFFSET (GdkColor, g) == G_STRUCT_OFFSET (GdkColor, green));
G_STATIC_ASSERT (G_STRUCT_OFFSET (GdkColor, b) == G_STRUCT_OFFSET (GdkColor, blue));
G_STATIC_ASSERT (G_STRUCT_OFFSET (GdkColor, a) == G_STRUCT_OFFSET (GdkColor, alpha));

/* The committee notes that since all known implementations but one "get it right"
 * this may well not be a defect at all.
 * https://open-std.org/JTC1/SC22/WG14/www/docs/n2396.htm#dr_496
 */
#ifndef _MSC_VER
G_STATIC_ASSERT (G_STRUCT_OFFSET (GdkColor, r) == G_STRUCT_OFFSET (GdkColor, values[0]));
G_STATIC_ASSERT (G_STRUCT_OFFSET (GdkColor, g) == G_STRUCT_OFFSET (GdkColor, values[1]));
G_STATIC_ASSERT (G_STRUCT_OFFSET (GdkColor, b) == G_STRUCT_OFFSET (GdkColor, values[2]));
G_STATIC_ASSERT (G_STRUCT_OFFSET (GdkColor, a) == G_STRUCT_OFFSET (GdkColor, values[3]));
#endif

#define GDK_COLOR_SRGB(r,g,b,a) (GdkColor) { \
  .color_state = GDK_COLOR_STATE_SRGB, \
  .values = { (r), (g), (b), (a) } \
}

void              gdk_color_init                (GdkColor               *self,
                                                 GdkColorState          *color_state,
                                                 const float             values[4]);
void              gdk_color_init_copy           (GdkColor               *self,
                                                 const GdkColor         *color);
void              gdk_color_init_from_rgba      (GdkColor               *self,
                                                 const GdkRGBA          *rgba);
void              gdk_color_finish              (GdkColor               *self);

gboolean          gdk_color_equal               (const GdkColor         *self,
                                                 const GdkColor         *other);
gboolean          gdk_color_is_clear            (const GdkColor         *self);
gboolean          gdk_color_is_opaque           (const GdkColor         *self);

void              gdk_color_convert             (GdkColor               *self,
                                                 GdkColorState          *color_state,
                                                 const GdkColor         *other);

void              gdk_color_to_float            (const GdkColor         *self,
                                                 GdkColorState          *target,
                                                 float                   values[4]);

void              gdk_color_from_rgba           (GdkColor               *self,
                                                 GdkColorState          *color_state,
                                                 const GdkRGBA          *rgba);

void              gdk_color_print               (const GdkColor         *self,
                                                 GString                *string);

char *            gdk_color_to_string           (const GdkColor         *self);

#include "gdkcolorimpl.h"

G_END_DECLS
