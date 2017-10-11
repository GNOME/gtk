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

#include "gskslbinaryprivate.h"

#include "gskslpreprocessorprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskspvwriterprivate.h"

typedef struct _GskSlBinary GskSlBinary;
struct _GskSlBinary
{
  const char *          sign;
  GskSlType *           (* check_type)                          (GskSlPreprocessor      *preproc,
                                                                 GskSlType              *ltype,
                                                                 GskSlType              *rtype);
  GskSlValue *          (* get_constant)                        (GskSlType              *type,
                                                                 GskSlValue             *lvalue,
                                                                 GskSlValue             *rvalue);
  guint32               (* write_spv)                           (GskSpvWriter           *writer,
                                                                 GskSlType              *type,
                                                                 GskSlType              *ltype,
                                                                 guint32                 left_id,
                                                                 GskSlType              *rtype,
                                                                 guint32                 right_id);
};

#define GSK_SL_BINARY_FUNC_SCALAR(func,type,...) \
static void \
func (gpointer value, gpointer scalar) \
{ \
  type x = *(type *) value; \
  type y = *(type *) scalar; \
  __VA_ARGS__ \
  *(type *) value = x; \
}

/* MULTIPLICATION */

static GskSlType *
gsk_sl_multiplication_check_type (GskSlPreprocessor *preproc,
                                  GskSlType         *ltype,
                                  GskSlType         *rtype)
{
  GskSlScalarType scalar;

  if (gsk_sl_scalar_type_can_convert (gsk_sl_type_get_scalar_type (ltype),
                                      gsk_sl_type_get_scalar_type (rtype)))
    scalar = gsk_sl_type_get_scalar_type (ltype);
  else if (gsk_sl_scalar_type_can_convert (gsk_sl_type_get_scalar_type (rtype),
                                           gsk_sl_type_get_scalar_type (ltype)))
    scalar = gsk_sl_type_get_scalar_type (rtype);
  else
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                 "Operand types %s and %s do not share compatible scalar types.",
                                 gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
      return NULL;
    }
  if (scalar == GSK_SL_BOOL)
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Cannot multiply booleans.");
      return NULL;
    }

  if (gsk_sl_type_is_matrix (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          if (gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (gsk_sl_type_get_index_type (rtype)))
            {
              gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                         "Incompatible dimensions when multiplying %s * %s.",
                                         gsk_sl_type_get_name (ltype),
                                         gsk_sl_type_get_name (rtype));
              return NULL;
            }
          return gsk_sl_type_get_matrix (scalar,
                                         gsk_sl_type_get_length (rtype),
                                         gsk_sl_type_get_length (gsk_sl_type_get_index_type (ltype)));
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          if (gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
            {
              gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                         "Matrix column count doesn't match vector length.");
              return NULL;
            }
          return gsk_sl_type_get_vector (scalar, gsk_sl_type_get_length (gsk_sl_type_get_index_type (ltype)));
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          return gsk_sl_type_get_matrix (scalar,
                                         gsk_sl_type_get_length (ltype),
                                         gsk_sl_type_get_length (gsk_sl_type_get_index_type (ltype)));
        }
      else
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                     "Right operand is incompatible type for multiplication.");
          return NULL;
        }
    }
  else if (gsk_sl_type_is_vector (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          if (gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (gsk_sl_type_get_index_type (rtype)))
            {
              gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                         "Vector length for %s doesn't match row count for %s",
                                         gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
              return NULL;
            }
          return gsk_sl_type_get_vector (scalar, gsk_sl_type_get_length (rtype));
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          if (gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
            {
              gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                         "Vector operands %s and %s to arithmetic multiplication have different length.",
                                         gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
              return NULL;
            }
          return gsk_sl_type_get_vector (scalar, gsk_sl_type_get_length (ltype));
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          return gsk_sl_type_get_vector (scalar,
                                         gsk_sl_type_get_length (ltype));
        }
      else
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                     "Right operand is incompatible type for multiplication.");
          return NULL;
        }
    }
  else if (gsk_sl_type_is_scalar (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          return gsk_sl_type_get_matrix (scalar,
                                         gsk_sl_type_get_length (rtype),
                                         gsk_sl_type_get_length (gsk_sl_type_get_index_type (rtype)));
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          return gsk_sl_type_get_vector (scalar,
                                         gsk_sl_type_get_length (rtype));
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          return gsk_sl_type_get_scalar (scalar);
        }
      else
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Right operand is incompatible type for multiplication.");
          return NULL;
        }
    }
  else
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Left operand is incompatible type for multiplication.");
      return NULL;
    }
}

GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_multiplication_int, gint32, x *= y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_multiplication_uint, guint32, x *= y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_multiplication_float, float, x *= y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_multiplication_double, double, x *= y;)
static void (* mult_funcs[]) (gpointer, gpointer) = {
  [GSK_SL_INT] = gsk_sl_expression_multiplication_int,
  [GSK_SL_UINT] = gsk_sl_expression_multiplication_uint,
  [GSK_SL_FLOAT] = gsk_sl_expression_multiplication_float,
  [GSK_SL_DOUBLE] = gsk_sl_expression_multiplication_double,
};

