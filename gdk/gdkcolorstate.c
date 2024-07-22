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

#include <glib/gi18n-lib.h>

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
typedef const float GdkColorMatrix[3][3];

#define IDENTITY ((float**)0)
#define NONE ((GdkTransferFunc)0)

#define TRANSFORM(name, eotf, matrix, oetf) \
static void \
name (GdkColorState  *self, \
      float         (*values)[4], \
      gsize           n_values) \
{ \
  for (gsize i = 0; i < n_values; i++) \
    { \
      if (eotf != NONE) \
        { \
          values[i][0] = eotf (values[i][0]); \
          values[i][1] = eotf (values[i][1]); \
          values[i][2] = eotf (values[i][2]); \
        } \
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
      if (oetf != NONE) \
        { \
          values[i][0] = oetf (values[i][0]); \
          values[i][1] = oetf (values[i][1]); \
          values[i][2] = oetf (values[i][2]); \
        } \
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

static inline float
bt709_eotf (float v)
{
  if (v < 0.081)
    return v / 4.5;
  else
    return powf ((v + 0.099) / 1.099, 1/0.45);
}

static inline float
bt709_oetf (float v)
{
  if (v < 0.081)
    return v * 4.5;
  else
    return 1.099  * powf (v, 0.45) - 0.099;
}

static inline float
hlg_eotf (float v)
{
  const float a = 0.17883277;
  const float b = 0.28466892;
  const float c = 0.55991073;

  if (v <= 0.5)
    return (v * v) / 3;
  else
    return expf (((v - c) / a) + b) / 12.0;
}

static inline float
hlg_oetf (float v)
{
  const float a = 0.17883277;
  const float b = 0.28466892;
  const float c = 0.55991073;

  if (v <= 1/12.0)
    return sqrtf (3 * v);
  else
    return a * logf (12 * v - b) + c;
}

/* See http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html
 * for how to derive the abc_to_xyz matrices from chromaticity coordinates.
 */

static const float srgb_to_xyz[3][3] = {
  { 0.4125288,  0.3581642,  0.1774037 },
  { 0.2127102,  0.7163284,  0.0709615 },
  { 0.0193373,  0.1193881,  0.9343260 },
};

static const float xyz_to_srgb[3][3] = {
  {  3.2409699, -1.5373832, -0.4986108 },
  { -0.9692436,  1.8759675,  0.0415551 },
  {  0.0556301, -0.2039770,  1.0569715 },
};

static const float rec2020_to_xyz[3][3] = {
  { 0.6369615,  0.1448079,  0.1663273 },
  { 0.2627016,  0.6788934,  0.0584050 },
  { 0.0000000,  0.0281098,  1.0449416 },
};

static const float xyz_to_rec2020[3][3] = {
  {  1.7166512, -0.3556708, -0.2533663 },
  { -0.6666844,  1.6164812,  0.0157685 },
  {  0.0176399, -0.0427706,  0.9421031 },
};

static const float pal_to_xyz[3][3] = {
 { 0.4305538,  0.3415498,  0.1783523 },
 { 0.2220043,  0.7066548,  0.0713409 },
 { 0.0201822,  0.1295534,  0.9393222 },
};

static const float xyz_to_pal[3][3] = {
 {  3.0633611, -1.3933902, -0.4758237 },
 { -0.9692436,  1.8759675,  0.0415551 },
 {  0.0678610, -0.2287993,  1.0690896 },
};

static const float ntsc_to_xyz[3][3] = {
 { 0.3935209,  0.3652581,  0.1916769 },
 { 0.2123764,  0.7010599,  0.0865638 },
 { 0.0187391,  0.1119339,  0.9583847 },
};

static const float xyz_to_ntsc[3][3] = {
 {  3.5060033, -1.7397907, -0.5440583 },
 { -1.0690476,  1.9777789,  0.0351714 },
 {  0.0563066, -0.1969757,  1.0499523 },
};

static const float p3_to_xyz[3][3] = {
  { 0.4865709,  0.2656677,  0.1982173 },
  { 0.2289746,  0.6917385,  0.0792869 },
  { 0.0000000,  0.0451134,  1.0439444 },
};

static const float xyz_to_p3[3][3] = {
  {  2.4934969, -0.9313836, -0.4027108 },
  { -0.8294890,  1.7626641,  0.0236247 },
  {  0.0358458, -0.0761724,  0.9568845 },
};

/* premultiplied matrices for default conversions */

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

  return &self->cicp;
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
    .cicp = { 1, 13, 0, 1 },
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
    .cicp = { 1, 8, 0, 1 },
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
    .cicp = { 9, 16, 0, 1 },
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
    .cicp = { 9, 8, 0, 1 },
  },
};

 /* }}} */
/* {{{ Cicp implementation */

typedef struct _GdkCicpColorState GdkCicpColorState;
struct _GdkCicpColorState
{
  GdkColorState parent;

