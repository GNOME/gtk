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

#include "gskslstatementprivate.h"

#include "gskslexpressionprivate.h"
#include "gskslfunctionprivate.h"
#include "gskslpointertypeprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslprinterprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskslqualifierprivate.h"
#include "gskslvalueprivate.h"
#include "gskslvariableprivate.h"
#include "gskspvwriterprivate.h"

#include <string.h>

typedef struct _GskSlStatementClass GskSlStatementClass;

struct _GskSlStatement {
  const GskSlStatementClass *class;
  guint ref_count;
};

struct _GskSlStatementClass {
  void                  (* free)                                (GskSlStatement         *statement);

  void                  (* print)                               (const GskSlStatement   *statement,
                                                                 GskSlPrinter           *printer);
  GskSlJump             (* get_jump)                            (const GskSlStatement   *statement);
  void                  (* write_spv)                           (const GskSlStatement   *statement,
                                                                 GskSpvWriter           *writer);
};

static GskSlStatement *
gsk_sl_statement_alloc (const GskSlStatementClass *klass,
                        gsize                      size)
{
  GskSlStatement *statement;

  statement = g_slice_alloc0 (size);

  statement->class = klass;
  statement->ref_count = 1;

  return statement;
}
#define gsk_sl_statement_new(_name, _klass) ((_name *) gsk_sl_statement_alloc ((_klass), sizeof (_name)))

/* EMPTY */

typedef struct _GskSlStatementEmpty GskSlStatementEmpty;

struct _GskSlStatementEmpty {
  GskSlStatement parent;
};

static void
gsk_sl_statement_empty_free (GskSlStatement *statement)
{
  GskSlStatementEmpty *empty = (GskSlStatementEmpty *) statement;

  g_slice_free (GskSlStatementEmpty, empty);
}

static void
gsk_sl_statement_empty_print (const GskSlStatement *statement,
                              GskSlPrinter         *printer)
{
  gsk_sl_printer_append (printer, ";");
}

static GskSlJump
gsk_sl_statement_empty_get_jump (const GskSlStatement *statement)
{
  return GSK_SL_JUMP_NONE;
}

static void
gsk_sl_statement_empty_write_spv (const GskSlStatement *statement,
                                  GskSpvWriter         *writer)
{
}

static const GskSlStatementClass GSK_SL_STATEMENT_EMPTY = {
  gsk_sl_statement_empty_free,
  gsk_sl_statement_empty_print,
  gsk_sl_statement_empty_get_jump,
  gsk_sl_statement_empty_write_spv
};

/* COMPOUND */

typedef struct _GskSlStatementCompound GskSlStatementCompound;

struct _GskSlStatementCompound {
  GskSlStatement parent;

  GskSlScope *scope;
  GSList *statements;
};

static void
gsk_sl_statement_compound_free (GskSlStatement *statement)
{
  GskSlStatementCompound *compound = (GskSlStatementCompound *) statement;

  g_slist_free_full (compound->statements, (GDestroyNotify) gsk_sl_statement_unref);
  if (compound->scope)
    gsk_sl_scope_unref (compound->scope);

  g_slice_free (GskSlStatementCompound, compound);
}

static void
gsk_sl_statement_compound_print (const GskSlStatement *statement,
                                 GskSlPrinter         *printer)
{
  GskSlStatementCompound *compound = (GskSlStatementCompound *) statement;
  GSList *l;

  gsk_sl_printer_append (printer, "{");
  gsk_sl_printer_push_indentation (printer);
  for (l = compound->statements; l; l = l->next)
    {
      gsk_sl_printer_newline (printer);
      gsk_sl_statement_print (l->data, printer);
    }
  gsk_sl_printer_pop_indentation (printer);
  gsk_sl_printer_newline (printer);
  gsk_sl_printer_append (printer, "}");
}

static GskSlJump
gsk_sl_statement_compound_get_jump (const GskSlStatement *statement)
{
  GskSlStatementCompound *compound = (GskSlStatementCompound *) statement;
  GSList *last;

  if (compound->statements == NULL)
    return GSK_SL_JUMP_NONE;

  last = g_slist_last (compound->statements);

  return gsk_sl_statement_get_jump (last->data);
}

static void
gsk_sl_statement_compound_write_spv (const GskSlStatement *statement,
                                     GskSpvWriter         *writer)
{
  GskSlStatementCompound *compound = (GskSlStatementCompound *) statement;
  GSList *l;

  for (l = compound->statements; l; l = l->next)
    {
      gsk_sl_statement_write_spv (l->data, writer);
    }
}

