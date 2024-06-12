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

#define gdk_color_init(self, cs, comp) _gdk_color_init ((self), (cs), (comp))
static inline void
_gdk_color_init (GdkColor        *self,
                 GdkColorState   *color_state,
                 const float      components[4])
{
  self->color_state = color_state;
  self->values[0] = components[0];
  self->values[1] = components[1];
  self->values[2] = components[2];
  self->values[3] = components[3];
}

#define gdk_color_init_from_rgb(self, rgba) _gdk_color_init_from_rgba (self, rgba)
static inline void
_gdk_color_init_from_rgba (GdkColor      *self,
                           const GdkRGBA *rgba)
{
  self->color_state = GDK_COLOR_STATE_SRGB;
  self->values[0] = rgba->red;
  self->values[1] = rgba->green;
  self->values[2] = rgba->blue;
  self->values[3] = rgba->alpha;
}

#define gdk_color_get_color_state(self) _gdk_color_get_color_state (self)
static inline GdkColorState *
_gdk_color_get_color_state (const GdkColor *self)
{
  return self->color_state;
}

#define gdk_color_get_components(self) _gdk_color_get_components (self)
static inline const float *
_gdk_color_get_components (const GdkColor *self)
{
  return self->values;
}

#define gdk_color_equal(self, other) _gdk_color_equal (self, other)
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

static inline gboolean
all_zero (const GdkColor *c)
{
  return c->values[0] == 0 && c->values[1] == 0 && c->values[2] == 0;
}

#define gdk_color_is_black(self) _gdk_color_is_black (self)
static inline gboolean
_gdk_color_is_black (const GdkColor *self)
{
  if (gdk_color_state_equal (self->color_state, GDK_COLOR_STATE_SRGB))
    return all_zero (self);
  else
    {
      GdkColor c;
      gboolean res;

      gdk_color_convert (&c, GDK_COLOR_STATE_SRGB, self);
      res = all_zero (&c);
      return res;
    }
}

#define gdk_color_is_clear(self) _gdk_color_is_clear (self)
static inline gboolean
_gdk_color_is_clear (const GdkColor *self)
{
  return self->values[3] < ((float) 0x00ff / (float) 0xffff);
}

#define gdk_color_is_opaque(self) _gdk_color_is_opaque (self)
static inline gboolean
_gdk_color_is_opaque (const GdkColor *self)
{
  return self->values[3] > ((float)0xff00 / (float)0xffff);
}

#define gdk_color_convert(self, cs, other) _gdk_color_convert (self, cs, other)
static inline void
_gdk_color_convert (GdkColor        *self,
                    GdkColorState   *color_state,
                    const GdkColor  *other)
{
  if (gdk_color_state_equal (color_state, other->color_state))
    {
      gdk_color_init (self, color_state, other->values);
      return;
    }

  (gdk_color_convert) (self, color_state, other);
}
