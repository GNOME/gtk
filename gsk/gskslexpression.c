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

#include "gskslexpressionprivate.h"

#include "gskslpreprocessorprivate.h"
#include "gskslfunctionprivate.h"
#include "gskslpointertypeprivate.h"
#include "gskslprinterprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskslvariableprivate.h"
#include "gskspvwriterprivate.h"

#include <string.h>

typedef struct _GskSlExpressionClass GskSlExpressionClass;

struct _GskSlExpression {
  const GskSlExpressionClass *class;
  guint ref_count;
};

struct _GskSlExpressionClass {
  void                  (* free)                                (GskSlExpression        *expression);

  void                  (* print)                               (const GskSlExpression  *expression,
                                                                 GskSlPrinter           *printer);
  GskSlType *           (* get_return_type)                     (const GskSlExpression  *expression);
  GskSlValue *          (* get_constant)                        (const GskSlExpression  *expression);
  guint32               (* write_spv)                           (const GskSlExpression  *expression,
                                                                 GskSpvWriter           *writer);
};

static GskSlExpression *
gsk_sl_expression_alloc (const GskSlExpressionClass *klass,
                         gsize                 size)
{
  GskSlExpression *expression;

  expression = g_slice_alloc0 (size);

  expression->class = klass;
  expression->ref_count = 1;

  return expression;
}
#define gsk_sl_expression_new(_name, _klass) ((_name *) gsk_sl_expression_alloc ((_klass), sizeof (_name)))

/* ASSIGNMENT */

typedef struct _GskSlExpressionAssignment GskSlExpressionAssignment;

struct _GskSlExpressionAssignment {
  GskSlExpression parent;

  GskSlTokenType op;
  GskSlExpression *lvalue;
  GskSlExpression *rvalue;
};

static void
gsk_sl_expression_assignment_free (GskSlExpression *expression)
{
  GskSlExpressionAssignment *assignment = (GskSlExpressionAssignment *) expression;

  gsk_sl_expression_unref (assignment->lvalue);
  if (assignment->rvalue)
    gsk_sl_expression_unref (assignment->rvalue);

  g_slice_free (GskSlExpressionAssignment, assignment);
}

static void
gsk_sl_expression_assignment_print (const GskSlExpression *expression,
                                    GskSlPrinter          *printer)
{
  const GskSlExpressionAssignment *assignment = (const GskSlExpressionAssignment *) expression;

  gsk_sl_expression_print (assignment->lvalue, printer);

  switch ((guint) assignment->op)
  {
    case GSK_SL_TOKEN_EQUAL:
      gsk_sl_printer_append (printer, " = ");
      break;
    case GSK_SL_TOKEN_MUL_ASSIGN:
      gsk_sl_printer_append (printer, " *= ");
      break;
    case GSK_SL_TOKEN_DIV_ASSIGN:
      gsk_sl_printer_append (printer, " /= ");
      break;
    case GSK_SL_TOKEN_MOD_ASSIGN:
      gsk_sl_printer_append (printer, " %= ");
      break;
    case GSK_SL_TOKEN_ADD_ASSIGN:
      gsk_sl_printer_append (printer, " += ");
      break;
    case GSK_SL_TOKEN_SUB_ASSIGN:
      gsk_sl_printer_append (printer, " -= ");
      break;
    case GSK_SL_TOKEN_LEFT_ASSIGN:
      gsk_sl_printer_append (printer, " <<= ");
      break;
    case GSK_SL_TOKEN_RIGHT_ASSIGN:
      gsk_sl_printer_append (printer, " >>= ");
      break;
    case GSK_SL_TOKEN_AND_ASSIGN:
      gsk_sl_printer_append (printer, " &= ");
      break;
    case GSK_SL_TOKEN_XOR_ASSIGN:
      gsk_sl_printer_append (printer, " ^= ");
      break;
    case GSK_SL_TOKEN_OR_ASSIGN:
      gsk_sl_printer_append (printer, " |= ");
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  gsk_sl_expression_print (assignment->rvalue, printer);
}

static GskSlType *
gsk_sl_expression_assignment_get_return_type (const GskSlExpression *expression)
{
  const GskSlExpressionAssignment *assignment = (const GskSlExpressionAssignment *) expression;

  return gsk_sl_expression_get_return_type (assignment->lvalue);
}

static GskSlValue *
gsk_sl_expression_assignment_get_constant (const GskSlExpression *expression)
{
  return NULL;
}

static guint32
gsk_sl_expression_assignment_write_spv (const GskSlExpression *expression,
                                        GskSpvWriter    *writer)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_ASSIGNMENT = {
  gsk_sl_expression_assignment_free,
  gsk_sl_expression_assignment_print,
  gsk_sl_expression_assignment_get_return_type,
  gsk_sl_expression_assignment_get_constant,
  gsk_sl_expression_assignment_write_spv
};

/* MULTIPLICATION */

typedef struct _GskSlExpressionMultiplication GskSlExpressionMultiplication;

struct _GskSlExpressionMultiplication {
  GskSlExpression parent;

  GskSlType *type;
  GskSlExpression *left;
  GskSlExpression *right;
};

static void
gsk_sl_expression_multiplication_free (GskSlExpression *expression)
{
  GskSlExpressionMultiplication *multiplication = (GskSlExpressionMultiplication *) expression;

  gsk_sl_expression_unref (multiplication->left);
  gsk_sl_expression_unref (multiplication->right);
  gsk_sl_type_unref (multiplication->type);

  g_slice_free (GskSlExpressionMultiplication, multiplication);
}

static void
gsk_sl_expression_multiplication_print (const GskSlExpression *expression,
                                        GskSlPrinter          *printer)
{
  GskSlExpressionMultiplication *multiplication = (GskSlExpressionMultiplication *) expression;

  gsk_sl_expression_print (multiplication->left, printer);
  gsk_sl_printer_append (printer, " * ");
  gsk_sl_expression_print (multiplication->right, printer);
}

static GskSlType *
gsk_sl_expression_multiplication_get_result_type (GskSlPreprocessor *preproc,
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

static GskSlType *
gsk_sl_expression_multiplication_get_return_type (const GskSlExpression *expression)
{
  GskSlExpressionMultiplication *multiplication = (GskSlExpressionMultiplication *) expression;

  return multiplication->type;
}

#define GSK_SL_OPERATION_FUNC_SCALAR(func,type,...) \
static void \
func (gpointer value, gpointer scalar) \
{ \
  type x = *(type *) value; \
  type y = *(type *) scalar; \
  __VA_ARGS__ \
  *(type *) value = x; \
}
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_multiplication_int, gint32, x *= y;)
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_multiplication_uint, guint32, x *= y;)
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_multiplication_float, float, x *= y;)
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_multiplication_double, double, x *= y;)
static void (* mult_funcs[]) (gpointer, gpointer) = {
  [GSK_SL_INT] = gsk_sl_expression_multiplication_int,
  [GSK_SL_UINT] = gsk_sl_expression_multiplication_uint,
  [GSK_SL_FLOAT] = gsk_sl_expression_multiplication_float,
  [GSK_SL_DOUBLE] = gsk_sl_expression_multiplication_double,
};

GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_division_int, gint32, x = y == 0 ? G_MAXINT32 : x / y;)
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_division_uint, guint32, x = y == 0 ? G_MAXUINT32 : x / y;)
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_division_float, float, x /= y;)
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_division_double, double, x /= y;)
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_division_int_inv, gint32, x = x == 0 ? G_MAXINT32 : y / x;)
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_division_uint_inv, guint32, x = x == 0 ? G_MAXUINT32 : y / x;)
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_division_float_inv, float, x = y / x;)
GSK_SL_OPERATION_FUNC_SCALAR(gsk_sl_expression_division_double_inv, double, x = y / x;)
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
gsk_sl_expression_multiplication_get_constant (const GskSlExpression *expression)
{
  const GskSlExpressionMultiplication *multiplication = (const GskSlExpressionMultiplication *) expression;
  GskSlValue *result, *lvalue, *rvalue;
  GskSlType *ltype, *rtype;
  GskSlScalarType scalar;

  scalar = gsk_sl_type_get_scalar_type (multiplication->type);
  lvalue = gsk_sl_expression_get_constant (multiplication->left);
  if (lvalue == NULL)
    return NULL;
  rvalue = gsk_sl_expression_get_constant (multiplication->right);
  if (rvalue == NULL)
    {
      gsk_sl_value_free (lvalue);
      return NULL;
    }
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

      result = gsk_sl_value_new (multiplication->type);
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
      if (gsk_sl_type_get_scalar_type (multiplication->type) == GSK_SL_DOUBLE)
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
gsk_sl_expression_multiplication_write_spv (const GskSlExpression *expression,
                                            GskSpvWriter          *writer)
{
  const GskSlExpressionMultiplication *multiplication = (const GskSlExpressionMultiplication *) expression;
  GskSlType *ltype, *rtype;
  guint32 left_id, right_id, result_id, result_type_id;

  ltype = gsk_sl_expression_get_return_type (multiplication->left);
  rtype = gsk_sl_expression_get_return_type (multiplication->right);

  left_id = gsk_sl_expression_write_spv (multiplication->left, writer);
  if (gsk_sl_type_get_scalar_type (ltype) != gsk_sl_type_get_scalar_type (multiplication->type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (ltype, gsk_sl_type_get_scalar_type (multiplication->type));
      left_id = gsk_spv_writer_add_conversion (writer, left_id, ltype, new_type);
      ltype = new_type;
    }
  right_id = gsk_sl_expression_write_spv (multiplication->right, writer);
  if (gsk_sl_type_get_scalar_type (rtype) != gsk_sl_type_get_scalar_type (multiplication->type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (rtype, gsk_sl_type_get_scalar_type (multiplication->type));
      right_id = gsk_spv_writer_add_conversion (writer, right_id, rtype, new_type);
      rtype = new_type;
    }

  result_type_id = gsk_spv_writer_get_id_for_type (writer, multiplication->type);
  result_id = gsk_spv_writer_next_id (writer);

  if (gsk_sl_type_is_matrix (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_MATRIX_TIMES_MATRIX,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             left_id,
                                             right_id });

          return result_id;
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_VECTOR_TIMES_MATRIX,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             right_id,
                                             left_id });

          return result_id;
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_MATRIX_TIMES_SCALAR,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             left_id,
                                             right_id });

          return result_id;
        }
    }
  else if (gsk_sl_type_is_vector (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_MATRIX_TIMES_VECTOR,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             right_id,
                                             left_id });

          return result_id;
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          switch (gsk_sl_type_get_scalar_type (multiplication->type))
            {
            case GSK_SL_FLOAT:
            case GSK_SL_DOUBLE:
              gsk_spv_writer_add (writer,
                                  GSK_SPV_WRITER_SECTION_CODE,
                                  5, GSK_SPV_OP_F_MUL,
                                  (guint32[4]) { result_type_id,
                                                 result_id,
                                                 left_id,
                                                 right_id });
              break;
            case GSK_SL_INT:
            case GSK_SL_UINT:
              gsk_spv_writer_add (writer,
                                  GSK_SPV_WRITER_SECTION_CODE,
                                  5, GSK_SPV_OP_I_MUL,
                                  (guint32[4]) { result_type_id,
                                                 result_id,
                                                 left_id,
                                                 right_id });
              break;
            case GSK_SL_VOID:
            case GSK_SL_BOOL:
            default:
              g_assert_not_reached ();
              break;
            }

          return result_id;
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_VECTOR_TIMES_SCALAR,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             left_id,
                                             right_id });

          return result_id;
        }
    }
  else if (gsk_sl_type_is_scalar (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_MATRIX_TIMES_SCALAR,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             right_id,
                                             left_id });

          return result_id;
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_VECTOR_TIMES_SCALAR,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             right_id,
                                             left_id });

          return result_id;
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          switch (gsk_sl_type_get_scalar_type (multiplication->type))
            {
            case GSK_SL_FLOAT:
            case GSK_SL_DOUBLE:
              gsk_spv_writer_add (writer,
                                  GSK_SPV_WRITER_SECTION_CODE,
                                  5, GSK_SPV_OP_F_MUL,
                                  (guint32[4]) { result_type_id,
                                                 result_id,
                                                 left_id,
                                                 right_id });
              break;
            case GSK_SL_INT:
            case GSK_SL_UINT:
              gsk_spv_writer_add (writer,
                                  GSK_SPV_WRITER_SECTION_CODE,
                                  5, GSK_SPV_OP_I_MUL,
                                  (guint32[4]) { result_type_id,
                                                 result_id,
                                                 left_id,
                                                 right_id });
              break;
            case GSK_SL_VOID:
            case GSK_SL_BOOL:
            default:
              g_assert_not_reached ();
              break;
            }

          return result_id;
        }
    }

  g_assert_not_reached ();

  return 0;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_MULTIPLICATION = {
  gsk_sl_expression_multiplication_free,
  gsk_sl_expression_multiplication_print,
  gsk_sl_expression_multiplication_get_return_type,
  gsk_sl_expression_multiplication_get_constant,
  gsk_sl_expression_multiplication_write_spv
};

