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

#include "gdkcolorprivate.h"
#include "gdkcolorstateprivate.h"
#include "gdkrgbaprivate.h"

/*< private >
 * GdkColor:
 * @color_state: the color state to interpret the values in
 * @values: the 3 coordinates that define the color, followed
 *   by the alpha value
 *
 * A `GdkColor` represents a color.
 *
 * The color state defines the meaning and range of the values.
 * E.g., the srgb color state has r, g, b components representing
 * red, green and blue with a range of [0,1], while the oklch color
 * state has l, c, h components representing luminosity, chromaticity
 * and hue, with l ranging from 0 to 1 and c from 0 to about 0.4, while
 * h is interpreted as angle in degrees.
 *
 * value[3] is always the alpha value with a range of [0,1].
 *
 * The values are also available under the names red, green, blue
 * and alpha, or r, g, b and a.
 */

/*< private >
 * gdk_color_init:
 * @self: the `GdkColor` struct to initialize
 * @color_state: the color state
 * @values: the values
 *
 * Initializes the `GdkColor` with the given color state
 * and values.
 *
 * Note that this takes a reference on @color_state that
 * must be freed by calling [function@Gdk.Color.finish]
 * when the `GdkColor` is no longer needed.
 */
void
(gdk_color_init) (GdkColor      *self,
                  GdkColorState *color_state,
                  const float    values[4])
{
  _gdk_color_init (self, color_state, values);
}

/*< private >
 * gdk_color_init_copy:
 * @self: the `GdkColor` struct to initialize
 * @color: the `GdkColor` to copy
 *
 * Initializes the `GdkColor` by copying the contents
 * of another `GdkColor`.
 *
 * Note that this takes a reference on the color state
 * that must be freed by calling [function@Gdk.Color.finish]
 * when the `GdkColor` is no longer needed.
 */
void
(gdk_color_init_copy) (GdkColor      *self,
                       const GdkColor *color)
{
  _gdk_color_init_copy (self, color);
}

/*< private >
 * gdk_color_init_from_rgba:
 * @self: the `GdkColor` struct to initialize
 * @rgba: the `GdkRGBA` to copy
 *
 * Initializes the `GdkColor` by copying the contents
 * of a `GdkRGBA`.
 *
 * Note that `GdkRGBA` colors are always in the sRGB
 * color state.
 *
 * Note that this takes a reference on the color state
 * that must be freed by calling [function@Gdk.Color.finish]
 * when the `GdkColor` is no longer needed.
 */
void
(gdk_color_init_from_rgba) (GdkColor       *self,
                            const GdkRGBA  *rgba)
{
  _gdk_color_init_from_rgba (self, rgba);
}

/*< private >
 * @self: a `GdkColor`
 *
 * Drop the reference on the color state of @self.
 *
 * After this, @self is empty and can be initialized again
 * with [function@Gdk.Color.init] and its variants.
 */
void
(gdk_color_finish) (GdkColor *self)
{
  _gdk_color_finish (self);
}

/*< private >
 * gdk_color_equal:
 * @self: a `GdkColor`
 * @other: another `GdkColor`
 *
 * Compares two `GdkColor` structs for equality.
 *
 * Returns: `TRUE` if @self and @other are equal
 */
gboolean
(gdk_color_equal) (const GdkColor *self,
                   const GdkColor *other)
{
  return _gdk_color_equal (self, other);
}

/*< private >
 * gdk_color_is_clear:
 * @self: a `GdkColor`
 *
 * Returns whether @self is fully transparent.
 *
 * Returns: `TRUE` if @self is transparent
 */
gboolean
(gdk_color_is_clear) (const GdkColor *self)
{
  return _gdk_color_is_clear (self);
}

/*< private >
 * gdk_color_is_opaque:
 * @self: a `GdkColor`
 *
 * Returns whether @self is fully opaque.
 *
 * Returns: `TRUE` if @self if opaque
 */
gboolean
(gdk_color_is_opaque) (const GdkColor *self)
{
  return _gdk_color_is_opaque (self);
}

/*< private >
 * gdk_color_convert:
 * @self: the `GdkColor` to store the result in
 * @color_state: the target color start
 * @other: the `GdkColor` to convert
 *
 * Converts a given `GdkColor` to another color state.
 *
 * After the conversion, @self will represent the same
 * color as @other in @color_state, to the degree possible.
 *
 * Different color states have different gamuts of colors
 * they can represent, and converting a color to a color
 * state with a smaller gamut may yield an 'out of gamut'
 * result.
 */
void
(gdk_color_convert) (GdkColor        *self,
                     GdkColorState   *color_state,
                     const GdkColor  *other)
{
  gdk_color_convert (self, color_state, other);
}

/*< private >
 * gdk_color_to_float:
 * @self: a `GdkColor`
 * @target: the color state to convert to
 * @values: the location to store the result in
 *
 * Converts a given `GdkColor to another color state
 * and stores the result in a `float[4]`.
 */
void
(gdk_color_to_float) (const GdkColor *self,
                      GdkColorState  *target,
                      float           values[4])
{
  gdk_color_to_float (self, target, values);
}

/*< private >
 * gdk_color_from_rgba:
 * @self: the `GdkColor` to store the result in
 * @color_state: the target color state
 * @rgba: the `GdkRGBA` to convert
 *
 * Converts a given `GdkRGBA` to the target @color_state.
 */
void
gdk_color_from_rgba (GdkColor        *self,
                     GdkColorState   *color_state,
                     const GdkRGBA   *rgba)
{
  GdkColor tmp = {
    .color_state = GDK_COLOR_STATE_SRGB,
    .r = rgba->red,
    .g = rgba->green,
    .b = rgba->blue,
    .a = rgba->alpha
  };

  gdk_color_convert (self, color_state, &tmp);
  gdk_color_finish (&tmp);
}

/*< private >
 * gdk_color_print:
 * @self: the `GdkColor` to print
 * @string: the string to print to
 *
 * Appends a representation of @self to @string.
 *
 * The representation is inspired by CSS3 colors,
 * but not 100% identical, and looks like this:
 *
 *     color(NAME R G B / A)
 *
 * where `NAME` identifies color state, and
 * `R`, `G`, `B` and `A` are the components of the color.
 *
 * The alpha may be omitted if it is 1.
 */
void
gdk_color_print (const GdkColor *self,
                 GString        *string)
{
  if (gdk_color_state_equal (self->color_state, GDK_COLOR_STATE_SRGB))
    {
      gdk_rgba_print ((const GdkRGBA *) self->values, string);
    }
  else
    {
      g_string_append_printf (string, "color(%s %g %g %g",
                              gdk_color_state_get_name (self->color_state),
                              self->r, self->g, self->b);
      if (self->a < 1)
        g_string_append_printf (string, " / %g", self->a);
      g_string_append_c (string, ')');
    }
}

/*< private >
 * gdk_color_print:
 * @self: the `GdkColor` to print
 *
 * Create a string representation of @self.
 *
 * See [method@Gdk.Color.print] for details about
 * the format.

 * Returns: (transfer full): a newly-allocated string
 */
char *
gdk_color_to_string (const GdkColor *self)
{
  GString *string = g_string_new ("");
  gdk_color_print (self, string);
  return g_string_free (string, FALSE);
}


