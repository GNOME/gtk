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

/* API */

const char *
gsk_sl_binary_get_sign (const GskSlBinary *binary)
{
  return binary->sign;
}

GskSlType *
gsk_sl_binary_check_type (const GskSlBinary *binary,
                          GskSlPreprocessor *stream,
                          GskSlType         *ltype,
                          GskSlType         *rtype)
{
  return binary->check_type (stream, ltype, rtype);
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

    case GSK_SL_TOKEN_DIV_ASSIGN:
    case GSK_SL_TOKEN_MOD_ASSIGN:
    case GSK_SL_TOKEN_ADD_ASSIGN:
    case GSK_SL_TOKEN_SUB_ASSIGN:
    case GSK_SL_TOKEN_LEFT_ASSIGN:
    case GSK_SL_TOKEN_RIGHT_ASSIGN:
    case GSK_SL_TOKEN_AND_ASSIGN:
    case GSK_SL_TOKEN_XOR_ASSIGN:
    case GSK_SL_TOKEN_OR_ASSIGN:
    default:
      return NULL;
  }
}
