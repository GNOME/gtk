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

#include "gskslnodeprivate.h"

#include "gskslpreprocessorprivate.h"
#include "gskslfunctionprivate.h"
#include "gskslpointertypeprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskspvwriterprivate.h"

#include <string.h>

static GskSlNode *
gsk_sl_node_alloc (const GskSlNodeClass *klass,
                   gsize                 size)
{
  GskSlNode *node;

  node = g_slice_alloc0 (size);

  node->class = klass;
  node->ref_count = 1;

  return node;
}
#define gsk_sl_node_new(_name, _klass) ((_name *) gsk_sl_node_alloc ((_klass), sizeof (_name)))

/* FUNCTION */

typedef struct _GskSlNodeFunction GskSlNodeFunction;

struct _GskSlNodeFunction {
  GskSlNode parent;

  GskSlScope *scope;
  GskSlType *return_type;
  char *name;
  GSList *statements;
};

static void
gsk_sl_node_function_free (GskSlNode *node)
{
  GskSlNodeFunction *function = (GskSlNodeFunction *) node;

  if (function->scope)
    gsk_sl_scope_unref (function->scope);
  if (function->return_type)
    gsk_sl_type_unref (function->return_type);
  g_free (function->name);
  g_slist_free_full (function->statements, (GDestroyNotify) gsk_sl_node_unref);

  g_slice_free (GskSlNodeFunction, function);
}

static void
gsk_sl_node_function_print (GskSlNode *node,
                            GString   *string)
{
  GskSlNodeFunction *function = (GskSlNodeFunction *) node;
  GSList *l;

  g_string_append (string, gsk_sl_type_get_name (function->return_type));
  g_string_append (string, "\n");

  g_string_append (string, function->name);
  g_string_append (string, " (");
  g_string_append (string, ")\n");

  g_string_append (string, "{\n");
  for (l = function->statements; l; l = l->next)
    {
      g_string_append (string, "  ");
      gsk_sl_node_print (l->data, string);
      g_string_append (string, ";\n");
    }
  g_string_append (string, "}\n");
}

static GskSlType *
gsk_sl_node_function_get_return_type (GskSlNode *node)
{
  GskSlNodeFunction *function = (GskSlNodeFunction *) node;

  return function->return_type;
}

static gboolean
gsk_sl_node_function_is_constant (GskSlNode *node)
{
  return TRUE;
}

static guint32
gsk_sl_node_function_write_spv (const GskSlNode *node,
                                GskSpvWriter    *writer)
{
  GskSlNodeFunction *function = (GskSlNodeFunction *) node;
  guint32 return_type_id, function_type_id, function_id, label_id;
  GSList *l;

  /* declare type of function */
  return_type_id = gsk_spv_writer_get_id_for_type (writer, function->return_type);
  function_type_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_DECLARE,
                      3, GSK_SPV_OP_TYPE_FUNCTION,
                      (guint32[2]) { function_type_id,
                                     return_type_id });

  /* add debug info */
  /* FIXME */

  /* add function body */
  function_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      5, GSK_SPV_OP_FUNCTION,
                      (guint32[4]) { return_type_id,
                                     function_id,
                                     0,
                                     function_type_id });
  label_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      2, GSK_SPV_OP_LABEL,
                      (guint32[4]) { label_id });

  for (l = function->statements; l; l = l->next)
    {
      gsk_sl_node_write_spv (l->data, writer);
    }

  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      1, GSK_SPV_OP_FUNCTION_END,
                      NULL);
  return function_id;
}

static const GskSlNodeClass GSK_SL_NODE_FUNCTION = {
  gsk_sl_node_function_free,
  gsk_sl_node_function_print,
  gsk_sl_node_function_get_return_type,
  gsk_sl_node_function_is_constant,
  gsk_sl_node_function_write_spv
};

/* ASSIGNMENT */

typedef struct _GskSlNodeAssignment GskSlNodeAssignment;

struct _GskSlNodeAssignment {
  GskSlNode parent;

  GskSlTokenType op;
  GskSlNode *lvalue;
  GskSlNode *rvalue;
};

static void
gsk_sl_node_assignment_free (GskSlNode *node)
{
  GskSlNodeAssignment *assignment = (GskSlNodeAssignment *) node;

  gsk_sl_node_unref (assignment->lvalue);
  if (assignment->rvalue)
    gsk_sl_node_unref (assignment->rvalue);

  g_slice_free (GskSlNodeAssignment, assignment);
}

static void
gsk_sl_node_assignment_print (GskSlNode *node,
                              GString   *string)
{
  GskSlNodeAssignment *assignment = (GskSlNodeAssignment *) node;

  gsk_sl_node_print (assignment->lvalue, string);

  switch ((guint) assignment->op)
  {
    case GSK_SL_TOKEN_EQUAL:
      g_string_append (string, " = ");
      break;
    case GSK_SL_TOKEN_MUL_ASSIGN:
      g_string_append (string, " *= ");
      break;
    case GSK_SL_TOKEN_DIV_ASSIGN:
      g_string_append (string, " /= ");
      break;
    case GSK_SL_TOKEN_MOD_ASSIGN:
      g_string_append (string, " %= ");
      break;
    case GSK_SL_TOKEN_ADD_ASSIGN:
      g_string_append (string, " += ");
      break;
    case GSK_SL_TOKEN_SUB_ASSIGN:
      g_string_append (string, " -= ");
      break;
    case GSK_SL_TOKEN_LEFT_ASSIGN:
      g_string_append (string, " <<= ");
      break;
    case GSK_SL_TOKEN_RIGHT_ASSIGN:
      g_string_append (string, " >>= ");
      break;
    case GSK_SL_TOKEN_AND_ASSIGN:
      g_string_append (string, " &= ");
      break;
    case GSK_SL_TOKEN_XOR_ASSIGN:
      g_string_append (string, " ^= ");
      break;
    case GSK_SL_TOKEN_OR_ASSIGN:
      g_string_append (string, " |= ");
      break;
    default:
      g_assert_not_reached ();
      break;
  }
  gsk_sl_node_print (assignment->rvalue, string);
}

static GskSlType *
gsk_sl_node_assignment_get_return_type (GskSlNode *node)
{
  GskSlNodeAssignment *assignment = (GskSlNodeAssignment *) node;

  return gsk_sl_node_get_return_type (assignment->lvalue);
}

static gboolean
gsk_sl_node_assignment_is_constant (GskSlNode *node)
{
  //GskSlNodeAssignment *assignment = (GskSlNodeAssignment *) node;

  return FALSE;
}

