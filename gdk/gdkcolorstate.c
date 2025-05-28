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

#define GDK_COLOR_STATE_IMPL
#include "gdkcolorstateprivate.h"

#include <math.h>

#include "gdkcolordefs.h"

#include <glib/gi18n-lib.h>

/**
 * GdkColorState:
 *
 * Provides information to interpret colors and pixels in a variety of ways.
 *
 * They are also known as
 * [*color spaces*](https://en.wikipedia.org/wiki/Color_space).
 *
 * Crucially, GTK knows how to convert colors from one color
 * state to another.
 *
 * `GdkColorState` objects are immutable and therefore threadsafe.
 *
 * Since: 4.16
 */

G_DEFINE_BOXED_TYPE (GdkColorState, gdk_color_state,
                     gdk_color_state_ref, gdk_color_state_unref);

/* {{{ Public API */

/**
 * gdk_color_state_ref:
 * @self: a `GdkColorState`
 *
 * Increase the reference count of @self.
 *
 * Returns: the object that was passed in
 *
 * Since: 4.16
 */
GdkColorState *
(gdk_color_state_ref) (GdkColorState *self)
{
  return _gdk_color_state_ref (self);
}

/**
 * gdk_color_state_unref:
 * @self:a `GdkColorState`
 *
 * Decrease the reference count of @self.
 *
 * Unless @self is static, it will be freed
 * when the reference count reaches zero.
 *
 * Since: 4.16
 */
void
(gdk_color_state_unref) (GdkColorState *self)
{
  _gdk_color_state_unref (self);
}

/**
 * gdk_color_state_get_srgb:
 *
 * Returns the color state object representing the sRGB color space.
 *
 * This color state uses the primaries defined by BT.709-6 and the transfer function
 * defined by IEC 61966-2-1.
 *
 * It is equivalent to the [Cicp](class.CicpParams.html) tuple 1/13/0/1.
 *
 * See e.g. [the CSS Color Module](https://www.w3.org/TR/css-color-4/#predefined-sRGB)
 * for details about this colorstate.
 *
 * Returns: the color state object for sRGB
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_srgb (void)
{
  return GDK_COLOR_STATE_SRGB;
}

/**
 * gdk_color_state_get_srgb_linear:
 *
 * Returns the color state object representing the linearized sRGB color space.
 *
 * This color state uses the primaries defined by BT.709-6 and a linear transfer function.
 *
 * It is equivalent to the [Cicp](class.CicpParams.html) tuple 1/8/0/1.
 *
 * See e.g. [the CSS Color Module](https://www.w3.org/TR/css-color-4/#predefined-sRGB-linear)
 * for details about this colorstate.
 *
 * Returns: the color state object for linearized sRGB
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_srgb_linear (void)
{
  return GDK_COLOR_STATE_SRGB_LINEAR;
}

/**
 * gdk_color_state_get_rec2100_pq:
 *
 * Returns the color state object representing the rec2100-pq color space.
 *
 * This color state uses the primaries defined by BT.2020-2 and BT.2100-0 and the transfer
 * function defined by SMPTE ST 2084 and BT.2100-2.
 *
 * It is equivalent to the [Cicp](class.CicpParams.html) tuple 9/16/0/1.
 *
 * See e.g. [the CSS HDR Module](https://drafts.csswg.org/css-color-hdr/#valdef-color-rec2100-pq)
 * for details about this colorstate.
 *
 * Returns: the color state object for rec2100-pq
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_rec2100_pq (void)
{
  return GDK_COLOR_STATE_REC2100_PQ;
}

/**
 * gdk_color_state_get_rec2100_linear:
 *
 * Returns the color state object representing the linear rec2100 color space.
 *
 * This color state uses the primaries defined by BT.2020-2 and BT.2100-0 and a linear
 * transfer function.
 *
 * It is equivalent to the [Cicp](class.CicpParams.html) tuple 9/8/0/1.
 *
 * See e.g. [the CSS HDR Module](https://drafts.csswg.org/css-color-hdr/#valdef-color-rec2100-linear)
 * for details about this colorstate.
 *
 * Returns: the color state object for linearized rec2100
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_rec2100_linear (void)
{
  return GDK_COLOR_STATE_REC2100_LINEAR;
}

/**
 * gdk_color_state_get_oklab:
 *
 * Returns the color state object representing the oklab color space.
 *
 * This is a perceptually uniform color state.
 *
 * Returns: the color state object for oklab
 *
 * Since: 4.18
 */
GdkColorState *
gdk_color_state_get_oklab (void)
{
  return GDK_COLOR_STATE_OKLAB;
}

/**
 * gdk_color_state_get_oklch:
 *
 * Returns the color state object representing the oklch color space.
 *
 * This is the polar variant of oklab, in which the hue is encoded as
 * a polar coordinate.
 *
 * Returns: the color state object for oklch
 *
 * Since: 4.18
 */
GdkColorState *
gdk_color_state_get_oklch (void)
{
  return GDK_COLOR_STATE_OKLCH;
}

/**
 * gdk_color_state_equal:
 * @self: a `GdkColorState`
 * @other: another `GdkColorStatee`
 *
 * Compares two `GdkColorStates` for equality.
 *
 * Note that this function is not guaranteed to be perfect and two objects
 * describing the same color state may compare not equal. However, different
 * color states will never compare equal.
 *
 * Returns: %TRUE if the two color states compare equal
 *
 * Since: 4.16
 */
