/* gdkcolorstate.c
 *
 * Copyright 2024 Matthias Clasen
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

#include "gdkcolorstateprivate.h"

#include <glib/gi18n-lib.h>

/**
 * GdkColorState:
 *
 * A `GdkColorState` object provides the information to interpret
 * colors and pixels in a variety of ways.
 *
 * They are also known as
 * [*color spaces*](https://en.wikipedia.org/wiki/Color_space).
 *
 * Crucially, GTK knows how to convert colors from one color
 * state to another.
 *
 * `GdkColorState objects are immutable and therefore threadsafe.
 *
 * Since 4.16
 */

G_DEFINE_BOXED_TYPE (GdkColorState, gdk_color_state,
                     gdk_color_state_ref, gdk_color_state_unref);

/* {{{ Public API */

/**
 * gdk_color_state_ref:
 * @self: a `GdkColorState`
 *
 * Increase the reference count of @self.
 *
 * Returns: the object that was passed in
 * 
 * Since: 4.16
 */
GdkColorState *
(gdk_color_state_ref) (GdkColorState *self)
{
  return _gdk_color_state_ref (self);
}

/**
 * gdk_color_state_unref:
 * @self:a `GdkColorState`
 *
 * Decrease the reference count of @self.
 *
 * Unless @self is static, it will be freed
 * when the reference count reaches zero.
 *
 * Since: 4.16
 */
void
(gdk_color_state_unref) (GdkColorState *self)
{
  _gdk_color_state_unref (self);
}

/**
 * gdk_color_state_get_srgb:
 *
 * Returns the color state object representing the sRGB color space.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_srgb (void)
{
  return GDK_COLOR_STATE_SRGB;
}

/**
 * gdk_color_state_get_srgb_linear:
 *
 * Returns the color state object representing the linearized sRGB color space.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_srgb_linear (void)
{
  return GDK_COLOR_STATE_SRGB_LINEAR;
}

/**
 * gdk_color_state_equal:
 * @self: a `GdkColorState`
 * @other: another `GdkColorStatee`
 *
 * Compares two `GdkColorStates` for equality.
 *
 * Note that this function is not guaranteed to be perfect and two objects
 * describing the same color state may compare not equal. However, different
 * color states will never compare equal.
 *
 * Returns: %TRUE if the two color states compare equal
 *
 * Since: 4.16
 */
gboolean
(gdk_color_state_equal) (GdkColorState *self,
                         GdkColorState *other)
{
  return _gdk_color_state_equal (self, other);
}

/*< private >
 * gdk_color_state_new_from_icc_profile:
 * @icc_profile: The ICC profiles given as a `GBytes`
 * @error: Return location for an error
 *
 * Creates a new color state for the given ICC profile data.
 *
 * If the given ICC profile can not be represented as a
 * [struct@Gdk.ColorState], `NULL` is returned and an error
 * is raised.
 *
 * Returns: a new `GdkColorState` or %NULL on error
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_new_from_icc_profile (GBytes  *icc_profile,
                                      GError **error)
{
  GBytes *bytes;

  g_return_val_if_fail (icc_profile != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  bytes = g_resources_lookup_data ("/org/gtk/libgdk/icc/srgb.icc", 0, NULL);
  if (g_bytes_equal (icc_profile, bytes))
    {
      g_bytes_unref (bytes);
      return GDK_COLOR_STATE_SRGB;
    }

  bytes = g_resources_lookup_data ("/org/gtk/libgdk/icc/srgb-linear.icc", 0, NULL);
  if (g_bytes_equal (icc_profile, bytes))
    {
      g_bytes_unref (bytes);
      return GDK_COLOR_STATE_SRGB_LINEAR;
    }

  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                       _("Failed to load ICC profile"));
  return NULL;
}

/*< private >
 * gdk_color_state_new_from_cicp_data:
 * @color_primaries: the color primaries
 * @transfer_characteristics: the transfer function
 * @matrix_coefficients: the color matrix
 * @full_range: whether the data is 'full-range'
 * @error: Return location for an error
 *
 * Creates a new color state for the given CICP data.
 *
 * If the given CICP data can not be represented as a
 * [struct@Gdk.ColorState], `NULL` is returned and an error
 * is raised.
 *
 * See [CICP](https://en.wikipedia.org/wiki/Coding-independent_code_points)
 * for more information about CICP.
 *
 * Returns: a new `GdkColorState` or %NULL on error
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_new_from_cicp_data (int      color_primaries,
                                    int      transfer_characteristics,
                                    int      matrix_coefficients,
                                    gboolean full_range)
{
  if (color_primaries == 0 && transfer_characteristics == 13 &&
      matrix_coefficients == 0 && full_range)
    return GDK_COLOR_STATE_SRGB;
  else if (color_primaries == 0 && transfer_characteristics == 8 &&
           matrix_coefficients == 0 && full_range)
    return GDK_COLOR_STATE_SRGB_LINEAR;

  return NULL;
}

/*< private >
 * gdk_color_state_save_to_icc_profile:
 * @self: a `GdkColorState`
 * @error: Return location for an error
 *
 * Saves the color state to an
 * [ICC profile](https://en.wikipedia.org/wiki/ICC_profile).
 *
 * It may not be possible to represent a color state as ICC profile.
 * In that case, @error will be set and %NULL will be returned.
 *
 * Returns: (nullable): A new `GBytes` containing the ICC profile
 *
 * Since: 4.16
 */