static guint32
gsk_sl_node_assignment_write_spv (const GskSlNode *node,
                                  GskSpvWriter    *writer)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlNodeClass GSK_SL_NODE_ASSIGNMENT = {
  gsk_sl_node_assignment_free,
  gsk_sl_node_assignment_print,
  gsk_sl_node_assignment_get_return_type,
  gsk_sl_node_assignment_is_constant,
  gsk_sl_node_assignment_write_spv
};

/* BINARY */

typedef enum {
  GSK_SL_OPERATION_MUL,
  GSK_SL_OPERATION_DIV,
  GSK_SL_OPERATION_MOD,
  GSK_SL_OPERATION_ADD,
  GSK_SL_OPERATION_SUB,
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

typedef struct _GskSlNodeOperation GskSlNodeOperation;

struct _GskSlNodeOperation {
  GskSlNode parent;

  GskSlOperation op;
  GskSlNode *left;
  GskSlNode *right;
};

static void
gsk_sl_node_operation_free (GskSlNode *node)
{
  GskSlNodeOperation *operation = (GskSlNodeOperation *) node;

  gsk_sl_node_unref (operation->left);
  if (operation->right)
    gsk_sl_node_unref (operation->right);

  g_slice_free (GskSlNodeOperation, operation);
}

static void
gsk_sl_node_operation_print (GskSlNode *node,
                             GString   *string)
{
  const char *op_str[] = {
    [GSK_SL_OPERATION_MUL] = " * ",
    [GSK_SL_OPERATION_DIV] = " / ",
    [GSK_SL_OPERATION_MOD] = " % ",
    [GSK_SL_OPERATION_ADD] = " + ",
    [GSK_SL_OPERATION_SUB] = " - ",
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
  GskSlNodeOperation *operation = (GskSlNodeOperation *) node;

  /* XXX: figure out the need for bracketing here */

  gsk_sl_node_print (operation->left, string);
  g_string_append (string, op_str[operation->op]);
  gsk_sl_node_print (operation->right, string);
}

static GskSlType *
gsk_sl_node_arithmetic_type_check (GskSlPreprocessor *stream,
                                   gboolean           multiply,
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
      if (stream)
        {
          gsk_sl_preprocessor_error (stream, "Operand types %s and %s do not share compatible scalar types.",
                                             gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
        }
      return NULL;
    }

  if (gsk_sl_type_is_matrix (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          if (multiply)
            {
              if (gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (gsk_sl_type_get_index_type (rtype)))
                {
                  if (stream)
                    gsk_sl_preprocessor_error (stream, "Matrices to multiplication have incompatible dimensions.");
                  return NULL;
                }
              return gsk_sl_type_get_matrix (scalar,
                                             gsk_sl_type_get_length (gsk_sl_type_get_index_type (ltype)),
                                             gsk_sl_type_get_length (rtype));
            }
          else
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
                  if (stream)
                    gsk_sl_preprocessor_error (stream, "Matrix types %s and %s have different size.",
                                               gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
                  return NULL;
                }
            }
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          if (multiply)
            {
              if (gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
                {
                  if (stream)
                    gsk_sl_preprocessor_error (stream, "Matrix column count doesn't match vector length.");
                  return NULL;
                }
              return gsk_sl_type_get_vector (scalar, gsk_sl_type_get_length (gsk_sl_type_get_index_type (ltype)));
            }
          else
            {
              if (stream)
                gsk_sl_preprocessor_error (stream, "Cannot perform arithmetic operation between matrix and vector.");
              return NULL;
            }
        }
      else if (gsk_sl_type_is_scalar (rtype))
        {
          return gsk_sl_type_get_matrix (scalar,
                                         gsk_sl_type_get_length (ltype),
                                         gsk_sl_type_get_length (gsk_sl_type_get_index_type (ltype)));
        }
      else
        {
          if (stream)
            gsk_sl_preprocessor_error (stream, "Right operand is incompatible type for arithemtic operation.");
          return NULL;
        }
    }
  else if (gsk_sl_type_is_vector (ltype))
    {
      if (gsk_sl_type_is_matrix (rtype))
        {
          if (multiply)
            {
              if (gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (gsk_sl_type_get_index_type (rtype)))
                {
                  if (stream)
                    gsk_sl_preprocessor_error (stream, "Vector length for %s doesn't match row count for %s",
                                                       gsk_sl_type_get_name (ltype), gsk_sl_type_get_name (rtype));
                  return NULL;
                }
              return gsk_sl_type_get_vector (scalar, gsk_sl_type_get_length (rtype));
            }
          else
            {
              if (stream)
                gsk_sl_preprocessor_error (stream, "Cannot perform arithmetic operation between vector and matrix.");
              return NULL;
            }
        }
      else if (gsk_sl_type_is_vector (rtype))
        {
          if (gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
            {
              if (stream)
                gsk_sl_preprocessor_error (stream, "Vector operands %s and %s to arithmetic operation have different length.",
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
          if (stream)
            gsk_sl_preprocessor_error (stream, "Right operand is incompatible type for arithemtic operation.");
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
          if (stream)
            gsk_sl_preprocessor_error (stream, "Right operand is incompatible type for arithemtic operation.");
          return NULL;
        }
    }
  else
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Left operand is incompatible type for arithemtic operation.");
      return NULL;
    }
}

static GskSlType *
gsk_sl_node_bitwise_type_check (GskSlPreprocessor *stream,
                                GskSlType         *ltype,
                                GskSlType         *rtype)
{
  GskSlScalarType lscalar, rscalar;

  lscalar = gsk_sl_type_get_scalar_type (ltype);
  if (lscalar != GSK_SL_INT && lscalar != GSK_SL_UINT)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Left operand %s is not an integer type.", gsk_sl_type_get_name (ltype));
      return NULL;
    }
  rscalar = gsk_sl_type_get_scalar_type (ltype);
  if (rscalar != GSK_SL_INT && rscalar != GSK_SL_UINT)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand %s is not an integer type.", gsk_sl_type_get_name (rtype));
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (ltype) && !gsk_sl_type_is_vector (ltype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Left operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (ltype));
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (rtype) && !gsk_sl_type_is_vector (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (rtype));
      return NULL;
    }
  if (gsk_sl_type_is_vector (ltype) && gsk_sl_type_is_vector (rtype) &&
      gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Vector operands %s and %s do not have the same length.",
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
gsk_sl_node_shift_type_check (GskSlPreprocessor *stream,
                              GskSlType         *ltype,
                              GskSlType         *rtype)
{
  GskSlScalarType lscalar, rscalar;

  lscalar = gsk_sl_type_get_scalar_type (ltype);
  if (lscalar != GSK_SL_INT && lscalar != GSK_SL_UINT)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Left operand %s is not an integer type.", gsk_sl_type_get_name (ltype));
      return FALSE;
    }
  rscalar = gsk_sl_type_get_scalar_type (ltype);
  if (rscalar != GSK_SL_INT && rscalar != GSK_SL_UINT)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand %s is not an integer type.", gsk_sl_type_get_name (rtype));
      return FALSE;
    }
  if (!gsk_sl_type_is_scalar (ltype) && !gsk_sl_type_is_vector (ltype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Left operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (ltype));
      return FALSE;
    }
  if (!gsk_sl_type_is_scalar (rtype) && !gsk_sl_type_is_vector (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand %s is neither a scalar nor a vector.", gsk_sl_type_get_name (rtype));
      return FALSE;
    }
  if (gsk_sl_type_is_scalar (ltype) && gsk_sl_type_is_vector (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand to shift cannot be a vector if left operand is a scalar.");
      return FALSE;
    }
  if (gsk_sl_type_is_vector (ltype) && gsk_sl_type_is_vector (rtype) &&
      gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Vector operands do not have the same length.");
      return FALSE;
    }

  return TRUE;
}

static gboolean
gsk_sl_node_relational_type_check (GskSlPreprocessor *stream,
                                   GskSlType        *ltype,
                                   GskSlType        *rtype)
{
  if (!gsk_sl_type_is_scalar (ltype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Left operand to relational operator is not a scalar.");
      return FALSE;
    }
  if (gsk_sl_type_get_scalar_type (ltype) == GSK_SL_BOOL)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Left operand to relational operator must not be bool.");
      return FALSE;
    }
  if (!gsk_sl_type_is_scalar (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand to relational operator is not a scalar.");
      return FALSE;
    }
  if (gsk_sl_type_get_scalar_type (rtype) == GSK_SL_BOOL)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand to relational operator must not be bool.");
      return FALSE;
    }


  return TRUE;
}

static GskSlType *
gsk_sl_node_operation_get_return_type (GskSlNode *node)
{
  GskSlNodeOperation *operation = (GskSlNodeOperation *) node;

  switch (operation->op)
  {
    case GSK_SL_OPERATION_MUL:
      return gsk_sl_node_arithmetic_type_check (NULL,
                                                TRUE,
                                                gsk_sl_node_get_return_type (operation->left),
                                                gsk_sl_node_get_return_type (operation->right));
    case GSK_SL_OPERATION_DIV:
    case GSK_SL_OPERATION_ADD:
    case GSK_SL_OPERATION_SUB:
      return gsk_sl_node_arithmetic_type_check (NULL,
                                                FALSE,
                                                gsk_sl_node_get_return_type (operation->left),
                                                gsk_sl_node_get_return_type (operation->right));
    case GSK_SL_OPERATION_LSHIFT:
    case GSK_SL_OPERATION_RSHIFT:
      return gsk_sl_node_get_return_type (operation->left);
    case GSK_SL_OPERATION_MOD:
    case GSK_SL_OPERATION_AND:
    case GSK_SL_OPERATION_XOR:
    case GSK_SL_OPERATION_OR:
      return gsk_sl_node_bitwise_type_check (NULL,
                                             gsk_sl_node_get_return_type (operation->left),
                                             gsk_sl_node_get_return_type (operation->right));
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

static gboolean
gsk_sl_node_operation_is_constant (GskSlNode *node)
{
  GskSlNodeOperation *operation = (GskSlNodeOperation *) node;

  return gsk_sl_node_is_constant (operation->left)
      && gsk_sl_node_is_constant (operation->right);
}

static guint32
gsk_sl_node_operation_write_spv (const GskSlNode *node,
                                 GskSpvWriter    *writer)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlNodeClass GSK_SL_NODE_OPERATION = {
  gsk_sl_node_operation_free,
  gsk_sl_node_operation_print,
  gsk_sl_node_operation_get_return_type,
  gsk_sl_node_operation_is_constant,
  gsk_sl_node_operation_write_spv
};

/* DECLARATION */

typedef struct _GskSlNodeDeclaration GskSlNodeDeclaration;

struct _GskSlNodeDeclaration {
  GskSlNode parent;

  char *name;
  GskSlPointerType *type;
  GskSlNode *initial;
  guint constant :1;
};

static void
gsk_sl_node_declaration_free (GskSlNode *node)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  g_free (declaration->name);
  gsk_sl_pointer_type_unref (declaration->type);
  if (declaration->initial)
    gsk_sl_node_unref (declaration->initial);

  g_slice_free (GskSlNodeDeclaration, declaration);
}

static void
gsk_sl_node_declaration_print (GskSlNode *node,
                               GString   *string)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  gsk_sl_pointer_type_print (declaration->type, string);
  if (declaration->name)
    {
      g_string_append (string, " ");
      g_string_append (string, declaration->name);
      if (declaration->initial)
        {
          g_string_append (string, " = ");
          gsk_sl_node_print (declaration->initial, string);
        }
    }
}

static GskSlType *
gsk_sl_node_declaration_get_return_type (GskSlNode *node)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  return gsk_sl_pointer_type_get_type (declaration->type);
}

static gboolean
gsk_sl_node_declaration_is_constant (GskSlNode *node)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  return declaration->constant;
}

static guint32
gsk_sl_node_declaration_write_spv (const GskSlNode *node,
                                   GskSpvWriter    *writer)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;
  guint32 pointer_type_id, declaration_id;

  pointer_type_id = gsk_spv_writer_get_id_for_pointer_type (writer, declaration->type);
  declaration_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      4, GSK_SPV_OP_VARIABLE,
                      (guint32[3]) { pointer_type_id,
                                     declaration_id,
                                     gsk_sl_pointer_type_get_storage_class (declaration->type)});
  gsk_spv_writer_set_id_for_declaration (writer, (GskSlNode *) node, declaration_id);
  
  if (declaration->initial)
    {
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          3, GSK_SPV_OP_STORE,
                          (guint32[2]) { declaration_id,
                                         gsk_sl_node_write_spv (declaration->initial, writer)});
    }

  return declaration_id;
}