gboolean
(gdk_color_state_equal) (GdkColorState *self,
                         GdkColorState *other)
{
  return _gdk_color_state_equal (self, other);
}

/**
 * gdk_color_state_equivalent:
 * @self: a `GdkColorState`
 * @other: another `GdkColorStatee`
 *
 * Compares two `GdkColorStates` for equivalence.
 *
 * Two objects that represent the same color state should be equivalent,
 * even though they may not be equal in the sense of [method@Gdk.ColorState.equal].
 *
 * Returns: %TRUE if the two color states are equivalent
 *
 * Since: 4.20
 */
gboolean
(gdk_color_state_equivalent) (GdkColorState *self,
                              GdkColorState *other)
{
  return _gdk_color_state_equivalent (self, other);
}

/**
 * gdk_color_state_create_cicp_params:
 * @self: a `GdkColorState`
 *
 * Create a [class@Gdk.CicpParams] representing the colorstate.
 *
 * It is not guaranteed that every `GdkColorState` can be
 * represented with Cicp parameters. If that is the case,
 * this function returns `NULL`.
 *
 * Returns: (transfer full) (nullable): A new [class@Gdk.CicpParams]
 *
 * Since: 4.16
 */
GdkCicpParams *
gdk_color_state_create_cicp_params (GdkColorState *self)
{
  const GdkCicp *cicp = gdk_color_state_get_cicp (self);

  if (cicp)
    return gdk_cicp_params_new_for_cicp (cicp);

  return NULL;
}

/* }}} */
/* {{{ Conversion functions */

typedef float (* GdkTransferFunc) (float v);
typedef void  (* GdkConvertFunc)  (GdkColorState *self,
                                   float          values[4]);
typedef const float GdkColorMatrix[9];

#define IDENTITY ((float*)0)
#define NONE ((GdkTransferFunc)0)

#define CONVERT_FUNC(name) \
static void \
gdk_convert_ ## name (GdkColorState  *self, \
                      float         (*values)[4], \
                      gsize           n_values) \
{ \
  for (gsize i = 0; i < n_values; i++) \
    { \
      name (self, values[i]); \
    } \
}

#define TRANSFORM(name, eotf, matrix, nonlinear, matrix2, oetf) \
static inline void \
name (GdkColorState  *self, \
      float           values[4]) \
{ \
  if (eotf != NONE) \
    { \
      values[0] = eotf (values[0]); \
      values[1] = eotf (values[1]); \
      values[2] = eotf (values[2]); \
    } \
  if (matrix != IDENTITY) \
    { \
      float res[3]; \
      res[0] = matrix[0] * values[0] + matrix[1] * values[1] + matrix[2] * values[2]; \
      res[1] = matrix[3] * values[0] + matrix[4] * values[1] + matrix[5] * values[2]; \
      res[2] = matrix[6] * values[0] + matrix[7] * values[1] + matrix[8] * values[2]; \
      values[0] = res[0]; \
      values[1] = res[1]; \
      values[2] = res[2]; \
    } \
  if (nonlinear != NONE) \
    { \
      values[0] = nonlinear (values[0]); \
      values[1] = nonlinear (values[1]); \
      values[2] = nonlinear (values[2]); \
    } \
  if (matrix2 != IDENTITY) \
    { \
      float res[3]; \
      res[0] = matrix2[0] * values[0] + matrix2[1] * values[1] + matrix2[2] * values[2]; \
      res[1] = matrix2[3] * values[0] + matrix2[4] * values[1] + matrix2[5] * values[2]; \
      res[2] = matrix2[6] * values[0] + matrix2[7] * values[1] + matrix2[8] * values[2]; \
      values[0] = res[0]; \
      values[1] = res[1]; \
      values[2] = res[2]; \
    } \
  if (oetf != NONE) \
    { \
      values[0] = oetf (values[0]); \
      values[1] = oetf (values[1]); \
      values[2] = oetf (values[2]); \
    } \
} \
CONVERT_FUNC (name)

#define TRANSFORM_PAIR(name, func1, func2) \
static inline void \
name (GdkColorState *self, \
      float          values[4]) \
{ \
  func1 (self, values); \
  func2 (self, values); \
} \
CONVERT_FUNC (name)

TRANSFORM(srgb_to_srgb_linear,           srgb_eotf, IDENTITY,        NONE, IDENTITY, NONE)
TRANSFORM(srgb_linear_to_srgb,           NONE,      IDENTITY,        NONE, IDENTITY, srgb_oetf)
TRANSFORM(rec2100_pq_to_rec2100_linear,  pq_eotf,   IDENTITY,        NONE, IDENTITY, NONE)
TRANSFORM(rec2100_linear_to_rec2100_pq,  NONE,      IDENTITY,        NONE, IDENTITY, pq_oetf)
TRANSFORM(srgb_linear_to_rec2100_linear, NONE,      srgb_to_rec2020, NONE, IDENTITY, NONE)
TRANSFORM(rec2100_linear_to_srgb_linear, NONE,      rec2020_to_srgb, NONE, IDENTITY, NONE)
TRANSFORM(srgb_to_rec2100_linear,        srgb_eotf, srgb_to_rec2020, NONE, IDENTITY, NONE)
TRANSFORM(rec2100_pq_to_srgb_linear,     pq_eotf,   rec2020_to_srgb, NONE, IDENTITY, NONE)
TRANSFORM(srgb_linear_to_rec2100_pq,     NONE,      srgb_to_rec2020, NONE, IDENTITY, pq_oetf)
TRANSFORM(rec2100_linear_to_srgb,        NONE,      rec2020_to_srgb, NONE, IDENTITY, srgb_oetf)
TRANSFORM(srgb_to_rec2100_pq,            srgb_eotf, srgb_to_rec2020, NONE, IDENTITY, pq_oetf)
TRANSFORM(rec2100_pq_to_srgb,            pq_eotf,   rec2020_to_srgb, NONE, IDENTITY, srgb_oetf)

