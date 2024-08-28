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

#include "gdkcolorstateprivate.h"


#define gdk_color_init(...) _gdk_color_init (__VA_ARGS__)
static inline void
_gdk_color_init (GdkColor        *self,
                 GdkColorState   *color_state,
                 const float      values[4])
{
  self->color_state = gdk_color_state_ref (color_state);
  memcpy (self->values, values, sizeof (float) * 4);
}

#define gdk_color_init_copy(self, color) _gdk_color_init_copy ((self), (color))
static inline void
_gdk_color_init_copy (GdkColor       *self,
                      const GdkColor *color)
{
  _gdk_color_init (self, color->color_state, color->values);
}

#define gdk_color_init_from_rgb(self, rgba) _gdk_color_init_from_rgba ((self), (rgba))
static inline void
_gdk_color_init_from_rgba (GdkColor      *self,
                           const GdkRGBA *rgba)
{
  _gdk_color_init (self, GDK_COLOR_STATE_SRGB, (const float *) rgba);
}

#define gdk_color_finish(self) _gdk_color_finish ((self))
static inline void
_gdk_color_finish (GdkColor *self)
{
  gdk_color_state_unref (self->color_state);
  self->color_state = NULL;
}

#define gdk_color_get_color_state(self) _gdk_color_get_color_state ((self))
static inline GdkColorState *
_gdk_color_get_color_state (const GdkColor *self)
{
  return self->color_state;
}

#define gdk_color_equal(self, other) _gdk_color_equal ((self), (other))
static inline gboolean
_gdk_color_equal (const GdkColor *self,
                  const GdkColor *other)
{
  return self->values[0] == other->values[0] &&
         self->values[1] == other->values[1] &&
         self->values[2] == other->values[2] &&
         self->values[3] == other->values[3] &&
         gdk_color_state_equal (self->color_state, other->color_state);
}

#define gdk_color_is_clear(self) _gdk_color_is_clear ((self))
static inline gboolean
_gdk_color_is_clear (const GdkColor *self)
{
  return self->alpha < (255.f / 65535.f);
}

#define gdk_color_is_opaque(self) _gdk_color_is_opaque ((self))
static inline gboolean
_gdk_color_is_opaque (const GdkColor *self)
{
  return self->alpha > (65280.f / 65535.f);
}

#define gdk_color_convert(self, cs, other) _gdk_color_convert ((self), (cs), (other))
static inline void
_gdk_color_convert (GdkColor        *self,
                    GdkColorState   *color_state,
                    const GdkColor  *other)
{
  if (gdk_color_state_equal (color_state, other->color_state))
    {
      gdk_color_init_copy (self, other);
      return;
    }

  self->color_state = gdk_color_state_ref (color_state);

  gdk_color_state_convert_color (other->color_state,
                                 other->values,
                                 self->color_state,
                                 self->values);
}

#define gdk_color_to_float(self, cs, values) _gdk_color_to_float ((self), (cs), (values))
static inline void
_gdk_color_to_float (const GdkColor  *self,
                     GdkColorState   *color_state,
                     float            values[4])
{
  if (gdk_color_state_equal (self->color_state, color_state))
    {
      values[0] = self->values[0];
      values[1] = self->values[1];
      values[2] = self->values[2];
      values[3] = self->values[3];
      return;
    }

  gdk_color_state_convert_color (self->color_state,
                                 self->values,
                                 color_state,
                                 values);
}

#define gdk_color_get_depth(self) _gdk_color_get_depth ((self))
static inline GdkMemoryDepth
_gdk_color_get_depth (const GdkColor *self)
{
  return gdk_color_state_get_depth (self->color_state);
}

