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

#include "gskslfunctiontypeprivate.h"
#include "gskslnativefunctionprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslprinterprivate.h"
#include "gskslqualifierprivate.h"
#include "gskslscopeprivate.h"
#include "gskslstatementprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskslvalueprivate.h"
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
  GskSlValue *          (* get_constant)                        (const GskSlFunction    *function,
                                                                 GskSlValue            **values,
                                                                 gsize                   n_values);
  void                  (* print)                               (const GskSlFunction    *function,
                                                                 GskSlPrinter           *printer);
  guint32               (* write_spv)                           (const GskSlFunction    *function,
                                                                 GskSpvWriter           *writer,
                                                                 GskSpvWriterFunc        initializer,
                                                                 gpointer                initializer_data);
  guint32               (* write_call_spv)                      (GskSlFunction          *function,
                                                                 GskSpvWriter           *writer,
                                                                 guint32                *arguments);
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

static GskSlValue *
gsk_sl_function_constructor_get_constant (const GskSlFunction  *function,
                                          GskSlValue          **values,
                                          gsize                 n_values)
{
  return NULL;
}

static void
gsk_sl_function_constructor_print (const GskSlFunction *function,
                                   GskSlPrinter        *printer)
{
}

static guint32
gsk_sl_function_constructor_write_spv (const GskSlFunction *function,
                                       GskSpvWriter        *writer,
                                       GskSpvWriterFunc     initializer,
                                       gpointer             initializer_data)
{
  g_assert (initializer == NULL);

  return 0;
}

static guint32
gsk_sl_function_constructor_write_call_spv (GskSlFunction *function,
                                            GskSpvWriter  *writer,
                                            guint32       *arguments)
{
  const GskSlFunctionConstructor *constructor = (const GskSlFunctionConstructor *) function;

  return gsk_spv_writer_composite_construct (writer,
                                             constructor->type,
                                             arguments,
                                             gsk_sl_type_get_n_members (constructor->type));
}

static const GskSlFunctionClass GSK_SL_FUNCTION_CONSTRUCTOR = {
  gsk_sl_function_constructor_free,
  gsk_sl_function_constructor_get_return_type,
  gsk_sl_function_constructor_get_name,
  gsk_sl_function_constructor_get_n_arguments,
  gsk_sl_function_constructor_get_argument_type,
  gsk_sl_function_constructor_get_constant,
  gsk_sl_function_constructor_print,
  gsk_sl_function_constructor_write_spv,
  gsk_sl_function_constructor_write_call_spv
};

/* NATIVE */

typedef struct _GskSlFunctionNative GskSlFunctionNative;

struct _GskSlFunctionNative {
  GskSlFunction parent;

  char *name;
  GskSlFunctionType *type;
  void (* get_constant) (gpointer *retval, gpointer *arguments, gpointer user_data);
  guint32 (* write_spv) (GskSpvWriter *writer, guint32 *arguments, gpointer user_data);
  gpointer user_data;
  GDestroyNotify destroy;
};

static void
gsk_sl_function_native_free (GskSlFunction *function)
{
  GskSlFunctionNative *native = (GskSlFunctionNative *) function;

  if (native->destroy)
    native->destroy (native->user_data);

  gsk_sl_function_type_unref (native->type);
  g_free (native->name);

  g_slice_free (GskSlFunctionNative, native);
}

static GskSlType *
gsk_sl_function_native_get_return_type (const GskSlFunction *function)
{
  const GskSlFunctionNative *native = (const GskSlFunctionNative *) function;

  return gsk_sl_function_type_get_return_type (native->type);
}

static const char *
gsk_sl_function_native_get_name (const GskSlFunction *function)
{
  const GskSlFunctionNative *native = (const GskSlFunctionNative *) function;

  return native->name;
}

static gsize
gsk_sl_function_native_get_n_arguments (const GskSlFunction *function)
{
  const GskSlFunctionNative *native = (const GskSlFunctionNative *) function;

  return gsk_sl_function_type_get_n_arguments (native->type);
}