TRANSFORM(oklab_to_srgb_linear,          NONE,      oklab_to_lms,    from_oklab_nl, lms_to_srgb,    NONE)
TRANSFORM(oklab_to_srgb,                 NONE,      oklab_to_lms,    from_oklab_nl, lms_to_srgb,    srgb_oetf)
TRANSFORM(oklab_to_rec2100_linear,       NONE,      oklab_to_lms,    from_oklab_nl, lms_to_rec2020, NONE)
TRANSFORM(oklab_to_rec2100_pq,           NONE,      oklab_to_lms,    from_oklab_nl, lms_to_rec2020, pq_oetf)
TRANSFORM(srgb_linear_to_oklab,          NONE,      srgb_to_lms,     to_oklab_nl,   lms_to_oklab,   NONE)
TRANSFORM(srgb_to_oklab,                 srgb_eotf, srgb_to_lms,     to_oklab_nl,   lms_to_oklab,   NONE)
TRANSFORM(rec2100_linear_to_oklab,       NONE,      rec2020_to_lms,  to_oklab_nl,   lms_to_oklab,   NONE)
TRANSFORM(rec2100_pq_to_oklab,           pq_eotf,   rec2020_to_lms,  to_oklab_nl,   lms_to_oklab,   NONE)

#define DEG_TO_RAD(x) ((x) * G_PI / 180)
#define RAD_TO_DEG(x) ((x) * 180 / G_PI)

static inline void
_sincosf (float  angle,
          float *out_s,
          float *out_c)
{
#ifdef HAVE_SINCOSF
  sincosf (angle, out_s, out_c);
#else
  *out_s = sinf (angle);
  *out_c = cosf (angle);
#endif
}

static void
oklch_to_oklab (GdkColorState *self,
                float          values[4])
{
  float L, C, H, a, b;

  L = values[0];
  C = values[1];
  H = values[2];

  _sincosf (DEG_TO_RAD (H), &b, &a);
  a *= C;
  b *= C;

  values[0] = L;
  values[1] = a;
  values[2] = b;
}

static void
oklab_to_oklch (GdkColorState *self,
                float          values[4])
{
  float L, a, b, C, H;

  L = values[0];
  a = values[1];
  b = values[2];

  C = hypotf (a, b);
  H = RAD_TO_DEG (atan2 (b, a));

  H = fmod (H, 360);
  if (H < 0)
    H += 360;

  values[0] = L;
  values[1] = C;
  values[2] = H;
}

TRANSFORM_PAIR (srgb_to_oklch,           srgb_to_oklab,           oklab_to_oklch)
TRANSFORM_PAIR (srgb_linear_to_oklch,    srgb_linear_to_oklab,    oklab_to_oklch)
TRANSFORM_PAIR (rec2100_pq_to_oklch,     rec2100_pq_to_oklab,     oklab_to_oklch)
TRANSFORM_PAIR (rec2100_linear_to_oklch, rec2100_linear_to_oklab, oklab_to_oklch)
TRANSFORM_PAIR (oklch_to_srgb,           oklch_to_oklab,          oklab_to_srgb)
TRANSFORM_PAIR (oklch_to_srgb_linear,    oklch_to_oklab,          oklab_to_srgb_linear)
TRANSFORM_PAIR (oklch_to_rec2100_pq,     oklch_to_oklab,          oklab_to_rec2100_pq)
TRANSFORM_PAIR (oklch_to_rec2100_linear, oklch_to_oklab,          oklab_to_rec2100_pq)

/* }}} */
/* {{{ Default implementation */
/* {{{ Vfuncs */

static gboolean
gdk_default_color_state_equal (GdkColorState *self,
                               GdkColorState *other)
{
  return self == other;
}

static const char *
gdk_default_color_state_get_name (GdkColorState *color_state)
{
  GdkDefaultColorState *self = (GdkDefaultColorState *) color_state;

  return self->name;
}

static GdkColorState *
gdk_default_color_state_get_no_srgb_tf (GdkColorState *color_state)
{
  GdkDefaultColorState *self = (GdkDefaultColorState *) color_state;

  return self->no_srgb;
}

static GdkFloatColorConvert
gdk_default_color_state_get_convert_to (GdkColorState  *color_state,
                                        GdkColorState  *target)
{
  GdkDefaultColorState *self = (GdkDefaultColorState *) color_state;

  if (GDK_IS_DEFAULT_COLOR_STATE (target))
    return self->convert_to[GDK_DEFAULT_COLOR_STATE_ID (target)];

  return NULL;
}

static GdkFloatColorConvert
gdk_default_color_state_get_convert_from (GdkColorState  *color_state,
                                          GdkColorState  *source)
{
  /* This is ok because the default-to-default conversion functions
   * don't use the passed colorstate at all.
   */
  return gdk_default_color_state_get_convert_to (source, color_state);
}

static const GdkCicp *
gdk_default_color_state_get_cicp (GdkColorState *color_state)
{
  GdkDefaultColorState *self = (GdkDefaultColorState *) color_state;

  if (self->cicp.color_primaries == 0)
    return NULL;

  return &self->cicp;
}

