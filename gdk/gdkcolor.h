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


/* The interpretation of the first 3 components depends on the color state.
 * values[3] is always alpha.
 */
struct _GdkColor
{
  GdkColorState *color_state;
  float values[4];
};

G_STATIC_ASSERT (sizeof (GdkColor) % sizeof (gpointer) == 0);

#define GDK_TYPE_COLOR (gdk_color_get_type ())

G_BEGIN_DECLS

#define GDK_COLOR_INIT(cs, c1, c2, c3, c4) \
  (GdkColor) { (cs), { (c1), (c2), (c3), (c4) } }

#define GDK_COLOR_INIT_SRGB(c1, c2, c3, c4) \
  GDK_COLOR_INIT (GDK_COLOR_STATE_SRGB, (c1), (c2), (c3), (c4) )

#define GDK_COLOR_INIT_RGBA(r) \
  GDK_COLOR_INIT_SRGB ((r)->red, (r)->green, (r)->blue, (r)->alpha)

GDK_AVAILABLE_IN_4_16
GType             gdk_color_get_type            (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_4_16
GdkColor *        gdk_color_copy                (const GdkColor         *self);
GDK_AVAILABLE_IN_4_16
void              gdk_color_free                (GdkColor               *self);

GDK_AVAILABLE_IN_4_16
void              gdk_color_init                (GdkColor               *self,
                                                 GdkColorState          *color_state,
                                                 const float             components[4]);
GDK_AVAILABLE_IN_4_16
void              gdk_color_init_from_rgba      (GdkColor               *self,
                                                 const GdkRGBA          *rgba);

GDK_AVAILABLE_IN_4_16
void              gdk_color_convert             (GdkColor               *self,
                                                 GdkColorState          *color_state,
                                                 const GdkColor         *other);
GDK_AVAILABLE_IN_4_16
void              gdk_color_convert_rgba        (GdkColor               *self,
                                                 GdkColorState          *color_state,
                                                 const GdkRGBA          *rgba);
GDK_AVAILABLE_IN_4_16
void              gdk_color_mix                 (GdkColor               *self,
                                                 GdkColorState          *color_state,
                                                 const GdkColor         *src1,
                                                 const GdkColor         *src2,
                                                 double                  progress);

GDK_AVAILABLE_IN_4_16
void              gdk_color_print               (const GdkColor         *self,
                                                 GString                *string);

GDK_AVAILABLE_IN_4_16
char *            gdk_color_to_string           (const GdkColor         *self);

GDK_AVAILABLE_IN_4_16
GdkColorState *   gdk_color_get_color_state     (const GdkColor         *self);

GDK_AVAILABLE_IN_4_16
const float *     gdk_color_get_components      (const GdkColor         *self);

GDK_AVAILABLE_IN_4_16
gboolean          gdk_color_equal               (const GdkColor         *color1,
                                                 const GdkColor         *color2);
GDK_AVAILABLE_IN_4_16
gboolean          gdk_color_is_black            (const GdkColor         *self);
GDK_AVAILABLE_IN_4_16
gboolean          gdk_color_is_clear            (const GdkColor         *self);
GDK_AVAILABLE_IN_4_16
gboolean          gdk_color_is_opaque           (const GdkColor         *self);

#include "gdkcolorimpl.h"

G_END_DECLS
