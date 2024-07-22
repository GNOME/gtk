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

#include <math.h>

/**
 * GdkColorState:
 *
 * A `GdkColorState` object provides the information to interpret
 * colors and pixels in a variety of ways.
 *
 * They are also known as
 * [*color spaces*](https://en.wikipedia.org/wiki/Color_space).
 *
 * Crucially, GTK knows how to convert colors from one color
 * state to another.
 *
 * `GdkColorState objects are immutable and therefore threadsafe.
 *
 * Since 4.16
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
 * It is equivalent to H.273 ColourPrimaries 1 with TransferCharacteristics 13 and MatrixCoefficients 0.
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
 * It is equivalent to H.273 ColourPrimaries 1 with TransferCharacteristics 8 and MatrixCoefficients 0.
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
 * It is equivalent to H.273 ColourPrimaries code point 9 with TransferCharacteristics 16.
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
 * It is equivalent to H.273 ColourPrimaries code point 9 with TransferCharacteristics 8.
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

/* }}} */
/* {{{ Conversion functions */

#define TRANSFORM(name, eotf, matrix, oetf) \
static void \
name (GdkColorState  *self, \
      float         (*values)[4], \
      gsize           n_values) \
{ \
  for (gsize i = 0; i < n_values; i++) \
    { \
      values[i][0] = eotf (values[i][0]); \
      values[i][1] = eotf (values[i][1]); \
      values[i][2] = eotf (values[i][2]); \
      if ((float **)matrix != IDENTITY) \
        { \
          float res[3]; \
          res[0] = matrix[0][0] * values[i][0] + matrix[0][1] * values[i][1] + matrix[0][2] * values[i][2]; \
          res[1] = matrix[1][0] * values[i][0] + matrix[1][1] * values[i][1] + matrix[1][2] * values[i][2]; \
          res[2] = matrix[2][0] * values[i][0] + matrix[2][1] * values[i][1] + matrix[2][2] * values[i][2]; \
          values[i][0] = res[0]; \
          values[i][1] = res[1]; \
          values[i][2] = res[2]; \
        } \
      values[i][0] = oetf (values[i][0]); \
      values[i][1] = oetf (values[i][1]); \
      values[i][2] = oetf (values[i][2]); \
    } \
}

static inline float
srgb_oetf (float v)
{
  if (v > 0.0031308f)
    return 1.055f * powf (v, 1.f / 2.4f) - 0.055f;
  else
    return 12.92f * v;
}

static inline float
srgb_eotf (float v)
{
  if (v >= 0.04045f)
    return powf (((v + 0.055f) / (1.f + 0.055f)), 2.4f);
  else
    return v / 12.92f;
}

static inline float
pq_eotf (float v)
{
  float ninv = (1 << 14) / 2610.0;
  float minv = (1 << 5) / 2523.0;
  float c1 = 3424.0 / (1 << 12);
  float c2 = 2413.0 / (1 << 7);
  float c3 = 2392.0 / (1 << 7);

  float x = powf (MAX ((powf (v, minv) - c1), 0) / (c2 - (c3 * (powf (v, minv)))), ninv);

  return x * 10000 / 203.0;
}

static inline float
pq_oetf (float v)
{
  float x = v * 203.0 / 10000.0;
  float n = 2610.0 / (1 << 14);
  float m = 2523.0 / (1 << 5);
  float c1 = 3424.0 / (1 << 12);
  float c2 = 2413.0 / (1 << 7);
  float c3 = 2392.0 / (1 << 7);

  return powf (((c1 + (c2 * powf (x, n))) / (1 + (c3 * powf (x, n)))), m);
}

/* These matrices are derived by combining the standard abc_to_xyz onces:
 *
 * rec2020_to_srgb = srgb_to_xyz⁻¹ * rec2020_to_xyz
 * srgb_to_rec2020 = rec2020_to_xyz⁻¹ * srgb_to_xyz
 *
 * These values were used here:
 *
 * static const float srgb_to_xyz[3][3] = {
 *   { 0.4125288,  0.3581642,  0.1774037 },
 *   { 0.2127102,  0.7163284,  0.0709615 },
 *   { 0.0193373,  0.1193881,  0.9343260 },
 * }
 *
 * static const float rec2020_to_xyz[3][3] = {
 *   { 0.6369615,  0.1448079,  0.1663273 },
 *   { 0.2627016,  0.6788934,  0.0584050 },
 *   { 0.0000000,  0.0281098,  1.0449416 },
 * };
 *
 * See http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
 * for how to derive the abc_to_xyz matrices from chromaticity coordinates.
 */

static const float rec2020_to_srgb[3][3] = {
  { 1.659944, -0.588220, -0.071724 },
  { -0.124350, 1.132559, -0.008210 },
  { -0.018466, -0.102459, 1.120924 },
};

static const float srgb_to_rec2020[3][3] = {
  { 0.627610, 0.329815, 0.042574 },
  { 0.069029, 0.919817, 0.011154 },
  { 0.016649, 0.089510, 0.893842 },
};

