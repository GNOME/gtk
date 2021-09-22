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

#include "gdkintl.h"

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
 * Since: 4.6
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
gdk_lcms_color_space_get_lcms_profile (GdkLcmsColorSpace *self)
{
  return self->lcms_profile;
}
