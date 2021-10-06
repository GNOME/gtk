/* gdkiccprofile.c
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
 * GdkICCProfile:
 *
 * `GdkICCProfile` is used to describe ICC color profiles.
 *
 * It is used to associate color profiles defined by the [International
 * Color Consortium (ICC)](https://color.org/) with texture and color data.
 *
 * Each `GdkColorProfile` encapsulates an
 * [ICC profile](https://en.wikipedia.org/wiki/ICC_profile). That profile
 * can be queried via the using [property@Gdk.ColorProfile:icc-profile]
 * property.
 *
 * `GdkICCProfile` objects are immutable and therefore threadsafe.
 *
 * Since: 4.6
 */

#include "config.h"

#include "gdkiccprofileprivate.h"
#include "gdkcolorprofileprivate.h"

#include "gdkintl.h"

struct _GdkICCProfile
{
  GdkColorProfile parent_instance;

  GBytes *icc_profile;
  cmsHPROFILE lcms_profile;
};

struct _GdkICCProfileClass
{
  GdkColorProfileClass parent_class;
};

enum {
  PROP_0,
  PROP_ICC_PROFILE,

  N_PROPS
};

static GParamSpec *properties[N_PROPS];

static gboolean
gdk_icc_profile_real_init (GInitable     *initable,
                           GCancellable  *cancellable,
                           GError       **error)
{
  GdkICCProfile *self = GDK_ICC_PROFILE (initable);

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

  return TRUE;
}

static void
gdk_icc_profile_initable_init (GInitableIface *iface)
{
  iface->init = gdk_icc_profile_real_init;
}


G_DEFINE_TYPE_WITH_CODE (GdkICCProfile, gdk_icc_profile, GDK_TYPE_COLOR_PROFILE,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE, gdk_icc_profile_initable_init))

