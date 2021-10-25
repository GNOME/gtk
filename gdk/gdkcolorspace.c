/* gdkcolorspace.c
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
 * GdkColorSpace:
 *
 * `GdkColorSpace` is used to describe color spaces.
 *
 * Tell developers what a color space is instead of just linking to
 * https://en.wikipedia.org/wiki/Color_space
 *
 * `GdkColorSpace` objects are immutable and therefore threadsafe.
 *
 * Since: 4.6
 */

#include "config.h"

#include "gdkcolorspaceprivate.h"

#include "gdkintl.h"
#include "gdklcmscolorspaceprivate.h"

enum {
  PROP_0,

  N_PROPS
};

//static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE (GdkColorSpace, gdk_color_space, G_TYPE_OBJECT)

static gboolean
gdk_color_space_default_supports_format (GdkColorSpace   *self,
                                         GdkMemoryFormat  format)
{
  return FALSE;
}

static GBytes *
gdk_color_space_default_save_to_icc_profile (GdkColorSpace  *self,
                                             GError        **error)
{
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("This color space does not support ICC profiles"));
  return NULL;
}

static void
gdk_color_space_default_convert_color (GdkColorSpace  *self,
                                       float          *components,
                                       const GdkColor *source)
{
  g_assert_not_reached ();
}

static void
gdk_color_space_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  //GdkColorSpace *self = GDK_COLOR_SPACE (gobject);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_color_space_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  //GdkColorSpace *self = GDK_COLOR_SPACE (gobject);

  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_color_space_dispose (GObject *object)
{
  //GdkColorSpace *self = GDK_COLOR_SPACE (object);

  G_OBJECT_CLASS (gdk_color_space_parent_class)->dispose (object);
}

static void
gdk_color_space_class_init (GdkColorSpaceClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  klass->supports_format = gdk_color_space_default_supports_format;
  klass->save_to_icc_profile = gdk_color_space_default_save_to_icc_profile;
  klass->convert_color = gdk_color_space_default_convert_color;

  gobject_class->set_property = gdk_color_space_set_property;
  gobject_class->get_property = gdk_color_space_get_property;
  gobject_class->dispose = gdk_color_space_dispose;
}

static void
gdk_color_space_init (GdkColorSpace *self)
{
}

/**
 * gdk_color_space_get_srgb:
 *
 * Returns the color profile representing the sRGB color space.
 *
 * If you don't know anything about color profiles but need one for
 * use with some function, this one is most likely the right one.
 *
 * Returns: (transfer none): the color profile for the sRGB
 *   color space.
 *
 * Since: 4.6
 */
GdkColorSpace *
gdk_color_space_get_srgb (void)
{
  static GdkColorSpace *srgb_profile;

  if (g_once_init_enter (&srgb_profile))
    {
      GdkColorSpace *new_profile;

      new_profile = gdk_lcms_color_space_new_from_lcms_profile (cmsCreate_sRGBProfile ());

      g_once_init_leave (&srgb_profile, new_profile);
    }

  return srgb_profile;
}