/* ARITHMETIC */

static GskSlType *
gsk_sl_expression_arithmetic_type_check (GskSlPreprocessor *preproc,
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

typedef struct _GskSlExpressionArithmetic GskSlExpressionArithmetic;

struct _GskSlExpressionArithmetic {
  GskSlExpression parent;

  GskSlType *type;
  GskSlExpression *left;
  GskSlExpression *right;
};

static void
gsk_sl_expression_arithmetic_free (GskSlExpression *expression)
{
  GskSlExpressionArithmetic *arithmetic = (GskSlExpressionArithmetic *) expression;

  gsk_sl_type_unref (arithmetic->type);
  gsk_sl_expression_unref (arithmetic->left);
  gsk_sl_expression_unref (arithmetic->right);

  g_slice_free (GskSlExpressionArithmetic, arithmetic);
}

static void
gsk_sl_expression_division_print (const GskSlExpression *expression,
                                  GskSlPrinter          *printer)
{
  GskSlExpressionArithmetic *arithmetic = (GskSlExpressionArithmetic *) expression;

  gsk_sl_expression_print (arithmetic->left, printer);
  gsk_sl_printer_append (printer, " / ");
  gsk_sl_expression_print (arithmetic->right, printer);
}

static void
gsk_sl_expression_addition_print (const GskSlExpression *expression,
                                  GskSlPrinter          *printer)
{
  GskSlExpressionArithmetic *arithmetic = (GskSlExpressionArithmetic *) expression;

  gsk_sl_expression_print (arithmetic->left, printer);
  gsk_sl_printer_append (printer, " + ");
  gsk_sl_expression_print (arithmetic->right, printer);
}

static void
gsk_sl_expression_subtraction_print (const GskSlExpression *expression,
                                     GskSlPrinter          *printer)
{
  GskSlExpressionArithmetic *arithmetic = (GskSlExpressionArithmetic *) expression;

  gsk_sl_expression_print (arithmetic->left, printer);
  gsk_sl_printer_append (printer, " - ");
  gsk_sl_expression_print (arithmetic->right, printer);
}

static GskSlType *
gsk_sl_expression_arithmetic_get_return_type (const GskSlExpression *expression)
{
  GskSlExpressionArithmetic *arithmetic = (GskSlExpressionArithmetic *) expression;

  return arithmetic->type;
}

static GskSlValue *
gsk_sl_expression_division_get_constant (const GskSlExpression *expression)
{
  const GskSlExpressionArithmetic *arithmetic = (const GskSlExpressionArithmetic *) expression;
  GskSlValue *result, *lvalue, *rvalue;
  GskSlType *ltype, *rtype;
  GskSlScalarType scalar;
  gsize ln, rn;

  scalar = gsk_sl_type_get_scalar_type (arithmetic->type);
  lvalue = gsk_sl_expression_get_constant (arithmetic->left);
  if (lvalue == NULL)
    return NULL;
  rvalue = gsk_sl_expression_get_constant (arithmetic->right);
  if (rvalue == NULL)
    {
      gsk_sl_value_free (lvalue);
      return NULL;
    }
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
gsk_sl_expression_division_write_spv (const GskSlExpression *expression,
                                      GskSpvWriter          *writer)
{
  const GskSlExpressionArithmetic *arithmetic = (const GskSlExpressionArithmetic *) expression;
  GskSlType *ltype, *rtype;
  guint32 left_id, right_id, result_id, result_type_id;

  ltype = gsk_sl_expression_get_return_type (arithmetic->left);
  rtype = gsk_sl_expression_get_return_type (arithmetic->right);

  left_id = gsk_sl_expression_write_spv (arithmetic->left, writer);
  if (gsk_sl_type_get_scalar_type (ltype) != gsk_sl_type_get_scalar_type (arithmetic->type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (ltype, gsk_sl_type_get_scalar_type (arithmetic->type));
      left_id = gsk_spv_writer_add_conversion (writer, left_id, ltype, new_type);
      ltype = new_type;
    }
  right_id = gsk_sl_expression_write_spv (arithmetic->right, writer);
  if (gsk_sl_type_get_scalar_type (rtype) != gsk_sl_type_get_scalar_type (arithmetic->type))
    {
      GskSlType *new_type = gsk_sl_type_get_matching (rtype, gsk_sl_type_get_scalar_type (arithmetic->type));
      right_id = gsk_spv_writer_add_conversion (writer, right_id, rtype, new_type);
      rtype = new_type;
    }

  result_type_id = gsk_spv_writer_get_id_for_type (writer, arithmetic->type);

  if (gsk_sl_type_is_matrix (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          gsize cols = gsk_sl_type_get_length (ltype);
          gsize c;
          guint32 col_type_id, left_part_id, right_part_id, ids[cols + 2];

          col_type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_index_type (ltype));
          for (c = 0; c < cols; c++)
            {
              left_part_id = gsk_spv_writer_next_id (writer);
              gsk_spv_writer_add (writer,
                                  GSK_SPV_WRITER_SECTION_CODE,
                                  5, GSK_SPV_OP_COMPOSITE_EXTRACT,
                                  (guint32[4]) { col_type_id,
                                                 left_part_id,
                                                 left_id,
                                                 c });
              right_part_id = gsk_spv_writer_next_id (writer);
              gsk_spv_writer_add (writer,
                                  GSK_SPV_WRITER_SECTION_CODE,
                                  5, GSK_SPV_OP_COMPOSITE_EXTRACT,
                                  (guint32[4]) { col_type_id,
                                                 right_part_id,
                                                 right_id,
                                                 c });
              ids[2 + c] = gsk_spv_writer_next_id (writer);
              gsk_spv_writer_add (writer,
                                  GSK_SPV_WRITER_SECTION_CODE,
                                  5, GSK_SPV_OP_F_DIV,
                                  (guint32[4]) { col_type_id,
                                                 ids[2 + c],
                                                 left_part_id,
                                                 right_part_id });
            }

          ids[0] = result_type_id;
          ids[1] = gsk_spv_writer_next_id (writer);
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              3 + cols, GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                              ids);

          return ids[1];
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          guint32 tmp_id, one_id, rtype_id;

          rtype_id = gsk_spv_writer_get_id_for_type (writer, rtype);
          one_id = gsk_spv_writer_get_id_for_one (writer, gsk_sl_type_get_scalar_type (arithmetic->type));
          tmp_id = gsk_spv_writer_next_id (writer);
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_F_DIV,
                              (guint32[4]) { rtype_id,
                                             tmp_id,
                                             one_id,
                                             right_id });

          result_id = gsk_spv_writer_next_id (writer);
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_MATRIX_TIMES_SCALAR,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             left_id,
                                             tmp_id });

          return result_id;
        }
      else
        {
          g_assert_not_reached ();
          return 0;
        }
    }
  else if (gsk_sl_type_is_matrix (rtype))
    {
      guint32 tmp_id, one_id, ltype_id;

      ltype_id = gsk_spv_writer_get_id_for_type (writer, ltype);
      one_id = gsk_spv_writer_get_id_for_one (writer, gsk_sl_type_get_scalar_type (arithmetic->type));
      tmp_id = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          5, GSK_SPV_OP_F_DIV,
                          (guint32[4]) { ltype_id,
                                         tmp_id,
                                         one_id,
                                         left_id });

      result_id = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          5, GSK_SPV_OP_MATRIX_TIMES_SCALAR,
                          (guint32[4]) { result_type_id,
                                         result_id,
                                         right_id,
                                         tmp_id });

      return result_id;
    }
  else
    {
      /* ltype and rtype are not matices */

      if (gsk_sl_type_is_scalar (ltype) && gsk_sl_type_is_vector (rtype))
        {
           guint32 tmp_id = gsk_spv_writer_next_id (writer);
           gsk_spv_writer_add (writer,
                               GSK_SPV_WRITER_SECTION_CODE,
                               3 + gsk_sl_type_get_length (rtype), GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                              (guint32[6]) { result_type_id,
                                             tmp_id,
                                             left_id, left_id, left_id, left_id });
           left_id = tmp_id;
        }
      else if (gsk_sl_type_is_scalar (rtype) && gsk_sl_type_is_vector (ltype))
        {
           guint32 tmp_id = gsk_spv_writer_next_id (writer);
           gsk_spv_writer_add (writer,
                               GSK_SPV_WRITER_SECTION_CODE,
                               3 + gsk_sl_type_get_length (ltype), GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                              (guint32[6]) { result_type_id,
                                             tmp_id,
                                             right_id, right_id, right_id, right_id });
           right_id = tmp_id;
        }

      /* ltype and rtype have the same number of components now */

      result_id = gsk_spv_writer_next_id (writer);
      switch (gsk_sl_type_get_scalar_type (arithmetic->type))
        {
        case GSK_SL_FLOAT:
        case GSK_SL_DOUBLE:
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_F_DIV,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             left_id,
                                             right_id });
          break;
        case GSK_SL_INT:
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_S_DIV,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             left_id,
                                             right_id });
          break;
        case GSK_SL_UINT:
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5, GSK_SPV_OP_U_DIV,
                              (guint32[4]) { result_type_id,
                                             result_id,
                                             left_id,
                                             right_id });
          break;
        case GSK_SL_VOID:
        case GSK_SL_BOOL:
        default:
          g_assert_not_reached ();
          break;
        }

      return result_id;
    }
}

static GskSlValue *
gsk_sl_expression_arithmetic_get_constant (const GskSlExpression *expression)
{
  //GskSlExpressionArithmetic *arithmetic = (const GskSlExpressionArithmetic *) expression;

  /* FIXME: These need constant evaluations */
  return NULL;
}

static guint32
gsk_sl_expression_arithmetic_write_spv (const GskSlExpression *expression,
                                        GskSpvWriter          *writer)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_DIVISION = {
  gsk_sl_expression_arithmetic_free,
  gsk_sl_expression_division_print,
  gsk_sl_expression_arithmetic_get_return_type,
  gsk_sl_expression_division_get_constant,
  gsk_sl_expression_division_write_spv
};

static const GskSlExpressionClass GSK_SL_EXPRESSION_ADDITION = {
  gsk_sl_expression_arithmetic_free,
  gsk_sl_expression_addition_print,
  gsk_sl_expression_arithmetic_get_return_type,
  gsk_sl_expression_arithmetic_get_constant,
  gsk_sl_expression_arithmetic_write_spv
};

static const GskSlExpressionClass GSK_SL_EXPRESSION_SUBTRACTION = {
  gsk_sl_expression_arithmetic_free,
  gsk_sl_expression_subtraction_print,
  gsk_sl_expression_arithmetic_get_return_type,
  gsk_sl_expression_arithmetic_get_constant,
  gsk_sl_expression_arithmetic_write_spv
};

/* OPERATION */

typedef enum {
  GSK_SL_OPERATION_MOD,
  GSK_SL_OPERATION_LSHIFT,
  GSK_SL_OPERATION_RSHIFT,
  GSK_SL_OPERATION_LESS,
  GSK_SL_OPERATION_GREATER,
  GSK_SL_OPERATION_LESS_EQUAL,
  GSK_SL_OPERATION_GREATER_EQUAL,
  GSK_SL_OPERATION_EQUAL,
  GSK_SL_OPERATION_NOT_EQUAL,
  GSK_SL_OPERATION_AND,
  GSK_SL_OPERATION_XOR,
  GSK_SL_OPERATION_OR,
  GSK_SL_OPERATION_LOGICAL_AND,
  GSK_SL_OPERATION_LOGICAL_XOR,
  GSK_SL_OPERATION_LOGICAL_OR
} GskSlOperation;

typedef struct _GskSlExpressionOperation GskSlExpressionOperation;

struct _GskSlExpressionOperation {
  GskSlExpression parent;

  GskSlOperation op;
  GskSlExpression *left;
  GskSlExpression *right;
};

static void
gsk_sl_expression_operation_free (GskSlExpression *expression)
{
  GskSlExpressionOperation *operation = (GskSlExpressionOperation *) expression;

  gsk_sl_expression_unref (operation->left);
  if (operation->right)
    gsk_sl_expression_unref (operation->right);

  g_slice_free (GskSlExpressionOperation, operation);
}

