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
#include "gtk/gtkcolorutilsprivate.h"

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

static GdkColorStateClass * get_class (GdkColorState *self);


G_DEFINE_BOXED_TYPE (GdkColorState, gdk_color_state,
                     gdk_color_state_ref, gdk_color_state_unref);

/* {{{ Public API */

GdkColorState *
(gdk_color_state_ref) (GdkColorState *self)
{
  return _gdk_color_state_ref (self);
}

void
(gdk_color_state_unref) (GdkColorState *self)
{
  _gdk_color_state_unref (self);
}

GdkColorState *
(gdk_color_state_get_srgb) (void)
{
  return _gdk_color_state_get_srgb ();
}

GdkColorState *
(gdk_color_state_get_srgb_linear) (void)
{
  return _gdk_color_state_get_srgb_linear ();
}

GdkColorState *
(gdk_color_state_get_hsl) (void)
{
  return _gdk_color_state_get_hsl ();
}

GdkColorState *
(gdk_color_state_get_hwb) (void)
{
  return _gdk_color_state_get_hwb ();
}

GdkColorState *
(gdk_color_state_get_oklab) (void)
{
  return _gdk_color_state_get_oklab ();
}

GdkColorState *
(gdk_color_state_get_oklch) (void)
{
  return _gdk_color_state_get_oklch ();
}

GdkColorState *
(gdk_color_state_get_display_p3) (void)
{
  return _gdk_color_state_get_display_p3 ();
}

GdkColorState *
(gdk_color_state_get_xyz) (void)
{
  return _gdk_color_state_get_xyz ();
}

GdkColorState *
(gdk_color_state_get_rec2020) (void)
{
  return _gdk_color_state_get_rec2020 ();
}

GdkColorState *
(gdk_color_state_get_rec2100_pq) (void)
{
  return _gdk_color_state_get_rec2100_pq ();
}

GdkColorState *
(gdk_color_state_get_rec2100_linear) (void)
{
  return _gdk_color_state_get_rec2100_linear ();
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
  return get_class (self)->is_linear (self);
}

/**
 * gdk_color_state_save_to_icc_profile:
 * @self: a `GdkColorState`
 * @error: Return location for an error
 *
 * Saves the color state to an
 * [ICC profile](https://en.wikipedia.org/wiki/ICC_profile).
 *
 * Some color states cannot be represented as ICC profiles.
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
  return get_class (self)->save_to_icc_profile (self, error);
}

/**
 * gdk_color_state_save_to_cicp_data:
 * @self: a `GdkColorState`
 * @color_primaries: return location for color primaries
 * @transfer_characteristics: return location for transfer characteristics
 * @matrix_coefficients: return location for matrix_coefficients
 * @full_range: return location for the full range flag
 * @error: Return location for an error
 *
 * Saves the color state as
 * [CICP](https://en.wikipedia.org/wiki/Coding-independent_code_points)
 * data.
 *
 * Some color states cannot be represented as CICP data.
 * In that case, @error will be set and `FALSE` will be returned.
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
  return get_class (self)->save_to_cicp_data (self,
                                              color_primaries,
                                              transfer_characteristics,
                                              matrix_coefficients,
                                              full_range,
                                              error);
}

/* }}} */
/* {{{ ICC implementation */

#define GDK_LCMS_COLOR_STATE(c) ((GdkLcmsColorState *)(c))

typedef struct {
  GdkColorState state;

  cmsHPROFILE lcms_profile;
} GdkLcmsColorState;

/* {{{ Helpers */

static cmsHPROFILE
gdk_lcms_color_state_get_lcms_profile (GdkColorState *self)
{
  g_return_val_if_fail (GDK_IS_LCMS_COLOR_STATE (self), NULL);

  return GDK_LCMS_COLOR_STATE (self)->lcms_profile;
}

static cmsHPROFILE
gdk_lcms_get_srgb_profile (void)
{
   return cmsCreate_sRGBProfile ();
}

static cmsHPROFILE
gdk_lcms_get_srgb_linear_profile (void)
{
  cmsToneCurve *curve;
  cmsHPROFILE profile;

  curve = cmsBuildGamma (NULL, 1.0);
  profile = cmsCreateRGBProfile (&(cmsCIExyY) {
                                   0.3127, 0.3290, 1.0
                                 },
                                 &(cmsCIExyYTRIPLE) {
                                   { 0.6400, 0.3300, 1.0 },
                                   { 0.3000, 0.6000, 1.0 },
                                   { 0.1500, 0.0600, 1.0 }
                                 },
                                 (cmsToneCurve*[3]) { curve, curve, curve });
  cmsFreeToneCurve (curve);

  return profile;
}

