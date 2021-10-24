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

#include "config.h"

#include "gdkcolor.h"

#include "gdkcolorspaceprivate.h"
#include "gdkrgba.h"

/**
 * GdkColor:
 *
 * GdkColor is a high level description of a color for use in a color managed context.
 *
 * FIXME: Add stuff about colors in the real world and looking at `GdkColorSpace` for
 * how this is important.
 *
 * If you only want to use RGB colors, consider using `GdkRGBA` instead.
 */

#define GDK_IS_COLOR(self) ((self) != NULL)

struct _GdkColor
{
  GdkColorSpace *color_space;
  float components[]; /* alpha is last component, so that RGBA is in exactly that order */
};

G_DEFINE_BOXED_TYPE (GdkColor, gdk_color,
                     gdk_color_copy, gdk_color_free)

static GdkColor *
gdk_color_alloc (GdkColorSpace *space)
{
  gsize n_components = gdk_color_space_get_n_components (space);
  GdkColor *self;

  self = g_malloc (sizeof (GdkColor) + n_components * sizeof (float));

  self->color_space = g_object_ref (space);

  return self;
}

/**
 * gdk_color_new:
 * @space: the color space of the color
 * @alpha: the alpha value for this color
 * @components: (array length=n_components) (nullable): an array of
 *   component values for this color
 * @n_components: number of components
 *
 * Creates a new GdkColor representing the color for the given component
 * values in the given color space.
 *
 * The alpha value is independent of the component values (also known as
 * "unassociated" or "component values are not premultiplied").
 *
 * If the number of components passed is larger than the color space's
 * components, extra values will be discarded. If it is smaller, the
 * remaining components will be initialized as 0.
 *
 * Returns: a new GdkColor.
 **/
GdkColor *
gdk_color_new (GdkColorSpace *space,
               float          alpha,
               float         *components,
               gsize          n_components)
{
  gsize n_color_components;
  GdkColor *self;

  g_return_val_if_fail (GDK_IS_COLOR_SPACE (space), NULL);
  g_return_val_if_fail (components != NULL || n_components == 0, NULL);

  self = gdk_color_alloc (space);
  n_color_components = gdk_color_space_get_n_components (space);
  if (n_components)
    memcpy (self->components, components, MIN (n_components, n_color_components));
  if (n_components < n_color_components)
    memset (&self->components[n_components], 0, sizeof (float) * (n_color_components - n_components));
  self->components[n_color_components] = alpha;

  return self;
}

G_STATIC_ASSERT (sizeof (GdkRGBA) == 4 * sizeof (float));

/**
 * gdk_color_new_from_rgba:
 * @rgba: a GdkRGBA
 *
 * Creates a new GdkColor from the given GdkRGBA.
 *
 * Returns: a new GdkColor
 **/
GdkColor *
gdk_color_new_from_rgba (const GdkRGBA *rgba)
{
  GdkColor *self;

  g_return_val_if_fail (rgba != NULL, NULL);

  self = gdk_color_alloc (gdk_color_space_get_srgb ());
  memcpy (self->components, rgba, sizeof (GdkRGBA));

  return self;
}

/**
 * gdk_color_copy:
 * @self: a `GdkColor`
 *
 * Copies the color.
 *
 * Returns: a copy of the color.
 **/
GdkColor *
gdk_color_copy (const GdkColor *self)
{
  GdkColor *copy;

  copy = gdk_color_alloc (self->color_space);
  memcpy (copy->components,
          self->components,
          sizeof (float) * (gdk_color_space_get_n_components (self->color_space) + 1));
  
  return copy;
}

/**
 * gdk_color_free:
 * @self: a #GdkColor
 *
 * Frees a `GdkColor`.
 **/
void
gdk_color_free (GdkColor *self)
{
  g_object_unref (self->color_space);
  g_free (self);
}

/**
 * gdk_color_get_color_space:
 * @self: a `GdkColor`
 *
 * Returns the color space that this color is defined in.
 *
 * Returns: the color space this color is defined in
 **/
GdkColorSpace *
gdk_color_get_color_space (const GdkColor *self)
{
  g_return_val_if_fail (GDK_IS_COLOR (self), gdk_color_space_get_srgb ());

  return self->color_space;
}

/**
 * gdk_color_get_alpha:
 * @self: a `GdkColor`
 *
 * Gets the alpha value of this color. Alpha values range from 0.0
 * (fully translucent) to 1.0 (fully opaque).
 *
 * Returns: The alpha value
 **/
float
gdk_color_get_alpha (const GdkColor *self)
{
  g_return_val_if_fail (GDK_IS_COLOR (self), 1.f);

  return self->components[gdk_color_space_get_n_components (self->color_space)];
}

/**
 * gdk_color_get_components:
 * @self: a `GdkColor`
 *
 * Returns the array of component values for this color.
 *
 * For an RGB color, this will be an array of 3 values with intensities of
 * the red, green and blue components, respectively, in a range from 0 to 1.
 *
 * Other color spaces that are not RGB colors can have a different amount of
 * components with different meanings and different ranges.
 *
 * Returns: the component values
 **/
const float *
gdk_color_get_components (const GdkColor *self)
{
  g_return_val_if_fail (GDK_IS_COLOR (self), NULL);

  return self->components;
}

/**
 * gdk_color_get_n_components:
 * @self: a `GdkColor`
 *
 * Gets the number of components of this color. This will be the number of
 * components returned by gdk_color_get_components().
 *
 * Returns: the number of components
 **/
gsize
gdk_color_get_n_components (const GdkColor *self)
{
  g_return_val_if_fail (GDK_IS_COLOR (self), 3);

  return gdk_color_space_get_n_components (self->color_space);
}

/**
 * gdk_color_convert:
 * @self: a `GdkColor`
 * @space: the color space to convert to
 *
 * Converts a color into a different colorspace, potentially
 * tone-mapping it if it is out of gamut.
 *
 * Returns: a new color in the new colorspace.
 **/
GdkColor *
gdk_color_convert (const GdkColor *self,
                   GdkColorSpace  *space)
{
  GdkColor *result;
  gsize n_components;

  g_return_val_if_fail (GDK_IS_COLOR (self), NULL);
  g_return_val_if_fail (GDK_IS_COLOR_SPACE (space), NULL);

  result = gdk_color_alloc (space);
  n_components = gdk_color_space_get_n_components (self->color_space);
  result->components[n_components] = self->components[n_components];
  GDK_COLOR_SPACE_GET_CLASS (space)->convert_color (space, result->components, self);

  return result;
}

