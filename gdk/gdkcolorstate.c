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
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_srgb_linear (void)
{
  return GDK_COLOR_STATE_SRGB_LINEAR;
}

/**
 * gdk_color_state_get_xyz:
 *
 * Returns the color state object representing the XYZ color space.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_xyz (void)
{
  return GDK_COLOR_STATE_XYZ;
}

/**
 * gdk_color_state_get_oklab:
 *
 * Returns the color state object representing the OKLAB color space.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_oklab (void)
{
  return GDK_COLOR_STATE_OKLAB;
}

/**
 * gdk_color_state_get_oklch:
 *
 * Returns the color state object representing the OKLCH color space.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_oklch (void)
{
  return GDK_COLOR_STATE_OKLCH;
}

/**
 * gdk_color_state_get_rec2020:
 *
 * Returns the color state object representing the rec2020 color space.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_rec2020 (void)
{
  return GDK_COLOR_STATE_REC2020;
}

/**
 * gdk_color_state_get_rec2020_linear:
 *
 * Returns the color state object representing the linear rec2020 color space.
 *
 * Since: 4.16
 */
GdkColorState *
gdk_color_state_get_rec2020_linear (void)
{
  return GDK_COLOR_STATE_REC2020_LINEAR;
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

/* }}} */
/* {{{ Conversion functions */

#define COORDINATE_TRANSFORM(name, tf) \
static void \
name(GdkColorState  *self, \
     float         (*values)[4], \
     gsize           n_values) \
{ \
  for (gsize i = 0; i < n_values; i++) \
    { \
      values[i][0] = tf (values[i][0]); \
      values[i][1] = tf (values[i][1]); \
      values[i][2] = tf (values[i][2]); \
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

COORDINATE_TRANSFORM(gdk_default_srgb_to_srgb_linear, srgb_eotf)
COORDINATE_TRANSFORM(gdk_default_srgb_linear_to_srgb, srgb_oetf)

static inline void
vec3_multiply (const float matrix[3][3],
               const float vec[3],
               float       res[3])
{
  res[0] = matrix[0][0] * vec[0] + matrix[0][1] * vec[1] + matrix[0][2] * vec[2];
  res[1] = matrix[1][0] * vec[0] + matrix[1][1] * vec[1] + matrix[1][2] * vec[2];
  res[2] = matrix[2][0] * vec[0] + matrix[2][1] * vec[1] + matrix[2][2] * vec[2];
}

#define LINEAR_TRANSFORM(name, matrix) \
static void \
name (GdkColorState  *self, \
      float         (*values)[4], \
      gsize           n_values) \
{ \
  for (gsize i = 0; i < n_values; i++) \
    { \
      float res[3]; \
\
      vec3_multiply (matrix, values[i], res); \
\
      values[i][0] = res[0]; \
      values[i][1] = res[1]; \
      values[i][2] = res[2]; \
    } \
}

static const float srgb_linear_to_xyz[3][3] = {
  { (506752.0 / 1228815.0),  (87881.0 / 245763.0),   (12673.0 /   70218.0) },
  {  (87098.0 /  409605.0), (175762.0 / 245763.0),   (12673.0 /  175545.0) },
  {  ( 7918.0 /  409605.0),  (87881.0 / 737289.0), (1001167.0 / 1053270.0) },
};

static const float xyz_to_srgb_linear[3][3] = {
  {    (12831.0 /   3959.0),     - (329.0 /    214.0),  - (1974.0 /   3959.0) },
  { - (851781.0 / 878810.0),   (1648619.0 / 878810.0),   (36519.0 / 878810.0) },
  {      (705.0 /  12673.0),    - (2585.0 /  12673.0),     (705.0 /    667.0) },
};

LINEAR_TRANSFORM(gdk_default_xyz_to_srgb_linear, xyz_to_srgb_linear)
LINEAR_TRANSFORM(gdk_default_srgb_linear_to_xyz, srgb_linear_to_xyz)

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
gdk_default_oklab_to_oklch (GdkColorState  *self,
                            float         (*values)[4],
                            gsize           n_values)
{
  for (gsize i = 0; i < n_values; i++)
    {
      float a = values[i][1];
      float b = values[i][2];
      float C, H;

      C = hypotf (a, b);
      H = RAD_TO_DEG (atan2 (b, a));
      H = fmod (H, 360);
      if (H < 0)
        H += 360;

      values[i][1] = C;
      values[i][2] = H;
    }
}

static void
gdk_default_oklch_to_oklab (GdkColorState  *self,
                            float         (*values)[4],
                            gsize           n_values)
{
  for (gsize i = 0; i < n_values; i++)
    {
      float C = values[i][1];
      float H = values[i][2];
      float a, b;

      _sincosf (DEG_TO_RAD (H), &b, &a);
      a *= C;
      b *= C;

      values[i][1] = a;
      values[i][2] = b;
    }
}

static const float oklab_to_lms[3][3] = {
  { 1,   0.3963377774,   0.2158037573 },
  { 1, - 0.1055613458, - 0.0638541728 },
  { 1, - 0.0894841775, - 1.2914855480 },
};

static const float lms_to_srgb_linear[3][3] = {
  {   4.0767416621, - 3.3077115913,   0.2309699292 },
  { - 1.2684380046,   2.6097574011, - 0.3413193965 },
  { - 0.0041960863, - 0.7034186147,   1.7076147010 },
};

#define SUM(a, b, i, j) ((a)[i][0] * (b)[0][j] + (a)[i][1] * (b)[1][j] + (a)[i][2] * (b)[2][j])
#define MATMUL(name, a, b) \
static const float name[3][3] = { \
  { SUM((a),(b),0,0), SUM((a),(b),0,1), SUM((a),(b),0,2) }, \
  { SUM((a),(b),1,0), SUM((a),(b),1,1), SUM((a),(b),1,2) }, \
  { SUM((a),(b),2,0), SUM((a),(b),2,1), SUM((a),(b),2,2) }, \
};

MATMUL(lms_to_xyz, lms_to_srgb_linear, srgb_linear_to_xyz)

static void
gdk_default_oklab_to_xyz (GdkColorState  *self,
                          float         (*values)[4],
                          gsize           n_values)
{
  for (gsize i = 0; i < n_values; i++)
    {
      float lms[3];

      vec3_multiply (oklab_to_lms, values[i], lms);

      lms[0] = powf (lms[0], 3);
      lms[1] = powf (lms[1], 3);
      lms[2] = powf (lms[2], 3);

      vec3_multiply (lms_to_xyz, lms, values[i]);
    }
}

static const float srgb_linear_to_lms[3][3] = {
  { 0.4122214708, 0.5363325363, 0.0514459929 },
  { 0.2119034982, 0.6806995451, 0.1073969566 },
  { 0.0883024619, 0.2817188376, 0.6299787005 },
};

static const float lms_to_oklab[3][3] = {
  { 0.2104542553,   0.7936177850, - 0.0040720468 },
  { 1.9779984951, - 2.4285922050,   0.4505937099 },
  { 0.0259040371,   0.7827717662, - 0.8086757660 },
};

MATMUL(xyz_to_lms, xyz_to_srgb_linear, srgb_linear_to_lms)

static void
gdk_default_xyz_to_oklab (GdkColorState  *self,
                          float         (*values)[4],
                          gsize           n_values)
{
  for (gsize i = 0; i < n_values; i++)
    {
      float lms[3];

      vec3_multiply (xyz_to_lms, values[i], lms);

      lms[0] = cbrtf (lms[0]);
      lms[1] = cbrtf (lms[1]);
      lms[2] = cbrtf (lms[2]);

      vec3_multiply (lms_to_oklab, lms, values[i]);
    }
}

static inline float
rec2020_eotf (float v)
{
  float alpha = 1.09929682680944;
  float beta = 0.018053968510807;

  int sign = v < 0 ? -1 : 1;
  float abs = fabsf (v);

  if (abs < beta * 4.5 )
    return v/ 4.5;
  else
    return sign * powf ((abs + alpha - 1) / alpha, 1.0 / 0.45);
}

static inline float
rec2020_oetf (float v)
{
  float alpha = 1.09929682680944;
  float beta = 0.018053968510807;
  int sign = v < 0 ? -1 : 1;
  float abs = fabsf (v);

  if (abs > beta)
    return sign * (alpha * powf (abs, 0.45) - (alpha - 1));
  else
    return 4.5 * v;
}

COORDINATE_TRANSFORM(gdk_default_rec2020_to_rec2020_linear, rec2020_eotf)
COORDINATE_TRANSFORM(gdk_default_rec2020_linear_to_rec2020, rec2020_oetf)

static const float rec2020_linear_to_xyz[3][3] = {
  { (63426534.0 / 99577255.0),  (20160776.0 / 139408157.0),  (47086771.0 / 278816314.0) },
  { (26158966.0 / 99577255.0), (472592308.0 / 697040785.0),   (8267143.0 / 139408157.0) },
  {  (      0   /        1),    (19567812.0 / 697040785.0), (295819943.0 / 278816314.0) },
};

static const float xyz_to_rec2020_linear[3][3] = {
  {   (30757411.0 / 17917100.0), - (6372589.0 / 17917100.0), - (4539589.0 / 17917100.0) },
  { - (19765991.0 / 29648200.0),  (47925759.0 / 29648200.0),    (467509.0 / 29648200.0) },
  {     (792561.0 / 44930125.0), - (1921689.0 / 44930125.0),  (42328811.0 / 44930125.0) },
};

LINEAR_TRANSFORM(gdk_default_rec2020_linear_to_xyz, rec2020_linear_to_xyz)
LINEAR_TRANSFORM(gdk_default_xyz_to_rec2020_linear, xyz_to_rec2020_linear)

#define CONCAT(name, f1, f2) \
static void \
name (GdkColorState  *self, \
      float         (*values)[4], \
      gsize           n_values) \
{ \
  f1 (self, values, n_values); \
  f2 (self, values, n_values); \
}

CONCAT(gdk_default_xyz_to_srgb, gdk_default_xyz_to_srgb_linear, gdk_default_srgb_linear_to_srgb);
CONCAT(gdk_default_srgb_to_xyz, gdk_default_srgb_to_srgb_linear, gdk_default_srgb_linear_to_xyz);
CONCAT(gdk_default_oklch_to_xyz, gdk_default_oklch_to_oklab, gdk_default_oklab_to_xyz);
CONCAT(gdk_default_xyz_to_oklch, gdk_default_xyz_to_oklab, gdk_default_oklab_to_oklch);
CONCAT(gdk_default_rec2020_to_xyz, gdk_default_rec2020_to_rec2020_linear, gdk_default_rec2020_linear_to_xyz);
CONCAT(gdk_default_xyz_to_rec2020, gdk_default_xyz_to_rec2020_linear, gdk_default_rec2020_linear_to_rec2020);

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
      [GDK_COLOR_STATE_ID_XYZ]  = gdk_default_srgb_to_xyz,
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
      [GDK_COLOR_STATE_ID_XYZ]  = gdk_default_srgb_linear_to_xyz,
    },
  },
  [GDK_COLOR_STATE_ID_XYZ] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_XYZ,
    },
    .name = "xyz",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_SRGB] = gdk_default_xyz_to_srgb,
      [GDK_COLOR_STATE_ID_SRGB_LINEAR] = gdk_default_xyz_to_srgb_linear,
      [GDK_COLOR_STATE_ID_OKLAB] = gdk_default_xyz_to_oklab,
      [GDK_COLOR_STATE_ID_OKLCH] = gdk_default_xyz_to_oklch,
      [GDK_COLOR_STATE_ID_REC2020] = gdk_default_xyz_to_rec2020,
      [GDK_COLOR_STATE_ID_REC2020_LINEAR] = gdk_default_xyz_to_rec2020_linear,
    },
  },
  [GDK_COLOR_STATE_ID_OKLAB] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_SRGB_LINEAR,
    },
    .name = "oklab",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_XYZ] = gdk_default_oklab_to_xyz,
    },
  },
  [GDK_COLOR_STATE_ID_OKLCH] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_SRGB_LINEAR,
    },
    .name = "oklch",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_XYZ] = gdk_default_oklch_to_xyz,
    },
  },
  [GDK_COLOR_STATE_ID_REC2020] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_REC2020_LINEAR,
    },
    .name = "rec2020",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_XYZ] = gdk_default_rec2020_to_xyz,
    },
  },
  [GDK_COLOR_STATE_ID_REC2020_LINEAR] = {
    .parent = {
      .klass = &GDK_DEFAULT_COLOR_STATE_CLASS,
      .ref_count = 0,
      .depth = GDK_MEMORY_FLOAT16,
      .rendering_color_state = GDK_COLOR_STATE_REC2020_LINEAR,
    },
    .name = "rec2020-linear",
    .no_srgb = NULL,
    .convert_to = {
      [GDK_COLOR_STATE_ID_XYZ] = gdk_default_rec2020_linear_to_xyz,
    },
  },
};

/* }}} */
/* {{{ Private API */

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
