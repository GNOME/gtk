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

#include "gskslbinaryprivate.h"
#include "gskslfunctionprivate.h"
#include "gskslpreprocessorprivate.h"
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
  gboolean              (* is_assignable)                       (const GskSlExpression  *expression,
                                                                 GError                **error);
  GskSlType *           (* get_return_type)                     (const GskSlExpression  *expression);
  GskSlValue *          (* get_constant)                        (const GskSlExpression  *expression);
  guint32               (* write_spv)                           (const GskSlExpression  *expression,
                                                                 GskSpvWriter           *writer);
  GskSpvAccessChain *   (* get_spv_access_chain)                (const GskSlExpression  *expression,
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

static GskSpvAccessChain *
gsk_sl_expression_get_spv_access_chain (const GskSlExpression *expression,
                                        GskSpvWriter          *writer)
{
  return expression->class->get_spv_access_chain (expression, writer);
}

static gboolean
gsk_sl_expression_default_is_assignable (const GskSlExpression  *expression,
                                         GError                **error)
{
  g_set_error (error,
               GSK_SL_COMPILER_ERROR,
               GSK_SL_COMPILER_ERROR_SYNTAX,
               "Assignment requires l-value.");
                   
  return FALSE;
}

static GskSpvAccessChain *
gsk_sl_expression_default_get_spv_access_chain (const GskSlExpression *expression,
                                                GskSpvWriter          *writer)
{
  return NULL;
}

/* ASSIGNMENT */

typedef struct _GskSlExpressionAssignment GskSlExpressionAssignment;

struct _GskSlExpressionAssignment {
  GskSlExpression parent;

  const GskSlBinary *binary;
  GskSlType *type;
  GskSlExpression *lvalue;
  GskSlExpression *rvalue;
};

static void
gsk_sl_expression_assignment_free (GskSlExpression *expression)
{
  GskSlExpressionAssignment *assignment = (GskSlExpressionAssignment *) expression;

  gsk_sl_expression_unref (assignment->lvalue);
  gsk_sl_expression_unref (assignment->rvalue);
  gsk_sl_type_unref (assignment->type);

  g_slice_free (GskSlExpressionAssignment, assignment);
}

static void
gsk_sl_expression_assignment_print (const GskSlExpression *expression,
                                    GskSlPrinter          *printer)
{
  const GskSlExpressionAssignment *assignment = (const GskSlExpressionAssignment *) expression;

  gsk_sl_expression_print (assignment->lvalue, printer);
  gsk_sl_printer_append (printer, " ");
  if (assignment->binary)
    gsk_sl_printer_append (printer, gsk_sl_binary_get_sign (assignment->binary));
  gsk_sl_printer_append (printer, "= ");
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
                                        GskSpvWriter          *writer)
{
  const GskSlExpressionAssignment *assignment = (const GskSlExpressionAssignment *) expression;
  GskSpvAccessChain *chain;
  GskSlType *rtype, *ltype;
  guint32 rvalue;

  chain = gsk_sl_expression_get_spv_access_chain (assignment->lvalue, writer);
  g_assert (chain);
  ltype = gsk_sl_expression_get_return_type (assignment->lvalue),
  rtype = gsk_sl_expression_get_return_type (assignment->rvalue),
  rvalue = gsk_sl_expression_write_spv (assignment->rvalue, writer);

  if (assignment->binary)
    {
      rvalue = gsk_sl_binary_write_spv (assignment->binary,
                                        writer,
                                        assignment->type,
                                        ltype,
                                        gsk_spv_access_chain_load (chain),
                                        rtype,
                                        rvalue);
      rtype = assignment->type;
    }

  rvalue = gsk_spv_writer_convert (writer, rvalue, ltype, rtype);
  gsk_spv_access_chain_store (chain, rvalue);
  gsk_spv_access_chain_free (chain);

  return rvalue;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_ASSIGNMENT = {
  gsk_sl_expression_assignment_free,
  gsk_sl_expression_assignment_print,
  gsk_sl_expression_default_is_assignable,
  gsk_sl_expression_assignment_get_return_type,
  gsk_sl_expression_assignment_get_constant,
  gsk_sl_expression_assignment_write_spv,
  gsk_sl_expression_default_get_spv_access_chain
};

/* BINARY */

typedef struct _GskSlExpressionBinary GskSlExpressionBinary;

struct _GskSlExpressionBinary {
  GskSlExpression parent;

  const GskSlBinary *binary;
  GskSlType *type;
  GskSlExpression *left;
  GskSlExpression *right;
};

static void
gsk_sl_expression_binary_free (GskSlExpression *expression)
{
  GskSlExpressionBinary *binary = (GskSlExpressionBinary *) expression;

  gsk_sl_expression_unref (binary->left);
  gsk_sl_expression_unref (binary->right);
  gsk_sl_type_unref (binary->type);

  g_slice_free (GskSlExpressionBinary, binary);
}

static void
gsk_sl_expression_binary_print (const GskSlExpression *expression,
                                GskSlPrinter          *printer)
{
  GskSlExpressionBinary *binary = (GskSlExpressionBinary *) expression;

  gsk_sl_expression_print (binary->left, printer);
  gsk_sl_printer_append (printer, " ");
  gsk_sl_printer_append (printer, gsk_sl_binary_get_sign (binary->binary));
  gsk_sl_printer_append (printer, " ");
  gsk_sl_expression_print (binary->right, printer);
}

static GskSlType *
gsk_sl_expression_binary_get_return_type (const GskSlExpression *expression)
{
  GskSlExpressionBinary *binary = (GskSlExpressionBinary *) expression;

  return binary->type;
}

static GskSlValue *
gsk_sl_expression_binary_get_constant (const GskSlExpression *expression)
{
  const GskSlExpressionBinary *binary = (const GskSlExpressionBinary *) expression;
  GskSlValue *lvalue, *rvalue;

  lvalue = gsk_sl_expression_get_constant (binary->left);
  if (lvalue == NULL)
    return NULL;
  rvalue = gsk_sl_expression_get_constant (binary->right);
  if (rvalue == NULL)
    {
      gsk_sl_value_free (lvalue);
      return NULL;
    }

  return gsk_sl_binary_get_constant (binary->binary,
                                     binary->type,
                                     lvalue,
                                     rvalue);
}

static guint32
gsk_sl_expression_binary_write_spv (const GskSlExpression *expression,
                                    GskSpvWriter          *writer)
{
  const GskSlExpressionBinary *binary = (const GskSlExpressionBinary *) expression;

  return gsk_sl_binary_write_spv (binary->binary,
                                  writer,
                                  binary->type,
                                  gsk_sl_expression_get_return_type (binary->left),
                                  gsk_sl_expression_write_spv (binary->left, writer),
                                  gsk_sl_expression_get_return_type (binary->right),
                                  gsk_sl_expression_write_spv (binary->right, writer));
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_BINARY = {
  gsk_sl_expression_binary_free,
  gsk_sl_expression_binary_print,
  gsk_sl_expression_default_is_assignable,
  gsk_sl_expression_binary_get_return_type,
  gsk_sl_expression_binary_get_constant,
  gsk_sl_expression_binary_write_spv,
  gsk_sl_expression_default_get_spv_access_chain
};

/* LOGICAL_OR */

typedef struct _GskSlExpressionLogicalOr GskSlExpressionLogicalOr;

struct _GskSlExpressionLogicalOr {
  GskSlExpression parent;

  GskSlExpression *left;
  GskSlExpression *right;
};

static void
gsk_sl_expression_logical_or_free (GskSlExpression *expression)
{
  GskSlExpressionLogicalOr *logical_or = (GskSlExpressionLogicalOr *) expression;

  gsk_sl_expression_unref (logical_or->left);
  gsk_sl_expression_unref (logical_or->right);

  g_slice_free (GskSlExpressionLogicalOr, logical_or);
}

static void
gsk_sl_expression_logical_or_print (const GskSlExpression *expression,
                                    GskSlPrinter          *printer)
{
  GskSlExpressionLogicalOr *logical_or = (GskSlExpressionLogicalOr *) expression;

  gsk_sl_expression_print (logical_or->left, printer);
  gsk_sl_printer_append (printer, " || ");
  gsk_sl_expression_print (logical_or->right, printer);
}

static GskSlType *
gsk_sl_expression_logical_or_get_return_type (const GskSlExpression *expression)
{
  return gsk_sl_type_get_scalar (GSK_SL_BOOL);
}

static GskSlValue *
gsk_sl_expression_logical_or_get_constant (const GskSlExpression *expression)
{
  const GskSlExpressionLogicalOr *logical_or = (const GskSlExpressionLogicalOr *) expression;
  GskSlValue *lvalue, *rvalue;

  lvalue = gsk_sl_expression_get_constant (logical_or->left);
  if (lvalue == NULL)
    return NULL;
  rvalue = gsk_sl_expression_get_constant (logical_or->right);
  if (rvalue == NULL)
    {
      gsk_sl_value_free (lvalue);
      return NULL;
    }

  if (*(guint32 *) gsk_sl_value_get_data (lvalue))
    {
      gsk_sl_value_free (rvalue);
      return lvalue;
    }
  else
    {
      gsk_sl_value_free (lvalue);
      return rvalue;
    }
}

static guint32
gsk_sl_expression_logical_or_write_spv (const GskSlExpression *expression,
                                        GskSpvWriter          *writer)
{
  const GskSlExpressionLogicalOr *logical_or = (const GskSlExpressionLogicalOr *) expression;
  GskSpvCodeBlock *current_block, *after_block, *or_block;
  guint32 current_id, condition_id, after_id, or_id, left_id, right_id, result_id;

  left_id = gsk_sl_expression_write_spv (logical_or->left, writer);

  current_block = gsk_spv_writer_pop_code_block (writer);
  current_id = gsk_spv_code_block_get_label (current_block);
  gsk_spv_writer_push_code_block (writer, current_block);

  or_id = gsk_spv_writer_push_new_code_block (writer);
  or_block = gsk_spv_writer_pop_code_block (writer);

  after_id = gsk_spv_writer_push_new_code_block (writer);
  after_block = gsk_spv_writer_pop_code_block (writer);

  /* mirror glslang */
  condition_id = gsk_spv_writer_logical_not (writer, gsk_sl_type_get_scalar (GSK_SL_BOOL), left_id);
  gsk_spv_writer_selection_merge (writer, after_id, 0);
  gsk_spv_writer_branch_conditional (writer, condition_id, or_id, after_id, NULL, 0);

  gsk_spv_writer_push_code_block (writer, or_block);
  right_id = gsk_sl_expression_write_spv (logical_or->right, writer);
  gsk_spv_writer_branch (writer, after_id);
  gsk_spv_writer_commit_code_block (writer);

  gsk_spv_writer_push_code_block (writer, after_block);
  gsk_spv_writer_commit_code_block (writer);

  result_id = gsk_spv_writer_phi (writer, 
                                  gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                  (guint32 **) (guint32[4][2]) {
                                      { left_id, current_id },
                                      { right_id, or_id }
                                  },
                                  2);

  return result_id;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_LOGICAL_OR = {
  gsk_sl_expression_logical_or_free,
  gsk_sl_expression_logical_or_print,
  gsk_sl_expression_default_is_assignable,
  gsk_sl_expression_logical_or_get_return_type,
  gsk_sl_expression_logical_or_get_constant,
  gsk_sl_expression_logical_or_write_spv,
  gsk_sl_expression_default_get_spv_access_chain
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

static gboolean
gsk_sl_expression_reference_is_assignable (const GskSlExpression  *expression,
                                           GError                **error)
{
  const GskSlExpressionReference *reference = (const GskSlExpressionReference *) expression;

  if (gsk_sl_variable_is_constant (reference->variable))
    {
      g_set_error (error,
                   GSK_SL_COMPILER_ERROR,
                   GSK_SL_COMPILER_ERROR_CONSTANT,
                   "Cannot assign constant \"%s\".", gsk_sl_variable_get_name (reference->variable));
      return FALSE;
    }

  return TRUE;
}

static GskSlType *
gsk_sl_expression_reference_get_return_type (const GskSlExpression *expression)
{
  const GskSlExpressionReference *reference = (const GskSlExpressionReference *) expression;

  return gsk_sl_variable_get_type (reference->variable);
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

  return gsk_sl_variable_load_spv (reference->variable, writer);
}

static GskSpvAccessChain *
gsk_sl_expression_reference_get_spv_access_chain (const GskSlExpression *expression,
                                                  GskSpvWriter          *writer)
{
  GskSlExpressionReference *reference = (GskSlExpressionReference *) expression;

  return gsk_spv_access_chain_new (writer, reference->variable);
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_REFERENCE = {
  gsk_sl_expression_reference_free,
  gsk_sl_expression_reference_print,
  gsk_sl_expression_reference_is_assignable,
  gsk_sl_expression_reference_get_return_type,
  gsk_sl_expression_reference_get_constant,
  gsk_sl_expression_reference_write_spv,
  gsk_sl_expression_reference_get_spv_access_chain
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
          return gsk_spv_writer_convert (writer, value_id, value_type, type);
        }
      else if (gsk_sl_type_is_vector (type))
        {
          GskSlType *scalar_type = gsk_sl_type_get_scalar (gsk_sl_type_get_scalar_type (type));
          guint32 scalar_id;

          scalar_id = gsk_spv_writer_convert (writer, value_id, value_type, scalar_type);

          return gsk_spv_writer_composite_construct (writer,
                                                     type,
                                                     (guint[4]) { scalar_id, scalar_id, scalar_id, scalar_id },
                                                     gsk_sl_type_get_n_components (type));
        }
      else if (gsk_sl_type_is_matrix (type))
        {
          GskSlType *scalar_type = gsk_sl_type_get_scalar (gsk_sl_type_get_scalar_type (type));
          GskSlType *col_type = gsk_sl_type_get_index_type (type);
          gsize cols = gsk_sl_type_get_length (type);
          gsize rows = gsk_sl_type_get_length (col_type);
          guint32 ids[gsk_sl_type_get_length (type)];
          guint32 scalar_id, zero_id;
          gsize c;

          scalar_id = gsk_spv_writer_convert (writer, value_id, value_type, scalar_type);
          zero_id = gsk_spv_writer_get_id_for_zero (writer, gsk_sl_type_get_scalar_type (scalar_type));

          for (c = 0; c < cols; c++)
            {
              ids[c] = gsk_spv_writer_composite_construct (writer,
                                                           type,
                                                           (guint32[4]) {
                                                               c == 0 ? scalar_id : zero_id,
                                                               c == 1 ? scalar_id : zero_id,
                                                               c == 2 ? scalar_id : zero_id,
                                                               c == 3 ? scalar_id : zero_id
                                                           },
                                                           rows);
            }
          return gsk_spv_writer_composite_construct (writer, type, ids, cols);
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
      guint32 col_ids[4], ids[4], one_id, zero_id;
      GskSlValue *value;

      value_id = gsk_sl_expression_write_spv (constructor->arguments[0], writer);

      if (gsk_sl_type_get_scalar_type (value_type) != gsk_sl_type_get_scalar_type (type))
        {
          GskSlType *new_value_type = gsk_sl_type_get_matching (value_type, gsk_sl_type_get_scalar_type (type));
          value_id = gsk_spv_writer_convert (writer, value_id, value_type, new_value_type);
          value_type = new_value_type;
        }
      
      value = gsk_sl_value_new (scalar_type);
      zero_id = gsk_spv_writer_get_id_for_zero (writer, gsk_sl_type_get_scalar_type (scalar_type));
      one_id = gsk_spv_writer_get_id_for_one (writer, gsk_sl_type_get_scalar_type (scalar_type));
      gsk_sl_value_free (value);

      for (c = 0; c < cols; c++)
        {
          for (r = 0; r < rows; r++)
            {
              if (c < value_cols && r < value_rows)
                {
                  col_ids[r] = gsk_spv_writer_composite_extract (writer,
                                                                 scalar_type,
                                                                 value_id,
                                                                 (guint32[2]) { c, r },
                                                                 2);
                }
              else if (c == r)
                col_ids[r] = one_id;
              else
                col_ids[r] = zero_id;
            }
          ids[c] = gsk_spv_writer_composite_construct (writer, col_type, col_ids, rows);
        }
      return gsk_spv_writer_composite_construct (writer, type, ids, cols);
    }
  else
    {
      gsize n_components = gsk_sl_type_get_n_components (type);
      GskSlScalarType scalar = gsk_sl_type_get_scalar_type (type);
      GskSlType *component_type;
      guint32 component_ids[16];
      gsize component = 0, arg, i;

      component_type = gsk_sl_type_get_scalar (scalar);
      for (arg = 0; arg < constructor->n_arguments; arg++)
        {
          value_type = gsk_sl_expression_get_return_type (constructor->arguments[arg]);
          value_id = gsk_sl_expression_write_spv (constructor->arguments[arg], writer);
          if (gsk_sl_type_get_scalar_type (value_type) != scalar)
            {
              GskSlType *new_type = gsk_sl_type_get_matching (value_type, scalar);
              value_id = gsk_spv_writer_convert (writer,
                                                 value_id,
                                                 value_type,
                                                 new_type);
              value_type = new_type;
            }

          if (gsk_sl_type_is_scalar (value_type))
            {
              component_ids[component] = value_id;
              component++;
            }
          else if (gsk_sl_type_is_vector (value_type))
            {
              for (i = 0; component < n_components && i < gsk_sl_type_get_length (value_type); i++)
                {
                  component_ids[component] = gsk_spv_writer_composite_extract (writer,
                                                                               component_type,
                                                                               value_id,
                                                                               (guint32[1]) { i },
                                                                               1);
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
                      component_ids[component] = gsk_spv_writer_composite_extract (writer,
                                                                                   component_type,
                                                                                   value_id,
                                                                                   (guint32[2]) { c, r },
                                                                                   2);
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
          return gsk_spv_writer_composite_construct (writer, type, component_ids, gsk_sl_type_get_length (type));
        }
      else if (gsk_sl_type_is_matrix (type))
        {
          GskSlType *col_type = gsk_sl_type_get_index_type (type);
          gsize c, cols = gsk_sl_type_get_length (type);
          gsize rows = gsk_sl_type_get_length (col_type);
          guint32 ids[cols];

          for (c = 0; c < cols; c++)
            {
              ids[c] = gsk_spv_writer_composite_construct (writer, col_type, &component_ids[c * rows], rows);
            }
          return gsk_spv_writer_composite_construct (writer, type, ids, cols);
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
  gsk_sl_expression_default_is_assignable,
  gsk_sl_expression_constructor_get_return_type,
  gsk_sl_expression_constructor_get_constant,
  gsk_sl_expression_constructor_write_spv,
  gsk_sl_expression_default_get_spv_access_chain
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
  const GskSlExpressionFunctionCall *function_call = (const GskSlExpressionFunctionCall *) expression;
  guint arguments[function_call->n_arguments];
  gsize i;

  for (i = 0; i < function_call->n_arguments; i++)
    {
      arguments[i] = gsk_sl_expression_write_spv (function_call->arguments[i], writer);
      arguments[i] = gsk_spv_writer_convert (writer, 
                                             arguments[i],
                                             gsk_sl_expression_get_return_type (function_call->arguments[i]),
                                             gsk_sl_function_get_argument_type (function_call->function, i));
    }

  return gsk_sl_function_write_call_spv (function_call->function, writer, arguments);
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_FUNCTION_CALL = {
  gsk_sl_expression_function_call_free,
  gsk_sl_expression_function_call_print,
  gsk_sl_expression_default_is_assignable,
  gsk_sl_expression_function_call_get_return_type,
  gsk_sl_expression_function_call_get_constant,
  gsk_sl_expression_function_call_write_spv,
  gsk_sl_expression_default_get_spv_access_chain
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

static gboolean
gsk_sl_expression_member_is_assignable (const GskSlExpression  *expression,
                                        GError                **error)
{
  const GskSlExpressionMember *member = (const GskSlExpressionMember *) expression;

  return gsk_sl_expression_is_assignable (member->expr, error);
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

  type = gsk_sl_expression_get_return_type (member->expr);

  return gsk_spv_writer_composite_extract (writer,
                                           gsk_sl_type_get_member_type (type, member->id),
                                           gsk_sl_expression_write_spv (member->expr, writer),
                                           (guint32[1]) { member->id },
                                           1);
}

static GskSpvAccessChain *
gsk_sl_expression_member_get_spv_access_chain (const GskSlExpression *expression,
                                               GskSpvWriter          *writer)
{
  const GskSlExpressionMember *member = (const GskSlExpressionMember *) expression;
  GskSpvAccessChain *chain;
  GskSlValue *value;
  GskSlType *type;

  chain = gsk_sl_expression_get_spv_access_chain (member->expr, writer);
  if (chain == NULL)
    return NULL;

  value = gsk_sl_value_new_for_data (gsk_sl_type_get_scalar (GSK_SL_INT), &(gint32) { member->id }, NULL, NULL);
  type = gsk_sl_expression_get_return_type (member->expr);
  gsk_spv_access_chain_add_index (chain, 
                                  gsk_sl_type_get_member_type (type, member->id),
                                  gsk_spv_writer_get_id_for_value (writer, value));
  gsk_sl_value_free (value);

  return chain;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_MEMBER = {
  gsk_sl_expression_member_free,
  gsk_sl_expression_member_print,
  gsk_sl_expression_member_is_assignable,
  gsk_sl_expression_member_get_return_type,
  gsk_sl_expression_member_get_constant,
  gsk_sl_expression_member_write_spv,
  gsk_sl_expression_member_get_spv_access_chain
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

static gboolean
gsk_sl_expression_swizzle_is_assignable (const GskSlExpression  *expression,
                                         GError                **error)
{
  const GskSlExpressionSwizzle *swizzle = (const GskSlExpressionSwizzle *) expression;

  if (!gsk_sl_expression_is_assignable (swizzle->expr, error))
    return FALSE;

  switch (swizzle->length)
  {
    default:
      g_assert_not_reached ();
    case 4:
      if (swizzle->indexes[0] == swizzle->indexes[3] ||
          swizzle->indexes[1] == swizzle->indexes[3] ||
          swizzle->indexes[2] == swizzle->indexes[3])
        break;
    case 3:
      if (swizzle->indexes[0] == swizzle->indexes[2] ||
          swizzle->indexes[1] == swizzle->indexes[2])
        break;
    case 2:
      if (swizzle->indexes[0] == swizzle->indexes[1])
        break;
    case 1:
      return TRUE;
  }
  
  g_set_error (error,
               GSK_SL_COMPILER_ERROR,
               GSK_SL_COMPILER_ERROR_SYNTAX,
               "Cannot assign to swizzle with duplicate components.");
  return FALSE;
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
  guint32 expr_id;

  type = gsk_sl_expression_get_return_type (swizzle->expr);
  expr_id = gsk_sl_expression_write_spv (swizzle->expr, writer);

  if (gsk_sl_type_is_scalar (type))
    {
      if (swizzle->length == 1)
        return expr_id;

      return gsk_spv_writer_composite_construct (writer,
                                                 gsk_sl_expression_get_return_type (expression),
                                                 (guint32[4]) { expr_id, expr_id, expr_id, expr_id },
                                                 swizzle->length);
    }
  else if (gsk_sl_type_is_vector (type))
    {
      if (swizzle->length == 1)
        {
          return gsk_spv_writer_composite_extract (writer,
                                                   type,
                                                   expr_id,
                                                   (guint32[1]) { swizzle->indexes[0] },
                                                   1);
        }
      else
        {
          return gsk_spv_writer_vector_shuffle (writer,
                                                type,
                                                expr_id,
                                                expr_id,
                                                (guint32[4]) {
                                                    swizzle->indexes[0],
                                                    swizzle->indexes[1],
                                                    swizzle->indexes[2],
                                                    swizzle->indexes[3]
                                                },
                                                swizzle->length);
        }
    }
  else
    {
      g_assert_not_reached ();
      return 0;
    }
}

static GskSpvAccessChain *
gsk_sl_expression_swizzle_get_spv_access_chain (const GskSlExpression *expression,
                                                GskSpvWriter          *writer)
{
  const GskSlExpressionSwizzle *swizzle = (const GskSlExpressionSwizzle *) expression;
  GskSpvAccessChain *chain;

  chain = gsk_sl_expression_get_spv_access_chain (swizzle->expr, writer);
  if (chain == NULL)
    return NULL;

  gsk_spv_access_chain_swizzle (chain, 
                                swizzle->indexes,
                                swizzle->length);

  return chain;
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_SWIZZLE = {
  gsk_sl_expression_swizzle_free,
  gsk_sl_expression_swizzle_print,
  gsk_sl_expression_swizzle_is_assignable,
  gsk_sl_expression_swizzle_get_return_type,
  gsk_sl_expression_swizzle_get_constant,
  gsk_sl_expression_swizzle_write_spv,
  gsk_sl_expression_swizzle_get_spv_access_chain
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
  GskSlType *type = gsk_sl_expression_get_return_type (negation->expr);

  switch (gsk_sl_type_get_scalar_type (type))
    {
    case GSK_SL_INT:
    case GSK_SL_UINT:
      return gsk_spv_writer_s_negate (writer,
                                      type,
                                      gsk_sl_expression_write_spv (negation->expr, writer));

    case GSK_SL_FLOAT:
    case GSK_SL_DOUBLE:
      return gsk_spv_writer_f_negate (writer,
                                      type,
                                      gsk_sl_expression_write_spv (negation->expr, writer));

    case GSK_SL_VOID:
    case GSK_SL_BOOL:
    default:
      g_assert_not_reached ();
      return 0;
    }
}

static const GskSlExpressionClass GSK_SL_EXPRESSION_NEGATION = {
  gsk_sl_expression_negation_free,
  gsk_sl_expression_negation_print,
  gsk_sl_expression_default_is_assignable,
  gsk_sl_expression_negation_get_return_type,
  gsk_sl_expression_negation_get_constant,
  gsk_sl_expression_negation_write_spv,
  gsk_sl_expression_default_get_spv_access_chain
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
  gsk_sl_expression_default_is_assignable,
  gsk_sl_expression_constant_get_return_type,
  gsk_sl_expression_constant_get_constant,
  gsk_sl_expression_constant_write_spv,
  gsk_sl_expression_default_get_spv_access_chain
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
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_unary (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_STAR) &&
          !gsk_sl_token_is (token, GSK_SL_TOKEN_SLASH) &&
          !gsk_sl_token_is (token, GSK_SL_TOKEN_PERCENT))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_unary (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionBinary *binary_expr;
          binary_expr = gsk_sl_expression_new (GskSlExpressionBinary, &GSK_SL_EXPRESSION_BINARY);
          binary_expr->binary = binary;
          binary_expr->type = gsk_sl_type_ref (result_type);
          binary_expr->left = expression;
          binary_expr->right = right;
          expression = (GskSlExpression *) binary_expr;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_additive (GskSlScope        *scope,
                                  GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_multiplicative (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_PLUS) &&
          !gsk_sl_token_is (token, GSK_SL_TOKEN_DASH))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_multiplicative (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionBinary *binary_expr;
          binary_expr = gsk_sl_expression_new (GskSlExpressionBinary, &GSK_SL_EXPRESSION_BINARY);
          binary_expr->binary = binary;
          binary_expr->type = gsk_sl_type_ref (result_type);
          binary_expr->left = expression;
          binary_expr->right = right;
          expression = (GskSlExpression *) binary_expr;
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
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_additive (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_OP) &&
          !gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_OP))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_additive (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionBinary *binary_expr;
          binary_expr = gsk_sl_expression_new (GskSlExpressionBinary, &GSK_SL_EXPRESSION_BINARY);
          binary_expr->binary = binary;
          binary_expr->type = gsk_sl_type_ref (result_type);
          binary_expr->left = expression;
          binary_expr->right = right;
          expression = (GskSlExpression *) binary_expr;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_relational (GskSlScope        *scope,
                                    GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_shift (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_ANGLE) &&
          !gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_ANGLE) &&
          !gsk_sl_token_is (token, GSK_SL_TOKEN_LE_OP) &&
          !gsk_sl_token_is (token, GSK_SL_TOKEN_GE_OP))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_shift (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionBinary *binary_expr;
          binary_expr = gsk_sl_expression_new (GskSlExpressionBinary, &GSK_SL_EXPRESSION_BINARY);
          binary_expr->binary = binary;
          binary_expr->type = gsk_sl_type_ref (result_type);
          binary_expr->left = expression;
          binary_expr->right = right;
          expression = (GskSlExpression *) binary_expr;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_equality (GskSlScope        *scope,
                                  GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_relational (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_EQ_OP) &&
          !gsk_sl_token_is (token, GSK_SL_TOKEN_NE_OP))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_relational (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionBinary *binary_expr;
          binary_expr = gsk_sl_expression_new (GskSlExpressionBinary, &GSK_SL_EXPRESSION_BINARY);
          binary_expr->binary = binary;
          binary_expr->type = gsk_sl_type_ref (result_type);
          binary_expr->left = expression;
          binary_expr->right = right;
          expression = (GskSlExpression *) binary_expr;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_and (GskSlScope        *scope,
                             GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_equality (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_AMPERSAND))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_equality (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionBinary *binary_expr;
          binary_expr = gsk_sl_expression_new (GskSlExpressionBinary, &GSK_SL_EXPRESSION_BINARY);
          binary_expr->binary = binary;
          binary_expr->type = gsk_sl_type_ref (result_type);
          binary_expr->left = expression;
          binary_expr->right = right;
          expression = (GskSlExpression *) binary_expr;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_xor (GskSlScope        *scope,
                             GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_and (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_CARET))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_and (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionBinary *binary_expr;
          binary_expr = gsk_sl_expression_new (GskSlExpressionBinary, &GSK_SL_EXPRESSION_BINARY);
          binary_expr->binary = binary;
          binary_expr->type = gsk_sl_type_ref (result_type);
          binary_expr->left = expression;
          binary_expr->right = right;
          expression = (GskSlExpression *) binary_expr;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_or (GskSlScope        *scope,
                            GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_xor (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_VERTICAL_BAR))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_xor (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionBinary *binary_expr;
          binary_expr = gsk_sl_expression_new (GskSlExpressionBinary, &GSK_SL_EXPRESSION_BINARY);
          binary_expr->binary = binary;
          binary_expr->type = gsk_sl_type_ref (result_type);
          binary_expr->left = expression;
          binary_expr->right = right;
          expression = (GskSlExpression *) binary_expr;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_logical_and (GskSlScope        *scope,
                                     GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_or (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_AND_OP))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_or (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionBinary *binary_expr;
          binary_expr = gsk_sl_expression_new (GskSlExpressionBinary, &GSK_SL_EXPRESSION_BINARY);
          binary_expr->binary = binary;
          binary_expr->type = gsk_sl_type_ref (result_type);
          binary_expr->left = expression;
          binary_expr->right = right;
          expression = (GskSlExpression *) binary_expr;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_logical_xor (GskSlScope        *scope,
                                     GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_logical_and (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_XOR_OP))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_logical_and (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionBinary *binary_expr;
          binary_expr = gsk_sl_expression_new (GskSlExpressionBinary, &GSK_SL_EXPRESSION_BINARY);
          binary_expr->binary = binary;
          binary_expr->type = gsk_sl_type_ref (result_type);
          binary_expr->left = expression;
          binary_expr->right = right;
          expression = (GskSlExpression *) binary_expr;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
        }
    }

  return expression;
}

static GskSlExpression *
gsk_sl_expression_parse_logical_or (GskSlScope        *scope,
                                    GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  const GskSlBinary *binary;
  GskSlExpression *expression, *right;
  GskSlType *result_type;

  expression = gsk_sl_expression_parse_logical_xor (scope, stream);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_OR_OP))
        return expression;

      binary = gsk_sl_binary_get_for_token (token->type);
      gsk_sl_preprocessor_consume (stream, NULL);

      right = gsk_sl_expression_parse_logical_xor (scope, stream);

      result_type = gsk_sl_binary_check_type (binary,
                                              stream,
                                              gsk_sl_expression_get_return_type (expression),
                                              gsk_sl_expression_get_return_type (right));
      if (result_type)
        {
          GskSlExpressionLogicalOr *logical_expr;
          logical_expr = gsk_sl_expression_new (GskSlExpressionLogicalOr, &GSK_SL_EXPRESSION_LOGICAL_OR);
          logical_expr->left = expression;
          logical_expr->right = right;
          expression = (GskSlExpression *) logical_expr;
        }
      else
        {
          gsk_sl_expression_unref ((GskSlExpression *) right);
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
                                    GskSlPreprocessor *preproc)
{
  const GskSlBinary *binary;
  const GskSlToken *token;
  GskSlExpression *lvalue, *rvalue;
  GskSlExpressionAssignment *assign;
  GskSlType *result_type;
  GError *error = NULL;

  lvalue = gsk_sl_expression_parse_conditional (scope, preproc);

  token = gsk_sl_preprocessor_get (preproc);
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

  if (!gsk_sl_expression_is_assignable (lvalue, &error))
    {
      gsk_sl_preprocessor_emit_error (preproc,
                                      TRUE,
                                      gsk_sl_preprocessor_get_location (preproc),
                                      error);

      g_clear_error (&error);

      /* Continue parsing like normal here to get more errors */
      gsk_sl_preprocessor_consume (preproc, lvalue);
      gsk_sl_expression_unref (lvalue);

      return gsk_sl_expression_parse_assignment (scope, preproc);
    }

  binary = gsk_sl_binary_get_for_token (token->type);
  gsk_sl_preprocessor_consume (preproc, NULL);

  rvalue = gsk_sl_expression_parse_assignment (scope, preproc);
  if (binary)
    {
      result_type = gsk_sl_binary_check_type (binary,
                                              preproc,
                                              gsk_sl_expression_get_return_type (lvalue),
                                              gsk_sl_expression_get_return_type (rvalue));
      if (result_type == NULL)
        {
          gsk_sl_expression_unref (rvalue);
          return lvalue;
        }
    }
  else
    {
      result_type = gsk_sl_expression_get_return_type (rvalue);
    }
  if (!gsk_sl_type_can_convert (gsk_sl_expression_get_return_type (lvalue), result_type))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Cannot assign value of type %s to variable of type %s",
                                 gsk_sl_type_get_name (result_type),
                                 gsk_sl_type_get_name (gsk_sl_expression_get_return_type (lvalue)));
      gsk_sl_expression_unref (rvalue);
      return lvalue;
    }

  assign = gsk_sl_expression_new (GskSlExpressionAssignment, &GSK_SL_EXPRESSION_ASSIGNMENT);
  assign->binary = binary;
  assign->type = gsk_sl_type_ref (result_type);
  assign->lvalue = lvalue;
  assign->rvalue = rvalue;

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

gboolean
gsk_sl_expression_is_assignable (const GskSlExpression  *expression,
                                 GError                **error)
{
  return expression->class->is_assignable (expression, error);
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
  GskSpvAccessChain *chain;
  GskSlValue *constant;
  guint32 result_id;

  constant = gsk_sl_expression_get_constant (expression);
  if (constant)
    {
      result_id = gsk_spv_writer_get_id_for_value (writer, constant);
      gsk_sl_value_free (constant);

      return result_id;
    }

  chain = gsk_sl_expression_get_spv_access_chain (expression, writer);
  if (chain)
    {
      result_id = gsk_spv_access_chain_load (chain);
      gsk_spv_access_chain_free (chain);
      return result_id;
    }

  return expression->class->write_spv (expression, writer);
}
