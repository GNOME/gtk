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
};

static void
gsk_sl_node_function_free (GskSlNode *node)
{
  GskSlNodeFunction *function = (GskSlNodeFunction *) node;

  if (function->return_type)
    gsk_sl_type_unref (function->return_type);
  g_free (function->name);

  g_slice_free (GskSlNodeFunction, function);
}

static void
gsk_sl_node_function_print (GskSlNode *node,
                            GString   *string)
{
  GskSlNodeFunction *function = (GskSlNodeFunction *) node;

  gsk_sl_type_print (function->return_type, string);
  g_string_append (string, "\n");

  g_string_append (string, function->name);
  g_string_append (string, " (");
  g_string_append (string, ")\n");

  g_string_append (string, "{\n");
  g_string_append (string, "}\n");
}

static const GskSlNodeClass GSK_SL_NODE_FUNCTION = {
  gsk_sl_node_function_free,
  gsk_sl_node_function_print
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

static gboolean
gsk_sl_node_parse_function_definition (GskSlNodeProgram  *program,
                                       GskSlPreprocessor *stream)
{
  GskSlNodeFunction *function;
  const GskSlToken *token;

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

  token = gsk_sl_preprocessor_get (stream);
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_RIGHT_BRACE))
    {
      gsk_sl_preprocessor_error (stream, "Expected a closing \"}\"");
      gsk_sl_node_unref ((GskSlNode *) function);
      return FALSE;
    }
  gsk_sl_preprocessor_consume (stream, (GskSlNode *) function);

  program->functions = g_slist_prepend (program->functions, function);

  return TRUE;
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
