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

#include "gdknamedcolorstateprivate.h"
#include "gdklcmscolorstateprivate.h"

#include "gtk/gtkcolorutilsprivate.h"


G_DEFINE_TYPE (GdkColorState, gdk_color_state, G_TYPE_OBJECT)

static void
gdk_color_state_init (GdkColorState *self)
{
}

static GBytes *
gdk_color_state_default_save_to_icc_profile (GdkColorState  *self,
                                             GError        **error)
{
  g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                       _("This color state does not support ICC profiles"));
  return NULL;
}

static gboolean
gdk_color_state_default_equal (GdkColorState *self,
                               GdkColorState *other)
{
  return self == other;
}

static const char *
gdk_color_state_default_get_name (GdkColorState *self)
{
  return "color state";
}

static GdkMemoryDepth
gdk_color_state_default_get_min_depth (GdkColorState *self)
{
  return GDK_MEMORY_U8;
}

static void
gdk_color_state_class_init (GdkColorStateClass *class)
{
  class->save_to_icc_profile = gdk_color_state_default_save_to_icc_profile;
  class->equal = gdk_color_state_default_equal;
  class->get_name = gdk_color_state_default_get_name;
  class->get_min_depth = gdk_color_state_default_get_min_depth;
}

/**
 * gdk_color_state_equal:
 * @cs1: a `GdkColorState`
 * @cs2: another `GdkColorStatee`
 *
 * Compares two `GdkColorStates` for equality.
 *
 * Note that this function is not guaranteed to be perfect and two objects
 * describing the same color state may compare not equal. However, different
 * color state will never compare equal.
 *
 * Returns: %TRUE if the two color states compare equal
 *
 * Since: 4.16
 */
gboolean
gdk_color_state_equal (GdkColorState *cs1,
                       GdkColorState *cs2)
{
  if (cs1 == cs2)
    return TRUE;

  if (GDK_COLOR_STATE_GET_CLASS (cs1) != GDK_COLOR_STATE_GET_CLASS (cs2))
    return FALSE;

  return GDK_COLOR_STATE_GET_CLASS (cs1)->equal (cs1, cs2);
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
  g_return_val_if_fail (GDK_IS_COLOR_STATE (self), FALSE);

  return self == gdk_color_state_get_srgb_linear ();
}

/**
 * gdk_color_state_save_to_icc_profile:
 * @self: a `GdkColorState`
 * @error: Return location for an error
 *
 * Saves the color state to an
 * [ICC profile](https://en.wikipedia.org/wiki/ICC_profile).
 *
 * Some color state cannot be represented as ICC profiles. In
 * that case, an error will be set and %NULL will be returned.
 *
 * Returns: (nullable): A new `GBytes` containing the ICC profile
 *
 * Since: 4.16
 */