static GBytes *
gdk_lcms_save_profile (cmsHPROFILE   profile,
                       GError      **error)
{
  cmsUInt32Number size;
  guchar *data;

  size = 0;
  if (!cmsSaveProfileToMem (profile, NULL, &size))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Could not prepare ICC profile"));
      return NULL;
    }

  data = g_malloc (size);
  if (!cmsSaveProfileToMem (profile, data, &size))
    {
      g_free (data);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Failed to save ICC profile"));
      return NULL;
    }

  return g_bytes_new_take (data, size);
}

/* }}} */
/* {{{ GdkColorState vfuncs */

static void
gdk_lcms_color_state_free (GdkColorState *state)
{
  GdkLcmsColorState *self = (GdkLcmsColorState *) state;

  cmsCloseProfile (self->lcms_profile);

  g_free (self);
}

static gboolean
gdk_lcms_color_state_equal (GdkColorState *state1,
                            GdkColorState *state2)
{
  GBytes *icc1, *icc2;
  gboolean res;

  icc1 = gdk_color_state_save_to_icc_profile (state1, NULL);
  icc2 = gdk_color_state_save_to_icc_profile (state2, NULL);

  res = g_bytes_equal (icc1, icc2);

  g_bytes_unref (icc1);
  g_bytes_unref (icc2);

  return res;
}

static gboolean
gdk_lcms_color_state_is_linear (GdkColorState *state)
{
  return FALSE; /* FIXME */
}

static GBytes *
gdk_lcms_color_state_save_to_icc_profile (GdkColorState  *state,
                                          GError        **error)
{
  GdkLcmsColorState *self = GDK_LCMS_COLOR_STATE (state);

  return gdk_lcms_save_profile (self->lcms_profile, error);
}

static gboolean
gdk_lcms_color_state_save_to_cicp_data (GdkColorState  *self,
                                        int            *color_primaries,
                                        int            *transfer_characteristics,
                                        int            *matrix_coefficients,
                                        gboolean       *full_range,
                                        GError        **error)
{
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("This color state does not support CICP data"));
  return FALSE;
}

static const char *
gdk_lcms_color_state_get_name (GdkColorState *self)
{
  static char buffer[48];

  g_snprintf (buffer, sizeof (buffer), "lcms color state %p", self);

  return buffer;
}

static guint
gdk_lcms_color_state_get_min_depth (GdkColorState *self)
{
  return GDK_MEMORY_U16; /* ? */
}

static int
gdk_lcms_color_state_get_hue_coord (GdkColorState *self)
{
  return -1;
}

static GdkColorStateClass LCMS_COLOR_STATE_CLASS =
{
  GDK_TYPE_LCMS_COLOR_STATE,
  gdk_lcms_color_state_free,
  gdk_lcms_color_state_equal,
  gdk_lcms_color_state_is_linear,
  gdk_lcms_color_state_save_to_icc_profile,
  gdk_lcms_color_state_save_to_cicp_data,
  gdk_lcms_color_state_get_name,
  gdk_lcms_color_state_get_min_depth,
  gdk_lcms_color_state_get_hue_coord,
};

/* }}} */
/* {{{ Private API */

GdkColorState *
gdk_color_state_new_from_lcms_profile (cmsHPROFILE lcms_profile)
{
  GdkLcmsColorState *self;

  self= g_new0 (GdkLcmsColorState, 1);

  self->state.klass = &LCMS_COLOR_STATE_CLASS;
  self->state.ref_count = 1;

  self->lcms_profile = lcms_profile;

  return (GdkColorState *) self;
}

/* }}} */
/* {{{ Public API */

/**
 * gdk_color_state_new_from_icc_profile:
 * @icc_profile: The ICC profiles given as a `GBytes`
 * @error: Return location for an error
 *
 * Creates a new color state for the given ICC profile data.
 *
 * if the profile is not valid, %NULL is returned and an error
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
  cmsHPROFILE lcms_profile;
  const guchar *data;
  gsize size;

  g_return_val_if_fail (icc_profile != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  data = g_bytes_get_data (icc_profile, &size);

  lcms_profile = cmsOpenProfileFromMem (data, size);
  if (lcms_profile == NULL)
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Failed to load ICC profile"));
      return NULL;
    }

  return gdk_color_state_new_from_lcms_profile (lcms_profile);
}

/* }}} */
/* }}} */
/* {{{ Named implementation */