static GskSlType *
gsk_sl_function_native_get_argument_type (const GskSlFunction *function,
                                          gsize                i)
{
  const GskSlFunctionNative *native = (const GskSlFunctionNative *) function;

  return gsk_sl_function_type_get_argument_type (native->type, i);
}

static GskSlValue *
gsk_sl_function_native_get_constant (const GskSlFunction  *function,
                                     GskSlValue          **values,
                                     gsize                 n_values)
{
  const GskSlFunctionNative *native = (const GskSlFunctionNative *) function;
  gpointer data[gsk_sl_function_type_get_n_arguments (native->type)];
  GskSlValue *result;
  gsize i;

  if (native->get_constant == NULL)
    return NULL;

  result = gsk_sl_value_new (gsk_sl_function_type_get_return_type (native->type));

  for (i = 0; i < n_values; i++)
    {
      data[i] = gsk_sl_value_get_data (values[i]);
    }
  
  native->get_constant (gsk_sl_value_get_data (result),
                        data,
                        native->user_data);

  return result;
}

static void
gsk_sl_function_native_print (const GskSlFunction *function,
                              GskSlPrinter        *printer)
{
}

static guint32
gsk_sl_function_native_write_spv (const GskSlFunction *function,
                                  GskSpvWriter        *writer,
                                  GskSpvWriterFunc     initializer,
                                  gpointer             initializer_data)
{
  g_assert (initializer == NULL);

  return 0;
}

static guint32
gsk_sl_function_native_write_call_spv (GskSlFunction *function,
                                       GskSpvWriter  *writer,
                                       guint32       *arguments)
{
  const GskSlFunctionNative *native = (const GskSlFunctionNative *) function;

  return native->write_spv (writer, arguments, native->user_data);
}

static const GskSlFunctionClass GSK_SL_FUNCTION_NATIVE = {
  gsk_sl_function_native_free,
  gsk_sl_function_native_get_return_type,
  gsk_sl_function_native_get_name,
  gsk_sl_function_native_get_n_arguments,
  gsk_sl_function_native_get_argument_type,
  gsk_sl_function_native_get_constant,
  gsk_sl_function_native_print,
  gsk_sl_function_native_write_spv,
  gsk_sl_function_native_write_call_spv
};

/* DECLARED */

typedef struct _GskSlFunctionDeclared GskSlFunctionDeclared;

struct _GskSlFunctionDeclared {
  GskSlFunction parent;

  GskSlScope *scope;
  char *name;
  GskSlFunctionType *function_type;
  GskSlVariable **arguments;
  GskSlStatement *statement;
};

static void
gsk_sl_function_declared_free (GskSlFunction *function)
{
  GskSlFunctionDeclared *declared = (GskSlFunctionDeclared *) function;
  guint i;

  for (i = 0; i < gsk_sl_function_type_get_n_arguments (declared->function_type); i++)
    gsk_sl_variable_unref (declared->arguments[i]);
  g_free (declared->arguments);
  if (declared->scope)
    gsk_sl_scope_unref (declared->scope);
  gsk_sl_function_type_unref (declared->function_type);
  g_free (declared->name);
  if (declared->statement)
    gsk_sl_statement_unref (declared->statement);

  g_slice_free (GskSlFunctionDeclared, declared);
}

static GskSlType *
gsk_sl_function_declared_get_return_type (const GskSlFunction *function)
{
  const GskSlFunctionDeclared *declared = (const GskSlFunctionDeclared *) function;

  return gsk_sl_function_type_get_return_type (declared->function_type);
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

  return gsk_sl_function_type_get_n_arguments (declared->function_type);
}

static GskSlType *
gsk_sl_function_declared_get_argument_type (const GskSlFunction *function,
                                            gsize                i)
{
  const GskSlFunctionDeclared *declared = (const GskSlFunctionDeclared *) function;

  return gsk_sl_function_type_get_argument_type (declared->function_type, i);
}

static GskSlValue *
gsk_sl_function_declared_get_constant (const GskSlFunction  *function,
                                       GskSlValue          **values,
                                       gsize                 n_values)
{
  return NULL;
}

