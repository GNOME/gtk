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

#include "gdkcolordefs.h"

#include <graphene.h>

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
typedef const float GdkColorMatrix[9];

#define IDENTITY ((float*)0)
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
      if (matrix != IDENTITY) \
        { \
          float res[3]; \
          res[0] = matrix[0] * values[i][0] + matrix[1] * values[i][1] + matrix[2] * values[i][2]; \
          res[1] = matrix[3] * values[i][0] + matrix[4] * values[i][1] + matrix[5] * values[i][2]; \
          res[2] = matrix[6] * values[i][0] + matrix[7] * values[i][1] + matrix[8] * values[i][2]; \
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

static const float *
gdk_default_color_state_get_primaries (GdkColorState *color_state)
{
  GdkDefaultColorState *self = (GdkDefaultColorState *) color_state;

  return self->primaries;
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
  .get_primaries = gdk_default_color_state_get_primaries,
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
    .primaries = srgb_primaries,
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
    .primaries = srgb_primaries,
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
    .primaries = rec2020_primaries,
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
    .primaries = rec2020_primaries,
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

  float *to_srgb;
  float *to_rec2020;
  float *from_srgb;
  float *from_rec2020;

  GdkCicp cicp;

  const float *primaries;
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

  g_free (self->to_srgb);
  g_free (self->to_rec2020);
  g_free (self->from_srgb);
  g_free (self->from_rec2020);

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

static const float *
gdk_cicp_color_state_get_primaries (GdkColorState  *color_state)
{
  GdkCicpColorState *self = (GdkCicpColorState *) color_state;

  return self->primaries;
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
  .get_primaries = gdk_cicp_color_state_get_primaries,
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
  const float *primaries;

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
      primaries = srgb_primaries;
      to_xyz = srgb_to_xyz;
      from_xyz = xyz_to_srgb;
      break;
    case 5:
      primaries = pal_primaries;
      to_xyz = pal_to_xyz;
      from_xyz = xyz_to_pal;
      break;
    case 6:
      primaries = ntsc_primaries;
      to_xyz = ntsc_to_xyz;
      from_xyz = xyz_to_ntsc;
      break;
    case 9:
      primaries = rec2020_primaries;
      to_xyz = rec2020_to_xyz;
      from_xyz = xyz_to_rec2020;
      break;
    case 12:
      primaries = p3_primaries;
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

  self->primaries = primaries;

  self->to_srgb = multiply (g_new (float, 9), xyz_to_srgb, to_xyz);
  self->to_rec2020 = multiply (g_new (float, 9), xyz_to_rec2020, to_xyz);
  self->from_srgb = multiply (g_new (float, 9), from_xyz, srgb_to_xyz);
  self->from_rec2020 = multiply (g_new (float, 9), from_xyz, rec2020_to_xyz);

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
/* {{{ Primaries implementation */

typedef struct _GdkPrimariesColorState GdkPrimariesColorState;
struct _GdkPrimariesColorState
{
  GdkColorState parent;

  char *name;

  GdkTransferFunc eotf;
  GdkTransferFunc oetf;

  float primaries[8];

  float to_srgb[9];
  float to_rec2020[9];
  float from_srgb[9];
  float from_rec2020[9];

  GdkCicp cicp;
};

/* {{{ Conversion functions */

#define prim ((GdkPrimariesColorState *)self)

TRANSFORM(gdk_primaries_to_srgb,             prim->eotf,  prim->to_srgb,      srgb_oetf)
TRANSFORM(gdk_primaries_to_srgb_linear,      prim->eotf,  prim->to_srgb,      NONE)
TRANSFORM(gdk_primaries_to_rec2100_pq,       prim->eotf,  prim->to_rec2020,   pq_oetf)
TRANSFORM(gdk_primaries_to_rec2100_linear,   prim->eotf,  prim->to_rec2020,   NONE)
TRANSFORM(gdk_primaries_from_srgb,           srgb_eotf,   prim->from_srgb,    prim->oetf)
TRANSFORM(gdk_primaries_from_srgb_linear,    NONE,        prim->from_srgb,    prim->oetf)
TRANSFORM(gdk_primaries_from_rec2100_pq,     pq_eotf,     prim->from_rec2020, prim->oetf)
TRANSFORM(gdk_primaries_from_rec2100_linear, NONE,        prim->from_rec2020, prim->oetf)

#undef prim

/* }}} */
/* {{{ Vfuncs */

static void
gdk_primaries_color_state_free (GdkColorState *cs)
{
  GdkPrimariesColorState *self = (GdkPrimariesColorState *) cs;

  g_free (self->name);

  g_free (self);
}

static gboolean
gdk_primaries_color_state_equal (GdkColorState *self,
                                 GdkColorState *other)
{
  GdkPrimariesColorState *cs1 = (GdkPrimariesColorState *) self;
  GdkPrimariesColorState *cs2 = (GdkPrimariesColorState *) other;

  return memcmp (cs1->primaries, cs2->primaries, sizeof (float) * 8) == 0 &&
         cs1->eotf == cs2->eotf;
}

static const char *
gdk_primaries_color_state_get_name (GdkColorState *self)
{
  GdkPrimariesColorState *cs = (GdkPrimariesColorState *) self;

  return cs->name;
}

static GdkColorState *
gdk_primaries_color_state_get_no_srgb_tf (GdkColorState *self)
{
  return NULL;
}

static GdkFloatColorConvert
gdk_primaries_color_state_get_convert_to (GdkColorState *self,
                                          GdkColorState *target)
{
  if (!GDK_IS_DEFAULT_COLOR_STATE (target))
    return NULL;

  switch (GDK_DEFAULT_COLOR_STATE_ID (target))
    {
    case GDK_COLOR_STATE_ID_SRGB:
      return gdk_primaries_to_srgb;
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      return gdk_primaries_to_srgb_linear;
    case GDK_COLOR_STATE_ID_REC2100_PQ:
      return gdk_primaries_to_rec2100_pq;
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return gdk_primaries_to_rec2100_linear;

    case GDK_COLOR_STATE_N_IDS:
    default:
      g_assert_not_reached ();
    }

  return NULL;
}

static GdkFloatColorConvert
gdk_primaries_color_state_get_convert_from (GdkColorState *self,
                                            GdkColorState *source)
{
  if (!GDK_IS_DEFAULT_COLOR_STATE (source))
    return NULL;

  switch (GDK_DEFAULT_COLOR_STATE_ID (source))
    {
    case GDK_COLOR_STATE_ID_SRGB:
      return gdk_primaries_from_srgb;
    case GDK_COLOR_STATE_ID_SRGB_LINEAR:
      return gdk_primaries_from_srgb_linear;
    case GDK_COLOR_STATE_ID_REC2100_PQ:
      return gdk_primaries_from_rec2100_pq;
    case GDK_COLOR_STATE_ID_REC2100_LINEAR:
      return gdk_primaries_from_rec2100_linear;

    case GDK_COLOR_STATE_N_IDS:
    default:
      g_assert_not_reached ();
    }

  return NULL;
}

static const GdkCicp *
gdk_primaries_color_state_get_cicp (GdkColorState *color_state)
{
  GdkPrimariesColorState *self = (GdkPrimariesColorState *) color_state;

  return &self->cicp;
}

static const float *
gdk_primaries_color_state_get_primaries (GdkColorState  *color_state)
{
  GdkPrimariesColorState *self = (GdkPrimariesColorState *) color_state;

  return self->primaries;
}

/* }}} */

static const
GdkColorStateClass GDK_PRIMARIES_COLOR_STATE_CLASS = {
  .free = gdk_primaries_color_state_free,
  .equal = gdk_primaries_color_state_equal,
  .get_name = gdk_primaries_color_state_get_name,
  .get_no_srgb_tf = gdk_primaries_color_state_get_no_srgb_tf,
  .get_convert_to = gdk_primaries_color_state_get_convert_to,
  .get_convert_from = gdk_primaries_color_state_get_convert_from,
  .get_cicp = gdk_primaries_color_state_get_cicp,
  .get_primaries = gdk_primaries_color_state_get_primaries,
};

static void
compute_to_xyz_from_primaries (float       to_xyz[9],
                               const float primaries[8])
{
  float rx, ry, gx, gy, bx, by, wx, wy;
  float rY, bY, gY;

  rx = primaries[0]; ry = primaries[1];
  gx = primaries[2]; gy = primaries[3];
  bx = primaries[4]; by = primaries[5];
  wx = primaries[6]; wy = primaries[7];

  bY = (((1 - wx)/wy - (1 - rx)/ry)*(gx/gy - rx/ry) - (wx/wy - rx/ry)*((1 - gx)/gy - (1 - rx)/ry)) /
        (((1 - bx)/by - (1 - rx)/ry)*(gx/gy - rx/ry) - (bx/by - rx/ry)*((1 - gx)/gy - (1 - rx)/ry));

  gY = (wx/wy - rx/ry - bY*(bx/by - rx/ry)) / (gx/gy - rx/ry);

  rY = 1 - gY - bY;

  to_xyz[0] = rY/ry * rx;        to_xyz[1] = gY/gy * gx;        to_xyz[2] = bY/by * bx;
  to_xyz[3] = rY;                to_xyz[4] = gY;                to_xyz[5] = bY;
  to_xyz[6] = rY/ry * (1-rx-ry); to_xyz[7] = gY/gy * (1-gx-gy); to_xyz[8] = bY/by * (1-bx-by);
}

static void
invert (float       res[9],
        const float   a[9])
{
  graphene_matrix_t m, m2;
  float v[16];

  graphene_matrix_init_from_float (&m,
                                   (const float *)&(float[]) {
                                    a[0], a[1], a[2], 0,
                                    a[3], a[4], a[5], 0,
                                    a[6], a[7], a[8], 0,
                                       0,    0,    0, 1
                                   });

  graphene_matrix_inverse (&m, &m2);
  graphene_matrix_to_float (&m2, v);

  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      res[3 * i + j] = v[4 * i + j];
}

GdkColorState *
gdk_color_state_new_for_primaries (const float   primaries[8],
                                   guint         transfer_function,
                                   GError      **error)
{
  GdkPrimariesColorState *self;
  float to_xyz[9];
  float from_xyz[9];
  GdkTransferFunc eotf;
  GdkTransferFunc oetf;

  if (memcmp (primaries, srgb_primaries, sizeof (float) * 8) == 0)
    return gdk_color_state_new_for_cicp (&(GdkCicp) { 1, transfer_function, 0, GDK_CICP_RANGE_FULL },
                                         error);
  else if (memcmp (primaries, pal_primaries, sizeof (float) * 8) == 0)
    return gdk_color_state_new_for_cicp (&(GdkCicp) { 5, transfer_function, 0, GDK_CICP_RANGE_FULL },
                                         error);
  else if (memcmp (primaries, ntsc_primaries, sizeof (float) * 8) == 0)
    return gdk_color_state_new_for_cicp (&(GdkCicp) { 6, transfer_function, 0, GDK_CICP_RANGE_FULL },
                                         error);
  else if (memcmp (primaries, rec2020_primaries, sizeof (float) * 8) == 0)
    return gdk_color_state_new_for_cicp (&(GdkCicp) { 9, transfer_function, 0, GDK_CICP_RANGE_FULL },
                                         error);
  else if (memcmp (primaries, p3_primaries, sizeof (float) * 8) == 0)
    return gdk_color_state_new_for_cicp (&(GdkCicp) { 12, transfer_function, 0, GDK_CICP_RANGE_FULL },
                                         error);

  compute_to_xyz_from_primaries (to_xyz, primaries);
  invert (from_xyz, to_xyz);

  switch (transfer_function)
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
                   _("primaries: Transfer function %u not supported"),
                   transfer_function);
      return NULL;
    }

  self = g_new0 (GdkPrimariesColorState, 1);

  self->parent.klass = &GDK_PRIMARIES_COLOR_STATE_CLASS;
  self->parent.ref_count = 1;

  self->parent.rendering_color_state = GDK_COLOR_STATE_REC2100_LINEAR;
  self->parent.depth = GDK_MEMORY_FLOAT16;

  self->eotf = eotf;
  self->oetf = oetf;

  self->cicp.color_primaries = 2;
  self->cicp.transfer_function = transfer_function;
  self->cicp.matrix_coefficients = 0;
  self->cicp.range = GDK_CICP_RANGE_FULL;

  memcpy (self->primaries, primaries, sizeof (float) * 8);

  multiply (self->to_srgb, xyz_to_srgb, to_xyz);
  multiply (self->to_rec2020, xyz_to_rec2020, to_xyz);
  multiply (self->from_srgb, from_xyz, srgb_to_xyz);
  multiply (self->from_rec2020, from_xyz, rec2020_to_xyz);

  self->name = g_strdup_printf ("primaries(%f,%f,%f,%f,%f,%f,%f,%f)",
                                primaries[0], primaries[1], primaries[2],
                                primaries[3], primaries[4], primaries[5],
                                primaries[6], primaries[7]);

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
