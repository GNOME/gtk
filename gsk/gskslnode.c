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

#include "gskslexpressionprivate.h"
#include "gskslfunctionprivate.h"
#include "gskslpointertypeprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvariableprivate.h"
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
  gsk_sl_node_function_write_spv
};

/* DECLARATION */

typedef struct _GskSlNodeDeclaration GskSlNodeDeclaration;

struct _GskSlNodeDeclaration {
  GskSlNode parent;

  GskSlVariable *variable;
  GskSlExpression *initial;
};

static void
gsk_sl_node_declaration_free (GskSlNode *node)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  gsk_sl_variable_unref (declaration->variable);
  if (declaration->initial)
    gsk_sl_expression_unref (declaration->initial);

  g_slice_free (GskSlNodeDeclaration, declaration);
}

static void
gsk_sl_node_declaration_print (GskSlNode *node,
                               GString   *string)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;

  gsk_sl_variable_print (declaration->variable, string);
  if (declaration->initial)
    {
      g_string_append (string, " = ");
      gsk_sl_expression_print (declaration->initial, string);
    }
}

static guint32
gsk_sl_node_declaration_write_spv (const GskSlNode *node,
                                   GskSpvWriter    *writer)
{
  GskSlNodeDeclaration *declaration = (GskSlNodeDeclaration *) node;
  guint32 variable_id;

  variable_id = gsk_spv_writer_get_id_for_variable (writer, declaration->variable);
  
  if (declaration->initial)
    {
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          3, GSK_SPV_OP_STORE,
                          (guint32[2]) { variable_id,
                                         gsk_sl_expression_write_spv (declaration->initial, writer)});
    }

  return variable_id;
}

static const GskSlNodeClass GSK_SL_NODE_DECLARATION = {
  gsk_sl_node_declaration_free,
  gsk_sl_node_declaration_print,
  gsk_sl_node_declaration_write_spv
};

/* RETURN */

typedef struct _GskSlNodeReturn GskSlNodeReturn;

struct _GskSlNodeReturn {
  GskSlNode parent;

  GskSlExpression *value;
};

static void
gsk_sl_node_return_free (GskSlNode *node)
{
  GskSlNodeReturn *return_node = (GskSlNodeReturn *) node;

  if (return_node->value)
    gsk_sl_expression_unref (return_node->value);

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
      gsk_sl_expression_print (return_node->value, string);
    }
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
  gsk_sl_node_return_write_spv
};

/* EXPRESSION */
 
typedef struct _GskSlNodeExpression GskSlNodeExpression;

struct _GskSlNodeExpression {
  GskSlNode parent;

  GskSlExpression *expression;
};

static void
gsk_sl_node_expression_free (GskSlNode *node)
{
  GskSlNodeExpression *expression_node = (GskSlNodeExpression *) node;
 
  gsk_sl_expression_unref (expression_node->expression);
 
  g_slice_free (GskSlNodeExpression, expression_node);
}
 
static void
gsk_sl_node_expression_print (GskSlNode *node,
                              GString   *string)
{
  GskSlNodeExpression *expression_node = (GskSlNodeExpression *) node;

  gsk_sl_expression_print (expression_node->expression, string);
}
 
static guint32
gsk_sl_node_expression_write_spv (const GskSlNode *node,
                                  GskSpvWriter    *writer)
{
  GskSlNodeExpression *expression_node = (GskSlNodeExpression *) node;

  return gsk_sl_expression_write_spv (expression_node->expression, writer);
}
 
static const GskSlNodeClass GSK_SL_NODE_EXPRESSION = {
  gsk_sl_node_expression_free,
  gsk_sl_node_expression_print,
  gsk_sl_node_expression_write_spv
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
gsk_sl_node_parse_declaration (GskSlScope        *scope,
                               GskSlPreprocessor *stream,
                               GskSlPointerType  *type)
{
  GskSlNodeDeclaration *declaration;
  const GskSlToken *token;

  declaration = gsk_sl_node_new (GskSlNodeDeclaration, &GSK_SL_NODE_DECLARATION);
  
  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {
      declaration->variable = gsk_sl_variable_new (type, NULL);
      return (GskSlNode *) declaration;
    }

  declaration->variable = gsk_sl_variable_new (type, token->str);
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) declaration);

  token = gsk_sl_preprocessor_get (stream);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_EQUAL))
    {
      gsk_sl_preprocessor_consume (stream, (GskSlNode *) declaration);
      declaration->initial = gsk_sl_expression_parse_assignment (scope, stream);
    }

  gsk_sl_scope_add_variable (scope, declaration->variable);

  return (GskSlNode *) declaration;
}

GskSlNode *
gsk_sl_node_parse_function_definition (GskSlScope        *scope,
                                       GskSlPreprocessor *stream)
{
  GskSlNodeFunction *function;
  const GskSlToken *token;
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
          GskSlNode *node = NULL;

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
              GskSlExpression *expression = gsk_sl_expression_parse_constructor_call (function->scope, stream, type);

              if (expression)
                {
                  GskSlNodeExpression *node;
                  
                  node = gsk_sl_node_new (GskSlNodeExpression, &GSK_SL_NODE_EXPRESSION);
                  node->expression = expression;

                  function->statements = g_slist_append (function->statements, node);
                }
              else
                {
                  node = NULL;
                }
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
              return_node->value = gsk_sl_expression_parse (function->scope, stream);
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
              else if (!gsk_sl_type_can_convert (function->return_type, gsk_sl_expression_get_return_type (return_node->value)))
                {
                  gsk_sl_preprocessor_error (stream, "Cannot convert return type %s to function type %s.",
                                                     gsk_sl_type_get_name (gsk_sl_expression_get_return_type (return_node->value)),
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
        {
          GskSlExpression * expression = gsk_sl_expression_parse (function->scope, stream);

          if (expression)
            {
              GskSlNodeExpression *node;
              
              node = gsk_sl_node_new (GskSlNodeExpression, &GSK_SL_NODE_EXPRESSION);
              node->expression = expression;

              function->statements = g_slist_append (function->statements, node);
            }
          else
            {
              result = FALSE;
            }
        }
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

guint32
gsk_sl_node_write_spv (const GskSlNode *node,
                       GskSpvWriter    *writer)
{
  return node->class->write_spv (node, writer);
}