static GskSlValue *
gsk_sl_multiplication_get_constant (GskSlType  *type,
                                    GskSlValue *lvalue,
                                    GskSlValue *rvalue)
{
  GskSlValue *result;
  GskSlType *ltype, *rtype;
  GskSlScalarType scalar;

  scalar = gsk_sl_type_get_scalar_type (type);
  lvalue = gsk_sl_value_convert_components (lvalue, scalar);
  rvalue = gsk_sl_value_convert_components (rvalue, scalar);
  ltype = gsk_sl_value_get_type (lvalue);
  rtype = gsk_sl_value_get_type (rvalue);

  if ((gsk_sl_type_is_matrix (rtype) && gsk_sl_type_is_matrix (ltype)) ||
      (gsk_sl_type_is_vector (rtype) && gsk_sl_type_is_matrix (ltype)) ||
      (gsk_sl_type_is_matrix (rtype) && gsk_sl_type_is_vector (ltype)))
    {
      gsize c, cols;
      gsize r, rows;
      gsize i, n;
      gpointer data, ldata, rdata;

      result = gsk_sl_value_new (type);
      data = gsk_sl_value_get_data (result);
      ldata = gsk_sl_value_get_data (lvalue);
      rdata = gsk_sl_value_get_data (rvalue);

      if (gsk_sl_type_is_vector (rtype))
        {
          cols = 1;
          rows = gsk_sl_type_get_length (gsk_sl_value_get_type (result));
          n = gsk_sl_type_get_length (rtype);
        }
      else if (gsk_sl_type_is_vector (ltype))
        {
          cols = gsk_sl_type_get_length (gsk_sl_value_get_type (result));
          rows = 1;
          n = gsk_sl_type_get_length (ltype);
        }
      else
        {
          cols = gsk_sl_type_get_length (gsk_sl_value_get_type (result));
          rows = gsk_sl_type_get_length (gsk_sl_type_get_index_type (gsk_sl_value_get_type (result)));
          n = gsk_sl_type_get_length (ltype);
        }
#define MATRIXMULT(TYPE) G_STMT_START{\
        for (c = 0; c < cols; c++) \
          { \
            for (r = 0; r < rows; r++) \
              { \
                TYPE result = 0; \
                for (i = 0; i < n; i++) \
                  { \
                    result += *((TYPE *) rdata + c * n + i) *  \
                              *((TYPE *) ldata + i * rows + r); \
                  } \
                *((TYPE *) data + c * rows + r) = result; \
              } \
          } \
      }G_STMT_END
      if (gsk_sl_type_get_scalar_type (type) == GSK_SL_DOUBLE)
        MATRIXMULT(double);
      else
        MATRIXMULT(float);
      gsk_sl_value_free (lvalue);
      gsk_sl_value_free (rvalue);
      return result;
    }
  else
    {
      /* we can multiply componentwise */
      gsize ln, rn;

      ln = gsk_sl_type_get_n_components (ltype);
      rn = gsk_sl_type_get_n_components (rtype);
      if (ln == 1)
        {
          gsk_sl_value_componentwise (rvalue, mult_funcs[scalar], gsk_sl_value_get_data (lvalue));
          gsk_sl_value_free (lvalue);
          result = rvalue;
        }
      else if (rn == 1)
        {
          gsk_sl_value_componentwise (lvalue, mult_funcs[scalar], gsk_sl_value_get_data (rvalue));
          gsk_sl_value_free (rvalue);
          result = lvalue;
        }
      else
        {
          guchar *ldata, *rdata;
          gsize i, stride;

          stride = gsk_sl_scalar_type_get_size (scalar);
          ldata = gsk_sl_value_get_data (lvalue);
          rdata = gsk_sl_value_get_data (rvalue);
          for (i = 0; i < ln; i++)
            {
              mult_funcs[scalar] (ldata + i * stride, rdata + i * stride);
            }
          gsk_sl_value_free (rvalue);
          result = lvalue;
        }
    }

  return result;
}

static guint32
gsk_sl_multiplication_write_spv (GskSpvWriter *writer,
                                 GskSlType    *type,
                                 GskSlType    *ltype,
                                 guint32       left_id,
                                 GskSlType    *rtype,
                                 guint32       right_id)
{
  if (gsk_sl_type_get_scalar_type (ltype) != gsk_sl_type_get_scalar_type (type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (ltype, gsk_sl_type_get_scalar_type (type));
      left_id = gsk_spv_writer_convert (writer, left_id, ltype, new_type);
      ltype = new_type;
    }
  if (gsk_sl_type_get_scalar_type (rtype) != gsk_sl_type_get_scalar_type (type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (rtype, gsk_sl_type_get_scalar_type (type));
      right_id = gsk_spv_writer_convert (writer, right_id, rtype, new_type);
      rtype = new_type;
    }

  if (gsk_sl_type_is_matrix (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          return gsk_spv_writer_matrix_times_matrix (writer, type, left_id, right_id);
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          return gsk_spv_writer_matrix_times_vector (writer, type, left_id, right_id);
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          return gsk_spv_writer_matrix_times_scalar (writer, type, left_id, right_id);
        }
    }
  else if (gsk_sl_type_is_vector (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          return gsk_spv_writer_vector_times_matrix (writer, type, left_id, right_id);
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          switch (gsk_sl_type_get_scalar_type (type))
            {
            case GSK_SL_FLOAT:
            case GSK_SL_DOUBLE:
              return gsk_spv_writer_f_mul (writer, type, left_id, right_id);
            case GSK_SL_INT:
            case GSK_SL_UINT:
              return gsk_spv_writer_i_mul (writer, type, left_id, right_id);
            case GSK_SL_VOID:
            case GSK_SL_BOOL:
            default:
              g_assert_not_reached ();
              break;
            }
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          return gsk_spv_writer_vector_times_scalar (writer, type, left_id, right_id);
        }
    }
  else if (gsk_sl_type_is_scalar (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          return gsk_spv_writer_matrix_times_scalar (writer, type, right_id, left_id);
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          return gsk_spv_writer_vector_times_scalar (writer, type, right_id, left_id);
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          switch (gsk_sl_type_get_scalar_type (type))
            {
            case GSK_SL_FLOAT:
            case GSK_SL_DOUBLE:
              return gsk_spv_writer_f_mul (writer, type, left_id, right_id);

            case GSK_SL_INT:
            case GSK_SL_UINT:
              return gsk_spv_writer_i_mul (writer, type, left_id, right_id);

            case GSK_SL_VOID:
            case GSK_SL_BOOL:
            default:
              g_assert_not_reached ();
              break;
            }
        }
    }

  g_assert_not_reached ();

  return 0;
}

static const GskSlBinary GSK_SL_BINARY_MULTIPLICATION = {
  "*",
  gsk_sl_multiplication_check_type,
  gsk_sl_multiplication_get_constant,
  gsk_sl_multiplication_write_spv
};

/* DIVISION */

static GskSlType *
gsk_sl_arithmetic_check_type (GskSlPreprocessor *preproc,
                              GskSlType         *ltype,
                              GskSlType         *rtype)
{
  GskSlScalarType scalar;

  if (gsk_sl_scalar_type_can_convert (gsk_sl_type_get_scalar_type (ltype),
                                      gsk_sl_type_get_scalar_type (rtype)))
    scalar = gsk_sl_type_get_scalar_type (ltype);
  else if (gsk_sl_scalar_type_can_convert (gsk_sl_type_get_scalar_type (rtype),
                                           gsk_sl_type_get_scalar_type (ltype)))
    scalar = gsk_sl_type_get_scalar_type (rtype);
  else
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                 "Operand types %s and %s do not share compatible scalar types.",
                                 gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
      return NULL;
    }

  if (gsk_sl_type_is_matrix (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          if (gsk_sl_type_can_convert (ltype, rtype))
            {
              return ltype;
            }
          else if (gsk_sl_type_can_convert (rtype, ltype))
            {
              return rtype;
            }
          else
            {
              gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                         "Matrix types %s and %s have different size.",
                                         gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
              return NULL;
            }
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                     "Cannot perform arithmetic arithmetic between matrix and vector.");
          return NULL;
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          return gsk_sl_type_get_matrix (scalar,
                                         gsk_sl_type_get_length (ltype),
                                         gsk_sl_type_get_length (gsk_sl_type_get_index_type (ltype)));
        }
      else
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                     "Right operand is incompatible type for arithemtic arithmetic.");
          return NULL;
        }
    }
  else if (gsk_sl_type_is_vector (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Cannot perform arithmetic arithmetic between vector and matrix.");
          return NULL;
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          if (gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
            {
              gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                         "Vector operands %s and %s to arithmetic arithmetic have different length.",
                                         gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
              return NULL;
            }
          return gsk_sl_type_get_vector (scalar, gsk_sl_type_get_length (ltype));
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          return gsk_sl_type_get_vector (scalar,
                                         gsk_sl_type_get_length (ltype));
        }
      else
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                     "Right operand is incompatible type for arithemtic arithmetic.");
          return NULL;
        }
    }
  else if (gsk_sl_type_is_scalar (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          return gsk_sl_type_get_matrix (scalar,
                                         gsk_sl_type_get_length (rtype),
                                         gsk_sl_type_get_length (gsk_sl_type_get_index_type (rtype)));
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          return gsk_sl_type_get_vector (scalar,
                                         gsk_sl_type_get_length (rtype));
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          return gsk_sl_type_get_scalar (scalar);
        }
      else
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Right operand is incompatible type for arithemtic arithmetic.");
          return NULL;
        }
    }
  else
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Left operand is incompatible type for arithemtic arithmetic.");
      return NULL;
    }
}

GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_division_int, gint32, x = y == 0 ? G_MAXINT32 : x / y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_division_uint, guint32, x = y == 0 ? G_MAXUINT32 : x / y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_division_float, float, x /= y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_division_double, double, x /= y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_division_int_inv, gint32, x = x == 0 ? G_MAXINT32 : y / x;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_division_uint_inv, guint32, x = x == 0 ? G_MAXUINT32 : y / x;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_division_float_inv, float, x = y / x;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_division_double_inv, double, x = y / x;)
static void (* div_funcs[]) (gpointer, gpointer) = {
  [GSK_SL_INT] = gsk_sl_expression_division_int,
  [GSK_SL_UINT] = gsk_sl_expression_division_uint,
  [GSK_SL_FLOAT] = gsk_sl_expression_division_float,
  [GSK_SL_DOUBLE] = gsk_sl_expression_division_double,
};
static void (* div_inv_funcs[]) (gpointer, gpointer) = {
  [GSK_SL_INT] = gsk_sl_expression_division_int_inv,
  [GSK_SL_UINT] = gsk_sl_expression_division_uint_inv,
  [GSK_SL_FLOAT] = gsk_sl_expression_division_float_inv,
  [GSK_SL_DOUBLE] = gsk_sl_expression_division_double_inv,
};

static GskSlValue *
gsk_sl_division_get_constant (GskSlType  *type,
                              GskSlValue *lvalue,
                              GskSlValue *rvalue)
{
  GskSlValue *result;
  GskSlType *ltype, *rtype;
  GskSlScalarType scalar;
  gsize ln, rn;

  scalar = gsk_sl_type_get_scalar_type (type);
  lvalue = gsk_sl_value_convert_components (lvalue, scalar);
  rvalue = gsk_sl_value_convert_components (rvalue, scalar);
  ltype = gsk_sl_value_get_type (lvalue);
  rtype = gsk_sl_value_get_type (rvalue);

  ln = gsk_sl_type_get_n_components (ltype);
  rn = gsk_sl_type_get_n_components (rtype);
  if (ln == 1)
    {
      gsk_sl_value_componentwise (rvalue, div_inv_funcs[scalar], gsk_sl_value_get_data (lvalue));
      gsk_sl_value_free (lvalue);
      result = rvalue;
    }
  else if (rn == 1)
    {
      gsk_sl_value_componentwise (lvalue, div_funcs[scalar], gsk_sl_value_get_data (rvalue));
      gsk_sl_value_free (rvalue);
      result = lvalue;
    }
  else
    {
      guchar *ldata, *rdata;
      gsize i, stride;

      stride = gsk_sl_scalar_type_get_size (scalar);
      ldata = gsk_sl_value_get_data (lvalue);
      rdata = gsk_sl_value_get_data (rvalue);
      for (i = 0; i < ln; i++)
        {
          div_funcs[scalar] (ldata + i * stride, rdata + i * stride);
        }
      gsk_sl_value_free (rvalue);
      result = lvalue;
    }

  return result;
}

