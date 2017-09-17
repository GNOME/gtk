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
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"

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

/* PROGRAM */

typedef struct _GskSlNodeProgram GskSlNodeProgram;

struct _GskSlNodeProgram {
  GskSlNode parent;

  GskSlScope *scope;
  GSList *declarations;
  GSList *functions;
};

static void
gsk_sl_node_program_free (GskSlNode *node)
{
  GskSlNodeProgram *program = (GskSlNodeProgram *) node;

  g_slist_free (program->declarations);
  g_slist_free (program->functions);
  gsk_sl_scope_unref (program->scope);

  g_slice_free (GskSlNodeProgram, program);
}

static void
gsk_sl_node_program_print (GskSlNode *node,
                           GString   *string)
{
  GskSlNodeProgram *program = (GskSlNodeProgram *) node;
  GSList *l;

  for (l = program->declarations; l; l = l->next)
    gsk_sl_node_print (l->data, string);

  for (l = program->functions; l; l = l->next)
    gsk_sl_node_print (l->data, string);
}

static GskSlType *
gsk_sl_node_program_get_return_type (GskSlNode *node)
{
  return NULL;
}

static gboolean
gsk_sl_node_program_is_constant (GskSlNode *node)
{
  return TRUE;
}

static const GskSlNodeClass GSK_SL_NODE_PROGRAM = {
  gsk_sl_node_program_free,
  gsk_sl_node_program_print,
  gsk_sl_node_program_get_return_type,
  gsk_sl_node_program_is_constant
};

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

  gsk_sl_type_print (function->return_type, string);
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

