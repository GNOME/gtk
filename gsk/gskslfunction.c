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

#include "gskslfunctionprivate.h"

#include "gskslnativefunctionprivate.h"
#include "gskslstatementprivate.h"
#include "gskslpointertypeprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslprinterprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvariableprivate.h"
#include "gskspvwriterprivate.h"

typedef struct _GskSlFunctionClass GskSlFunctionClass;

struct _GskSlFunction
{
  const GskSlFunctionClass *class;

  int ref_count;
};

struct _GskSlFunctionClass {
  void                  (* free)                                (GskSlFunction          *function);

  GskSlType *           (* get_return_type)                     (const GskSlFunction    *function);
  const char *          (* get_name)                            (const GskSlFunction    *function);
  gsize                 (* get_n_arguments)                     (const GskSlFunction    *function);
  GskSlType *           (* get_argument_type)                   (const GskSlFunction    *function,
                                                                 gsize                   i);
  void                  (* print)                               (const GskSlFunction    *function,
                                                                 GskSlPrinter           *printer);
  guint32               (* write_spv)                           (const GskSlFunction    *function,
                                                                 GskSpvWriter           *writer);
};

static GskSlFunction *
gsk_sl_function_alloc (const GskSlFunctionClass *klass,
                       gsize                     size)
{
  GskSlFunction *function;

  function = g_slice_alloc0 (size);

  function->class = klass;
  function->ref_count = 1;

  return function;
}
#define gsk_sl_function_new(_name, _klass) ((_name *) gsk_sl_function_alloc ((_klass), sizeof (_name)))

/* CONSTRUCTOR */

typedef struct _GskSlFunctionBuiltinConstructor GskSlFunctionBuiltinConstructor;

struct _GskSlFunctionBuiltinConstructor {
  GskSlFunction parent;

  GskSlType *type;
};

static void
gsk_sl_function_builtin_constructor_free (GskSlFunction *function)
{
  GskSlFunctionBuiltinConstructor *builtin_constructor = (GskSlFunctionBuiltinConstructor *) function;

  gsk_sl_type_unref (builtin_constructor->type);

  g_slice_free (GskSlFunctionBuiltinConstructor, builtin_constructor);
}

static GskSlType *
gsk_sl_function_builtin_constructor_get_return_type (const GskSlFunction *function)
{
  const GskSlFunctionBuiltinConstructor *builtin_constructor = (const GskSlFunctionBuiltinConstructor *) function;

  return builtin_constructor->type;
}

static const char *
gsk_sl_function_builtin_constructor_get_name (const GskSlFunction *function)
{
  const GskSlFunctionBuiltinConstructor *builtin_constructor = (const GskSlFunctionBuiltinConstructor *) function;

  return gsk_sl_type_get_name (builtin_constructor->type);
}

static gsize
gsk_sl_function_builtin_constructor_get_n_arguments (const GskSlFunction *function)
{
  return 0;
}

static GskSlType *
gsk_sl_function_builtin_constructor_get_argument_type (const GskSlFunction *function,
                                                       gsize                i)
{
  return NULL;
}

static void
gsk_sl_function_builtin_constructor_print (const GskSlFunction *function,
                                           GskSlPrinter        *printer)
{
}

static guint32
gsk_sl_function_builtin_constructor_write_spv (const GskSlFunction *function,
                                               GskSpvWriter        *writer)
{
  return 0;
}

static const GskSlFunctionClass GSK_SL_FUNCTION_BUILTIN_CONSTRUCTOR = {
  gsk_sl_function_builtin_constructor_free,
  gsk_sl_function_builtin_constructor_get_return_type,
  gsk_sl_function_builtin_constructor_get_name,
  gsk_sl_function_builtin_constructor_get_n_arguments,
  gsk_sl_function_builtin_constructor_get_argument_type,
  gsk_sl_function_builtin_constructor_print,
  gsk_sl_function_builtin_constructor_write_spv,
};

/* CONSTRUCTOR */

typedef struct _GskSlFunctionConstructor GskSlFunctionConstructor;