static void
gsk_sl_expression_operation_print (const GskSlExpression *expression,
                                   GskSlPrinter          *printer)
{
  const char *op_str[] = {
    [GSK_SL_OPERATION_MOD] = " % ",
    [GSK_SL_OPERATION_LSHIFT] = " << ",
    [GSK_SL_OPERATION_RSHIFT] = " >> ",
    [GSK_SL_OPERATION_LESS] = " < ",
    [GSK_SL_OPERATION_GREATER] = " > ",
    [GSK_SL_OPERATION_LESS_EQUAL] = " <= ",
    [GSK_SL_OPERATION_GREATER_EQUAL] = " >= ",
    [GSK_SL_OPERATION_EQUAL] = " == ",
    [GSK_SL_OPERATION_NOT_EQUAL] = " != ",
    [GSK_SL_OPERATION_AND] = " & ",
    [GSK_SL_OPERATION_XOR] = " ^ ",
    [GSK_SL_OPERATION_OR] = " | ",
    [GSK_SL_OPERATION_LOGICAL_AND] = " && ",
    [GSK_SL_OPERATION_LOGICAL_XOR] = " ^^ ",
    [GSK_SL_OPERATION_LOGICAL_OR] = " || "
  };
  GskSlExpressionOperation *operation = (GskSlExpressionOperation *) expression;

  /* XXX: figure out the need for bracketing here */

  gsk_sl_expression_print (operation->left, printer);
  gsk_sl_printer_append (printer, op_str[operation->op]);
  gsk_sl_expression_print (operation->right, printer);
}

static GskSlType *
gsk_sl_expression_bitwise_type_check (GskSlPreprocessor *stream,
                                      GskSlType         *ltype,
                                      GskSlType         *rtype)
{
  GskSlScalarType lscalar, rscalar;

  lscalar = gsk_sl_type_get_scalar_type (ltype);
  if (lscalar != GSK_SL_INT && lscalar != GSK_SL_UINT)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Left operand %s is not an integer type.", gsk_sl_type_get_name (ltype));
      return NULL;
    }
  rscalar = gsk_sl_type_get_scalar_type (ltype);
  if (rscalar != GSK_SL_INT && rscalar != GSK_SL_UINT)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Right operand %s is not an integer type.", gsk_sl_type_get_name (rtype));
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (ltype) && !gsk_sl_type_is_vector (ltype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Left operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (ltype));
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (rtype) && !gsk_sl_type_is_vector (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Right operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (rtype));
      return NULL;
    }
  if (gsk_sl_type_is_vector (ltype) && gsk_sl_type_is_vector (rtype) &&
      gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
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

static gboolean
gsk_sl_expression_shift_type_check (GskSlPreprocessor *stream,
                                    GskSlType         *ltype,
                                    GskSlType         *rtype)
{
  GskSlScalarType lscalar, rscalar;

  lscalar = gsk_sl_type_get_scalar_type (ltype);
  if (lscalar != GSK_SL_INT && lscalar != GSK_SL_UINT)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Left operand %s is not an integer type.", gsk_sl_type_get_name (ltype));
      return FALSE;
    }
  rscalar = gsk_sl_type_get_scalar_type (ltype);
  if (rscalar != GSK_SL_INT && rscalar != GSK_SL_UINT)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Right operand %s is not an integer type.", gsk_sl_type_get_name (rtype));
      return FALSE;
    }
  if (!gsk_sl_type_is_scalar (ltype) && !gsk_sl_type_is_vector (ltype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Left operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (ltype));
      return FALSE;
    }
  if (!gsk_sl_type_is_scalar (rtype) && !gsk_sl_type_is_vector (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Right operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (rtype));
      return FALSE;
    }
  if (gsk_sl_type_is_scalar (ltype) && gsk_sl_type_is_vector (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Right operand to shift cannot be a vector if left operand is a scalar.");
      return FALSE;
    }
  if (gsk_sl_type_is_vector (ltype) && gsk_sl_type_is_vector (rtype) &&
      gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Vector operands do not have the same length.");
      return FALSE;
    }

  return TRUE;
}

static gboolean
gsk_sl_expression_relational_type_check (GskSlPreprocessor *stream,
                                         GskSlType        *ltype,
                                         GskSlType        *rtype)
{
  if (!gsk_sl_type_is_scalar (ltype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Left operand to relational operator is not a scalar.");
      return FALSE;
    }
  if (gsk_sl_type_get_scalar_type (ltype) == GSK_SL_BOOL)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Left operand to relational operator must not be bool.");
      return FALSE;
    }
  if (!gsk_sl_type_is_scalar (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Right operand to relational operator is not a scalar.");
      return FALSE;
    }
  if (gsk_sl_type_get_scalar_type (rtype) == GSK_SL_BOOL)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Right operand to relational operator must not be bool.");
      return FALSE;
    }

  return TRUE;
}

static GskSlType *
gsk_sl_expression_operation_get_return_type (const GskSlExpression *expression)
{
  GskSlExpressionOperation *operation = (GskSlExpressionOperation *) expression;

  switch (operation->op)
  {
    case GSK_SL_OPERATION_LSHIFT:
    case GSK_SL_OPERATION_RSHIFT:
      return gsk_sl_expression_get_return_type (operation->left);
    case GSK_SL_OPERATION_MOD:
    case GSK_SL_OPERATION_AND:
    case GSK_SL_OPERATION_XOR:
    case GSK_SL_OPERATION_OR:
      return gsk_sl_expression_bitwise_type_check (NULL,
                                                   gsk_sl_expression_get_return_type (operation->left),
                                                   gsk_sl_expression_get_return_type (operation->right));
    case GSK_SL_OPERATION_LESS:
    case GSK_SL_OPERATION_GREATER:
    case GSK_SL_OPERATION_LESS_EQUAL:
    case GSK_SL_OPERATION_GREATER_EQUAL:
    case GSK_SL_OPERATION_EQUAL:
    case GSK_SL_OPERATION_NOT_EQUAL:
    case GSK_SL_OPERATION_LOGICAL_AND:
    case GSK_SL_OPERATION_LOGICAL_XOR:
    case GSK_SL_OPERATION_LOGICAL_OR:
      return gsk_sl_type_get_scalar (GSK_SL_BOOL);
    default:
      g_assert_not_reached ();
      return NULL;
  }
}

static GskSlValue *
gsk_sl_expression_operation_get_constant (const GskSlExpression *expression)
{
  //const GskSlExpressionOperation *operation = (const GskSlExpressionOperation *) expression;

  /* FIXME: These need constant evaluations */
  return NULL;
}

static guint32
gsk_sl_expression_operation_write_spv (const GskSlExpression *expression,
                                       GskSpvWriter          *writer)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_OPERATION = {
  gsk_sl_expression_operation_free,
  gsk_sl_expression_operation_print,
  gsk_sl_expression_operation_get_return_type,
  gsk_sl_expression_operation_get_constant,
  gsk_sl_expression_operation_write_spv
};

/* REFERENCE */

typedef struct _GskSlExpressionReference GskSlExpressionReference;

struct _GskSlExpressionReference {
  GskSlExpression parent;

  GskSlVariable *variable;
};

static void
gsk_sl_expression_reference_free (GskSlExpression *expression)
{
  GskSlExpressionReference *reference = (GskSlExpressionReference *) expression;

  gsk_sl_variable_unref (reference->variable);

  g_slice_free (GskSlExpressionReference, reference);
}

static void
gsk_sl_expression_reference_print (const GskSlExpression *expression,
                                   GskSlPrinter          *printer)
{
  const GskSlExpressionReference *reference = (const GskSlExpressionReference *) expression;

  gsk_sl_printer_append (printer, gsk_sl_variable_get_name (reference->variable));
}

static GskSlType *
gsk_sl_expression_reference_get_return_type (const GskSlExpression *expression)
{
  const GskSlExpressionReference *reference = (const GskSlExpressionReference *) expression;

  return gsk_sl_pointer_type_get_type (gsk_sl_variable_get_type (reference->variable));
}

static GskSlValue *
gsk_sl_expression_reference_get_constant (const GskSlExpression *expression)
{
  GskSlExpressionReference *reference = (GskSlExpressionReference *) expression;
  const GskSlValue *initial_value;

  if (!gsk_sl_variable_is_constant (reference->variable))
    return NULL;

  initial_value = gsk_sl_variable_get_initial_value (reference->variable);
  if (initial_value == NULL)
    return NULL;

  return gsk_sl_value_copy (initial_value);
}

static guint32
gsk_sl_expression_reference_write_spv (const GskSlExpression *expression,
                                       GskSpvWriter          *writer)
{
  GskSlExpressionReference *reference = (GskSlExpressionReference *) expression;
  guint32 declaration_id, result_id, type_id;

  type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_pointer_type_get_type (gsk_sl_variable_get_type (reference->variable)));
  declaration_id = gsk_spv_writer_get_id_for_variable (writer, reference->variable);
  result_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      4, GSK_SPV_OP_LOAD,
                      (guint32[3]) { type_id,
                                     result_id,
                                     declaration_id });

  return result_id;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_REFERENCE = {
  gsk_sl_expression_reference_free,
  gsk_sl_expression_reference_print,
  gsk_sl_expression_reference_get_return_type,
  gsk_sl_expression_reference_get_constant,
  gsk_sl_expression_reference_write_spv
};

/* CONSTRUCTOR CALL */

typedef struct _GskSlExpressionConstructor GskSlExpressionConstructor;

struct _GskSlExpressionConstructor {
  GskSlExpression parent;

  GskSlType *type;
  GskSlExpression **arguments;
  guint n_arguments;
};

static void
gsk_sl_expression_constructor_free (GskSlExpression *expression)
{
  GskSlExpressionConstructor *constructor = (GskSlExpressionConstructor *) expression;
  guint i;

  for (i = 0; i < constructor->n_arguments; i++)
    {
      gsk_sl_expression_unref (constructor->arguments[i]);
    }
  g_free (constructor->arguments);

  gsk_sl_type_unref (constructor->type);

  g_slice_free (GskSlExpressionConstructor, constructor);
}

static void
gsk_sl_expression_constructor_print (const GskSlExpression *expression,
                                       GskSlPrinter          *printer)
{
  const GskSlExpressionConstructor *constructor = (const GskSlExpressionConstructor *) expression;
  guint i;

  gsk_sl_printer_append (printer, gsk_sl_type_get_name (constructor->type));
  gsk_sl_printer_append (printer, " (");
  
  for (i = 0; i < constructor->n_arguments; i++)
    {
      if (i > 0)
        gsk_sl_printer_append (printer, ", ");
      gsk_sl_expression_print (constructor->arguments[i], printer);
    }

  gsk_sl_printer_append (printer, ")");
}

static GskSlType *
gsk_sl_expression_constructor_get_return_type (const GskSlExpression *expression)
{
  const GskSlExpressionConstructor *constructor = (const GskSlExpressionConstructor *) expression;

  return constructor->type;
}

static GskSlValue *
gsk_sl_expression_constructor_get_constant (const GskSlExpression *expression)
{
  const GskSlExpressionConstructor *constructor = (const GskSlExpressionConstructor *) expression;
  GskSlType *type = constructor->type;
  GskSlValue *values[constructor->n_arguments];
  GskSlValue *result;
  guint i;

  for (i = 0; i < constructor->n_arguments; i++)
    {
      values[i] = gsk_sl_expression_get_constant (constructor->arguments[i]);
      if (values[i] == NULL)
        {
          guint j;
          for (j = 0; j < i; j++)
            gsk_sl_value_free (values[j]);
          return NULL;
        }
    }

  result = gsk_sl_value_new (type);

  if (constructor->n_arguments == 1 && gsk_sl_type_is_scalar (gsk_sl_value_get_type (values[0])))
    {
      GskSlScalarType sscalar, dscalar;
      guchar *sdata, *ddata;
      gsize dstride;
      
      sscalar = gsk_sl_type_get_scalar_type (gsk_sl_value_get_type (values[0]));
      sdata = gsk_sl_value_get_data (values[0]);
      dscalar = gsk_sl_type_get_scalar_type (type);
      ddata = gsk_sl_value_get_data (result);
      dstride = gsk_sl_scalar_type_get_size (dscalar);

      if (gsk_sl_type_is_scalar (type))
        {
          gsk_sl_scalar_type_convert_value (dscalar, ddata, sscalar, sdata);
        }
      else if (gsk_sl_type_is_vector (type))
        {
          gsize i;
          for (i = 0; i < gsk_sl_type_get_n_components (type); i++)
            {
              gsk_sl_scalar_type_convert_value (dscalar, ddata + i * dstride, sscalar, sdata);
            }
        }
      else if (gsk_sl_type_is_matrix (type))
        {
          gsize i, n, step;

          n = gsk_sl_type_get_n_components (type);
          step = n / gsk_sl_type_get_length (type) + 1;
          for (i = 0; i < n; i += step)
            {
              gsk_sl_scalar_type_convert_value (dscalar, ddata + i * dstride, sscalar, sdata);
            }
        }
    }
  else if (constructor->n_arguments == 1 && gsk_sl_type_is_matrix (gsk_sl_value_get_type (values[0])) && gsk_sl_type_is_matrix (type))
    {
    }
  else
    {
      GskSlScalarType sscalar, dscalar;
      guchar *sdata, *ddata;
      gsize i, n, j, si, sn, sstride, dstride;
      
      dscalar = gsk_sl_type_get_scalar_type (type);
      dstride = gsk_sl_scalar_type_get_size (dscalar);
      n = gsk_sl_type_get_n_components (type);
      ddata = gsk_sl_value_get_data (result);
      sscalar = GSK_SL_VOID;
      sdata = NULL;
      sstride = 0;

      j = 0;
      sn = 0;
      si = 0;
      for (i = 0; i < n; i++)
        {
          if (si == sn)
            {
              sscalar = gsk_sl_type_get_scalar_type (gsk_sl_value_get_type (values[j]));
              sstride = gsk_sl_scalar_type_get_size (sscalar);
              sdata = gsk_sl_value_get_data (values[j]);
              si = 0;
              sn = gsk_sl_type_get_n_components (gsk_sl_value_get_type (values[j]));
              j++;
            }

          gsk_sl_scalar_type_convert_value (dscalar,
                                            ddata + dstride * i,
                                            sscalar,
                                            sdata + sstride * si);
          si++;
        }
    }

  for (i = 0; i < constructor->n_arguments; i++)
    gsk_sl_value_free (values[i]);

  return result;
}