static guint32
gsk_sl_division_write_spv (GskSpvWriter *writer,
                           GskSlType    *type,
                           GskSlType    *ltype,
                           guint32       left_id,
                           GskSlType    *rtype,
                           guint32       right_id)
{
  if (gsk_sl_type_get_scalar_type (ltype) != gsk_sl_type_get_scalar_type (type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (ltype, gsk_sl_type_get_scalar_type (type));
      left_id = gsk_spv_writer_convert (writer, left_id, ltype, new_type);
      ltype = new_type;
    }
  if (gsk_sl_type_get_scalar_type (rtype) != gsk_sl_type_get_scalar_type (type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (rtype, gsk_sl_type_get_scalar_type (type));
      right_id = gsk_spv_writer_convert (writer, right_id, rtype, new_type);
      rtype = new_type;
    }

  if (gsk_sl_type_is_matrix (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          GskSlType *col_type = gsk_sl_type_get_index_type (ltype);
          gsize cols = gsk_sl_type_get_length (ltype);
          gsize c;
          guint32 left_part_id, right_part_id, ids[cols];

          for (c = 0; c < cols; c++)
            {
              left_part_id = gsk_spv_writer_composite_extract (writer, 
                                                               col_type,
                                                               left_id,
                                                               (guint32[1]) { c }, 1);
              right_part_id = gsk_spv_writer_composite_extract (writer, 
                                                                col_type,
                                                                right_id,
                                                                (guint32[1]) { c }, 1);
              ids[c] = gsk_spv_writer_f_div (writer,
                                             col_type,
                                             left_part_id,
                                             right_part_id);
            }

          return gsk_spv_writer_composite_construct (writer, 
                                                     type,
                                                     ids,
                                                     cols);
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          guint32 tmp_id;

          tmp_id = gsk_spv_writer_f_div (writer,
                                         rtype,
                                         gsk_spv_writer_get_id_for_one (writer, gsk_sl_type_get_scalar_type (type)),
                                         right_id);

          return gsk_spv_writer_matrix_times_scalar (writer,
                                                     type,
                                                     left_id,
                                                     tmp_id);
        }
      else
        {
          g_assert_not_reached ();
          return 0;
        }
    }
  else if (gsk_sl_type_is_matrix (rtype))
    {
      guint32 tmp_id;

      tmp_id = gsk_spv_writer_f_div (writer,
                                     ltype,
                                     gsk_spv_writer_get_id_for_one (writer, gsk_sl_type_get_scalar_type (type)),
                                     left_id);
      return gsk_spv_writer_matrix_times_scalar (writer,
                                                 type,
                                                 right_id,
                                                 tmp_id);
    }
  else
    {
      /* ltype and rtype are not matrices */

      if (gsk_sl_type_is_scalar (ltype) && gsk_sl_type_is_vector (rtype))
        {
           guint32 tmp_id = gsk_spv_writer_composite_construct (writer,
                                                                type,
                                                                (guint32[4]) { left_id, left_id, left_id, left_id },
                                                                gsk_sl_type_get_length (rtype));
           left_id = tmp_id;
        }
      else if (gsk_sl_type_is_scalar (rtype) && gsk_sl_type_is_vector (ltype))
        {
           guint32 tmp_id = gsk_spv_writer_composite_construct (writer,
                                                                type,
                                                                (guint32[4]) { right_id, right_id, right_id, right_id },
                                                                gsk_sl_type_get_length (ltype));
           right_id = tmp_id;
        }

      /* ltype and rtype have the same number of components now */

      switch (gsk_sl_type_get_scalar_type (type))
        {
        case GSK_SL_FLOAT:
        case GSK_SL_DOUBLE:
          return gsk_spv_writer_f_div (writer, type, left_id, right_id);

        case GSK_SL_INT:
          return gsk_spv_writer_s_div (writer, type, left_id, right_id);

        case GSK_SL_UINT:
          return gsk_spv_writer_u_div (writer, type, left_id, right_id);

        case GSK_SL_VOID:
        case GSK_SL_BOOL:
        default:
          g_assert_not_reached ();
          return 0;
        }
    }
}

static const GskSlBinary GSK_SL_BINARY_DIVISION = {
  "/",
  gsk_sl_arithmetic_check_type,
  gsk_sl_division_get_constant,
  gsk_sl_division_write_spv
};

/* ADDITION */

GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_addition_int, gint32, x = y == 0 ? G_MAXINT32 : x / y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_addition_uint, guint32, x = y == 0 ? G_MAXUINT32 : x / y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_addition_float, float, x /= y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_addition_double, double, x /= y;)
static void (* add_funcs[]) (gpointer, gpointer) = {
  [GSK_SL_INT] = gsk_sl_expression_addition_int,
  [GSK_SL_UINT] = gsk_sl_expression_addition_uint,
  [GSK_SL_FLOAT] = gsk_sl_expression_addition_float,
  [GSK_SL_DOUBLE] = gsk_sl_expression_addition_double,
};

static GskSlValue *
gsk_sl_addition_get_constant (GskSlType  *type,
                              GskSlValue *lvalue,
                              GskSlValue *rvalue)
{
  GskSlValue *result;
  GskSlType *ltype, *rtype;
  GskSlScalarType scalar;
  gsize ln, rn;

  scalar = gsk_sl_type_get_scalar_type (type);
  lvalue = gsk_sl_value_convert_components (lvalue, scalar);
  rvalue = gsk_sl_value_convert_components (rvalue, scalar);
  ltype = gsk_sl_value_get_type (lvalue);
  rtype = gsk_sl_value_get_type (rvalue);

  ln = gsk_sl_type_get_n_components (ltype);
  rn = gsk_sl_type_get_n_components (rtype);
  if (ln == 1)
    {
      gsk_sl_value_componentwise (rvalue, add_funcs[scalar], gsk_sl_value_get_data (lvalue));
      gsk_sl_value_free (lvalue);
      result = rvalue;
    }
  else if (rn == 1)
    {
      gsk_sl_value_componentwise (lvalue, add_funcs[scalar], gsk_sl_value_get_data (rvalue));
      gsk_sl_value_free (rvalue);
      result = lvalue;
    }
  else
    {
      guchar *ldata, *rdata;
      gsize i, stride;

      stride = gsk_sl_scalar_type_get_size (scalar);
      ldata = gsk_sl_value_get_data (lvalue);
      rdata = gsk_sl_value_get_data (rvalue);
      for (i = 0; i < ln; i++)
        {
          add_funcs[scalar] (ldata + i * stride, rdata + i * stride);
        }
      gsk_sl_value_free (rvalue);
      result = lvalue;
    }

  return result;
}

static guint32
gsk_sl_addition_write_spv (GskSpvWriter *writer,
                           GskSlType    *type,
                           GskSlType    *ltype,
                           guint32       left_id,
                           GskSlType    *rtype,
                           guint32       right_id)
{
  if (gsk_sl_type_get_scalar_type (ltype) != gsk_sl_type_get_scalar_type (type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (ltype, gsk_sl_type_get_scalar_type (type));
      left_id = gsk_spv_writer_convert (writer, left_id, ltype, new_type);
      ltype = new_type;
    }
  if (gsk_sl_type_get_scalar_type (rtype) != gsk_sl_type_get_scalar_type (type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (rtype, gsk_sl_type_get_scalar_type (type));
      right_id = gsk_spv_writer_convert (writer, right_id, rtype, new_type);
      rtype = new_type;
    }

  if (gsk_sl_type_is_matrix (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          GskSlType *col_type = gsk_sl_type_get_index_type (ltype);
          gsize cols = gsk_sl_type_get_length (ltype);
          gsize c;
          guint32 left_part_id, right_part_id, ids[cols];

          for (c = 0; c < cols; c++)
            {
              left_part_id = gsk_spv_writer_composite_extract (writer, 
                                                               col_type,
                                                               left_id,
                                                               (guint32[1]) { c }, 1);
              right_part_id = gsk_spv_writer_composite_extract (writer, 
                                                                col_type,
                                                                right_id,
                                                                (guint32[1]) { c }, 1);
              ids[c] = gsk_spv_writer_f_add (writer,
                                             col_type,
                                             left_part_id,
                                             right_part_id);
            }

          return gsk_spv_writer_composite_construct (writer, 
                                                     type,
                                                     ids,
                                                     cols);
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          GskSlType *col_type = gsk_sl_type_get_index_type (ltype);
          gsize cols = gsk_sl_type_get_length (ltype);
          gsize c;
          guint32 left_part_id, right_part_id, ids[cols];

          right_part_id = gsk_spv_writer_composite_construct (writer,
                                                              col_type,
                                                              (guint32[4]) { right_id, right_id, right_id, right_id },
                                                              gsk_sl_type_get_length (col_type));
          for (c = 0; c < cols; c++)
            {
              left_part_id = gsk_spv_writer_composite_extract (writer, 
                                                               col_type,
                                                               left_id,
                                                               (guint32[1]) { c }, 1);
              ids[c] = gsk_spv_writer_f_add (writer,
                                             col_type,
                                             left_part_id,
                                             right_part_id);
            }

          return gsk_spv_writer_composite_construct (writer, 
                                                     type,
                                                     ids,
                                                     cols);
        }
      else
        {
          g_assert_not_reached ();
          return 0;
        }
    }
  else if (gsk_sl_type_is_matrix (rtype))
    {
      GskSlType *col_type = gsk_sl_type_get_index_type (rtype);
      gsize cols = gsk_sl_type_get_length (rtype);
      gsize c;
      guint32 left_part_id, right_part_id, ids[cols];

      left_part_id = gsk_spv_writer_composite_construct (writer,
                                                          col_type,
                                                          (guint32[4]) { left_id, left_id, left_id, left_id },
                                                          gsk_sl_type_get_length (col_type));
      for (c = 0; c < cols; c++)
        {
          right_part_id = gsk_spv_writer_composite_extract (writer, 
                                                            col_type,
                                                            right_id,
                                                            (guint32[1]) { c }, 1);
          ids[c] = gsk_spv_writer_f_add (writer,
                                         col_type,
                                         left_part_id,
                                         right_part_id);
        }

      return gsk_spv_writer_composite_construct (writer, 
                                                 type,
                                                 ids,
                                                 cols);
    }
  else
    {
      /* ltype and rtype are not matrices */

      if (gsk_sl_type_is_scalar (ltype) && gsk_sl_type_is_vector (rtype))
        {
           guint32 tmp_id = gsk_spv_writer_composite_construct (writer,
                                                                type,
                                                                (guint32[4]) { left_id, left_id, left_id, left_id },
                                                                gsk_sl_type_get_length (rtype));
           left_id = tmp_id;
        }
      else if (gsk_sl_type_is_scalar (rtype) && gsk_sl_type_is_vector (ltype))
        {
           guint32 tmp_id = gsk_spv_writer_composite_construct (writer,
                                                                type,
                                                                (guint32[4]) { right_id, right_id, right_id, right_id },
                                                                gsk_sl_type_get_length (ltype));
           right_id = tmp_id;
        }

      /* ltype and rtype have the same number of components now */

      switch (gsk_sl_type_get_scalar_type (type))
        {
        case GSK_SL_FLOAT:
        case GSK_SL_DOUBLE:
          return gsk_spv_writer_f_add (writer, type, left_id, right_id);

        case GSK_SL_INT:
        case GSK_SL_UINT:
          return gsk_spv_writer_i_add (writer, type, left_id, right_id);

        case GSK_SL_VOID:
        case GSK_SL_BOOL:
        default:
          g_assert_not_reached ();
          return 0;
        }
    }
}

static const GskSlBinary GSK_SL_BINARY_ADDITION = {
  "+",
  gsk_sl_arithmetic_check_type,
  gsk_sl_addition_get_constant,
  gsk_sl_addition_write_spv
};

GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_subtraction_int, gint32, x -= y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_subtraction_uint, guint32, x -= y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_subtraction_float, float, x -= y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_subtraction_double, double, x -= y;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_subtraction_int_inv, gint32, x = y - x;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_subtraction_uint_inv, guint32, x = y - x;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_subtraction_float_inv, float, x = y - x;)
GSK_SL_BINARY_FUNC_SCALAR(gsk_sl_expression_subtraction_double_inv, double, x = y - x;)
static void (* subtraction_funcs[]) (gpointer, gpointer) = {
  [GSK_SL_INT] = gsk_sl_expression_subtraction_int,
  [GSK_SL_UINT] = gsk_sl_expression_subtraction_uint,
  [GSK_SL_FLOAT] = gsk_sl_expression_subtraction_float,
  [GSK_SL_DOUBLE] = gsk_sl_expression_subtraction_double,
};
static void (* subtraction_inv_funcs[]) (gpointer, gpointer) = {
  [GSK_SL_INT] = gsk_sl_expression_subtraction_int_inv,
  [GSK_SL_UINT] = gsk_sl_expression_subtraction_uint_inv,
  [GSK_SL_FLOAT] = gsk_sl_expression_subtraction_float_inv,
  [GSK_SL_DOUBLE] = gsk_sl_expression_subtraction_double_inv,
};

static GskSlValue *
gsk_sl_subtraction_get_constant (GskSlType  *type,
                                 GskSlValue *lvalue,
                                 GskSlValue *rvalue)
{
  GskSlValue *result;
  GskSlType *ltype, *rtype;
  GskSlScalarType scalar;
  gsize ln, rn;

  scalar = gsk_sl_type_get_scalar_type (type);
  lvalue = gsk_sl_value_convert_components (lvalue, scalar);
  rvalue = gsk_sl_value_convert_components (rvalue, scalar);
  ltype = gsk_sl_value_get_type (lvalue);
  rtype = gsk_sl_value_get_type (rvalue);

  ln = gsk_sl_type_get_n_components (ltype);
  rn = gsk_sl_type_get_n_components (rtype);
  if (ln == 1)
    {
      gsk_sl_value_componentwise (rvalue, subtraction_inv_funcs[scalar], gsk_sl_value_get_data (lvalue));
      gsk_sl_value_free (lvalue);
      result = rvalue;
    }
  else if (rn == 1)
    {
      gsk_sl_value_componentwise (lvalue, subtraction_funcs[scalar], gsk_sl_value_get_data (rvalue));
      gsk_sl_value_free (rvalue);
      result = lvalue;
    }
  else
    {
      guchar *ldata, *rdata;
      gsize i, stride;

      stride = gsk_sl_scalar_type_get_size (scalar);
      ldata = gsk_sl_value_get_data (lvalue);
      rdata = gsk_sl_value_get_data (rvalue);
      for (i = 0; i < ln; i++)
        {
          subtraction_funcs[scalar] (ldata + i * stride, rdata + i * stride);
        }
      gsk_sl_value_free (rvalue);
      result = lvalue;
    }

  return result;
}

static guint32
gsk_sl_subtraction_write_spv (GskSpvWriter *writer,
                              GskSlType    *type,
                              GskSlType    *ltype,
                              guint32       left_id,
                              GskSlType    *rtype,
                              guint32       right_id)
{
  if (gsk_sl_type_get_scalar_type (ltype) != gsk_sl_type_get_scalar_type (type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (ltype, gsk_sl_type_get_scalar_type (type));
      left_id = gsk_spv_writer_convert (writer, left_id, ltype, new_type);
      ltype = new_type;
    }
  if (gsk_sl_type_get_scalar_type (rtype) != gsk_sl_type_get_scalar_type (type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (rtype, gsk_sl_type_get_scalar_type (type));
      right_id = gsk_spv_writer_convert (writer, right_id, rtype, new_type);
      rtype = new_type;
    }

  if (gsk_sl_type_is_matrix (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          GskSlType *col_type = gsk_sl_type_get_index_type (ltype);
          gsize cols = gsk_sl_type_get_length (ltype);
          gsize c;
          guint32 left_part_id, right_part_id, ids[cols];

          for (c = 0; c < cols; c++)
            {
              left_part_id = gsk_spv_writer_composite_extract (writer, 
                                                               col_type,
                                                               left_id,
                                                               (guint32[1]) { c }, 1);
              right_part_id = gsk_spv_writer_composite_extract (writer, 
                                                                col_type,
                                                                right_id,
                                                                (guint32[1]) { c }, 1);
              ids[c] = gsk_spv_writer_f_sub (writer,
                                             col_type,
                                             left_part_id,
                                             right_part_id);
            }

          return gsk_spv_writer_composite_construct (writer, 
                                                     type,
                                                     ids,
                                                     cols);
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          GskSlType *col_type = gsk_sl_type_get_index_type (ltype);
          gsize cols = gsk_sl_type_get_length (ltype);
          gsize c;
          guint32 left_part_id, right_part_id, ids[cols];

          right_part_id = gsk_spv_writer_composite_construct (writer,
                                                              col_type,
                                                              (guint32[4]) { right_id, right_id, right_id, right_id },
                                                              gsk_sl_type_get_length (col_type));
          for (c = 0; c < cols; c++)
            {
              left_part_id = gsk_spv_writer_composite_extract (writer, 
                                                               col_type,
                                                               left_id,
                                                               (guint32[1]) { c }, 1);
              ids[c] = gsk_spv_writer_f_sub (writer,
                                             col_type,
                                             left_part_id,
                                             right_part_id);
            }

          return gsk_spv_writer_composite_construct (writer, 
                                                     type,
                                                     ids,
                                                     cols);
        }
      else
        {
          g_assert_not_reached ();
          return 0;
        }
    }
  else if (gsk_sl_type_is_matrix (rtype))
    {
      GskSlType *col_type = gsk_sl_type_get_index_type (rtype);
      gsize cols = gsk_sl_type_get_length (rtype);
      gsize c;
      guint32 left_part_id, right_part_id, ids[cols];

      left_part_id = gsk_spv_writer_composite_construct (writer,
                                                          col_type,
                                                          (guint32[4]) { left_id, left_id, left_id, left_id },
                                                          gsk_sl_type_get_length (col_type));
      for (c = 0; c < cols; c++)
        {
          right_part_id = gsk_spv_writer_composite_extract (writer, 
                                                            col_type,
                                                            right_id,
                                                            (guint32[1]) { c }, 1);
          ids[c] = gsk_spv_writer_f_sub (writer,
                                         col_type,
                                         left_part_id,
                                         right_part_id);
        }

      return gsk_spv_writer_composite_construct (writer, 
                                                 type,
                                                 ids,
                                                 cols);
    }
  else
    {
      /* ltype and rtype are not matrices */

      if (gsk_sl_type_is_scalar (ltype) && gsk_sl_type_is_vector (rtype))
        {
           guint32 tmp_id = gsk_spv_writer_composite_construct (writer,
                                                                type,
                                                                (guint32[4]) { left_id, left_id, left_id, left_id },
                                                                gsk_sl_type_get_length (rtype));
           left_id = tmp_id;
        }
      else if (gsk_sl_type_is_scalar (rtype) && gsk_sl_type_is_vector (ltype))
        {
           guint32 tmp_id = gsk_spv_writer_composite_construct (writer,
                                                                type,
                                                                (guint32[4]) { right_id, right_id, right_id, right_id },
                                                                gsk_sl_type_get_length (ltype));
           right_id = tmp_id;
        }

      /* ltype and rtype have the same number of components now */

      switch (gsk_sl_type_get_scalar_type (type))
        {
        case GSK_SL_FLOAT:
        case GSK_SL_DOUBLE:
          return gsk_spv_writer_f_sub (writer, type, left_id, right_id);

        case GSK_SL_INT:
        case GSK_SL_UINT:
          return gsk_spv_writer_i_sub (writer, type, left_id, right_id);

        case GSK_SL_VOID:
        case GSK_SL_BOOL:
        default:
          g_assert_not_reached ();
          return 0;
        }
    }
}

static const GskSlBinary GSK_SL_BINARY_SUBTRACTION = {
  "-",
  gsk_sl_arithmetic_check_type,
  gsk_sl_subtraction_get_constant,
  gsk_sl_subtraction_write_spv
};

/* UNIMPLEMENTED */

static GskSlType *
gsk_sl_bitwise_check_type (GskSlPreprocessor *preproc,
                           GskSlType         *ltype,
                           GskSlType         *rtype)
{
  GskSlScalarType lscalar, rscalar;

  lscalar = gsk_sl_type_get_scalar_type (ltype);
  if (lscalar != GSK_SL_INT && lscalar != GSK_SL_UINT)
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Left operand %s is not an integer type.", gsk_sl_type_get_name (ltype));
      return NULL;
    }
  rscalar = gsk_sl_type_get_scalar_type (ltype);
  if (rscalar != GSK_SL_INT && rscalar != GSK_SL_UINT)
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Right operand %s is not an integer type.", gsk_sl_type_get_name (rtype));
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (ltype) && !gsk_sl_type_is_vector (ltype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Left operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (ltype));
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (rtype) && !gsk_sl_type_is_vector (rtype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Right operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (rtype));
      return NULL;
    }
  if (gsk_sl_type_is_vector (ltype) && gsk_sl_type_is_vector (rtype) &&
      gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                 "Vector operands %s and %s do not have the same length.",
                                 gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
      return NULL;
    }

  rscalar = lscalar == GSK_SL_UINT ? GSK_SL_UINT : rscalar;
  if (gsk_sl_type_is_scalar (ltype) && gsk_sl_type_is_scalar (rtype))
    return gsk_sl_type_get_scalar (rscalar);
  else
    return gsk_sl_type_get_vector (rscalar, gsk_sl_type_get_length (ltype));
}

static GskSlType *
gsk_sl_shift_check_type (GskSlPreprocessor *preproc,
                         GskSlType         *ltype,
                         GskSlType         *rtype)
{
  GskSlScalarType lscalar, rscalar;

  lscalar = gsk_sl_type_get_scalar_type (ltype);
  if (lscalar != GSK_SL_INT && lscalar != GSK_SL_UINT)
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Left operand %s is not an integer type.", gsk_sl_type_get_name (ltype));
      return NULL;
    }
  rscalar = gsk_sl_type_get_scalar_type (ltype);
  if (rscalar != GSK_SL_INT && rscalar != GSK_SL_UINT)
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Right operand %s is not an integer type.", gsk_sl_type_get_name (rtype));
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (ltype) && !gsk_sl_type_is_vector (ltype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Left operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (ltype));
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (rtype) && !gsk_sl_type_is_vector (rtype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Right operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (rtype));
      return NULL;
    }
  if (gsk_sl_type_is_scalar (ltype) && gsk_sl_type_is_vector (rtype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Right operand to shift cannot be a vector if left operand is a scalar.");
      return NULL;
    }
  if (gsk_sl_type_is_vector (ltype) && gsk_sl_type_is_vector (rtype) &&
      gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Vector operands do not have the same length.");
      return NULL;
    }

  if (gsk_sl_type_is_vector (ltype))
    return ltype;
  else if (gsk_sl_type_is_vector (rtype))
    return gsk_sl_type_get_vector (gsk_sl_type_get_scalar_type (ltype), gsk_sl_type_get_length (rtype));
  else
    return ltype;
}

static GskSlType *
gsk_sl_relational_check_type (GskSlPreprocessor *preproc,
                              GskSlType         *ltype,
                              GskSlType         *rtype)
{
  if (!gsk_sl_type_is_scalar (ltype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Left operand to relational operator is not a scalar.");
      return NULL;
    }
  if (gsk_sl_type_get_scalar_type (ltype) == GSK_SL_BOOL)
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Left operand to relational operator must not be bool.");
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (rtype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Right operand to relational operator is not a scalar.");
      return NULL;
    }
  if (gsk_sl_type_get_scalar_type (rtype) == GSK_SL_BOOL)
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Right operand to relational operator must not be bool.");
      return NULL;
    }

  return gsk_sl_type_get_scalar (GSK_SL_BOOL);
}

static GskSlType *
gsk_sl_equal_check_type (GskSlPreprocessor *preproc,
                         GskSlType         *ltype,
                         GskSlType         *rtype)
{
  if (gsk_sl_type_can_convert (ltype, rtype))
    return gsk_sl_type_get_scalar (GSK_SL_BOOL);
  if (gsk_sl_type_can_convert (rtype, ltype))
    return gsk_sl_type_get_scalar (GSK_SL_BOOL);

  gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Cannot convert %s and %s to the same type for comparison.",
                             gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
  return NULL;
}

static GskSlType *
gsk_sl_logical_check_type (GskSlPreprocessor *preproc,
                           GskSlType         *ltype,
                           GskSlType         *rtype)
{
  GskSlType *bool_type = gsk_sl_type_get_scalar (GSK_SL_BOOL);

  if (!gsk_sl_type_equal (bool_type, ltype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Left operand of logical operation is not bool, but %s",
                                 gsk_sl_type_get_name (ltype));
      return NULL;
    }
  if (!gsk_sl_type_equal (bool_type, rtype))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Right operand of logical operation is not bool, but %s",
                                 gsk_sl_type_get_name (rtype));
      return NULL;
    }

  return bool_type;
}