struct _GskSlFunctionConstructor {
  GskSlFunction parent;

  GskSlType *type;
};

static void
gsk_sl_function_constructor_free (GskSlFunction *function)
{
  GskSlFunctionConstructor *constructor = (GskSlFunctionConstructor *) function;

  gsk_sl_type_unref (constructor->type);

  g_slice_free (GskSlFunctionConstructor, constructor);
}

static GskSlType *
gsk_sl_function_constructor_get_return_type (const GskSlFunction *function)
{
  const GskSlFunctionConstructor *constructor = (const GskSlFunctionConstructor *) function;

  return constructor->type;
}

static const char *
gsk_sl_function_constructor_get_name (const GskSlFunction *function)
{
  const GskSlFunctionConstructor *constructor = (const GskSlFunctionConstructor *) function;

  return gsk_sl_type_get_name (constructor->type);
}

static gsize
gsk_sl_function_constructor_get_n_arguments (const GskSlFunction *function)
{
  const GskSlFunctionConstructor *constructor = (const GskSlFunctionConstructor *) function;

  return gsk_sl_type_get_n_members (constructor->type);
}

static GskSlType *
gsk_sl_function_constructor_get_argument_type (const GskSlFunction *function,
                                               gsize                i)
{
  const GskSlFunctionConstructor *constructor = (const GskSlFunctionConstructor *) function;

  return gsk_sl_type_get_member_type (constructor->type, i);
}

static void
gsk_sl_function_constructor_print (const GskSlFunction *function,
                                   GskSlPrinter        *printer)
{
}

static guint32
gsk_sl_function_constructor_write_spv (const GskSlFunction *function,
                                       GskSpvWriter        *writer)
{
  return 0;
}

static const GskSlFunctionClass GSK_SL_FUNCTION_CONSTRUCTOR = {
  gsk_sl_function_constructor_free,
  gsk_sl_function_constructor_get_return_type,
  gsk_sl_function_constructor_get_name,
  gsk_sl_function_constructor_get_n_arguments,
  gsk_sl_function_constructor_get_argument_type,
  gsk_sl_function_constructor_print,
  gsk_sl_function_constructor_write_spv,
};

/* NATIVE */

typedef struct _GskSlFunctionNative GskSlFunctionNative;

struct _GskSlFunctionNative {
  GskSlFunction parent;

  const GskSlNativeFunction *native;
};

static void
gsk_sl_function_native_free (GskSlFunction *function)
{
  GskSlFunctionNative *native = (GskSlFunctionNative *) function;

  g_slice_free (GskSlFunctionNative, native);
}

static GskSlType *
gsk_sl_function_native_get_return_type (const GskSlFunction *function)
{
  const GskSlFunctionNative *native = (const GskSlFunctionNative *) function;

  return gsk_sl_type_get_builtin (native->native->return_type);
}

static const char *
gsk_sl_function_native_get_name (const GskSlFunction *function)
{
  const GskSlFunctionNative *native = (const GskSlFunctionNative *) function;

  return native->native->name;
}

static gsize
gsk_sl_function_native_get_n_arguments (const GskSlFunction *function)
{
  const GskSlFunctionNative *native = (const GskSlFunctionNative *) function;

  return native->native->n_arguments;
}

static GskSlType *
gsk_sl_function_native_get_argument_type (const GskSlFunction *function,
                                          gsize                i)
{
  const GskSlFunctionNative *native = (const GskSlFunctionNative *) function;

  return gsk_sl_type_get_builtin (native->native->argument_types[i]);
}
static void
gsk_sl_function_native_print (const GskSlFunction *function,
                              GskSlPrinter        *printer)
{
}

static guint32
gsk_sl_function_native_write_spv (const GskSlFunction *function,
                                  GskSpvWriter        *writer)
{
  return 0;
}

