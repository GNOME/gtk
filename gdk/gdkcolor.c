/* GDK - The GIMP Drawing Kit
 *
 * Copyright (C) 2021 Benjamin Otte
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

#include "gdkcolorprivate.h"
#include "gdkiccprofileprivate.h"
#include "gdkderivedprofile.h"

static inline cmsHTRANSFORM *
gdk_color_get_transform (GdkICCProfile *src,
                         GdkICCProfile *dest)
{
  cmsHPROFILE *src_profile, *dest_profile;

  src_profile = gdk_icc_profile_get_lcms_profile (GDK_ICC_PROFILE (src));
  dest_profile = gdk_icc_profile_get_lcms_profile (GDK_ICC_PROFILE (dest));

  return gdk_icc_profile_lookup_transform (src,
                                           cmsFormatterForColorspaceOfProfile (src_profile, 4, 1),
                                           dest,
                                           cmsFormatterForColorspaceOfProfile (dest_profile, 4, 1));
}

void
gdk_color_convert (GdkColor        *self,
                   GdkColorProfile *profile,
                   const GdkColor  *other)
{
  GdkColorProfile *src_profile, *dest_profile;
  gsize n_components;
  float *in;

  n_components = gdk_color_profile_get_n_components (profile);

  gdk_color_init (self, profile, other->alpha, NULL, n_components);

  if (GDK_IS_DERIVED_PROFILE (other->profile))
    {
      in = g_alloca (sizeof (float) * n_components);
      gdk_derived_profile_convert_to_base_profile (GDK_DERIVED_PROFILE (other->profile),
                                                   gdk_color_get_components (other),
                                                   in);
      src_profile = gdk_derived_profile_get_base_profile (GDK_DERIVED_PROFILE (other->profile));
    }
  else
    {
      in = (float *)gdk_color_get_components (other);
      src_profile = other->profile;
    }

  if (GDK_IS_DERIVED_PROFILE (profile))
    dest_profile = gdk_derived_profile_get_base_profile (GDK_DERIVED_PROFILE (profile));
  else
    dest_profile = profile;

  cmsDoTransform (gdk_color_get_transform (GDK_ICC_PROFILE (src_profile),
                                           GDK_ICC_PROFILE (dest_profile)),
                  in,
                  (float *) gdk_color_get_components (self),
                  1);

  if (GDK_IS_DERIVED_PROFILE (profile))
    gdk_derived_profile_convert_from_base_profile (GDK_DERIVED_PROFILE (profile),
                                                   gdk_color_get_components (self),
                                                   (float *)gdk_color_get_components (self));
}

void
gdk_color_convert_rgba (GdkColor        *self,
                        GdkColorProfile *profile,
                        const GdkRGBA   *rgba)
{
  GdkColorProfile *dest_profile;
  gsize n_components;

  n_components = gdk_color_profile_get_n_components (profile);
  gdk_color_init (self, profile, rgba->alpha, NULL, n_components);

  if (GDK_IS_DERIVED_PROFILE (profile))
    dest_profile = gdk_derived_profile_get_base_profile (GDK_DERIVED_PROFILE (profile));
  else
    dest_profile = profile;

  cmsDoTransform (gdk_color_get_transform (GDK_ICC_PROFILE (gdk_color_profile_get_srgb ()),
                                           GDK_ICC_PROFILE (dest_profile)),
                  (float[3]) { rgba->red, rgba->green, rgba->blue },
                  (float *) gdk_color_get_components (self),
                  1);

  if (GDK_IS_DERIVED_PROFILE (profile))
    gdk_derived_profile_convert_from_base_profile (GDK_DERIVED_PROFILE (profile),
                                                   gdk_color_get_components (self),
                                                   (float *)gdk_color_get_components (self));
}

void
gdk_color_mix (GdkColor        *self,
               GdkColorProfile *color_profile,
               const GdkColor  *src1,
               const GdkColor  *src2,
               double           progress)
{
  if (src1->profile != color_profile)
    {
      GdkColor tmp;
      gdk_color_convert (&tmp, color_profile, src1);
      gdk_color_mix (self, color_profile, &tmp, src2, progress);
    }
  else if (src2->profile != color_profile)
    {
      GdkColor tmp;
      gdk_color_convert (&tmp, color_profile, src2);
      gdk_color_mix (self, color_profile, src1, &tmp, progress);
    }
  else
    {
      gsize i, n;
      const float *s1, *s2;
      float *d;

      n = gdk_color_profile_get_n_components (color_profile);

      gdk_color_init (self,
                      color_profile, 
                      src1->alpha * (1.0 - progress) + src2->alpha * progress,
                      NULL, n);

      d = (float *) gdk_color_get_components (self);
      s1 = gdk_color_get_components (src1);
      s2 = gdk_color_get_components (src2);

      if (self->alpha == 0)
        {
          for (i = 0; i < n; i++)
            d[i] = s1[i] * (1.0 - progress) + s2[i] * progress;
        }
      else
        {
          for (i = 0; i < n; i++)
            d[i] = (s1[i] * src1->alpha * (1.0 - progress) + s2[i] * src2->alpha * progress) / self->alpha;
        }
    }
}

