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
 * can be queried via the [property@Gdk.ColorProfile:icc-profile] property.
 *
 * GDK provides a predefined color profile for the standard sRGB profile,
 * which can be obtained with [func@Gdk.ColorProfile.get_srgb].
 *
 * The main source for color profiles in GTK is loading images which contain
 * ICC profile information. GTK's image loaders will attach the resulting
 * color profiles to the textures they produce.
 *
 * `GdkColorProfile` objects are immutable and therefore threadsafe.
 *
 * Since: 4.8
 */

#include "config.h"

#include "gdkcolorprofileprivate.h"

#include "gdkintl.h"

struct _GdkColorProfile
{
  GObject parent_instance;

  GBytes *icc_profile;
  cmsHPROFILE lcms_profile;

  int color_primaries;
  int matrix_coefficients;
  int transfer_characteristics;
  gboolean full_range;
};

struct _GdkColorProfileClass
{
  GObjectClass parent_class;
};

enum {
  PROP_0,
  PROP_ICC_PROFILE,

  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
gdk_color_profile_real_init (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error)
{
  GdkColorProfile *self = GDK_COLOR_PROFILE (initable);

  if (self->lcms_profile == NULL)
    {
      const guchar *data;
      gsize size;

      if (self->icc_profile == NULL)
        self->icc_profile = g_bytes_new (NULL, 0);

      data = g_bytes_get_data (self->icc_profile, &size);

      self->lcms_profile = cmsOpenProfileFromMem (data, size);
      if (self->lcms_profile == NULL)
        {
          g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Failed to load ICC profile"));
          return FALSE;
        }
    }

  /* FIXME: can we determine these values from icc */
  self->color_primaries = 2;
  self->transfer_characteristics = 2;
  self->matrix_coefficients = 0;
  self->full_range = TRUE;

  return TRUE;
}

static void
gdk_color_profile_initable_init (GInitableIface *iface)
{
  iface->init = gdk_color_profile_real_init;
}


G_DEFINE_TYPE_WITH_CODE (GdkColorProfile, gdk_color_profile, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gdk_color_profile_initable_init))