static guint32
gsk_sl_expression_constructor_write_spv (const GskSlExpression *expression,
                                         GskSpvWriter          *writer)
{
  const GskSlExpressionConstructor *constructor = (const GskSlExpressionConstructor *) expression;
  GskSlType *type = constructor->type;
  GskSlType *value_type;
  guint32 value_id;

  value_type = gsk_sl_expression_get_return_type (constructor->arguments[0]);

  if (constructor->n_arguments == 1 && gsk_sl_type_is_scalar (value_type))
    {
      value_id = gsk_sl_expression_write_spv (constructor->arguments[0], writer);

      if (gsk_sl_type_is_scalar (type))
        {
          return gsk_spv_writer_add_conversion (writer, value_id, value_type, type);
        }
      else if (gsk_sl_type_is_vector (type))
        {
          GskSlType *scalar_type = gsk_sl_type_get_scalar (gsk_sl_type_get_scalar_type (type));
          guint32 scalar_id, result_type_id, result_id;

          scalar_id = gsk_spv_writer_add_conversion (writer, value_id, value_type, scalar_type);
          result_type_id = gsk_spv_writer_get_id_for_type (writer, type);
          result_id = gsk_spv_writer_next_id (writer);

          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              3 + gsk_sl_type_get_n_components (type), GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                              (guint32[6]) { result_type_id,
                                             result_id,
                                             scalar_id, scalar_id, scalar_id, scalar_id });

          return result_id;
        }
      else if (gsk_sl_type_is_matrix (type))
        {
          GskSlType *scalar_type = gsk_sl_type_get_scalar (gsk_sl_type_get_scalar_type (type));
          GskSlType *col_type = gsk_sl_type_get_index_type (type);
          gsize cols = gsk_sl_type_get_length (type);
          gsize rows = gsk_sl_type_get_length (col_type);
          guint32 ids[2 + gsk_sl_type_get_length (type)];
          guint32 scalar_id, zero_id, col_type_id;
          gsize c;

          scalar_id = gsk_spv_writer_add_conversion (writer, value_id, value_type, scalar_type);
          zero_id = gsk_spv_writer_get_id_for_zero (writer, gsk_sl_type_get_scalar_type (scalar_type));
          col_type_id = gsk_spv_writer_get_id_for_type (writer, col_type);

          for (c = 0; c < cols; c++)
            {
              ids[2 + c] = gsk_spv_writer_next_id (writer);
              gsk_spv_writer_add (writer,
                                  GSK_SPV_WRITER_SECTION_CODE,
                                  3 + rows, GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                                  (guint32[6]) { col_type_id,
                                                 ids[2 + c],
                                                 c == 0 ? scalar_id : zero_id,
                                                 c == 1 ? scalar_id : zero_id,
                                                 c == 2 ? scalar_id : zero_id,
                                                 c == 3 ? scalar_id : zero_id });
            }
          ids[0] = gsk_spv_writer_get_id_for_type (writer, type);
          ids[1] = gsk_spv_writer_next_id (writer);
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              3 + cols, GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                              ids);
          return ids[1];
        }
      else
        {
          g_assert_not_reached ();
          return 0;
        }
    }
  else if (constructor->n_arguments == 1 && gsk_sl_type_is_matrix (value_type) && gsk_sl_type_is_matrix (type))
    {
      GskSlType *col_type = gsk_sl_type_get_index_type (type);
      GskSlType *scalar_type = gsk_sl_type_get_index_type (col_type);
      GskSlType *value_col_type = gsk_sl_type_get_index_type (value_type);
      gsize cols = gsk_sl_type_get_length (type);
      gsize rows = gsk_sl_type_get_length (col_type);
      gsize value_cols = gsk_sl_type_get_length (type);
      gsize value_rows = gsk_sl_type_get_length (value_col_type);
      gsize c, r;
      guint32 col_ids[6], ids[6], one_id, zero_id, scalar_type_id;
      GskSlValue *value;

      value_id = gsk_sl_expression_write_spv (constructor->arguments[0], writer);
      if (gsk_sl_type_get_scalar_type (value_type) != gsk_sl_type_get_scalar_type (type))
        {
          GskSlType *new_value_type = gsk_sl_type_get_matching (value_type, gsk_sl_type_get_scalar_type (type));
          value_id = gsk_spv_writer_add_conversion (writer, value_id, value_type, new_value_type);
          value_type = new_value_type;
        }
      
      value = gsk_sl_value_new (scalar_type);
      zero_id = gsk_spv_writer_get_id_for_zero (writer, gsk_sl_type_get_scalar_type (scalar_type));
      one_id = gsk_spv_writer_get_id_for_one (writer, gsk_sl_type_get_scalar_type (scalar_type));
      gsk_sl_value_free (value);

      scalar_type_id = gsk_spv_writer_get_id_for_type (writer, scalar_type);
      col_ids[0] = gsk_spv_writer_get_id_for_type (writer, col_type);
      for (c = 0; c < cols; c++)
        {
          for (r = 0; r < rows; r++)
            {
              if (c < value_cols && r < value_rows)
                {
                  col_ids[2 + r] = gsk_spv_writer_next_id (writer);
                  gsk_spv_writer_add (writer,
                                      GSK_SPV_WRITER_SECTION_CODE,
                                      6, GSK_SPV_OP_COMPOSITE_EXTRACT,
                                      (guint32[5]) { scalar_type_id,
                                                     col_ids[2 + r],
                                                     value_id,
                                                     c,
                                                     r });
                }
              else if (c == r)
                col_ids[2 + r] = one_id;
              else
                col_ids[2 + r] = zero_id;
            }
          col_ids[1] = gsk_spv_writer_next_id (writer);
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              3 + rows, GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                              col_ids);
          ids[c + 2] = col_ids[1];
        }
      ids[0] = gsk_spv_writer_get_id_for_type (writer, type);
      ids[1] = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          3 + cols, GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                          ids);
      return ids[1];
    }
  else
    {
      gsize n_components = gsk_sl_type_get_n_components (type);
      GskSlScalarType scalar = gsk_sl_type_get_scalar_type (type);
      guint32 component_ids[16], component_type_id;
      gsize component = 0, arg, i;

      component_type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (scalar));
      for (arg = 0; arg < constructor->n_arguments; arg++)
        {
          value_type = gsk_sl_expression_get_return_type (constructor->arguments[arg]);
          value_id = gsk_sl_expression_write_spv (constructor->arguments[arg], writer);
          if (gsk_sl_type_is_scalar (value_type))
            {
              component_ids[component] = gsk_spv_writer_add_conversion (writer,
                                                                        value_id,
                                                                        value_type,
                                                                        gsk_sl_type_get_scalar (scalar));
              component++;
            }
          else if (gsk_sl_type_is_vector (value_type))
            {
              GskSlType *converted_type = gsk_sl_type_get_vector (scalar, gsk_sl_type_get_length (value_type));
              value_id = gsk_spv_writer_add_conversion (writer, value_id, value_type, converted_type);

              for (i = 0; component < n_components && i < gsk_sl_type_get_length (value_type); i++)
                {
                  component_ids[component] = gsk_spv_writer_next_id (writer);
                  gsk_spv_writer_add (writer,
                                      GSK_SPV_WRITER_SECTION_CODE,
                                      5, GSK_SPV_OP_COMPOSITE_EXTRACT,
                                      (guint32[4]) { component_type_id,
                                                     component_ids[component],
                                                     value_id,
                                                     i });
                  component++;
                }
            }
          else if (gsk_sl_type_is_matrix (value_type))
            {
              GskSlType *component_type = gsk_sl_type_get_index_type (value_type);
              gsize c, cols = gsk_sl_type_get_length (value_type);
              gsize r, rows = gsk_sl_type_get_length (component_type);

              for (c = 0; c < cols && component < n_components; c++)
                {
                  for (r = 0; r < rows && component < n_components; r++)
                    {
                      component_ids[component] = gsk_spv_writer_next_id (writer);
                      gsk_spv_writer_add (writer,
                                          GSK_SPV_WRITER_SECTION_CODE,
                                          6, GSK_SPV_OP_COMPOSITE_EXTRACT,
                                          (guint32[5]) { component_type_id,
                                                         component_ids[component],
                                                         value_id,
                                                         c,
                                                         r });
                      component_ids[component] = gsk_spv_writer_add_conversion (writer,
                                                                                component_ids[component],
                                                                                gsk_sl_type_get_scalar (gsk_sl_type_get_scalar_type (value_type)),
                                                                                gsk_sl_type_get_scalar (scalar));
                      component++;
                    }
                }
            }
          else if (gsk_sl_type_is_matrix (value_type))
            {
              g_assert_not_reached ();
            }
        }

      if (gsk_sl_type_is_scalar (type))
        {
          return component_ids[0];
        }
      else if (gsk_sl_type_is_vector (type))
        {
          guint32 ids[2 + gsk_sl_type_get_length (type)];
          guint i;

          ids[0] = gsk_spv_writer_get_id_for_type (writer, type);
          ids[1] = gsk_spv_writer_next_id (writer);
          for (i = 0; i < gsk_sl_type_get_length (type); i++)
            ids[2 + i] = component_ids[i];

          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              2 + gsk_sl_type_get_length (type), GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                              ids);
          return ids[1];
        }
      else if (gsk_sl_type_is_matrix (type))
        {
          GskSlType *col_type = gsk_sl_type_get_index_type (type);
          gsize c, cols = gsk_sl_type_get_length (type);
          gsize r, rows = gsk_sl_type_get_length (col_type);
          guint32 ids[cols + 2], col_ids[rows + 2];

          col_ids[1] = gsk_spv_writer_get_id_for_type (writer, col_type);
          for (c = 0; c < cols; c++)
            {
              for (r = 0; r < rows; r++)
                {
                }
              col_ids[1] = gsk_spv_writer_next_id (writer);
              gsk_spv_writer_add (writer,
                                  GSK_SPV_WRITER_SECTION_CODE,
                                  3 + rows, GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                                  col_ids);
              ids[c + 2] = col_ids[1];
            }
          ids[0] = gsk_spv_writer_get_id_for_type (writer, type);
          ids[1] = gsk_spv_writer_next_id (writer);
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              3 + cols, GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                              ids);
          return ids[1];
        }
      else
        {
          g_assert_not_reached ();
          return 0;
        }
    }
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_CONSTRUCTOR = {
  gsk_sl_expression_constructor_free,
  gsk_sl_expression_constructor_print,
  gsk_sl_expression_constructor_get_return_type,
  gsk_sl_expression_constructor_get_constant,
  gsk_sl_expression_constructor_write_spv
};

/* FUNCTION_CALL */

typedef struct _GskSlExpressionFunctionCall GskSlExpressionFunctionCall;

struct _GskSlExpressionFunctionCall {
  GskSlExpression parent;

  GskSlFunction *function;
  GskSlExpression **arguments;
  guint n_arguments;
};

static void
gsk_sl_expression_function_call_free (GskSlExpression *expression)
{
  GskSlExpressionFunctionCall *function_call = (GskSlExpressionFunctionCall *) expression;
  guint i;

  for (i = 0; i < function_call->n_arguments; i++)
    {
      gsk_sl_expression_unref (function_call->arguments[i]);
    }
  g_free (function_call->arguments);

  if (function_call->function)
    gsk_sl_function_unref (function_call->function);

  g_slice_free (GskSlExpressionFunctionCall, function_call);
}

static void
gsk_sl_expression_function_call_print (const GskSlExpression *expression,
                                       GskSlPrinter          *printer)
{
  const GskSlExpressionFunctionCall *function_call = (const GskSlExpressionFunctionCall *) expression;
  guint i;

  gsk_sl_printer_append (printer, gsk_sl_function_get_name (function_call->function));
  gsk_sl_printer_append (printer, " (");
  
  for (i = 0; i < function_call->n_arguments; i++)
    {
      if (i > 0)
        gsk_sl_printer_append (printer, ", ");
      gsk_sl_expression_print (function_call->arguments[i], printer);
    }

  gsk_sl_printer_append (printer, ")");
}

