/* gdkcolor_profile.c
 *
 * Copyright 2021 (c) Benjamin Otte
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

/**
 * GdkColorProfile:
 *
 * `GdkColorProfile` is used to describe color spaces.
 *
 * It is used to associate color profiles defined by the [International
 * Color Consortium (ICC)](https://color.org/) with texture and color data.
 *
 * Each `GdkColorProfile` encapsulates an
 * [ICC profile](https://en.wikipedia.org/wiki/ICC_profile). That profile
 * can be queried via the using [property@Gdk.ColorProfile:icc-profile]
 * property.
 *
 * `GdkColorProfile` objects are immutable and therefore threadsafe.
 *
 * Since: 4.6
 */

#include "config.h"

#include "gdkcolorprofileprivate.h"

#include "gdkintl.h"


G_DEFINE_TYPE (GdkColorProfile, gdk_color_profile, G_TYPE_OBJECT)

static void
gdk_color_profile_init (GdkColorProfile *profile)
{
}

static gboolean
gdk_color_profile_real_is_linear (GdkColorProfile *profile)
{
  return FALSE;
}

static gsize
gdk_color_profile_real_get_n_components (GdkColorProfile *profile)
{
  return 0;
}

static gboolean
gdk_color_profile_real_equal (gconstpointer profile1,
                              gconstpointer profile2)
{
  return profile1 == profile2;
}

static void
gdk_color_profile_class_init (GdkColorProfileClass *klass)
{
  klass->is_linear = gdk_color_profile_real_is_linear;
  klass->get_n_components = gdk_color_profile_real_get_n_components;
  klass->equal = gdk_color_profile_real_equal;
}

/**
 * gdk_color_profile_is_linear:
 * @self: a `GdkColorProfile`
 *
 * Checks if the given profile is linear.
 *
 * GTK tries to do compositing in a linear profile.
 *
 * Some profiles may be linear, but it is not possible to
 * determine this easily. In those cases %FALSE will be returned.
 *
 * Returns: %TRUE if the profile can be proven linear
 *
 * Since: 4.6
 */
gboolean
gdk_color_profile_is_linear (GdkColorProfile *self)
{
  g_return_val_if_fail (GDK_IS_COLOR_PROFILE (self), FALSE);

  return GDK_COLOR_PROFILE_GET_CLASS (self)->is_linear (self);
}

/**
 * gdk_color_profile_get_n_components:
 * @self: a `GdkColorProfile
 *
 * Gets the number of color components - also called channels - for
 * the given profile. Note that this does not consider an alpha
 * channel because color profiles have no alpha information. So
 * for any form of RGB profile, this returned number will be 3.
 *
 * Returns: The number of components
 **/
gsize
gdk_color_profile_get_n_components (GdkColorProfile *self)
{
  g_return_val_if_fail (GDK_IS_COLOR_PROFILE (self), 3);

  return GDK_COLOR_PROFILE_GET_CLASS (self)->get_n_components (self);
}

/**
 * gdk_color_profile_equal:
 * @profile1: (type GdkColorProfile): a `GdkColorProfile`
 * @profile2: (type GdkColorProfile): another `GdkColorProfile`
 *
 * Compares two `GdkColorProfile`s for equality.
 *
 * Note that this function is not guaranteed to be perfect and two equal
 * profiles may compare not equal. However, different profiles will
 * never compare equal.
 *
 * Returns: %TRUE if the two color profiles compare equal
 *
 * Since: 4.6
 */
gboolean
gdk_color_profile_equal (gconstpointer profile1,
                         gconstpointer profile2)
{
  if (profile1 == profile2)
    return TRUE;

  if (G_OBJECT_TYPE (profile1) != G_OBJECT_TYPE (profile2))
    return FALSE;

  return GDK_COLOR_PROFILE_GET_CLASS (profile1)->equal (profile1, profile2);
}