static void
gdk_color_profile_set_property (GObject      *gobject,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GdkColorProfile *self = GDK_COLOR_PROFILE (gobject);

  switch (prop_id)
    {
    case PROP_ICC_PROFILE:
      self->icc_profile = g_value_dup_boxed (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_color_profile_get_property (GObject    *gobject,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GdkColorProfile *self = GDK_COLOR_PROFILE (gobject);

  switch (prop_id)
    {
    case PROP_ICC_PROFILE:
      g_value_set_boxed (value, self->icc_profile);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gdk_color_profile_dispose (GObject *object)
{
  GdkColorProfile *self = GDK_COLOR_PROFILE (object);

  g_clear_pointer (&self->icc_profile, g_bytes_unref);
  g_clear_pointer (&self->lcms_profile, cmsCloseProfile);

  G_OBJECT_CLASS (gdk_color_profile_parent_class)->dispose (object);
}

static void
gdk_color_profile_class_init (GdkColorProfileClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gdk_color_profile_set_property;
  gobject_class->get_property = gdk_color_profile_get_property;
  gobject_class->dispose = gdk_color_profile_dispose;

  /**
   * GdkColorProfile:icc-profile: (attributes org.gtk.Property.get=gdk_color_profile_get_icc_profile)
   *
   * the ICC profile for this color profile
   */
  properties[PROP_ICC_PROFILE] =
    g_param_spec_boxed ("icc-profile",
                        P_("ICC profile"),
                        P_("ICC profile for this color profile"),
                        G_TYPE_BYTES,
                        G_PARAM_READWRITE |
                        G_PARAM_CONSTRUCT_ONLY |
                        G_PARAM_STATIC_STRINGS |
                        G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (gobject_class, N_PROPS, properties);
}

static void
gdk_color_profile_init (GdkColorProfile *self)
{
}

/**
 * gdk_color_profile_get_srgb:
 *
 * Returns the color profile representing the sRGB color space.
 *
 * If you don't know anything about color profiles but need one for
 * use with some function, this one is most likely the right one.
 *
 * Returns: (transfer none): the color profile for the sRGB
 *   color space.
 *
 * Since: 4.8
 */
GdkColorProfile *
gdk_color_profile_get_srgb (void)
{
  static GdkColorProfile *srgb_profile;

  if (g_once_init_enter (&srgb_profile))
    {
      GdkColorProfile *new_profile;

      new_profile = gdk_color_profile_new_from_lcms_profile (cmsCreate_sRGBProfile (), NULL);
      g_assert (new_profile);

      new_profile->color_primaries = 0;
      new_profile->transfer_characteristics = 13;
      new_profile->matrix_coefficients = 0;
      new_profile->full_range = TRUE;

      g_once_init_leave (&srgb_profile, new_profile);
    }

  return srgb_profile;
}

/*<private>
 * gdk_color_profile_get_srgb_linear:
 *
 * Returns the linear color profile corresponding to the sRGB
 * color space.
 *
 * It can display the same colors, but it does not have a gamma curve.
 *
 * Returns: (transfer none): the color profile for the linear sRGB
 *   color space.
 *
 * Since: 4.8
 */
GdkColorProfile *
gdk_color_profile_get_srgb_linear (void)
{
  static GdkColorProfile *srgb_linear_profile;

  if (g_once_init_enter (&srgb_linear_profile))
    {
      cmsToneCurve *curve;
      cmsHPROFILE lcms_profile;
      GdkColorProfile *new_profile;

      curve = cmsBuildGamma (NULL, 1.0);
      lcms_profile = cmsCreateRGBProfile (&(cmsCIExyY) {
                                            0.3127, 0.3290, 1.0
                                          },
                                          &(cmsCIExyYTRIPLE) {
                                            { 0.6400, 0.3300, 1.0 },
                                            { 0.3000, 0.6000, 1.0 },
                                            { 0.1500, 0.0600, 1.0 }
                                          },
                                          (cmsToneCurve*[3]) { curve, curve, curve });
      cmsFreeToneCurve (curve);

      new_profile = gdk_color_profile_new_from_lcms_profile (lcms_profile, NULL);
      g_assert (new_profile);

      new_profile->color_primaries = 0;
      new_profile->transfer_characteristics = 8;
      new_profile->matrix_coefficients = 0;
      new_profile->full_range = TRUE;

      g_once_init_leave (&srgb_linear_profile, new_profile);
    }

  return srgb_linear_profile;
}

/**
 * gdk_color_profile_new_from_icc_bytes:
 * @bytes: The ICC profiles given as a `GBytes`
 * @error: Return location for an error
 *
 * Creates a new color profile for the given ICC profile data.
 *
 * if the profile is not valid, %NULL is returned and an error
 * is raised.
 *
 * Returns: a new `GdkColorProfile` or %NULL on error
 *
 * Since: 4.8
 */
GdkColorProfile *
gdk_color_profile_new_from_icc_bytes (GBytes  *bytes,
                                      GError **error)
{
  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_initable_new (GDK_TYPE_COLOR_PROFILE,
                         NULL,
                         error,
                         "icc-profile", bytes,
                         NULL);
}

GdkColorProfile *
gdk_color_profile_new_from_lcms_profile (cmsHPROFILE   lcms_profile,
                                         GError      **error)
{
  GdkColorProfile *result;
  cmsUInt32Number size;
  guchar *data;

  size = 0;
  if (!cmsSaveProfileToMem (lcms_profile, NULL, &size))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Could not prepare ICC profile"));
      return NULL;
    }

  data = g_malloc (size);
  if (!cmsSaveProfileToMem (lcms_profile, data, &size))
    {
      g_free (data);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Failed to save ICC profile"));
      return NULL;
    }

  result = g_object_new (GDK_TYPE_COLOR_PROFILE, NULL);
  result->lcms_profile = lcms_profile;
  result->icc_profile = g_bytes_new_take (data, size);

  return result;
}

/**
 * gdk_color_profile_get_icc_profile: (attributes org.gtk.Method.get_property=icc-profile)
 * @self: a `GdkColorProfile`
 *
 * Returns the serialized ICC profile of @self as %GBytes.
 *
 * Returns: (transfer none): the ICC profile
 *
 * Since: 4.8
 */
GBytes *
gdk_color_profile_get_icc_profile (GdkColorProfile *self)
{
  g_return_val_if_fail (GDK_IS_COLOR_PROFILE (self), NULL);

  return self->icc_profile;
}

cmsHPROFILE *
gdk_color_profile_get_lcms_profile (GdkColorProfile *self)
{
  g_return_val_if_fail (GDK_IS_COLOR_PROFILE (self), NULL);

  return self->lcms_profile;
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
 * Since: 4.8
 */
gboolean
gdk_color_profile_is_linear (GdkColorProfile *self)
{
  g_return_val_if_fail (GDK_IS_COLOR_PROFILE (self), FALSE);

  /* FIXME: Make this useful for lcms profiles */
  if (self->transfer_characteristics == 8)
    return TRUE;

  return FALSE;
}

/**
 * gdk_color_profile_get_n_components:
 * @self: a `GdkColorProfile
 *
 * Gets the number of color components - also called channels - for
 * the given profile.
 *
 * Note that this does not consider an alpha channel because color
 * profiles have no alpha information. So for any form of RGB profile,
 * this returned number will be 3, but for CMYK, it will be 4.
 *
 * Returns: The number of components
 *
 * Since: 4.8
 */
gsize
gdk_color_profile_get_n_components (GdkColorProfile *self)
{
  g_return_val_if_fail (GDK_IS_COLOR_PROFILE (self), 3);

  return cmsChannelsOf (cmsGetColorSpace (self->lcms_profile));
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
 * Since: 4.8
 */
gboolean
gdk_color_profile_equal (gconstpointer profile1,
                         gconstpointer profile2)
{
  return profile1 == profile2 ||
         g_bytes_equal (GDK_COLOR_PROFILE (profile1)->icc_profile,
                        GDK_COLOR_PROFILE (profile2)->icc_profile);
}

/* Check if the color profile and the memory format have the
 * same color components.
 */
gboolean
gdk_color_profile_supports_memory_format (GdkColorProfile *profile,
                                          GdkMemoryFormat  format)
{
  /* Currently, all our memory formats are RGB (with or without alpha).
   * Update this when that changes.
   */
  return cmsGetColorSpace (profile->lcms_profile) == cmsSigRgbData;
}

typedef struct _GdkColorTransformCache GdkColorTransformCache;

struct _GdkColorTransformCache
{
  GdkColorProfile *source;
  guint            source_type;
  GdkColorProfile *dest;
  guint            dest_type;
};

static void
gdk_color_transform_cache_free (gpointer data)
{
  g_free (data);
}

static guint
gdk_color_transform_cache_hash (gconstpointer data)
{
  const GdkColorTransformCache *cache = data;

  return g_direct_hash (cache->source) ^
         (g_direct_hash (cache->dest) >> 2) ^
         ((cache->source_type << 16) | (cache->source_type >> 16)) ^
         cache->dest_type;
}

static gboolean
gdk_color_transform_cache_equal (gconstpointer data1,
                                 gconstpointer data2)
{
  const GdkColorTransformCache *cache1 = data1;
  const GdkColorTransformCache *cache2 = data2;

  return cache1->source == cache2->source &&
         cache1->source_type == cache2->source_type &&
         cache1->dest == cache2->dest &&
         cache1->dest_type == cache2->dest_type;
}

cmsHTRANSFORM *
gdk_color_profile_lookup_transform (GdkColorProfile *source,
                                    guint            source_type,
                                    GdkColorProfile *dest,
                                    guint            dest_type)
{
  GdkColorTransformCache *entry;
  static GHashTable *cache = NULL;
  cmsHTRANSFORM *transform;

  if (cache == NULL)
    cache = g_hash_table_new_full (gdk_color_transform_cache_hash,
                                   gdk_color_transform_cache_equal,
                                   gdk_color_transform_cache_free,
                                   cmsDeleteTransform);

  transform = g_hash_table_lookup (cache,
                                   &(GdkColorTransformCache) {
                                     source, source_type,
                                     dest, dest_type
                                   });
  if (G_UNLIKELY (transform == NULL))
    {
      transform = cmsCreateTransform (gdk_color_profile_get_lcms_profile (source),
                                      source_type,
                                      gdk_color_profile_get_lcms_profile (dest),
                                      dest_type,
                                      INTENT_PERCEPTUAL,
                                      cmsFLAGS_COPY_ALPHA);
      entry = g_new (GdkColorTransformCache, 1);
      *entry = (GdkColorTransformCache) {
                 source, source_type,
                 dest, dest_type
               };
      g_hash_table_insert (cache, entry, transform);
    }

  return transform;
}

/**
 * gdk_color_profile_new_from_cicp:
 * @color_primaries: the color primaries to use
 * @transfer_characteristics: the transfer function to use
 * @matrix_coefficients: the matrix coefficients
 * @full_range: whether the data is full-range or not
 *
 * Creates a new color profile from CICP parameters.
 *
 * This function only supports a subset of possible combinations
 * of @color_primaries and @transfer_characteristics.
 *
 * @matrix_coefficients and @full_range must have the values
 * 0 and `TRUE`, since only full-range RGB profiles are
 * supported.
 *
 * Returns: a new `GdkColorProfile` or %NULL on error
 *
 * Since: 4.8
 */
GdkColorProfile *
gdk_color_profile_new_from_cicp (int        color_primaries,
                                 int        transfer_characteristics,
                                 int        matrix_coefficients,
                                 gboolean   full_range,
                                 GError   **error)
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
        return g_object_ref (gdk_color_profile_get_srgb ());
      else if (transfer_characteristics == 8 /* linear */)
        return g_object_ref (gdk_color_profile_get_srgb_linear());
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
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "Unsupported color primaries (%d)", color_primaries);
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
    {
      GdkColorProfile *color_profile;

      color_profile = gdk_color_profile_new_from_lcms_profile (profile, error);

      color_profile->color_primaries = color_primaries;
      color_profile->transfer_characteristics = transfer_characteristics;
      color_profile->matrix_coefficients = matrix_coefficients;
      color_profile->full_range = full_range;

      return color_profile;
    }

  return NULL;
}

/**
 * gdk_color_profile_get_cicp_data:
 * @self: a `GdkColorProfile`
 * @color_primaries: (out): return location for color primaries
 * @transfer_characteristics: (out): return location for transfer characteristics
 * @matrix_coefficients: (out): return location for matrix coefficients
 * @full_range: (out): return location for the full range flag
 *
 * Gets the CICP parameters of a color profile.
 *
 * This is mainly useful for color profiles created via
 * [ctor@Gdk.ColorProfile.new_for_cicp].
 *
 * Note that @color_primaries and @transfer_characteristics will be set to
 * 2 (unspecified) if the color profile does not contain CICP information.
 *
 * Since: 4.8
 */
void
gdk_color_profile_get_cicp_data (GdkColorProfile *self,
                                 int             *color_primaries,
                                 int             *transfer_characteristics,
                                 int             *matrix_coefficients,
                                 gboolean        *full_range)
{
  g_return_if_fail (GDK_IS_COLOR_PROFILE (self));

  *color_primaries = self->color_primaries;
  *transfer_characteristics = self->transfer_characteristics;
  *matrix_coefficients = self->matrix_coefficients;
  *full_range = self->full_range;
}