static gboolean
gdk_named_color_state_is_linear (GdkColorState *self)
{
  switch (GDK_NAMED_COLOR_STATE_ID (self))
    {
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
    case GDK_COLOR_STATE_ID_OKLAB:
    case GDK_COLOR_STATE_ID_OKLCH:
    case GDK_COLOR_STATE_ID_XYZ:
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return TRUE;

    case GDK_COLOR_STATE_ID_SRGB:
    case GDK_COLOR_STATE_ID_HSL:
    case GDK_COLOR_STATE_ID_HWB:
    case GDK_COLOR_STATE_ID_DISPLAY_P3:
    case GDK_COLOR_STATE_ID_REC2020:
    case GDK_COLOR_STATE_ID_REC2100_PQ:
      return FALSE;

    default:
      g_assert_not_reached ();
    }
}

static GBytes *
gdk_named_color_state_save_to_icc_profile (GdkColorState  *self,
                                           GError        **error)
{
  cmsHPROFILE profile;

  switch (GDK_NAMED_COLOR_STATE_ID (self))
    {
    case GDK_COLOR_STATE_ID_SRGB:
      profile = gdk_lcms_get_srgb_profile ();
      break;

    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      profile = gdk_lcms_get_srgb_linear_profile ();
      break;

    case GDK_COLOR_STATE_ID_OKLAB:
    case GDK_COLOR_STATE_ID_OKLCH:
    case GDK_COLOR_STATE_ID_HSL:
    case GDK_COLOR_STATE_ID_HWB:
    case GDK_COLOR_STATE_ID_DISPLAY_P3:
    case GDK_COLOR_STATE_ID_XYZ:
    case GDK_COLOR_STATE_ID_REC2020:
    case GDK_COLOR_STATE_ID_REC2100_PQ:
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      profile = NULL;
      break;

    default:
      g_assert_not_reached ();
    }

  if (profile)
    {
      GBytes *bytes;

      bytes = gdk_lcms_save_profile (profile, error);
      cmsCloseProfile (profile);

      return bytes;
    }
  else
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("ICC profile not supported for this color state"));

      return NULL;
    }
}

static gboolean
gdk_named_color_state_save_to_cicp_data (GdkColorState  *self,
                                         int            *color_primaries,
                                         int            *transfer_characteristics,
                                         int            *matrix_coefficients,
                                         gboolean       *full_range,
                                         GError        **error)
{
  switch (GDK_NAMED_COLOR_STATE_ID (self))
    {
    case GDK_COLOR_STATE_ID_SRGB:
      *color_primaries = 0;
      *transfer_characteristics = 13;
      *matrix_coefficients = 0;
      *full_range = TRUE;
      return TRUE;

    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      *color_primaries = 0;
      *transfer_characteristics = 8;
      *matrix_coefficients = 0;
      *full_range = TRUE;
      return TRUE;

    case GDK_COLOR_STATE_ID_REC2100_PQ:
      *color_primaries = 9;
      *transfer_characteristics = 16;
      *matrix_coefficients = 0;
      *full_range = TRUE;
      return TRUE;

    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      *color_primaries = 9;
      *transfer_characteristics = 8;
      *matrix_coefficients = 0;
      *full_range = TRUE;
      return TRUE;

    case GDK_COLOR_STATE_ID_DISPLAY_P3:
      *color_primaries = 12;
      *transfer_characteristics = 13;
      *matrix_coefficients = 0;
      *full_range = TRUE;
      return TRUE;

    case GDK_COLOR_STATE_ID_HSL:
    case GDK_COLOR_STATE_ID_HWB:
    case GDK_COLOR_STATE_ID_OKLAB:
    case GDK_COLOR_STATE_ID_OKLCH:
    case GDK_COLOR_STATE_ID_XYZ:
    case GDK_COLOR_STATE_ID_REC2020:
      break;

    default:
      g_assert_not_reached ();
    }

  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("This color state does not support CICP data"));
  return FALSE;
}