static const GskSlNodeClass GSK_SL_NODE_FUNCTION = {
  gsk_sl_node_function_free,
  gsk_sl_node_function_print,
  gsk_sl_node_function_get_return_type,
  gsk_sl_node_function_is_constant
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

static const GskSlNodeClass GSK_SL_NODE_ASSIGNMENT = {
  gsk_sl_node_assignment_free,
  gsk_sl_node_assignment_print,
  gsk_sl_node_assignment_get_return_type,
  gsk_sl_node_assignment_is_constant
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
          char *lstr = gsk_sl_type_to_string (ltype);
          char *rstr = gsk_sl_type_to_string (rtype);
          gsk_sl_preprocessor_error (stream, "Operand types %s and %s do not share compatible scalar types.", lstr, rstr);
          g_free (lstr);
          g_free (rstr);
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
                    gsk_sl_preprocessor_error (stream, "Matrices to arithmetic operation have different size.");
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
                    gsk_sl_preprocessor_error (stream, "Vector length doesn't match matrix row count.");
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
                gsk_sl_preprocessor_error (stream, "Vector operands to arithmetic operation have different length.");
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
        gsk_sl_preprocessor_error (stream, "Left operand is not an integer type.");
      return NULL;
    }
  rscalar = gsk_sl_type_get_scalar_type (ltype);
  if (rscalar != GSK_SL_INT && rscalar != GSK_SL_UINT)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand is not an integer type.");
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (ltype) && !gsk_sl_type_is_vector (ltype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Left operand is neither a scalar nor a vector.");
      return NULL;
    }
  if (!gsk_sl_type_is_scalar (rtype) && !gsk_sl_type_is_vector (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand is neither a scalar nor a vector.");
      return NULL;
    }
  if (gsk_sl_type_is_vector (ltype) && gsk_sl_type_is_vector (rtype) &&
      gsk_sl_type_get_length (ltype) != gsk_sl_type_get_length (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Vector operands do not have the same length.");
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
        gsk_sl_preprocessor_error (stream, "Left operand is not an integer type.");
      return FALSE;
    }
  rscalar = gsk_sl_type_get_scalar_type (ltype);
  if (rscalar != GSK_SL_INT && rscalar != GSK_SL_UINT)
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand is not an integer type.");
      return FALSE;
    }
  if (!gsk_sl_type_is_scalar (ltype) && !gsk_sl_type_is_vector (ltype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Left operand is neither a scalar nor a vector.");
      return FALSE;
    }
  if (!gsk_sl_type_is_scalar (rtype) && !gsk_sl_type_is_vector (rtype))
    {
      if (stream)
        gsk_sl_preprocessor_error (stream, "Right operand is neither a scalar nor a vector.");
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

static const GskSlNodeClass GSK_SL_NODE_OPERATION = {
  gsk_sl_node_operation_free,
  gsk_sl_node_operation_print,
  gsk_sl_node_operation_get_return_type,
  gsk_sl_node_operation_is_constant
};

/* DECLARATION */

typedef struct _GskSlNodeDeclaration GskSlNodeDeclaration;

struct _GskSlNodeDeclaration {
  GskSlNode parent;

  char *name;
  GskSlType *type;
  GskSlNode *initial;
  guint constant :1;
};

static void
gsk_sl_node_declaration_free (GskSlNode *node)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  g_free (declaration->name);
  gsk_sl_type_unref (declaration->type);
  if (declaration->initial)
    gsk_sl_node_unref (declaration->initial);

  g_slice_free (GskSlNodeDeclaration, declaration);
}

static void
gsk_sl_node_declaration_print (GskSlNode *node,
                               GString   *string)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  gsk_sl_type_print (declaration->type, string);
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

  return declaration->type;
}

static gboolean
gsk_sl_node_declaration_is_constant (GskSlNode *node)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  return declaration->constant;
}

static const GskSlNodeClass GSK_SL_NODE_DECLARATION = {
  gsk_sl_node_declaration_free,
  gsk_sl_node_declaration_print,
  gsk_sl_node_declaration_get_return_type,
  gsk_sl_node_declaration_is_constant
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

static const GskSlNodeClass GSK_SL_NODE_REFERENCE = {
  gsk_sl_node_reference_free,
  gsk_sl_node_reference_print,
  gsk_sl_node_reference_get_return_type,
  gsk_sl_node_reference_is_constant
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

static const GskSlNodeClass GSK_SL_NODE_CONSTANT = {
  gsk_sl_node_constant_free,
  gsk_sl_node_constant_print,
  gsk_sl_node_constant_get_return_type,
  gsk_sl_node_constant_is_constant
};

/* API */

static GskSlNodeFunction *
gsk_sl_node_parse_function_prototype (GskSlNodeProgram  *program,
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
gsk_sl_node_parse_primary_expression (GskSlNodeProgram  *program,
                                      GskSlScope        *scope,
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
            gsk_sl_preprocessor_consume (stream, (GskSlNode *) program);
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

    default:
      gsk_sl_preprocessor_error (stream, "Expected an expression.");
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) program);
      return NULL;
  }
}

static GskSlNode *
gsk_sl_node_parse_unary_expression (GskSlNodeProgram  *program,
                                    GskSlScope        *scope,
                                    GskSlPreprocessor *stream)
{
  return gsk_sl_node_parse_primary_expression (program, scope, stream);
}

static GskSlNode *
gsk_sl_node_parse_multiplicative_expression (GskSlNodeProgram  *program,
                                             GskSlScope        *scope,
                                             GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;
  GskSlOperation op;

  node = gsk_sl_node_parse_unary_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_unary_expression (program, scope, stream);
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
gsk_sl_node_parse_additive_expression (GskSlNodeProgram  *program,
                                       GskSlScope        *scope,
                                       GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;
  GskSlOperation op;

  node = gsk_sl_node_parse_multiplicative_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_additive_expression (program, scope, stream);
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
gsk_sl_node_parse_shift_expression (GskSlNodeProgram  *program,
                                    GskSlScope        *scope,
                                    GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;
  GskSlOperation op;

  node = gsk_sl_node_parse_additive_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_additive_expression (program, scope, stream);
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
gsk_sl_node_parse_relational_expression (GskSlNodeProgram  *program,
                                         GskSlScope        *scope,
                                         GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;
  GskSlOperation op;

  node = gsk_sl_node_parse_shift_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_shift_expression (program, scope, stream);
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
gsk_sl_node_parse_equality_expression (GskSlNodeProgram  *program,
                                       GskSlScope        *scope,
                                       GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;
  GskSlOperation op;

  node = gsk_sl_node_parse_relational_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_relational_expression (program, scope, stream);
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
gsk_sl_node_parse_and_expression (GskSlNodeProgram  *program,
                                  GskSlScope        *scope,
                                  GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_equality_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_equality_expression (program, scope, stream);
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
gsk_sl_node_parse_xor_expression (GskSlNodeProgram  *program,
                                  GskSlScope        *scope,
                                  GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_and_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_and_expression (program, scope, stream);
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
gsk_sl_node_parse_or_expression (GskSlNodeProgram  *program,
                                 GskSlScope        *scope,
                                 GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_xor_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_xor_expression (program, scope, stream);
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
gsk_sl_node_parse_logical_and_expression (GskSlNodeProgram  *program,
                                          GskSlScope        *scope,
                                          GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_or_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_or_expression (program, scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (operation->right)))
        {
          char *type_name = gsk_sl_type_to_string (gsk_sl_node_get_return_type (operation->right));
          gsk_sl_preprocessor_error (stream, "Right operand of && expression is not bool but %s", type_name);
          g_free (type_name);
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (node)))
        {
          char *type_name = gsk_sl_type_to_string (gsk_sl_node_get_return_type (node));
          gsk_sl_preprocessor_error (stream, "Left operand of && expression is not bool but %s", type_name);
          g_free (type_name);
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
gsk_sl_node_parse_logical_xor_expression (GskSlNodeProgram  *program,
                                          GskSlScope        *scope,
                                          GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_logical_and_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_logical_and_expression (program, scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (operation->right)))
        {
          char *type_name = gsk_sl_type_to_string (gsk_sl_node_get_return_type (operation->right));
          gsk_sl_preprocessor_error (stream, "Right operand of || expression is not bool but %s", type_name);
          g_free (type_name);
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (node)))
        {
          char *type_name = gsk_sl_type_to_string (gsk_sl_node_get_return_type (node));
          gsk_sl_preprocessor_error (stream, "Left operand of || expression is not bool but %s", type_name);
          g_free (type_name);
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
gsk_sl_node_parse_logical_or_expression (GskSlNodeProgram  *program,
                                         GskSlScope        *scope,
                                         GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *node;
  GskSlNodeOperation *operation;

  node = gsk_sl_node_parse_logical_xor_expression (program, scope, stream);
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
      operation->right = gsk_sl_node_parse_logical_xor_expression (program, scope, stream);
      if (operation->right == NULL)
        {
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (operation->right)))
        {
          char *type_name = gsk_sl_type_to_string (gsk_sl_node_get_return_type (operation->right));
          gsk_sl_preprocessor_error (stream, "Right operand of ^^ expression is not bool but %s", type_name);
          g_free (type_name);
          gsk_sl_node_ref (node);
          gsk_sl_node_unref ((GskSlNode *) operation);
        }
      else if (!gsk_sl_type_can_convert (gsk_sl_type_get_scalar (GSK_SL_BOOL),
                                         gsk_sl_node_get_return_type (node)))
        {
          char *type_name = gsk_sl_type_to_string (gsk_sl_node_get_return_type (node));
          gsk_sl_preprocessor_error (stream, "Left operand of ^^ expression is not bool but %s", type_name);
          g_free (type_name);
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
gsk_sl_node_parse_conditional_expression (GskSlNodeProgram  *program,
                                          GskSlScope        *scope,
                                          GskSlPreprocessor *stream)
{
  /* XXX: support conditionals */
  return gsk_sl_node_parse_logical_or_expression (program, scope, stream);
}

static GskSlNode *
gsk_sl_node_parse_assignment_expression (GskSlNodeProgram  *program,
                                         GskSlScope        *scope,
                                         GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *lvalue;
  GskSlNodeAssignment *assign;

  lvalue = gsk_sl_node_parse_conditional_expression (program, scope, stream);
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
      gsk_sl_preprocessor_error (stream, "Cannot assign to a constant lvalue.");

      /* Continue parsing like normal here to get more errors */
      gsk_sl_preprocessor_consume (stream, lvalue);
      gsk_sl_node_unref (lvalue);

      return gsk_sl_node_parse_assignment_expression (program, scope, stream);
    }

  assign = gsk_sl_node_new (GskSlNodeAssignment, &GSK_SL_NODE_ASSIGNMENT);
  assign->lvalue = lvalue;
  assign->op = token->type;

  gsk_sl_preprocessor_consume (stream, (GskSlNode *) assign);

  assign->rvalue = gsk_sl_node_parse_assignment_expression (program, scope, stream);
  if (assign->rvalue == NULL)
    {
      gsk_sl_node_unref ((GskSlNode *) assign);
      return lvalue;
    }

  return (GskSlNode *) assign;
}

static GskSlNode *
gsk_sl_node_parse_expression (GskSlNodeProgram  *program,
                              GskSlScope        *scope,
                              GskSlPreprocessor *stream)
{
  /* XXX: Allow comma here */
  return gsk_sl_node_parse_assignment_expression (program, scope, stream);
}

static GskSlNode *
gsk_sl_node_parse_declaration (GskSlNodeProgram  *program,
                               GskSlScope        *scope,
                               GskSlPreprocessor *stream)
{
  GskSlNodeDeclaration *declaration;
  const GskSlToken *token;
  GskSlType *type;

  type = gsk_sl_type_new_parse (stream);
  if (type == NULL)
    return FALSE;

  declaration = gsk_sl_node_new (GskSlNodeDeclaration, &GSK_SL_NODE_DECLARATION);
  declaration->type = type;
  
  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    return (GskSlNode *) declaration;

  declaration->name = g_strdup (token->str);
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) declaration);

  token = gsk_sl_preprocessor_get (stream);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_EQUAL))
    {
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) declaration);
      declaration->initial = gsk_sl_node_parse_assignment_expression (program, scope, stream);
    }

  gsk_sl_scope_add_variable (scope, declaration->name, (GskSlNode *) declaration);

  return (GskSlNode *) declaration;
}

static gboolean
gsk_sl_node_parse_function_definition (GskSlNodeProgram  *program,
                                       GskSlPreprocessor *stream)
{
  GskSlNodeFunction *function;
  const GskSlToken *token;
  GskSlNode *node;
  gboolean result = TRUE;

  function = gsk_sl_node_parse_function_prototype (program, stream);
  if (function == NULL)
    return FALSE;

  token = gsk_sl_preprocessor_get (stream);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
    {
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);
      program->functions = g_slist_prepend (program->functions, function);
      return TRUE;
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_BRACE))
    {
      gsk_sl_preprocessor_error (stream, "Expected an opening \"{\"");
      gsk_sl_node_unref ((GskSlNode *) function);
      return FALSE;
    }
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);

  function->scope = gsk_sl_scope_new (program->scope);

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
        node = gsk_sl_node_parse_declaration (program, function->scope, stream);
        if (node)
          {
            function->statements = g_slist_append (function->statements, node);
          }
        break;

      default:
        node = gsk_sl_node_parse_expression (program, function->scope, stream);
        if (node)
          function->statements = g_slist_append (function->statements, node);
        else
          result = FALSE;
        break;
      }
    }

out:
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);

  program->functions = g_slist_prepend (program->functions, function);
  return result;
}

static gboolean
gsk_sl_node_parse_program (GskSlNodeProgram  *program,
                           GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  gboolean result = TRUE;

  for (token = gsk_sl_preprocessor_get (stream);
       !gsk_sl_token_is (token, GSK_SL_TOKEN_EOF);
       token = gsk_sl_preprocessor_get (stream))
    {
      if (!gsk_sl_node_parse_function_definition (program, stream))
        {
          gsk_sl_preprocessor_consume (stream, (GskSlNode *) program);
          result = FALSE;
        }
    }

  return result;
}


GskSlNode *
gsk_sl_node_new_program (GBytes  *source,
                         GError **error)
{
  GskSlPreprocessor *stream;
  GskSlNodeProgram *program;

  program = gsk_sl_node_new (GskSlNodeProgram, &GSK_SL_NODE_PROGRAM);
  program->scope = gsk_sl_scope_new (NULL);

  stream = gsk_sl_preprocessor_new (source);

  gsk_sl_node_parse_program (program, stream);

  gsk_sl_preprocessor_unref (stream);

  return (GskSlNode *) program;
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