static GskSlType *
gsk_sl_expression_function_call_get_return_type (const GskSlExpression *expression)
{
  const GskSlExpressionFunctionCall *function_call = (const GskSlExpressionFunctionCall *) expression;

  return gsk_sl_function_get_return_type (function_call->function);
}

static GskSlValue *
gsk_sl_expression_function_call_get_constant (const GskSlExpression *expression)
{
  const GskSlExpressionFunctionCall *function_call = (const GskSlExpressionFunctionCall *) expression;
  GskSlValue *values[function_call->n_arguments];
  GskSlValue *result, *tmp;
  guint i;

  for (i = 0; i < function_call->n_arguments; i++)
    {
      tmp = gsk_sl_expression_get_constant (function_call->arguments[i]);
      if (tmp == NULL)
        {
          guint j;
          for (j = 0; j < i; j++)
            gsk_sl_value_free (values[j]);
          return NULL;
        }
      values[i] = gsk_sl_value_new_convert (tmp, gsk_sl_function_get_argument_type (function_call->function, i));
      gsk_sl_value_free (tmp);
    }

  result = gsk_sl_function_get_constant (function_call->function, values, function_call->n_arguments);

  for (i = 0; i < function_call->n_arguments; i++)
    gsk_sl_value_free (values[i]);

  return result;
}

static guint32
gsk_sl_expression_function_call_write_spv (const GskSlExpression *expression,
                                           GskSpvWriter          *writer)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_FUNCTION_CALL = {
  gsk_sl_expression_function_call_free,
  gsk_sl_expression_function_call_print,
  gsk_sl_expression_function_call_get_return_type,
  gsk_sl_expression_function_call_get_constant,
  gsk_sl_expression_function_call_write_spv
};

/* MEMBER */

typedef struct _GskSlExpressionMember GskSlExpressionMember;

struct _GskSlExpressionMember {
  GskSlExpression parent;

  GskSlExpression *expr;
  guint id;
};

static void
gsk_sl_expression_member_free (GskSlExpression *expression)
{
  GskSlExpressionMember *member = (GskSlExpressionMember *) expression;

  gsk_sl_expression_unref (member->expr);

  g_slice_free (GskSlExpressionMember, member);
}

static void
gsk_sl_expression_member_print (const GskSlExpression *expression,
                                GskSlPrinter          *printer)
{
  const GskSlExpressionMember *member = (const GskSlExpressionMember *) expression;

  gsk_sl_expression_print (member->expr, printer);
  gsk_sl_printer_append (printer, ".");
  gsk_sl_printer_append (printer, gsk_sl_type_get_member_name (gsk_sl_expression_get_return_type (member->expr), member->id));
}

static GskSlType *
gsk_sl_expression_member_get_return_type (const GskSlExpression *expression)
{
  const GskSlExpressionMember *member = (const GskSlExpressionMember *) expression;

  return gsk_sl_type_get_member_type (gsk_sl_expression_get_return_type (member->expr), member->id);
}

static GskSlValue *
gsk_sl_expression_member_get_constant (const GskSlExpression *expression)
{
  const GskSlExpressionMember *member = (const GskSlExpressionMember *) expression;
  GskSlValue *result, *value;

  value = gsk_sl_expression_get_constant (member->expr);
  if (value == NULL)
    return NULL;

  result = gsk_sl_value_new_member (value, member->id);

  gsk_sl_value_free (value);

  return result;
}

static guint32
gsk_sl_expression_member_write_spv (const GskSlExpression *expression,
                                    GskSpvWriter          *writer)
{
  const GskSlExpressionMember *member = (const GskSlExpressionMember *) expression;
  GskSlType *type;
  guint32 expr_id, type_id, result_id;

  type = gsk_sl_expression_get_return_type (member->expr);
  type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_member_type (type, member->id));
  expr_id = gsk_sl_expression_write_spv (member->expr, writer);
  result_id = gsk_spv_writer_next_id (writer);

  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      5, GSK_SPV_OP_ACCESS_CHAIN,
                      (guint32[4]) { type_id,
                                     result_id,
                                     expr_id,
                                     member->id });

  return result_id;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_MEMBER = {
  gsk_sl_expression_member_free,
  gsk_sl_expression_member_print,
  gsk_sl_expression_member_get_return_type,
  gsk_sl_expression_member_get_constant,
  gsk_sl_expression_member_write_spv
};

/* SWIZZLE */

typedef enum {
  GSK_SL_SWIZZLE_POINT,
  GSK_SL_SWIZZLE_COLOR,
  GSK_SL_SWIZZLE_TEXCOORD
} GskSlSwizzleName;

static const char *swizzle_options[] = { "xyzw", "rgba", "stpq" };

typedef struct _GskSlExpressionSwizzle GskSlExpressionSwizzle;

struct _GskSlExpressionSwizzle {
  GskSlExpression parent;

  GskSlExpression *expr;
  GskSlSwizzleName name;
  guint length;
  guint indexes[4];
};

static void
gsk_sl_expression_swizzle_free (GskSlExpression *expression)
{
  GskSlExpressionSwizzle *swizzle = (GskSlExpressionSwizzle *) expression;

  gsk_sl_expression_unref (swizzle->expr);

  g_slice_free (GskSlExpressionSwizzle, swizzle);
}

static void
gsk_sl_expression_swizzle_print (const GskSlExpression *expression,
                                 GskSlPrinter          *printer)
{
  const GskSlExpressionSwizzle *swizzle = (const GskSlExpressionSwizzle *) expression;
  guint i;

  gsk_sl_expression_print (swizzle->expr, printer);
  gsk_sl_printer_append (printer, ".");
  for (i = 0; i < swizzle->length; i++)
    {
      gsk_sl_printer_append_c (printer, swizzle_options[swizzle->name][swizzle->indexes[i]]);
    }
}

static GskSlType *
gsk_sl_expression_swizzle_get_return_type (const GskSlExpression *expression)
{
  const GskSlExpressionSwizzle *swizzle = (const GskSlExpressionSwizzle *) expression;
  GskSlType *type;

  type = gsk_sl_expression_get_return_type (swizzle->expr);

  if (swizzle->length == 1)
    return gsk_sl_type_get_scalar (gsk_sl_type_get_scalar_type (type));
  else
    return gsk_sl_type_get_vector (gsk_sl_type_get_scalar_type (type), swizzle->length);
}

static GskSlValue *
gsk_sl_expression_swizzle_get_constant (const GskSlExpression *expression)
{
  const GskSlExpressionSwizzle *swizzle = (const GskSlExpressionSwizzle *) expression;
  GskSlValue *result, *value;
  guchar *sdata, *ddata;
  gsize sstride, dstride;
  GskSlScalarType scalar_type;
  guint i;

  value = gsk_sl_expression_get_constant (swizzle->expr);
  if (value == NULL)
    return NULL;

  scalar_type = gsk_sl_type_get_scalar_type (gsk_sl_value_get_type (value));
  sdata = gsk_sl_value_get_data (value);
  sstride = gsk_sl_type_get_index_stride (gsk_sl_value_get_type (value));
  result = gsk_sl_value_new (gsk_sl_expression_get_return_type (expression));
  ddata = gsk_sl_value_get_data (result);
  dstride = gsk_sl_type_get_index_stride (gsk_sl_value_get_type (result));

  for (i = 0; i < swizzle->length; i++)
    {
      gsk_sl_scalar_type_convert_value (scalar_type,
                                        ddata + dstride * i,
                                        scalar_type,
                                        sdata + sstride * swizzle->indexes[i]);
    }

  gsk_sl_value_free (value);

  return result;
}

static guint32
gsk_sl_expression_swizzle_write_spv (const GskSlExpression *expression,
                                     GskSpvWriter          *writer)
{
  const GskSlExpressionSwizzle *swizzle = (const GskSlExpressionSwizzle *) expression;
  GskSlType *type;
  guint32 expr_id, type_id, result_id;

  type = gsk_sl_expression_get_return_type (swizzle->expr);
  expr_id = gsk_sl_expression_write_spv (swizzle->expr, writer);

  if (gsk_sl_type_is_scalar (type))
    {
      if (swizzle->length == 1)
        return expr_id;

      type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_expression_get_return_type (expression));
      result_id = gsk_spv_writer_next_id (writer);

      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          3 + swizzle->length, GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                          (guint32[6]) { type_id,
                                         result_id,
                                         expr_id,
                                         expr_id,
                                         expr_id,
                                         expr_id });

      return result_id;
    }
  else if (gsk_sl_type_is_vector (type))
    {
      type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_expression_get_return_type (expression));
      result_id = gsk_spv_writer_next_id (writer);

      if (swizzle->length == 1)
        {
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              4, GSK_SPV_OP_COMPOSITE_EXTRACT,
                              (guint32[6]) { type_id,
                                             result_id,
                                             swizzle->indexes[0] });
        }
      else
        {
          gsk_spv_writer_add (writer,
                              GSK_SPV_WRITER_SECTION_CODE,
                              5 + swizzle->length, GSK_SPV_OP_COMPOSITE_CONSTRUCT,
                              (guint32[8]) { type_id,
                                             result_id,
                                             expr_id,
                                             expr_id,
                                             swizzle->indexes[0],
                                             swizzle->indexes[1],
                                             swizzle->indexes[2],
                                             swizzle->indexes[3] });
        }

      return result_id;
    }
  else
    {
      g_assert_not_reached ();
      return 0;
    }
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_SWIZZLE = {
  gsk_sl_expression_swizzle_free,
  gsk_sl_expression_swizzle_print,
  gsk_sl_expression_swizzle_get_return_type,
  gsk_sl_expression_swizzle_get_constant,
  gsk_sl_expression_swizzle_write_spv
};

/* NEGATION */

typedef struct _GskSlExpressionNegation GskSlExpressionNegation;

struct _GskSlExpressionNegation {
  GskSlExpression parent;

  GskSlExpression *expr;
};

static void
gsk_sl_expression_negation_free (GskSlExpression *expression)
{
  GskSlExpressionNegation *negation = (GskSlExpressionNegation *) expression;

  gsk_sl_expression_unref (negation->expr);

  g_slice_free (GskSlExpressionNegation, negation);
}

static void
gsk_sl_expression_negation_print (const GskSlExpression *expression,
                                  GskSlPrinter          *printer)
{
  const GskSlExpressionNegation *negation = (const GskSlExpressionNegation *) expression;

  gsk_sl_printer_append (printer, "-");
  gsk_sl_expression_print (negation->expr, printer);
}

static GskSlType *
gsk_sl_expression_negation_get_return_type (const GskSlExpression *expression)
{
  const GskSlExpressionNegation *negation = (const GskSlExpressionNegation *) expression;

  return gsk_sl_expression_get_return_type (negation->expr);
}

#define GSK_SL_OPERATION_FUNC(func,type,...) \
static void \
func (gpointer value, gpointer unused) \
{ \
  type x = *(type *) value; \
  __VA_ARGS__ \
  *(type *) value = x; \
}
GSK_SL_OPERATION_FUNC(gsk_sl_expression_negation_int, gint32, x = -x;)
GSK_SL_OPERATION_FUNC(gsk_sl_expression_negation_uint, guint32, x = -x;)
GSK_SL_OPERATION_FUNC(gsk_sl_expression_negation_float, float, x = -x;)
GSK_SL_OPERATION_FUNC(gsk_sl_expression_negation_double, double, x = -x;)

static GskSlValue *
gsk_sl_expression_negation_get_constant (const GskSlExpression *expression)
{
  const GskSlExpressionNegation *negation = (const GskSlExpressionNegation *) expression;
  GskSlValue *value;

  value = gsk_sl_expression_get_constant (negation->expr);
  if (value == NULL)
    return NULL;

  switch (gsk_sl_type_get_scalar_type (gsk_sl_value_get_type (value)))
    {
    case GSK_SL_INT:
      gsk_sl_value_componentwise (value, gsk_sl_expression_negation_int, NULL);
      break;
    case GSK_SL_UINT:
      gsk_sl_value_componentwise (value, gsk_sl_expression_negation_uint, NULL);
      break;
    case GSK_SL_FLOAT:
      gsk_sl_value_componentwise (value, gsk_sl_expression_negation_float, NULL);
      break;
    case GSK_SL_DOUBLE:
      gsk_sl_value_componentwise (value, gsk_sl_expression_negation_double, NULL);
      break;
    case GSK_SL_VOID:
    case GSK_SL_BOOL:
    default:
      g_assert_not_reached ();
      break;
    }

  return value;
}