static const GskSlStatementClass GSK_SL_STATEMENT_COMPOUND = {
  gsk_sl_statement_compound_free,
  gsk_sl_statement_compound_print,
  gsk_sl_statement_compound_get_jump,
  gsk_sl_statement_compound_write_spv
};

/* DECLARATION */

typedef struct _GskSlStatementDeclaration GskSlStatementDeclaration;

struct _GskSlStatementDeclaration {
  GskSlStatement parent;

  GskSlVariable *variable;
  GskSlExpression *initial;
};

static void
gsk_sl_statement_declaration_free (GskSlStatement *statement)
{
  GskSlStatementDeclaration *declaration = (GskSlStatementDeclaration *) statement;

  gsk_sl_variable_unref (declaration->variable);
  if (declaration->initial)
    gsk_sl_expression_unref (declaration->initial);

  g_slice_free (GskSlStatementDeclaration, declaration);
}

static void
gsk_sl_statement_declaration_print (const GskSlStatement *statement,
                                    GskSlPrinter         *printer)
{
  GskSlStatementDeclaration *declaration = (GskSlStatementDeclaration *) statement;

  gsk_sl_variable_print (declaration->variable, printer);
  if (declaration->initial)
    {
      gsk_sl_printer_append (printer, " = ");
      gsk_sl_expression_print (declaration->initial, printer);
    }
  gsk_sl_printer_append (printer, ";");
}

static GskSlJump
gsk_sl_statement_declaration_get_jump (const GskSlStatement *statement)
{
  return GSK_SL_JUMP_NONE;
}

static void
gsk_sl_statement_declaration_write_spv (const GskSlStatement *statement,
                                        GskSpvWriter         *writer)
{
  GskSlStatementDeclaration *declaration = (GskSlStatementDeclaration *) statement;
  
  /* make sure it's written */
  gsk_spv_writer_get_id_for_variable (writer, declaration->variable);

  if (declaration->initial && ! gsk_sl_variable_get_initial_value (declaration->variable))
    {
      gsk_spv_writer_store (writer,
                            gsk_spv_writer_get_id_for_variable (writer, declaration->variable),
                            gsk_sl_expression_write_spv (declaration->initial, writer),
                            0);
    }
}

static const GskSlStatementClass GSK_SL_STATEMENT_DECLARATION = {
  gsk_sl_statement_declaration_free,
  gsk_sl_statement_declaration_print,
  gsk_sl_statement_declaration_get_jump,
  gsk_sl_statement_declaration_write_spv
};

/* RETURN */

typedef struct _GskSlStatementReturn GskSlStatementReturn;

struct _GskSlStatementReturn {
  GskSlStatement parent;

  GskSlExpression *value;
};

static void
gsk_sl_statement_return_free (GskSlStatement *statement)
{
  GskSlStatementReturn *return_statement = (GskSlStatementReturn *) statement;

  if (return_statement->value)
    gsk_sl_expression_unref (return_statement->value);

  g_slice_free (GskSlStatementReturn, return_statement);
}

static void
gsk_sl_statement_return_print (const GskSlStatement *statement,
                               GskSlPrinter         *printer)
{
  GskSlStatementReturn *return_statement = (GskSlStatementReturn *) statement;

  gsk_sl_printer_append (printer, "return");
  if (return_statement->value)
    {
      gsk_sl_printer_append (printer, " ");
      gsk_sl_expression_print (return_statement->value, printer);
    }
  gsk_sl_printer_append (printer, ";");
}

static GskSlJump
gsk_sl_statement_return_get_jump (const GskSlStatement *statement)
{
  return GSK_SL_JUMP_RETURN;
}

static void
gsk_sl_statement_return_write_spv (const GskSlStatement *statement,
                                   GskSpvWriter         *writer)
{
  GskSlStatementReturn *return_statement = (GskSlStatementReturn *) statement;

  if (return_statement->value)
    {
      gsk_spv_writer_return_value (writer,
                                   gsk_sl_expression_write_spv (return_statement->value, writer));
    }
  else
    {
      gsk_spv_writer_return (writer);
    }
}

static const GskSlStatementClass GSK_SL_STATEMENT_RETURN = {
  gsk_sl_statement_return_free,
  gsk_sl_statement_return_print,
  gsk_sl_statement_return_get_jump,
  gsk_sl_statement_return_write_spv
};

