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


G_DEFINE_TYPE (GdkColorState, gdk_color_state, G_TYPE_OBJECT)

static void
gdk_color_state_init (GdkColorState *self)
{
}

static GBytes *
gdk_color_state_default_save_to_icc_profile (GdkColorState  *self,
                                             GError        **error)
{
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("This color state does not support ICC profiles"));
  return NULL;
}

static gboolean
gdk_color_state_default_equal (GdkColorState *self,
                               GdkColorState *other)
{
  return self == other;
}

static const char *
gdk_color_state_default_get_name (GdkColorState *self)
{
  return "color state";
}

static GdkMemoryDepth
gdk_color_state_default_get_min_depth (GdkColorState *self)
{
  return GDK_MEMORY_U8;
}

static void
gdk_color_state_class_init (GdkColorStateClass *class)
{
  class->save_to_icc_profile = gdk_color_state_default_save_to_icc_profile;
  class->equal = gdk_color_state_default_equal;
  class->get_name = gdk_color_state_default_get_name;
  class->get_min_depth = gdk_color_state_default_get_min_depth;
}

/**
 * gdk_color_state_equal:
 * @cs1: a `GdkColorState`
 * @cs2: another `GdkColorStatee`
 *
 * Compares two `GdkColorStates` for equality.
 *
 * Note that this function is not guaranteed to be perfect and two objects
 * describing the same color state may compare not equal. However, different
 * color state will never compare equal.
 *
 * Returns: %TRUE if the two color states compare equal
 *
 * Since: 4.16
 */
gboolean
gdk_color_state_equal (GdkColorState *cs1,
                       GdkColorState *cs2)
{
  if (cs1 == cs2)
    return TRUE;

  if (GDK_COLOR_STATE_GET_CLASS (cs1) != GDK_COLOR_STATE_GET_CLASS (cs2))
    return FALSE;

  return GDK_COLOR_STATE_GET_CLASS (cs1)->equal (cs1, cs2);
}

/**
 * gdk_color_state_is_linear:
 * @self: a `GdkColorState`
 *
 * Returns whether the color state is linear.
 *
 * Returns: `TRUE` if the color state is linear
 * Since: 4.16
 */
gboolean
gdk_color_state_is_linear (GdkColorState *self)
{
  g_return_val_if_fail (GDK_IS_COLOR_STATE (self), FALSE);

  return self == gdk_color_state_get_srgb_linear ();
}

/**
 * gdk_color_state_save_to_icc_profile:
 * @self: a `GdkColorState`
 * @error: Return location for an error
 *
 * Saves the color state to an
 * [ICC profile](https://en.wikipedia.org/wiki/ICC_profile).
 *
 * Some color state cannot be represented as ICC profiles. In
 * that case, an error will be set and %NULL will be returned.
 *
 * Returns: (nullable): A new `GBytes` containing the ICC profile
 *
 * Since: 4.16
 */
GBytes *
gdk_color_state_save_to_icc_profile (GdkColorState  *self,
                                     GError        **error)
{
  g_return_val_if_fail (GDK_IS_COLOR_STATE (self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GDK_COLOR_STATE_GET_CLASS (self)->save_to_icc_profile (self, error);
}

GdkMemoryDepth
gdk_color_state_get_min_depth (GdkColorState *self)
{
  return GDK_COLOR_STATE_GET_CLASS (self)->get_min_depth (self);
}

const char *
gdk_color_state_get_name (GdkColorState *self)
{
  return GDK_COLOR_STATE_GET_CLASS (self)->get_name (self);
}