/* UNIMPLEMENTED */

static GskSlValue *
gsk_sl_unimplemented_get_constant (GskSlType  *type,
                                   GskSlValue *lvalue,
                                   GskSlValue *rvalue)
{
  g_assert_not_reached ();

  return NULL;
}

static guint32
gsk_sl_unimplemented_write_spv (GskSpvWriter *writer,
                                GskSlType    *type,
                                GskSlType    *ltype,
                                guint32       left_id,
                                GskSlType    *rtype,
                                guint32       right_id)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlBinary GSK_SL_BINARY_MODULO = {
  "%",
  gsk_sl_bitwise_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_LSHIFT = {
  "<<",
  gsk_sl_shift_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_RSHIFT = {
  ">>",
  gsk_sl_shift_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_LESS = {
  "<",
  gsk_sl_relational_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_GREATER = {
  ">",
  gsk_sl_relational_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_LESS_EQUAL = {
  "<=",
  gsk_sl_relational_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_GREATER_EQUAL = {
  ">=",
  gsk_sl_relational_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_EQUAL = {
  "==",
  gsk_sl_equal_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_NOT_EQUAL = {
  "!=",
  gsk_sl_equal_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_AND = {
  "&",
  gsk_sl_bitwise_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_XOR = {
  "^",
  gsk_sl_bitwise_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_OR = {
  "|",
  gsk_sl_bitwise_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_LOGICAL_AND = {
  "&&",
  gsk_sl_logical_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_LOGICAL_XOR = {
  "^^",
  gsk_sl_logical_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

static const GskSlBinary GSK_SL_BINARY_LOGICAL_OR = {
  "||",
  gsk_sl_logical_check_type,
  gsk_sl_unimplemented_get_constant,
  gsk_sl_unimplemented_write_spv
};

/* API */

const char *
gsk_sl_binary_get_sign (const GskSlBinary *binary)
{
  return binary->sign;
}

GskSlType *
gsk_sl_binary_check_type (const GskSlBinary *binary,
                          GskSlPreprocessor *preproc,
                          GskSlType         *ltype,
                          GskSlType         *rtype)
{
  return binary->check_type (preproc, ltype, rtype);
}

GskSlValue *
gsk_sl_binary_get_constant (const GskSlBinary *binary,
                            GskSlType         *type,
                            GskSlValue        *lvalue,
                            GskSlValue        *rvalue)
{
  return binary->get_constant (type, lvalue, rvalue);
}

guint32
gsk_sl_binary_write_spv (const GskSlBinary *binary,
                         GskSpvWriter      *writer,
                         GskSlType         *type,
                         GskSlType         *ltype,
                         guint32            left_id,
                         GskSlType         *rtype,
                         guint32            right_id)
{
  return binary->write_spv (writer, type, ltype, left_id, rtype, right_id);
}

const GskSlBinary *
gsk_sl_binary_get_for_token (GskSlTokenType token)
{
  switch ((guint) token)
  {
    case GSK_SL_TOKEN_STAR:
    case GSK_SL_TOKEN_MUL_ASSIGN:
      return &GSK_SL_BINARY_MULTIPLICATION;

    case GSK_SL_TOKEN_SLASH:
    case GSK_SL_TOKEN_DIV_ASSIGN:
      return &GSK_SL_BINARY_DIVISION;

    case GSK_SL_TOKEN_PERCENT:
    case GSK_SL_TOKEN_MOD_ASSIGN:
      return &GSK_SL_BINARY_MODULO;

    case GSK_SL_TOKEN_PLUS:
    case GSK_SL_TOKEN_ADD_ASSIGN:
      return &GSK_SL_BINARY_ADDITION;

    case GSK_SL_TOKEN_DASH:
    case GSK_SL_TOKEN_SUB_ASSIGN:
      return &GSK_SL_BINARY_SUBTRACTION;

    case GSK_SL_TOKEN_LEFT_OP:
    case GSK_SL_TOKEN_LEFT_ASSIGN:
      return &GSK_SL_BINARY_LSHIFT;

    case GSK_SL_TOKEN_RIGHT_OP:
    case GSK_SL_TOKEN_RIGHT_ASSIGN:
      return &GSK_SL_BINARY_RSHIFT;

    case GSK_SL_TOKEN_LEFT_ANGLE:
      return &GSK_SL_BINARY_LESS;

    case GSK_SL_TOKEN_RIGHT_ANGLE:
      return &GSK_SL_BINARY_GREATER;

    case GSK_SL_TOKEN_LE_OP:
      return &GSK_SL_BINARY_LESS_EQUAL;

    case GSK_SL_TOKEN_GE_OP:
      return &GSK_SL_BINARY_GREATER_EQUAL;

    case GSK_SL_TOKEN_EQ_OP:
      return &GSK_SL_BINARY_EQUAL;

    case GSK_SL_TOKEN_NE_OP:
      return &GSK_SL_BINARY_NOT_EQUAL;

    case GSK_SL_TOKEN_AMPERSAND:
    case GSK_SL_TOKEN_AND_ASSIGN:
      return &GSK_SL_BINARY_AND;

    case GSK_SL_TOKEN_CARET:
    case GSK_SL_TOKEN_XOR_ASSIGN:
      return &GSK_SL_BINARY_XOR;

    case GSK_SL_TOKEN_VERTICAL_BAR:
    case GSK_SL_TOKEN_OR_ASSIGN:
      return &GSK_SL_BINARY_OR;

    case GSK_SL_TOKEN_AND_OP:
      return &GSK_SL_BINARY_LOGICAL_AND;

    case GSK_SL_TOKEN_XOR_OP:
      return &GSK_SL_BINARY_LOGICAL_XOR;

    case GSK_SL_TOKEN_OR_OP:
      return &GSK_SL_BINARY_LOGICAL_OR;

    default:
      return NULL;
  }
}