const char *
gdk_color_state_get_name_from_id (GdkColorStateId id)
{
  const char *names[] = {
    "srgb", "srgb-linear", "hsl", "hwb",
    "oklab", "oklch", "display-p3", "xyz",
    "rec2020", "rec2100-pq", "rec2100-linear",
  };

  return names[(id - 1) >> 1];
}

static const char *
gdk_named_color_state_get_name (GdkColorState *self)
{
  return gdk_color_state_get_name_from_id (GDK_NAMED_COLOR_STATE_ID (self));
}

static guint
gdk_named_color_state_get_min_depth (GdkColorState *self)
{
  switch (GDK_NAMED_COLOR_STATE_ID (self))
    {
    case GDK_COLOR_STATE_ID_OKLAB:
    case GDK_COLOR_STATE_ID_OKLCH:
    case GDK_COLOR_STATE_ID_HSL:
    case GDK_COLOR_STATE_ID_HWB:
    case GDK_COLOR_STATE_ID_DISPLAY_P3:
    case GDK_COLOR_STATE_ID_XYZ:
    case GDK_COLOR_STATE_ID_REC2020:
    case GDK_COLOR_STATE_ID_REC2100_PQ:
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      return GDK_MEMORY_FLOAT16;

    /* We want to use fast paths for the common srgb-linear / srgb case,
     * which needs u8 framebuffers
     */
    case GDK_COLOR_STATE_ID_SRGB:
      return GDK_MEMORY_U8;

    default:
      g_assert_not_reached ();
    }
}

static int
gdk_named_color_state_get_hue_coord (GdkColorState *in)
{
  switch (GDK_NAMED_COLOR_STATE_ID (in))
    {
    case GDK_COLOR_STATE_ID_SRGB:
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
    case GDK_COLOR_STATE_ID_OKLAB:
    case GDK_COLOR_STATE_ID_DISPLAY_P3:
    case GDK_COLOR_STATE_ID_XYZ:
    case GDK_COLOR_STATE_ID_REC2020:
    case GDK_COLOR_STATE_ID_REC2100_PQ:
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return -1;

    case GDK_COLOR_STATE_ID_HSL:
    case GDK_COLOR_STATE_ID_HWB:
      return 0;

    case GDK_COLOR_STATE_ID_OKLCH:
      return 2;

    default:
      g_assert_not_reached ();
    }
}

static GdkColorStateClass NAMED_COLOR_STATE_CLASS =
{
  GDK_TYPE_NAMED_COLOR_STATE,
  NULL,
  NULL,
  gdk_named_color_state_is_linear,
  gdk_named_color_state_save_to_icc_profile,
  gdk_named_color_state_save_to_cicp_data,
  gdk_named_color_state_get_name,
  gdk_named_color_state_get_min_depth,
  gdk_named_color_state_get_hue_coord,
};

/* }}} */
/* {{{ CICP support */

GdkColorState *
gdk_color_state_new_from_cicp_data (int      color_primaries,
                                    int      transfer_characteristics,
                                    int      matrix_coefficients,
                                    gboolean full_range)
{
  /* FIXME We don't support these */
  if (!full_range || matrix_coefficients != 0)
    return NULL;

  /* Look for cases we can handle */
  if (color_primaries == 0 && transfer_characteristics == 13)
    return gdk_color_state_get_srgb ();
  else if (color_primaries == 0 && transfer_characteristics == 8)
    return gdk_color_state_get_srgb_linear ();
  else if (color_primaries == 9 && transfer_characteristics == 16)
    return gdk_color_state_get_rec2100_pq ();
  else if (color_primaries == 9 && transfer_characteristics == 8)
    return gdk_color_state_get_rec2100_linear ();
  else if (color_primaries == 12 && transfer_characteristics == 13)
    return gdk_color_state_get_display_p3 ();

  return NULL;
}

/* }}} */
/* {{{ Private API */

static GdkColorStateClass *
get_class (GdkColorState *self)
{
  if (GDK_IS_NAMED_COLOR_STATE (self))
    return &NAMED_COLOR_STATE_CLASS;

  return self->klass;
}

GdkMemoryDepth
gdk_color_state_get_min_depth (GdkColorState *self)
{
  return (GdkMemoryDepth) get_class (self)->get_min_depth (self);
}

const char *
gdk_color_state_get_name (GdkColorState *self)
{
  return get_class (self)->get_name (self);
}

int
gdk_color_state_get_hue_coord (GdkColorState *self)
{
  return get_class (self)->get_hue_coord (self);
}

/* }}} */
/* {{{ Conversion */