static gboolean
gdk_color_state_check_inf_nan (const float src[4],
                               float       dest[4])
{
  if (isnan (src[0]) ||
      isnan (src[1]) ||
      isnan (src[2]) ||
      isnan (src[3]))
    {
      dest = (float[4]) { 1.0, 0.0, 0.8, 1.0 };
      return TRUE;
    }
  if (isinf (src[0]) ||
      isinf (src[1]) ||
      isinf (src[2]) ||
      isinf (src[3]))
    {
      dest = (float[4]) { 0.0, 0.8, 1.0, 1.0 };
      return TRUE;
    }

  return FALSE;
}

static void
gdk_color_state_clamp_0_1 (GdkColorState *self,
                           const float    src[4],
                           float          dest[4])
{
  if (gdk_color_state_check_inf_nan (src, dest))
    return;

  dest[0] = CLAMP (src[0], 0.0f, 1.0f);
  dest[1] = CLAMP (src[1], 0.0f, 1.0f);
  dest[2] = CLAMP (src[2], 0.0f, 1.0f);
  dest[3] = CLAMP (src[3], 0.0f, 1.0f);
}

static void
gdk_color_state_clamp_unbounded (GdkColorState *self,
                                 const float    src[4],
                                 float          dest[4])
{
  if (gdk_color_state_check_inf_nan (src, dest))
    return;

  dest[0] = src[0];
  dest[1] = src[1];
  dest[2] = src[2];
  dest[3] = CLAMP (src[3], 0.0f, 1.0f);
}

static void
gdk_default_color_state_clamp (GdkColorState *color_state,
                               const float    in[4],
                               float          out[4])
{
  GdkDefaultColorState *self = (GdkDefaultColorState *) color_state;

  self->clamp (color_state, in, out);
}

/* }}} */

static const
GdkColorStateClass GDK_DEFAULT_COLOR_STATE_CLASS = {
  .free = NULL, /* crash here if this ever happens */
  .equal = gdk_default_color_state_equal,
  .get_name = gdk_default_color_state_get_name,
  .get_no_srgb_tf = gdk_default_color_state_get_no_srgb_tf,
  .get_convert_to = gdk_default_color_state_get_convert_to,
  .get_convert_from = gdk_default_color_state_get_convert_from,
  .get_cicp = gdk_default_color_state_get_cicp,
  .clamp = gdk_default_color_state_clamp,
};

GdkDefaultColorState gdk_default_color_states[] = {
  [GDK_COLOR_STATE_ID_SRGB] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_U8_SRGB,
      .rendering_color_state = GDK_COLOR_STATE_SRGB,
      .rendering_color_state_linear = GDK_COLOR_STATE_SRGB_LINEAR,
    },
    .name = "srgb",
    .no_srgb = GDK_COLOR_STATE_SRGB_LINEAR,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_convert_srgb_to_srgb_linear,
      [GDK_COLOR_STATE_ID_REC2100_PQ] = gdk_convert_srgb_to_rec2100_pq,
      [GDK_COLOR_STATE_ID_REC2100_LINEAR] = gdk_convert_srgb_to_rec2100_linear,
    },
    .clamp = gdk_color_state_clamp_0_1,
    .cicp = { 1, 13, 0, 1 },
  },
  [GDK_COLOR_STATE_ID_SRGB_LINEAR] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_U8,
      .rendering_color_state = GDK_COLOR_STATE_SRGB_LINEAR,
      .rendering_color_state_linear = GDK_COLOR_STATE_SRGB_LINEAR,
    },
    .name = "srgb-linear",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_convert_srgb_linear_to_srgb,
      [GDK_COLOR_STATE_ID_REC2100_PQ] = gdk_convert_srgb_linear_to_rec2100_pq,
      [GDK_COLOR_STATE_ID_REC2100_LINEAR] = gdk_convert_srgb_linear_to_rec2100_linear,
    },
    .clamp = gdk_color_state_clamp_0_1,
    .cicp = { 1, 8, 0, 1 },
  },
  [GDK_COLOR_STATE_ID_REC2100_PQ] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_REC2100_PQ,
      .rendering_color_state_linear = GDK_COLOR_STATE_REC2100_LINEAR,
    },
    .name = "rec2100-pq",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_convert_rec2100_pq_to_srgb,
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_convert_rec2100_pq_to_srgb_linear,
      [GDK_COLOR_STATE_ID_REC2100_LINEAR] = gdk_convert_rec2100_pq_to_rec2100_linear,
    },
    .clamp = gdk_color_state_clamp_0_1,
    .cicp = { 9, 16, 0, 1 },
  },
  [GDK_COLOR_STATE_ID_REC2100_LINEAR] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_REC2100_LINEAR,
      .rendering_color_state_linear = GDK_COLOR_STATE_REC2100_LINEAR,
    },
    .name = "rec2100-linear",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_convert_rec2100_linear_to_srgb,
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_convert_rec2100_linear_to_srgb_linear,
      [GDK_COLOR_STATE_ID_REC2100_PQ] = gdk_convert_rec2100_linear_to_rec2100_pq,
    },
    .clamp = gdk_color_state_clamp_unbounded,
    .cicp = { 9, 8, 0, 1 },
  },
};

static gboolean
gdk_builtin_color_state_equal (GdkColorState *self,
                               GdkColorState *other)
{
  return self == other;
}