static const GskSlNodeClass GSK_SL_NODE_DECLARATION = {
  gsk_sl_node_declaration_free,
  gsk_sl_node_declaration_print,
  gsk_sl_node_declaration_get_return_type,
  gsk_sl_node_declaration_is_constant,
  gsk_sl_node_declaration_write_spv
};

/* REFERENCE */

typedef struct _GskSlNodeReference GskSlNodeReference;

struct _GskSlNodeReference {
  GskSlNode parent;

  char *name;
  GskSlNode *declaration;
};

static void
gsk_sl_node_reference_free (GskSlNode *node)
{
  GskSlNodeReference *reference = (GskSlNodeReference *) node;

  g_free (reference->name);
  gsk_sl_node_unref (reference->declaration);

  g_slice_free (GskSlNodeReference, reference);
}

static void
gsk_sl_node_reference_print (GskSlNode *node,
                             GString   *string)
{
  GskSlNodeReference *reference = (GskSlNodeReference *) node;

  g_string_append (string, reference->name);
}

static GskSlType *
gsk_sl_node_reference_get_return_type (GskSlNode *node)
{
  GskSlNodeReference *reference = (GskSlNodeReference *) node;

  return gsk_sl_node_get_return_type (reference->declaration);
}

static gboolean
gsk_sl_node_reference_is_constant (GskSlNode *node)
{
  GskSlNodeReference *reference = (GskSlNodeReference *) node;

  return gsk_sl_node_is_constant (reference->declaration);
}