GBytes *
gdk_color_state_save_to_icc_profile (GdkColorState  *self,
                                     GError        **error)
{
  g_return_val_if_fail (GDK_IS_COLOR_STATE (self), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  return GDK_COLOR_STATE_GET_CLASS (self)->save_to_icc_profile (self, error);
}

GdkMemoryDepth
gdk_color_state_get_min_depth (GdkColorState *self)
{
  return GDK_COLOR_STATE_GET_CLASS (self)->get_min_depth (self);
}

const char *
gdk_color_state_get_name (GdkColorState *self)
{
  return GDK_COLOR_STATE_GET_CLASS (self)->get_name (self);
}

typedef void (* StepFunc) (float  s0, float  s1, float  s2,
                           float *d0, float *d1, float *d2);


struct _GdkColorStateTransform
{
  cmsHTRANSFORM transform;
  int n_funcs;
  StepFunc funcs[10];
  gboolean cms_first;
  gboolean copy_alpha;
};

static void
transform_list_append (GdkColorStateTransform *tf,
                       StepFunc                func)
{
  g_assert (tf->n_funcs < G_N_ELEMENTS (tf->funcs));
  tf->funcs[tf->n_funcs] = func;
  tf->n_funcs++;
}

static struct {
  GdkColorStateId n1, n2;
  StepFunc func;
} functions[] = {
  { GDK_COLOR_STATE_HWB, GDK_COLOR_STATE_SRGB, gtk_hwb_to_rgb },
  { GDK_COLOR_STATE_SRGB, GDK_COLOR_STATE_HWB, gtk_rgb_to_hwb },
  { GDK_COLOR_STATE_HSL, GDK_COLOR_STATE_SRGB, gtk_hsl_to_rgb },
  { GDK_COLOR_STATE_SRGB, GDK_COLOR_STATE_HSL, gtk_rgb_to_hsl },
  { GDK_COLOR_STATE_SRGB_LINEAR, GDK_COLOR_STATE_SRGB, gtk_rgb_to_linear_srgb },
  { GDK_COLOR_STATE_SRGB, GDK_COLOR_STATE_SRGB_LINEAR, gtk_linear_srgb_to_rgb },
  { GDK_COLOR_STATE_SRGB_LINEAR, GDK_COLOR_STATE_OKLAB, gtk_linear_srgb_to_oklab },
  { GDK_COLOR_STATE_OKLAB, GDK_COLOR_STATE_SRGB_LINEAR, gtk_oklab_to_linear_srgb },
  { GDK_COLOR_STATE_OKLAB, GDK_COLOR_STATE_OKLCH, gtk_oklab_to_oklch },
  { GDK_COLOR_STATE_OKLCH, GDK_COLOR_STATE_OKLAB, gtk_oklch_to_oklab },
};

static gpointer
find_function (int i, int j)
{
  for (int k = 0; k < G_N_ELEMENTS (functions); k++)
    {
      if (functions[k].n1 == i && functions[k].n2 == j)
        return functions[k].func;
    }

  return NULL;
}

static GdkColorStateId line1[] = { GDK_COLOR_STATE_SRGB,
                                   GDK_COLOR_STATE_SRGB_LINEAR,
                                   GDK_COLOR_STATE_OKLAB,
                                   GDK_COLOR_STATE_OKLCH };
static GdkColorStateId line2[] = { GDK_COLOR_STATE_HSL,
                                   GDK_COLOR_STATE_SRGB,
                                   GDK_COLOR_STATE_HWB};

static void
collect_functions (GdkColorStateTransform *tf,
                   GdkColorStateId         line[],
                   int                     si,
                   int                     di)
{
  int inc = di < si ? -1 : 1;
  int i0, i;

  i0 = i = si;
  do
    {
      i += inc;
      transform_list_append (tf, find_function (line[i0], line[i]));
      i0 = i;
    }
   while (i != di);
}

static void
get_transform_list (GdkColorStateTransform *tf,
                    GdkColorState          *from,
                    GdkColorState          *to)
{
  GdkColorStateId sn, dn;
  int si, di, sii, dii;

  sn = gdk_named_color_state_get_id (from);
  dn = gdk_named_color_state_get_id (to);

  for (int i = 0; i < G_N_ELEMENTS (line1); i++)
    {
      if (line1[i] == sn)
        si = i;
      if (line1[i] == dn)
        di = i;
    }

  if (si > -1 && di > -1)
    {
      collect_functions (tf, line1, si, di);
      return;
    }

  for (int i = 0; i < G_N_ELEMENTS (line2); i++)
    {
      if (line2[i] == sn)
        sii = i;
      if (line2[i] == dn)
        dii = i;
    }

  if (sii > -1 && dii > -1)
    {
      collect_functions (tf, line2, sii, dii);
      return;
    }

  if (si > -1 && dii > -1)
    {
      collect_functions (tf, line1, si, 0);
      collect_functions (tf, line2, 1, dii);
      return;
    }

  if (sii > -1 && di > -1)
    {
      collect_functions (tf, line2, sii, 1);
      collect_functions (tf, line2, 0, di);
      return;
    }
}

GdkColorStateTransform *
gdk_color_state_get_transform (GdkColorState *from,
                               GdkColorState *to,
                               gboolean       copy_alpha)
{
  GdkColorStateTransform *tf;

  tf = g_new0 (GdkColorStateTransform, 1);

  if (gdk_color_state_equal (from, to))
    {
    }
  else if (GDK_IS_LCMS_COLOR_STATE (from) &&
           GDK_IS_LCMS_COLOR_STATE (to))
    {
      tf->transform = cmsCreateTransform (gdk_lcms_color_state_get_lcms_profile (from),
                                          TYPE_RGBA_FLT,
                                          gdk_lcms_color_state_get_lcms_profile (to),
                                          TYPE_RGBA_FLT,
                                          INTENT_PERCEPTUAL,
                                          copy_alpha ? cmsFLAGS_COPY_ALPHA : 0);
    }
  else if (GDK_IS_NAMED_COLOR_STATE (from) &&
           GDK_IS_NAMED_COLOR_STATE (to))
    {
      get_transform_list (tf, from, to);
    }
  else if (GDK_IS_NAMED_COLOR_STATE (from) &&
           GDK_IS_LCMS_COLOR_STATE (to))
    {
      cmsHPROFILE profile;

      get_transform_list (tf, from, gdk_color_state_get_srgb ());

      profile = cmsCreate_sRGBProfile ();
      tf->transform = cmsCreateTransform (profile,
                                          TYPE_RGBA_FLT,
                                          gdk_lcms_color_state_get_lcms_profile (to),
                                          TYPE_RGBA_FLT,
                                          INTENT_PERCEPTUAL,
                                          copy_alpha ? cmsFLAGS_COPY_ALPHA : 0);
      cmsCloseProfile (profile);

      tf->cms_first = FALSE;
    }
  else if (GDK_IS_LCMS_COLOR_STATE (from) &&
           GDK_IS_NAMED_COLOR_STATE (to))
    {
      cmsHPROFILE profile;

      profile = cmsCreate_sRGBProfile ();
      tf->transform = cmsCreateTransform (gdk_lcms_color_state_get_lcms_profile (from),
                                          TYPE_RGBA_FLT,
                                          profile,
                                          TYPE_RGBA_FLT,
                                          INTENT_PERCEPTUAL,
                                          copy_alpha ? cmsFLAGS_COPY_ALPHA : 0);
      cmsCloseProfile (profile);

      get_transform_list (tf, gdk_color_state_get_srgb (), to);

      tf->cms_first = TRUE;
    }

  tf->copy_alpha = copy_alpha;

  return tf;
}

void
gdk_color_state_transform_free (GdkColorStateTransform *tf)
{
  if (tf->transform)
    cmsDeleteTransform (tf->transform);
  g_free (tf);
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

  if (tf->n_funcs)
    {
      for (int i = 0; i < width * 4; i += 4)
        {
          float s0 = dst[i];
          float s1 = dst[i + 1];
          float s2 = dst[i + 2];

          for (int k = 0; k < tf->n_funcs; k++)
            tf->funcs[k] (s0, s1, s2, &s0, &s1, &s2);

          dst[i]     = s0;
          dst[i + 1] = s1;
          dst[i + 2] = s2;
        }
    }

  if (!tf->cms_first && tf->transform)
    cmsDoTransform (tf->transform, dst, dst, width);
}
