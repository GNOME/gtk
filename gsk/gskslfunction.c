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

#include "gskslnodeprivate.h"
#include "gskslpreprocessorprivate.h"
#include "gskslscopeprivate.h"
#include "gsksltokenizerprivate.h"
#include "gsksltypeprivate.h"
#include "gskspvwriterprivate.h"

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

/* BUILTIN CONSTRUCTOR */

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

static void
gsk_sl_function_constructor_print (const GskSlFunction *function,
                                   GString             *string)
{
}

static guint
gsk_sl_function_builtin_get_args_by_type (const GskSlType *type)
{
  if (gsk_sl_type_is_scalar (type))
    return 1;
  else if (gsk_sl_type_is_vector (type))
    return gsk_sl_type_get_length (type);
  else if (gsk_sl_type_is_matrix (type))
    return gsk_sl_type_get_length (type) * gsk_sl_function_builtin_get_args_by_type (gsk_sl_type_get_index_type (type));
  else
    return 0;
}

static gboolean
gsk_sl_function_constructor_matches (const GskSlFunction  *function,
                                     GskSlType           **arguments,
                                     gsize                 n_arguments,
                                     GError              **error)
{
  const GskSlFunctionConstructor *constructor = (const GskSlFunctionConstructor *) function;
  guint needed, provided;
  gsize i;

  if (n_arguments == 1 && gsk_sl_type_is_scalar (arguments[0]))
    return TRUE;

  needed = gsk_sl_function_builtin_get_args_by_type (constructor->type);

  for (i = 0; i < n_arguments; i++)
    {
      if (needed == 0)
        {
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Too many arguments given to constructor, only the first %"G_GSIZE_FORMAT" are necessary.", i);
          return FALSE;
        }

      provided = gsk_sl_function_builtin_get_args_by_type (arguments[i]);
      if (provided == 0)
        {
          g_set_error (error,
                       G_FILE_ERROR, G_FILE_ERROR_FAILED,
                       "Invalid type %s for constructor in argument %"G_GSIZE_FORMAT,
                       gsk_sl_type_get_name (arguments[i]), i + 1);
          return FALSE;
        }

      needed -= MIN (needed, provided);
    }

  return TRUE;
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
  gsk_sl_function_constructor_print,
  gsk_sl_function_constructor_matches,
  gsk_sl_function_constructor_write_spv,
};

/* DECLARED */

typedef struct _GskSlFunctionDeclared GskSlFunctionDeclared;

struct _GskSlFunctionDeclared {
  GskSlFunction parent;

  GskSlScope *scope;
  GskSlType *return_type;
  char *name;
  GSList *statements;
};

static void
gsk_sl_function_declared_free (GskSlFunction *function)
{
  GskSlFunctionDeclared *declared = (GskSlFunctionDeclared *) function;

  if (declared->scope)
    gsk_sl_scope_unref (declared->scope);
  if (declared->return_type)
    gsk_sl_type_unref (declared->return_type);
  g_free (declared->name);
  g_slist_free_full (declared->statements, (GDestroyNotify) gsk_sl_node_unref);

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

static void
gsk_sl_function_declared_print (const GskSlFunction *function,
                                GString             *string)
{
  const GskSlFunctionDeclared *declared = (const GskSlFunctionDeclared *) function;
  GSList *l;

  g_string_append (string, gsk_sl_type_get_name (declared->return_type));
  g_string_append (string, "\n");

  g_string_append (string, declared->name);
  g_string_append (string, " (");
  g_string_append (string, ")\n");

  g_string_append (string, "{\n");
  for (l = declared->statements; l; l = l->next)
    {
      g_string_append (string, "  ");
      gsk_sl_node_print (l->data, string);
      g_string_append (string, ";\n");
    }
  g_string_append (string, "}\n");
}

static gboolean
gsk_sl_function_declared_matches (const GskSlFunction  *function,
                                  GskSlType           **arguments,
                                  gsize                 n_arguments,
                                  GError              **error)
{
  if (n_arguments > 0)
    {
       g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Function only takes %u arguments.", 0);
       return FALSE;
    }

  return TRUE;
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
      gsk_sl_node_write_spv (l->data, writer);
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
  gsk_sl_function_declared_print,
  gsk_sl_function_declared_matches,
  gsk_sl_function_declared_write_spv,
};

/* API */

GskSlFunction *
gsk_sl_function_new_constructor (GskSlType *type)
{
  GskSlFunctionConstructor *constructor;

  constructor = gsk_sl_function_new (GskSlFunctionConstructor, &GSK_SL_FUNCTION_CONSTRUCTOR);

  constructor->type = gsk_sl_type_ref (type);

  return &constructor->parent;
}

GskSlFunction *
gsk_sl_function_new_parse (GskSlScope        *scope,
                           GskSlPreprocessor *preproc,
                           GskSlType         *return_type,
                           const char        *name)
{
  GskSlFunctionDeclared *function;
  const GskSlToken *token;
  gboolean success = TRUE;

  function = gsk_sl_function_new (GskSlFunctionDeclared, &GSK_SL_FUNCTION_DECLARED);
  function->return_type = gsk_sl_type_ref (return_type);
  function->name = g_strdup (name);

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      gsk_sl_preprocessor_error (preproc, "Expected an openening \"(\"");
      gsk_sl_function_unref ((GskSlFunction *) function);
      return NULL;
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlNode *) function);

  token = gsk_sl_preprocessor_get (preproc);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_PAREN))
    {
      gsk_sl_preprocessor_error (preproc, "Expected a closing \")\"");
      gsk_sl_function_unref ((GskSlFunction *) function);
      return NULL;
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlNode *) function);

  token = gsk_sl_preprocessor_get (preproc);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_SEMICOLON))
    {
      gsk_sl_preprocessor_consume (preproc, (GskSlNode *) function);
      return (GskSlFunction *) function;
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_BRACE))
    {
      gsk_sl_preprocessor_error (preproc, "Expected an opening \"{\"");
      gsk_sl_function_unref ((GskSlFunction *) function);
      return NULL;
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlNode *) function);

  function->scope = gsk_sl_scope_new (scope, function->return_type);

  for (token = gsk_sl_preprocessor_get (preproc);
       !gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE) && !gsk_sl_token_is (token, GSK_SL_TOKEN_EOF);
       token = gsk_sl_preprocessor_get (preproc))
    {
      GskSlNode *statement;

      statement = gsk_sl_node_parse_statement (function->scope, preproc);
      if (statement)
        function->statements = g_slist_append (function->statements, statement);
      else
        success = FALSE;
    }

  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE))
    {
      gsk_sl_preprocessor_error (preproc, "Missing closing \"}\" at end.");
      gsk_sl_function_unref ((GskSlFunction *) function);
      return NULL;
    }
  gsk_sl_preprocessor_consume (preproc, (GskSlNode *) function);

  if (!success)
    {
      gsk_sl_function_unref ((GskSlFunction *) function);
      return NULL;
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

void
gsk_sl_function_print (const GskSlFunction *function,
                       GString             *string)
{
  function->class->print (function, string);
}

gboolean
gsk_sl_function_matches (const GskSlFunction  *function,
                         GskSlType           **arguments,
                         gsize                 n_arguments,
                         GError              **error)
{
  return function->class->matches (function, arguments, n_arguments, error);
}

guint32
gsk_sl_function_write_spv (const GskSlFunction *function,
                           GskSpvWriter        *writer)
{
  return function->class->write_spv (function, writer);
}

