/* gdklcmscolorspace.c
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

#include "config.h"

#include "gdklcmscolorspaceprivate.h"
#include "gdkcicpcolorspaceprivate.h"

#include <glib/gi18n-lib.h>

struct _GdkLcmsColorSpace
{
  GdkColorSpace parent_instance;

  cmsHPROFILE lcms_profile;
};

struct _GdkLcmsColorSpaceClass
{
  GdkColorSpaceClass parent_class;
};

G_DEFINE_TYPE (GdkLcmsColorSpace, gdk_lcms_color_space, GDK_TYPE_COLOR_SPACE)

static gboolean
gdk_lcms_color_space_supports_format (GdkColorSpace   *space,
                                      GdkMemoryFormat  format)
{
  GdkLcmsColorSpace *self = GDK_LCMS_COLOR_SPACE (space);

  return cmsGetColorSpace (self->lcms_profile) == cmsSigRgbData;
}

static GBytes *
gdk_lcms_color_space_save_to_icc_profile (GdkColorSpace  *space,
                                          GError        **error)
{
  GdkLcmsColorSpace *self = GDK_LCMS_COLOR_SPACE (space);
  cmsUInt32Number size;
  guchar *data;

  size = 0;
  if (!cmsSaveProfileToMem (self->lcms_profile, NULL, &size))
    {
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Could not prepare ICC profile"));
      return NULL;
    }

  data = g_malloc (size);
  if (!cmsSaveProfileToMem (self->lcms_profile, data, &size))
    {
      g_free (data);
      g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, _("Failed to save ICC profile"));
      return NULL;
    }

  return g_bytes_new_take (data, size);
}

static int
gdk_lcms_color_space_get_n_components (GdkColorSpace *space)
{
  return 3;
}

static gboolean
gdk_lcms_color_space_equal (GdkColorSpace *profile1,
                            GdkColorSpace *profile2)
{
  GBytes *icc1, *icc2;
  gboolean res;

  icc1 = gdk_color_space_save_to_icc_profile (profile1, NULL);
  icc2 = gdk_color_space_save_to_icc_profile (profile2, NULL);

  res = g_bytes_equal (icc1, icc2);

  g_bytes_unref (icc1);
  g_bytes_unref (icc2);

  return res;
}

static void
gdk_lcms_color_space_dispose (GObject *object)
{
  GdkLcmsColorSpace *self = GDK_LCMS_COLOR_SPACE (object);

  g_clear_pointer (&self->lcms_profile, cmsCloseProfile);

  G_OBJECT_CLASS (gdk_lcms_color_space_parent_class)->dispose (object);
}

static void
gdk_lcms_color_space_class_init (GdkLcmsColorSpaceClass *klass)
{
  GdkColorSpaceClass *color_space_class = GDK_COLOR_SPACE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  color_space_class->supports_format = gdk_lcms_color_space_supports_format;
  color_space_class->save_to_icc_profile = gdk_lcms_color_space_save_to_icc_profile;
  color_space_class->get_n_components = gdk_lcms_color_space_get_n_components;
  color_space_class->equal = gdk_lcms_color_space_equal;

  gobject_class->dispose = gdk_lcms_color_space_dispose;
}

static void
gdk_lcms_color_space_init (GdkLcmsColorSpace *self)
{
}

GdkColorSpace *
gdk_lcms_color_space_new_from_lcms_profile (cmsHPROFILE lcms_profile)
{
  GdkLcmsColorSpace *result;

  result = g_object_new (GDK_TYPE_LCMS_COLOR_SPACE, NULL);
  result->lcms_profile = lcms_profile;

  return GDK_COLOR_SPACE (result);
}

/**
 * gdk_color_space_new_from_icc_profile:
 * @icc_profile: The ICC profiles given as a `GBytes`
 * @error: Return location for an error
 *
 * Creates a new color profile for the given ICC profile data.
 *
 * if the profile is not valid, %NULL is returned and an error
 * is raised.
 *
 * Returns: a new `GdkLcmsColorSpace` or %NULL on error
 *
 * Since: 4.10
 */
GdkColorSpace *
gdk_color_space_new_from_icc_profile (GBytes  *icc_profile,
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

  return gdk_lcms_color_space_new_from_lcms_profile (lcms_profile);
}

cmsHPROFILE
gdk_lcms_color_space_get_lcms_profile (GdkColorSpace *self)
{
  g_return_val_if_fail (GDK_IS_LCMS_COLOR_SPACE (self), NULL);

  return GDK_LCMS_COLOR_SPACE (self)->lcms_profile;
}

/**
 * gdk_color_space_get_srgb:
 *
 * Returns the object representing the sRGB color space.
 *
 * If you don't know anything about color spaces but need one for
 * use with some function, this one is most likely the right one.
 *
 * Returns: (transfer none): the object for the sRGB color space.
 *
 * Since: 4.8
 */
GdkColorSpace *
gdk_color_space_get_srgb (void)
{
  static GdkColorSpace *srgb_color_space;

  if (g_once_init_enter (&srgb_color_space))
    {
      GdkColorSpace *color_space;

      color_space = gdk_lcms_color_space_new_from_lcms_profile (cmsCreate_sRGBProfile ());
      g_assert (color_space);

      g_once_init_leave (&srgb_color_space, color_space);
    }

  return srgb_color_space;
}

/*<private>
 * gdk_color_space_get_srgb_linear:
 *
 * Returns the object corresponding to the linear sRGB color space.
 *
 * It can display the same colors as the sRGB color space, but it
 * does not have a gamma curve.
 *
 * Returns: (transfer none): the object for the linear sRGB color space.
 *
 * Since: 4.8
 */
GdkColorSpace *
gdk_color_space_get_srgb_linear (void)
{
  static GdkColorSpace *srgb_linear_color_space;

  if (g_once_init_enter (&srgb_linear_color_space))
    {
      cmsToneCurve *curve;
      cmsHPROFILE lcms_profile;
      GdkColorSpace *color_space;

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

      color_space = gdk_lcms_color_space_new_from_lcms_profile (lcms_profile);
      g_assert (color_space);

      g_once_init_leave (&srgb_linear_color_space, color_space);
    }

  return srgb_linear_color_space;
}

typedef struct _GdkColorTransformCache GdkColorTransformCache;

struct _GdkColorTransformCache
{
  GdkColorSpace *source;
  guint          source_type;
  GdkColorSpace *dest;
  guint          dest_type;
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
gdk_color_space_lookup_transform (GdkColorSpace *source,
                                  guint          source_type,
                                  GdkColorSpace *dest,
                                  guint          dest_type)
{
  GdkColorTransformCache *entry;
  static GHashTable *cache = NULL;
  cmsHTRANSFORM *transform;

  if (GDK_IS_CICP_COLOR_SPACE (dest))
    dest = gdk_cicp_color_space_get_lcms_color_space (dest);

  if (GDK_IS_CICP_COLOR_SPACE (source))
    source = gdk_cicp_color_space_get_lcms_color_space (source);

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
  if (G_UNLIKELY (transform == NULL && source && dest))
    {
      transform = cmsCreateTransform (gdk_lcms_color_space_get_lcms_profile (source),
                                      source_type,
                                      gdk_lcms_color_space_get_lcms_profile (dest),
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