/* IF */
 
typedef struct _GskSlStatementIf GskSlStatementIf;

struct _GskSlStatementIf {
  GskSlStatement parent;

  GskSlExpression *condition;
  GskSlStatement *if_part;
  GskSlScope *if_scope;
  GskSlStatement *else_part;
  GskSlScope *else_scope;
};

static void
gsk_sl_statement_if_free (GskSlStatement *statement)
{
  GskSlStatementIf *if_stmt = (GskSlStatementIf *) statement;
 
  gsk_sl_expression_unref (if_stmt->condition);
  gsk_sl_scope_unref (if_stmt->if_scope);
 
  if (if_stmt->else_part)
    {
      gsk_sl_statement_unref (if_stmt->else_part);
      gsk_sl_scope_unref (if_stmt->else_scope);
    }

  g_slice_free (GskSlStatementIf, if_stmt);
}
 
static void
gsk_sl_statement_if_print (const GskSlStatement *statement,
                           GskSlPrinter         *printer)
{
  GskSlStatementIf *if_stmt = (GskSlStatementIf *) statement;

  gsk_sl_printer_append (printer, "if (");
  gsk_sl_expression_print (if_stmt->condition, printer);
  gsk_sl_printer_append (printer, ")");
  gsk_sl_printer_push_indentation (printer);
  gsk_sl_printer_newline (printer);
  gsk_sl_statement_print (if_stmt->if_part, printer);
  gsk_sl_printer_pop_indentation (printer);

  if (if_stmt->else_part)
    {
      gsk_sl_printer_newline (printer);
      gsk_sl_printer_append (printer, "else");
      gsk_sl_printer_push_indentation (printer);
      gsk_sl_printer_newline (printer);
      gsk_sl_statement_print (if_stmt->else_part, printer);
      gsk_sl_printer_pop_indentation (printer);
    }
}
 
static GskSlJump
gsk_sl_statement_if_get_jump (const GskSlStatement *statement)
{
  GskSlStatementIf *if_stmt = (GskSlStatementIf *) statement;

  if (if_stmt->else_part == NULL)
    return GSK_SL_JUMP_NONE;

  return MIN (gsk_sl_statement_get_jump (if_stmt->if_part),
              gsk_sl_statement_get_jump (if_stmt->else_part));
}

static void
gsk_sl_statement_if_write_spv (const GskSlStatement *statement,
                               GskSpvWriter         *writer)
{
#if 0 
  GskSlStatementIf *if_stmt = (GskSlStatementIf *) statement;
  guint32 label_id, if_id, else_id, condition_id;

  condition_id = gsk_sl_expression_write_spv (if_stmt->condition, writer);
  if_id = gsk_spv_writer_next_id (writer);
  else_id = gsk_spv_writer_next_id (writer);
  /* We compute the labels in this funny order to match what glslang does */
  if (if_stmt->else_part)
    label_id = gsk_spv_writer_next_id (writer);
  else
    label_id = else_id;
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      3, GSK_SPV_OP_SELECTION_MERGE,
                      (guint32[2]) { label_id,
                                     0});
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      4, GSK_SPV_OP_BRANCH_CONDITIONAL,
                      (guint32[3]) { condition_id,
                                     if_id,
                                     else_id });

  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      2, GSK_SPV_OP_LABEL,
                      (guint32[1]) { if_id });
  gsk_sl_statement_write_spv (if_stmt->if_part, writer);
  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      2, GSK_SPV_OP_BRANCH,
                      (guint32[1]) { label_id });

  if (if_stmt->else_part)
    {
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          2, GSK_SPV_OP_LABEL,
                          (guint32[1]) { else_id });
      gsk_sl_statement_write_spv (if_stmt->else_part, writer);
      gsk_spv_writer_add (writer,
                          GSK_SPV_WRITER_SECTION_CODE,
                          2, GSK_SPV_OP_BRANCH,
                          (guint32[1]) { label_id });
    }

  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      2, GSK_SPV_OP_LABEL,
                      (guint32[1]) { label_id });
#endif
  g_assert_not_reached ();
}

static const GskSlStatementClass GSK_SL_STATEMENT_IF = {
  gsk_sl_statement_if_free,
  gsk_sl_statement_if_print,
  gsk_sl_statement_if_get_jump,
  gsk_sl_statement_if_write_spv
};