static const GskSlFunctionClass GSK_SL_FUNCTION_NATIVE = {
  gsk_sl_function_native_free,
  gsk_sl_function_native_get_return_type,
  gsk_sl_function_native_get_name,
  gsk_sl_function_native_get_n_arguments,
  gsk_sl_function_native_get_argument_type,
  gsk_sl_function_native_print,
  gsk_sl_function_native_write_spv
};

/* DECLARED */

typedef struct _GskSlFunctionDeclared GskSlFunctionDeclared;

struct _GskSlFunctionDeclared {
  GskSlFunction parent;

  GskSlScope *scope;
  GskSlType *return_type;
  char *name;
  GskSlVariable **arguments;
  gsize n_arguments;
  GSList *statements;
};

static void
gsk_sl_function_declared_free (GskSlFunction *function)
{
  GskSlFunctionDeclared *declared = (GskSlFunctionDeclared *) function;
  guint i;

  for (i = 0; i < declared->n_arguments; i++)
    gsk_sl_variable_unref (declared->arguments[i]);
  g_free (declared->arguments);
  if (declared->scope)
    gsk_sl_scope_unref (declared->scope);
  if (declared->return_type)
    gsk_sl_type_unref (declared->return_type);
  g_free (declared->name);
  g_slist_free_full (declared->statements, (GDestroyNotify) gsk_sl_statement_unref);

  g_slice_free (GskSlFunctionDeclared, declared);
}

static GskSlType *
gsk_sl_function_declared_get_return_type (const GskSlFunction *function)
{
  const GskSlFunctionDeclared *declared = (const GskSlFunctionDeclared *) function;

  return declared->return_type;
}

static const char *
gsk_sl_function_declared_get_name (const GskSlFunction *function)
{
  const GskSlFunctionDeclared *declared = (const GskSlFunctionDeclared *) function;

  return declared->name;
}

static gsize
gsk_sl_function_declared_get_n_arguments (const GskSlFunction *function)
{
  const GskSlFunctionDeclared *declared = (const GskSlFunctionDeclared *) function;

  return declared->n_arguments;
}

static GskSlType *
gsk_sl_function_declared_get_argument_type (const GskSlFunction *function,
                                            gsize                i)
{
  const GskSlFunctionDeclared *declared = (const GskSlFunctionDeclared *) function;

  return gsk_sl_pointer_type_get_type (gsk_sl_variable_get_type (declared->arguments[i]));
}
static void
gsk_sl_function_declared_print (const GskSlFunction *function,
                                GskSlPrinter        *printer)
{
  const GskSlFunctionDeclared *declared = (const GskSlFunctionDeclared *) function;
  GSList *l;
  guint i;

  gsk_sl_printer_append (printer, gsk_sl_type_get_name (declared->return_type));
  gsk_sl_printer_newline (printer);

  gsk_sl_printer_append (printer, declared->name);
  gsk_sl_printer_append (printer, " (");
  for (i = 0; i < declared->n_arguments; i++)
    {
      if (i > 0)
        gsk_sl_printer_append (printer, ", ");
      gsk_sl_variable_print (declared->arguments[i], printer);
    }
  gsk_sl_printer_append (printer, ")");
  gsk_sl_printer_newline (printer);

  gsk_sl_printer_append (printer, "{");
  gsk_sl_printer_push_indentation (printer);
  for (l = declared->statements; l; l = l->next)
    {
      gsk_sl_printer_newline (printer);
      gsk_sl_statement_print (l->data, printer);
      gsk_sl_printer_append (printer, ";");
    }
  gsk_sl_printer_pop_indentation (printer);
  gsk_sl_printer_newline (printer);
  gsk_sl_printer_append (printer, "}");
  gsk_sl_printer_newline (printer);
}

static guint32
gsk_sl_function_declared_write_spv (const GskSlFunction *function,
                                    GskSpvWriter        *writer)
{
  GskSlFunctionDeclared *declared = (GskSlFunctionDeclared *) function;
  guint32 return_type_id, function_type_id, function_id, label_id;
  GSList *l;

  /* declare type of function */
  return_type_id = gsk_spv_writer_get_id_for_type (writer, declared->return_type);
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
                      (guint32[1]) { label_id });

  for (l = declared->statements; l; l = l->next)
    {
      gsk_sl_statement_write_spv (l->data, writer);
    }

  gsk_spv_writer_add (writer,
                      GSK_SPV_WRITER_SECTION_CODE,
                      1, GSK_SPV_OP_FUNCTION_END,
                      NULL);

  return function_id;
}

