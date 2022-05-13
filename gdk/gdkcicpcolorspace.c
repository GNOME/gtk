/* gdkcicpcolorspace.c
 *
 * Copyright 2022 (c) Red Hat, Inc
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

#include "gdkcicpcolorspaceprivate.h"
#include "gdklcmscolorspaceprivate.h"

#include <glib/gi18n-lib.h>

struct _GdkCicpColorSpace
{
  GdkColorSpace parent_instance;

  int color_primaries;
  int transfer_characteristics;
  int matrix_coefficients;
  gboolean full_range;

  GdkColorSpace *lcms;
};

struct _GdkCicpColorSpaceClass
{
  GdkColorSpaceClass parent_class;
};

G_DEFINE_TYPE (GdkCicpColorSpace, gdk_cicp_color_space, GDK_TYPE_COLOR_SPACE)

static gboolean
gdk_cicp_color_space_supports_format (GdkColorSpace   *space,
                                      GdkMemoryFormat  format)
{
  GdkCicpColorSpace *self = GDK_CICP_COLOR_SPACE (space);

  if (self->lcms)
    return gdk_color_space_supports_format (self->lcms, format);

  return FALSE;
}

static GBytes *
gdk_cicp_color_space_save_to_icc_profile (GdkColorSpace  *space,
                                          GError        **error)
{
  GdkCicpColorSpace *self = GDK_CICP_COLOR_SPACE (space);

  if (self->lcms)
    return gdk_color_space_save_to_icc_profile (self->lcms, error);

  return NULL;
}

static int
gdk_cicp_color_space_get_n_components (GdkColorSpace *space)
{
  GdkCicpColorSpace *self = GDK_CICP_COLOR_SPACE (space);

  if (self->lcms)
    return gdk_color_space_get_n_components (self->lcms);

  return 0;
}

static void
gdk_cicp_color_space_dispose (GObject *object)
{
  GdkCicpColorSpace *self = GDK_CICP_COLOR_SPACE (object);

  g_clear_object (&self->lcms);

  G_OBJECT_CLASS (gdk_cicp_color_space_parent_class)->dispose (object);
}

static void
gdk_cicp_color_space_class_init (GdkCicpColorSpaceClass *klass)
{
  GdkColorSpaceClass *color_space_class = GDK_COLOR_SPACE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  color_space_class->supports_format = gdk_cicp_color_space_supports_format;
  color_space_class->save_to_icc_profile = gdk_cicp_color_space_save_to_icc_profile;
  color_space_class->get_n_components = gdk_cicp_color_space_get_n_components;

  gobject_class->dispose = gdk_cicp_color_space_dispose;
}

static void
gdk_cicp_color_space_init (GdkCicpColorSpace *self)
{
}

static GdkColorSpace *
lcms_from_cicp (int        color_primaries,
                int        transfer_characteristics,
                int        matrix_coefficients,
                gboolean   full_range)
{
  cmsHPROFILE profile = NULL;
  cmsCIExyY whitepoint;
  cmsCIExyYTRIPLE primaries;
  cmsToneCurve *curve[3];
  cmsCIExyY whiteD65 = (cmsCIExyY) { 0.3127, 0.3290, 1.0 };
  cmsCIExyY whiteC = (cmsCIExyY) { 0.310, 0.316, 1.0 };
  cmsFloat64Number srgb_parameters[5] =
    { 2.4, 1.0 / 1.055,  0.055 / 1.055, 1.0 / 12.92, 0.04045 };
  cmsFloat64Number rec709_parameters[5] =
    { 2.2, 1.0 / 1.099,  0.099 / 1.099, 1.0 / 4.5, 0.081 };

  /* We only support full-range RGB profiles */
  g_assert (matrix_coefficients == 0);
  g_assert (full_range);

  if (color_primaries == 0 /* ITU_R_BT_709_5 */)
    {
      if (transfer_characteristics == 13 /* IEC_61966_2_1 */)
        return g_object_ref (gdk_color_space_get_srgb ());
      else if (transfer_characteristics == 8 /* linear */)
        return g_object_ref (gdk_color_space_get_srgb_linear ());
    }
  switch (color_primaries)
    {
    case 1:
      primaries.Green = (cmsCIExyY) { 0.300, 0.600, 1.0 };
      primaries.Blue  = (cmsCIExyY) { 0.150, 0.060, 1.0 };
      primaries.Red   = (cmsCIExyY) { 0.640, 0.330, 1.0 };
      whitepoint = whiteD65;
      break;
    case 4:
      primaries.Green = (cmsCIExyY) { 0.21, 0.71, 1.0 };
      primaries.Blue  = (cmsCIExyY) { 0.14, 0.08, 1.0 };
      primaries.Red   = (cmsCIExyY) { 0.67, 0.33, 1.0 };
      whitepoint = whiteC;
      break;
    case 5:
      primaries.Green = (cmsCIExyY) { 0.29, 0.60, 1.0 };
      primaries.Blue  = (cmsCIExyY) { 0.15, 0.06, 1.0 };
      primaries.Red   = (cmsCIExyY) { 0.64, 0.33, 1.0 };
      whitepoint = whiteD65;
      break;
    case 6:
    case 7:
      primaries.Green = (cmsCIExyY) { 0.310, 0.595, 1.0 };
      primaries.Blue  = (cmsCIExyY) { 0.155, 0.070, 1.0 };
      primaries.Red   = (cmsCIExyY) { 0.630, 0.340, 1.0 };
      whitepoint = whiteD65;
      break;
    case 8:
      primaries.Green = (cmsCIExyY) { 0.243, 0.692, 1.0 };
      primaries.Blue  = (cmsCIExyY) { 0.145, 0.049, 1.0 };
      primaries.Red   = (cmsCIExyY) { 0.681, 0.319, 1.0 };
      whitepoint = whiteC;
      break;
    case 9:
      primaries.Green = (cmsCIExyY) { 0.170, 0.797, 1.0 };
      primaries.Blue  = (cmsCIExyY) { 0.131, 0.046, 1.0 };
      primaries.Red   = (cmsCIExyY) { 0.708, 0.292, 1.0 };
      whitepoint = whiteD65;
      break;
    case 10:
      primaries.Green = (cmsCIExyY) { 0.0, 1.0, 1.0 };
      primaries.Blue  = (cmsCIExyY) { 0.0, 0.0, 1.0 };
      primaries.Red   = (cmsCIExyY) { 1.0, 0.0, 1.0 };
      whitepoint = (cmsCIExyY) { 0.333333, 0.333333, 1.0 };
      break;
    case 11:
      primaries.Green = (cmsCIExyY) { 0.265, 0.690, 1.0 };
      primaries.Blue  = (cmsCIExyY) { 0.150, 0.060, 1.0 };
      primaries.Red   = (cmsCIExyY) { 0.680, 0.320, 1.0 };
      whitepoint = (cmsCIExyY) { 0.314, 0.351, 1.0 };
      break;
    case 12:
      primaries.Green = (cmsCIExyY) { 0.265, 0.690, 1.0 };
      primaries.Blue  = (cmsCIExyY) { 0.150, 0.060, 1.0 };
      primaries.Red   = (cmsCIExyY) { 0.680, 0.320, 1.0 };
      whitepoint = whiteD65;
      break;
    case 22:
      primaries.Green = (cmsCIExyY) { 0.295, 0.605, 1.0 };
      primaries.Blue  = (cmsCIExyY) { 0.155, 0.077, 1.0 };
      primaries.Red   = (cmsCIExyY) { 0.630, 0.340, 1.0 };
      whitepoint = whiteD65;
      break;
    default:
      return NULL;
    }

  switch (transfer_characteristics)
    {
    case 1: /* ITU_R_BT_709_5 */
      curve[0] = curve[1] = curve[2] = cmsBuildParametricToneCurve (NULL, 4,
                                       rec709_parameters);
      profile = cmsCreateRGBProfile (&whitepoint, &primaries, curve);
      cmsFreeToneCurve (curve[0]);
      break;
    case 4: /* ITU_R_BT_470_6_System_M */
      curve[0] = curve[1] = curve[2] = cmsBuildGamma (NULL, 2.2f);
      profile = cmsCreateRGBProfile (&whitepoint, &primaries, curve);
      cmsFreeToneCurve (curve[0]);
      break;
    case 5: /* ITU_R_BT_470_6_System_B_G */
      curve[0] = curve[1] = curve[2] = cmsBuildGamma (NULL, 2.8f);
      profile = cmsCreateRGBProfile (&whitepoint, &primaries, curve);
      cmsFreeToneCurve (curve[0]);
      break;
    case 8: /* linear */
      curve[0] = curve[1] = curve[2] = cmsBuildGamma (NULL, 1.0f);
      profile = cmsCreateRGBProfile (&whitepoint, &primaries, curve);
      cmsFreeToneCurve (curve[0]);
      break;
    /* FIXME
     * We need to handle at least 16 (PQ) here.
     * Problem is: lcms can't do it
     */
    case 13: /* IEC_61966_2_1 */
    default:
      curve[0] = curve[1] = curve[2] = cmsBuildParametricToneCurve (NULL, 4,
                                       srgb_parameters);
      profile = cmsCreateRGBProfile (&whitepoint, &primaries, curve);
      cmsFreeToneCurve (curve[0]);
      break;
    }

  if (profile)
    return gdk_lcms_color_space_new_from_lcms_profile (profile);

  return NULL;
}

GdkColorSpace *
gdk_color_space_new_from_cicp (int      color_primaries,
                               int      transfer_characteristics,
                               int      matrix_coefficients,
                               gboolean full_range)
{
  GdkCicpColorSpace *result;

  result = g_object_new (GDK_TYPE_CICP_COLOR_SPACE, NULL);
  result->color_primaries = color_primaries;
  result->transfer_characteristics = transfer_characteristics;
  result->matrix_coefficients = matrix_coefficients;
  result->full_range = full_range;

  result->lcms = lcms_from_cicp (color_primaries,
                                 transfer_characteristics,
                                 matrix_coefficients,
                                 full_range);

  return GDK_COLOR_SPACE (result);
}

GdkColorSpace *
gdk_cicp_color_space_get_lcms_color_space (GdkColorSpace *space)
{
  GdkCicpColorSpace *self = GDK_CICP_COLOR_SPACE (space);

  return self->lcms;
}