static guint32
gsk_sl_node_reference_write_spv (const GskSlNode *node,
                                 GskSpvWriter    *writer)
{
  GskSlNodeReference *reference = (GskSlNodeReference *) node;
  guint32 declaration_id, result_id, type_id;

  type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_node_get_return_type (reference->declaration));
  declaration_id = gsk_spv_writer_get_id_for_declaration (writer, reference->declaration);
  result_id = gsk_spv_writer_next_id (writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      4, GSK_SPV_OP_LOAD,
                      (guint32[3]) { type_id,
                                     result_id,
                                     declaration_id });

  return result_id;
}

static const GskSlNodeClass GSK_SL_NODE_REFERENCE = {
  gsk_sl_node_reference_free,
  gsk_sl_node_reference_print,
  gsk_sl_node_reference_get_return_type,
  gsk_sl_node_reference_is_constant,
  gsk_sl_node_reference_write_spv
};

/* FUNCTION_CALL */

typedef struct _GskSlNodeFunctionCall GskSlNodeFunctionCall;

struct _GskSlNodeFunctionCall {
  GskSlNode parent;

  GskSlFunction *function;
  GskSlNode **arguments;
  guint n_arguments;
};

static void
gsk_sl_node_function_call_free (GskSlNode *node)
{
  GskSlNodeFunctionCall *function_call = (GskSlNodeFunctionCall *) node;
  guint i;

  for (i = 0; i < function_call->n_arguments; i++)
    {
      gsk_sl_node_unref (function_call->arguments[i]);
    }
  g_free (function_call->arguments);

  gsk_sl_function_unref (function_call->function);

  g_slice_free (GskSlNodeFunctionCall, function_call);
}

static void
gsk_sl_node_function_call_print (GskSlNode *node,
                                 GString   *string)
{
  GskSlNodeFunctionCall *function_call = (GskSlNodeFunctionCall *) node;
  guint i;

  gsk_sl_function_print_name (function_call->function, string);
  g_string_append (string, " (");
  
  for (i = 0; i < function_call->n_arguments; i++)
    {
      if (i > 0)
        g_string_append (string, ", ");
      gsk_sl_node_print (function_call->arguments[i], string);
    }

  g_string_append (string, ")");
}

static GskSlType *
gsk_sl_node_function_call_get_return_type (GskSlNode *node)
{
  GskSlNodeFunctionCall *function_call = (GskSlNodeFunctionCall *) node;

  return gsk_sl_function_get_return_type (function_call->function);
}

static gboolean
gsk_sl_node_function_call_is_constant (GskSlNode *node)
{
  return FALSE;
}

static guint32
gsk_sl_node_function_call_write_spv (const GskSlNode *node,
                                     GskSpvWriter    *writer)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlNodeClass GSK_SL_NODE_FUNCTION_CALL = {
  gsk_sl_node_function_call_free,
  gsk_sl_node_function_call_print,
  gsk_sl_node_function_call_get_return_type,
  gsk_sl_node_function_call_is_constant,
  gsk_sl_node_function_call_write_spv
};

/* RETURN */

typedef struct _GskSlNodeReturn GskSlNodeReturn;

struct _GskSlNodeReturn {
  GskSlNode parent;

  GskSlNode *value;
};

static void
gsk_sl_node_return_free (GskSlNode *node)
{
  GskSlNodeReturn *return_node = (GskSlNodeReturn *) node;

  if (return_node->value)
    gsk_sl_node_unref (return_node->value);

  g_slice_free (GskSlNodeReturn, return_node);
}

static void
gsk_sl_node_return_print (GskSlNode *node,
                          GString   *string)
{
  GskSlNodeReturn *return_node = (GskSlNodeReturn *) node;

  g_string_append (string, "return");
  if (return_node->value)
    {
      g_string_append (string, " ");
      gsk_sl_node_print (return_node->value, string);
    }
}

static GskSlType *
gsk_sl_node_return_get_return_type (GskSlNode *node)
{
  GskSlNodeReturn *return_node = (GskSlNodeReturn *) node;

  if (return_node->value)
    return gsk_sl_node_get_return_type (return_node->value);
  else
    return NULL;
}

static gboolean
gsk_sl_node_return_is_constant (GskSlNode *node)
{
  GskSlNodeReturn *return_node = (GskSlNodeReturn *) node;

  if (return_node->value)
    return gsk_sl_node_is_constant (return_node->value);
  else
    return TRUE;
}

static guint32
gsk_sl_node_return_write_spv (const GskSlNode *node,
                              GskSpvWriter    *writer)
{
  g_assert_not_reached ();

  return 0;
}

static const GskSlNodeClass GSK_SL_NODE_RETURN = {
  gsk_sl_node_return_free,
  gsk_sl_node_return_print,
  gsk_sl_node_return_get_return_type,
  gsk_sl_node_return_is_constant,
  gsk_sl_node_return_write_spv
};

/* CONSTANT */

typedef struct _GskSlNodeConstant GskSlNodeConstant;

struct _GskSlNodeConstant {
  GskSlNode parent;

  GskSlScalarType type;
  union {
    gint32       i32;
    guint32      u32;
    float        f;
    double       d;
    gboolean     b;
  };
};

