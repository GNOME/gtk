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

#include "gsksldeclarationprivate.h"

#include "gskslexpressionprivate.h"
#include "gskslfunctionprivate.h"
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

typedef struct _GskSlDeclarationClass GskSlDeclarationClass;

struct _GskSlDeclaration {
  const GskSlDeclarationClass *class;
  guint ref_count;
};

struct _GskSlDeclarationClass {
  gsize                 element_size;
  void                  (* finalize)                            (GskSlDeclaration       *declaration);

  void                  (* print)                               (const GskSlDeclaration *declaration,
                                                                 GskSlPrinter           *printer);
  void                  (* write_initializer_spv)               (const GskSlDeclaration *declaration,
                                                                 GskSpvWriter           *writer);
};

static gpointer
gsk_sl_declaration_new (const GskSlDeclarationClass *klass)
{
  GskSlDeclaration *declaration;

  declaration = g_slice_alloc0 (klass->element_size);

  declaration->class = klass;
  declaration->ref_count = 1;

  return declaration;
}

/* VARIABLE */

typedef struct _GskSlDeclarationVariable GskSlDeclarationVariable;

struct _GskSlDeclarationVariable {
  GskSlDeclaration parent;

  GskSlVariable *variable;
  GskSlExpression *initial;
};

static void
gsk_sl_declaration_variable_finalize (GskSlDeclaration *declaration)
{
  GskSlDeclarationVariable *variable = (GskSlDeclarationVariable *) declaration;

  gsk_sl_variable_unref (variable->variable);
  if (variable->initial)
    gsk_sl_expression_unref (variable->initial);
}

static void
gsk_sl_declaration_variable_print (const GskSlDeclaration *declaration,
                                   GskSlPrinter           *printer)
{
  GskSlDeclarationVariable *variable = (GskSlDeclarationVariable *) declaration;

  gsk_sl_variable_print (variable->variable, printer);
  if (variable->initial)
    {
      gsk_sl_printer_append (printer, " = ");
      gsk_sl_expression_print (variable->initial, printer);
    }
  gsk_sl_printer_append (printer, ";");
  gsk_sl_printer_newline (printer);
}

static void
gsk_sl_declaration_variable_write_initializer_spv (const GskSlDeclaration *declaration,
                                                   GskSpvWriter           *writer)
{
  GskSlDeclarationVariable *variable = (GskSlDeclarationVariable *) declaration;
  
  /* make sure it's written */
  gsk_spv_writer_get_id_for_variable (writer, variable->variable);

  if (variable->initial && ! gsk_sl_variable_get_initial_value (variable->variable))
    {
      gsk_sl_variable_store_spv (variable->variable,
                                 writer,
                                 gsk_sl_expression_write_spv (variable->initial, writer));
    }
}

static const GskSlDeclarationClass GSK_SL_DECLARATION_VARIABLE = {
  sizeof (GskSlDeclarationVariable),
  gsk_sl_declaration_variable_finalize,
  gsk_sl_declaration_variable_print,
  gsk_sl_declaration_variable_write_initializer_spv
};

/* TYPE */

typedef struct _GskSlDeclarationType GskSlDeclarationType;

struct _GskSlDeclarationType {
  GskSlDeclaration parent;

  GskSlQualifier qualifier;
  GskSlType *type;
};

static void
gsk_sl_declaration_type_finalize (GskSlDeclaration *declaration)
{
  GskSlDeclarationType *type = (GskSlDeclarationType *) declaration;

  gsk_sl_type_unref (type->type);
}

static void
gsk_sl_declaration_type_print (const GskSlDeclaration *declaration,
                               GskSlPrinter           *printer)
{
  GskSlDeclarationType *type = (GskSlDeclarationType *) declaration;

  if (gsk_sl_qualifier_print (&type->qualifier, printer))
    gsk_sl_printer_append (printer, " ");
  gsk_sl_printer_append (printer, gsk_sl_type_get_name (type->type));
  gsk_sl_printer_append (printer, ";");
  gsk_sl_printer_newline (printer);
}

