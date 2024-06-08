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
#include "gdkcolorstateprivate.h"
#include "gdklcmscolorstateprivate.h"

#include <lcms2.h>

static inline cmsHTRANSFORM *
gdk_color_get_transform (GdkColorState *src,
                         GdkColorState *dest)
{
  return gdk_lcms_color_state_lookup_transform (src, TYPE_RGBA_FLT, dest, TYPE_RGBA_FLT);
}

void
gdk_color_convert (GdkColor        *self,
                   GdkColorState   *color_state,
                   const GdkColor  *other)
{
  gdk_color_init (self,
                  color_state,
                  other->alpha,
                  NULL);

  cmsDoTransform (gdk_color_get_transform (other->color_state, color_state),
                  gdk_color_get_components (other),
                  (float *) gdk_color_get_components (self), 1);
}

void
gdk_color_convert_rgba (GdkColor        *self,
                        GdkColorState   *color_state,
                        const GdkRGBA   *rgba)
{
  gdk_color_init (self,
                  color_state,
                  rgba->alpha,
                  NULL);

  cmsDoTransform (gdk_color_get_transform (gdk_color_state_get_srgb (), color_state),
                  (float[3]) { rgba->red, rgba->green, rgba->blue },
                  (float *) gdk_color_get_components (self),
                  1);
}

void
gdk_color_mix (GdkColor        *self,
               GdkColorState   *color_state,
               const GdkColor  *src1,
               const GdkColor  *src2,
               double           progress)
{
  if (src1->color_state != color_state)
    {
      GdkColor tmp;
      gdk_color_convert (&tmp, color_state, src1);
      gdk_color_mix (self, color_state, &tmp, src2, progress);
    }
  else if (src2->color_state != color_state)
    {
      GdkColor tmp;
      gdk_color_convert (&tmp, color_state, src2);
      gdk_color_mix (self, color_state, src1, &tmp, progress);
    }
  else
    {
      gsize i;
      const float *s1, *s2;
      float *d;

      gdk_color_init (self,
                      color_state,
                      src1->alpha * (1.0 - progress) + src2->alpha * progress,
                      NULL);

      d = (float *) gdk_color_get_components (self);
      s1 = gdk_color_get_components (src1);
      s2 = gdk_color_get_components (src2);

      if (self->alpha == 0)
        {
          for (i = 0; i < 3; i++)
            d[i] = s1[i] * (1.0 - progress) + s2[i] * progress;
        }
      else
        {
          for (i = 0; i < 3; i++)
            d[i] = (s1[i] * src1->alpha * (1.0 - progress) + s2[i] * src2->alpha * progress) / self->alpha;
        }
    }
}