static void
gsk_sl_node_constant_free (GskSlNode *node)
{
  GskSlNodeConstant *constant = (GskSlNodeConstant *) node;

  g_slice_free (GskSlNodeConstant, constant);
}

static void
gsk_sl_node_constant_print (GskSlNode *node,
                            GString   *string)
{
  GskSlNodeConstant *constant = (GskSlNodeConstant *) node;
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  switch (constant->type)
  {
    case GSK_SL_FLOAT:
      g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, constant->f);
      g_string_append (string, buf);
      if (strchr (buf, '.') == NULL)
        g_string_append (string, ".0");
      break;

    case GSK_SL_DOUBLE:
      g_ascii_dtostr (buf, G_ASCII_DTOSTR_BUF_SIZE, constant->d);
      g_string_append (string, buf);
      if (strchr (buf, '.') == NULL)
        g_string_append (string, ".0");
      g_string_append (string, "lf");
      break;

    case GSK_SL_INT:
      g_string_append_printf (string, "%i", (gint) constant->i32);
      break;

    case GSK_SL_UINT:
      g_string_append_printf (string, "%uu", (guint) constant->u32);
      break;

    case GSK_SL_BOOL:
      g_string_append (string, constant->b ? "true" : "false");
      break;

    case GSK_SL_VOID:
    default:
      g_assert_not_reached ();
      break;
  }
}

static GskSlType *
gsk_sl_node_constant_get_return_type (GskSlNode *node)
{
  GskSlNodeConstant *constant = (GskSlNodeConstant *) node;

  return gsk_sl_type_get_scalar (constant->type);
}

static gboolean
gsk_sl_node_constant_is_constant (GskSlNode *node)
{
  return TRUE;
}

static guint32
gsk_sl_node_constant_write_spv (const GskSlNode *node,
                                GskSpvWriter    *writer)
{
  GskSlNodeConstant *constant = (GskSlNodeConstant *) node;
  guint32 type_id, result_id;

  switch (constant->type)
  {
    case GSK_SL_FLOAT:
      type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (GSK_SL_FLOAT));
      result_id = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          4, GSK_SPV_OP_CONSTANT,
                          (guint32[3]) { type_id,
                                         result_id,
                                         *(guint32 *) &constant->f });
      break;

    case GSK_SL_DOUBLE:
      type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (GSK_SL_DOUBLE));
      result_id = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          5, GSK_SPV_OP_CONSTANT,
                          (guint32[4]) { type_id,
                                         result_id,
                                         *(guint32 *) &constant->d,
                                         *(((guint32 *) &constant->d) + 1) });
      break;

    case GSK_SL_INT:
      type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (GSK_SL_INT));
      result_id = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          4, GSK_SPV_OP_CONSTANT,
                          (guint32[3]) { type_id,
                                         result_id,
                                         constant->i32 });
      break;

    case GSK_SL_UINT:
      type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (GSK_SL_UINT));
      result_id = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          4, GSK_SPV_OP_CONSTANT,
                          (guint32[3]) { type_id,
                                         result_id,
                                         constant->u32 });
      break;

    case GSK_SL_BOOL:
      type_id = gsk_spv_writer_get_id_for_type (writer, gsk_sl_type_get_scalar (GSK_SL_BOOL));
      result_id = gsk_spv_writer_next_id (writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_DECLARE,
                          3, constant->b ? GSK_SPV_OP_CONSTANT_TRUE : GSK_SPV_OP_CONSTANT_FALSE,
                          (guint32[2]) { type_id,
                                         result_id });
      break;

    case GSK_SL_VOID:
    default:
      g_assert_not_reached ();
      break;
  }

  return result_id;
}

static const GskSlNodeClass GSK_SL_NODE_CONSTANT = {
  gsk_sl_node_constant_free,
  gsk_sl_node_constant_print,
  gsk_sl_node_constant_get_return_type,
  gsk_sl_node_constant_is_constant,
  gsk_sl_node_constant_write_spv
};

/* API */

static GskSlNodeFunction *
gsk_sl_node_parse_function_prototype (GskSlScope        *scope,
                                      GskSlPreprocessor *stream)
{
  GskSlType *type;
  GskSlNodeFunction *function;
  const GskSlToken *token;

  type = gsk_sl_type_new_parse (stream);
  if (type == NULL)
    return NULL;

  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {
      gsk_sl_preprocessor_error (stream, "Expected a function name");
      gsk_sl_type_unref (type);
      return NULL;
    }

  function = gsk_sl_node_new (GskSlNodeFunction, &GSK_SL_NODE_FUNCTION);
  function->return_type = type;
  function->name = g_strdup (token->str);
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);

  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      gsk_sl_preprocessor_error (stream, "Expected an openening \"(\"");
      gsk_sl_node_unref ((GskSlNode *) function);
      return NULL;
    }
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);

  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    {
      gsk_sl_preprocessor_error (stream, "Expected a closing \")\"");
      gsk_sl_node_unref ((GskSlNode *) function);
      return NULL;
    }
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);

  return function;
}

static GskSlNode *
gsk_sl_node_parse_assignment_expression (GskSlScope        *scope,
                                         GskSlPreprocessor *stream);

static GskSlNode *
gsk_sl_node_parse_constructor_call (GskSlScope        *scope,
                                    GskSlPreprocessor *stream,
                                    GskSlType         *type)
{
  GskSlNodeFunctionCall *call;
  const GskSlToken *token;
  GskSlType **types;
  GError *error = NULL;
  gboolean fail = FALSE;
  guint i;

  call = gsk_sl_node_new (GskSlNodeFunctionCall, &GSK_SL_NODE_FUNCTION_CALL);
  call->function = gsk_sl_function_new_constructor (type);
  g_assert (call->function);

  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      gsk_sl_preprocessor_error (stream, "Expected opening \"(\" when calling constructor");
      gsk_sl_node_unref ((GskSlNode *) call);
      return NULL;
    }
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) call);

  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    {
      GPtrArray *arguments;
  
      arguments = g_ptr_array_new ();
      while (TRUE)
        {
          GskSlNode *node = gsk_sl_node_parse_assignment_expression (scope, stream);

          if (node != NULL)
            g_ptr_array_add (arguments, node);
          else
            fail = TRUE;
          
          token = gsk_sl_preprocessor_get (stream);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_COMMA))
            break;
          gsk_sl_preprocessor_consume (stream, (GskSlNode *) call);
        }

      call->n_arguments = arguments->len;
      call->arguments = (GskSlNode **) g_ptr_array_free (arguments, FALSE);
    }

  types = g_newa (GskSlType *, call->n_arguments);
  for (i = 0; i < call->n_arguments; i++)
    types[i] = gsk_sl_node_get_return_type (call->arguments[i]);
  if (!gsk_sl_function_matches (call->function, types, call->n_arguments, &error))
    {
      gsk_sl_preprocessor_error (stream, "%s", error->message);
      g_clear_error (&error);
      fail = TRUE;
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    {
      gsk_sl_preprocessor_error (stream, "Expected closing \")\" after arguments.");
      gsk_sl_node_unref ((GskSlNode *) call);
      return NULL;
    }
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) call);

  if (fail)
    {
      gsk_sl_node_unref ((GskSlNode *) call);
      return NULL;
    }

  return (GskSlNode *) call;
}