static const char *
gdk_builtin_color_state_get_name (GdkColorState *color_state)
{
  GdkBuiltinColorState *self = (GdkBuiltinColorState *) color_state;

  return self->name;
}

static GdkColorState *
gdk_builtin_color_state_get_no_srgb_tf (GdkColorState *color_state)
{
  return NULL;
}

static GdkFloatColorConvert
gdk_builtin_color_state_get_convert_to (GdkColorState  *color_state,
                                        GdkColorState  *target)
{
  GdkBuiltinColorState *self = (GdkBuiltinColorState *) color_state;

  return self->convert_to[GDK_DEFAULT_COLOR_STATE_ID (target)];
}

static GdkFloatColorConvert
gdk_builtin_color_state_get_convert_from (GdkColorState  *color_state,
                                          GdkColorState  *source)
{
  GdkBuiltinColorState *self = (GdkBuiltinColorState *) color_state;

  return self->convert_from[GDK_DEFAULT_COLOR_STATE_ID (source)];
}

static const GdkCicp *
gdk_builtin_color_state_get_cicp (GdkColorState *color_state)
{
  return NULL;
}

/* }}} */

static const
GdkColorStateClass GDK_BUILTIN_COLOR_STATE_CLASS = {
  .free = NULL, /* crash here if this ever happens */
  .equal = gdk_builtin_color_state_equal,
  .get_name = gdk_builtin_color_state_get_name,
  .get_no_srgb_tf = gdk_builtin_color_state_get_no_srgb_tf,
  .get_convert_to = gdk_builtin_color_state_get_convert_to,
  .get_convert_from = gdk_builtin_color_state_get_convert_from,
  .get_cicp = gdk_builtin_color_state_get_cicp,
  .clamp = gdk_color_state_clamp_unbounded,
};

GdkBuiltinColorState gdk_builtin_color_states[] = {
  [GDK_BUILTIN_COLOR_STATE_ID_OKLAB] = {
    .parent = {
      .klass = &GDK_BUILTIN_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_SRGB,
    },
    .name = "oklab",
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_convert_oklab_to_srgb,
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_convert_oklab_to_srgb_linear,
      [GDK_COLOR_STATE_ID_REC2100_PQ] = gdk_convert_oklab_to_rec2100_pq,
      [GDK_COLOR_STATE_ID_REC2100_LINEAR] = gdk_convert_oklab_to_rec2100_linear,
    },
    .convert_from = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_convert_srgb_to_oklab,
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_convert_srgb_linear_to_oklab,
      [GDK_COLOR_STATE_ID_REC2100_PQ] = gdk_convert_rec2100_pq_to_oklab,
      [GDK_COLOR_STATE_ID_REC2100_LINEAR] = gdk_convert_rec2100_linear_to_oklab,
    },
  },
  [GDK_BUILTIN_COLOR_STATE_ID_OKLCH] = {
    .parent = {
      .klass = &GDK_BUILTIN_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_SRGB,
    },
    .name = "oklch",
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_convert_oklch_to_srgb,
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_convert_oklch_to_srgb_linear,
      [GDK_COLOR_STATE_ID_REC2100_PQ] = gdk_convert_oklch_to_rec2100_pq,
      [GDK_COLOR_STATE_ID_REC2100_LINEAR] = gdk_convert_oklch_to_rec2100_linear,
    },
    .convert_from = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_convert_srgb_to_oklch,
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_convert_srgb_linear_to_oklch,
      [GDK_COLOR_STATE_ID_REC2100_PQ] = gdk_convert_rec2100_pq_to_oklch,
      [GDK_COLOR_STATE_ID_REC2100_LINEAR] = gdk_convert_rec2100_linear_to_oklch,
    },
  },
};

/* }}} */
/* {{{ Cicp implementation */

typedef struct _GdkCicpColorState GdkCicpColorState;
struct _GdkCicpColorState
{
  GdkColorState parent;

  GdkColorState *no_srgb;

  char *name;

  GdkTransferFunc eotf;
  GdkTransferFunc oetf;

  float to_srgb[9];
  float to_rec2020[9];
  float from_srgb[9];
  float from_rec2020[9];

  const float *from_yuv;
  const float *to_yuv;

  GdkCicp cicp;
};

/* {{{ Conversion functions */

#define TRANSFORM_FROM_CICP(name, matrix, oetf) \
static void \
name (GdkColorState  *color_state, \
      float         (*values)[4], \
      gsize           n_values) \
{ \
  GdkCicpColorState *self = (GdkCicpColorState *) color_state; \
\
  transform_from_cicp (self, oetf, self->matrix, values, n_values); \
}