static const GskSlFunctionClass GSK_SL_FUNCTION_DECLARED = {
  gsk_sl_function_declared_free,
  gsk_sl_function_declared_get_return_type,
  gsk_sl_function_declared_get_name,
  gsk_sl_function_declared_get_n_arguments,
  gsk_sl_function_declared_get_argument_type,
  gsk_sl_function_declared_print,
  gsk_sl_function_declared_write_spv,
};

/* API */

GskSlFunction *
gsk_sl_function_new_constructor (GskSlType *type)
{
  if (gsk_sl_type_is_scalar (type) ||
      gsk_sl_type_is_vector (type) ||
      gsk_sl_type_is_matrix (type))
    {
      GskSlFunctionBuiltinConstructor *constructor;

      constructor = gsk_sl_function_new (GskSlFunctionBuiltinConstructor, &GSK_SL_FUNCTION_BUILTIN_CONSTRUCTOR);
      constructor->type = gsk_sl_type_ref (type);

      return &constructor->parent;
    }
  else if (gsk_sl_type_is_struct (type))
    {
      GskSlFunctionConstructor *constructor;

      constructor = gsk_sl_function_new (GskSlFunctionConstructor, &GSK_SL_FUNCTION_CONSTRUCTOR);
      constructor->type = gsk_sl_type_ref (type);

      return &constructor->parent;
    }
  else
    {
      g_assert_not_reached ();

      return NULL;
    }
}

GskSlFunction *
gsk_sl_function_new_native (const GskSlNativeFunction *native)
{
  GskSlFunctionNative *function;

  function = gsk_sl_function_new (GskSlFunctionNative, &GSK_SL_FUNCTION_NATIVE);

  function->native = native;

  return &function->parent;
}

GskSlFunction *
gsk_sl_function_new_parse (GskSlScope        *scope,
                           GskSlPreprocessor *preproc,
                           GskSlType         *return_type,
                           const char        *name)
{
  GskSlFunctionDeclared *function;
  const GskSlToken *token;

  function = gsk_sl_function_new (GskSlFunctionDeclared, &GSK_SL_FUNCTION_DECLARED);
  function->return_type = gsk_sl_type_ref (return_type);
  function->name = g_strdup (name);

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected an openening \"(\"");
      return (GskSlFunction *) function;
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) function);

  function->scope = gsk_sl_scope_new (scope, function->return_type);

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    {
      GPtrArray *arguments = g_ptr_array_new ();

      while (TRUE)
        {
          GskSlDecorations decoration;
          GskSlType *type;
          GskSlVariable *variable;

          gsk_sl_decoration_list_parse (scope,
                                        preproc,
                                        &decoration);

          type = gsk_sl_type_new_parse (scope, preproc);

          token = gsk_sl_preprocessor_get (preproc);
          if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
            {
              GskSlPointerType *pointer_type;
              guint i;

              if (gsk_sl_scope_lookup_variable (function->scope, token->str))
                {
                  for (i = 0; i < function->n_arguments; i++)
                    {
                      if (g_str_equal (gsk_sl_variable_get_name (g_ptr_array_index (arguments, i)), token->str))
                        {
                          gsk_sl_preprocessor_error (preproc, DECLARATION, "Duplicate argument name \"%s\".", token->str);
                          break;
                        }
                    }
                  if (i == arguments->len)
                    gsk_sl_preprocessor_warn (preproc, SHADOW, "Function argument \"%s\" shadows global variable of same name.", token->str);
                }

              pointer_type = gsk_sl_pointer_type_new (type, TRUE, decoration.values[GSK_SL_DECORATION_CALLER_ACCESS].value);
              variable = gsk_sl_variable_new (pointer_type, g_strdup (token->str), NULL, decoration.values[GSK_SL_DECORATION_CONST].set);
              gsk_sl_pointer_type_unref (pointer_type);
              g_ptr_array_add (arguments, variable);
              
              gsk_sl_scope_add_variable (function->scope, variable);
              gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) function);
            }
          else
            {
              gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected an identifier as the variable name.");
            }

          gsk_sl_type_unref (type);

          token = gsk_sl_preprocessor_get (preproc);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_COMMA))
            break;

          gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) function);
        }

      function->n_arguments = arguments->len;
      function->arguments = (GskSlVariable **) g_ptr_array_free (arguments, FALSE);
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected a closing \")\"");
      gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_RIGHT_PAREN);
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) function);

  token = gsk_sl_preprocessor_get (preproc);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
    {
      gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) function);
      return (GskSlFunction *) function;
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_BRACE))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected an opening \"{\"");
      return (GskSlFunction *) function;
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) function);

  for (token = gsk_sl_preprocessor_get (preproc);
       !gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE) && !gsk_sl_token_is (token, GSK_SL_TOKEN_EOF);
       token = gsk_sl_preprocessor_get (preproc))
    {
      GskSlStatement *statement;

      statement = gsk_sl_statement_parse (function->scope, preproc);
      function->statements = g_slist_append (function->statements, statement);
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected closing \"}\" at end of function.");
      gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_RIGHT_BRACE);
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) function);

  return (GskSlFunction *) function;
}