GdkColorSpace *
gdk_color_space_get_named (GdkNamedColorSpace name)
{
  static GdkColorSpace *space_array[GDK_NAMED_COLOR_SPACE_N_SPACES];
  static GdkColorSpace **color_spaces;

  g_return_val_if_fail (name < GDK_NAMED_COLOR_SPACE_N_SPACES, gdk_color_space_get_srgb ());

  if (g_once_init_enter (&color_spaces))
    {
      static const cmsCIExyY D65 = { 0.3127, 0.3290, 1.0 };
      static const cmsFloat64Number srgb_tonecurve[5] = { 2.4, 1. / 1.055, 0.055 / 1.055, 1. / 12.92, 0.04045 };
      static const cmsFloat64Number rec709_tonecurve[5] = { 1.0 / 0.45, 1.0 / 1.099, 0.099 / 1.099, 1.0 / 4.5, 0.081 };
      cmsToneCurve *curve;
      cmsHPROFILE lcms_profile;

      space_array[GDK_NAMED_COLOR_SPACE_SRGB] = g_object_ref (gdk_color_space_get_srgb ());

      curve = cmsBuildGamma (NULL, 1.0);
      lcms_profile = cmsCreateRGBProfile (&D65,
                                          &(cmsCIExyYTRIPLE) {
                                            { 0.640, 0.330, 1.0 },
                                            { 0.300, 0.600, 1.0 },
                                            { 0.150, 0.060, 1.0 }
                                          },
                                          (cmsToneCurve*[3]) { curve, curve, curve });
      cmsFreeToneCurve (curve);
      space_array[GDK_NAMED_COLOR_SPACE_SRGB_LINEAR] = gdk_lcms_color_space_new_from_lcms_profile (lcms_profile);

      space_array[GDK_NAMED_COLOR_SPACE_XYZ_D50] = gdk_lcms_color_space_new_from_lcms_profile (cmsCreateXYZProfile ());
      /* XXX: This needs a D65 whitepoint here */
      space_array[GDK_NAMED_COLOR_SPACE_XYZ_D65] = gdk_lcms_color_space_new_from_lcms_profile (cmsCreateXYZProfile ());

      curve = cmsBuildParametricToneCurve (NULL, 4, srgb_tonecurve);
      lcms_profile = cmsCreateRGBProfile (&D65,
                                          &(cmsCIExyYTRIPLE) {
                                            { 0.680, 0.320, 1.0 },
                                            { 0.265, 0.690, 1.0 },
                                            { 0.150, 0.060, 1.0 }
                                          },
                                          (cmsToneCurve*[3]) { curve, curve, curve });
      cmsFreeToneCurve (curve);
      space_array[GDK_NAMED_COLOR_SPACE_DISPLAY_P3] = gdk_lcms_color_space_new_from_lcms_profile (lcms_profile);

      curve = cmsBuildGamma(NULL, 2.19921875);
      lcms_profile = cmsCreateRGBProfile (&D65,
                                          &(cmsCIExyYTRIPLE) {
                                            { 0.640, 0.330, 1.0 },
                                            { 0.210, 0.710, 1.0 },
                                            { 0.150, 0.060, 1.0 }
                                          },
                                          (cmsToneCurve*[3]) { curve, curve, curve });
      cmsFreeToneCurve (curve);
      space_array[GDK_NAMED_COLOR_SPACE_A98_RGB] = gdk_lcms_color_space_new_from_lcms_profile (lcms_profile);

      curve = cmsBuildParametricToneCurve (NULL, 4, rec709_tonecurve);
      lcms_profile = cmsCreateRGBProfile (cmsD50_xyY (),
                                          &(cmsCIExyYTRIPLE) {
                                            { 0.734699,	0.265301, 1.0 },
                                            { 0.159597, 0.840403, 1.0 },
                                            { 0.036598, 0.000105, 1.0 }
                                          },
                                          (cmsToneCurve*[3]) { curve, curve, curve });
      cmsFreeToneCurve (curve);
      space_array[GDK_NAMED_COLOR_SPACE_PROPHOTO_RGB] = gdk_lcms_color_space_new_from_lcms_profile (lcms_profile);

      curve = cmsBuildParametricToneCurve (NULL, 4, rec709_tonecurve);
      lcms_profile = cmsCreateRGBProfile (&D65,
                                          &(cmsCIExyYTRIPLE) {
                                            { 0.708, 0.292, 1.0 },
                                            { 0.170, 0.797, 1.0 },
                                            { 0.131, 0.046, 1.0 }
                                          },
                                          (cmsToneCurve*[3]) { curve, curve, curve });
      cmsFreeToneCurve (curve);
      space_array[GDK_NAMED_COLOR_SPACE_REC2020] = gdk_lcms_color_space_new_from_lcms_profile (lcms_profile);

      g_once_init_leave (&color_spaces, space_array);
    }

  return color_spaces[name];
}

/**
 * gdk_color_space_supports_format:
 * @self: a `GdkColorSpace`
 * @format: the format to check
 *
 * Checks if this color space can be used with textures in the given format.
 *
 * Returns: %TRUE if this colorspace supports the format
 *
 * Since: 4.6
 **/
gboolean
gdk_color_space_supports_format (GdkColorSpace   *self,
                                 GdkMemoryFormat  format)
{
  g_return_val_if_fail (GDK_IS_COLOR_SPACE (self), FALSE);
  g_return_val_if_fail (format < GDK_MEMORY_N_FORMATS, FALSE);

  return GDK_COLOR_SPACE_GET_CLASS (self)->supports_format (self, format);
}

/**
 * gdk_color_space_save_to_icc_profile:
 * @self: a `GdkColorSpace`
 * @error: Return location for an error
 *
 * Saves the color space to an
 * [ICC profile](https://en.wikipedia.org/wiki/ICC_profile).
 *
 * Some color spaces cannot be represented as ICC profiles. In
 * that case, an error will be set and %NULL will be returned.
 *
 * Returns: A new `GBytes` containing the ICC profile
 *
 * Since: 4.6
 **/
GBytes *
gdk_color_space_save_to_icc_profile (GdkColorSpace  *self,
                                     GError        **error)
{
  g_return_val_if_fail (GDK_IS_COLOR_SPACE (self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GDK_COLOR_SPACE_GET_CLASS (self)->save_to_icc_profile (self, error);
}

/**
 * gdk_color_space_equal:
 * @profile1: (type GdkColorSpace): a `GdkColorSpace`
 * @profile2: (type GdkColorSpace): another `GdkColorSpace`
 *
 * Compares two `GdkColorSpace`s for equality.
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
gdk_color_space_equal (gconstpointer profile1,
                       gconstpointer profile2)
{
  return profile1 == profile2;
}

