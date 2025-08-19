/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2025 Red Hat, Inc.
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

#include "config.h"

#include <stdint.h>
#include <glib.h>

#define GDK_FRACTIONAL_SCALE_FACTOR 120

typedef struct _GdkFractionalScale GdkFractionalScale;

struct _GdkFractionalScale
{
  uint32_t scale;
};

#define GDK_FRACTIONAL_SCALE_INIT(fractional_scale) (GdkFractionalScale) { fractional_scale }
#define GDK_FRACTIONAL_SCALE_INIT_INT(scale) GDK_FRACTIONAL_SCALE_INIT (scale * GDK_FRACTIONAL_SCALE_FACTOR)

static inline int
gdk_fractional_scale_to_int (const GdkFractionalScale *self)
{
  /* ceil() */
  return (self->scale + GDK_FRACTIONAL_SCALE_FACTOR - 1) / GDK_FRACTIONAL_SCALE_FACTOR;
}

static inline double
gdk_fractional_scale_to_double (const GdkFractionalScale *self)
{
  return (double) self->scale / GDK_FRACTIONAL_SCALE_FACTOR;
}

static inline int
gdk_fractional_scale_scale (const GdkFractionalScale *self,
                            int                       value)
{
  return (value * self->scale + GDK_FRACTIONAL_SCALE_FACTOR / 2) / GDK_FRACTIONAL_SCALE_FACTOR;
}

static inline gboolean
gdk_fractional_scale_equal (const GdkFractionalScale *a,
                            const GdkFractionalScale *b)
{
  return a->scale == b->scale;
}
