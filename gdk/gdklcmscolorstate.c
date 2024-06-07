/* gdklcmscolorstate.c
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

#include "gdklcmscolorstateprivate.h"

#include <glib/gi18n-lib.h>

struct _GdkLcmsColorState
{
  GdkColorState parent_instance;

  cmsHPROFILE lcms_profile;
};

struct _GdkLcmsColorStateClass
{
  GdkColorStateClass parent_class;
};

G_DEFINE_TYPE (GdkLcmsColorState, gdk_lcms_color_state, GDK_TYPE_COLOR_STATE)

static GBytes *
gdk_lcms_color_state_save_to_icc_profile (GdkColorState  *state,
                                          GError        **error)
{
  GdkLcmsColorState *self = GDK_LCMS_COLOR_STATE (state);
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

static gboolean
gdk_lcms_color_state_equal (GdkColorState *cs1,
                            GdkColorState *cs2)
{
  GBytes *icc1, *icc2;
  gboolean res;

  icc1 = gdk_color_state_save_to_icc_profile (cs1, NULL);
  icc2 = gdk_color_state_save_to_icc_profile (cs2, NULL);

  res = g_bytes_equal (icc1, icc2);

  g_bytes_unref (icc1);
  g_bytes_unref (icc2);

  return res;
}

static void
gdk_lcms_color_state_dispose (GObject *object)
{
  GdkLcmsColorState *self = GDK_LCMS_COLOR_STATE (object);

  g_clear_pointer (&self->lcms_profile, cmsCloseProfile);

  G_OBJECT_CLASS (gdk_lcms_color_state_parent_class)->dispose (object);
}

static const char *
gdk_lcms_color_state_get_name (GdkColorState *self)
{
  static char buffer[48];

  g_snprintf (buffer, sizeof (buffer), "lcms color state %p", self);

  return buffer;
}

static void
gdk_lcms_color_state_class_init (GdkLcmsColorStateClass *klass)
{
  GdkColorStateClass *color_state_class = GDK_COLOR_STATE_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gdk_lcms_color_state_dispose;

  color_state_class->save_to_icc_profile = gdk_lcms_color_state_save_to_icc_profile;
  color_state_class->equal = gdk_lcms_color_state_equal;
  color_state_class->get_name = gdk_lcms_color_state_get_name;
}

static void
gdk_lcms_color_state_init (GdkLcmsColorState *self)
{
}

GdkColorState *
gdk_lcms_color_state_new_from_lcms_profile (cmsHPROFILE lcms_profile)
{
  GdkLcmsColorState *result;

  result = g_object_new (GDK_TYPE_LCMS_COLOR_STATE, NULL);
  result->lcms_profile = lcms_profile;

  return GDK_COLOR_STATE (result);
}

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

  return gdk_lcms_color_state_new_from_lcms_profile (lcms_profile);
}

cmsHPROFILE
gdk_lcms_color_state_get_lcms_profile (GdkColorState *self)
{
  g_return_val_if_fail (GDK_IS_LCMS_COLOR_STATE (self), NULL);

  return GDK_LCMS_COLOR_STATE (self)->lcms_profile;
}

typedef struct _GdkColorTransformCache GdkColorTransformCache;

struct _GdkColorTransformCache
{
  GdkColorState *source;
  guint          source_type;
  GdkColorState *dest;
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
gdk_lcms_color_state_lookup_transform (GdkColorState *source,
                                       guint          source_type,
                                       GdkColorState *dest,
                                       guint          dest_type)
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
  if (G_UNLIKELY (transform == NULL && source && dest))
    {
      transform = cmsCreateTransform (gdk_lcms_color_state_get_lcms_profile (source),
                                      source_type,
                                      gdk_lcms_color_state_get_lcms_profile (dest),
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
