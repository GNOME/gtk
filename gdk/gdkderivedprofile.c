/* gdkderivedprofile.c
 *
 * Copyright 2021 (c) Red Hat, Inc.
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
 * GdkDerivedProfile:
 *
 * Since: 4.6
 */

#include "config.h"

#include "gdkderivedprofile.h"
#include "gdkcolorprofileprivate.h"
#include "gdkenumtypes.h"

#include "gdkintl.h"

struct _GdkDerivedProfile
{
  GdkColorProfile parent_instance;

  GdkColorSpace color_space;
  GdkColorProfile *base_profile;
};

struct _GdkDerivedProfileClass
{
  GdkColorProfileClass parent_class;
};

enum {
  PROP_0,
  PROP_BASE_PROFILE,
  PROP_COLOR_SPACE,

  N_PROPS
};

static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE (GdkDerivedProfile, gdk_derived_profile, GDK_TYPE_COLOR_PROFILE)

static void
gdk_derived_profile_init (GdkDerivedProfile *profile)
{
}

static void
gdk_derived_profile_set_property (GObject      *object,
                                  guint         prop_id,
                                  const GValue *value,
                                  GParamSpec   *pspec)
{
  GdkDerivedProfile *self = GDK_DERIVED_PROFILE (object);

  switch (prop_id)
    {
    case PROP_BASE_PROFILE:
      self->base_profile = g_value_dup_object (value);
      break;

    case PROP_COLOR_SPACE:
      self->color_space = g_value_get_enum (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_derived_profile_get_property (GObject    *object,
                                  guint       prop_id,
                                  GValue     *value,
                                  GParamSpec *pspec)
{
  GdkDerivedProfile *self = GDK_DERIVED_PROFILE (object);

  switch (prop_id)
    {
    case PROP_BASE_PROFILE:
      g_value_set_object (value, self->base_profile);
      break;

    case PROP_COLOR_SPACE:
      g_value_set_enum (value, self->color_space);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
gdk_derived_profile_dispose (GObject *object)
{
  GdkDerivedProfile *self = GDK_DERIVED_PROFILE (object);

  g_clear_object (&self->base_profile);

  G_OBJECT_CLASS (gdk_derived_profile_parent_class)->dispose (object);
}

static gboolean
gdk_derived_profile_is_linear (GdkColorProfile *profile)
{
  GdkDerivedProfile *self = GDK_DERIVED_PROFILE (profile);

  return self->base_profile == gdk_color_profile_get_srgb_linear ();
}

static gsize
gdk_derived_profile_get_n_components (GdkColorProfile *profile)
{
  GdkDerivedProfile *self = GDK_DERIVED_PROFILE (profile);

  return gdk_color_profile_get_n_components (self->base_profile);
}

static gboolean
gdk_derived_profile_equal (gconstpointer profile1,
                           gconstpointer profile2)
{
  GdkDerivedProfile *p1 = GDK_DERIVED_PROFILE (profile1);
  GdkDerivedProfile *p2 = GDK_DERIVED_PROFILE (profile2);

  return p1->color_space == p2->color_space &&
         gdk_color_profile_equal (p1->base_profile, p2->base_profile);
}

static void
gdk_derived_profile_class_init (GdkDerivedProfileClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkColorProfileClass *profile_class = GDK_COLOR_PROFILE_CLASS (klass);

  gobject_class->set_property = gdk_derived_profile_set_property;
  gobject_class->get_property = gdk_derived_profile_get_property;
  gobject_class->dispose = gdk_derived_profile_dispose;

  profile_class->is_linear = gdk_derived_profile_is_linear;
  profile_class->get_n_components = gdk_derived_profile_get_n_components;
  profile_class->equal = gdk_derived_profile_equal;

  /**
   * GdkDerivedProfile:base-profile: (attributes org.gtk.Property.get=gdk_derived_profile_get_base_profile)
   *
   * The base profile for this color profile.
   */
  properties[PROP_BASE_PROFILE] =
    g_param_spec_object ("base-profile",
                         P_("Base profile"),
                         P_("Base profile for this color profile"),
                         GDK_TYPE_ICC_PROFILE,
                         G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GdkDerivedProfile:color-space: (attributes org.gtk.Property.get=gdk_derived_profile_get_color_space)
   *
   * The color space for this color profile.
   */
  properties[PROP_COLOR_SPACE] =
    g_param_spec_enum ("color-space",
                       P_("Color space"),
                       P_("Color space for this color profile"),
                       GDK_TYPE_COLOR_SPACE,
                       GDK_COLOR_SPACE_HSL,
                       G_PARAM_READWRITE |
                       G_PARAM_CONSTRUCT_ONLY |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

/**
 * gdk_derived_profile_get_base_profile:
 * @profile: a `GdkDerivedProfile`
 *
 * Returns the base profile for this profile.
 *
 * Returns: (transfer none): the base profile
 *
 * Since: 4.6
 */
GdkColorProfile *
gdk_derived_profile_get_base_profile (GdkDerivedProfile *self)
{
  return self->base_profile;
}

/**
 * gdk_derived_profile_get_color_space:
 * @profile: a `GdkDerivedProfile`
 *
 * Returns the color space for this profile.
 *
 * Returns: the color space
 *
 * Since: 4.6
 */
GdkColorSpace
gdk_derived_profile_get_color_space (GdkDerivedProfile *self)
{
  return self->color_space;
}

/*<private>
 * gdk_color_profile_get_hsl:
 *
 * Returns the color profile corresponding to the HSL
 * color space.
 *
 * It can display the same colors as sRGB, but it encodes
 * the coordinates differently.
 *
 * Returns: (transfer none): the color profile for the HSL
 *   color space.
 */
GdkColorProfile *
gdk_color_profile_get_hsl (void)
{
  static GdkColorProfile *hsl_profile;

  if (g_once_init_enter (&hsl_profile))
    {
      GdkColorProfile *new_profile;

      new_profile = g_object_new (GDK_TYPE_DERIVED_PROFILE,
                                  "base-profile", gdk_color_profile_get_srgb (),
                                  "color-space", GDK_COLOR_SPACE_HSL,
                                  NULL);

      g_assert (new_profile);

      g_once_init_leave (&hsl_profile, new_profile);
    }

  return hsl_profile;
}

GdkColorProfile *
gdk_color_profile_get_hwb (void)
{
  static GdkColorProfile *hwb_profile;

  if (g_once_init_enter (&hwb_profile))
    {
      GdkColorProfile *new_profile;

      new_profile = g_object_new (GDK_TYPE_DERIVED_PROFILE,
                                  "base-profile", gdk_color_profile_get_srgb (),
                                  "color-space", GDK_COLOR_SPACE_HWB,
                                  NULL);

      g_assert (new_profile);

      g_once_init_leave (&hwb_profile, new_profile);
    }

  return hwb_profile;
}

static void
hsl_to_rgb (const float *in,
            float       *out)
{
  float lightness;
  float saturation;
  float m1, m2;
  float orig_hue, hue;

  lightness = in[2];
  saturation = in[1];
  orig_hue = in[0];

  if (lightness <= 0.5)
    m2 = lightness * (1 + saturation);
  else
    m2 = lightness + saturation - lightness * saturation;
  m1 = 2 * lightness - m2;

  if (saturation == 0)
    {
      out[0] = out[1] = out[2] = lightness;
    }
  else
    {
      hue = orig_hue + 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;

      if (hue < 60)
        out[0] = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        out[0] = m2;
      else if (hue < 240)
        out[0] = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        out[0] = m1;

      hue = orig_hue;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;

      if (hue < 60)
        out[1] = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        out[1] = m2;
      else if (hue < 240)
        out[1] = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        out[1] = m1;

      hue = orig_hue - 120;
      while (hue > 360)
        hue -= 360;
      while (hue < 0)
        hue += 360;

      if (hue < 60)
        out[2] = m1 + (m2 - m1) * hue / 60;
      else if (hue < 180)
        out[2] = m2;
      else if (hue < 240)
        out[2] = m1 + (m2 - m1) * (240 - hue) / 60;
      else
        out[2] = m1;
    }
}

static void
rgb_to_hsl (const float *in,
            float       *out)
{
  float min;
  float max;
  float red;
  float green;
  float blue;
  float delta;
  float hue;
  float saturation;
  float lightness;

  red = in[0];
  green = in[1];
  blue = in[2];

  if (red > green)
    {
      if (red > blue)
        max = red;
      else
        max = blue;

      if (green < blue)
        min = green;
      else
        min = blue;
    }
  else
    {
      if (green > blue)
        max = green;
      else
        max = blue;

      if (red < blue)
        min = red;
      else
        min = blue;
    }

  lightness = (max + min) / 2;
  saturation = 0;
  hue = 0;

  if (max != min)
    {
      if (lightness <= 0.5)
        saturation = (max - min) / (max + min);
      else
        saturation = (max - min) / (2 - max - min);

      delta = max -min;
      if (red == max)
        hue = (green - blue) / delta;
      else if (green == max)
        hue = 2 + (blue - red) / delta;
      else if (blue == max)
        hue = 4 + (red - green) / delta;

      hue *= 60;
      if (hue < 0.0)
        hue += 360;
    }

  out[0] = hue;
  out[1] = saturation;
  out[2] = lightness;
}

static void
hwb_to_rgb (const float *in,
            float       *out)
{
  float hue;
  float white;
  float black;
  float m[3];

  hue = in[0];
  white = in[1];
  black = in[2];

  white /= 100;
  black /= 100;

  if (white + black >= 1)
    {
      out[0] = out[1] = out[2] = white / (white + black);
      return;
    }

  m[0] = hue;
  m[1] = 1;
  m[2] = 0.5;

  hsl_to_rgb (m, out);

  out[0] = out[0] * (1 - white - black) + white;
  out[1] = out[1] * (1 - white - black) + white;
  out[2] = out[2] * (1 - white - black) + white;
}

static void
rgb_to_hwb (const float *in,
            float       *out)
{
  float white;
  float black;

  rgb_to_hsl (in, out);

  white = MIN (MIN (out[0], out[1]), out[2]);
  black = 1 - MAX (MAX (out[0], out[1]), out[2]);

  out[1] = white * 100;
  out[2] = black * 100;
}

void
gdk_derived_profile_convert_to_base_profile (GdkDerivedProfile *profile,
                                             const float       *in,
                                             float             *out)
{
  switch (gdk_derived_profile_get_color_space (profile))
    {
    case GDK_COLOR_SPACE_HSL:
      hsl_to_rgb (in, out);
      break;
    case GDK_COLOR_SPACE_HWB:
      hwb_to_rgb (in, out);
      break;
    default:
      g_assert_not_reached ();
    }

}

void
gdk_derived_profile_convert_from_base_profile (GdkDerivedProfile *profile,
                                               const float       *in,
                                               float            *out)
{
  switch (gdk_derived_profile_get_color_space (profile))
    {
    case GDK_COLOR_SPACE_HSL:
      rgb_to_hsl (in, out);
      break;
    case GDK_COLOR_SPACE_HWB:
      rgb_to_hwb (in, out);
      break;
    default:
      g_assert_not_reached ();
    }
}