static void
gsk_sl_function_declared_print (const GskSlFunction *function,
                                GskSlPrinter        *printer)
{
  const GskSlFunctionDeclared *declared = (const GskSlFunctionDeclared *) function;
  guint i;

  gsk_sl_printer_append (printer, gsk_sl_type_get_name (gsk_sl_function_type_get_return_type (declared->function_type)));
  gsk_sl_printer_newline (printer);

  gsk_sl_printer_append (printer, declared->name);
  gsk_sl_printer_append (printer, " (");
  for (i = 0; i < gsk_sl_function_type_get_n_arguments (declared->function_type); i++)
    {
      if (i > 0)
        gsk_sl_printer_append (printer, ", ");
      gsk_sl_variable_print (declared->arguments[i], printer);
    }
  gsk_sl_printer_append (printer, ")");

  if (declared->statement)
    {
      gsk_sl_printer_newline (printer);
      gsk_sl_statement_print (declared->statement, printer);
    }
  else
    {
      gsk_sl_printer_append (printer, ";");
    }

  gsk_sl_printer_newline (printer);
}

static guint32
gsk_sl_function_declared_write_spv (const GskSlFunction *function,
                                    GskSpvWriter        *writer,
                                    GskSpvWriterFunc     initializer,
                                    gpointer             initializer_data)
{
  GskSlFunctionDeclared *declared = (GskSlFunctionDeclared *) function;
  guint32 function_type_id, function_id, label_id;
  GskSlType *return_type;
  gsize i;

  if (declared->statement == NULL)
    return 0;

  return_type = gsk_sl_function_type_get_return_type (declared->function_type);

  /* declare type of function */
  function_type_id = gsk_spv_writer_get_id_for_function_type (writer, declared->function_type);

  function_id = gsk_spv_writer_function (writer, return_type, 0, function_type_id);
  /* add function header */
  for (i = 0; i < gsk_sl_function_type_get_n_arguments (declared->function_type); i++)
    {
      gsk_spv_writer_get_id_for_variable (writer, declared->arguments[i]);
    }

  /* add debug info */
  gsk_spv_writer_name (writer, function_id, declared->name);

  /* add function body */
  label_id = gsk_spv_writer_make_id (writer);
  gsk_spv_writer_start_code_block (writer, label_id, 0, 0);
  gsk_spv_writer_label (writer, GSK_SPV_WRITER_SECTION_DECLARE, label_id);

  if (initializer)
    initializer (writer, initializer_data);

  if (!gsk_sl_statement_write_spv (declared->statement, writer))
    gsk_spv_writer_return (writer);

  gsk_spv_writer_function_end (writer);

  return function_id;
}

static guint32
gsk_sl_function_declared_write_call_spv (GskSlFunction *function,
                                         GskSpvWriter  *writer,
                                         guint32       *arguments)
{
  GskSlFunctionDeclared *declared = (GskSlFunctionDeclared *) function;
  gsize n = gsk_sl_function_type_get_n_arguments (declared->function_type);
  guint32 real_args[n];
  guint32 result;
  gsize i;

  for (i = 0; i < gsk_sl_function_type_get_n_arguments (declared->function_type); i++)
    {
      if (gsk_sl_function_type_is_argument_const (declared->function_type, i))
        {
          real_args[i] = arguments[i];
        }
      else
        {
          real_args[i] = gsk_spv_writer_variable (writer,
                                                  GSK_SPV_WRITER_SECTION_DECLARE,
                                                  gsk_sl_function_type_get_argument_type (declared->function_type, i),
                                                  GSK_SPV_STORAGE_CLASS_FUNCTION,
                                                  GSK_SPV_STORAGE_CLASS_FUNCTION,
                                                  0);
          if (gsk_sl_function_type_is_argument_in (declared->function_type, i))
            {
              gsk_spv_writer_store (writer, real_args[i], arguments[i], 0);
            }
        }
    }

  result = gsk_spv_writer_function_call (writer,
                                         gsk_sl_function_type_get_return_type (declared->function_type),
                                         gsk_spv_writer_get_id_for_function (writer, function),
                                         real_args,
                                         gsk_sl_function_type_get_n_arguments (declared->function_type));

  return result;
}