static guint32
gsk_sl_expression_negation_write_spv (const GskSlExpression *expression,
                                      GskSpvWriter          *writer)
{
  const GskSlExpressionNegation *negation = (const GskSlExpressionNegation *) expression;
  guint type_id, expr_id, result_id;
  GskSlType *type;

  type = gsk_sl_expression_get_return_type (negation->expr);
  type_id = gsk_spv_writer_get_id_for_type (writer, type);
  expr_id = gsk_sl_expression_write_spv (negation->expr, writer);
  result_id = gsk_spv_writer_next_id (writer);

  switch (gsk_sl_type_get_scalar_type (type))
    {
    case GSK_SL_INT:
    case GSK_SL_UINT:
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          4, GSK_SPV_OP_S_NEGATE,
                          (guint32[3]) { type_id,
                                         result_id,
                                         expr_id });
      break;
    case GSK_SL_FLOAT:
    case GSK_SL_DOUBLE:
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          4, GSK_SPV_OP_F_NEGATE,
                          (guint32[3]) { type_id,
                                         result_id,
                                         expr_id });
      break;
    case GSK_SL_VOID:
    case GSK_SL_BOOL:
    default:
      g_assert_not_reached ();
      break;
    }

  return result_id;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_NEGATION = {
  gsk_sl_expression_negation_free,
  gsk_sl_expression_negation_print,
  gsk_sl_expression_negation_get_return_type,
  gsk_sl_expression_negation_get_constant,
  gsk_sl_expression_negation_write_spv
};

/* CONSTANT */

typedef struct _GskSlExpressionConstant GskSlExpressionConstant;

struct _GskSlExpressionConstant {
  GskSlExpression parent;

  GskSlValue *value;
};

static void
gsk_sl_expression_constant_free (GskSlExpression *expression)
{
  GskSlExpressionConstant *constant = (GskSlExpressionConstant *) expression;

  gsk_sl_value_free (constant->value);

  g_slice_free (GskSlExpressionConstant, constant);
}

static void
gsk_sl_expression_constant_print (const GskSlExpression *expression,
                                  GskSlPrinter          *printer)
{
  const GskSlExpressionConstant *constant = (const GskSlExpressionConstant *) expression;

  gsk_sl_value_print (constant->value, printer);
}

static GskSlType *
gsk_sl_expression_constant_get_return_type (const GskSlExpression *expression)
{
  const GskSlExpressionConstant *constant = (const GskSlExpressionConstant *) expression;

  return gsk_sl_value_get_type (constant->value);
}

static GskSlValue *
gsk_sl_expression_constant_get_constant (const GskSlExpression *expression)
{
  const GskSlExpressionConstant *constant = (const GskSlExpressionConstant *) expression;

  return gsk_sl_value_copy (constant->value);
}

static guint32
gsk_sl_expression_constant_write_spv (const GskSlExpression *expression,
                                      GskSpvWriter          *writer)
{
  const GskSlExpressionConstant *constant = (const GskSlExpressionConstant *) expression;

  return gsk_spv_writer_get_id_for_value (writer, constant->value);
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_CONSTANT = {
  gsk_sl_expression_constant_free,
  gsk_sl_expression_constant_print,
  gsk_sl_expression_constant_get_return_type,
  gsk_sl_expression_constant_get_constant,
  gsk_sl_expression_constant_write_spv
};

/* If parsing fails completely, just assume 1.0 */
static GskSlExpression *
gsk_sl_expression_error_new (void)
{
  GskSlExpressionConstant *constant;

  constant = gsk_sl_expression_new (GskSlExpressionConstant, &GSK_SL_EXPRESSION_CONSTANT);
  constant->value = gsk_sl_value_new (gsk_sl_type_get_scalar (GSK_SL_FLOAT));

  return (GskSlExpression *) constant;
}

GskSlExpression *
gsk_sl_expression_parse_constructor (GskSlScope        *scope,
                                     GskSlPreprocessor *stream,
                                     GskSlType         *type)
{
  GskSlExpressionConstructor *call;
  const GskSlToken *token;
  gssize missing_args;
  GPtrArray *arguments;

  call = gsk_sl_expression_new (GskSlExpressionConstructor, &GSK_SL_EXPRESSION_CONSTRUCTOR);
  call->type = gsk_sl_type_ref (type);

  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      gsk_sl_preprocessor_error (stream, SYNTAX, "Expected opening \"(\" when calling function.");
      gsk_sl_expression_unref ((GskSlExpression *) call);
      return gsk_sl_expression_error_new ();
    }
  gsk_sl_preprocessor_consume (stream, (GskSlExpression *) call);

  missing_args = gsk_sl_type_get_n_components (type);
  arguments = g_ptr_array_new ();

  token = gsk_sl_preprocessor_get (stream);
  
  while (TRUE)
    {
      GskSlExpression *expression = gsk_sl_expression_parse_assignment (scope, stream);

      if (missing_args == 0)
        {
          gsk_sl_preprocessor_error (stream, ARGUMENT_COUNT,
                                     "Too many arguments given to builtin constructor, need only %u.",
                                     arguments->len);
          missing_args = -1;
        }
      else if (missing_args > 0)
        {
          GskSlType *return_type = gsk_sl_expression_get_return_type (expression);
          gsize provided = gsk_sl_type_get_n_components (return_type);

          if (provided == 0)
            {
              gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                         "Invalid type %s for builtin constructor",
                                         gsk_sl_type_get_name (return_type));
              missing_args = -1;
            }
          else if (gsk_sl_type_is_matrix (return_type) && 
                   gsk_sl_type_is_matrix (type))
            {
              if (arguments->len == 0)
                {
                  missing_args = 0;
                }
              else
                {
                  gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                             "Matrix type %s only valid as first argument for %s",
                                             gsk_sl_type_get_name (return_type),
                                             gsk_sl_type_get_name (type));
                  missing_args = -1;
                }
            }
          else
            {
              missing_args -= MIN (missing_args, provided);
            }
        }

      g_ptr_array_add (arguments, expression);
      
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_COMMA))
        break;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) call);
    }

  if (missing_args > 0)
    {
      if (arguments->len != 1 || !gsk_sl_type_is_scalar (gsk_sl_expression_get_return_type (g_ptr_array_index (arguments, 0))))
        {
          gsk_sl_preprocessor_error (stream, ARGUMENT_COUNT,
                                     "Not enough arguments given to builtin constructor, %"G_GSIZE_FORMAT" are missing.",
                                     missing_args);
          missing_args = -1;
        }
    }

  call->n_arguments = arguments->len;
  call->arguments = (GskSlExpression **) g_ptr_array_free (arguments, FALSE);

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    {
      gsk_sl_preprocessor_error (stream, SYNTAX, "Expected closing \")\" after arguments.");
      gsk_sl_preprocessor_sync (stream, GSK_SL_TOKEN_RIGHT_PAREN);
    }
  gsk_sl_preprocessor_consume (stream, (GskSlExpression *) call);

  if (missing_args < 0)
    {
      gsk_sl_expression_unref ((GskSlExpression *) call);
      return gsk_sl_expression_error_new ();
    }
  
  return (GskSlExpression *) call;
}

GskSlExpression *
gsk_sl_expression_parse_function_call (GskSlScope           *scope,
                                       GskSlPreprocessor    *stream,
                                       GskSlFunctionMatcher *matcher)
{
  GskSlExpressionFunctionCall *call;
  const GskSlToken *token;

  call = gsk_sl_expression_new (GskSlExpressionFunctionCall, &GSK_SL_EXPRESSION_FUNCTION_CALL);

  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      gsk_sl_preprocessor_error (stream, SYNTAX, "Expected opening \"(\" when calling function.");
      gsk_sl_expression_unref ((GskSlExpression *) call);
      return gsk_sl_expression_error_new ();
    }
  gsk_sl_preprocessor_consume (stream, (GskSlExpression *) call);

  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    {
      GPtrArray *arguments;
  
      arguments = g_ptr_array_new ();
      while (TRUE)
        {
          GskSlExpression *expression = gsk_sl_expression_parse_assignment (scope, stream);

          if (matcher == NULL)
            {
              /* no checking necessary */
            }
          else if (matcher)
            {
              GskSlType *type = gsk_sl_expression_get_return_type (expression);

              gsk_sl_function_matcher_match_argument (matcher, arguments->len, type);
              if (!gsk_sl_function_matcher_has_matches (matcher))
                {
                  gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                             "No overloaded function available that matches the first %u arguments",
                                             arguments->len + 1);
                  matcher = NULL;
                }
            }

          g_ptr_array_add (arguments, expression);
          
          token = gsk_sl_preprocessor_get (stream);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_COMMA))
            break;
          gsk_sl_preprocessor_consume (stream, (GskSlExpression *) call);
        }

      call->n_arguments = arguments->len;
      call->arguments = (GskSlExpression **) g_ptr_array_free (arguments, FALSE);
    }

  if (matcher)
    {
      gsk_sl_function_matcher_match_n_arguments (matcher, call->n_arguments);
      if (!gsk_sl_function_matcher_has_matches (matcher))
        {
          gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                     "No overloaded function available with %u arguments.",
                                     call->n_arguments);
          matcher = NULL;
        }
      else
        {
          call->function = gsk_sl_function_matcher_get_match (matcher);
          if (call->function)
            gsk_sl_function_ref (call->function);
          else
            {
              gsk_sl_preprocessor_error (stream, UNIQUENESS,
                                         "Cannot find unique match for overloaded function.");
              matcher = NULL;
            }
        }           
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    {
      gsk_sl_preprocessor_error (stream, SYNTAX, "Expected closing \")\" after arguments.");
      gsk_sl_preprocessor_sync (stream, GSK_SL_TOKEN_RIGHT_PAREN);
      matcher = NULL;
    }
  gsk_sl_preprocessor_consume (stream, (GskSlExpression *) call);

  if (matcher == NULL)
    {
      gsk_sl_expression_unref ((GskSlExpression *) call);
      return gsk_sl_expression_error_new ();
    }
  
  return (GskSlExpression *) call;
}