#define IDENTITY ((float**)0)
#define NONE(x) x

TRANSFORM(gdk_default_srgb_to_srgb_linear,           srgb_eotf, IDENTITY,        NONE);
TRANSFORM(gdk_default_srgb_linear_to_srgb,           NONE,      IDENTITY,        srgb_oetf)
TRANSFORM(gdk_default_rec2100_pq_to_rec2100_linear,  pq_eotf,   IDENTITY,        NONE)
TRANSFORM(gdk_default_rec2100_linear_to_rec2100_pq,  NONE,      IDENTITY,        pq_oetf)
TRANSFORM(gdk_default_srgb_linear_to_rec2100_linear, NONE,      srgb_to_rec2020, NONE)
TRANSFORM(gdk_default_rec2100_linear_to_srgb_linear, NONE,      rec2020_to_srgb, NONE)
TRANSFORM(gdk_default_srgb_to_rec2100_linear,        srgb_eotf, srgb_to_rec2020, NONE)
TRANSFORM(gdk_default_rec2100_pq_to_srgb_linear,     pq_eotf,   rec2020_to_srgb, NONE)
TRANSFORM(gdk_default_srgb_linear_to_rec2100_pq,     NONE,      srgb_to_rec2020, pq_oetf)
TRANSFORM(gdk_default_rec2100_linear_to_srgb,        NONE,      rec2020_to_srgb, srgb_oetf)
TRANSFORM(gdk_default_srgb_to_rec2100_pq,            srgb_eotf, srgb_to_rec2020, pq_oetf)
TRANSFORM(gdk_default_rec2100_pq_to_srgb,            pq_eotf,   rec2020_to_srgb, srgb_oetf)

#undef IDENTITY
#undef NONE

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

  if (!GDK_IS_DEFAULT_COLOR_STATE (target))
    return NULL;

  return self->convert_to[GDK_DEFAULT_COLOR_STATE_ID (target)];
}

static GdkFloatColorConvert
gdk_default_color_state_get_convert_from (GdkColorState  *color_state,
                                          GdkColorState  *source)
{
  GdkDefaultColorState *self = (GdkDefaultColorState *) source;

  if (!GDK_IS_DEFAULT_COLOR_STATE (color_state))
    return NULL;

  return self->convert_to[GDK_DEFAULT_COLOR_STATE_ID (color_state)];
}

/* }}} */

static const
GdkColorStateClass GDK_DEFAULT_COLOR_STATE_CLASS = {
  .free = NULL, /* crash here if this ever happens */
  .equal = gdk_default_color_state_equal,
  .get_name = gdk_default_color_state_get_name,
  .get_no_srgb_tf = gdk_default_color_state_get_no_srgb_tf,
  .get_convert_to = gdk_default_color_state_get_convert_to,
};

GdkDefaultColorState gdk_default_color_states[] = {
  [GDK_COLOR_STATE_ID_SRGB] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_U8_SRGB,
      .rendering_color_state = GDK_COLOR_STATE_SRGB_LINEAR,
    },
    .name = "srgb",
    .no_srgb = GDK_COLOR_STATE_SRGB_LINEAR,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_default_srgb_to_srgb_linear,
      [GDK_COLOR_STATE_ID_REC2100_PQ] = gdk_default_srgb_to_rec2100_pq,
      [GDK_COLOR_STATE_ID_REC2100_LINEAR] = gdk_default_srgb_to_rec2100_linear,
    },
  },
  [GDK_COLOR_STATE_ID_SRGB_LINEAR] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_U8,
      .rendering_color_state = GDK_COLOR_STATE_SRGB_LINEAR,
    },
    .name = "srgb-linear",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_default_srgb_linear_to_srgb,
      [GDK_COLOR_STATE_ID_REC2100_PQ] = gdk_default_srgb_linear_to_rec2100_pq,
      [GDK_COLOR_STATE_ID_REC2100_LINEAR] = gdk_default_srgb_linear_to_rec2100_linear,
    },
  },
  [GDK_COLOR_STATE_ID_REC2100_PQ] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_REC2100_LINEAR,
    },
    .name = "rec2100-pq",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_default_rec2100_pq_to_srgb,
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_default_rec2100_pq_to_srgb_linear,
      [GDK_COLOR_STATE_ID_REC2100_LINEAR] = gdk_default_rec2100_pq_to_rec2100_linear,
    },
  },
  [GDK_COLOR_STATE_ID_REC2100_LINEAR] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_REC2100_LINEAR,
    },
    .name = "rec2100-linear",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_default_rec2100_linear_to_srgb,
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_default_rec2100_linear_to_srgb_linear,
      [GDK_COLOR_STATE_ID_REC2100_PQ] = gdk_default_rec2100_linear_to_rec2100_pq,
    },
  },
};

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

/* }}} */

/* vim:set foldmethod=marker expandtab: */