/* EXPRESSION */
 
typedef struct _GskSlStatementExpression GskSlStatementExpression;

struct _GskSlStatementExpression {
  GskSlStatement parent;

  GskSlExpression *expression;
};

static void
gsk_sl_statement_expression_free (GskSlStatement *statement)
{
  GskSlStatementExpression *expression_statement = (GskSlStatementExpression *) statement;
 
  gsk_sl_expression_unref (expression_statement->expression);
 
  g_slice_free (GskSlStatementExpression, expression_statement);
}
 
static void
gsk_sl_statement_expression_print (const GskSlStatement *statement,
                                   GskSlPrinter         *printer)
{
  GskSlStatementExpression *expression_statement = (GskSlStatementExpression *) statement;

  gsk_sl_expression_print (expression_statement->expression, printer);
  gsk_sl_printer_append (printer, ";");
}
 
static GskSlJump
gsk_sl_statement_expression_get_jump (const GskSlStatement *statement)
{
  return GSK_SL_JUMP_NONE;
}

static void
gsk_sl_statement_expression_write_spv (const GskSlStatement *statement,
                                       GskSpvWriter         *writer)
{
  GskSlStatementExpression *expression_statement = (GskSlStatementExpression *) statement;

  gsk_sl_expression_write_spv (expression_statement->expression, writer);
}
 
static const GskSlStatementClass GSK_SL_STATEMENT_EXPRESSION = {
  gsk_sl_statement_expression_free,
  gsk_sl_statement_expression_print,
  gsk_sl_statement_expression_get_jump,
  gsk_sl_statement_expression_write_spv
};

/* API */

static GskSlStatement *
gsk_sl_statement_parse_declaration (GskSlScope           *scope,
                                    GskSlPreprocessor    *stream,
                                    const GskSlQualifier *qualifier,
                                    GskSlType            *type)
{
  GskSlStatementDeclaration *declaration;
  GskSlPointerType *pointer_type;
  GskSlValue *value = NULL;
  const GskSlToken *token;
  char *name;

  declaration = gsk_sl_statement_new (GskSlStatementDeclaration, &GSK_SL_STATEMENT_DECLARATION);
  
  token = gsk_sl_preprocessor_get (stream);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {
      name = g_strdup (token->str);
      gsk_sl_preprocessor_consume (stream, (GskSlStatement *) declaration);

      token = gsk_sl_preprocessor_get (stream);
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_EQUAL))
        {
          GskSlValue *unconverted;

          gsk_sl_preprocessor_consume (stream, (GskSlStatement *) declaration);
          declaration->initial = gsk_sl_expression_parse_assignment (scope, stream);
          if (!gsk_sl_type_can_convert (type, gsk_sl_expression_get_return_type (declaration->initial)))
            {
              gsk_sl_preprocessor_error (stream, TYPE_MISMATCH,
                                         "Cannot convert from initializer type %s to variable type %s",
                                         gsk_sl_type_get_name (gsk_sl_expression_get_return_type (declaration->initial)),
                                         gsk_sl_type_get_name (type));
              gsk_sl_expression_unref (declaration->initial);
              declaration->initial = NULL;
            }
          else
            {
              unconverted = gsk_sl_expression_get_constant (declaration->initial);
              if (unconverted)
                {
                  value = gsk_sl_value_new_convert (unconverted, type);
                  gsk_sl_value_free (unconverted);
                }
            }
        }
    }
  else
    {
      name = NULL;
      value = NULL;
    }

  pointer_type = gsk_sl_pointer_type_new (type, qualifier);
  declaration->variable = gsk_sl_variable_new (pointer_type, name, value);
  gsk_sl_pointer_type_unref (pointer_type);
  gsk_sl_scope_add_variable (scope, declaration->variable);

  return (GskSlStatement *) declaration;
}