  GdkColorState *no_srgb;

  const char *name;

  GdkTransferFunc eotf;
  GdkTransferFunc oetf;

  float to_srgb[3][3];
  float to_rec2020[3][3];
  float from_srgb[3][3];
  float from_rec2020[3][3];

  GdkCicp cicp;
};

/* {{{ Conversion functions */

#define cicp ((GdkCicpColorState *)self)

TRANSFORM(gdk_cicp_to_srgb,             cicp->eotf,  cicp->to_srgb,      srgb_oetf)
TRANSFORM(gdk_cicp_to_srgb_linear,      cicp->eotf,  cicp->to_srgb,      NONE)
TRANSFORM(gdk_cicp_to_rec2100_pq,       cicp->eotf,  cicp->to_rec2020,   pq_oetf)
TRANSFORM(gdk_cicp_to_rec2100_linear,   cicp->eotf,  cicp->to_rec2020,   NONE)
TRANSFORM(gdk_cicp_from_srgb,           srgb_eotf,   cicp->from_srgb,    cicp->oetf)
TRANSFORM(gdk_cicp_from_srgb_linear,    NONE,        cicp->from_srgb,    cicp->oetf)
TRANSFORM(gdk_cicp_from_rec2100_pq,     pq_eotf,     cicp->from_rec2020, cicp->oetf)
TRANSFORM(gdk_cicp_from_rec2100_linear, NONE,        cicp->from_rec2020, cicp->oetf)

#undef cicp

 /* }}} */
/* {{{ Vfuncs */

static void
gdk_cicp_color_state_free (GdkColorState *cs)
{
  GdkCicpColorState *self = (GdkCicpColorState *) cs;

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
      return gdk_cicp_to_srgb;
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      return gdk_cicp_to_srgb_linear;
    case GDK_COLOR_STATE_ID_REC2100_PQ:
      return gdk_cicp_to_rec2100_pq;
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return gdk_cicp_to_rec2100_linear;

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
      return gdk_cicp_from_srgb;
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      return gdk_cicp_from_srgb_linear;
    case GDK_COLOR_STATE_ID_REC2100_PQ:
      return gdk_cicp_from_rec2100_pq;
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return gdk_cicp_from_rec2100_linear;

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
};

static inline void
multiply (float       res[3][3],
          const float m1[3][3],
          const float m2[3][3])
{
  if ((float **) m1 == IDENTITY)
    memcpy (res, m2, sizeof (float) * 3 * 3);
  else if ((float **) m2 == IDENTITY)
    memcpy (res, m1, sizeof (float) * 3 * 3);
  else
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++)
        res[i][j] = m1[i][0] * m2[0][j] + m1[i][1] * m2[1][j] + m1[i][2] * m2[2][j];
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

  if (cicp->range == GDK_CICP_RANGE_NARROW || cicp->matrix_coefficients != 0)
    {
      g_set_error (error,
                   G_IO_ERROR, G_IO_ERROR_FAILED,
                   _("cicp: Narrow range or YUV not supported"));
      return NULL;
    }

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
      if (gdk_cicp_equivalent (cicp, &gdk_default_color_states[i].cicp))
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
      to_xyz = ntsc_to_xyz;
      from_xyz = xyz_to_ntsc;
      break;
    case 9:
      to_xyz = rec2020_to_xyz;
      from_xyz = xyz_to_rec2020;
      break;
    case 10:
      to_xyz = IDENTITY;
      from_xyz = IDENTITY;
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

  self = g_new0 (GdkCicpColorState, 1);

  self->parent.klass = &GDK_CICP_COLOR_STATE_CLASS;
  self->parent.ref_count = 1;

  /* sRGB is special-cased by being a default colorstate */
  self->parent.rendering_color_state = GDK_COLOR_STATE_REC2100_LINEAR;

  self->parent.depth = GDK_MEMORY_FLOAT16;

  memcpy (&self->cicp, cicp, sizeof (GdkCicp));

  self->eotf = eotf;
  self->oetf = oetf;

  multiply (self->to_srgb,      xyz_to_srgb,    to_xyz);
  multiply (self->to_rec2020,   xyz_to_rec2020, to_xyz);
  multiply (self->from_srgb,    from_xyz,       srgb_to_xyz);
  multiply (self->from_rec2020, from_xyz,       rec2020_to_xyz);

  self->name = g_strdup_printf ("cicp-%u/%u/%u/%u",
                                cicp->color_primaries,
                                cicp->transfer_function,
                                cicp->matrix_coefficients,
                                cicp->range);

  if (cicp->transfer_function == 13)
    {
      GdkCicp no_srgb;

      memcpy (&no_srgb, cicp, sizeof (GdkCicp));
      no_srgb.transfer_function = 8;

      self->no_srgb = gdk_color_state_new_for_cicp (&no_srgb, NULL);
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

/* }}} */

/* vim:set foldmethod=marker expandtab: */
