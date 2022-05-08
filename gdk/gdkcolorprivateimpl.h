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

static inline gboolean
gdk_color_is_allocated (const GdkColor *self)
{
  return GPOINTER_TO_SIZE (self->profile) & 1;
}

static inline void
gdk_color_init (GdkColor        *self,
                GdkColorProfile *profile,
                float            alpha,
                float           *components,
                gsize            n_components)
{
  gboolean allocated = n_components > GDK_COLOR_MAX_NATIVE_COMPONENTS;

  g_assert (n_components == gdk_color_profile_get_n_components (profile));

  self->profile = GSIZE_TO_POINTER (GPOINTER_TO_SIZE (g_object_ref (profile)) | allocated);
  self->alpha = alpha;
  if (allocated)
    {
      if (components)
        self->components = g_memdup (components, sizeof (float) * n_components);
      else
        self->components = g_new (float, n_components);
    }
  else
    {
      if (components)
        memcpy (self->values, components, sizeof (float) * n_components);
    }
}

static inline void
gdk_color_init_from_rgba (GdkColor      *self,
                          const GdkRGBA *rgba)
{
  gdk_color_init (self,
                  gdk_color_profile_get_srgb(),
                  rgba->alpha,
                  (float[3]) { rgba->red, rgba->green, rgba->blue },
                  3);
}

static inline void
gdk_color_finish (GdkColor *self)
{
  if (gdk_color_is_allocated (self))
    g_free (self->components);

  g_object_unref (gdk_color_get_color_profile (self));
}

static inline GdkColorProfile *
gdk_color_get_color_profile (const GdkColor *self)
{
  return GSIZE_TO_POINTER (GPOINTER_TO_SIZE (self->profile) & ~1); 
}

static inline float
gdk_color_get_alpha (const GdkColor *self)
{
  return self->alpha;
}

static inline const float *
gdk_color_get_components (const GdkColor *self)
{
  if (gdk_color_is_allocated (self))
    return self->components;
  else
    return self->values;
}

static inline gsize
gdk_color_get_n_components (const GdkColor *self)
{
  return gdk_color_profile_get_n_components (gdk_color_get_color_profile (self));
}