static void
gdk_icc_profile_set_property (GObject      *gobject,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  GdkICCProfile *self = GDK_ICC_PROFILE (gobject);

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
gdk_icc_profile_get_property (GObject    *gobject,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  GdkICCProfile *self = GDK_ICC_PROFILE (gobject);

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
gdk_icc_profile_dispose (GObject *object)
{
  GdkICCProfile *self = GDK_ICC_PROFILE (object);

  g_clear_pointer (&self->icc_profile, g_bytes_unref);
  g_clear_pointer (&self->lcms_profile, cmsCloseProfile);

  G_OBJECT_CLASS (gdk_icc_profile_parent_class)->dispose (object);
}

static gboolean
gdk_icc_profile_is_linear (GdkColorProfile *profile)
{
  return profile == gdk_color_profile_get_srgb_linear ();
}

static gsize
gdk_icc_profile_get_n_components (GdkColorProfile *profile)
{
  GdkICCProfile *self = GDK_ICC_PROFILE (profile);

  return cmsChannelsOf (cmsGetColorSpace (self->lcms_profile));
}

static gboolean
gdk_icc_profile_equal (gconstpointer profile1,
                         gconstpointer profile2)
{
  return g_bytes_equal (GDK_ICC_PROFILE (profile1)->icc_profile,
                        GDK_ICC_PROFILE (profile2)->icc_profile);
}

static void
gdk_icc_profile_class_init (GdkICCProfileClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GdkColorProfileClass *profile_class = GDK_COLOR_PROFILE_CLASS (klass);

  gobject_class->set_property = gdk_icc_profile_set_property;
  gobject_class->get_property = gdk_icc_profile_get_property;
  gobject_class->dispose = gdk_icc_profile_dispose;

  profile_class->is_linear = gdk_icc_profile_is_linear;
  profile_class->get_n_components = gdk_icc_profile_get_n_components;
  profile_class->equal = gdk_icc_profile_equal;

  /**
   * GdkICCProfile:icc-profile: (attributes org.gtk.Property.get=gdk_icc_profile_get_icc_profile)
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
gdk_icc_profile_init (GdkICCProfile *self)
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
 * Since: 4.6
 */
GdkColorProfile *
gdk_color_profile_get_srgb (void)
{
  static GdkColorProfile *srgb_profile;

  if (g_once_init_enter (&srgb_profile))
    {
      GdkColorProfile *new_profile;

      new_profile = GDK_COLOR_PROFILE (gdk_icc_profile_new_from_lcms_profile (cmsCreate_sRGBProfile (), NULL));
      g_assert (new_profile);

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

      new_profile = GDK_COLOR_PROFILE (gdk_icc_profile_new_from_lcms_profile (lcms_profile, NULL));
      g_assert (new_profile);

      g_once_init_leave (&srgb_linear_profile, new_profile);
    }

  return srgb_linear_profile;
}

/**
 * gdk_icc_profile_new_from_icc_bytes:
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
 * Since: 4.6
 */
GdkICCProfile *
gdk_icc_profile_new_from_icc_bytes (GBytes  *bytes,
                                    GError **error)
{
  g_return_val_if_fail (bytes != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return g_initable_new (GDK_TYPE_ICC_PROFILE,
                         NULL,
                         error,
                         "icc-profile", bytes,
                         NULL);
}

GdkICCProfile *
gdk_icc_profile_new_from_lcms_profile (cmsHPROFILE   lcms_profile,
                                       GError      **error)
{
  GdkICCProfile *result;
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

  result = g_object_new (GDK_TYPE_ICC_PROFILE, NULL);
  result->lcms_profile = lcms_profile;
  result->icc_profile = g_bytes_new_take (data, size);

  return result;
}

/**
 * gdk_icc_profile_get_icc_profile: (attributes org.gtk.Method.get_property=icc-profile)
 * @self: a `GdkICCProfile`
 *
 * Returns the serialized ICC profile of @self as %GBytes.
 *
 * Returns: (transfer none): the ICC profile
 *
 * Since: 4.6
 */
GBytes *
gdk_icc_profile_get_icc_profile (GdkICCProfile *self)
{
  g_return_val_if_fail (GDK_IS_ICC_PROFILE (self), NULL);

  return self->icc_profile;
}

cmsHPROFILE *
gdk_icc_profile_get_lcms_profile (GdkICCProfile *self)
{
  g_return_val_if_fail (GDK_IS_ICC_PROFILE (self), NULL);

  return self->lcms_profile;
}


typedef struct _GdkICCTransformCache GdkICCTransformCache;

struct _GdkICCTransformCache
{
  GdkICCProfile *source;
  guint          source_type;
  GdkICCProfile *dest;
  guint          dest_type;
};

static void
gdk_icc_transform_cache_free (gpointer data)
{
  g_free (data);
}

static guint
gdk_icc_transform_cache_hash (gconstpointer data)
{
  const GdkICCTransformCache *cache = data;

  return g_direct_hash (cache->source) ^
         (g_direct_hash (cache->dest) >> 2) ^
         ((cache->source_type << 16) | (cache->source_type >> 16)) ^
         cache->dest_type;
}

static gboolean
gdk_icc_transform_cache_equal (gconstpointer data1,
                               gconstpointer data2)
{
  const GdkICCTransformCache *cache1 = data1;
  const GdkICCTransformCache *cache2 = data2;

  return cache1->source == cache2->source &&
         cache1->source_type == cache2->source_type &&
         cache1->dest == cache2->dest &&
         cache1->dest_type == cache2->dest_type;
}

cmsHTRANSFORM *
gdk_icc_profile_lookup_transform (GdkICCProfile *source,
                                  guint          source_type,
                                  GdkICCProfile *dest,
                                  guint          dest_type)
{
  GdkICCTransformCache *entry;
  static GHashTable *cache = NULL;
  cmsHTRANSFORM *transform;

  if (cache == NULL)
    cache = g_hash_table_new_full (gdk_icc_transform_cache_hash,
                                   gdk_icc_transform_cache_equal,
                                   gdk_icc_transform_cache_free,
                                   cmsDeleteTransform);

  transform = g_hash_table_lookup (cache,
                                   &(GdkICCTransformCache) {
                                     source, source_type,
                                     dest, dest_type
                                   });
  if (G_UNLIKELY (transform == NULL))
    {
      transform = cmsCreateTransform (gdk_icc_profile_get_lcms_profile (source),
                                      source_type,
                                      gdk_icc_profile_get_lcms_profile (dest),
                                      dest_type,
                                      INTENT_PERCEPTUAL,
                                      cmsFLAGS_COPY_ALPHA);
      entry = g_new (GdkICCTransformCache, 1);
      *entry = (GdkICCTransformCache) {
                 source, source_type,
                 dest, dest_type
               };
      g_hash_table_insert (cache, entry, transform);
    }

  return transform;
}