static const GskSlFunctionClass GSK_SL_FUNCTION_DECLARED = {
  gsk_sl_function_declared_free,
  gsk_sl_function_declared_get_return_type,
  gsk_sl_function_declared_get_name,
  gsk_sl_function_declared_get_n_arguments,
  gsk_sl_function_declared_get_argument_type,
  gsk_sl_function_declared_get_constant,
  gsk_sl_function_declared_print,
  gsk_sl_function_declared_write_spv,
  gsk_sl_function_declared_write_call_spv
};

/* API */

GskSlFunction *
gsk_sl_function_new_constructor (GskSlType *type)
{
  if (gsk_sl_type_is_struct (type))
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
gsk_sl_function_new_native (const char        *name,
                            GskSlFunctionType *type,
                            void               (* get_constant) (gpointer *retval, gpointer *arguments, gpointer user_data),
                            guint32            (* write_spv) (GskSpvWriter *writer, guint32 *arguments, gpointer user_data),
                            gpointer           user_data,
                            GDestroyNotify     destroy)
{
  GskSlFunctionNative *function;

  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (write_spv != NULL, NULL);

  function = gsk_sl_function_new (GskSlFunctionNative, &GSK_SL_FUNCTION_NATIVE);

  function->name = g_strdup (name);
  function->type = gsk_sl_function_type_ref (type);
  function->get_constant = get_constant;
  function->write_spv = write_spv;
  function->user_data = user_data;
  function->destroy = destroy;

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
  function->function_type = gsk_sl_function_type_new (return_type);
  function->name = g_strdup (name);

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Expected an openening \"(\"");
      return (GskSlFunction *) function;
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) function);

  function->scope = gsk_sl_scope_new (scope, return_type);

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    {
      GPtrArray *arguments = g_ptr_array_new ();

      while (TRUE)
        {
          GskSlQualifier qualifier;
          GskSlType *type;
          GskSlVariable *variable;

          gsk_sl_qualifier_parse (&qualifier, scope, preproc, GSK_SL_QUALIFIER_PARAMETER);

          type = gsk_sl_type_new_parse (scope, preproc);

          token = gsk_sl_preprocessor_get (preproc);
          if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
            {
              char *name;

              name = g_strdup (token->str);
              gsk_sl_preprocessor_consume (preproc, (GskSlStatement *) function);

              type = gsk_sl_type_parse_array (type, scope, preproc);

              variable = gsk_sl_variable_new (name, type, &qualifier, NULL);
              function->function_type = gsk_sl_function_type_add_argument (function->function_type,
                                                                           qualifier.storage,
                                                                           type);
              g_ptr_array_add (arguments, variable);
              
              gsk_sl_scope_try_add_variable (function->scope, preproc, variable);

              g_free (name);
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

      g_assert (gsk_sl_function_type_get_n_arguments (function->function_type) == arguments->len);
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

  function->statement = gsk_sl_statement_parse_compound (function->scope, preproc, FALSE);

  if (!gsk_sl_type_is_void (return_type) &&
      gsk_sl_statement_get_jump (function->statement) < GSK_SL_JUMP_RETURN)
    {
      gsk_sl_preprocessor_error (preproc, SYNTAX, "Function does not return a value.");
    }

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

GskSlValue *
gsk_sl_function_get_constant (const GskSlFunction  *function,
                              GskSlValue          **values,
                              gsize                 n_values)
{
  return function->class->get_constant (function, values, n_values);
}

void
gsk_sl_function_print (const GskSlFunction *function,
                       GskSlPrinter        *printer)
{
  function->class->print (function, printer);
}

guint32
gsk_sl_function_write_spv (const GskSlFunction *function,
                           GskSpvWriter        *writer,
                           GskSpvWriterFunc     initializer,
                           gpointer             initializer_data)
{
  return function->class->write_spv (function, writer, initializer, initializer_data);
}

guint32
gsk_sl_function_write_call_spv (GskSlFunction *function,
                                GskSpvWriter  *writer,
                                guint32       *arguments)
{
  return function->class->write_call_spv (function, writer, arguments);
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
