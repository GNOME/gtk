/* gdkcolorstateimpl.h
 *
 * Copyright 2024 Red Hat, Inc.
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

#pragma once

typedef enum
{
  GDK_COLOR_STATE_ID_SRGB           =  1,
  GDK_COLOR_STATE_ID_SRGB_LINEAR    =  3,
  GDK_COLOR_STATE_ID_HSL            =  5,
  GDK_COLOR_STATE_ID_HWB            =  7,
  GDK_COLOR_STATE_ID_OKLAB          =  9,
  GDK_COLOR_STATE_ID_OKLCH          = 11,
  GDK_COLOR_STATE_ID_DISPLAY_P3     = 13,
  GDK_COLOR_STATE_ID_XYZ            = 15,
  GDK_COLOR_STATE_ID_REC2020        = 17,
  GDK_COLOR_STATE_ID_REC2100_PQ     = 19,
  GDK_COLOR_STATE_ID_REC2100_LINEAR = 21,
} GdkColorStateId;

enum
{
  GDK_TYPE_NAMED_COLOR_STATE,
  GDK_TYPE_LCMS_COLOR_STATE,
};

typedef struct _GdkColorStateClass GdkColorStateClass;

struct _GdkColorState
{
  GdkColorStateClass *klass;
  int ref_count;
};

struct _GdkColorStateClass
{
  int type;

  void                  (* free)                (GdkColorState  *self);
  gboolean              (* equal)               (GdkColorState  *self,
                                                 GdkColorState  *other);
  gboolean              (* is_linear)           (GdkColorState  *self);
  GBytes *              (* save_to_icc_profile) (GdkColorState  *self,
                                                 GError        **error);
  gboolean              (* save_to_cicp_data)   (GdkColorState  *self,
                                                 int            *color_primaries,
                                                 int            *transfer_characteristics,
                                                 int            *matrix_coefficients,
                                                 gboolean       *full_range,
                                                 GError        **error);
  const char *          (* get_name)            (GdkColorState  *self);
  guint                 (* get_min_depth)       (GdkColorState  *self);
  int                   (* get_hue_coord)       (GdkColorState  *self);
};

#define GDK_NAMED_COLOR_STATE_ID(c) ((GdkColorStateId) (GPOINTER_TO_INT (c)))
#define GDK_IS_NAMED_COLOR_STATE(c) ((GPOINTER_TO_INT(c)) & 1)
#define GDK_IS_LCMS_COLOR_STATE(c) ((c)->klass->type == GDK_TYPE_LCMS_COLOR_STATE)

#define gdk_color_state_ref(self) _gdk_color_state_ref (self)
static inline GdkColorState *
_gdk_color_state_ref (GdkColorState *self)
{
  if (GDK_IS_NAMED_COLOR_STATE (self))
    return self;

  self->ref_count++;

  return self;
}

#define gdk_color_state_unref(self) _gdk_color_state_unref (self)
static inline void
_gdk_color_state_unref (GdkColorState *self)
{
  if (GDK_IS_NAMED_COLOR_STATE (self))
    return;

  self->ref_count--;

  if (self->ref_count == 0)
    self->klass->free (self);
}

#define GDK_COLOR_STATE_SRGB ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_SRGB))
#define GDK_COLOR_STATE_SRGB_LINEAR ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_SRGB_LINEAR))
#define GDK_COLOR_STATE_HSL ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_HSL))
#define GDK_COLOR_STATE_HWB ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_HWB))
#define GDK_COLOR_STATE_OKLAB ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_OKLAB))
#define GDK_COLOR_STATE_OKLCH ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_OKLCH))
#define GDK_COLOR_STATE_DISPLAY_P3 ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_DISPLAY_P3))
#define GDK_COLOR_STATE_XYZ ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_XYZ))
#define GDK_COLOR_STATE_REC2020 ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_REC2020))
#define GDK_COLOR_STATE_REC2100_PQ ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_REC2100_PQ))
#define GDK_COLOR_STATE_REC2100_LINEAR ((GdkColorState *) GINT_TO_POINTER (GDK_COLOR_STATE_ID_REC2100_LINEAR))

#define gdk_color_state_get_srgb() _gdk_color_state_get_srgb ()
static inline GdkColorState *
_gdk_color_state_get_srgb (void)
{
  return GDK_COLOR_STATE_SRGB;
}

#define gdk_color_state_get_srgb_linear() _gdk_color_state_get_srgb_linear ()
static inline GdkColorState *
_gdk_color_state_get_srgb_linear (void)
{
  return GDK_COLOR_STATE_SRGB_LINEAR;
}

#define gdk_color_state_get_hsl() _gdk_color_state_get_hsl ()
static inline GdkColorState *
_gdk_color_state_get_hsl (void)
{
  return GDK_COLOR_STATE_HSL;
}

#define gdk_color_state_get_hwb() _gdk_color_state_get_hwb ()
static inline GdkColorState *
_gdk_color_state_get_hwb (void)
{
  return GDK_COLOR_STATE_HWB;
}

#define gdk_color_state_get_oklab() _gdk_color_state_get_oklab ()
static inline GdkColorState *
_gdk_color_state_get_oklab (void)
{
  return GDK_COLOR_STATE_OKLAB;
}

#define gdk_color_state_get_oklch() _gdk_color_state_get_oklch ()
static inline GdkColorState *
_gdk_color_state_get_oklch (void)
{
  return GDK_COLOR_STATE_OKLCH;
}

#define gdk_color_state_get_display_p3() _gdk_color_state_get_display_p3 ()
static inline GdkColorState *
_gdk_color_state_get_display_p3 (void)
{
  return GDK_COLOR_STATE_DISPLAY_P3;
}

#define gdk_color_state_get_xyz() _gdk_color_state_get_xyz ()
static inline GdkColorState *
_gdk_color_state_get_xyz (void)
{
  return GDK_COLOR_STATE_XYZ;
}

#define gdk_color_state_get_rec2020() _gdk_color_state_get_rec2020 ()
static inline GdkColorState *
_gdk_color_state_get_rec2020 (void)
{
  return GDK_COLOR_STATE_REC2020;
}

#define gdk_color_state_get_rec2100_pq() _gdk_color_state_get_rec2100_pq ()
static inline GdkColorState *
_gdk_color_state_get_rec2100_pq (void)
{
  return GDK_COLOR_STATE_REC2100_PQ;
}

#define gdk_color_state_get_rec2100_linear() _gdk_color_state_get_rec2100_linear ()
static inline GdkColorState *
_gdk_color_state_get_rec2100_linear (void)
{
  return GDK_COLOR_STATE_REC2100_LINEAR;
}

#define gdk_color_state_equal(a,b) _gdk_color_state_equal ((a), (b))
static inline gboolean
_gdk_color_state_equal (GdkColorState *self,
                        GdkColorState *other)
{
  if (self == other)
    return TRUE;

  if (GDK_IS_NAMED_COLOR_STATE (self) || GDK_IS_NAMED_COLOR_STATE (other))
    return FALSE;

  if (self->klass != other->klass)
    return FALSE;

  return self->klass->equal (self, other);
}