static void
transform_from_cicp (GdkCicpColorState *self,
                     GdkTransferFunc    oetf,
                     float              matrix[9],
                     float            (*values)[4],
                     gsize              n_values)
{
  for (gsize i = 0; i < n_values; i++)
    {
      if (self->cicp.range == GDK_CICP_RANGE_NARROW)
        {
          values[i][0] = CLAMP ((values[i][0] - 16.0/255.0) * 255.0 / 224.0, 0.0, 1.0);
          values[i][1] = CLAMP ((values[i][1] - 16.0/255.0) * 255.0 / 219.0, 0.0, 1.0);
          values[i][2] = CLAMP ((values[i][2] - 16.0/255.0) * 255.0 / 224.0, 0.0, 1.0);
        }
      if (self->from_yuv)
        {
          float res[3];
          values[i][0] -= 0.5;
          values[i][2] -= 0.5;
          res[0] = self->from_yuv[0] * values[i][0] + self->from_yuv[1] * values[i][1] + self->from_yuv[2] * values[i][2];
          res[1] = self->from_yuv[3] * values[i][0] + self->from_yuv[4] * values[i][1] + self->from_yuv[5] * values[i][2];
          res[2] = self->from_yuv[6] * values[i][0] + self->from_yuv[7] * values[i][1] + self->from_yuv[8] * values[i][2];
          values[i][0] = res[0];
          values[i][1] = res[1];
          values[i][2] = res[2];
        }
      if (self->eotf != NONE)
        {
          values[i][0] = self->eotf (values[i][0]);
          values[i][1] = self->eotf (values[i][1]);
          values[i][2] = self->eotf (values[i][2]);
        }
      if (matrix != IDENTITY)
        {
          float res[3];
          res[0] = matrix[0] * values[i][0] + matrix[1] * values[i][1] + matrix[2] * values[i][2];
          res[1] = matrix[3] * values[i][0] + matrix[4] * values[i][1] + matrix[5] * values[i][2];
          res[2] = matrix[6] * values[i][0] + matrix[7] * values[i][1] + matrix[8] * values[i][2];
          values[i][0] = res[0];
          values[i][1] = res[1];
          values[i][2] = res[2];
        }
      if (oetf != NONE)
        {
          values[i][0] = oetf (values[i][0]);
          values[i][1] = oetf (values[i][1]);
          values[i][2] = oetf (values[i][2]);
        }
    }
}

#define TRANSFORM_TO_CICP(name, eotf, matrix) \
static void \
name (GdkColorState  *color_state, \
      float         (*values)[4], \
      gsize           n_values) \
{ \
  GdkCicpColorState *self = (GdkCicpColorState *) color_state; \
\
  transform_to_cicp (self, eotf, self->matrix, values, n_values); \
}

static void
transform_to_cicp (GdkCicpColorState *self,
                   GdkTransferFunc    eotf,
                   float              matrix[9],
                   float            (*values)[4],
                   gsize              n_values)
{
  for (gsize i = 0; i < n_values; i++)
    {
      if (eotf != NONE)
        {
          values[i][0] = eotf (values[i][0]);
          values[i][1] = eotf (values[i][1]);
          values[i][2] = eotf (values[i][2]);
        }
      if (matrix != IDENTITY)
        {
          float res[3];
          res[0] = matrix[0] * values[i][0] + matrix[1] * values[i][1] + matrix[2] * values[i][2];
          res[1] = matrix[3] * values[i][0] + matrix[4] * values[i][1] + matrix[5] * values[i][2];
          res[2] = matrix[6] * values[i][0] + matrix[7] * values[i][1] + matrix[8] * values[i][2];
          values[i][0] = res[0];
          values[i][1] = res[1];
          values[i][2] = res[2];
        }
      else
        {
          float tmp = values[i][2];
          values[i][2] = values[i][0];
          values[i][0] = values[i][1];
          values[i][1] = tmp;
        }
      if (self->oetf != NONE)
        {
          values[i][0] = self->oetf (values[i][0]);
          values[i][1] = self->oetf (values[i][1]);
          values[i][2] = self->oetf (values[i][2]);
        }
      if (self->to_yuv)
        {
          float res[3];
          res[0] = self->to_yuv[0] * values[i][0] + self->to_yuv[1] * values[i][1] + self->to_yuv[2] * values[i][2];
          res[1] = self->to_yuv[3] * values[i][0] + self->to_yuv[4] * values[i][1] + self->to_yuv[5] * values[i][2];
          res[2] = self->to_yuv[6] * values[i][0] + self->to_yuv[7] * values[i][1] + self->to_yuv[8] * values[i][2];
          values[i][0] = res[0] + 0.5;
          values[i][1] = res[1];
          values[i][2] = res[2] + 0.5;
        }
      values[i][0] = CLAMP (values[i][0], 0.0, 1.0);
      values[i][1] = CLAMP (values[i][1], 0.0, 1.0);
      values[i][2] = CLAMP (values[i][2], 0.0, 1.0);
      if (self->cicp.range == GDK_CICP_RANGE_NARROW)
        {
          values[i][0] = values[i][0] * 224.0 / 255.0 + 16.0 / 255.0;
          values[i][1] = values[i][1] * 219.0 / 255.0 + 16.0 / 255.0;
          values[i][2] = values[i][2] * 224.0 / 255.0 + 16.0 / 255.0;
        }
    }
}

TRANSFORM_FROM_CICP(gdk_convert_cicp_to_srgb,           to_srgb,      srgb_oetf)
TRANSFORM_FROM_CICP(gdk_convert_cicp_to_srgb_linear,    to_srgb,      NONE)
TRANSFORM_FROM_CICP(gdk_convert_cicp_to_rec2100_pq,     to_rec2020,   pq_oetf)
TRANSFORM_FROM_CICP(gdk_convert_cicp_to_rec2100_linear, to_rec2020,   NONE)

TRANSFORM_TO_CICP(gdk_convert_cicp_from_srgb,           srgb_eotf,   from_srgb)
TRANSFORM_TO_CICP(gdk_convert_cicp_from_srgb_linear,    NONE,        from_srgb)
TRANSFORM_TO_CICP(gdk_convert_cicp_from_rec2100_pq,     pq_eotf,     from_rec2020)
TRANSFORM_TO_CICP(gdk_convert_cicp_from_rec2100_linear, NONE,        from_rec2020)