GskSlFunction *
gsk_sl_function_ref (GskSlFunction *function)
{
  g_return_val_if_fail (function != NULL, NULL);

  function->ref_count += 1;

  return function;
}

void
gsk_sl_function_unref (GskSlFunction *function)
{
  if (function == NULL)
    return;

  function->ref_count -= 1;
  if (function->ref_count > 0)
    return;

  function->class->free (function);
}

gboolean
gsk_sl_function_is_builtin_constructor (const GskSlFunction *function)
{
  return function->class == &GSK_SL_FUNCTION_BUILTIN_CONSTRUCTOR;
}

GskSlType *
gsk_sl_function_get_return_type (const GskSlFunction *function)
{
  return function->class->get_return_type (function);
}

const char *
gsk_sl_function_get_name (const GskSlFunction *function)
{
  return function->class->get_name (function);
}

gsize
gsk_sl_function_get_n_arguments (const GskSlFunction *function)
{
  return function->class->get_n_arguments (function);
}

GskSlType *
gsk_sl_function_get_argument_type (const GskSlFunction *function,
                                   gsize                i)
{
  return function->class->get_argument_type (function, i);
}

void
gsk_sl_function_print (const GskSlFunction *function,
                       GskSlPrinter        *printer)
{
  function->class->print (function, printer);
}

guint32
gsk_sl_function_write_spv (const GskSlFunction *function,
                           GskSpvWriter        *writer)
{
  return function->class->write_spv (function, writer);
}

void
gsk_sl_function_matcher_init (GskSlFunctionMatcher *matcher,
                              GList                *list)
{
  matcher->best_matches = list;
  matcher->matches = NULL;
}

void
gsk_sl_function_matcher_finish (GskSlFunctionMatcher *matcher)
{
  g_list_free (matcher->best_matches);
  g_list_free (matcher->matches);
}

gboolean
gsk_sl_function_matcher_has_matches (GskSlFunctionMatcher *matcher)
{
  return matcher->best_matches || matcher->matches;
}

GskSlFunction *
gsk_sl_function_matcher_get_match (GskSlFunctionMatcher *matcher)
{
  if (matcher->best_matches == NULL)
    return NULL;

  if (matcher->best_matches->next != NULL)
    return NULL;

  return matcher->best_matches->data;
}