static GskSlStatement *
gsk_sl_statement_parse_if (GskSlScope        *scope,
                           GskSlPreprocessor *preproc)
{
  GskSlStatementIf *if_stmt;
  const GskSlToken *token;
  GskSlValue *value;

  if_stmt = gsk_sl_statement_new (GskSlStatementIf, &GSK_SL_STATEMENT_IF);

  /* GSK_SL_TOKEN_IF */
  gsk_sl_preprocessor_consume (preproc, if_stmt);

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected an opening \"(\"");
      return (GskSlStatement *) if_stmt;
    }
  gsk_sl_preprocessor_consume (preproc, if_stmt);

  if_stmt->condition = gsk_sl_expression_parse (scope, preproc);
  if (!gsk_sl_type_equal (gsk_sl_expression_get_return_type (if_stmt->condition), gsk_sl_type_get_scalar (GSK_SL_BOOL)))
    {
      gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                 "Condition in if statment returns %s, not a bool",
                                 gsk_sl_type_get_name (gsk_sl_expression_get_return_type (if_stmt->condition)));
    }
  value = gsk_sl_expression_get_constant (if_stmt->condition);
  if (value)
    {
      gsk_sl_preprocessor_warn (preproc, CONSTANT,
                                "Condition in if statement is always %s",
                                *(guint32 *) gsk_sl_value_get_data (value) ? "true" : "false");
      gsk_sl_value_free (value);
    }
  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected a closing \")\" after statement");
  else
    gsk_sl_preprocessor_consume (preproc, if_stmt);

  if_stmt->if_scope = gsk_sl_scope_new (scope, gsk_sl_scope_get_return_type (scope));
  if_stmt->if_part = gsk_sl_statement_parse (if_stmt->if_scope, preproc);

  token = gsk_sl_preprocessor_get (preproc);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_ELSE))
    {
      gsk_sl_preprocessor_consume (preproc, if_stmt);
      if_stmt->else_scope = gsk_sl_scope_new (scope, gsk_sl_scope_get_return_type (scope));
      if_stmt->else_part = gsk_sl_statement_parse (if_stmt->else_scope, preproc);
    }

  return (GskSlStatement *) if_stmt;
}

GskSlStatement *
gsk_sl_statement_parse_compound (GskSlScope        *scope,
                                 GskSlPreprocessor *preproc,
                                 gboolean           new_scope)
{
  GskSlStatementCompound *compound;
  const GskSlToken *token;
  GskSlJump jump = GSK_SL_JUMP_NONE;

  compound = gsk_sl_statement_new (GskSlStatementCompound, &GSK_SL_STATEMENT_COMPOUND);
  if (new_scope)
    {
      compound->scope = gsk_sl_scope_new (scope, gsk_sl_scope_get_return_type (scope));
      scope = compound->scope;
    }

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_BRACE))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected an opening \"{\"");
      return (GskSlStatement *) compound;
    }
  gsk_sl_preprocessor_consume (preproc, compound);

  for (token = gsk_sl_preprocessor_get (preproc);
       !gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE) && !gsk_sl_token_is (token, GSK_SL_TOKEN_EOF);
       token = gsk_sl_preprocessor_get (preproc))
    {
      GskSlStatement *statement;

      if (jump != GSK_SL_JUMP_NONE)
        gsk_sl_preprocessor_warn (preproc, DEAD_CODE, "Statement cannot be reached.");

      statement = gsk_sl_statement_parse (scope, preproc);
      compound->statements = g_slist_prepend (compound->statements, statement);
      jump = gsk_sl_statement_get_jump (statement);
    }
  compound->statements = g_slist_reverse (compound->statements);

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected closing \"}\" at end of block.");
      gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_RIGHT_BRACE);
    }
  gsk_sl_preprocessor_consume (preproc, compound);
  
  return (GskSlStatement *) compound;
}

