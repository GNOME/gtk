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

#ifndef __GDK_COLOR_PRIVATE_H__
#define __GDK_COLOR_PRIVATE_H__

#include <gdk/gdkcolorspace.h>
#include <gdk/gdkrgba.h>

/* RGB - and it makes the struct size a multiple of 8 bytes, ie pointer-aligned */
#define GDK_COLOR_MAX_NATIVE_COMPONENTS 3

typedef struct _GdkColor GdkColor;

struct _GdkColor
{
  GdkColorSpace *color_space;
  float alpha;
  union {
    float values[GDK_COLOR_MAX_NATIVE_COMPONENTS];
    float *components;
  };
};

G_STATIC_ASSERT (sizeof (GdkColor) % sizeof (gpointer) == 0);

static inline void              gdk_color_init                          (GdkColor               *self,
                                                                         GdkColorSpace          *color_space,
                                                                         float                   alpha,
                                                                         float                  *components,
                                                                         gsize                   n_components);
static inline void              gdk_color_init_from_rgba                (GdkColor               *self,
                                                                         const GdkRGBA          *rgba);
static inline void              gdk_color_finish                        (GdkColor               *self);

void                            gdk_color_convert                       (GdkColor               *self,
                                                                         GdkColorSpace          *color_space,
                                                                         const GdkColor         *other);
void                            gdk_color_convert_rgba                  (GdkColor               *self,
                                                                         GdkColorSpace          *color_space,
                                                                         const GdkRGBA          *rgba);

void                            gdk_color_mix                           (GdkColor               *self,
                                                                         GdkColorSpace          *color_space,
                                                                         const GdkColor         *src1,
                                                                         const GdkColor         *src2,
                                                                         double                  progress);

static inline GdkColorSpace *   gdk_color_get_color_space               (const GdkColor         *self);
static inline float             gdk_color_get_alpha                     (const GdkColor         *self);
static inline const float *     gdk_color_get_components                (const GdkColor         *self);
static inline gsize             gdk_color_get_n_components              (const GdkColor         *self);

#include "gdkcolorprivateimpl.h"

G_END_DECLS

#endif