static GskSlExpression *
gsk_sl_expression_parse_primary (GskSlScope        *scope,
                                 GskSlPreprocessor *stream)
{
  GskSlExpressionConstant *constant;
  const GskSlToken *token;

  token = gsk_sl_preprocessor_get (stream);
  switch ((guint) token->type)
  {
    case GSK_SL_TOKEN_IDENTIFIER:
      {
        GskSlExpression *expr;
        GskSlVariable *variable;
        GskSlType *type;
        char *name;
        
        type = gsk_sl_scope_lookup_type (scope, token->str);
        if (type)
          {
            GskSlFunctionMatcher matcher;
            GskSlFunction *constructor;

            gsk_sl_preprocessor_consume (stream, NULL);
            constructor = gsk_sl_function_new_constructor (type);
            gsk_sl_function_matcher_init (&matcher, g_list_prepend (NULL, constructor));
            expr = gsk_sl_expression_parse_function_call (scope, stream, &matcher);
            gsk_sl_function_matcher_finish (&matcher);
            gsk_sl_function_unref (constructor);

            return expr;
          }

        name = g_strdup (token->str);
        gsk_sl_preprocessor_consume (stream, NULL);

        token = gsk_sl_preprocessor_get (stream);
        if (gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
          {
            GskSlFunctionMatcher matcher;
            
            gsk_sl_scope_match_function (scope, &matcher, name);
            
            if (!gsk_sl_function_matcher_has_matches (&matcher))
              gsk_sl_preprocessor_error (stream, DECLARATION, "No function named \"%s\".", name);
            
            expr = gsk_sl_expression_parse_function_call (scope, stream, &matcher);

            gsk_sl_function_matcher_finish (&matcher);
          }
        else
          {
            GskSlExpressionReference *reference;
            variable = gsk_sl_scope_lookup_variable (scope, name);
            if (variable == NULL)
              {
                gsk_sl_preprocessor_error (stream, DECLARATION, "No variable named \"%s\".", name);
                expr = gsk_sl_expression_error_new ();
              }
            else
              {
                reference = gsk_sl_expression_new (GskSlExpressionReference, &GSK_SL_EXPRESSION_REFERENCE);
                reference->variable = gsk_sl_variable_ref (variable);
                expr = (GskSlExpression *) reference;
              }
          }

        g_free (name);
        return expr;
      }

    case GSK_SL_TOKEN_INTCONSTANT:
      constant = gsk_sl_expression_new (GskSlExpressionConstant, &GSK_SL_EXPRESSION_CONSTANT);
      constant->value = gsk_sl_value_new (gsk_sl_type_get_scalar (GSK_SL_INT));
      *(gint32 *) gsk_sl_value_get_data (constant->value) = token->i32;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) constant);
      return (GskSlExpression *) constant;

    case GSK_SL_TOKEN_UINTCONSTANT:
      constant = gsk_sl_expression_new (GskSlExpressionConstant, &GSK_SL_EXPRESSION_CONSTANT);
      constant->value = gsk_sl_value_new (gsk_sl_type_get_scalar (GSK_SL_UINT));
      *(guint32 *) gsk_sl_value_get_data (constant->value) = token->u32;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) constant);
      return (GskSlExpression *) constant;

    case GSK_SL_TOKEN_FLOATCONSTANT:
      constant = gsk_sl_expression_new (GskSlExpressionConstant, &GSK_SL_EXPRESSION_CONSTANT);
      constant->value = gsk_sl_value_new (gsk_sl_type_get_scalar (GSK_SL_FLOAT));
      *(float *) gsk_sl_value_get_data (constant->value) = token->f;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) constant);
      return (GskSlExpression *) constant;

    case GSK_SL_TOKEN_BOOLCONSTANT:
      constant = gsk_sl_expression_new (GskSlExpressionConstant, &GSK_SL_EXPRESSION_CONSTANT);
      constant->value = gsk_sl_value_new (gsk_sl_type_get_scalar (GSK_SL_BOOL));
      *(guint32 *) gsk_sl_value_get_data (constant->value) = token->b;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) constant);
      return (GskSlExpression *) constant;

    case GSK_SL_TOKEN_DOUBLECONSTANT:
      constant = gsk_sl_expression_new (GskSlExpressionConstant, &GSK_SL_EXPRESSION_CONSTANT);
      constant->value = gsk_sl_value_new (gsk_sl_type_get_scalar (GSK_SL_DOUBLE));
      *(double *) gsk_sl_value_get_data (constant->value) = token->f;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) constant);
      return (GskSlExpression *) constant;

    case GSK_SL_TOKEN_LEFT_PAREN:
      {
        GskSlExpression *expr;

        gsk_sl_preprocessor_consume (stream, NULL);
        expr = gsk_sl_expression_parse (scope, stream);

        token = gsk_sl_preprocessor_get (stream);
        if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
          {
            gsk_sl_preprocessor_error (stream, SYNTAX, "Expected closing \")\".");
            gsk_sl_preprocessor_sync (stream, GSK_SL_TOKEN_RIGHT_PAREN);
          }
        gsk_sl_preprocessor_consume (stream, NULL);

        return expr;
      }
    case GSK_SL_TOKEN_VOID:
    case GSK_SL_TOKEN_FLOAT:
    case GSK_SL_TOKEN_DOUBLE:
    case GSK_SL_TOKEN_INT:
    case GSK_SL_TOKEN_UINT:
    case GSK_SL_TOKEN_BOOL:
    case GSK_SL_TOKEN_BVEC2:
    case GSK_SL_TOKEN_BVEC3:
    case GSK_SL_TOKEN_BVEC4:
    case GSK_SL_TOKEN_IVEC2:
    case GSK_SL_TOKEN_IVEC3:
    case GSK_SL_TOKEN_IVEC4:
    case GSK_SL_TOKEN_UVEC2:
    case GSK_SL_TOKEN_UVEC3:
    case GSK_SL_TOKEN_UVEC4:
    case GSK_SL_TOKEN_VEC2:
    case GSK_SL_TOKEN_VEC3:
    case GSK_SL_TOKEN_VEC4:
    case GSK_SL_TOKEN_DVEC2:
    case GSK_SL_TOKEN_DVEC3:
    case GSK_SL_TOKEN_DVEC4:
    case GSK_SL_TOKEN_MAT2:
    case GSK_SL_TOKEN_MAT3:
    case GSK_SL_TOKEN_MAT4:
    case GSK_SL_TOKEN_DMAT2:
    case GSK_SL_TOKEN_DMAT3:
    case GSK_SL_TOKEN_DMAT4:
    case GSK_SL_TOKEN_MAT2X2:
    case GSK_SL_TOKEN_MAT2X3:
    case GSK_SL_TOKEN_MAT2X4:
    case GSK_SL_TOKEN_MAT3X2:
    case GSK_SL_TOKEN_MAT3X3:
    case GSK_SL_TOKEN_MAT3X4:
    case GSK_SL_TOKEN_MAT4X2:
    case GSK_SL_TOKEN_MAT4X3:
    case GSK_SL_TOKEN_MAT4X4:
    case GSK_SL_TOKEN_DMAT2X2:
    case GSK_SL_TOKEN_DMAT2X3:
    case GSK_SL_TOKEN_DMAT2X4:
    case GSK_SL_TOKEN_DMAT3X2:
    case GSK_SL_TOKEN_DMAT3X3:
    case GSK_SL_TOKEN_DMAT3X4:
    case GSK_SL_TOKEN_DMAT4X2:
    case GSK_SL_TOKEN_DMAT4X3:
    case GSK_SL_TOKEN_DMAT4X4:
      {
        GskSlExpression *expression;
        GskSlType *type;

        type = gsk_sl_type_new_parse (scope, stream);
        expression = gsk_sl_expression_parse_constructor (scope, stream, type);
        gsk_sl_type_unref (type);

        return expression;
      }
    case GSK_SL_TOKEN_STRUCT:
      {
        GskSlFunctionMatcher matcher;
        GskSlFunction *constructor;
        GskSlExpression *expression;
        GskSlType *type;

        type = gsk_sl_type_new_parse (scope, stream);
        constructor = gsk_sl_function_new_constructor (type);
        gsk_sl_function_matcher_init (&matcher, g_list_prepend (NULL, constructor));
        expression = gsk_sl_expression_parse_function_call (scope, stream, &matcher);
        gsk_sl_function_matcher_finish (&matcher);
        gsk_sl_function_unref (constructor);
        gsk_sl_type_unref (type);

        return expression;
      }

    default:
      gsk_sl_preprocessor_error (stream, SYNTAX, "Expected an expression.");
      gsk_sl_preprocessor_consume (stream, NULL);
      return gsk_sl_expression_error_new ();
  }
}

static GskSlExpression *
gsk_sl_expression_parse_field_selection (GskSlScope        *scope,
                                         GskSlPreprocessor *stream,
                                         GskSlExpression   *expr,
                                         const char        *name)
{
  GskSlType *type;
  guint n;

  if (g_str_equal (name, "length"))
    {
       gsk_sl_preprocessor_error (stream, UNSUPPORTED, ".length() is not implemented yet.");
       return expr;
    }

  type = gsk_sl_expression_get_return_type (expr);

  if (gsk_sl_type_is_scalar (type) || gsk_sl_type_is_vector (type))
    {
      GskSlExpressionSwizzle *swizzle;
      guint type_length = MAX (1, gsk_sl_type_get_length (type));
      
      swizzle = gsk_sl_expression_new (GskSlExpressionSwizzle, &GSK_SL_EXPRESSION_SWIZZLE);

      for (swizzle->name = 0; swizzle->name < G_N_ELEMENTS(swizzle_options); swizzle->name++)
        {
          const char *found = strchr (swizzle_options[swizzle->name], name[0]);
          if (found)
            break;
        }
      if (swizzle->name == G_N_ELEMENTS(swizzle_options))
        {
          gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Type %s has no member named \"%s\".", gsk_sl_type_get_name (type), name);
          gsk_sl_expression_unref ((GskSlExpression *) swizzle);
          return expr;
        }
      for (swizzle->length = 0; swizzle->length < 4 && name[swizzle->length]; swizzle->length++)
        {
          const char *found = strchr (swizzle_options[swizzle->name], name[swizzle->length]);
          if (found == NULL)
            {
              gsk_sl_preprocessor_error (stream, SYNTAX,
                                         "Character '%c' is not valid for swizzle. Must be one of \"%s\".",
                                         name[swizzle->length], swizzle_options[swizzle->name]);
              gsk_sl_expression_unref ((GskSlExpression *) swizzle);
              return expr;
            }
          swizzle->indexes[swizzle->length] = found - swizzle_options[swizzle->name];
          if (swizzle->indexes[swizzle->length] >= type_length)
            {
              gsk_sl_preprocessor_error (stream, SYNTAX,
                                         "Swizzle index '%c' not allowed for type %s",
                                         name[swizzle->length], gsk_sl_type_get_name (type));
              gsk_sl_expression_unref ((GskSlExpression *) swizzle);
              return expr;
            }
        }
      swizzle->expr = expr;

      if (name[swizzle->length])
        {
          gsk_sl_preprocessor_error (stream, SYNTAX, "Too many swizzle options. A maximum of 4 characters are allowed.");
          return (GskSlExpression *) swizzle;
        }
  
      return (GskSlExpression *) swizzle;
    }
  else if (gsk_sl_type_find_member (type, name, &n, NULL, NULL))
    {
      GskSlExpressionMember *member;
      
      member = gsk_sl_expression_new (GskSlExpressionMember, &GSK_SL_EXPRESSION_MEMBER);
      member->expr = expr;
      member->id = n;
  
      return (GskSlExpression *) member;
    }
  else
    {
      gsk_sl_preprocessor_error (stream, TYPE_MISMATCH, "Type %s has no fields to select.", gsk_sl_type_get_name (type));
      return expr;
    }
}

static GskSlExpression *
gsk_sl_expression_parse_postfix (GskSlScope        *scope,
                                 GskSlPreprocessor *stream)
{
  GskSlExpression *expr;
  const GskSlToken *token;
  
  expr = gsk_sl_expression_parse_primary (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_DOT))
        {
          gsk_sl_preprocessor_consume (stream, NULL);
          token = gsk_sl_preprocessor_get (stream);
          if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
            {
              char *field = g_strdup (token->str);
              gsk_sl_preprocessor_consume (stream, NULL);
              expr = gsk_sl_expression_parse_field_selection (scope, stream, expr, field);
              g_free (field);
            }
          else
            {
              gsk_sl_preprocessor_error (stream, SYNTAX, "Expected an identifier to select a field.");
              continue;
            }
        }
      else 
        {
          break;
        }
    }

  return expr;
}

static GskSlExpression *
gsk_sl_expression_parse_unary (GskSlScope        *scope,
                               GskSlPreprocessor *preproc)
{
  const GskSlToken *token;
  GskSlType *type;

  token = gsk_sl_preprocessor_get (preproc);

  if (gsk_sl_token_is (token, GSK_SL_TOKEN_DASH))
    {
      GskSlExpressionNegation *negation = gsk_sl_expression_new (GskSlExpressionNegation, &GSK_SL_EXPRESSION_NEGATION); 
      GskSlExpression *expr;

      gsk_sl_preprocessor_consume (preproc, negation);
      negation->expr = gsk_sl_expression_parse_unary (scope, preproc);
      type = gsk_sl_expression_get_return_type (negation->expr);
      if (!gsk_sl_type_is_scalar (type) && !gsk_sl_type_is_vector (type) && !gsk_sl_type_is_matrix (type))
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                     "Negation only works on scalars, vectors and matrices, not on %s.",
                                     gsk_sl_type_get_name (type));
          expr = gsk_sl_expression_ref (negation->expr);
          gsk_sl_expression_unref ((GskSlExpression *) negation);
          return expr;
        }
      else if (gsk_sl_type_get_scalar_type (type) == GSK_SL_BOOL)
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                     "Negation does not work on boolean types like %s.",
                                     gsk_sl_type_get_name (type));
          expr = gsk_sl_expression_ref (negation->expr);
          gsk_sl_expression_unref ((GskSlExpression *) negation);
          return expr;
        }
      else
        {
          return (GskSlExpression *) negation;
        }
    }
  else
    {
      return gsk_sl_expression_parse_postfix (scope, preproc);
    }
}

