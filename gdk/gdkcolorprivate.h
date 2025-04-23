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

#include <gdk/gdkcolor.h>

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

#include "gdkcolorimpl.h"