void
gsk_sl_function_matcher_match_n_arguments (GskSlFunctionMatcher *matcher,
                                           gsize                 n_arguments)
{
  GList *l;

  for (l = matcher->best_matches; l; l = l->next)
    {
      if (gsk_sl_function_get_n_arguments (l->data) != n_arguments)
        matcher->best_matches = g_list_delete_link (matcher->best_matches, l);
    }
  for (l = matcher->matches; l; l = l->next)
    {
      if (gsk_sl_function_get_n_arguments (l->data) != n_arguments)
        matcher->matches = g_list_delete_link (matcher->matches, l);
    }
}

typedef enum {
  MATCH_NONE,
  MATCH_CONVERT_TO_DOUBLE,
  MATCH_CONVERT,
  MATCH_EXACT
} GskSlFunctionMatch;

static GskSlFunctionMatch
gsk_sl_function_matcher_match_types (const GskSlType *function_type,
                                     const GskSlType *argument_type)
{
  if (!gsk_sl_type_can_convert (function_type, argument_type))
    return MATCH_NONE;

  if (gsk_sl_type_equal (function_type, argument_type))
    return MATCH_EXACT;

  if (gsk_sl_type_get_scalar_type (function_type))
    return MATCH_CONVERT_TO_DOUBLE;
  
  return MATCH_CONVERT;
}

void
gsk_sl_function_matcher_match_argument (GskSlFunctionMatcher *matcher,
                                        gsize                 n,
                                        const GskSlType      *argument_type)
{
  GList *best_matches = NULL, *matches = NULL, *l;
  GskSlFunctionMatch best = MATCH_NONE, function_match;

  for (l = matcher->best_matches; l; l = l->next)
    {
      GskSlType *function_type;
      GskSlFunctionMatch function_match;
      
      if (gsk_sl_function_get_n_arguments (l->data) <= n)
        continue;

      function_type = gsk_sl_function_get_argument_type (l->data, n);
      function_match = gsk_sl_function_matcher_match_types (function_type, argument_type);
      if (function_match == MATCH_NONE)
        continue;

      if (function_match == best)
        {
          best_matches = g_list_prepend (best_matches, l->data);
          best = function_match;
        }
      else if (function_match > best)
        {
          matches = g_list_concat (matches, best_matches);
          best_matches = g_list_prepend (NULL, l->data);
          best = function_match;
        }
      else 
        {
          matches = g_list_prepend (matches, l->data);
        }
    }

  for (l = matcher->matches; l; l = l->next)
    {
      GskSlType *function_type;
      
      if (gsk_sl_function_get_n_arguments (l->data) <= n)
        continue;

      function_type = gsk_sl_function_get_argument_type (l->data, n);
      function_match = gsk_sl_function_matcher_match_types (function_type, argument_type);
      if (function_match == MATCH_NONE)
        continue;

      if (function_match > best)
        {
          matches = g_list_concat (matches, best_matches);
          best_matches = NULL;
          best = function_match;
        }
      matches = g_list_prepend (matches, l->data);
    }

  g_list_free (matcher->best_matches);
  g_list_free (matcher->matches);
  matcher->best_matches = best_matches;
  matcher->matches = matches;
}

void
gsk_sl_function_matcher_match_function (GskSlFunctionMatcher *matcher,
                                        const GskSlFunction  *function)
{
  GList *l;
  gsize i, n;

  n = gsk_sl_function_get_n_arguments (function);

  for (l = matcher->best_matches; l; l = l->next)
    {
      GskSlFunction *f = l->data;

      if (gsk_sl_function_get_n_arguments (f) != n)
        continue;

      for (i = 0; i < n; i++)
        {
          if (!gsk_sl_type_equal (gsk_sl_function_get_argument_type (f, i),
                                  gsk_sl_function_get_argument_type (function, i)))
            break;
        }
      if (i == n)
        {
          g_list_free (matcher->best_matches);
          g_list_free (matcher->matches);
          matcher->best_matches = g_list_prepend (NULL, f);
          matcher->matches = NULL;
          return;
        }
    }

  g_list_free (matcher->best_matches);
  g_list_free (matcher->matches);
  matcher->best_matches = NULL;
  matcher->matches = NULL;
}