#define two_step(name, func1, func2)    \
static void                             \
name (float r,  float g,  float b,      \
      float *x, float *y, float *z)     \
{                                       \
  func1 (r, g, b, x, y, z);             \
  func2 (*x, *y, *z, x, y, z);          \
}

/* A full complement of xyz <> anything step functions */
two_step (srgb_to_xyz, gtk_rgb_to_linear_srgb, gtk_linear_srgb_to_xyz)
two_step (xyz_to_srgb, gtk_xyz_to_linear_srgb, gtk_linear_srgb_to_rgb)
two_step (hsl_to_xyz, gtk_hsl_to_rgb, srgb_to_xyz)
two_step (xyz_to_hsl, xyz_to_srgb, gtk_rgb_to_hsl)
two_step (hwb_to_xyz, gtk_hwb_to_rgb, srgb_to_xyz)
two_step (xyz_to_hwb, xyz_to_srgb, gtk_rgb_to_hwb)
two_step (oklab_to_xyz, gtk_oklab_to_linear_srgb, gtk_linear_srgb_to_xyz)
two_step (xyz_to_oklab, gtk_xyz_to_linear_srgb, gtk_linear_srgb_to_oklab)
two_step (oklch_to_xyz, gtk_oklch_to_oklab, oklab_to_xyz)
two_step (xyz_to_oklch, xyz_to_oklab, gtk_oklab_to_oklch)
two_step (p3_to_xyz, gtk_p3_to_rgb, srgb_to_xyz)
two_step (xyz_to_p3, xyz_to_srgb, gtk_rgb_to_p3)
two_step (rec2100_linear_to_xyz, gtk_rec2100_linear_to_rec2020_linear, gtk_rec2020_linear_to_xyz)
two_step (xyz_to_rec2100_linear, gtk_xyz_to_rec2020_linear, gtk_rec2020_linear_to_rec2100_linear)
two_step (rec2100_pq_to_xyz, gtk_rec2100_pq_to_rec2100_linear, rec2100_linear_to_xyz)
two_step (xyz_to_rec2100_pq, xyz_to_rec2100_linear, gtk_rec2100_linear_to_rec2100_pq)


