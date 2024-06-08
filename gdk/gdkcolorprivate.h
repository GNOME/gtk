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

#include <gdk/gdkcolorstate.h>
#include <gdk/gdkrgba.h>

typedef struct _GdkColor GdkColor;

struct _GdkColor
{
  GdkColorState *color_state;
  float alpha;
  float values[3];
};

G_STATIC_ASSERT (sizeof (GdkColor) % sizeof (gpointer) == 0);

static inline void              gdk_color_init                          (GdkColor               *self,
                                                                         GdkColorState          *color_state,
                                                                         float                   alpha,
                                                                         float                   components[3]);
static inline void              gdk_color_init_from_rgba                (GdkColor               *self,
                                                                         const GdkRGBA          *rgba);
static inline void              gdk_color_finish                        (GdkColor               *self);

void                            gdk_color_convert                       (GdkColor               *self,
                                                                         GdkColorState          *color_state,
                                                                         const GdkColor         *other);
void                            gdk_color_convert_rgba                  (GdkColor               *self,
                                                                         GdkColorState          *color_state,
                                                                         const GdkRGBA          *rgba);
void                            gdk_color_mix                           (GdkColor               *self,
                                                                         GdkColorState          *color_state,
                                                                         const GdkColor         *src1,
                                                                         const GdkColor         *src2,
                                                                         double                  progress);

static inline GdkColorState *   gdk_color_get_color_state               (const GdkColor         *self);
static inline float             gdk_color_get_alpha                     (const GdkColor         *self);
static inline const float *     gdk_color_get_components                (const GdkColor         *self);

#include "gdkcolorprivateimpl.h"

G_END_DECLS