static GskSlNode *
gsk_sl_node_parse_primary_expression (GskSlScope        *scope,
                                      GskSlPreprocessor *stream)
{
  GskSlNodeConstant *constant;
  const GskSlToken *token;

  token = gsk_sl_preprocessor_get (stream);
  switch ((guint) token->type)
  {
    case GSK_SL_TOKEN_IDENTIFIER:
      {
        GskSlNodeReference *reference;
        GskSlNode *decl;

        decl = gsk_sl_scope_lookup_variable (scope, token->str);
        if (decl == NULL)
          {
            gsk_sl_preprocessor_error (stream, "No variable named \"%s\".", token->str);
            gsk_sl_preprocessor_consume (stream, NULL);
            return NULL;
          }

        reference = gsk_sl_node_new (GskSlNodeReference, &GSK_SL_NODE_REFERENCE);
        reference->name = g_strdup (token->str);
        reference->declaration = gsk_sl_node_ref (decl);
        gsk_sl_preprocessor_consume (stream, (GskSlNode *) reference);
        return (GskSlNode *) reference;
      }

    case GSK_SL_TOKEN_INTCONSTANT:
      constant = gsk_sl_node_new (GskSlNodeConstant, &GSK_SL_NODE_CONSTANT);
      constant->type = GSK_SL_INT;
      constant->i32 = token->i32;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) constant);
      return (GskSlNode *) constant;

    case GSK_SL_TOKEN_UINTCONSTANT:
      constant = gsk_sl_node_new (GskSlNodeConstant, &GSK_SL_NODE_CONSTANT);
      constant->type = GSK_SL_UINT;
      constant->u32 = token->u32;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) constant);
      return (GskSlNode *) constant;

    case GSK_SL_TOKEN_FLOATCONSTANT:
      constant = gsk_sl_node_new (GskSlNodeConstant, &GSK_SL_NODE_CONSTANT);
      constant->type = GSK_SL_FLOAT;
      constant->f = token->f;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) constant);
      return (GskSlNode *) constant;

    case GSK_SL_TOKEN_BOOLCONSTANT:
      constant = gsk_sl_node_new (GskSlNodeConstant, &GSK_SL_NODE_CONSTANT);
      constant->type = GSK_SL_BOOL;
      constant->b = token->b;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) constant);
      return (GskSlNode *) constant;

    case GSK_SL_TOKEN_DOUBLECONSTANT:
      constant = gsk_sl_node_new (GskSlNodeConstant, &GSK_SL_NODE_CONSTANT);
      constant->type = GSK_SL_DOUBLE;
      constant->d = token->d;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) constant);
      return (GskSlNode *) constant;

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
        GskSlType *type;

        type = gsk_sl_type_new_parse (stream);
        if (type == NULL)
          return NULL;

        return gsk_sl_node_parse_constructor_call (scope, stream, type);
      }

    default:
      gsk_sl_preprocessor_error (stream, "Expected an expression.");
      gsk_sl_preprocessor_consume (stream, NULL);
      return NULL;
  }
}

static GskSlNode *
gsk_sl_node_parse_postfix_expression (GskSlScope        *scope,
                                      GskSlPreprocessor *stream)
{
  return gsk_sl_node_parse_primary_expression (scope, stream);
}

static GskSlNode *
gsk_sl_node_parse_unary_expression (GskSlScope        *scope,
                                    GskSlPreprocessor *stream)
{
  return gsk_sl_node_parse_postfix_expression (scope, stream);
}

