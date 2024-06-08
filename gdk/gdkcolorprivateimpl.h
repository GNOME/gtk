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

static inline void
gdk_color_init (GdkColor        *self,
                GdkColorState   *color_state,
                float            alpha,
                float            components[3])
{
  self->color_state = g_object_ref (color_state);
  self->alpha = alpha;
  if (components)
    memcpy (self->values, components, sizeof (float) * 3);
}

static inline void
gdk_color_init_from_rgba (GdkColor      *self,
                          const GdkRGBA *rgba)
{
  gdk_color_init (self,
                  gdk_color_state_get_srgb (),
                  rgba->alpha,
                  (float[3]) { rgba->red, rgba->green, rgba->blue });
}

static inline void
gdk_color_finish (GdkColor *self)
{
  g_object_unref (self->color_state);
}

static inline GdkColorState *
gdk_color_get_color_state (const GdkColor *self)
{
  return self->color_state;
}

static inline float
gdk_color_get_alpha (const GdkColor *self)
{
  return self->alpha;
}

static inline const float *
gdk_color_get_components (const GdkColor *self)
{
  return self->values;
}