GskSlStatement *
gsk_sl_statement_parse (GskSlScope        *scope,
                        GskSlPreprocessor *preproc)
{
  const GskSlToken *token;
  GskSlStatement *statement;

  token = gsk_sl_preprocessor_get (preproc);

  switch ((guint) token->type)
  {
    case GSK_SL_TOKEN_SEMICOLON:
      statement = (GskSlStatement *) gsk_sl_statement_new (GskSlStatementEmpty, &GSK_SL_STATEMENT_EMPTY);
      break;

    case GSK_SL_TOKEN_EOF:
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Unexpected end of document");
      return (GskSlStatement *) gsk_sl_statement_new (GskSlStatementEmpty, &GSK_SL_STATEMENT_EMPTY);

    case GSK_SL_TOKEN_LEFT_BRACE:
      return gsk_sl_statement_parse_compound (scope, preproc, TRUE);

    case GSK_SL_TOKEN_IF:
      return gsk_sl_statement_parse_if (scope, preproc);

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
    case GSK_SL_TOKEN_STRUCT:
      {
        GskSlType *type;
        GskSlQualifier qualifier;

its_a_type:
        gsk_sl_qualifier_parse (&qualifier, scope, preproc, GSK_SL_QUALIFIER_LOCAL);

        type = gsk_sl_type_new_parse (scope, preproc);

        token = gsk_sl_preprocessor_get (preproc);

        if (token->type == GSK_SL_TOKEN_LEFT_PAREN)
          {
            GskSlStatementExpression *statement_expression;
                
            statement_expression = gsk_sl_statement_new (GskSlStatementExpression, &GSK_SL_STATEMENT_EXPRESSION);
            if (gsk_sl_type_is_scalar (type) || gsk_sl_type_is_vector (type) || gsk_sl_type_is_matrix (type))
              {
                statement_expression->expression = gsk_sl_expression_parse_constructor (scope, preproc, type);
              }
            else
              {
                GskSlFunction *constructor;
                GskSlFunctionMatcher matcher;

                constructor = gsk_sl_function_new_constructor (type);
                gsk_sl_function_matcher_init (&matcher, g_list_prepend (NULL, constructor));
                statement_expression->expression = gsk_sl_expression_parse_function_call (scope, preproc, &matcher);
                gsk_sl_function_matcher_finish (&matcher);
                gsk_sl_function_unref (constructor);
              }
            statement = (GskSlStatement *) statement_expression;
          }
        else
          {
            statement = gsk_sl_statement_parse_declaration (scope, preproc, &qualifier, type);
          }

        gsk_sl_type_unref (type);
      }
      break;

    case GSK_SL_TOKEN_RETURN:
      {
        GskSlStatementReturn *return_statement;
        GskSlType *return_type;

        return_statement = gsk_sl_statement_new (GskSlStatementReturn, &GSK_SL_STATEMENT_RETURN);
        gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) return_statement);
        token = gsk_sl_preprocessor_get (preproc);
        if (!gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
          return_statement->value = gsk_sl_expression_parse (scope, preproc);

        return_type = gsk_sl_scope_get_return_type (scope);
        statement = (GskSlStatement *) return_statement;

        if (return_type == NULL)
          {
            gsk_sl_preprocessor_error (preproc, SCOPE, "Cannot return from here.");
          }
        else if (return_statement->value == NULL)
          {
            if (!gsk_sl_type_is_void (return_type))
              {
                gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,"Functions expectes a return value of type %s", gsk_sl_type_get_name (return_type));
              }
          }
        else
          {
            if (gsk_sl_type_is_void (return_type))
              {
                gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH, "Cannot return a value from a void function.");
              }
            else if (!gsk_sl_type_can_convert (return_type, gsk_sl_expression_get_return_type (return_statement->value)))
              {
                gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                           "Cannot convert type %s to return type %s.",
                                           gsk_sl_type_get_name (gsk_sl_expression_get_return_type (return_statement->value)),
                                           gsk_sl_type_get_name (return_type));
                break;
              }
            }
        }
      break;

    case GSK_SL_TOKEN_IDENTIFIER:
      if (gsk_sl_scope_lookup_type (scope, token->str))
        goto its_a_type;
      /* else fall through*/

    default:
      {
        GskSlStatementExpression *statement_expression;

        statement_expression = gsk_sl_statement_new (GskSlStatementExpression, &GSK_SL_STATEMENT_EXPRESSION);
        statement_expression->expression = gsk_sl_expression_parse (scope, preproc);

        statement = (GskSlStatement *) statement_expression;
      }
      break;
  }

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "No semicolon at end of statement.");
      gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_SEMICOLON);
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) statement);

  return statement;
}

GskSlStatement *
gsk_sl_statement_ref (GskSlStatement *statement)
{
  g_return_val_if_fail (statement != NULL, NULL);

  statement->ref_count += 1;

  return statement;
}

void
gsk_sl_statement_unref (GskSlStatement *statement)
{
  if (statement == NULL)
    return;

  statement->ref_count -= 1;
  if (statement->ref_count > 0)
    return;

  statement->class->free (statement);
}

void
gsk_sl_statement_print (const GskSlStatement *statement,
                        GskSlPrinter         *printer)
{
  statement->class->print (statement, printer);
}

GskSlJump
gsk_sl_statement_get_jump (const GskSlStatement *statement)
{
  return statement->class->get_jump (statement);
}

void
gsk_sl_statement_write_spv (const GskSlStatement *statement,
                            GskSpvWriter         *writer)
{
  statement->class->write_spv (statement, writer);
}