static GskSlExpression *
gsk_sl_expression_parse_multiplicative (GskSlScope        *scope,
                                        GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression, *right;
  enum { MUL, DIV, MOD } op;

  expression = gsk_sl_expression_parse_unary (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_STAR))
        op = MUL;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_SLASH))
        op = DIV;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_PERCENT))
        op = MOD;
      else
        return expression;

      gsk_sl_preprocessor_consume (stream, NULL);
      right = gsk_sl_expression_parse_unary (scope, stream);
      if (op == MUL)
        {
          GskSlType *result_type;

          result_type = gsk_sl_expression_multiplication_get_result_type (stream,
                                                                          gsk_sl_expression_get_return_type (expression),
                                                                          gsk_sl_expression_get_return_type (right));
          if (result_type)
            {
              GskSlExpressionMultiplication *multiplication;
              multiplication = gsk_sl_expression_new (GskSlExpressionMultiplication, &GSK_SL_EXPRESSION_MULTIPLICATION);
              multiplication->type = gsk_sl_type_ref (result_type);
              multiplication->left = expression;
              multiplication->right = right;
              expression = (GskSlExpression *) multiplication;
            }
          else
            {
              gsk_sl_expression_unref ((GskSlExpression *) right);
            }
        }
      else if (op == DIV)
        {
          GskSlType *result_type;

          result_type = gsk_sl_expression_arithmetic_type_check (stream,
                                                                 gsk_sl_expression_get_return_type (expression),
                                                                 gsk_sl_expression_get_return_type (right));
          if (result_type)
            {
              GskSlExpressionArithmetic *division;
              division = gsk_sl_expression_new (GskSlExpressionArithmetic, &GSK_SL_EXPRESSION_DIVISION);
              division->type = gsk_sl_type_ref (result_type);
              division->left = expression;
              division->right = right;
              expression = (GskSlExpression *) division;
            }
          else
            {
              gsk_sl_expression_unref ((GskSlExpression *) right);
            }
        }
      else
        {
          if (gsk_sl_expression_bitwise_type_check (stream,
                                                    gsk_sl_expression_get_return_type (expression),
                                                    gsk_sl_expression_get_return_type (right)))
            {
              GskSlExpressionOperation *operation;
              operation = gsk_sl_expression_new (GskSlExpressionOperation, &GSK_SL_EXPRESSION_OPERATION);
              operation->op = GSK_SL_OPERATION_MOD;
              operation->left = expression;
              operation->right = right;
              expression = (GskSlExpression *) operation;
            }
          else
            {
              gsk_sl_expression_unref ((GskSlExpression *) right);
            }
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_additive (GskSlScope        *scope,
                                  GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression, *right;
  GskSlType *result_type;
  enum { ADD, SUB } op;

  expression = gsk_sl_expression_parse_multiplicative (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_PLUS))
        op = ADD;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_DASH))
        op = SUB;
      else
        return expression;

      gsk_sl_preprocessor_consume (stream, NULL);
      right = gsk_sl_expression_parse_additive (scope, stream);
      result_type = gsk_sl_expression_arithmetic_type_check (stream,
                                                             gsk_sl_expression_get_return_type (expression),
                                                             gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionArithmetic *arithmetic;

          arithmetic = gsk_sl_expression_new (GskSlExpressionArithmetic, op == ADD ? &GSK_SL_EXPRESSION_ADDITION : &GSK_SL_EXPRESSION_SUBTRACTION);
          arithmetic->type = gsk_sl_type_ref (result_type);
          arithmetic->left = expression;
          arithmetic->right = right;
          expression = (GskSlExpression *) arithmetic;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_shift (GskSlScope        *scope,
                               GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression;
  GskSlExpressionOperation *operation;
  GskSlOperation op;

  expression = gsk_sl_expression_parse_additive (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_OP))
        op = GSK_SL_OPERATION_LSHIFT;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_OP))
        op = GSK_SL_OPERATION_RSHIFT;
      else
        return expression;

      operation = gsk_sl_expression_new (GskSlExpressionOperation, &GSK_SL_EXPRESSION_OPERATION);
      operation->left = expression;
      operation->op = op;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) operation);
      operation->right = gsk_sl_expression_parse_additive (scope, stream);
      if (!gsk_sl_expression_shift_type_check (stream,
                                               gsk_sl_expression_get_return_type (operation->left),
                                               gsk_sl_expression_get_return_type (operation->right)))
        {
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else
        {
          expression = (GskSlExpression *) operation;
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_relational (GskSlScope        *scope,
                                    GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression;
  GskSlExpressionOperation *operation;
  GskSlOperation op;

  expression = gsk_sl_expression_parse_shift (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_ANGLE))
        op = GSK_SL_OPERATION_LESS;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_ANGLE))
        op = GSK_SL_OPERATION_GREATER;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_LE_OP))
        op = GSK_SL_OPERATION_LESS_EQUAL;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_GE_OP))
        op = GSK_SL_OPERATION_GREATER_EQUAL;
      else
        return expression;

      operation = gsk_sl_expression_new (GskSlExpressionOperation, &GSK_SL_EXPRESSION_OPERATION);
      operation->left = expression;
      operation->op = op;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) operation);
      operation->right = gsk_sl_expression_parse_shift (scope, stream);
      if (!gsk_sl_expression_relational_type_check (stream,
                                                    gsk_sl_expression_get_return_type (operation->left),
                                                    gsk_sl_expression_get_return_type (operation->right)))
        {
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else
        {
          expression = (GskSlExpression *) operation;
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_equality (GskSlScope        *scope,
                                  GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression;
  GskSlExpressionOperation *operation;
  GskSlOperation op;

  expression = gsk_sl_expression_parse_relational (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_EQ_OP))
        op = GSK_SL_OPERATION_EQUAL;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_NE_OP))
        op = GSK_SL_OPERATION_NOT_EQUAL;
      else
        return expression;

      operation = gsk_sl_expression_new (GskSlExpressionOperation, &GSK_SL_EXPRESSION_OPERATION);
      operation->left = expression;
      operation->op = op;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) operation);
      operation->right = gsk_sl_expression_parse_relational (scope, stream);
      expression = (GskSlExpression *) operation;
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_and (GskSlScope        *scope,
                             GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression;
  GskSlExpressionOperation *operation;

  expression = gsk_sl_expression_parse_equality (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_AMPERSAND))
        return expression;

      operation = gsk_sl_expression_new (GskSlExpressionOperation, &GSK_SL_EXPRESSION_OPERATION);
      operation->left = expression;
      operation->op = GSK_SL_OPERATION_AND;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) operation);
      operation->right = gsk_sl_expression_parse_equality (scope, stream);
      if (!gsk_sl_expression_bitwise_type_check (stream,
                                                 gsk_sl_expression_get_return_type (operation->left),
                                                 gsk_sl_expression_get_return_type (operation->right)))
        {
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else
        {
          expression = (GskSlExpression *) operation;
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_xor (GskSlScope        *scope,
                             GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression;
  GskSlExpressionOperation *operation;

  expression = gsk_sl_expression_parse_and (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_CARET))
        return expression;

      operation = gsk_sl_expression_new (GskSlExpressionOperation, &GSK_SL_EXPRESSION_OPERATION);
      operation->left = expression;
      operation->op = GSK_SL_OPERATION_XOR;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) operation);
      operation->right = gsk_sl_expression_parse_and (scope, stream);
      if (!gsk_sl_expression_bitwise_type_check (stream,
                                                 gsk_sl_expression_get_return_type (operation->left),
                                                 gsk_sl_expression_get_return_type (operation->right)))
        {
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else
        {
          expression = (GskSlExpression *) operation;
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_or (GskSlScope        *scope,
                            GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression;
  GskSlExpressionOperation *operation;

  expression = gsk_sl_expression_parse_xor (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_VERTICAL_BAR))
        return expression;

      operation = gsk_sl_expression_new (GskSlExpressionOperation, &GSK_SL_EXPRESSION_OPERATION);
      operation->left = expression;
      operation->op = GSK_SL_OPERATION_OR;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) operation);
      operation->right = gsk_sl_expression_parse_xor (scope, stream);
      if (!gsk_sl_expression_bitwise_type_check (stream,
                                                 gsk_sl_expression_get_return_type (operation->left),
                                                 gsk_sl_expression_get_return_type (operation->right)))
        {
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else
        {
          expression = (GskSlExpression *) operation;
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_logical_and (GskSlScope        *scope,
                                     GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression;
  GskSlExpressionOperation *operation;

  expression = gsk_sl_expression_parse_or (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_AND_OP))
        return expression;

      operation = gsk_sl_expression_new (GskSlExpressionOperation, &GSK_SL_EXPRESSION_OPERATION);
      operation->left = expression;
      operation->op = GSK_SL_OPERATION_LOGICAL_AND;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) operation);
      operation->right = gsk_sl_expression_parse_or (scope, stream);
      if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                    gsk_sl_expression_get_return_type (operation->right)))
        {
          gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                     "Right operand of && expression is not bool but %s",
                                     gsk_sl_type_get_name (gsk_sl_expression_get_return_type (operation->right)));
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_expression_get_return_type (expression)))
        {
          gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                     "Left operand of && expression is not bool but %s",
                                     gsk_sl_type_get_name (gsk_sl_expression_get_return_type (expression)));
          expression = operation->right;
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else
        {
          expression = (GskSlExpression *) operation;
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_logical_xor (GskSlScope        *scope,
                                     GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression;
  GskSlExpressionOperation *operation;

  expression = gsk_sl_expression_parse_logical_and (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_XOR_OP))
        return expression;

      operation = gsk_sl_expression_new (GskSlExpressionOperation, &GSK_SL_EXPRESSION_OPERATION);
      operation->left = expression;
      operation->op = GSK_SL_OPERATION_LOGICAL_XOR;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) operation);
      operation->right = gsk_sl_expression_parse_logical_and (scope, stream);
      if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                    gsk_sl_expression_get_return_type (operation->right)))
        {
          gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                     "Right operand of ^^ expression is not bool but %s",
                                     gsk_sl_type_get_name (gsk_sl_expression_get_return_type (operation->right)));
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_expression_get_return_type (expression)))
        {
          gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                     "Left operand of ^^ expression is not bool but %s",
                                     gsk_sl_type_get_name (gsk_sl_expression_get_return_type (expression)));
          expression = operation->right;
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else
        {
          expression = (GskSlExpression *) operation;
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_logical_or (GskSlScope        *scope,
                                    GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *expression;
  GskSlExpressionOperation *operation;

  expression = gsk_sl_expression_parse_logical_xor (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_OR_OP))
        return expression;

      operation = gsk_sl_expression_new (GskSlExpressionOperation, &GSK_SL_EXPRESSION_OPERATION);
      operation->left = expression;
      operation->op = GSK_SL_OPERATION_LOGICAL_OR;
      gsk_sl_preprocessor_consume (stream, (GskSlExpression *) operation);
      operation->right = gsk_sl_expression_parse_logical_xor (scope, stream);
      if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                    gsk_sl_expression_get_return_type (operation->right)))
        {
          gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                     "Right operand of || expression is not bool but %s",
                                     gsk_sl_type_get_name (gsk_sl_expression_get_return_type (operation->right)));
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_expression_get_return_type (expression)))
        {
          gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                     "Left operand of || expression is not bool but %s",
                                     gsk_sl_type_get_name (gsk_sl_expression_get_return_type (expression)));
          expression = operation->right;
          gsk_sl_expression_ref (expression);
          gsk_sl_expression_unref ((GskSlExpression *) operation);
        }
      else
        {
          expression = (GskSlExpression *) operation;
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_conditional (GskSlScope        *scope,
                                     GskSlPreprocessor *stream)
{
  /* XXX: support conditionals */
  return gsk_sl_expression_parse_logical_or (scope, stream);
}

GskSlExpression *
gsk_sl_expression_parse_constant (GskSlScope        *scope,
                                  GskSlPreprocessor *stream)
{
  return gsk_sl_expression_parse_conditional (scope, stream);
}

GskSlExpression *
gsk_sl_expression_parse_assignment (GskSlScope        *scope,
                                    GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlExpression *lvalue;
  GskSlExpressionAssignment *assign;

  lvalue = gsk_sl_expression_parse_conditional (scope, stream);

  token = gsk_sl_preprocessor_get (stream);
  switch ((guint) token->type)
  {
      case GSK_SL_TOKEN_EQUAL:
      case GSK_SL_TOKEN_MUL_ASSIGN:
      case GSK_SL_TOKEN_DIV_ASSIGN:
      case GSK_SL_TOKEN_MOD_ASSIGN:
      case GSK_SL_TOKEN_ADD_ASSIGN:
      case GSK_SL_TOKEN_SUB_ASSIGN:
      case GSK_SL_TOKEN_LEFT_ASSIGN:
      case GSK_SL_TOKEN_RIGHT_ASSIGN:
      case GSK_SL_TOKEN_AND_ASSIGN:
      case GSK_SL_TOKEN_XOR_ASSIGN:
      case GSK_SL_TOKEN_OR_ASSIGN:
        break;

      default:
        return lvalue;
  }

#if 0
  if (gsk_sl_expression_is_constant (lvalue))
    {
      gsk_sl_preprocessor_error (stream, "Cannot assign to a return lvalue.");

      /* Continue parsing like normal here to get more errors */
      gsk_sl_preprocessor_consume (stream, lvalue);
      gsk_sl_expression_unref (lvalue);

      return gsk_sl_expression_parse_assignment (scope, stream);
    }
#endif

  assign = gsk_sl_expression_new (GskSlExpressionAssignment, &GSK_SL_EXPRESSION_ASSIGNMENT);
  assign->lvalue = lvalue;
  assign->op = token->type;

  gsk_sl_preprocessor_consume (stream, (GskSlExpression *) assign);

  assign->rvalue = gsk_sl_expression_parse_assignment (scope, stream);

  return (GskSlExpression *) assign;
}

GskSlExpression *
gsk_sl_expression_parse (GskSlScope        *scope,
                         GskSlPreprocessor *stream)
{
  /* XXX: Allow comma here */
  return gsk_sl_expression_parse_assignment (scope, stream);
}

GskSlExpression *
gsk_sl_expression_ref (GskSlExpression *expression)
{
  g_return_val_if_fail (expression != NULL, NULL);

  expression->ref_count += 1;

  return expression;
}

void
gsk_sl_expression_unref (GskSlExpression *expression)
{
  if (expression == NULL)
    return;

  expression->ref_count -= 1;
  if (expression->ref_count > 0)
    return;

  expression->class->free (expression);
}

void
gsk_sl_expression_print (const GskSlExpression *expression,
                         GskSlPrinter          *printer)
{
  expression->class->print (expression, printer);
}

GskSlType *
gsk_sl_expression_get_return_type (const GskSlExpression *expression)
{
  return expression->class->get_return_type (expression);
}

GskSlValue *
gsk_sl_expression_get_constant (const GskSlExpression *expression)
{
  return expression->class->get_constant (expression);
}

guint32
gsk_sl_expression_write_spv (const GskSlExpression *expression,
                             GskSpvWriter          *writer)
{
  return expression->class->write_spv (expression, writer);
}