/* }}} */
/* {{{ Vfuncs */

static void
gdk_cicp_color_state_free (GdkColorState *cs)
{
  GdkCicpColorState *self = (GdkCicpColorState *) cs;

  g_free (self->name);

  if (self->no_srgb)
    gdk_color_state_unref (self->no_srgb);

  g_free (self);
}

static gboolean
gdk_cicp_color_state_equal (GdkColorState *self,
                            GdkColorState *other)
{
  GdkCicpColorState *cs1 = (GdkCicpColorState *) self;
  GdkCicpColorState *cs2 = (GdkCicpColorState *) other;

  return gdk_cicp_equal (&cs1->cicp, &cs2->cicp);
}

static const char *
gdk_cicp_color_state_get_name (GdkColorState *self)
{
  GdkCicpColorState *cs = (GdkCicpColorState *) self;

  return cs->name;
}

static GdkColorState *
gdk_cicp_color_state_get_no_srgb_tf (GdkColorState *self)
{
  GdkCicpColorState *cs = (GdkCicpColorState *) self;

  return cs->no_srgb;
}

static GdkFloatColorConvert
gdk_cicp_color_state_get_convert_to (GdkColorState *self,
                                     GdkColorState *target)
{
  if (!GDK_IS_DEFAULT_COLOR_STATE (target))
    return NULL;

  switch (GDK_DEFAULT_COLOR_STATE_ID (target))
    {
    case GDK_COLOR_STATE_ID_SRGB:
      return gdk_convert_cicp_to_srgb;
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      return gdk_convert_cicp_to_srgb_linear;
    case GDK_COLOR_STATE_ID_REC2100_PQ:
      return gdk_convert_cicp_to_rec2100_pq;
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return gdk_convert_cicp_to_rec2100_linear;

    case GDK_COLOR_STATE_N_IDS:
    default:
      g_assert_not_reached ();
    }

  return NULL;
}

static GdkFloatColorConvert
gdk_cicp_color_state_get_convert_from (GdkColorState *self,
                                       GdkColorState *source)
{
  if (!GDK_IS_DEFAULT_COLOR_STATE (source))
    return NULL;

  switch (GDK_DEFAULT_COLOR_STATE_ID (source))
    {
    case GDK_COLOR_STATE_ID_SRGB:
      return gdk_convert_cicp_from_srgb;
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      return gdk_convert_cicp_from_srgb_linear;
    case GDK_COLOR_STATE_ID_REC2100_PQ:
      return gdk_convert_cicp_from_rec2100_pq;
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return gdk_convert_cicp_from_rec2100_linear;

    case GDK_COLOR_STATE_N_IDS:
    default:
      g_assert_not_reached ();
    }

  return NULL;
}

static const GdkCicp *
gdk_cicp_color_state_get_cicp (GdkColorState  *color_state)
{
  GdkCicpColorState *self = (GdkCicpColorState *) color_state;

  return &self->cicp;
}

/* }}} */

static const
GdkColorStateClass GDK_CICP_COLOR_STATE_CLASS = {
  .free = gdk_cicp_color_state_free,
  .equal = gdk_cicp_color_state_equal,
  .get_name = gdk_cicp_color_state_get_name,
  .get_no_srgb_tf = gdk_cicp_color_state_get_no_srgb_tf,
  .get_convert_to = gdk_cicp_color_state_get_convert_to,
  .get_convert_from = gdk_cicp_color_state_get_convert_from,
  .get_cicp = gdk_cicp_color_state_get_cicp,
  .clamp = gdk_color_state_clamp_0_1,
};

static inline float *
multiply (float       res[9],
          const float m1[9],
          const float m2[9])
{
#define IDX(i,j) 3*i+j
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      res[IDX(i,j)] =   m1[IDX(i,0)] * m2[IDX(0,j)]
                      + m1[IDX(i,1)] * m2[IDX(1,j)]
                      + m1[IDX(i,2)] * m2[IDX(2,j)];

  return res;
}

