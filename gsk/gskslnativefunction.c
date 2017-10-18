/* GTK - The GIMP Toolkit
 *   
 * Copyright © 2017 Benjamin Otte <otte@gnome.org>
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

#include "gskslnativefunctionprivate.h"

#include "gskslenvironmentprivate.h"
#include "gskslimagetypeprivate.h"
#include "gskslfunctionprivate.h"
#include "gskslfunctiontypeprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltypeprivate.h"
#include "gskspvwriterprivate.h"

#include <math.h>

#define GSK_SL_BUILTIN_TYPE_VOID (gsk_sl_type_get_void ())
#define GSK_SL_BUILTIN_TYPE_FLOAT (gsk_sl_type_get_scalar (GSK_SL_FLOAT))
#define GSK_SL_BUILTIN_TYPE_DOUBLE (gsk_sl_type_get_scalar (GSK_SL_DOUBLE))
#define GSK_SL_BUILTIN_TYPE_INT (gsk_sl_type_get_scalar (GSK_SL_INT))
#define GSK_SL_BUILTIN_TYPE_UINT (gsk_sl_type_get_scalar (GSK_SL_UINT))
#define GSK_SL_BUILTIN_TYPE_BOOL (gsk_sl_type_get_scalar (GSK_SL_BOOL))
#define GSK_SL_BUILTIN_TYPE_BVEC2 (gsk_sl_type_get_vector (GSK_SL_BOOL, 2))
#define GSK_SL_BUILTIN_TYPE_BVEC3 (gsk_sl_type_get_vector (GSK_SL_BOOL, 3))
#define GSK_SL_BUILTIN_TYPE_BVEC4 (gsk_sl_type_get_vector (GSK_SL_BOOL, 4))
#define GSK_SL_BUILTIN_TYPE_IVEC2 (gsk_sl_type_get_vector (GSK_SL_INT, 2))
#define GSK_SL_BUILTIN_TYPE_IVEC3 (gsk_sl_type_get_vector (GSK_SL_INT, 3))
#define GSK_SL_BUILTIN_TYPE_IVEC4 (gsk_sl_type_get_vector (GSK_SL_INT, 4))
#define GSK_SL_BUILTIN_TYPE_UVEC2 (gsk_sl_type_get_vector (GSK_SL_UINT, 2))
#define GSK_SL_BUILTIN_TYPE_UVEC3 (gsk_sl_type_get_vector (GSK_SL_UINT, 3))
#define GSK_SL_BUILTIN_TYPE_UVEC4 (gsk_sl_type_get_vector (GSK_SL_UINT, 4))
#define GSK_SL_BUILTIN_TYPE_VEC2 (gsk_sl_type_get_vector (GSK_SL_FLOAT, 2))
#define GSK_SL_BUILTIN_TYPE_VEC3 (gsk_sl_type_get_vector (GSK_SL_FLOAT, 3))
#define GSK_SL_BUILTIN_TYPE_VEC4 (gsk_sl_type_get_vector (GSK_SL_FLOAT, 4))
#define GSK_SL_BUILTIN_TYPE_DVEC2 (gsk_sl_type_get_vector (GSK_SL_DOUBLE, 2))
#define GSK_SL_BUILTIN_TYPE_DVEC3 (gsk_sl_type_get_vector (GSK_SL_DOUBLE, 3))
#define GSK_SL_BUILTIN_TYPE_DVEC4 (gsk_sl_type_get_vector (GSK_SL_DOUBLE, 4))
#define GSK_SL_BUILTIN_TYPE_MAT2 (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 2, 2))
#define GSK_SL_BUILTIN_TYPE_MAT2X3 (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 2, 3))
#define GSK_SL_BUILTIN_TYPE_MAT2X4 (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 2, 4))
#define GSK_SL_BUILTIN_TYPE_MAT3X2 (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 3, 2))
#define GSK_SL_BUILTIN_TYPE_MAT3 (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 3, 3))
#define GSK_SL_BUILTIN_TYPE_MAT3X4 (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 3, 4))
#define GSK_SL_BUILTIN_TYPE_MAT4X2 (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 4, 2))
#define GSK_SL_BUILTIN_TYPE_MAT4X3 (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 4, 3))
#define GSK_SL_BUILTIN_TYPE_MAT4 (gsk_sl_type_get_matrix (GSK_SL_FLOAT, 4, 4))
#define GSK_SL_BUILTIN_TYPE_DMAT2 (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 2, 2))
#define GSK_SL_BUILTIN_TYPE_DMAT2X3 (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 2, 3))
#define GSK_SL_BUILTIN_TYPE_DMAT2X4 (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 2, 4))
#define GSK_SL_BUILTIN_TYPE_DMAT3X2 (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 3, 2))
#define GSK_SL_BUILTIN_TYPE_DMAT3 (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 3, 3))
#define GSK_SL_BUILTIN_TYPE_DMAT3X4 (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 3, 4))
#define GSK_SL_BUILTIN_TYPE_DMAT4X2 (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 4, 2))
#define GSK_SL_BUILTIN_TYPE_DMAT4X3 (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 4, 3))
#define GSK_SL_BUILTIN_TYPE_DMAT4 (gsk_sl_type_get_matrix (GSK_SL_DOUBLE, 4, 4))

#define TYPE_VOID void
#define TYPE_FLOAT float
#define TYPE_DOUBLE double
#define TYPE_INT gint32
#define TYPE_UINT guint32
#define TYPE_BOOL guint32
#define TYPE_BVEC2 guint32
#define TYPE_BVEC3 guint32
#define TYPE_BVEC4 guint32
#define TYPE_IVEC2 gint32
#define TYPE_IVEC3 gint32
#define TYPE_IVEC4 gint32
#define TYPE_UVEC2 guint32
#define TYPE_UVEC3 guint32
#define TYPE_UVEC4 guint32
#define TYPE_VEC2 float
#define TYPE_VEC3 float
#define TYPE_VEC4 float
#define TYPE_DVEC2 double
#define TYPE_DVEC3 double
#define TYPE_DVEC4 double
#define TYPE_MAT2 float
#define TYPE_MAT2X3 float
#define TYPE_MAT2X4 float
#define TYPE_MAT3X2 float
#define TYPE_MAT3 float
#define TYPE_MAT3X4 float
#define TYPE_MAT4X2 float
#define TYPE_MAT4X3 float
#define TYPE_MAT4 float
#define TYPE_DMAT2 double
#define TYPE_DMAT2X3 double
#define TYPE_DMAT2X4 double
#define TYPE_DMAT3X2 double
#define TYPE_DMAT3 double
#define TYPE_DMAT3X4 double
#define TYPE_DMAT4X2 double
#define TYPE_DMAT4X3 double
#define TYPE_DMAT4 double

#define TYPE_VOID_LEN 0
#define TYPE_FLOAT_LEN 1
#define TYPE_DOUBLE_LEN 1
#define TYPE_INT_LEN 1
#define TYPE_UINT_LEN 1
#define TYPE_BOOL_LEN 1
#define TYPE_BVEC2_LEN 2
#define TYPE_BVEC3_LEN 3
#define TYPE_BVEC4_LEN 4
#define TYPE_IVEC2_LEN 2
#define TYPE_IVEC3_LEN 3
#define TYPE_IVEC4_LEN 4
#define TYPE_UVEC2_LEN 2
#define TYPE_UVEC3_LEN 3
#define TYPE_UVEC4_LEN 4
#define TYPE_VEC2_LEN 2
#define TYPE_VEC3_LEN 3
#define TYPE_VEC4_LEN 4
#define TYPE_DVEC2_LEN 2
#define TYPE_DVEC3_LEN 3
#define TYPE_DVEC4_LEN 4
#define TYPE_MAT2_LEN 4
#define TYPE_MAT2X3_LEN 6
#define TYPE_MAT2X4_LEN 8
#define TYPE_MAT3X2_LEN 6
#define TYPE_MAT3_LEN 9
#define TYPE_MAT3X4_LEN 12
#define TYPE_MAT4X2_LEN 8
#define TYPE_MAT4X3_LEN 12
#define TYPE_MAT4_LEN 16
#define TYPE_DMAT2_LEN 4
#define TYPE_DMAT2X3_LEN 6
#define TYPE_DMAT2X4_LEN 8
#define TYPE_DMAT3X2_LEN 6
#define TYPE_DMAT3_LEN 9
#define TYPE_DMAT3X4_LEN 12
#define TYPE_DMAT4X2_LEN 8
#define TYPE_DMAT4X3_LEN 12
#define TYPE_DMAT4_LEN 16

#define NATIVE1_FUNCS(result, name, arg1, init_code, loop_code, final_code) \
static void \
gsk_sl_native_ ## name ## _ ## arg1 ## _get_constant (gpointer *retval, gpointer *arguments, gpointer user_data) \
{\
  TYPE_ ## result res = 0; \
  gsize i; \
\
  init_code\
\
  for (i = 0; i < TYPE_ ## arg1 ## _LEN; i++) \
    {\
      G_GNUC_UNUSED TYPE_ ## arg1 x = ((TYPE_ ## arg1 *) arguments[0])[i % TYPE_ ## arg1 ## _LEN]; \
      \
      loop_code \
      \
      ((TYPE_ ## result *) retval)[i % TYPE_ ## result ## _LEN] = res; \
    } \
\
  final_code \
}\
\
static guint32 \
gsk_sl_native_ ## name ## _ ## arg1 ## _write_spv (GskSpvWriter *writer, guint32 *arguments, gpointer user_data) \
{ \
  return gsk_spv_writer_ ## name (writer, GSK_SL_BUILTIN_TYPE_ ## result, arguments[0]); \
}

#define NATIVE2_FUNCS(result, name, arg1, arg2, init_code, loop_code, final_code) \
static void \
gsk_sl_native_ ## name ## _ ## arg1 ## arg2 ## _get_constant (gpointer *retval, gpointer *arguments, gpointer user_data) \
{\
  TYPE_ ## result res = 0; \
  gsize i; \
\
  init_code\
\
  for (i = 0; i < MAX (TYPE_ ## arg1 ## _LEN, TYPE_ ## arg2 ## _LEN); i++) \
    {\
      G_GNUC_UNUSED TYPE_ ## arg1 x = ((TYPE_ ## arg1 *) arguments[0])[i % TYPE_ ## arg1 ## _LEN]; \
      G_GNUC_UNUSED TYPE_ ## arg2 y = ((TYPE_ ## arg2 *) arguments[1])[i % TYPE_ ## arg2 ## _LEN]; \
      \
      loop_code \
      \
      ((TYPE_ ## result *) retval)[i % TYPE_ ## result ## _LEN] = res; \
    } \
\
  final_code \
}\
\
static guint32 \
gsk_sl_native_ ## name ## _ ## arg1 ## arg2 ## _write_spv (GskSpvWriter *writer, guint32 *arguments, gpointer user_data) \
{ \
  if (TYPE_ ## arg1 ## _LEN < TYPE_ ## arg2 ## _LEN) \
    arguments[0] = gsk_spv_writer_composite_construct (writer, GSK_SL_BUILTIN_TYPE_ ## arg2, (guint32[4]) { arguments[0], arguments[0], arguments[0], arguments[0] }, TYPE_ ## arg2 ## _LEN); \
  if (TYPE_ ## arg2 ## _LEN < TYPE_ ## arg1 ## _LEN) \
    arguments[1] = gsk_spv_writer_composite_construct (writer, GSK_SL_BUILTIN_TYPE_ ## arg1, (guint32[4]) { arguments[1], arguments[1], arguments[1], arguments[1] }, TYPE_ ## arg1 ## _LEN); \
  return gsk_spv_writer_ ## name (writer, GSK_SL_BUILTIN_TYPE_ ## result, arguments[0], arguments[1]); \
}

#define NATIVE3_FUNCS(result, name, arg1, arg2, arg3, init_code, loop_code, final_code) \
static void \
gsk_sl_native_ ## name ## _ ## arg1 ## arg2 ## arg3 ## _get_constant (gpointer *retval, gpointer *arguments, gpointer user_data) \
{\
  TYPE_ ## result res = 0; \
  gsize i; \
\
  init_code \
\
  for (i = 0; i < MAX (TYPE_ ## arg1 ## _LEN, MAX (TYPE_ ## arg2 ## _LEN, TYPE_ ## arg3 ## _LEN)); i++) \
    {\
      G_GNUC_UNUSED TYPE_ ## arg1 x = ((TYPE_ ## arg1 *) arguments[0])[i % TYPE_ ## arg1 ## _LEN]; \
      G_GNUC_UNUSED TYPE_ ## arg2 y = ((TYPE_ ## arg2 *) arguments[1])[i % TYPE_ ## arg2 ## _LEN]; \
      G_GNUC_UNUSED TYPE_ ## arg3 z = ((TYPE_ ## arg3 *) arguments[2])[i % TYPE_ ## arg3 ## _LEN]; \
      \
      loop_code \
      \
      ((TYPE_ ## result *) retval)[i % TYPE_ ## result ## _LEN] = res; \
    } \
\
  final_code \
}\
\
static guint32 \
gsk_sl_native_ ## name ## _ ## arg1 ## arg2 ## arg3 ## _write_spv (GskSpvWriter *writer, guint32 *arguments, gpointer user_data) \
{ \
  if (TYPE_ ## arg1 ## _LEN < TYPE_ ## arg2 ## _LEN) \
    arguments[0] = gsk_spv_writer_composite_construct (writer, GSK_SL_BUILTIN_TYPE_ ## arg2, (guint32[4]) { arguments[0], arguments[0], arguments[0], arguments[0] }, TYPE_ ## arg2 ## _LEN); \
  else if (TYPE_ ## arg1 ## _LEN < TYPE_ ## arg3 ## _LEN) \
    arguments[0] = gsk_spv_writer_composite_construct (writer, GSK_SL_BUILTIN_TYPE_ ## arg3, (guint32[4]) { arguments[0], arguments[0], arguments[0], arguments[0] }, TYPE_ ## arg3 ## _LEN); \
  if (TYPE_ ## arg2 ## _LEN < TYPE_ ## arg1 ## _LEN) \
    arguments[1] = gsk_spv_writer_composite_construct (writer, GSK_SL_BUILTIN_TYPE_ ## arg1, (guint32[4]) { arguments[1], arguments[1], arguments[1], arguments[1] }, TYPE_ ## arg1 ## _LEN); \
  else if (TYPE_ ## arg2 ## _LEN < TYPE_ ## arg3 ## _LEN) \
    arguments[1] = gsk_spv_writer_composite_construct (writer, GSK_SL_BUILTIN_TYPE_ ## arg3, (guint32[4]) { arguments[1], arguments[1], arguments[1], arguments[1] }, TYPE_ ## arg3 ## _LEN); \
  if (TYPE_ ## arg3 ## _LEN < TYPE_ ## arg1 ## _LEN) \
    arguments[2] = gsk_spv_writer_composite_construct (writer, GSK_SL_BUILTIN_TYPE_ ## arg1, (guint32[4]) { arguments[2], arguments[2], arguments[2], arguments[2] }, TYPE_ ## arg1 ## _LEN); \
  else if (TYPE_ ## arg3 ## _LEN < TYPE_ ## arg2 ## _LEN) \
    arguments[2] = gsk_spv_writer_composite_construct (writer, GSK_SL_BUILTIN_TYPE_ ## arg2, (guint32[4]) { arguments[2], arguments[2], arguments[2], arguments[2] }, TYPE_ ## arg2 ## _LEN); \
  return gsk_spv_writer_ ## name (writer, GSK_SL_BUILTIN_TYPE_ ## result, arguments[0], arguments[1], arguments[2]); \
}

NATIVE1_FUNCS (FLOAT, radians, FLOAT, , res = G_PI * x / 180.0;, )
NATIVE1_FUNCS (VEC2, radians, VEC2, , res = G_PI * x / 180.0;, )
NATIVE1_FUNCS (VEC3, radians, VEC3, , res = G_PI * x / 180.0;, )
NATIVE1_FUNCS (VEC4, radians, VEC4, , res = G_PI * x / 180.0;, )
NATIVE1_FUNCS (FLOAT, degrees, FLOAT, , res = 180.0 * x / G_PI;, )
NATIVE1_FUNCS (VEC2, degrees, VEC2, , res = 180.0 * x / G_PI;, )
NATIVE1_FUNCS (VEC3, degrees, VEC3, , res = 180.0 * x / G_PI;, )
NATIVE1_FUNCS (VEC4, degrees, VEC4, , res = 180.0 * x / G_PI;, )
NATIVE1_FUNCS (FLOAT, sin, FLOAT, , res = sin (x);, )
NATIVE1_FUNCS (VEC2, sin, VEC2, , res = sinf (x);, )
NATIVE1_FUNCS (VEC3, sin, VEC3, , res = sinf (x);, )
NATIVE1_FUNCS (VEC4, sin, VEC4, , res = sinf (x);, )
NATIVE1_FUNCS (FLOAT, cos, FLOAT, , res = cosf (x);, )
NATIVE1_FUNCS (VEC2, cos, VEC2, , res = cosf (x);, )
NATIVE1_FUNCS (VEC3, cos, VEC3, , res = cosf (x);, )
NATIVE1_FUNCS (VEC4, cos, VEC4, , res = cosf (x);, )
NATIVE1_FUNCS (FLOAT, tan, FLOAT, , res = tanf (x);, )
NATIVE1_FUNCS (VEC2, tan, VEC2, , res = tanf (x);, )
NATIVE1_FUNCS (VEC3, tan, VEC3, , res = tanf (x);, )
NATIVE1_FUNCS (VEC4, tan, VEC4, , res = tanf (x);, )
NATIVE1_FUNCS (FLOAT, asin, FLOAT, , res = asinf (x);, )
NATIVE1_FUNCS (VEC2, asin, VEC2, , res = asinf (x);, )
NATIVE1_FUNCS (VEC3, asin, VEC3, , res = asinf (x);, )
NATIVE1_FUNCS (VEC4, asin, VEC4, , res = asinf (x);, )
NATIVE1_FUNCS (FLOAT, acos, FLOAT, , res = acosf (x);, )
NATIVE1_FUNCS (VEC2, acos, VEC2, , res = acosf (x);, )
NATIVE1_FUNCS (VEC3, acos, VEC3, , res = acosf (x);, )
NATIVE1_FUNCS (VEC4, acos, VEC4, , res = acosf (x);, )
#define gsk_spv_writer_atan gsk_spv_writer_atan2
NATIVE2_FUNCS (FLOAT, atan, FLOAT, FLOAT, , res = atan2f (x, y);, )
NATIVE2_FUNCS (VEC2, atan, VEC2, VEC2, , res = atan2f (x, y);, )
NATIVE2_FUNCS (VEC3, atan, VEC3, VEC3, , res = atan2f (x, y);, )
NATIVE2_FUNCS (VEC4, atan, VEC4, VEC4, , res = atan2f (x, y);, )
#undef gsk_spv_writer_atan
NATIVE1_FUNCS (FLOAT, atan, FLOAT, , res = atanf (x);, )
NATIVE1_FUNCS (VEC2, atan, VEC2, , res = atanf (x);, )
NATIVE1_FUNCS (VEC3, atan, VEC3, , res = atanf (x);, )
NATIVE1_FUNCS (VEC4, atan, VEC4, , res = atanf (x);, )

NATIVE2_FUNCS (FLOAT, pow, FLOAT, FLOAT, , res = powf (x, y);, )
NATIVE2_FUNCS (VEC2, pow, VEC2, VEC2, , res = powf (x, y);, )
NATIVE2_FUNCS (VEC3, pow, VEC3, VEC3, , res = powf (x, y);, )
NATIVE2_FUNCS (VEC4, pow, VEC4, VEC4, , res = powf (x, y);, )
NATIVE1_FUNCS (FLOAT, exp, FLOAT, , res = expf (x);, )
NATIVE1_FUNCS (VEC2, exp, VEC2, , res = expf (x);, )
NATIVE1_FUNCS (VEC3, exp, VEC3, , res = expf (x);, )
NATIVE1_FUNCS (VEC4, exp, VEC4, , res = expf (x);, )
NATIVE1_FUNCS (FLOAT, log, FLOAT, , res = logf (x);, )
NATIVE1_FUNCS (VEC2, log, VEC2, , res = logf (x);, )
NATIVE1_FUNCS (VEC3, log, VEC3, , res = logf (x);, )
NATIVE1_FUNCS (VEC4, log, VEC4, , res = logf (x);, )
NATIVE1_FUNCS (FLOAT, exp2, FLOAT, , res = exp2f (x);, )
NATIVE1_FUNCS (VEC2, exp2, VEC2, , res = exp2f (x);, )
NATIVE1_FUNCS (VEC3, exp2, VEC3, , res = exp2f (x);, )
NATIVE1_FUNCS (VEC4, exp2, VEC4, , res = exp2f (x);, )
NATIVE1_FUNCS (FLOAT, log2, FLOAT, , res = log2f (x);, )
NATIVE1_FUNCS (VEC2, log2, VEC2, , res = log2f (x);, )
NATIVE1_FUNCS (VEC3, log2, VEC3, , res = log2f (x);, )
NATIVE1_FUNCS (VEC4, log2, VEC4, , res = log2f (x);, )
NATIVE1_FUNCS (FLOAT, sqrt, FLOAT, , res = sqrtf (x);, )
NATIVE1_FUNCS (VEC2, sqrt, VEC2, , res = sqrtf (x);, )
NATIVE1_FUNCS (VEC3, sqrt, VEC3, , res = sqrtf (x);, )
NATIVE1_FUNCS (VEC4, sqrt, VEC4, , res = sqrtf (x);, )
#define gsk_spv_writer_inversesqrt gsk_spv_writer_inverse_sqrt
NATIVE1_FUNCS (FLOAT, inversesqrt, FLOAT, , res = 1.0 / sqrtf (x);, )
NATIVE1_FUNCS (VEC2, inversesqrt, VEC2, , res = 1.0 / sqrtf (x);, )
NATIVE1_FUNCS (VEC3, inversesqrt, VEC3, , res = 1.0 / sqrtf (x);, )
NATIVE1_FUNCS (VEC4, inversesqrt, VEC4, , res = 1.0 / sqrtf (x);, )
#undef gsk_spv_writer_inversesqrt

#define gsk_spv_writer_abs gsk_spv_writer_f_abs
NATIVE1_FUNCS (FLOAT, abs, FLOAT, , res = fabsf (x);, )
NATIVE1_FUNCS (VEC2, abs, VEC2, , res = fabsf (x);, )
NATIVE1_FUNCS (VEC3, abs, VEC3, , res = fabsf (x);, )
NATIVE1_FUNCS (VEC4, abs, VEC4, , res = fabsf (x);, )
#undef gsk_spv_writer_abs
#define gsk_spv_writer_abs gsk_spv_writer_s_abs
NATIVE1_FUNCS (INT, abs, INT, , res = ABS (x);, )
NATIVE1_FUNCS (IVEC2, abs, IVEC2, , res = ABS (x);, )
NATIVE1_FUNCS (IVEC3, abs, IVEC3, , res = ABS (x);, )
NATIVE1_FUNCS (IVEC4, abs, IVEC4, , res = ABS (x);, )
#undef gsk_spv_writer_abs
#define gsk_spv_writer_sign gsk_spv_writer_f_sign
NATIVE1_FUNCS (FLOAT, sign, FLOAT, , res = x < 0 ? -1 : x > 0 ? 1 : 0;, )
NATIVE1_FUNCS (VEC2, sign, VEC2, , res = x < 0 ? -1 : x > 0 ? 1 : 0;, )
NATIVE1_FUNCS (VEC3, sign, VEC3, , res = x < 0 ? -1 : x > 0 ? 1 : 0;, )
NATIVE1_FUNCS (VEC4, sign, VEC4, , res = x < 0 ? -1 : x > 0 ? 1 : 0;, )
#undef gsk_spv_writer_sign
#define gsk_spv_writer_sign gsk_spv_writer_s_sign
NATIVE1_FUNCS (INT, sign, INT, , res = x < 0 ? -1 : x > 0 ? 1 : 0;, )
NATIVE1_FUNCS (IVEC2, sign, IVEC2, , res = x < 0 ? -1 : x > 0 ? 1 : 0;, )
NATIVE1_FUNCS (IVEC3, sign, IVEC3, , res = x < 0 ? -1 : x > 0 ? 1 : 0;, )
NATIVE1_FUNCS (IVEC4, sign, IVEC4, , res = x < 0 ? -1 : x > 0 ? 1 : 0;, )
#undef gsk_spv_writer_sign
NATIVE1_FUNCS (FLOAT, floor, FLOAT, , res = floorf (x);, )
NATIVE1_FUNCS (VEC2, floor, VEC2, , res = floorf (x);, )
NATIVE1_FUNCS (VEC3, floor, VEC3, , res = floorf (x);, )
NATIVE1_FUNCS (VEC4, floor, VEC4, , res = floorf (x);, )
NATIVE1_FUNCS (FLOAT, ceil, FLOAT, , res = ceilf (x);, )
NATIVE1_FUNCS (VEC2, ceil, VEC2, , res = ceilf (x);, )
NATIVE1_FUNCS (VEC3, ceil, VEC3, , res = ceilf (x);, )
NATIVE1_FUNCS (VEC4, ceil, VEC4, , res = ceilf (x);, )
NATIVE1_FUNCS (FLOAT, fract, FLOAT, , res = x - floorf (x);, )
NATIVE1_FUNCS (VEC2, fract, VEC2, , res = x - floorf (x);, )
NATIVE1_FUNCS (VEC3, fract, VEC3, , res = x - floorf (x);, )
NATIVE1_FUNCS (VEC4, fract, VEC4, , res = x - floorf (x);, )
NATIVE1_FUNCS (FLOAT, trunc, FLOAT, , res = x > 0 ? floorf (x) : ceilf (x);, );
NATIVE1_FUNCS (VEC2, trunc, VEC2, , res = x > 0 ? floorf (x) : ceilf (x);, );
NATIVE1_FUNCS (VEC3, trunc, VEC3, , res = x > 0 ? floorf (x) : ceilf (x);, );
NATIVE1_FUNCS (VEC4, trunc, VEC4, , res = x > 0 ? floorf (x) : ceilf (x);, );
NATIVE1_FUNCS (FLOAT, round, FLOAT, , res = roundf (x);, );
NATIVE1_FUNCS (VEC2, round, VEC2, , res = roundf (x);, );
NATIVE1_FUNCS (VEC3, round, VEC3, , res = roundf (x);, );
NATIVE1_FUNCS (VEC4, round, VEC4, , res = roundf (x);, );
#define gsk_spv_writer_roundEven gsk_spv_writer_round_even
NATIVE1_FUNCS (FLOAT, roundEven, FLOAT, , res = floorf (x / 2) == floorf (x) / 2 ? ceilf (x - 0.5) : floorf (x + 0.5);, );
NATIVE1_FUNCS (VEC2, roundEven, VEC2, , res = floorf (x / 2) == floorf (x) / 2 ? ceilf (x - 0.5) : floorf (x + 0.5);, );
NATIVE1_FUNCS (VEC3, roundEven, VEC3, , res = floorf (x / 2) == floorf (x) / 2 ? ceilf (x - 0.5) : floorf (x + 0.5);, );
NATIVE1_FUNCS (VEC4, roundEven, VEC4, , res = floorf (x / 2) == floorf (x) / 2 ? ceilf (x - 0.5) : floorf (x + 0.5);, );
#undef gsk_spv_writer_roundEven
#define gsk_spv_writer_mod gsk_spv_writer_f_mod
NATIVE2_FUNCS (FLOAT, mod, FLOAT, FLOAT, , res = y ? fmod (x, y) : x;, )
NATIVE2_FUNCS (VEC2, mod, VEC2, FLOAT, , res = y ? fmod (x, y) : x;, )
NATIVE2_FUNCS (VEC3, mod, VEC3, FLOAT, , res = y ? fmod (x, y) : x;, )
NATIVE2_FUNCS (VEC4, mod, VEC4, FLOAT, , res = y ? fmod (x, y) : x;, )
NATIVE2_FUNCS (VEC2, mod, VEC2, VEC2, , res = y ? fmod (x, y) : x;, )
NATIVE2_FUNCS (VEC3, mod, VEC3, VEC3, , res = y ? fmod (x, y) : x;, )
NATIVE2_FUNCS (VEC4, mod, VEC4, VEC4, , res = y ? fmod (x, y) : x;, )
#undef gsk_spv_writer_mod
#define gsk_spv_writer_min gsk_spv_writer_f_min
NATIVE2_FUNCS (FLOAT, min, FLOAT, FLOAT, , res = MIN (x, y);, )
NATIVE2_FUNCS (VEC2, min, VEC2, FLOAT, , res = MIN (x, y);, )
NATIVE2_FUNCS (VEC3, min, VEC3, FLOAT, , res = MIN (x, y);, )
NATIVE2_FUNCS (VEC4, min, VEC4, FLOAT, , res = MIN (x, y);, )
NATIVE2_FUNCS (VEC2, min, VEC2, VEC2, , res = MIN (x, y);, )
NATIVE2_FUNCS (VEC3, min, VEC3, VEC3, , res = MIN (x, y);, )
NATIVE2_FUNCS (VEC4, min, VEC4, VEC4, , res = MIN (x, y);, )
#undef gsk_spv_writer_min
#define gsk_spv_writer_min gsk_spv_writer_s_min
NATIVE2_FUNCS (INT, min, INT, INT, , res = MIN (x, y);, );
NATIVE2_FUNCS (IVEC2, min, IVEC2, INT, , res = MIN (x, y);, );
NATIVE2_FUNCS (IVEC3, min, IVEC3, INT, , res = MIN (x, y);, );
NATIVE2_FUNCS (IVEC4, min, IVEC4, INT, , res = MIN (x, y);, );
NATIVE2_FUNCS (IVEC2, min, IVEC2, IVEC2, , res = MIN (x, y);, );
NATIVE2_FUNCS (IVEC3, min, IVEC3, IVEC3, , res = MIN (x, y);, );
NATIVE2_FUNCS (IVEC4, min, IVEC4, IVEC4, , res = MIN (x, y);, );
#undef gsk_spv_writer_min
#define gsk_spv_writer_min gsk_spv_writer_u_min
NATIVE2_FUNCS (UINT, min, UINT, UINT, , res = MIN (x, y);, );
NATIVE2_FUNCS (UVEC2, min, UVEC2, UINT, , res = MIN (x, y);, );
NATIVE2_FUNCS (UVEC3, min, UVEC3, UINT, , res = MIN (x, y);, );
NATIVE2_FUNCS (UVEC4, min, UVEC4, UINT, , res = MIN (x, y);, );
NATIVE2_FUNCS (UVEC2, min, UVEC2, UVEC2, , res = MIN (x, y);, );
NATIVE2_FUNCS (UVEC3, min, UVEC3, UVEC3, , res = MIN (x, y);, );
NATIVE2_FUNCS (UVEC4, min, UVEC4, UVEC4, , res = MIN (x, y);, );
#undef gsk_spv_writer_min
#define gsk_spv_writer_max gsk_spv_writer_f_max
NATIVE2_FUNCS (FLOAT, max, FLOAT, FLOAT, , res = MAX (x, y);, )
NATIVE2_FUNCS (VEC2, max, VEC2, FLOAT, , res = MAX (x, y);, )
NATIVE2_FUNCS (VEC3, max, VEC3, FLOAT, , res = MAX (x, y);, )
NATIVE2_FUNCS (VEC4, max, VEC4, FLOAT, , res = MAX (x, y);, )
NATIVE2_FUNCS (VEC2, max, VEC2, VEC2, , res = MAX (x, y);, )
NATIVE2_FUNCS (VEC3, max, VEC3, VEC3, , res = MAX (x, y);, )
NATIVE2_FUNCS (VEC4, max, VEC4, VEC4, , res = MAX (x, y);, )
#undef gsk_spv_writer_max
#define gsk_spv_writer_max gsk_spv_writer_s_max
NATIVE2_FUNCS (INT, max, INT, INT, , res = MAX (x, y);, );
NATIVE2_FUNCS (IVEC2, max, IVEC2, INT, , res = MAX (x, y);, );
NATIVE2_FUNCS (IVEC3, max, IVEC3, INT, , res = MAX (x, y);, );
NATIVE2_FUNCS (IVEC4, max, IVEC4, INT, , res = MAX (x, y);, );
NATIVE2_FUNCS (IVEC2, max, IVEC2, IVEC2, , res = MAX (x, y);, );
NATIVE2_FUNCS (IVEC3, max, IVEC3, IVEC3, , res = MAX (x, y);, );
NATIVE2_FUNCS (IVEC4, max, IVEC4, IVEC4, , res = MAX (x, y);, );
#undef gsk_spv_writer_max
#define gsk_spv_writer_max gsk_spv_writer_u_max
NATIVE2_FUNCS (UINT, max, UINT, UINT, , res = MAX (x, y);, );
NATIVE2_FUNCS (UVEC2, max, UVEC2, UINT, , res = MAX (x, y);, );
NATIVE2_FUNCS (UVEC3, max, UVEC3, UINT, , res = MAX (x, y);, );
NATIVE2_FUNCS (UVEC4, max, UVEC4, UINT, , res = MAX (x, y);, );
NATIVE2_FUNCS (UVEC2, max, UVEC2, UVEC2, , res = MAX (x, y);, );
NATIVE2_FUNCS (UVEC3, max, UVEC3, UVEC3, , res = MAX (x, y);, );
NATIVE2_FUNCS (UVEC4, max, UVEC4, UVEC4, , res = MAX (x, y);, );
#undef gsk_spv_writer_max
#define gsk_spv_writer_clamp gsk_spv_writer_f_clamp
NATIVE3_FUNCS (FLOAT, clamp, FLOAT, FLOAT, FLOAT, , res = CLAMP (x, y, z);, )
NATIVE3_FUNCS (VEC2, clamp, VEC2, FLOAT, FLOAT, , res = CLAMP (x, y, z);, )
NATIVE3_FUNCS (VEC3, clamp, VEC3, FLOAT, FLOAT, , res = CLAMP (x, y, z);, )
NATIVE3_FUNCS (VEC4, clamp, VEC4, FLOAT, FLOAT, , res = CLAMP (x, y, z);, )
NATIVE3_FUNCS (VEC2, clamp, VEC2, VEC2, VEC2, , res = CLAMP (x, y, z);, )
NATIVE3_FUNCS (VEC3, clamp, VEC3, VEC3, VEC3, , res = CLAMP (x, y, z);, )
NATIVE3_FUNCS (VEC4, clamp, VEC4, VEC4, VEC4, , res = CLAMP (x, y, z);, )
#undef gsk_spv_writer_clamp
#define gsk_spv_writer_clamp gsk_spv_writer_s_clamp
NATIVE3_FUNCS (INT, clamp, INT, INT, INT, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (IVEC2, clamp, IVEC2, INT, INT, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (IVEC3, clamp, IVEC3, INT, INT, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (IVEC4, clamp, IVEC4, INT, INT, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (IVEC2, clamp, IVEC2, IVEC2, IVEC2, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (IVEC3, clamp, IVEC3, IVEC3, IVEC3, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (IVEC4, clamp, IVEC4, IVEC4, IVEC4, , res = CLAMP (x, y, z);, );
#undef gsk_spv_writer_clamp
#define gsk_spv_writer_clamp gsk_spv_writer_u_clamp
NATIVE3_FUNCS (UINT, clamp, UINT, UINT, UINT, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (UVEC2, clamp, UVEC2, UINT, UINT, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (UVEC3, clamp, UVEC3, UINT, UINT, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (UVEC4, clamp, UVEC4, UINT, UINT, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (UVEC2, clamp, UVEC2, UVEC2, UVEC2, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (UVEC3, clamp, UVEC3, UVEC3, UVEC3, , res = CLAMP (x, y, z);, );
NATIVE3_FUNCS (UVEC4, clamp, UVEC4, UVEC4, UVEC4, , res = CLAMP (x, y, z);, );
#undef gsk_spv_writer_clamp
#define gsk_spv_writer_mix gsk_spv_writer_f_mix
NATIVE3_FUNCS (FLOAT, mix, FLOAT, FLOAT, FLOAT, , res = x * (1.0f - z) + y * z;, )
NATIVE3_FUNCS (VEC2, mix, VEC2, VEC2, FLOAT, , res = x * (1.0f - z) + y * z;, )
NATIVE3_FUNCS (VEC3, mix, VEC3, VEC3, FLOAT, , res = x * (1.0f - z) + y * z;, )
NATIVE3_FUNCS (VEC4, mix, VEC4, VEC4, FLOAT, , res = x * (1.0f - z) + y * z;, )
NATIVE3_FUNCS (VEC2, mix, VEC2, VEC2, VEC2, , res = x * (1.0f - z) + y * z;, )
NATIVE3_FUNCS (VEC3, mix, VEC3, VEC3, VEC3, , res = x * (1.0f - z) + y * z;, )
NATIVE3_FUNCS (VEC4, mix, VEC4, VEC4, VEC4, , res = x * (1.0f - z) + y * z;, )
#undef gsk_spv_writer_mix
#define gsk_spv_writer_mix(writer, type, x, y, b) gsk_spv_writer_select (writer, type, b, x, y)
NATIVE3_FUNCS (FLOAT, mix, FLOAT, FLOAT, BOOL, , res = z ? x : y;, );
NATIVE3_FUNCS (VEC2, mix, VEC2, VEC2, BVEC2, , res = z ? x : y;, );
NATIVE3_FUNCS (VEC3, mix, VEC3, VEC3, BVEC3, , res = z ? x : y;, );
NATIVE3_FUNCS (VEC4, mix, VEC4, VEC4, BVEC4, , res = z ? x : y;, );
#undef gsk_spv_writer_mix
NATIVE2_FUNCS (FLOAT, step, FLOAT, FLOAT, , res = y < x ? 0.0f : 1.0f;, )
NATIVE2_FUNCS (VEC2, step, FLOAT, VEC2, , res = y < x ? 0.0f : 1.0f;, )
NATIVE2_FUNCS (VEC3, step, FLOAT, VEC3, , res = y < x ? 0.0f : 1.0f;, )
NATIVE2_FUNCS (VEC4, step, FLOAT, VEC4, , res = y < x ? 0.0f : 1.0f;, )
NATIVE2_FUNCS (VEC2, step, VEC2, VEC2, , res = y < x ? 0.0f : 1.0f;, )
NATIVE2_FUNCS (VEC3, step, VEC3, VEC3, , res = y < x ? 0.0f : 1.0f;, )
NATIVE2_FUNCS (VEC4, step, VEC4, VEC4, , res = y < x ? 0.0f : 1.0f;, )
#define gsk_spv_writer_smoothstep gsk_spv_writer_smooth_step
NATIVE3_FUNCS (FLOAT, smoothstep, FLOAT, FLOAT, FLOAT, , float t = (z - x) / (y -x); t = CLAMP (t, 0.0f, 1.0f); res = t * t * (3.0f - 2.0f * t);, )
NATIVE3_FUNCS (VEC2, smoothstep, VEC2, VEC2, FLOAT, , float t = (z - x) / (y -x); t = CLAMP (t, 0.0f, 1.0f); res = t * t * (3.0f - 2.0f * t);, )
NATIVE3_FUNCS (VEC3, smoothstep, VEC3, VEC3, FLOAT, , float t = (z - x) / (y -x); t = CLAMP (t, 0.0f, 1.0f); res = t * t * (3.0f - 2.0f * t);, )
NATIVE3_FUNCS (VEC4, smoothstep, VEC4, VEC4, FLOAT, , float t = (z - x) / (y -x); t = CLAMP (t, 0.0f, 1.0f); res = t * t * (3.0f - 2.0f * t);, )
NATIVE3_FUNCS (VEC2, smoothstep, VEC2, VEC2, VEC2, , float t = (z - x) / (y -x); t = CLAMP (t, 0.0f, 1.0f); res = t * t * (3.0f - 2.0f * t);, )
NATIVE3_FUNCS (VEC3, smoothstep, VEC3, VEC3, VEC3, , float t = (z - x) / (y -x); t = CLAMP (t, 0.0f, 1.0f); res = t * t * (3.0f - 2.0f * t);, )
NATIVE3_FUNCS (VEC4, smoothstep, VEC4, VEC4, VEC4, , float t = (z - x) / (y -x); t = CLAMP (t, 0.0f, 1.0f); res = t * t * (3.0f - 2.0f * t);, )
#undef gsk_spv_writer_smoothstep
#define gsk_spv_writer_isnan gsk_spv_writer_is_nan
NATIVE1_FUNCS (BOOL, isnan, FLOAT, , res = isnanf (x);, );
NATIVE1_FUNCS (BVEC2, isnan, VEC2, , res = isnanf (x);, );
NATIVE1_FUNCS (BVEC3, isnan, VEC3, , res = isnanf (x);, );
NATIVE1_FUNCS (BVEC4, isnan, VEC4, , res = isnanf (x);, );
#undef gsk_spv_writer_isnan
#define gsk_spv_writer_isinf gsk_spv_writer_is_inf
NATIVE1_FUNCS (BOOL, isinf, FLOAT, , res = isinff (x);, );
NATIVE1_FUNCS (BVEC2, isinf, VEC2, , res = isinff (x);, );
NATIVE1_FUNCS (BVEC3, isinf, VEC3, , res = isinff (x);, );
NATIVE1_FUNCS (BVEC4, isinf, VEC4, , res = isinff (x);, );
#undef gsk_spv_writer_isinf

static float
dotf (float *x, float *y, gsize n)
{
  float res = 0;
  gsize i;

  for (i = 0; i < n; i++)
    {
      res += x[i] * y[i];
    }
  return res;
}

static void 
gsk_sl_native_length_FLOAT_get_constant (gpointer *retval, gpointer *arguments, gpointer user_data)
{
  *(float *) retval = dotf (arguments[0], arguments[0], 1);
}
static guint32
gsk_sl_native_length_FLOAT_write_spv (GskSpvWriter *writer, guint32 *arguments, gpointer user_data)
{
  return gsk_spv_writer_length (writer, GSK_SL_BUILTIN_TYPE_FLOAT, arguments[0]);
}
static void 
gsk_sl_native_length_VEC2_get_constant (gpointer *retval, gpointer *arguments, gpointer user_data)
{
  *(float *) retval = dotf (arguments[0], arguments[0], 2);
}
#define gsk_sl_native_length_VEC2_write_spv gsk_sl_native_length_FLOAT_write_spv
static void 
gsk_sl_native_length_VEC3_get_constant (gpointer *retval, gpointer *arguments, gpointer user_data)
{
  *(float *) retval = dotf (arguments[0], arguments[0], 3);
}
#define gsk_sl_native_length_VEC3_write_spv gsk_sl_native_length_FLOAT_write_spv
static void 
gsk_sl_native_length_VEC4_get_constant (gpointer *retval, gpointer *arguments, gpointer user_data)
{
  *(float *) retval = dotf (arguments[0], arguments[0], 4);
}
#define gsk_sl_native_length_VEC4_write_spv gsk_sl_native_length_FLOAT_write_spv

static void 
gsk_sl_native_distance_FLOATFLOAT_get_constant (gpointer *retval, gpointer *arguments, gpointer user_data)
{
  float x = ((float *) arguments[0])[0] - ((float *) arguments[1])[0];
  *(float *) retval = sqrtf (x * x);
}
static guint32
gsk_sl_native_distance_FLOATFLOAT_write_spv (GskSpvWriter *writer, guint32 *arguments, gpointer user_data)
{
  return gsk_spv_writer_distance (writer, GSK_SL_BUILTIN_TYPE_FLOAT, arguments[0], arguments[1]);
}
static void 
gsk_sl_native_distance_VEC2VEC2_get_constant (gpointer *retval, gpointer *arguments, gpointer user_data)
{
  float res = 0;
  gsize i;
  for (i = 0; i < 2; i++)
    {
      float tmp = ((float *) arguments[0])[i] - ((float *) arguments[1])[i];
      res += tmp * tmp;
    }
  *(float *) retval = sqrt (res);
}
#define gsk_sl_native_distance_VEC2VEC2_write_spv gsk_sl_native_distance_FLOATFLOAT_write_spv
static void 
gsk_sl_native_distance_VEC3VEC3_get_constant (gpointer *retval, gpointer *arguments, gpointer user_data)
{
  float res = 0;
  gsize i;
  for (i = 0; i < 3; i++)
    {
      float tmp = ((float *) arguments[0])[i] - ((float *) arguments[1])[i];
      res += tmp * tmp;
    }
  *(float *) retval = sqrt (res);
}
#define gsk_sl_native_distance_VEC3VEC3_write_spv gsk_sl_native_distance_FLOATFLOAT_write_spv
static void 
gsk_sl_native_distance_VEC4VEC4_get_constant (gpointer *retval, gpointer *arguments, gpointer user_data)
{
  float res = 0;
  gsize i;
  for (i = 0; i < 4; i++)
    {
      float tmp = ((float *) arguments[0])[i] - ((float *) arguments[1])[i];
      res += tmp * tmp;
    }
  *(float *) retval = sqrt (res);
}
#define gsk_sl_native_distance_VEC4VEC4_write_spv gsk_sl_native_distance_FLOATFLOAT_write_spv

NATIVE2_FUNCS (FLOAT, dot, FLOAT, FLOAT, , res += x * y;, );
NATIVE2_FUNCS (FLOAT, dot, VEC2, VEC2, , res += x * y;, );
NATIVE2_FUNCS (FLOAT, dot, VEC3, VEC3, , res += x * y;, );
NATIVE2_FUNCS (FLOAT, dot, VEC4, VEC4, , res += x * y;, );

static void 
gsk_sl_native_cross_VEC3VEC3_get_constant (gpointer *retval, gpointer *arguments, gpointer user_data)
{
  float *x = (float *) arguments[0];
  float *y = (float *) arguments[1];
  float *res = (float *) retval;

  res[0] = x[1] * y[2] - x[2] * y[1];
  res[1] = x[2] * y[0] - x[0] * y[2];
  res[2] = x[0] * y[1] - x[1] * y[0];
}
static guint32
gsk_sl_native_cross_VEC3VEC3_write_spv (GskSpvWriter *writer, guint32 *arguments, gpointer user_data)
{
  return gsk_spv_writer_cross (writer, GSK_SL_BUILTIN_TYPE_FLOAT, arguments[0], arguments[1]);
}

NATIVE1_FUNCS (FLOAT, normalize, FLOAT, float length = sqrtf (dotf (arguments[0], arguments[0], 1));, res = x / length;, )
NATIVE1_FUNCS (VEC2, normalize, VEC2, float length = sqrtf (dotf (arguments[0], arguments[0], 2));, res = x / length;, )
NATIVE1_FUNCS (VEC3, normalize, VEC3, float length = sqrtf (dotf (arguments[0], arguments[0], 3));, res = x / length;, )
NATIVE1_FUNCS (VEC4, normalize, VEC4, float length = sqrtf (dotf (arguments[0], arguments[0], 4));, res = x / length;, )
#define gsk_spv_writer_faceforward gsk_spv_writer_face_forward
NATIVE3_FUNCS (FLOAT, faceforward, FLOAT, FLOAT, FLOAT, float swap = dotf (arguments[1], arguments[2], 1);, res = swap < 0 ? x : -x;, )
NATIVE3_FUNCS (FLOAT, faceforward, VEC2, VEC2, VEC2, float swap = dotf (arguments[1], arguments[2], 2);, res = swap < 0 ? x : -x;, )
NATIVE3_FUNCS (FLOAT, faceforward, VEC3, VEC3, VEC3, float swap = dotf (arguments[1], arguments[2], 3);, res = swap < 0 ? x : -x;, )
NATIVE3_FUNCS (FLOAT, faceforward, VEC4, VEC4, VEC4, float swap = dotf (arguments[1], arguments[2], 4);, res = swap < 0 ? x : -x;, )
#undef gsk_spv_writer_faceforward
NATIVE2_FUNCS (FLOAT, reflect, FLOAT, FLOAT, float dot = 2 * dotf (arguments[1], arguments[2], 1);, res = x - dot * y;, )
NATIVE2_FUNCS (VEC2, reflect, VEC2, VEC2, float dot = 2 * dotf (arguments[1], arguments[2], 2);, res = x - dot * y;, )
NATIVE2_FUNCS (VEC3, reflect, VEC3, VEC3, float dot = 2 * dotf (arguments[1], arguments[2], 3);, res = x - dot * y;, )
NATIVE2_FUNCS (VEC4, reflect, VEC4, VEC4, float dot = 2 * dotf (arguments[1], arguments[2], 4);, res = x - dot * y;, )

#define REFRACT(TYPE) \
static void \
gsk_sl_native_refract_ ## TYPE ## TYPE ## FLOAT_get_constant (gpointer *retval, gpointer *arguments, gpointer user_data) \
{ \
  float *x = (float *) arguments[0]; \
  float *y = (float *) arguments[1]; \
  float eta = *((float *) arguments[2]); \
  float *res = (float *) retval; \
  gsize i; \
\
  float dot = 2 * dotf (x, y, TYPE_ ## TYPE ## _LEN); \
  float k = 1.0f - eta * eta * (1.0f - dot * dot); \
\
  for (i = 0; i < TYPE_ ## TYPE ## _LEN; i++) \
    { \
      if (k < 0.0) \
        res[i] = 0.0; \
      else \
        res[i] = eta * x[i] - (eta * dot + sqrt (k)) * y[i]; \
    } \
} \
\
static guint32 \
gsk_sl_native_refract_ ## TYPE ## TYPE ## FLOAT_write_spv (GskSpvWriter *writer, guint32 *arguments, gpointer user_data) \
{ \
  return gsk_spv_writer_refract (writer, GSK_SL_BUILTIN_TYPE_ ## TYPE, arguments[0], arguments[1], arguments[2]); \
}
REFRACT (FLOAT)
REFRACT (VEC2)
REFRACT (VEC3)
REFRACT (VEC4)

#define MATRIX_COMP_MULT(TYPE,cols,rows) \
static void \
gsk_sl_native_matrixCompMult_ ## TYPE ## TYPE ## _get_constant (gpointer *retval, gpointer *arguments, gpointer user_data) \
{ \
  float *x = (float *) arguments[0]; \
  float *y = (float *) arguments[1]; \
  float *res = (float *) retval; \
  gsize i; \
\
  for (i = 0; i < cols * rows; i++) \
    { \
      res[i] = x[i] * y[i]; \
    } \
} \
\
static guint32 \
gsk_sl_native_matrixCompMult_ ## TYPE ## TYPE ## _write_spv (GskSpvWriter *writer, guint32 *arguments, gpointer user_data) \
{ \
  guint32 components[cols], x, y; \
  gsize c; \
\
  for (c = 0; c < cols; c++)\
    { \
      x = gsk_spv_writer_composite_extract (writer, GSK_SL_BUILTIN_TYPE_VEC ## rows, arguments[0], (guint32[1]) { c }, 1); \
      y = gsk_spv_writer_composite_extract (writer, GSK_SL_BUILTIN_TYPE_VEC ## rows, arguments[1], (guint32[1]) { c }, 1); \
      components[c] = gsk_spv_writer_f_mul (writer, GSK_SL_BUILTIN_TYPE_VEC ## rows, x, y); \
    } \
\
  return gsk_spv_writer_composite_construct (writer, GSK_SL_BUILTIN_TYPE_ ## TYPE, components, cols); \
}

MATRIX_COMP_MULT(MAT2, 2, 2)
MATRIX_COMP_MULT(MAT2X3, 2, 3)
MATRIX_COMP_MULT(MAT2X4, 2, 4)
MATRIX_COMP_MULT(MAT3X2, 3, 2)
MATRIX_COMP_MULT(MAT3, 3, 3)
MATRIX_COMP_MULT(MAT3X4, 3, 4)
MATRIX_COMP_MULT(MAT4X2, 4, 2)
MATRIX_COMP_MULT(MAT4X3, 4, 3)
MATRIX_COMP_MULT(MAT4, 4, 4)

#define MATRIX_TRANSPOSE(TYPE, cols, rows) \
static void  \
gsk_sl_native_transpose_ ## TYPE ## _get_constant (gpointer *retval, gpointer *arguments, gpointer user_data) \
{ \
  float *x = (float *) arguments[0]; \
  float *res = (float *) retval; \
  gsize c, r; \
\
  for (c = 0; c < cols; c++) \
    for (r = 0; r < rows; r++) \
      res[r * cols + c] = x[c * rows + r]; \
} \
static guint32 \
gsk_sl_native_transpose_ ## TYPE ## _write_spv (GskSpvWriter *writer, guint32 *arguments, gpointer user_data) \
{ \
  return gsk_spv_writer_transpose (writer, gsk_sl_type_get_matrix (GSK_SL_FLOAT, rows, cols), arguments[0]); \
}

MATRIX_TRANSPOSE(MAT2, 2, 2)
MATRIX_TRANSPOSE(MAT2X3, 2, 3)
MATRIX_TRANSPOSE(MAT2X4, 2, 4)
MATRIX_TRANSPOSE(MAT3X2, 3, 2)
MATRIX_TRANSPOSE(MAT3, 3, 3)
MATRIX_TRANSPOSE(MAT3X4, 3, 4)
MATRIX_TRANSPOSE(MAT4X2, 4, 2)
MATRIX_TRANSPOSE(MAT4X3, 4, 3)
MATRIX_TRANSPOSE(MAT4, 4, 4)

#define MATRIX_OUTER_PRODUCT(cols, rows) \
static void  \
gsk_sl_native_outerProduct_ ## VEC ## cols ## VEC ## rows ## _get_constant (gpointer *retval, gpointer *arguments, gpointer user_data) \
{ \
  float *x = (float *) arguments[0]; \
  float *y = (float *) arguments[1]; \
  float *res = (float *) retval; \
  gsize c, r; \
\
  for (c = 0; c < cols; c++) \
    for (r = 0; r < rows; r++) \
      res[r * cols + c] = x[r] * y[c]; \
} \
static guint32 \
gsk_sl_native_outerProduct_ ## VEC ## cols ## VEC ## rows ## _write_spv (GskSpvWriter *writer, guint32 *arguments, gpointer user_data) \
{ \
  return gsk_spv_writer_outer_product (writer, gsk_sl_type_get_matrix (GSK_SL_FLOAT, cols, rows), arguments[0], arguments[1]); \
}
MATRIX_OUTER_PRODUCT (2, 2)
MATRIX_OUTER_PRODUCT (2, 3)
MATRIX_OUTER_PRODUCT (2, 4)
MATRIX_OUTER_PRODUCT (3, 2)
MATRIX_OUTER_PRODUCT (3, 3)
MATRIX_OUTER_PRODUCT (3, 4)
MATRIX_OUTER_PRODUCT (4, 2)
MATRIX_OUTER_PRODUCT (4, 3)
MATRIX_OUTER_PRODUCT (4, 4)

#define gsk_spv_writer_lessThan gsk_spv_writer_f_ord_less_than
NATIVE2_FUNCS (BVEC2, lessThan, VEC2, VEC2, , res = x < y;, )
NATIVE2_FUNCS (BVEC3, lessThan, VEC3, VEC3, , res = x < y;, )
NATIVE2_FUNCS (BVEC4, lessThan, VEC4, VEC4, , res = x < y;, )
#undef gsk_spv_writer_lessThan
#define gsk_spv_writer_lessThan gsk_spv_writer_s_less_than
NATIVE2_FUNCS (BVEC2, lessThan, IVEC2, IVEC2, , res = x < y;, )
NATIVE2_FUNCS (BVEC3, lessThan, IVEC3, IVEC3, , res = x < y;, )
NATIVE2_FUNCS (BVEC4, lessThan, IVEC4, IVEC4, , res = x < y;, )
#undef gsk_spv_writer_lessThan
#define gsk_spv_writer_lessThan gsk_spv_writer_u_less_than
NATIVE2_FUNCS (BVEC2, lessThan, UVEC2, UVEC2, , res = x < y;, )
NATIVE2_FUNCS (BVEC3, lessThan, UVEC3, UVEC3, , res = x < y;, )
NATIVE2_FUNCS (BVEC4, lessThan, UVEC4, UVEC4, , res = x < y;, )
#undef gsk_spv_writer_lessThan
#define gsk_spv_writer_lessThanEqual gsk_spv_writer_f_ord_less_than_equal
NATIVE2_FUNCS (BVEC2, lessThanEqual, VEC2, VEC2, , res = x <= y;, )
NATIVE2_FUNCS (BVEC3, lessThanEqual, VEC3, VEC3, , res = x <= y;, )
NATIVE2_FUNCS (BVEC4, lessThanEqual, VEC4, VEC4, , res = x <= y;, )
#undef gsk_spv_writer_lessThanEqual
#define gsk_spv_writer_lessThanEqual gsk_spv_writer_s_less_than_equal
NATIVE2_FUNCS (BVEC2, lessThanEqual, IVEC2, IVEC2, , res = x <= y;, )
NATIVE2_FUNCS (BVEC3, lessThanEqual, IVEC3, IVEC3, , res = x <= y;, )
NATIVE2_FUNCS (BVEC4, lessThanEqual, IVEC4, IVEC4, , res = x <= y;, )
#undef gsk_spv_writer_lessThanEqual
#define gsk_spv_writer_lessThanEqual gsk_spv_writer_u_less_than_equal
NATIVE2_FUNCS (BVEC2, lessThanEqual, UVEC2, UVEC2, , res = x <= y;, )
NATIVE2_FUNCS (BVEC3, lessThanEqual, UVEC3, UVEC3, , res = x <= y;, )
NATIVE2_FUNCS (BVEC4, lessThanEqual, UVEC4, UVEC4, , res = x <= y;, )
#undef gsk_spv_writer_lessThanEqual
#define gsk_spv_writer_greaterThan gsk_spv_writer_f_ord_greater_than
NATIVE2_FUNCS (BVEC2, greaterThan, VEC2, VEC2, , res = x > y;, )
NATIVE2_FUNCS (BVEC3, greaterThan, VEC3, VEC3, , res = x > y;, )
NATIVE2_FUNCS (BVEC4, greaterThan, VEC4, VEC4, , res = x > y;, )
#undef gsk_spv_writer_greaterThan
#define gsk_spv_writer_greaterThan gsk_spv_writer_s_greater_than
NATIVE2_FUNCS (BVEC2, greaterThan, IVEC2, IVEC2, , res = x > y;, )
NATIVE2_FUNCS (BVEC3, greaterThan, IVEC3, IVEC3, , res = x > y;, )
NATIVE2_FUNCS (BVEC4, greaterThan, IVEC4, IVEC4, , res = x > y;, )
#undef gsk_spv_writer_greaterThan
#define gsk_spv_writer_greaterThan gsk_spv_writer_u_greater_than
NATIVE2_FUNCS (BVEC2, greaterThan, UVEC2, UVEC2, , res = x > y;, )
NATIVE2_FUNCS (BVEC3, greaterThan, UVEC3, UVEC3, , res = x > y;, )
NATIVE2_FUNCS (BVEC4, greaterThan, UVEC4, UVEC4, , res = x > y;, )
#undef gsk_spv_writer_greaterThan
#define gsk_spv_writer_greaterThanEqual gsk_spv_writer_f_ord_greater_than_equal
NATIVE2_FUNCS (BVEC2, greaterThanEqual, VEC2, VEC2, , res = x >= y;, )
NATIVE2_FUNCS (BVEC3, greaterThanEqual, VEC3, VEC3, , res = x >= y;, )
NATIVE2_FUNCS (BVEC4, greaterThanEqual, VEC4, VEC4, , res = x >= y;, )
#undef gsk_spv_writer_greaterThanEqual
#define gsk_spv_writer_greaterThanEqual gsk_spv_writer_s_greater_than_equal
NATIVE2_FUNCS (BVEC2, greaterThanEqual, IVEC2, IVEC2, , res = x >= y;, )
NATIVE2_FUNCS (BVEC3, greaterThanEqual, IVEC3, IVEC3, , res = x >= y;, )
NATIVE2_FUNCS (BVEC4, greaterThanEqual, IVEC4, IVEC4, , res = x >= y;, )
#undef gsk_spv_writer_greaterThanEqual
#define gsk_spv_writer_greaterThanEqual gsk_spv_writer_u_greater_than_equal
NATIVE2_FUNCS (BVEC2, greaterThanEqual, UVEC2, UVEC2, , res = x >= y;, )
NATIVE2_FUNCS (BVEC3, greaterThanEqual, UVEC3, UVEC3, , res = x >= y;, )
NATIVE2_FUNCS (BVEC4, greaterThanEqual, UVEC4, UVEC4, , res = x >= y;, )
#undef gsk_spv_writer_greaterThanEqual
#define gsk_spv_writer_equal gsk_spv_writer_f_ord_equal
NATIVE2_FUNCS (BVEC2, equal, VEC2, VEC2, , res = x == y;, )
NATIVE2_FUNCS (BVEC3, equal, VEC3, VEC3, , res = x == y;, )
NATIVE2_FUNCS (BVEC4, equal, VEC4, VEC4, , res = x == y;, )
#undef gsk_spv_writer_equal
#define gsk_spv_writer_equal gsk_spv_writer_i_equal
NATIVE2_FUNCS (BVEC2, equal, IVEC2, IVEC2, , res = x == y;, )
NATIVE2_FUNCS (BVEC3, equal, IVEC3, IVEC3, , res = x == y;, )
NATIVE2_FUNCS (BVEC4, equal, IVEC4, IVEC4, , res = x == y;, )
NATIVE2_FUNCS (BVEC2, equal, UVEC2, UVEC2, , res = x == y;, )
NATIVE2_FUNCS (BVEC3, equal, UVEC3, UVEC3, , res = x == y;, )
NATIVE2_FUNCS (BVEC4, equal, UVEC4, UVEC4, , res = x == y;, )
#undef gsk_spv_writer_equal
#define gsk_spv_writer_equal gsk_spv_writer_logical_equal
NATIVE2_FUNCS (BVEC2, equal, BVEC2, BVEC2, , res = x == y;, )
NATIVE2_FUNCS (BVEC3, equal, BVEC3, BVEC3, , res = x == y;, )
NATIVE2_FUNCS (BVEC4, equal, BVEC4, BVEC4, , res = x == y;, )
#undef gsk_spv_writer_equal
#define gsk_spv_writer_notEqual gsk_spv_writer_f_ord_not_equal
NATIVE2_FUNCS (BVEC2, notEqual, VEC2, VEC2, , res = x != y;, )
NATIVE2_FUNCS (BVEC3, notEqual, VEC3, VEC3, , res = x != y;, )
NATIVE2_FUNCS (BVEC4, notEqual, VEC4, VEC4, , res = x != y;, )
#undef gsk_spv_writer_notEqual
#define gsk_spv_writer_notEqual gsk_spv_writer_i_not_equal
NATIVE2_FUNCS (BVEC2, notEqual, IVEC2, IVEC2, , res = x != y;, )
NATIVE2_FUNCS (BVEC3, notEqual, IVEC3, IVEC3, , res = x != y;, )
NATIVE2_FUNCS (BVEC4, notEqual, IVEC4, IVEC4, , res = x != y;, )
NATIVE2_FUNCS (BVEC2, notEqual, UVEC2, UVEC2, , res = x != y;, )
NATIVE2_FUNCS (BVEC3, notEqual, UVEC3, UVEC3, , res = x != y;, )
NATIVE2_FUNCS (BVEC4, notEqual, UVEC4, UVEC4, , res = x != y;, )
#undef gsk_spv_writer_notEqual
#define gsk_spv_writer_notEqual gsk_spv_writer_logical_not_equal
NATIVE2_FUNCS (BVEC2, notEqual, BVEC2, BVEC2, , res = x != y;, )
NATIVE2_FUNCS (BVEC3, notEqual, BVEC3, BVEC3, , res = x != y;, )
NATIVE2_FUNCS (BVEC4, notEqual, BVEC4, BVEC4, , res = x != y;, )
#undef gsk_spv_writer_notEqual
NATIVE1_FUNCS (BOOL, any, BVEC2, , res |= x;, )
NATIVE1_FUNCS (BOOL, any, BVEC3, , res |= x;, )
NATIVE1_FUNCS (BOOL, any, BVEC4, , res |= x;, )
NATIVE1_FUNCS (BOOL, all, BVEC2, res = TRUE;, res &= x;, )
NATIVE1_FUNCS (BOOL, all, BVEC3, res = TRUE;, res &= x;, )
NATIVE1_FUNCS (BOOL, all, BVEC4, res = TRUE;, res &= x;, )
NATIVE1_FUNCS (BVEC2, not, BVEC2, , res = !x;, )
NATIVE1_FUNCS (BVEC3, not, BVEC3, , res = !x;, )
NATIVE1_FUNCS (BVEC4, not, BVEC4, , res = !x;, )

NATIVE1_FUNCS (FLOAT, sinh, FLOAT, , res = sinhf (x);, );
NATIVE1_FUNCS (VEC2, sinh, VEC2, , res = sinhf (x);, );
NATIVE1_FUNCS (VEC3, sinh, VEC3, , res = sinhf (x);, );
NATIVE1_FUNCS (VEC4, sinh, VEC4, , res = sinhf (x);, );
NATIVE1_FUNCS (FLOAT, cosh, FLOAT, , res = coshf (x);, );
NATIVE1_FUNCS (VEC2, cosh, VEC2, , res = coshf (x);, );
NATIVE1_FUNCS (VEC3, cosh, VEC3, , res = coshf (x);, );
NATIVE1_FUNCS (VEC4, cosh, VEC4, , res = coshf (x);, );
NATIVE1_FUNCS (FLOAT, tanh, FLOAT, , res = tanhf (x);, );
NATIVE1_FUNCS (VEC2, tanh, VEC2, , res = tanhf (x);, );
NATIVE1_FUNCS (VEC3, tanh, VEC3, , res = tanhf (x);, );
NATIVE1_FUNCS (VEC4, tanh, VEC4, , res = tanhf (x);, );
NATIVE1_FUNCS (FLOAT, asinh, FLOAT, , res = asinhf (x);, );
NATIVE1_FUNCS (VEC2, asinh, VEC2, , res = asinhf (x);, );
NATIVE1_FUNCS (VEC3, asinh, VEC3, , res = asinhf (x);, );
NATIVE1_FUNCS (VEC4, asinh, VEC4, , res = asinhf (x);, );
NATIVE1_FUNCS (FLOAT, acosh, FLOAT, , res = acoshf (x);, );
NATIVE1_FUNCS (VEC2, acosh, VEC2, , res = acoshf (x);, );
NATIVE1_FUNCS (VEC3, acosh, VEC3, , res = acoshf (x);, );
NATIVE1_FUNCS (VEC4, acosh, VEC4, , res = acoshf (x);, );
NATIVE1_FUNCS (FLOAT, atanh, FLOAT, , res = atanhf (x);, );
NATIVE1_FUNCS (VEC2, atanh, VEC2, , res = atanhf (x);, );
NATIVE1_FUNCS (VEC3, atanh, VEC3, , res = atanhf (x);, );
NATIVE1_FUNCS (VEC4, atanh, VEC4, , res = atanhf (x);, );

static guint32
gsk_sl_native_functions_write_spv_unimplemented (GskSpvWriter *writer,
                                                 guint32      *arguments,
                                                 gpointer      user_data)
{
  g_assert_not_reached ();

  return 0;
}

static void
gsk_sl_native_functions_add_one (GskSlScope *scope,
                                 GskSlType  *return_type,
                                 const char *name,
                                 GskSlType **arguments,
                                 gsize       n_arguments,
                                 void        (* get_constant) (gpointer *retval, gpointer *arguments, gpointer user_data),
                                 guint32     (* write_spv) (GskSpvWriter *writer, guint32 *arguments, gpointer user_data))
{
  GskSlFunctionType *ftype;
  GskSlFunction *function;
  gsize i;

  ftype = gsk_sl_function_type_new (return_type);
  for (i = 0; i < n_arguments; i++)
    {
      ftype = gsk_sl_function_type_add_argument (ftype,
                                                 GSK_SL_STORAGE_PARAMETER_IN,
                                                 arguments[i]);
    }

  function = gsk_sl_function_new_native (name,
                                         ftype,
                                         get_constant,
                                         write_spv,
                                         NULL,
                                         NULL);
  gsk_sl_scope_add_function (scope, function);

  gsk_sl_function_unref (function);
  gsk_sl_function_type_unref (ftype);
}

#define NATIVE1(type, name, arg1) \
  gsk_sl_native_functions_add_one (scope, GSK_SL_BUILTIN_TYPE_ ## type, G_STRINGIFY (name), \
                                   (GskSlType *[1]) { GSK_SL_BUILTIN_TYPE_ ## arg1 }, 1, \
                                   gsk_sl_native_ ## name ## _ ## arg1 ## _get_constant, \
                                   gsk_sl_native_ ## name ## _ ## arg1 ## _write_spv)
#define NATIVE2(type, name, arg1, arg2) \
  gsk_sl_native_functions_add_one (scope, GSK_SL_BUILTIN_TYPE_ ## type, G_STRINGIFY (name), \
                                   (GskSlType *[2]) { GSK_SL_BUILTIN_TYPE_ ## arg1, GSK_SL_BUILTIN_TYPE_ ## arg2 }, 2, \
                                   gsk_sl_native_ ## name ## _ ## arg1 ## arg2 ## _get_constant, \
                                   gsk_sl_native_ ## name ## _ ## arg1 ## arg2 ## _write_spv)
#define NATIVE3(type, name, arg1, arg2, arg3) \
  gsk_sl_native_functions_add_one (scope, GSK_SL_BUILTIN_TYPE_ ## type, G_STRINGIFY (name), \
                                   (GskSlType *[3]) { GSK_SL_BUILTIN_TYPE_ ## arg1, GSK_SL_BUILTIN_TYPE_ ## arg2, GSK_SL_BUILTIN_TYPE_ ## arg3 }, 3, \
                                   gsk_sl_native_ ## name ## _ ## arg1 ## arg2 ## arg3 ##_get_constant, \
                                   gsk_sl_native_ ## name ## _ ## arg1 ## arg2 ## arg3 ## _write_spv)
#define UNIMPLEMENTED_NATIVE1(type, name, arg1) \
  gsk_sl_native_functions_add_one (scope, GSK_SL_BUILTIN_TYPE_ ## type, G_STRINGIFY (name), (GskSlType *[1]) { GSK_SL_BUILTIN_TYPE_ ## arg1 }, 1, NULL, gsk_sl_native_functions_write_spv_unimplemented)
#define UNIMPLEMENTED_NATIVE2(type, name, arg1, arg2) \
  gsk_sl_native_functions_add_one (scope, GSK_SL_BUILTIN_TYPE_ ## type, G_STRINGIFY (name), (GskSlType *[2]) { GSK_SL_BUILTIN_TYPE_ ## arg1, GSK_SL_BUILTIN_TYPE_ ## arg2 }, 2, NULL, gsk_sl_native_functions_write_spv_unimplemented)
#define UNIMPLEMENTED_NATIVE3(type, name, arg1, arg2, arg3) \
  gsk_sl_native_functions_add_one (scope, GSK_SL_BUILTIN_TYPE_ ## type, G_STRINGIFY (name), (GskSlType *[3]) { GSK_SL_BUILTIN_TYPE_ ## arg1, GSK_SL_BUILTIN_TYPE_ ## arg2, GSK_SL_BUILTIN_TYPE_ ## arg3 }, 3, NULL, gsk_sl_native_functions_write_spv_unimplemented)

static void
gsk_sl_native_functions_add_100 (GskSlScope       *scope,
                                 GskSlEnvironment *environment)
{
  NATIVE1 (FLOAT, radians, FLOAT);
  NATIVE1 (VEC2, radians, VEC2);
  NATIVE1 (VEC3, radians, VEC3);
  NATIVE1 (VEC4, radians, VEC4);
  NATIVE1 (FLOAT, degrees, FLOAT);
  NATIVE1 (VEC2, degrees, VEC2);
  NATIVE1 (VEC3, degrees, VEC3);
  NATIVE1 (VEC4, degrees, VEC4);
  NATIVE1 (FLOAT, sin, FLOAT);
  NATIVE1 (VEC2, sin, VEC2);
  NATIVE1 (VEC3, sin, VEC3);
  NATIVE1 (VEC4, sin, VEC4);
  NATIVE1 (FLOAT, cos, FLOAT);
  NATIVE1 (VEC2, cos, VEC2);
  NATIVE1 (VEC3, cos, VEC3);
  NATIVE1 (VEC4, cos, VEC4);
  NATIVE1 (FLOAT, tan, FLOAT);
  NATIVE1 (VEC2, tan, VEC2);
  NATIVE1 (VEC3, tan, VEC3);
  NATIVE1 (VEC4, tan, VEC4);
  NATIVE1 (FLOAT, asin, FLOAT);
  NATIVE1 (VEC2, asin, VEC2);
  NATIVE1 (VEC3, asin, VEC3);
  NATIVE1 (VEC4, asin, VEC4);
  NATIVE1 (FLOAT, acos, FLOAT);
  NATIVE1 (VEC2, acos, VEC2);
  NATIVE1 (VEC3, acos, VEC3);
  NATIVE1 (VEC4, acos, VEC4);
  NATIVE2 (FLOAT, atan, FLOAT, FLOAT);
  NATIVE2 (VEC2, atan, VEC2, VEC2);
  NATIVE2 (VEC3, atan, VEC3, VEC3);
  NATIVE2 (VEC4, atan, VEC4, VEC4);
  NATIVE1 (FLOAT, atan, FLOAT);
  NATIVE1 (VEC2, atan, VEC2);
  NATIVE1 (VEC3, atan, VEC3);
  NATIVE1 (VEC4, atan, VEC4);

  NATIVE2 (FLOAT, pow, FLOAT, FLOAT);
  NATIVE2 (VEC2, pow, VEC2, VEC2);
  NATIVE2 (VEC3, pow, VEC3, VEC3);
  NATIVE2 (VEC4, pow, VEC4, VEC4);
  NATIVE1 (FLOAT, exp, FLOAT);
  NATIVE1 (VEC2, exp, VEC2);
  NATIVE1 (VEC3, exp, VEC3);
  NATIVE1 (VEC4, exp, VEC4);
  NATIVE1 (FLOAT, log, FLOAT);
  NATIVE1 (VEC2, log, VEC2);
  NATIVE1 (VEC3, log, VEC3);
  NATIVE1 (VEC4, log, VEC4);
  NATIVE1 (FLOAT, exp2, FLOAT);
  NATIVE1 (VEC2, exp2, VEC2);
  NATIVE1 (VEC3, exp2, VEC3);
  NATIVE1 (VEC4, exp2, VEC4);
  NATIVE1 (FLOAT, log2, FLOAT);
  NATIVE1 (VEC2, log2, VEC2);
  NATIVE1 (VEC3, log2, VEC3);
  NATIVE1 (VEC4, log2, VEC4);
  NATIVE1 (FLOAT, sqrt, FLOAT);
  NATIVE1 (VEC2, sqrt, VEC2);
  NATIVE1 (VEC3, sqrt, VEC3);
  NATIVE1 (VEC4, sqrt, VEC4);
  NATIVE1 (FLOAT, inversesqrt, FLOAT);
  NATIVE1 (VEC2, inversesqrt, VEC2);
  NATIVE1 (VEC3, inversesqrt, VEC3);
  NATIVE1 (VEC4, inversesqrt, VEC4);

  NATIVE1 (FLOAT, abs, FLOAT);
  NATIVE1 (VEC2, abs, VEC2);
  NATIVE1 (VEC3, abs, VEC3);
  NATIVE1 (VEC4, abs, VEC4);
  NATIVE1 (FLOAT, sign, FLOAT);
  NATIVE1 (VEC2, sign, VEC2);
  NATIVE1 (VEC3, sign, VEC3);
  NATIVE1 (VEC4, sign, VEC4);
  NATIVE1 (FLOAT, floor, FLOAT);
  NATIVE1 (VEC2, floor, VEC2);
  NATIVE1 (VEC3, floor, VEC3);
  NATIVE1 (VEC4, floor, VEC4);
  NATIVE1 (FLOAT, ceil, FLOAT);
  NATIVE1 (VEC2, ceil, VEC2);
  NATIVE1 (VEC3, ceil, VEC3);
  NATIVE1 (VEC4, ceil, VEC4);
  NATIVE1 (FLOAT, fract, FLOAT);
  NATIVE1 (VEC2, fract, VEC2);
  NATIVE1 (VEC3, fract, VEC3);
  NATIVE1 (VEC4, fract, VEC4);
  NATIVE2 (FLOAT, mod, FLOAT, FLOAT);
  NATIVE2 (VEC2, mod, VEC2, FLOAT);
  NATIVE2 (VEC3, mod, VEC3, FLOAT);
  NATIVE2 (VEC4, mod, VEC4, FLOAT);
  NATIVE2 (VEC2, mod, VEC2, VEC2);
  NATIVE2 (VEC3, mod, VEC3, VEC3);
  NATIVE2 (VEC4, mod, VEC4, VEC4);
  NATIVE2 (FLOAT, min, FLOAT, FLOAT);
  NATIVE2 (VEC2, min, VEC2, FLOAT);
  NATIVE2 (VEC3, min, VEC3, FLOAT);
  NATIVE2 (VEC4, min, VEC4, FLOAT);
  NATIVE2 (VEC2, min, VEC2, VEC2);
  NATIVE2 (VEC3, min, VEC3, VEC3);
  NATIVE2 (VEC4, min, VEC4, VEC4);
  NATIVE2 (FLOAT, max, FLOAT, FLOAT);
  NATIVE2 (VEC2, max, VEC2, FLOAT);
  NATIVE2 (VEC3, max, VEC3, FLOAT);
  NATIVE2 (VEC4, max, VEC4, FLOAT);
  NATIVE2 (VEC2, max, VEC2, VEC2);
  NATIVE2 (VEC3, max, VEC3, VEC3);
  NATIVE2 (VEC4, max, VEC4, VEC4);
  NATIVE3 (FLOAT, clamp, FLOAT, FLOAT, FLOAT);
  NATIVE3 (VEC2, clamp, VEC2, FLOAT, FLOAT);
  NATIVE3 (VEC3, clamp, VEC3, FLOAT, FLOAT);
  NATIVE3 (VEC4, clamp, VEC4, FLOAT, FLOAT);
  NATIVE3 (VEC2, clamp, VEC2, VEC2, VEC2);
  NATIVE3 (VEC3, clamp, VEC3, VEC3, VEC3);
  NATIVE3 (VEC4, clamp, VEC4, VEC4, VEC4);
  NATIVE3 (FLOAT, mix, FLOAT, FLOAT, FLOAT);
  NATIVE3 (VEC2, mix, VEC2, VEC2, FLOAT);
  NATIVE3 (VEC3, mix, VEC3, VEC3, FLOAT);
  NATIVE3 (VEC4, mix, VEC4, VEC4, FLOAT);
  NATIVE3 (VEC2, mix, VEC2, VEC2, VEC2);
  NATIVE3 (VEC3, mix, VEC3, VEC3, VEC3);
  NATIVE3 (VEC4, mix, VEC4, VEC4, VEC4);
  NATIVE2 (FLOAT, step, FLOAT, FLOAT);
  NATIVE2 (VEC2, step, FLOAT, VEC2);
  NATIVE2 (VEC3, step, FLOAT, VEC3);
  NATIVE2 (VEC4, step, FLOAT, VEC4);
  NATIVE2 (VEC2, step, VEC2, VEC2);
  NATIVE2 (VEC3, step, VEC3, VEC3);
  NATIVE2 (VEC4, step, VEC4, VEC4);
  NATIVE3 (FLOAT, smoothstep, FLOAT, FLOAT, FLOAT);
  NATIVE3 (VEC2, smoothstep, VEC2, VEC2, FLOAT);
  NATIVE3 (VEC3, smoothstep, VEC3, VEC3, FLOAT);
  NATIVE3 (VEC4, smoothstep, VEC4, VEC4, FLOAT);
  NATIVE3 (VEC2, smoothstep, VEC2, VEC2, VEC2);
  NATIVE3 (VEC3, smoothstep, VEC3, VEC3, VEC3);
  NATIVE3 (VEC4, smoothstep, VEC4, VEC4, VEC4);

  NATIVE1 (FLOAT, length, FLOAT);
  NATIVE1 (FLOAT, length, VEC2);
  NATIVE1 (FLOAT, length, VEC3);
  NATIVE1 (FLOAT, length, VEC4);
  NATIVE2 (FLOAT, distance, FLOAT, FLOAT);
  NATIVE2 (FLOAT, distance, VEC2, VEC2);
  NATIVE2 (FLOAT, distance, VEC3, VEC3);
  NATIVE2 (FLOAT, distance, VEC4, VEC4);
  NATIVE2 (FLOAT, dot, FLOAT, FLOAT);
  NATIVE2 (FLOAT, dot, VEC2, VEC2);
  NATIVE2 (FLOAT, dot, VEC3, VEC3);
  NATIVE2 (FLOAT, dot, VEC4, VEC4);
  NATIVE2 (VEC3, cross, VEC3, VEC3);
  NATIVE1 (FLOAT, normalize, FLOAT);
  NATIVE1 (VEC2, normalize, VEC2);
  NATIVE1 (VEC3, normalize, VEC3);
  NATIVE1 (VEC4, normalize, VEC4);
  NATIVE3 (FLOAT, faceforward, FLOAT, FLOAT, FLOAT);
  NATIVE3 (FLOAT, faceforward, VEC2, VEC2, VEC2);
  NATIVE3 (FLOAT, faceforward, VEC3, VEC3, VEC3);
  NATIVE3 (FLOAT, faceforward, VEC4, VEC4, VEC4);
  NATIVE2 (FLOAT, reflect, FLOAT, FLOAT);
  NATIVE2 (VEC2, reflect, VEC2, VEC2);
  NATIVE2 (VEC3, reflect, VEC3, VEC3);
  NATIVE2 (VEC4, reflect, VEC4, VEC4);
  NATIVE3 (FLOAT, refract, FLOAT, FLOAT, FLOAT);
  NATIVE3 (VEC2, refract, VEC2, VEC2, FLOAT);
  NATIVE3 (VEC3, refract, VEC3, VEC3, FLOAT);
  NATIVE3 (VEC4, refract, VEC4, VEC4, FLOAT);

  NATIVE2 (MAT2, matrixCompMult, MAT2, MAT2);
  NATIVE2 (MAT3, matrixCompMult, MAT3, MAT3);
  NATIVE2 (MAT4, matrixCompMult, MAT4, MAT4);

  NATIVE2 (BVEC2, lessThan, VEC2, VEC2);
  NATIVE2 (BVEC3, lessThan, VEC3, VEC3);
  NATIVE2 (BVEC4, lessThan, VEC4, VEC4);
  NATIVE2 (BVEC2, lessThan, IVEC2, IVEC2);
  NATIVE2 (BVEC3, lessThan, IVEC3, IVEC3);
  NATIVE2 (BVEC4, lessThan, IVEC4, IVEC4);
  NATIVE2 (BVEC2, lessThanEqual, VEC2, VEC2);
  NATIVE2 (BVEC3, lessThanEqual, VEC3, VEC3);
  NATIVE2 (BVEC4, lessThanEqual, VEC4, VEC4);
  NATIVE2 (BVEC2, lessThanEqual, IVEC2, IVEC2);
  NATIVE2 (BVEC3, lessThanEqual, IVEC3, IVEC3);
  NATIVE2 (BVEC4, lessThanEqual, IVEC4, IVEC4);
  NATIVE2 (BVEC2, greaterThan, VEC2, VEC2);
  NATIVE2 (BVEC3, greaterThan, VEC3, VEC3);
  NATIVE2 (BVEC4, greaterThan, VEC4, VEC4);
  NATIVE2 (BVEC2, greaterThan, IVEC2, IVEC2);
  NATIVE2 (BVEC3, greaterThan, IVEC3, IVEC3);
  NATIVE2 (BVEC4, greaterThan, IVEC4, IVEC4);
  NATIVE2 (BVEC2, greaterThanEqual, VEC2, VEC2);
  NATIVE2 (BVEC3, greaterThanEqual, VEC3, VEC3);
  NATIVE2 (BVEC4, greaterThanEqual, VEC4, VEC4);
  NATIVE2 (BVEC2, greaterThanEqual, IVEC2, IVEC2);
  NATIVE2 (BVEC3, greaterThanEqual, IVEC3, IVEC3);
  NATIVE2 (BVEC4, greaterThanEqual, IVEC4, IVEC4);
  NATIVE2 (BVEC2, equal, VEC2, VEC2);
  NATIVE2 (BVEC3, equal, VEC3, VEC3);
  NATIVE2 (BVEC4, equal, VEC4, VEC4);
  NATIVE2 (BVEC2, equal, IVEC2, IVEC2);
  NATIVE2 (BVEC3, equal, IVEC3, IVEC3);
  NATIVE2 (BVEC4, equal, IVEC4, IVEC4);
  NATIVE2 (BVEC2, equal, BVEC2, BVEC2);
  NATIVE2 (BVEC3, equal, BVEC3, BVEC3);
  NATIVE2 (BVEC4, equal, BVEC4, BVEC4);
  NATIVE2 (BVEC2, notEqual, VEC2, VEC2);
  NATIVE2 (BVEC3, notEqual, VEC3, VEC3);
  NATIVE2 (BVEC4, notEqual, VEC4, VEC4);
  NATIVE2 (BVEC2, notEqual, IVEC2, IVEC2);
  NATIVE2 (BVEC3, notEqual, IVEC3, IVEC3);
  NATIVE2 (BVEC4, notEqual, IVEC4, IVEC4);
  NATIVE2 (BVEC2, notEqual, BVEC2, BVEC2);
  NATIVE2 (BVEC3, notEqual, BVEC3, BVEC3);
  NATIVE2 (BVEC4, notEqual, BVEC4, BVEC4);
  NATIVE1 (BOOL, any, BVEC2);
  NATIVE1 (BOOL, any, BVEC3);
  NATIVE1 (BOOL, any, BVEC4);
  NATIVE1 (BOOL, all, BVEC2);
  NATIVE1 (BOOL, all, BVEC3);
  NATIVE1 (BOOL, all, BVEC4);
  NATIVE1 (BVEC2, not, BVEC2);
  NATIVE1 (BVEC3, not, BVEC3);
  NATIVE1 (BVEC4, not, BVEC4);
}

static void
gsk_sl_native_functions_add_120 (GskSlScope       *scope,
                                 GskSlEnvironment *environment)
{
  NATIVE2 (MAT2, outerProduct, VEC2, VEC2);
  NATIVE2 (MAT3, outerProduct, VEC3, VEC3);
  NATIVE2 (MAT4, outerProduct, VEC4, VEC4);
  NATIVE2 (MAT2X3, outerProduct, VEC3, VEC2);
  NATIVE2 (MAT2X4, outerProduct, VEC4, VEC2);
  NATIVE2 (MAT3X2, outerProduct, VEC2, VEC3);
  NATIVE2 (MAT3X4, outerProduct, VEC4, VEC3);
  NATIVE2 (MAT4X2, outerProduct, VEC2, VEC4);
  NATIVE2 (MAT4X3, outerProduct, VEC3, VEC4);
  NATIVE1 (MAT2, transpose, MAT2);
  NATIVE1 (MAT3, transpose, MAT3);
  NATIVE1 (MAT4, transpose, MAT4);
  NATIVE1 (MAT2X3, transpose, MAT3X2);
  NATIVE1 (MAT2X4, transpose, MAT4X2);
  NATIVE1 (MAT3X2, transpose, MAT2X3);
  NATIVE1 (MAT3X4, transpose, MAT4X3);
  NATIVE1 (MAT4X2, transpose, MAT2X4);
  NATIVE1 (MAT4X3, transpose, MAT3X4);
  NATIVE2 (MAT2X3, matrixCompMult, MAT2X3, MAT2X3);
  NATIVE2 (MAT2X4, matrixCompMult, MAT2X4, MAT2X4);
  NATIVE2 (MAT3X2, matrixCompMult, MAT3X2, MAT3X2);
  NATIVE2 (MAT3X4, matrixCompMult, MAT3X4, MAT3X4);
  NATIVE2 (MAT4X2, matrixCompMult, MAT4X2, MAT4X2);
  NATIVE2 (MAT4X3, matrixCompMult, MAT4X3, MAT4X3);
}

static void
gsk_sl_native_functions_add_130 (GskSlScope       *scope,
                                 GskSlEnvironment *environment)
{
  NATIVE1 (FLOAT, sinh, FLOAT);
  NATIVE1 (VEC2, sinh, VEC2);
  NATIVE1 (VEC3, sinh, VEC3);
  NATIVE1 (VEC4, sinh, VEC4);
  NATIVE1 (FLOAT, cosh, FLOAT);
  NATIVE1 (VEC2, cosh, VEC2);
  NATIVE1 (VEC3, cosh, VEC3);
  NATIVE1 (VEC4, cosh, VEC4);
  NATIVE1 (FLOAT, tanh, FLOAT);
  NATIVE1 (VEC2, tanh, VEC2);
  NATIVE1 (VEC3, tanh, VEC3);
  NATIVE1 (VEC4, tanh, VEC4);
  NATIVE1 (FLOAT, asinh, FLOAT);
  NATIVE1 (VEC2, asinh, VEC2);
  NATIVE1 (VEC3, asinh, VEC3);
  NATIVE1 (VEC4, asinh, VEC4);
  NATIVE1 (FLOAT, acosh, FLOAT);
  NATIVE1 (VEC2, acosh, VEC2);
  NATIVE1 (VEC3, acosh, VEC3);
  NATIVE1 (VEC4, acosh, VEC4);
  NATIVE1 (FLOAT, atanh, FLOAT);
  NATIVE1 (VEC2, atanh, VEC2);
  NATIVE1 (VEC3, atanh, VEC3);
  NATIVE1 (VEC4, atanh, VEC4);

  NATIVE1 (INT, abs, INT);
  NATIVE1 (IVEC2, abs, IVEC2);
  NATIVE1 (IVEC3, abs, IVEC3);
  NATIVE1 (IVEC4, abs, IVEC4);
  NATIVE1 (INT, sign, INT);
  NATIVE1 (IVEC2, sign, IVEC2);
  NATIVE1 (IVEC3, sign, IVEC3);
  NATIVE1 (IVEC4, sign, IVEC4);
  NATIVE1 (FLOAT, trunc, FLOAT);
  NATIVE1 (VEC2, trunc, VEC2);
  NATIVE1 (VEC3, trunc, VEC3);
  NATIVE1 (VEC4, trunc, VEC4);
  NATIVE1 (FLOAT, round, FLOAT);
  NATIVE1 (VEC2, round, VEC2);
  NATIVE1 (VEC3, round, VEC3);
  NATIVE1 (VEC4, round, VEC4);
  NATIVE1 (FLOAT, roundEven, FLOAT);
  NATIVE1 (VEC2, roundEven, VEC2);
  NATIVE1 (VEC3, roundEven, VEC3);
  NATIVE1 (VEC4, roundEven, VEC4);
  UNIMPLEMENTED_NATIVE2 (FLOAT, modf, FLOAT, FLOAT); //OUT!
  UNIMPLEMENTED_NATIVE2 (VEC2, modf, VEC2, VEC2); //OUT!
  UNIMPLEMENTED_NATIVE2 (VEC3, modf, VEC3, VEC3); //OUT!
  UNIMPLEMENTED_NATIVE2 (VEC4, modf, VEC4, VEC4); //OUT!
  NATIVE2 (INT, min, INT, INT);
  NATIVE2 (IVEC2, min, IVEC2, INT);
  NATIVE2 (IVEC3, min, IVEC3, INT);
  NATIVE2 (IVEC4, min, IVEC4, INT);
  NATIVE2 (IVEC2, min, IVEC2, IVEC2);
  NATIVE2 (IVEC3, min, IVEC3, IVEC3);
  NATIVE2 (IVEC4, min, IVEC4, IVEC4);
  NATIVE2 (UINT, min, UINT, UINT);
  NATIVE2 (UVEC2, min, UVEC2, UINT);
  NATIVE2 (UVEC3, min, UVEC3, UINT);
  NATIVE2 (UVEC4, min, UVEC4, UINT);
  NATIVE2 (UVEC2, min, UVEC2, UVEC2);
  NATIVE2 (UVEC3, min, UVEC3, UVEC3);
  NATIVE2 (UVEC4, min, UVEC4, UVEC4);
  NATIVE2 (INT, max, INT, INT);
  NATIVE2 (IVEC2, max, IVEC2, INT);
  NATIVE2 (IVEC3, max, IVEC3, INT);
  NATIVE2 (IVEC4, max, IVEC4, INT);
  NATIVE2 (IVEC2, max, IVEC2, IVEC2);
  NATIVE2 (IVEC3, max, IVEC3, IVEC3);
  NATIVE2 (IVEC4, max, IVEC4, IVEC4);
  NATIVE2 (UINT, max, UINT, UINT);
  NATIVE2 (UVEC2, max, UVEC2, UINT);
  NATIVE2 (UVEC3, max, UVEC3, UINT);
  NATIVE2 (UVEC4, max, UVEC4, UINT);
  NATIVE2 (UVEC2, max, UVEC2, UVEC2);
  NATIVE2 (UVEC3, max, UVEC3, UVEC3);
  NATIVE2 (UVEC4, max, UVEC4, UVEC4);
  NATIVE3 (INT, clamp, INT, INT, INT);
  NATIVE3 (IVEC2, clamp, IVEC2, INT, INT);
  NATIVE3 (IVEC3, clamp, IVEC3, INT, INT);
  NATIVE3 (IVEC4, clamp, IVEC4, INT, INT);
  NATIVE3 (IVEC2, clamp, IVEC2, IVEC2, IVEC2);
  NATIVE3 (IVEC3, clamp, IVEC3, IVEC3, IVEC3);
  NATIVE3 (IVEC4, clamp, IVEC4, IVEC4, IVEC4);
  NATIVE3 (UINT, clamp, UINT, UINT, UINT);
  NATIVE3 (UVEC2, clamp, UVEC2, UINT, UINT);
  NATIVE3 (UVEC3, clamp, UVEC3, UINT, UINT);
  NATIVE3 (UVEC4, clamp, UVEC4, UINT, UINT);
  NATIVE3 (UVEC2, clamp, UVEC2, UVEC2, UVEC2);
  NATIVE3 (UVEC3, clamp, UVEC3, UVEC3, UVEC3);
  NATIVE3 (UVEC4, clamp, UVEC4, UVEC4, UVEC4);
  NATIVE3 (FLOAT, mix, FLOAT, FLOAT, BOOL);
  NATIVE3 (VEC2, mix, VEC2, VEC2, BVEC2);
  NATIVE3 (VEC3, mix, VEC3, VEC3, BVEC3);
  NATIVE3 (VEC4, mix, VEC4, VEC4, BVEC4);
  NATIVE1 (BOOL, isnan, FLOAT);
  NATIVE1 (BVEC2, isnan, VEC2);
  NATIVE1 (BVEC3, isnan, VEC3);
  NATIVE1 (BVEC4, isnan, VEC4);
  NATIVE1 (BOOL, isinf, FLOAT);
  NATIVE1 (BVEC2, isinf, VEC2);
  NATIVE1 (BVEC3, isinf, VEC3);
  NATIVE1 (BVEC4, isinf, VEC4);

  NATIVE2 (BVEC2, lessThan, UVEC2, UVEC2);
  NATIVE2 (BVEC3, lessThan, UVEC3, UVEC3);
  NATIVE2 (BVEC4, lessThan, UVEC4, UVEC4);
  NATIVE2 (BVEC2, lessThanEqual, UVEC2, UVEC2);
  NATIVE2 (BVEC3, lessThanEqual, UVEC3, UVEC3);
  NATIVE2 (BVEC4, lessThanEqual, UVEC4, UVEC4);
  NATIVE2 (BVEC2, greaterThan, UVEC2, UVEC2);
  NATIVE2 (BVEC3, greaterThan, UVEC3, UVEC3);
  NATIVE2 (BVEC4, greaterThan, UVEC4, UVEC4);
  NATIVE2 (BVEC2, greaterThanEqual, UVEC2, UVEC2);
  NATIVE2 (BVEC3, greaterThanEqual, UVEC3, UVEC3);
  NATIVE2 (BVEC4, greaterThanEqual, UVEC4, UVEC4);
  NATIVE2 (BVEC2, equal, UVEC2, UVEC2);
  NATIVE2 (BVEC3, equal, UVEC3, UVEC3);
  NATIVE2 (BVEC4, equal, UVEC4, UVEC4);
  NATIVE2 (BVEC2, notEqual, UVEC2, UVEC2);
  NATIVE2 (BVEC3, notEqual, UVEC3, UVEC3);
  NATIVE2 (BVEC4, notEqual, UVEC4, UVEC4);
}

static void
gsk_sl_native_functions_add_150 (GskSlScope       *scope,
                                 GskSlEnvironment *environment)
{
  UNIMPLEMENTED_NATIVE1 (FLOAT, determinant, MAT2);
  UNIMPLEMENTED_NATIVE1 (FLOAT, determinant, MAT3);
  UNIMPLEMENTED_NATIVE1 (FLOAT, determinant, MAT4);
  UNIMPLEMENTED_NATIVE1 (MAT2, inverse, MAT2);
  UNIMPLEMENTED_NATIVE1 (MAT3, inverse, MAT3);
  UNIMPLEMENTED_NATIVE1 (MAT4, inverse, MAT4);
}

#define PACK_TEXTURE_CALL_INFO(types, proj, lod, bias, offset, fetch, grad) \
  GUINT_TO_POINTER(((types) & 0xFF) | ((proj) << 16) | ((lod) << 18) | ((bias) << 19) | ((offset) << 20) | ((fetch) << 21) | ((grad) << 22))
#define UNPACK_TEXTURE_CALL_INFO(_info, type, proj, lod, bias, offset, fetch, grad) G_STMT_START{\
  guint info = GPOINTER_TO_UINT(_info); \
\
  type = info & 0xFF; \
  proj = (info >> 16) & 0x3; \
  lod = (info >> 18) & 0x1; \
  bias = (info >> 19) & 0x1; \
  offset = (info >> 20) & 0x1; \
  fetch = (info >> 21) & 0x1; \
  grad = (info >> 22) & 0x1; \
}G_STMT_END

static guint32
gsk_sl_native_texture_write_spv (GskSpvWriter *writer,
                                 guint32      *arguments,
                                 gpointer      user_data)
{
  guint types, proj, lod, bias, offset, fetch, grad, dref, min_lod;
  const GskSlImageType *image;
  GskSlType *type;
  guint32 extra_args[9], n_extra_args;
  guint32 mask;
  guint length;
  gboolean explicit_lod = FALSE;
  gsize i;

  UNPACK_TEXTURE_CALL_INFO(user_data, types, proj, lod, bias, offset, fetch, grad);
  type = gsk_sl_type_get_sampler (types);
  image = gsk_sl_type_get_image_type (type);
  length = gsk_sl_image_type_get_lookup_dimensions (image, proj > 0);
  mask = 0;
  n_extra_args = 0;
  i = 2;

  if (fetch && gsk_sl_image_type_needs_lod_argument (image, fetch))
    min_lod = i++;
  else
    min_lod = 0;

  /* match used flags to their argument number */
  if (lod)
    lod = i++;
  if (grad)
    {
      grad = i;
      i += 2;
    }
  if (offset)
    offset = i++;
  if (length > 4)
    dref = arguments[i++];
  else if (image->shadow)
    {
      dref = gsk_spv_writer_composite_extract (writer,
                                               gsk_sl_type_get_scalar (GSK_SL_FLOAT),
                                               arguments[1],
                                               (guint32[1]) { length - 1 - (proj ? 1 : 0) },
                                               1);
    }
  else
    dref = 0;
  if (proj)
    {
      guint real_length = (proj == 2 ? 4 : MIN (4, length));

      if (real_length != gsk_sl_image_type_get_dimensions (image) + 1)
        {
          guint32 tmp_id;

          tmp_id = gsk_spv_writer_composite_extract (writer,
                                                     gsk_sl_type_get_scalar (GSK_SL_FLOAT),
                                                     arguments[1],
                                                     (guint32[1]) { real_length - 1 },
                                                     1);
          arguments[1] = gsk_spv_writer_composite_insert (writer,
                                                          gsk_sl_type_get_vector (GSK_SL_FLOAT, real_length),
                                                          tmp_id,
                                                          arguments[1],
                                                          (guint32[1]) { gsk_sl_image_type_get_dimensions (image) },
                                                          1);
        }
    }

  if (bias)
    bias = i++;

  if (bias)
    {
      mask |= GSK_SPV_IMAGE_OPERANDS_BIAS;
      extra_args[n_extra_args++] = arguments[bias];
    }
  if (lod)
    {
      mask |= GSK_SPV_IMAGE_OPERANDS_LOD;
      extra_args[n_extra_args++] = arguments[lod];
      explicit_lod = TRUE;
    }
  if (min_lod && !image->multisampled)
    {
      g_assert (!lod);
      mask |= GSK_SPV_IMAGE_OPERANDS_LOD;
      extra_args[n_extra_args++] = arguments[min_lod];
    }

  if (grad)
    {
      mask |= GSK_SPV_IMAGE_OPERANDS_GRAD;
      extra_args[n_extra_args++] = arguments[grad];
      extra_args[n_extra_args++] = arguments[grad + 1];
      explicit_lod = TRUE;
    }
  if (offset)
    {
      if (gsk_spv_writer_get_value_for_id (writer, arguments[offset]))
        mask |= GSK_SPV_IMAGE_OPERANDS_CONST_OFFSET;
      else
        mask |= GSK_SPV_IMAGE_OPERANDS_OFFSET;
      extra_args[n_extra_args++] = arguments[offset];
    }
  if (min_lod && image->multisampled)
    {
      mask |= GSK_SPV_IMAGE_OPERANDS_SAMPLE;
      extra_args[n_extra_args++] = arguments[min_lod];
    }

  if (fetch)
    {
      guint32 image_id;

      image_id = gsk_spv_writer_image (writer,
                                       gsk_spv_writer_get_id_for_image_type (writer, image),
                                       arguments[0]);
                                       
      return gsk_spv_writer_image_fetch (writer,
                                         gsk_sl_image_type_get_pixel_type (image),
                                         image_id,
                                         arguments[1],
                                         mask,
                                         extra_args,
                                         n_extra_args);
    }
  else if (explicit_lod)
    {
      if (dref)
        {
          if (proj)
            {
              return gsk_spv_writer_image_sample_proj_dref_explicit_lod (writer,
                                                                         gsk_sl_image_type_get_pixel_type (image),
                                                                         arguments[0],
                                                                         arguments[1],
                                                                         dref,
                                                                         mask,
                                                                         extra_args,
                                                                         n_extra_args);
            }
          else
            {
              return gsk_spv_writer_image_sample_dref_explicit_lod (writer,
                                                                    gsk_sl_image_type_get_pixel_type (image),
                                                                    arguments[0],
                                                                    arguments[1],
                                                                    dref,
                                                                    mask,
                                                                    extra_args,
                                                                    n_extra_args);
            }
        }
      else
        {
          if (proj)
            {
              return gsk_spv_writer_image_sample_proj_explicit_lod (writer,
                                                                    gsk_sl_image_type_get_pixel_type (image),
                                                                    arguments[0],
                                                                    arguments[1],
                                                                    mask,
                                                                    extra_args,
                                                                    n_extra_args);
            }
          else
            {
              return gsk_spv_writer_image_sample_explicit_lod (writer,
                                                               gsk_sl_image_type_get_pixel_type (image),
                                                               arguments[0],
                                                               arguments[1],
                                                               mask,
                                                               extra_args,
                                                               n_extra_args);
            }
        }
    }
  else
    {
      if (dref)
        {
          if (proj)
            {
              return gsk_spv_writer_image_sample_proj_dref_implicit_lod (writer,
                                                                         gsk_sl_image_type_get_pixel_type (image),
                                                                         arguments[0],
                                                                         arguments[1],
                                                                         dref,
                                                                         mask,
                                                                         extra_args,
                                                                         n_extra_args);
            }
          else
            {
              return gsk_spv_writer_image_sample_dref_implicit_lod (writer,
                                                                    gsk_sl_image_type_get_pixel_type (image),
                                                                    arguments[0],
                                                                    arguments[1],
                                                                    dref,
                                                                    mask,
                                                                    extra_args,
                                                                    n_extra_args);
            }
        }
      else
        {
          if (proj)
            {
              return gsk_spv_writer_image_sample_proj_implicit_lod (writer,
                                                                    gsk_sl_image_type_get_pixel_type (image),
                                                                    arguments[0],
                                                                    arguments[1],
                                                                    mask,
                                                                    extra_args,
                                                                    n_extra_args);
            }
          else
            {
              return gsk_spv_writer_image_sample_implicit_lod (writer,
                                                               gsk_sl_image_type_get_pixel_type (image),
                                                               arguments[0],
                                                               arguments[1],
                                                               mask,
                                                               extra_args,
                                                               n_extra_args);
            }
        }
    }
}

