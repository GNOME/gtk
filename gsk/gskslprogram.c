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

#include "gskslprogramprivate.h"

#include "gskslexpressionprivate.h"
#include "gskslfunctionprivate.h"
#include "gskslnodeprivate.h"
#include "gskslpointertypeprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
#include "gskslvariableprivate.h"
#include "gskspvwriterprivate.h"

struct _GskSlProgram {
  GObject parent_instance;

  GskSlScope *scope;
  GSList *variables;
  GSList *functions;
};

G_DEFINE_TYPE (GskSlProgram, gsk_sl_program, G_TYPE_OBJECT)

static void
gsk_sl_program_dispose (GObject *object)
{
  GskSlProgram *program = GSK_SL_PROGRAM (object);

  g_slist_free_full (program->variables, (GDestroyNotify) gsk_sl_variable_unref);
  g_slist_free_full (program->functions, (GDestroyNotify) gsk_sl_function_unref);
  gsk_sl_scope_unref (program->scope);

  G_OBJECT_CLASS (gsk_sl_program_parent_class)->dispose (object);
}

static void
gsk_sl_program_class_init (GskSlProgramClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gsk_sl_program_dispose;
}

static void
gsk_sl_program_init (GskSlProgram *program)
{
  program->scope = gsk_sl_scope_new (NULL, NULL);
}

static void
gsk_sl_program_parse_variable (GskSlProgram      *program,
                               GskSlScope        *scope,
                               GskSlPreprocessor *preproc,
                               GskSlDecorations  *decoration,
                               GskSlType         *type,
                               const char        *name)
{
  GskSlVariable *variable;
  const GskSlToken *token;
  GskSlValue *value = NULL;
  GskSlPointerType *pointer_type;

  token = gsk_sl_preprocessor_get (preproc);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_EQUAL))
    {
      GskSlValue *unconverted;
      GskSlExpression *initial;

      gsk_sl_preprocessor_consume (preproc, program);

      initial = gsk_sl_expression_parse_assignment (scope, preproc);

      if (!gsk_sl_type_can_convert (type, gsk_sl_expression_get_return_type (initial)))
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                     "Cannot convert from initializer type %s to variable type %s",
                                     gsk_sl_type_get_name (gsk_sl_expression_get_return_type (initial)),
                                     gsk_sl_type_get_name (type));
          gsk_sl_expression_unref (initial);
        }
      else
        {
          unconverted = gsk_sl_expression_get_constant (initial);
          gsk_sl_expression_unref (initial);
          if (unconverted)
            {
              value = gsk_sl_value_new_convert (unconverted, type);
              gsk_sl_value_free (unconverted);
            }
          else 
            {
              gsk_sl_preprocessor_error (preproc, UNSUPPORTED, "Non-constant initializer are not supported yet.");
              value = NULL;
            }
        }

      token = gsk_sl_preprocessor_get (preproc);
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
    gsk_sl_preprocessor_error (preproc, SYNTAX, "No semicolon at end of variable declaration.");
  else
    gsk_sl_preprocessor_consume (preproc, NULL);

  pointer_type = gsk_sl_pointer_type_new (type, FALSE, decoration->values[GSK_SL_DECORATION_CALLER_ACCESS].value);
  variable = gsk_sl_variable_new (pointer_type, g_strdup (name), value, decoration->values[GSK_SL_DECORATION_CONST].set);
  gsk_sl_pointer_type_unref (pointer_type);
      
  program->variables = g_slist_append (program->variables, variable);
  gsk_sl_scope_add_variable (scope, variable);
}

static void
gsk_sl_program_parse_declaration (GskSlProgram      *program,
                                  GskSlScope        *scope,
                                  GskSlPreprocessor *preproc)
{
  GskSlType *type;
  const GskSlToken *token;
  GskSlDecorations decoration;
  char *name;

  gsk_sl_decoration_list_parse (scope,
                                preproc,
                                &decoration);

  type = gsk_sl_type_new_parse (preproc);

  token = gsk_sl_preprocessor_get (preproc);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
    {
      GskSlPointerType *ptype = gsk_sl_pointer_type_new (type, FALSE, decoration.values[GSK_SL_DECORATION_CALLER_ACCESS].value);
      GskSlVariable *variable = gsk_sl_variable_new (ptype, NULL, NULL, decoration.values[GSK_SL_DECORATION_CONST].set);
      gsk_sl_pointer_type_unref (ptype);
      program->variables = g_slist_append (program->variables, variable);
      gsk_sl_preprocessor_consume (preproc, program);
      gsk_sl_type_unref (type);
      return;
    }
  else if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected a variable name");
      gsk_sl_preprocessor_consume (preproc, program);
      gsk_sl_type_unref (type);
      return;
    }

  name = g_strdup (token->str);
  gsk_sl_preprocessor_consume (preproc, program);

  token = gsk_sl_preprocessor_get (preproc);

  if (gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      GskSlFunction *function;

      function = gsk_sl_function_new_parse (scope,
                                            preproc,
                                            type,
                                            name);
      program->functions = g_slist_append (program->functions, function);
    }
  else
    {
      gsk_sl_program_parse_variable (program, scope, preproc, &decoration, type, name);
    }

  g_free (name);

  return;
}

void
gsk_sl_program_parse (GskSlProgram      *program,
                      GskSlPreprocessor *preproc)
{
  const GskSlToken *token;

  for (token = gsk_sl_preprocessor_get (preproc);
       !gsk_sl_token_is (token, GSK_SL_TOKEN_EOF);
       token = gsk_sl_preprocessor_get (preproc))
    {
      gsk_sl_program_parse_declaration (program, program->scope, preproc);
    }
}

void
gsk_sl_program_print (GskSlProgram *program,
                      GString      *string)
{
  GSList *l;

  g_return_if_fail (GSK_IS_SL_PROGRAM (program));
  g_return_if_fail (string != NULL);

  for (l = program->variables; l; l = l->next)
    {
      const GskSlValue *value;
      gsk_sl_variable_print (l->data, string);
      value = gsk_sl_variable_get_initial_value (l->data);
      if (value)
        {
          g_string_append (string, " = ");
          gsk_sl_value_print (value, string);
        }
      g_string_append (string, ";\n");
    }

  for (l = program->functions; l; l = l->next)
    {
      if (l != program->functions || program->variables != NULL)
        g_string_append (string, "\n");
      gsk_sl_function_print (l->data, string);
    }
}

static void
gsk_sl_program_write_spv (GskSlProgram *program,
                          GskSpvWriter *writer)
{
  GSList *l;

  for (l = program->variables; l; l = l->next)
    gsk_spv_writer_get_id_for_variable (writer, l->data);

  for (l = program->functions; l; l = l->next)
    {
      guint32 id = gsk_sl_function_write_spv (l->data, writer);

      if (g_str_equal (gsk_sl_function_get_name (l->data), "main"))
        gsk_spv_writer_set_entry_point (writer, id);
    }
}

GBytes *
gsk_sl_program_to_spirv (GskSlProgram *program)
{
  GskSpvWriter *writer;
  GBytes *bytes;

  g_return_val_if_fail (GSK_IS_SL_PROGRAM (program), NULL);

  writer = gsk_spv_writer_new ();

  gsk_sl_program_write_spv (program, writer);
  bytes = gsk_spv_writer_write (writer);

  gsk_spv_writer_unref (writer);

  return bytes;
}