static GskSlNode *
gsk_sl_node_parse_multiplicative_expression (GskSlScope        *scope,
                                             GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;
  GskSlOperation op;

  node = gsk_sl_node_parse_unary_expression (scope, stream);
  if (node == NULL)
    return NULL;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_STAR))
        op = GSK_SL_OPERATION_MUL;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_SLASH))
        op = GSK_SL_OPERATION_DIV;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_PERCENT))
        op = GSK_SL_OPERATION_MOD;
      else
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = op;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_unary_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if ((op == GSK_SL_OPERATION_MOD &&
                !gsk_sl_node_bitwise_type_check (stream,
                                                 gsk_sl_node_get_return_type (operation->left),
                                                 gsk_sl_node_get_return_type (operation->right))) ||
               (op != GSK_SL_OPERATION_MOD &&
                !gsk_sl_node_arithmetic_type_check (stream,
                                                    FALSE,
                                                    gsk_sl_node_get_return_type (operation->left),
                                                    gsk_sl_node_get_return_type (operation->right))))
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_additive_expression (GskSlScope        *scope,
                                       GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;
  GskSlOperation op;

  node = gsk_sl_node_parse_multiplicative_expression (scope, stream);
  if (node == NULL)
    return NULL;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_PLUS))
        op = GSK_SL_OPERATION_ADD;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_DASH))
        op = GSK_SL_OPERATION_SUB;
      else
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = op;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_additive_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_node_arithmetic_type_check (stream,
                                                   FALSE,
                                                   gsk_sl_node_get_return_type (operation->left),
                                                   gsk_sl_node_get_return_type (operation->right)))
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_shift_expression (GskSlScope        *scope,
                                    GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;
  GskSlOperation op;

  node = gsk_sl_node_parse_additive_expression (scope, stream);
  if (node == NULL)
    return NULL;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_OP))
        op = GSK_SL_OPERATION_LSHIFT;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_OP))
        op = GSK_SL_OPERATION_RSHIFT;
      else
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = op;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_additive_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_node_shift_type_check (stream,
                                              gsk_sl_node_get_return_type (operation->left),
                                              gsk_sl_node_get_return_type (operation->right)))
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_relational_expression (GskSlScope        *scope,
                                         GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;
  GskSlOperation op;

  node = gsk_sl_node_parse_shift_expression (scope, stream);
  if (node == NULL)
    return NULL;

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
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = op;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_shift_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_node_relational_type_check (stream,
                                                   gsk_sl_node_get_return_type (operation->left),
                                                   gsk_sl_node_get_return_type (operation->right)))
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_equality_expression (GskSlScope        *scope,
                                       GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;
  GskSlOperation op;

  node = gsk_sl_node_parse_relational_expression (scope, stream);
  if (node == NULL)
    return NULL;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_EQ_OP))
        op = GSK_SL_OPERATION_EQUAL;
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_NE_OP))
        op = GSK_SL_OPERATION_NOT_EQUAL;
      else
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = op;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_relational_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_and_expression (GskSlScope        *scope,
                                  GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_equality_expression (scope, stream);
  if (node == NULL)
    return NULL;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_AMPERSAND))
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = GSK_SL_OPERATION_AND;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_equality_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_node_bitwise_type_check (stream,
                                                gsk_sl_node_get_return_type (operation->left),
                                                gsk_sl_node_get_return_type (operation->right)))
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_xor_expression (GskSlScope        *scope,
                                  GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_and_expression (scope, stream);
  if (node == NULL)
    return NULL;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_CARET))
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = GSK_SL_OPERATION_XOR;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_and_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_node_bitwise_type_check (stream,
                                                gsk_sl_node_get_return_type (operation->left),
                                                gsk_sl_node_get_return_type (operation->right)))
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_or_expression (GskSlScope        *scope,
                                 GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_xor_expression (scope, stream);
  if (node == NULL)
    return NULL;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_VERTICAL_BAR))
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = GSK_SL_OPERATION_OR;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_xor_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_node_bitwise_type_check (stream,
                                                gsk_sl_node_get_return_type (operation->left),
                                                gsk_sl_node_get_return_type (operation->right)))
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_logical_and_expression (GskSlScope        *scope,
                                          GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_or_expression (scope, stream);
  if (node == NULL)
    return NULL;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_AND_OP))
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = GSK_SL_OPERATION_LOGICAL_AND;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_or_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (operation->right)))
        {
          gsk_sl_preprocessor_error (stream, "Right operand of && expression is not bool but %s",
                                             gsk_sl_type_get_name (gsk_sl_node_get_return_type (operation->right)));
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (node)))
        {
          gsk_sl_preprocessor_error (stream, "Left operand of && expression is not bool but %s",
                                             gsk_sl_type_get_name (gsk_sl_node_get_return_type (node)));
          node = operation->right;
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_logical_xor_expression (GskSlScope        *scope,
                                          GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_logical_and_expression (scope, stream);
  if (node == NULL)
    return NULL;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_XOR_OP))
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = GSK_SL_OPERATION_LOGICAL_XOR;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_logical_and_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (operation->right)))
        {
          gsk_sl_preprocessor_error (stream, "Right operand of ^^ expression is not bool but %s",
                                             gsk_sl_type_get_name (gsk_sl_node_get_return_type (operation->right)));
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (node)))
        {
          gsk_sl_preprocessor_error (stream, "Left operand of ^^ expression is not bool but %s",
                                             gsk_sl_type_get_name (gsk_sl_node_get_return_type (node)));
          node = operation->right;
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_logical_or_expression (GskSlScope        *scope,
                                         GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_logical_xor_expression (scope, stream);
  if (node == NULL)
    return NULL;

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      if (!gsk_sl_token_is (token, GSK_SL_TOKEN_OR_OP))
        return node;

      operation = gsk_sl_node_new (GskSlNodeOperation, &GSK_SL_NODE_OPERATION);
      operation->left = node;
      operation->op = GSK_SL_OPERATION_LOGICAL_OR;
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) operation);
      operation->right = gsk_sl_node_parse_logical_xor_expression (scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (operation->right)))
        {
          gsk_sl_preprocessor_error (stream, "Right operand of || expression is not bool but %s",
                                             gsk_sl_type_get_name (gsk_sl_node_get_return_type (operation->right)));
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (node)))
        {
          gsk_sl_preprocessor_error (stream, "Left operand of || expression is not bool but %s",
                                             gsk_sl_type_get_name (gsk_sl_node_get_return_type (node)));
          node = operation->right;
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else
        {
          node = (GskSlNode *) operation;
        }
    }

  return node;
}

static GskSlNode *
gsk_sl_node_parse_conditional_expression (GskSlScope        *scope,
                                          GskSlPreprocessor *stream)
{
  /* XXX: support conditionals */
  return gsk_sl_node_parse_logical_or_expression (scope, stream);
}

static GskSlNode *
gsk_sl_node_parse_assignment_expression (GskSlScope        *scope,
                                         GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *lvalue;
  GskSlNodeAssignment *assign;

  lvalue = gsk_sl_node_parse_conditional_expression (scope, stream);
  if (lvalue == NULL)
    return NULL;

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

  if (gsk_sl_node_is_constant (lvalue))
    {
      gsk_sl_preprocessor_error (stream, "Cannot assign to a return lvalue.");

      /* Continue parsing like normal here to get more errors */
      gsk_sl_preprocessor_consume (stream, lvalue);
      gsk_sl_node_unref (lvalue);

      return gsk_sl_node_parse_assignment_expression (scope, stream);
    }

  assign = gsk_sl_node_new (GskSlNodeAssignment, &GSK_SL_NODE_ASSIGNMENT);
  assign->lvalue = lvalue;
  assign->op = token->type;

  gsk_sl_preprocessor_consume (stream, (GskSlNode *) assign);

  assign->rvalue = gsk_sl_node_parse_assignment_expression (scope, stream);
  if (assign->rvalue == NULL)
    {
      gsk_sl_node_unref ((GskSlNode *) assign);
      return lvalue;
    }

  return (GskSlNode *) assign;
}

static GskSlNode *
gsk_sl_node_parse_expression (GskSlScope        *scope,
                              GskSlPreprocessor *stream)
{
  /* XXX: Allow comma here */
  return gsk_sl_node_parse_assignment_expression (scope, stream);
}