static void
gsk_sl_declaration_type_write_initializer_spv (const GskSlDeclaration *declaration,
                                               GskSpvWriter           *writer)
{
  /* This declaration is only relevant for printing as it just declares a type */
}

static const GskSlDeclarationClass GSK_SL_DECLARATION_TYPE = {
  sizeof (GskSlDeclarationType),
  gsk_sl_declaration_type_finalize,
  gsk_sl_declaration_type_print,
  gsk_sl_declaration_type_write_initializer_spv
};

/* FUNCTION */
 
typedef struct _GskSlDeclarationFunction GskSlDeclarationFunction;

struct _GskSlDeclarationFunction {
  GskSlDeclaration parent;

  GskSlFunction *function;
};

static void
gsk_sl_declaration_function_free (GskSlDeclaration *declaration)
{
  GskSlDeclarationFunction *function = (GskSlDeclarationFunction *) declaration;
 
  gsk_sl_function_unref (function->function);
}
 
static void
gsk_sl_declaration_function_print (const GskSlDeclaration *declaration,
                                   GskSlPrinter         *printer)
{
  GskSlDeclarationFunction *function = (GskSlDeclarationFunction *) declaration;

  gsk_sl_function_print (function->function, printer);
}
 
static void
gsk_sl_declaration_function_write_initializer_spv (const GskSlDeclaration *declaration,
                                                   GskSpvWriter           *writer)
{
  /* functions are written out as-needed, so no need to force it */
}
 
static const GskSlDeclarationClass GSK_SL_DECLARATION_FUNCTION = {
  sizeof (GskSlDeclarationFunction),
  gsk_sl_declaration_function_free,
  gsk_sl_declaration_function_print,
  gsk_sl_declaration_function_write_initializer_spv
};

/* API */

static GskSlDeclaration *
gsk_sl_declaration_parse_variable (GskSlScope           *scope,
                                   GskSlPreprocessor    *preproc,
                                   const GskSlQualifier *qualifier,
                                   GskSlType            *base_type,
                                   const char           *name)
{
  GskSlDeclarationVariable *variable;
  GskSlValue *initial_value = NULL;
  GskSlExpression *initial = NULL;
  GskSlType *type;
  const GskSlToken *token;

  type = gsk_sl_type_ref (base_type);
  type = gsk_sl_type_parse_array (type, scope, preproc);

  gsk_sl_qualifier_check_type (qualifier, preproc, type);

  token = gsk_sl_preprocessor_get (preproc);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_EQUAL))
    {
      gsk_sl_preprocessor_consume (preproc, NULL);

      initial = gsk_sl_expression_parse_assignment (scope, preproc);

      if (!gsk_sl_type_can_convert (type, gsk_sl_expression_get_return_type (initial)))
        {
          gsk_sl_preprocessor_error (preproc, TYPE_MISMATCH,
                                     "Cannot convert from initializer type %s to variable type %s",
                                     gsk_sl_type_get_name (gsk_sl_expression_get_return_type (initial)),
                                     gsk_sl_type_get_name (type));
          gsk_sl_expression_unref (initial);
          initial = NULL;
        }
      else
        {
          GskSlValue *unconverted = gsk_sl_expression_get_constant (initial);
          if (unconverted)
            {
              initial_value = gsk_sl_value_new_convert (unconverted, type);
              gsk_sl_value_free (unconverted);
            }
        }

      token = gsk_sl_preprocessor_get (preproc);
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "No semicolon at end of variable declaration.");
      gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_SEMICOLON);
    }
  gsk_sl_preprocessor_consume (preproc, NULL);

  if (qualifier->storage == GSK_SL_STORAGE_GLOBAL_CONST &&
      initial_value == NULL)
    {
      gsk_sl_preprocessor_error (preproc, DECLARATION, "Variables with \"const\" qualifier must be initialized with a constant value.");
      initial_value = gsk_sl_value_new (type);
    }

  variable = gsk_sl_declaration_new (&GSK_SL_DECLARATION_VARIABLE);
  variable->variable = gsk_sl_variable_new (name, type, qualifier, initial_value);
  variable->initial = initial;
  gsk_sl_scope_add_variable (scope, variable->variable);

  gsk_sl_type_unref (type);

  return &variable->parent;
}

