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

G_BEGIN_DECLS

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

GDK_AVAILABLE_IN_4_20
void              gdk_color_init                (GdkColor               *self,
                                                 GdkColorState          *color_state,
                                                 const float             values[4]);
GDK_AVAILABLE_IN_4_20
void              gdk_color_init_copy           (GdkColor               *self,
                                                 const GdkColor         *color);
GDK_AVAILABLE_IN_4_20
void              gdk_color_init_from_rgba      (GdkColor               *self,
                                                 const GdkRGBA          *rgba);
GDK_AVAILABLE_IN_4_20
void              gdk_color_finish              (GdkColor               *self);

GDK_AVAILABLE_IN_4_20
gboolean          gdk_color_equal               (const GdkColor         *self,
                                                 const GdkColor         *other);
GDK_AVAILABLE_IN_4_20
gboolean          gdk_color_is_clear            (const GdkColor         *self);
GDK_AVAILABLE_IN_4_20
gboolean          gdk_color_is_opaque           (const GdkColor         *self);

GDK_AVAILABLE_IN_4_20
void              gdk_color_convert             (GdkColor               *self,
                                                 GdkColorState          *color_state,
                                                 const GdkColor         *other);

GDK_AVAILABLE_IN_4_20
void              gdk_color_to_float            (const GdkColor         *self,
                                                 GdkColorState          *target,
                                                 float                   values[4]);

GDK_AVAILABLE_IN_4_20
void              gdk_color_from_rgba           (GdkColor               *self,
                                                 GdkColorState          *color_state,
                                                 const GdkRGBA          *rgba);

GDK_AVAILABLE_IN_4_20
void              gdk_color_print               (const GdkColor         *self,
                                                 GString                *string);

GDK_AVAILABLE_IN_4_20
char *            gdk_color_to_string           (const GdkColor         *self);

G_END_DECLS