static void
gsk_sl_native_functions_add_texture (GskSlScope       *scope,
                                     GskSlEnvironment *environment)
{
  GskSlType *type;
  const GskSlImageType *image;
  guint types, proj, lod, bias, offset, fetch, grad;

  for (types = 0; types < GSK_SL_N_SAMPLER_TYPES; types++)
    {
      type = gsk_sl_type_get_sampler (types);
      image = gsk_sl_type_get_image_type (type);

      for (proj = 0; proj < 3; proj++)
        {
          if (proj && !gsk_sl_image_type_supports_projection (image, proj == 2))
            continue;

          for (lod = 0; lod < 2; lod++)
            {
              if (lod && !gsk_sl_image_type_supports_lod (image))
                continue;

              for (bias = 0; bias < 2; bias++)
                {
                  if (bias && lod)
                    continue;

                  if (bias && gsk_sl_environment_get_stage (environment) != GSK_SL_SHADER_FRAGMENT)
                    continue;

                  if (bias && !gsk_sl_image_type_supports_bias (image))
                    continue;

                  for (offset = 0; offset < 2; offset++)
                    {
                      if (offset && !gsk_sl_image_type_supports_offset (image))
                        continue;

                      for (fetch = 0; fetch < 2; fetch++)
                        {
                          if ((proj > 0) + offset + fetch + bias + lod > 3)
                              continue;
                          if (fetch && (lod || bias))
                              continue;
                          if (fetch && !gsk_sl_image_type_supports_texel_fetch (image))
                              continue;
                          if (!fetch && !gsk_sl_image_type_supports_texture (image))
                              continue;

                          for (grad = 0; grad < 2; grad++) 
                            {
                              GskSlFunction *function;
                              GskSlFunctionType *function_type;
                              char *function_name;
                              guint length;

                              if ((proj > 0) + offset + fetch + grad + bias + lod > 3)
                                continue;

                              if (grad && (lod || bias))
                                continue;
                            
                              if (grad && !gsk_sl_image_type_supports_gradient (image))
                                continue;

                              length = gsk_sl_image_type_get_lookup_dimensions (image, proj > 0);
                              if (length > 4 && bias)
                                continue;

                              function_type = gsk_sl_function_type_new (gsk_sl_image_type_get_pixel_type (image));
                              function_type = gsk_sl_function_type_add_argument (function_type,
                                                                                 GSK_SL_STORAGE_PARAMETER_IN,
                                                                                 type);

                              /* P */
                              if (proj == 2)
                                {
                                  function_type = gsk_sl_function_type_add_argument (function_type,
                                                                                     GSK_SL_STORAGE_PARAMETER_IN,
                                                                                     gsk_sl_type_get_vector (GSK_SL_FLOAT, 4));
                                }
                              else
                                {
                                  GskSlScalarType scalar;
                                  GskSlType *arg_type;

                                  scalar = fetch ? GSK_SL_INT : GSK_SL_FLOAT;
                                  if (length == 1)
                                    arg_type = gsk_sl_type_get_scalar (scalar);
                                  else
                                    arg_type = gsk_sl_type_get_vector (scalar, MIN (length, 4));
                                  function_type = gsk_sl_function_type_add_argument (function_type,
                                                                                     GSK_SL_STORAGE_PARAMETER_IN,
                                                                                     arg_type);
                                }

                              /* non-optional lod */
                              if (fetch && gsk_sl_image_type_needs_lod_argument (image, fetch))
                                function_type = gsk_sl_function_type_add_argument (function_type,
                                                                                   GSK_SL_STORAGE_PARAMETER_IN,
                                                                                   gsk_sl_type_get_scalar (GSK_SL_INT));

                              if (lod)
                                function_type = gsk_sl_function_type_add_argument (function_type,
                                                                                   GSK_SL_STORAGE_PARAMETER_IN,
                                                                                   gsk_sl_type_get_scalar (GSK_SL_FLOAT));

                              if (grad)
                                {
                                  GskSlType *arg_type;
                                  guint dims = gsk_sl_image_type_get_dimensions (image);

                                  if (dims ==1)
                                    arg_type = gsk_sl_type_get_scalar (GSK_SL_FLOAT);
                                  else
                                    arg_type = gsk_sl_type_get_vector (GSK_SL_FLOAT, dims);

                                  function_type = gsk_sl_function_type_add_argument (function_type,
                                                                                     GSK_SL_STORAGE_PARAMETER_IN,
                                                                                     arg_type);
                                  function_type = gsk_sl_function_type_add_argument (function_type,
                                                                                     GSK_SL_STORAGE_PARAMETER_IN,
                                                                                     arg_type);
                                }

                              if (offset)
                                {
                                  GskSlType *arg_type;
                                  guint dims = gsk_sl_image_type_get_dimensions (image);

                                  if (dims ==1)
                                    arg_type = gsk_sl_type_get_scalar (GSK_SL_INT);
                                  else
                                    arg_type = gsk_sl_type_get_vector (GSK_SL_INT, dims);

                                  function_type = gsk_sl_function_type_add_argument (function_type,
                                                                                     GSK_SL_STORAGE_PARAMETER_IN,
                                                                                     arg_type);
                                }

                              /* compare */
                              if (length > 4)
                                function_type = gsk_sl_function_type_add_argument (function_type,
                                                                                   GSK_SL_STORAGE_PARAMETER_IN,
                                                                                   gsk_sl_type_get_scalar (GSK_SL_FLOAT));

                              if (bias)
                                function_type = gsk_sl_function_type_add_argument (function_type,
                                                                                   GSK_SL_STORAGE_PARAMETER_IN,
                                                                                   gsk_sl_type_get_scalar (GSK_SL_FLOAT));

                              function_name = g_strconcat (fetch ? "texel" : "texture",
                                                           proj ? "Proj" : "",
                                                           lod ? "Lod" : "",
                                                           grad ? "Grad" : "",
                                                           fetch ? "Fetch" : "",
                                                           offset ? "Offset" : "",
                                                           NULL);

                              function = gsk_sl_function_new_native (function_name,
                                                                     function_type,
                                                                     NULL,
                                                                     gsk_sl_native_texture_write_spv,
                                                                     PACK_TEXTURE_CALL_INFO (types, proj, lod, bias, offset, fetch, grad),
                                                                     NULL);
                              gsk_sl_scope_add_function (scope, function);
#if 0
                              g_print ("%p %p %p %p %p %p %p %u %u\n", PACK_TEXTURE_CALL_INFO (types, proj, lod, bias, offset, fetch, grad),
                                                  GUINT_TO_POINTER(((types) & 0xFF) | ((proj) << 16) | ((lod) << 18) | ((bias) < 19) | ((offset) << 20) | ((fetch) << 21)),
                                                  GUINT_TO_POINTER(((types) & 0xFF) | ((proj) << 16) | ((lod) << 18) | ((bias) < 19) | ((offset) << 20)),
                                                  GUINT_TO_POINTER(((types) & 0xFF) | ((proj) << 16) | ((lod) << 18) | ((bias) < 19)),
                                                  GUINT_TO_POINTER(((types) & 0xFF) | ((proj) << 16) | ((lod) << 18)),
                                                  GUINT_TO_POINTER(((types) & 0xFF) | ((proj) << 16)),
                                                  GUINT_TO_POINTER(((types) & 0xFF) ), types, (types) & 0xFF);
                              g_print ("%u %u %u %u %u %u %u\n", types, proj, lod, bias, offset, fetch, grad);
                              {
                                guint i;
                                g_print ("%s", gsk_sl_type_get_name (gsk_sl_function_type_get_return_type (function_type)));
                                g_print (" %s(", function_name);
                                for (i = 0; i < gsk_sl_function_type_get_n_arguments (function_type); i++)
                                  {
                                    if (i > 0)
                                      g_print (",");
                                    g_print ("%s", gsk_sl_type_get_name (gsk_sl_function_type_get_argument_type (function_type, i)));
                                  }
                                g_print (");\n");
                              }
#endif

                              g_free (function_name);
                              gsk_sl_function_unref (function);
                              gsk_sl_function_type_unref (function_type);
                            }
                        }
                    }
                }
            }
        }
    }
}

void
gsk_sl_native_functions_add (GskSlScope       *scope,
                             GskSlEnvironment *environment)
{
  guint version = gsk_sl_environment_get_version (environment);

  gsk_sl_native_functions_add_100 (scope, environment);

  if (version < 120)
    return;
  gsk_sl_native_functions_add_120 (scope, environment);

  if (version < 130)
    return;
  gsk_sl_native_functions_add_130 (scope, environment);

  if (version < 150)
    return;
  gsk_sl_native_functions_add_150 (scope, environment);

  gsk_sl_native_functions_add_texture (scope, environment);
}