GBytes *
gdk_color_state_save_to_icc_profile (GdkColorState  *self,
                                     GError        **error)
{
  GBytes *bytes;

  if (self == GDK_COLOR_STATE_SRGB)
    {
      bytes = g_resources_lookup_data ("/org/gtk/libgdk/icc/srgb.icc", 0, NULL);
      g_assert (bytes != NULL);
    }
  else if (self == GDK_COLOR_STATE_SRGB_LINEAR)
    {
      bytes = g_resources_lookup_data ("/org/gtk/libgdk/icc/srgb-linear.icc", 0, NULL);
      g_assert (bytes != NULL);
    }
  else
    {
      bytes = NULL;
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           _("ICC profile not supported for this color state"));
    }

  return bytes;
}

/*< private >
 * gdk_color_state_save_to_cicp_data:
 * @self: a `GdkColorState`
 * @color_primaries: return location for color primaries
 * @transfer_characteristics: return location for transfer characteristics
 * @matrix_coefficients: return location for matrix_coefficients
 * @full_range: return location for the full range flag
 * @error: Return location for an error
 *
 * Saves the color state as CICP data.
 *
 * It may not be possible to represent a color state as CICP data.
 * In that case, @error will be set and `FALSE` will be returned.
 *
 * See [CICP](https://en.wikipedia.org/wiki/Coding-independent_code_points)
 * for more information about CICP.
 *
 * Returns: (nullable): `TRUE` if the out arguments were set
 *
 * Since: 4.16
 */
gboolean
gdk_color_state_save_to_cicp_data (GdkColorState  *self,
                                   int            *color_primaries,
                                   int            *transfer_characteristics,
                                   int            *matrix_coefficients,
                                   gboolean       *full_range,
                                   GError        **error)
{
  if (self == GDK_COLOR_STATE_SRGB)
    {
      *color_primaries = 0;
      *transfer_characteristics = 13;
      *matrix_coefficients = 0;
      *full_range = TRUE;
      return TRUE;
    }
  else if (self == GDK_COLOR_STATE_SRGB_LINEAR)
    {
      *color_primaries = 0;
      *transfer_characteristics = 8;
      *matrix_coefficients = 0;
      *full_range = TRUE;
      return TRUE;
    }

  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("This color state does not support CICP data"));
  return FALSE;
}

/* }}} */
/* {{{ Default implementation */

const char *
gdk_color_state_get_name_from_id (GdkColorStateId id)
{
  const char *names[] = {
    "srgb", "srgb-linear",
  };

  return names[id];
}

GdkColorState gdk_default_color_states[] = {
  { NULL, 0 },
  { NULL, 0 },
};

/* }}} */
/* {{{ Private API */

const char *
gdk_color_state_get_name (GdkColorState *self)
{
  if (GDK_IS_DEFAULT_COLOR_STATE (self))
    {
      switch (GDK_DEFAULT_COLOR_STATE_ID (self))
        {
        case GDK_COLOR_STATE_ID_SRGB: return "srgb";
        case GDK_COLOR_STATE_ID_SRGB_LINEAR: return "srgb-linear";
        default: g_assert_not_reached ();
        }
    }

  return self->klass->get_name (self);
}

void
gdk_color_state_print (GdkColorState *self,
                       GString       *string)
{
  g_string_append (string, gdk_color_state_get_name (self));
}

GdkMemoryDepth
gdk_color_state_get_min_depth (GdkColorState *self)
{
  if (self == GDK_COLOR_STATE_SRGB || self == GDK_COLOR_STATE_SRGB_LINEAR)
    return GDK_MEMORY_U8;

  return GDK_MEMORY_FLOAT16;
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