static struct {
 GdkColorStateId n1;
 GdkColorStateId n2;
 StepFunc func;
} functions[] = {
  {  GDK_COLOR_STATE_ID_SRGB,            GDK_COLOR_STATE_ID_SRGB_LINEAR,    gtk_rgb_to_linear_srgb },
  {  GDK_COLOR_STATE_ID_SRGB,            GDK_COLOR_STATE_ID_HSL,            gtk_rgb_to_hsl },
  {  GDK_COLOR_STATE_ID_SRGB,            GDK_COLOR_STATE_ID_HWB,            gtk_rgb_to_hwb },
  {  GDK_COLOR_STATE_ID_SRGB,            GDK_COLOR_STATE_ID_OKLAB,          gtk_rgb_to_oklab },
  {  GDK_COLOR_STATE_ID_SRGB,            GDK_COLOR_STATE_ID_DISPLAY_P3,     gtk_rgb_to_p3 },
  {  GDK_COLOR_STATE_ID_SRGB,            GDK_COLOR_STATE_ID_XYZ,            srgb_to_xyz },
  {  GDK_COLOR_STATE_ID_SRGB_LINEAR,     GDK_COLOR_STATE_ID_SRGB,           gtk_linear_srgb_to_rgb },
  {  GDK_COLOR_STATE_ID_SRGB_LINEAR,     GDK_COLOR_STATE_ID_OKLAB,          gtk_linear_srgb_to_oklab },
  {  GDK_COLOR_STATE_ID_SRGB_LINEAR,     GDK_COLOR_STATE_ID_XYZ,            gtk_linear_srgb_to_xyz },
  {  GDK_COLOR_STATE_ID_HSL,             GDK_COLOR_STATE_ID_SRGB,           gtk_hsl_to_rgb },
  {  GDK_COLOR_STATE_ID_HSL,             GDK_COLOR_STATE_ID_XYZ,            hsl_to_xyz },
  {  GDK_COLOR_STATE_ID_HWB,             GDK_COLOR_STATE_ID_SRGB,           gtk_hwb_to_rgb },
  {  GDK_COLOR_STATE_ID_HWB,             GDK_COLOR_STATE_ID_XYZ,            hwb_to_xyz },
  {  GDK_COLOR_STATE_ID_OKLAB,           GDK_COLOR_STATE_ID_SRGB,           gtk_oklab_to_rgb },
  {  GDK_COLOR_STATE_ID_OKLAB,           GDK_COLOR_STATE_ID_SRGB_LINEAR,    gtk_oklab_to_linear_srgb },
  {  GDK_COLOR_STATE_ID_OKLAB,           GDK_COLOR_STATE_ID_OKLCH,          gtk_oklab_to_oklch },
  {  GDK_COLOR_STATE_ID_OKLAB,           GDK_COLOR_STATE_ID_XYZ,            oklab_to_xyz },
  {  GDK_COLOR_STATE_ID_OKLCH,           GDK_COLOR_STATE_ID_OKLAB,          gtk_oklch_to_oklab },
  {  GDK_COLOR_STATE_ID_OKLCH,           GDK_COLOR_STATE_ID_XYZ,            oklch_to_xyz },
  {  GDK_COLOR_STATE_ID_DISPLAY_P3,      GDK_COLOR_STATE_ID_SRGB,           gtk_p3_to_rgb },
  {  GDK_COLOR_STATE_ID_DISPLAY_P3,      GDK_COLOR_STATE_ID_XYZ,            p3_to_xyz },
  {  GDK_COLOR_STATE_ID_XYZ,             GDK_COLOR_STATE_ID_SRGB,           xyz_to_srgb },
  {  GDK_COLOR_STATE_ID_XYZ,             GDK_COLOR_STATE_ID_SRGB_LINEAR,    gtk_xyz_to_linear_srgb },
  {  GDK_COLOR_STATE_ID_XYZ,             GDK_COLOR_STATE_ID_HSL,            xyz_to_hsl },
  {  GDK_COLOR_STATE_ID_XYZ,             GDK_COLOR_STATE_ID_HWB,            xyz_to_hwb },
  {  GDK_COLOR_STATE_ID_XYZ,             GDK_COLOR_STATE_ID_OKLAB,          xyz_to_oklab },
  {  GDK_COLOR_STATE_ID_XYZ,             GDK_COLOR_STATE_ID_OKLCH,          xyz_to_oklch },
  {  GDK_COLOR_STATE_ID_XYZ,             GDK_COLOR_STATE_ID_DISPLAY_P3,     xyz_to_p3 },
  {  GDK_COLOR_STATE_ID_XYZ,             GDK_COLOR_STATE_ID_REC2020,        gtk_xyz_to_rec2020 },
  {  GDK_COLOR_STATE_ID_XYZ,             GDK_COLOR_STATE_ID_REC2100_PQ,     xyz_to_rec2100_pq },
  {  GDK_COLOR_STATE_ID_XYZ,             GDK_COLOR_STATE_ID_REC2100_LINEAR, xyz_to_rec2100_linear },
  {  GDK_COLOR_STATE_ID_REC2020,         GDK_COLOR_STATE_ID_XYZ,            gtk_rec2020_to_xyz },
  {  GDK_COLOR_STATE_ID_REC2100_PQ,      GDK_COLOR_STATE_ID_XYZ,            rec2100_pq_to_xyz },
  {  GDK_COLOR_STATE_ID_REC2100_PQ,      GDK_COLOR_STATE_ID_REC2100_LINEAR, gtk_rec2100_pq_to_rec2100_linear },
  {  GDK_COLOR_STATE_ID_REC2100_LINEAR,  GDK_COLOR_STATE_ID_REC2100_PQ,     gtk_rec2100_linear_to_rec2100_pq },
  {  GDK_COLOR_STATE_ID_REC2100_LINEAR,  GDK_COLOR_STATE_ID_XYZ,            rec2100_linear_to_xyz },
};

static StepFunc
find_function (GdkColorStateId n1, GdkColorStateId n2)
{
  for (int k = 0; k < G_N_ELEMENTS (functions); k++)
    {
      if (functions[k].n1 == n1 && functions[k].n2 == n2)
        return functions[k].func;
    }

  return NULL;
}

