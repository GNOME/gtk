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
#include "gdkcolorspaceprivate.h"
#include "gdklcmscolorspaceprivate.h"

#include <lcms2.h>

static inline cmsHTRANSFORM *
gdk_color_get_transform (GdkColorSpace *src,
                         GdkColorSpace *dest)
{
  return gdk_color_space_lookup_transform (src, TYPE_RGBA_FLT, dest, TYPE_RGBA_FLT);
}

void
gdk_color_convert (GdkColor        *self,
                   GdkColorSpace   *color_space,
                   const GdkColor  *other)
{
  gdk_color_init (self,
                  color_space,
                  other->alpha,
                  NULL,
                  gdk_color_space_get_n_components (color_space));

  cmsDoTransform (gdk_color_get_transform (other->color_space, color_space),
                  gdk_color_get_components (other),
                  (float *) gdk_color_get_components (self),
                  1);
}

void
gdk_color_convert_rgba (GdkColor        *self,
                        GdkColorSpace   *color_space,
                        const GdkRGBA   *rgba)
{
  gdk_color_init (self,
                  color_space,
                  rgba->alpha,
                  NULL,
                  gdk_color_space_get_n_components (color_space));

  cmsDoTransform (gdk_color_get_transform (gdk_color_space_get_srgb (), color_space),
                  (float[3]) { rgba->red, rgba->green, rgba->blue },
                  (float *) gdk_color_get_components (self),
                  1);
}

void
gdk_color_mix (GdkColor        *self,
               GdkColorSpace   *color_space,
               const GdkColor  *src1,
               const GdkColor  *src2,
               double           progress)
{
  if (src1->color_space != color_space)
    {
      GdkColor tmp;
      gdk_color_convert (&tmp, color_space, src1);
      gdk_color_mix (self, color_space, &tmp, src2, progress);
    }
  else if (src2->color_space != color_space)
    {
      GdkColor tmp;
      gdk_color_convert (&tmp, color_space, src2);
      gdk_color_mix (self, color_space, src1, &tmp, progress);
    }
  else
    {
      gsize i, n;
      const float *s1, *s2;
      float *d;

      n = gdk_color_space_get_n_components (color_space);

      gdk_color_init (self,
                      color_space,
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