GdkColorState *
gdk_color_state_new_for_cicp (const GdkCicp  *cicp,
                              GError        **error)
{
  GdkCicpColorState *self;
  GdkTransferFunc eotf;
  GdkTransferFunc oetf;
  gconstpointer to_xyz;
  gconstpointer from_xyz;
  gconstpointer to_yuv = NULL;
  gconstpointer from_yuv = NULL;

  if (cicp->color_primaries == 2 ||
      cicp->transfer_function == 2 ||
      cicp->matrix_coefficients == 2)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("cicp: Unspecified parameters not supported"));
      return NULL;
    }

  for (guint i = 0; i < GDK_COLOR_STATE_N_IDS; i++)
    {
      if (gdk_cicp_equal (cicp, &gdk_default_color_states[i].cicp))
        return (GdkColorState *) &gdk_default_color_states[i];
    }

  switch (cicp->transfer_function)
    {
    case 1:
    case 6:
    case 14:
    case 15:
      eotf = bt709_eotf;
      oetf = bt709_oetf;
      break;
    case 4:
      eotf = gamma22_eotf;
      oetf = gamma22_oetf;
      break;
    case 5:
      eotf = gamma28_eotf;
      oetf = gamma28_oetf;
      break;
    case 8:
      eotf = NONE;
      oetf = NONE;
      break;
    case 13:
      eotf = srgb_eotf;
      oetf = srgb_oetf;
      break;
    case 16:
      eotf = pq_eotf;
      oetf = pq_oetf;
      break;
    case 18:
      eotf = hlg_eotf;
      oetf = hlg_oetf;
      break;
    default:
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("cicp: Transfer function %u not supported"),
                   cicp->transfer_function);
      return NULL;
    }

  switch (cicp->color_primaries)
    {
    case 1:
      to_xyz = srgb_to_xyz;
      from_xyz = xyz_to_srgb;
      break;
    case 5:
      to_xyz = pal_to_xyz;
      from_xyz = xyz_to_pal;
      break;
    case 6:
    case 7:
      to_xyz = ntsc_to_xyz;
      from_xyz = xyz_to_ntsc;
      break;
    case 9:
      to_xyz = rec2020_to_xyz;
      from_xyz = xyz_to_rec2020;
      break;
    case 12:
      to_xyz = p3_to_xyz;
      from_xyz = xyz_to_p3;
      break;
    default:
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("cicp: Color primaries %u not supported"),
                   cicp->color_primaries);
      return NULL;
    }

  switch (cicp->matrix_coefficients)
    {
    case 0:
      to_yuv = IDENTITY;
      from_yuv = IDENTITY;
      break;
    case 1:
      to_yuv = rgb_to_bt709;
      from_yuv = bt709_to_rgb;
      break;
    case 5:
    case 6:
      to_yuv = rgb_to_bt601;
      from_yuv = bt601_to_rgb;
      break;
    case 9:
      to_yuv = rgb_to_bt2020;
      from_yuv = bt2020_to_rgb;
      break;
    default:
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("cicp: Matrix coefficients %u, %s not supported"),
                   cicp->matrix_coefficients,
                   cicp->range == GDK_CICP_RANGE_NARROW ? "narrow" : "full");
      return NULL;
    }

  self = g_new0 (GdkCicpColorState, 1);

  self->parent.klass = &GDK_CICP_COLOR_STATE_CLASS;
  self->parent.ref_count = 1;

  /* sRGB is special-cased by being a default colorstate */
  self->parent.rendering_color_state = GDK_COLOR_STATE_REC2100_PQ;
  self->parent.rendering_color_state_linear = GDK_COLOR_STATE_REC2100_LINEAR;

  self->parent.depth = GDK_MEMORY_FLOAT16;

  memcpy (&self->cicp, cicp, sizeof (GdkCicp));

  self->to_yuv = to_yuv;
  self->from_yuv = from_yuv;

  self->eotf = eotf;
  self->oetf = oetf;

  multiply (self->to_srgb, xyz_to_srgb, to_xyz);
  multiply (self->to_rec2020, xyz_to_rec2020, to_xyz);
  multiply (self->from_srgb, from_xyz, srgb_to_xyz);
  multiply (self->from_rec2020, from_xyz, rec2020_to_xyz);

  self->name = g_strdup_printf ("cicp-%u/%u/%u/%u",
                                cicp->color_primaries,
                                cicp->transfer_function,
                                cicp->matrix_coefficients,
                                cicp->range);

  if (cicp->transfer_function == 13)
    {
      self->no_srgb = gdk_color_state_new_for_cicp (&(GdkCicp) {
                                                      cicp->color_primaries,
                                                      8,
                                                      cicp->matrix_coefficients,
                                                      cicp->range },
                                                    NULL);
    }

  return (GdkColorState *) self;
}

/* }}} */
/* {{{ Private API */

/*<private>
 * gdk_color_state_get_name:
 * @self: a colorstate
 *
 * Returns the name of @self.
 *
 * This is *not* a translated, user-visible string.
 *
 * Returns: (transfer none): a name for representing the color state
 *     in diagnostic output
 */
const char *
gdk_color_state_get_name (GdkColorState *self)
{
  return self->klass->get_name (self);
}

/*<private>
 * gdk_color_state_get_no_srgb_tf:
 * @self: a colorstate
 *
 * This function checks if the colorstate uses an sRGB transfer function
 * as final operation. In that case, it is suitable for use with GL_SRGB
 * (and the Vulkan equivalents).
 *
 * If it is suitable, the colorstate without the transfer function is
 * returned. Otherwise, this function returns NULL.
 *
 * Returns: (transfer none): the colorstate without sRGB transfer function.
 **/
GdkColorState *
gdk_color_state_get_no_srgb_tf (GdkColorState *self)
{
  if (!GDK_DEBUG_CHECK (LINEAR))
    return FALSE;

  return self->klass->get_no_srgb_tf (self);
}

/*< private >
 * gdk_color_state_clamp:
 * @self: a `GdkColorState`
 * @src: the values to clamp
 * @dest: (out): location to store the result, may be identical to
 *   the src argument
 *
 * Clamps the values to be within the allowed ranges for the given
 * color state.
 */
void
gdk_color_state_clamp (GdkColorState *self,
                       const float    src[4],
                       float          dest[4])
{
  self->klass->clamp (self, src, dest);
}

GdkColorState *
gdk_color_state_yuv (void)
{
  static GdkColorState *yuv_cs;

  if (g_once_init_enter_pointer (&yuv_cs))
    {
      GdkColorState *cs;

      cs = gdk_color_state_new_for_cicp (&(GdkCicp) { 1, 13, 5, 0 }, NULL);
      g_assert (cs);

      g_once_init_leave_pointer (&yuv_cs, cs);
    }

  return yuv_cs;
}

/* }}} */

/* vim:set foldmethod=marker: */