GskSlDeclaration *
gsk_sl_declaration_parse (GskSlScope        *scope,
                          GskSlPreprocessor *preproc)
{
  GskSlDeclaration *result;
  GskSlType *type;
  const GskSlToken *token;
  GskSlQualifier qualifier;
  char *name;

  gsk_sl_qualifier_parse (&qualifier, scope, preproc, GSK_SL_QUALIFIER_GLOBAL);

  type = gsk_sl_type_new_parse (scope, preproc);

  token = gsk_sl_preprocessor_get (preproc);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
    {
      GskSlDeclarationType *type_decl = gsk_sl_declaration_new (&GSK_SL_DECLARATION_TYPE);

      type_decl->qualifier = qualifier;
      type_decl->type = type;
      gsk_sl_preprocessor_consume (preproc, type_decl);
      return &type_decl->parent;
    }
  else if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected a variable name");
      gsk_sl_preprocessor_consume (preproc, NULL);
      gsk_sl_type_unref (type);
      return NULL;
    }

  name = g_strdup (token->str);
  gsk_sl_preprocessor_consume (preproc, NULL);

  token = gsk_sl_preprocessor_get (preproc);

  if (gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      GskSlFunctionMatcher matcher;
      GskSlDeclarationFunction *function;

      function = gsk_sl_declaration_new (&GSK_SL_DECLARATION_FUNCTION);
      function->function = gsk_sl_function_new_parse (scope,
                                                      preproc,
                                                      type,
                                                      name);
      gsk_sl_scope_match_function (scope, &matcher, gsk_sl_function_get_name (function->function));
      gsk_sl_function_matcher_match_function (&matcher, function->function);
      if (gsk_sl_function_matcher_has_matches (&matcher))
        gsk_sl_preprocessor_error (preproc, DECLARATION, "A function with the same prototype has already been defined.");
      else
        gsk_sl_scope_add_function (scope, function->function);
      gsk_sl_function_matcher_finish (&matcher);
      result = &function->parent;
    }
  else
    {
      result = gsk_sl_declaration_parse_variable (scope, preproc, &qualifier, type, name);
    }

  g_free (name);
  gsk_sl_type_unref (type);

  return result;
}

GskSlDeclaration *
gsk_sl_declaration_ref (GskSlDeclaration *declaration)
{
  g_return_val_if_fail (declaration != NULL, NULL);

  declaration->ref_count += 1;

  return declaration;
}

void
gsk_sl_declaration_unref (GskSlDeclaration *declaration)
{
  if (declaration == NULL)
    return;

  declaration->ref_count -= 1;
  if (declaration->ref_count > 0)
    return;

  declaration->class->finalize (declaration);

  g_slice_free1 (declaration->class->element_size, declaration);
}

GskSlFunction *
gsk_sl_declaration_get_function (GskSlDeclaration *declaration)
{
  if (declaration->class != &GSK_SL_DECLARATION_FUNCTION)
    return NULL;

  return ((GskSlDeclarationFunction *) declaration)->function;
}

void
gsk_sl_declaration_print (const GskSlDeclaration *declaration,
                          GskSlPrinter           *printer)
{
  declaration->class->print (declaration, printer);
}

void
gsk_sl_declaration_write_initializer_spv (const GskSlDeclaration *declaration,
                                          GskSpvWriter           *writer)
{
  declaration->class->write_initializer_spv (declaration, writer);
}

