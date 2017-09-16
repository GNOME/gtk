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

  GSList *declarations;
  GSList *functions;
};

static void
gsk_sl_node_program_free (GskSlNode *node)
{
  GskSlNodeProgram *program = (GskSlNodeProgram *) node;

  g_slist_free (program->declarations);
  g_slist_free (program->functions);

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

static const GskSlNodeClass GSK_SL_NODE_PROGRAM = {
  gsk_sl_node_program_free,
  gsk_sl_node_program_print
};

/* FUNCTION */

typedef struct _GskSlNodeFunction GskSlNodeFunction;

struct _GskSlNodeFunction {
  GskSlNode parent;

  GskSlType *return_type;
  char *name;
  GSList *statements;
};

static void
gsk_sl_node_function_free (GskSlNode *node)
{
  GskSlNodeFunction *function = (GskSlNodeFunction *) node;

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

static const GskSlNodeClass GSK_SL_NODE_FUNCTION = {
  gsk_sl_node_function_free,
  gsk_sl_node_function_print
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

static const GskSlNodeClass GSK_SL_NODE_ASSIGNMENT = {
  gsk_sl_node_assignment_free,
  gsk_sl_node_assignment_print
};

/* CONSTANT */

typedef struct _GskSlNodeConstant GskSlNodeConstant;

struct _GskSlNodeConstant {
  GskSlNode parent;

  GskSlBuiltinType type;
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
    case GSK_SL_VEC2:
    case GSK_SL_VEC3:
    case GSK_SL_VEC4:
    case GSK_SL_N_BUILTIN_TYPES:
    default:
      g_assert_not_reached ();
      break;
  }
}

static const GskSlNodeClass GSK_SL_NODE_CONSTANT = {
  gsk_sl_node_constant_free,
  gsk_sl_node_constant_print
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
gsk_sl_node_parse_constant (GskSlNodeProgram  *program,
                            GskSlPreprocessor *stream)
{
  GskSlNodeConstant *constant;
  const GskSlToken *token;

  constant = gsk_sl_node_new (GskSlNodeConstant, &GSK_SL_NODE_CONSTANT);

  token = gsk_sl_preprocessor_get (stream);
  switch ((guint) token->type)
  {
    case GSK_SL_TOKEN_INTCONSTANT:
      constant->type = GSK_SL_INT;
      constant->i32 = token->i32;
      break;

    case GSK_SL_TOKEN_UINTCONSTANT:
      constant->type = GSK_SL_UINT;
      constant->u32 = token->u32;
      break;

    case GSK_SL_TOKEN_FLOATCONSTANT:
      constant->type = GSK_SL_FLOAT;
      constant->f = token->f;
      break;

    case GSK_SL_TOKEN_BOOLCONSTANT:
      constant->type = GSK_SL_BOOL;
      constant->b = token->b;
      break;

    case GSK_SL_TOKEN_DOUBLECONSTANT:
      constant->type = GSK_SL_DOUBLE;
      constant->d = token->d;
      break;

    default:
      g_assert_not_reached ();
      return NULL;
  }
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) constant);

  return (GskSlNode *) constant;
}

static GskSlNode *
gsk_sl_node_parse_assignment_expression (GskSlNodeProgram  *program,
                                         GskSlPreprocessor *stream)
{
  const GskSlToken *token;
  GskSlNode *lvalue;
  GskSlNodeAssignment *assign;

  lvalue = gsk_sl_node_parse_constant (program, stream);
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

  assign = gsk_sl_node_new (GskSlNodeAssignment, &GSK_SL_NODE_ASSIGNMENT);
  assign->lvalue = lvalue;
  assign->op = token->type;

  gsk_sl_preprocessor_consume (stream, (GskSlNode *) assign);
  assign->rvalue = gsk_sl_node_parse_assignment_expression (program, stream);
  if (assign->rvalue == NULL)
    {
      gsk_sl_node_unref ((GskSlNode *) assign);
      return lvalue;
    }

  return (GskSlNode *) assign;
}

static GskSlNode *
gsk_sl_node_parse_expression (GskSlNodeProgram  *program,
                              GskSlPreprocessor *stream)
{
  /* XXX: Allow comma here */
  return gsk_sl_node_parse_assignment_expression (program, stream);
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

      case GSK_SL_TOKEN_INTCONSTANT:
      case GSK_SL_TOKEN_UINTCONSTANT:
      case GSK_SL_TOKEN_FLOATCONSTANT:
      case GSK_SL_TOKEN_BOOLCONSTANT:
      case GSK_SL_TOKEN_DOUBLECONSTANT:
      case GSK_SL_TOKEN_LEFT_PAREN:
        node = gsk_sl_node_parse_expression (program, stream);
        if (node)
          function->statements = g_slist_append (function->statements, node);
        else
          result = FALSE;
        break;

      default:
        gsk_sl_preprocessor_error (stream, "Unexpected token in stream.");
        gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);
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