static GskSlNode *
gsk_sl_node_parse_declaration (GskSlScope        *scope,
                               GskSlPreprocessor *stream,
                               GskSlPointerType  *type)
{
  GskSlNodeDeclaration *declaration;
  const GskSlToken *token;

  declaration = gsk_sl_node_new (GskSlNodeDeclaration, &GSK_SL_NODE_DECLARATION);
  declaration->type = gsk_sl_pointer_type_ref (type);
  
  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    return (GskSlNode *) declaration;

  declaration->name = g_strdup (token->str);
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) declaration);

  token = gsk_sl_preprocessor_get (stream);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_EQUAL))
    {
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) declaration);
      declaration->initial = gsk_sl_node_parse_assignment_expression (scope, stream);
    }

  gsk_sl_scope_add_variable (scope, declaration->name, (GskSlNode *) declaration);

  return (GskSlNode *) declaration;
}

GskSlNode *
gsk_sl_node_parse_function_definition (GskSlScope        *scope,
                                       GskSlPreprocessor *stream)
{
  GskSlNodeFunction *function;
  const GskSlToken *token;
  GskSlNode *node;
  gboolean result = TRUE;

  function = gsk_sl_node_parse_function_prototype (scope, stream);
  if (function == NULL)
    return FALSE;

  token = gsk_sl_preprocessor_get (stream);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
    {
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);
      return (GskSlNode *) function;
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_BRACE))
    {
      gsk_sl_preprocessor_error (stream, "Expected an opening \"{\"");
      gsk_sl_node_unref ((GskSlNode *) function);
      return FALSE;
    }
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);

  function->scope = gsk_sl_scope_new (scope);

  while (TRUE)
    {
      token = gsk_sl_preprocessor_get (stream);
      switch ((guint) token->type)
      {
      case GSK_SL_TOKEN_SEMICOLON:
        gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);
        break;

      case GSK_SL_TOKEN_EOF:
        gsk_sl_preprocessor_error (stream, "Unexpected end of function, expected \"}\"");
        goto out;

      case GSK_SL_TOKEN_RIGHT_BRACE:
        goto out;

      case GSK_SL_TOKEN_CONST:
      case GSK_SL_TOKEN_IN:
      case GSK_SL_TOKEN_OUT:
      case GSK_SL_TOKEN_INOUT:
      case GSK_SL_TOKEN_INVARIANT:
      case GSK_SL_TOKEN_COHERENT:
      case GSK_SL_TOKEN_VOLATILE:
      case GSK_SL_TOKEN_RESTRICT:
      case GSK_SL_TOKEN_READONLY:
      case GSK_SL_TOKEN_WRITEONLY:
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
          GskSlType *type;
          GskSlPointerTypeFlags flags;
          gboolean success;

          success = gsk_sl_type_qualifier_parse (stream,
                                                 GSK_SL_POINTER_TYPE_PARAMETER_QUALIFIER 
                                                 | GSK_SL_POINTER_TYPE_MEMORY_QUALIFIER,
                                                 &flags);

          type = gsk_sl_type_new_parse (stream);
          if (type == NULL)
            break;

          token = gsk_sl_preprocessor_get (stream);

          if (token->type == GSK_SL_TOKEN_LEFT_BRACE)
            {
              node = gsk_sl_node_parse_constructor_call (function->scope, stream, type);
            }
          else
            {
              GskSlPointerType *pointer_type;
          
              pointer_type = gsk_sl_pointer_type_new (type, flags | GSK_SL_POINTER_TYPE_LOCAL);
              node = gsk_sl_node_parse_declaration (function->scope, stream, pointer_type);
              gsk_sl_pointer_type_unref (pointer_type);
            }

          gsk_sl_type_unref (type);

          if (!success)
            {
              gsk_sl_node_unref (node);
            }
          else if (node)
            {
              function->statements = g_slist_append (function->statements, node);
            }
        }
        break;

      case GSK_SL_TOKEN_RETURN:
        {
          GskSlNodeReturn *return_node;

          return_node = gsk_sl_node_new (GskSlNodeReturn, &GSK_SL_NODE_RETURN);
          gsk_sl_preprocessor_consume (stream, (GskSlNode *) return_node);
          token = gsk_sl_preprocessor_get (stream);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
            {
              return_node->value = gsk_sl_node_parse_expression (function->scope, stream);
              if (return_node->value == NULL)
                {
                  gsk_sl_node_unref ((GskSlNode *) return_node);
                  break;
                }
              if (function->return_type == NULL)
                {
                  gsk_sl_preprocessor_error (stream, "Cannot return a value from a void function.");
                  gsk_sl_node_unref ((GskSlNode *) return_node);
                  break;
                }
              else if (!gsk_sl_type_can_convert (function->return_type, gsk_sl_node_get_return_type (return_node->value)))
                {
                  gsk_sl_preprocessor_error (stream, "Cannot convert return type %s to function type %s.",
                                                     gsk_sl_type_get_name (gsk_sl_node_get_return_type (return_node->value)),
                                                     gsk_sl_type_get_name (function->return_type));
                  gsk_sl_node_unref ((GskSlNode *) return_node);
                  break;
                }
              }
            else
              {
                if (function->return_type != NULL)
                  {
                    gsk_sl_preprocessor_error (stream, "Return statement does not return a value.");
                    gsk_sl_node_unref ((GskSlNode *) return_node);
                    break;
                  }
              }
            function->statements = g_slist_append (function->statements, return_node);
          }
        break;

      default:
        node = gsk_sl_node_parse_expression (function->scope, stream);
        if (node)
          function->statements = g_slist_append (function->statements, node);
        else
          result = FALSE;
        break;
      }
    }

out:
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);

  if (result)
    {
      return (GskSlNode *) function;
    }
  else
    {
      gsk_sl_node_unref ((GskSlNode *) function);
      return NULL;
    }
}

GskSlNode *
gsk_sl_node_ref (GskSlNode *node)
{
  g_return_val_if_fail (node != NULL, NULL);

  node->ref_count += 1;

  return node;
}

void
gsk_sl_node_unref (GskSlNode *node)
{
  if (node == NULL)
    return;

  node->ref_count -= 1;
  if (node->ref_count > 0)
    return;

  node->class->free (node);
}

void
gsk_sl_node_print (GskSlNode *node,
                   GString   *string)
{
  node->class->print (node, string);
}

GskSlType *
gsk_sl_node_get_return_type (GskSlNode *node)
{
  return node->class->get_return_type (node);
}

gboolean
gsk_sl_node_is_constant (GskSlNode *node)
{
  return node->class->is_constant (node);
}

guint32
gsk_sl_node_write_spv (const GskSlNode *node,
                       GskSpvWriter    *writer)
{
  return node->class->write_spv (node, writer);
}