void
gdk_color_state_transform_init (GdkColorStateTransform *tf,
                                GdkColorState          *from,
                                GdkColorState          *to,
                                gboolean                copy_alpha)
{
  memset (tf, 0, sizeof (GdkColorStateTransform));

  if (GDK_IS_NAMED_COLOR_STATE (from) &&
      GDK_IS_NAMED_COLOR_STATE (to))
    {
      tf->step1 = find_function (GDK_NAMED_COLOR_STATE_ID (from),
                                 GDK_NAMED_COLOR_STATE_ID (to));
      if (tf->step1 == NULL && from != to)
        {
          tf->step1 = find_function (GDK_NAMED_COLOR_STATE_ID (from),
                                     GDK_COLOR_STATE_ID_XYZ);
          tf->step2 = find_function (GDK_COLOR_STATE_ID_XYZ,
                                     GDK_NAMED_COLOR_STATE_ID (to));
        }
    }
  else
    {
      cmsHPROFILE profile1, profile2;

      if (GDK_IS_LCMS_COLOR_STATE (from))
        profile1 = gdk_lcms_color_state_get_lcms_profile (from);
      else
        profile1 = NULL;

      if (GDK_IS_LCMS_COLOR_STATE (to))
        profile2 = gdk_lcms_color_state_get_lcms_profile (to);
      else
        profile2 = NULL;

      if (profile1 && profile2)
        {
          tf->transform = cmsCreateTransform (profile1,
                                              TYPE_RGBA_FLT,
                                              profile2,
                                              TYPE_RGBA_FLT,
                                              INTENT_PERCEPTUAL,
                                              copy_alpha ? cmsFLAGS_COPY_ALPHA : 0);
        }
      else if (GDK_IS_NAMED_COLOR_STATE (from) && profile2)
        {
          tf->step1 = find_function (GDK_NAMED_COLOR_STATE_ID (from),
                                     GDK_COLOR_STATE_ID_XYZ);

          profile1 = cmsCreateXYZProfile ();
          tf->transform = cmsCreateTransform (profile1,
                                              TYPE_RGBA_FLT,
                                              profile2,
                                              TYPE_RGBA_FLT,
                                              INTENT_PERCEPTUAL,
                                              copy_alpha ? cmsFLAGS_COPY_ALPHA : 0);

          tf->cms_first = FALSE;
        }
      else if (profile1 && GDK_IS_NAMED_COLOR_STATE (to))
        {
          profile2 = cmsCreateXYZProfile ();
          tf->transform = cmsCreateTransform (profile1,
                                              TYPE_RGBA_FLT,
                                              profile2,
                                              TYPE_RGBA_FLT,
                                              INTENT_PERCEPTUAL,
                                              copy_alpha ? cmsFLAGS_COPY_ALPHA : 0);

          tf->step1 = find_function (GDK_COLOR_STATE_ID_XYZ,
                                     GDK_NAMED_COLOR_STATE_ID (to));

          tf->cms_first = TRUE;
        }
      else
        g_assert_not_reached ();

      g_clear_pointer (&profile1, cmsCloseProfile);
      g_clear_pointer (&profile2, cmsCloseProfile);
    }

  tf->copy_alpha = copy_alpha;
}

void
gdk_color_state_transform_finish (GdkColorStateTransform *tf)
{
  if (tf->transform)
    cmsDeleteTransform (tf->transform);
}

void
gdk_color_state_transform (GdkColorStateTransform *tf,
                           const float            *src,
                           float                  *dst,
                           int                     width)
{
  if (tf->copy_alpha)
    memcpy (dst, src, sizeof (float) * 4 * width);
  else
    {
      for (int i = 0; i < width * 4; i += 4)
        {
          dst[i] = src[i];
          dst[i + 1] = src[i + 1];
          dst[i + 2] = src[i + 2];
        }
    }

  if (tf->cms_first && tf->transform)
    cmsDoTransform (tf->transform, dst, dst, width);

  if (tf->step1 && tf->step2)
    {
      for (int i = 0; i < width * 4; i += 4)
        {
          tf->step1 (dst[i], dst[i+1], dst[i+2], &dst[i], &dst[i+1], &dst[i+2]);
          tf->step2 (dst[i], dst[i+1], dst[i+2], &dst[i], &dst[i+1], &dst[i+2]);
        }
    }
  else if (tf->step1)
    {
      for (int i = 0; i < width * 4; i += 4)
        tf->step1 (dst[i], dst[i+1], dst[i+2], &dst[i], &dst[i+1], &dst[i+2]);
    }

  if (!tf->cms_first && tf->transform)
    cmsDoTransform (tf->transform, dst, dst, width);
}

/* }}} */

/* vim:set foldmethod=marker expandtab: */
