/*
 * Copyright © 2016 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors: Benjamin Otte <otte@gnome.org>
 */

#include "config.h"

#include "gtkcsstokensourceprivate.h"

#include "gtkcssnumbervalueprivate.h"
#include "gtkcssprovider.h"

typedef struct _GtkCssTokenSourceTokenizer GtkCssTokenSourceTokenizer;
struct _GtkCssTokenSourceTokenizer {
  GtkCssTokenSource parent;
  GtkCssTokenizer *tokenizer;
  GFile *location;
  GtkCssToken current_token;
};

static void
gtk_css_token_source_tokenizer_finalize (GtkCssTokenSource *source)
{
  GtkCssTokenSourceTokenizer *tok = (GtkCssTokenSourceTokenizer *) source;

  gtk_css_token_clear (&tok->current_token);
  gtk_css_tokenizer_unref (tok->tokenizer);
  if (tok->location)
    g_object_unref (tok->location);
}

static void
gtk_css_token_source_tokenizer_consume_token (GtkCssTokenSource *source,
                                              GObject           *consumer)
{
  GtkCssTokenSourceTokenizer *tok = (GtkCssTokenSourceTokenizer *) source;

  gtk_css_token_clear (&tok->current_token);
}

static const GtkCssToken *
gtk_css_token_source_tokenizer_peek_token (GtkCssTokenSource   *source)
{
  GtkCssTokenSourceTokenizer *tok = (GtkCssTokenSourceTokenizer *) source;

  if (gtk_css_token_is (&tok->current_token, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_tokenizer_read_token (tok->tokenizer, &tok->current_token);

#if 0
      if (!gtk_css_token_is (&tok->current_token, GTK_CSS_TOKEN_EOF)) {
        char *s = gtk_css_token_to_string (&tok->current_token);
        g_print ("%3zu:%02zu %2d %s\n",
                 gtk_css_tokenizer_get_line (tok->tokenizer), gtk_css_tokenizer_get_line_char (tok->tokenizer),
                 tok->current_token.type, s);
        g_free (s);
      }
#endif
    }

  return &tok->current_token;
}

static void
gtk_css_token_source_tokenizer_error (GtkCssTokenSource *source,
                                      const GError      *error)
{
  GtkCssTokenSourceTokenizer *tok = (GtkCssTokenSourceTokenizer *) source;

  /* XXX */
  g_print ("ERROR: %zu:%zu: %s\n",
           gtk_css_tokenizer_get_line (tok->tokenizer),
           gtk_css_tokenizer_get_line_char (tok->tokenizer),
           error->message);
}

static GFile *
gtk_css_token_source_tokenizer_get_location (GtkCssTokenSource *source)
{
  GtkCssTokenSourceTokenizer *tok = (GtkCssTokenSourceTokenizer *) source;

  return tok->location;
}

const GtkCssTokenSourceClass GTK_CSS_TOKEN_SOURCE_TOKENIZER = {
  gtk_css_token_source_tokenizer_finalize,
  gtk_css_token_source_tokenizer_consume_token,
  gtk_css_token_source_tokenizer_peek_token,
  gtk_css_token_source_tokenizer_error,
  gtk_css_token_source_tokenizer_get_location,
};

GtkCssTokenSource *
gtk_css_token_source_new_for_tokenizer (GtkCssTokenizer *tokenizer,
                                        GFile           *location)
{
  GtkCssTokenSourceTokenizer *source;

  g_return_val_if_fail (tokenizer != NULL, NULL);
  g_return_val_if_fail (location == NULL || G_IS_FILE (location), NULL);

  source = gtk_css_token_source_new (GtkCssTokenSourceTokenizer, &GTK_CSS_TOKEN_SOURCE_TOKENIZER);
  source->tokenizer = gtk_css_tokenizer_ref (tokenizer);
  if (location)
    source->location = g_object_ref (location);

  return &source->parent;
}

typedef struct _GtkCssTokenSourcePart GtkCssTokenSourcePart;
struct _GtkCssTokenSourcePart {
  GtkCssTokenSource parent;
  GtkCssTokenSource *source;
  GtkCssTokenType end_type;
};

static void
gtk_css_token_source_part_finalize (GtkCssTokenSource *source)
{
  GtkCssTokenSourcePart *part = (GtkCssTokenSourcePart *) source;

  gtk_css_token_source_unref (part->source);
}

static void
gtk_css_token_source_part_consume_token (GtkCssTokenSource *source,
                                         GObject           *consumer)
{
  GtkCssTokenSourcePart *part = (GtkCssTokenSourcePart *) source;
  const GtkCssToken *token;

  if (!gtk_css_token_get_pending_block (source))
    {
      token = gtk_css_token_source_peek_token (part->source);
      if (gtk_css_token_is (token, part->end_type))
        return;
    }

  gtk_css_token_source_consume_token_as (part->source, consumer);
}

static const GtkCssToken *
gtk_css_token_source_part_peek_token (GtkCssTokenSource *source)
{
  GtkCssTokenSourcePart *part = (GtkCssTokenSourcePart *) source;
  static const GtkCssToken eof_token = { GTK_CSS_TOKEN_EOF };
  const GtkCssToken *token;

  token = gtk_css_token_source_peek_token (part->source);
  if (!gtk_css_token_get_pending_block (source) &&
      gtk_css_token_is (token, part->end_type))
    return &eof_token;

  return token;
}

static void
gtk_css_token_source_part_error (GtkCssTokenSource *source,
                                 const GError      *error)
{
  GtkCssTokenSourcePart *part = (GtkCssTokenSourcePart *) source;

  gtk_css_token_source_emit_error (part->source, error);
}

static GFile *
gtk_css_token_source_part_get_location (GtkCssTokenSource *source)
{
  GtkCssTokenSourcePart *part = (GtkCssTokenSourcePart *) source;

  return gtk_css_token_source_get_location (part->source);
}

const GtkCssTokenSourceClass GTK_CSS_TOKEN_SOURCE_PART = {
  gtk_css_token_source_part_finalize,
  gtk_css_token_source_part_consume_token,
  gtk_css_token_source_part_peek_token,
  gtk_css_token_source_part_error,
  gtk_css_token_source_part_get_location,
};

GtkCssTokenSource *
gtk_css_token_source_new_for_part (GtkCssTokenSource *source,
                                   GtkCssTokenType    end_type)
{
  GtkCssTokenSourcePart *part;

  g_return_val_if_fail (source != NULL, NULL);
  g_return_val_if_fail (end_type != GTK_CSS_TOKEN_EOF, NULL);

  part = gtk_css_token_source_new (GtkCssTokenSourcePart, &GTK_CSS_TOKEN_SOURCE_PART);
  part->source = gtk_css_token_source_ref (source);
  part->end_type = end_type;
  gtk_css_token_source_set_consumer (&part->parent,
                                     gtk_css_token_source_get_consumer (source));

  return &part->parent;
}

GtkCssTokenSource *
gtk_css_token_source_alloc (gsize                         struct_size,
                            const GtkCssTokenSourceClass *klass)
{
  GtkCssTokenSource *source;

  source = g_malloc0 (struct_size);
  source->klass = klass;
  source->ref_count = 1;

  return source;
}

GtkCssTokenSource *
gtk_css_token_source_ref (GtkCssTokenSource *source)
{
  source->ref_count++;

  return source;
}

void
gtk_css_token_source_unref (GtkCssTokenSource *source)
{
  source->ref_count--;
  if (source->ref_count > 0)
    return;

  source->klass->finalize (source);

  g_clear_object (&source->consumer);

  g_free (source);
}


void
gtk_css_token_source_consume_token (GtkCssTokenSource *source)
{
  gtk_css_token_source_consume_token_as (source, source->consumer);
}

void
gtk_css_token_source_consume_token_as (GtkCssTokenSource *source,
                                       GObject           *consumer)
{
  const GtkCssToken *token;
  
  if (source->blocks)
    {
      token = gtk_css_token_source_peek_token (source);
      if (gtk_css_token_is (token, GPOINTER_TO_UINT (source->blocks->data)))
        source->blocks = g_slist_remove (source->blocks, source->blocks->data);
    }

  source->klass->consume_token (source, consumer);

  token = gtk_css_token_source_peek_token (source);
  switch (token->type)
    {
    case GTK_CSS_TOKEN_FUNCTION:
    case GTK_CSS_TOKEN_OPEN_PARENS:
      source->blocks = g_slist_prepend (source->blocks, GUINT_TO_POINTER (GTK_CSS_TOKEN_CLOSE_PARENS));
      break;
    case GTK_CSS_TOKEN_OPEN_SQUARE:
      source->blocks = g_slist_prepend (source->blocks, GUINT_TO_POINTER (GTK_CSS_TOKEN_CLOSE_SQUARE));
      break;
    case GTK_CSS_TOKEN_OPEN_CURLY:
      source->blocks = g_slist_prepend (source->blocks, GUINT_TO_POINTER (GTK_CSS_TOKEN_CLOSE_CURLY));
      break;
    default:
      break;
    }

  source->klass->consume_token (source, consumer);

  if (source->blocks)
    {
      if (token_type == GPOINTER_TO_UINT (source->blocks->data))
        source->blocks = g_slist_remove (source->blocks, source->blocks->data);
    }
}

const GtkCssToken *
gtk_css_token_source_peek_token (GtkCssTokenSource *source)
{
  return source->klass->peek_token (source);
}

const GtkCssToken *
gtk_css_token_source_get_token (GtkCssTokenSource *source)
{
  const GtkCssToken *token;

  for (token = gtk_css_token_source_peek_token (source);
       gtk_css_token_is (token, GTK_CSS_TOKEN_COMMENT) ||
       gtk_css_token_is (token, GTK_CSS_TOKEN_WHITESPACE);
       token = gtk_css_token_source_peek_token (source))
    {
      gtk_css_token_source_consume_token (source);
    }

  return token;
}

void
gtk_css_token_source_consume_all (GtkCssTokenSource *source)
{
  const GtkCssToken *token;

  for (token = gtk_css_token_source_get_token (source);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_token_source_get_token (source))
    {
      gtk_css_token_source_consume_token (source);
    }
}

char *
gtk_css_token_source_consume_to_string (GtkCssTokenSource *source)
{
  GString *string;
  const GtkCssToken *token;

  string = g_string_new (NULL);

  for (token = gtk_css_token_source_peek_token (source);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_token_source_peek_token (source))
    {
      if (gtk_css_token_is (token, GTK_CSS_TOKEN_COMMENT))
        continue;
      gtk_css_token_print (token, string);
      gtk_css_token_source_consume_token (source);
    }

  return g_string_free (string, FALSE);
}

gboolean
gtk_css_token_source_consume_function (GtkCssTokenSource      *source,
                                       guint                   min_args,
                                       guint                   max_args,
                                       gboolean (* parse_func) (GtkCssTokenSource *, guint, gpointer),
                                       gpointer                data)
{
  const GtkCssToken *token;
  GtkCssTokenSource *func_source;
  gboolean result = FALSE;
  char *function_name;
  guint arg;

  token = gtk_css_token_source_get_token (source);
  g_return_val_if_fail (gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION), FALSE);
  function_name = g_strdup (token->string.string);
  gtk_css_token_source_consume_token (source);
  func_source = gtk_css_token_source_new_for_part (source, GTK_CSS_TOKEN_CLOSE_PARENS);

  for (arg = 0; arg < max_args; arg++)
    {
      if (!parse_func (func_source, arg, data))
        {
          gtk_css_token_source_consume_all (func_source);
          break;
        }
      token = gtk_css_token_source_get_token (func_source);
      if (gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
        {
          if (arg + 1 < min_args)
            {
              gtk_css_token_source_error (source, "%s() requires at least %u arguments", function_name, min_args);
              gtk_css_token_source_consume_all (source);
            }
          else
            {
              result = TRUE;
            }
          break;
        }
      else if (gtk_css_token_is (token, GTK_CSS_TOKEN_COMMA))
        {
          gtk_css_token_source_consume_token (func_source);
          continue;
        }
      else
        {
          gtk_css_token_source_error (func_source, "Unexpected data at end of %s() argument", function_name);
          gtk_css_token_source_consume_all (func_source);
          break;
        }
    }

  gtk_css_token_source_unref (func_source);
  token = gtk_css_token_source_get_token (source);
  if (!gtk_css_token_is (token, GTK_CSS_TOKEN_CLOSE_PARENS))
    {
      gtk_css_token_source_error (source, "Expected ')' at end of %s()", function_name);
      gtk_css_token_source_consume_all (source);
      g_free (function_name);
      return FALSE;
    }
  gtk_css_token_source_consume_token (source);
  g_free (function_name);

  return result;
}

gboolean
gtk_css_token_source_consume_number (GtkCssTokenSource *source,
                                     double            *number)
{
  const GtkCssToken *token;
  GtkCssValue *value;

  token = gtk_css_token_source_get_token (source);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_INTEGER))
    {
      *number = token->number.number;
      gtk_css_token_source_consume_token (source);
      return TRUE;
    }

  /* because CSS allows calc() in numbers. Go CSS! */
  value = gtk_css_number_value_token_parse (source, GTK_CSS_PARSE_NUMBER);
  if (value == NULL)
    return FALSE;

  *number = _gtk_css_number_value_get (value, 100);
  _gtk_css_value_unref (value);
  return TRUE;
}

GFile *
gtk_css_token_source_resolve_url (GtkCssTokenSource *source,
                                  const char        *url)
{
  char *scheme;
  GFile *file, *location, *base;

  scheme = g_uri_parse_scheme (url);
  if (scheme != NULL)
    {
      file = g_file_new_for_uri (url);
      g_free (scheme);
      return file;
    }

  location = gtk_css_token_source_get_location (source);
  if (location)
    {
      base = g_file_get_parent (location);
    }
  else
    {
      char *dir = g_get_current_dir ();
      base = g_file_new_for_path (dir);
      g_free (dir);
    }

  file = g_file_resolve_relative_path (base, url);
  g_object_unref (base);

  return file;
}

GFile *
gtk_css_token_source_consume_url (GtkCssTokenSource *source)
{
  const GtkCssToken *token;
  GFile *file;

  token = gtk_css_token_source_get_token (source);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_URL))
    {
      file = gtk_css_token_source_resolve_url (source, token->string.string);
      gtk_css_token_source_consume_token (source);
      return file;
    }
  else if (gtk_css_token_is_function (token, "url"))
    {
      gtk_css_token_source_consume_token (source);
      token = gtk_css_token_source_get_token (source);
      if (!gtk_css_token_is (token, GTK_CSS_TOKEN_STRING))
        {
          gtk_css_token_source_error (source, "Expected string inside url()");
          gtk_css_token_source_consume_all (source);
          return NULL;
        }
      file = gtk_css_token_source_resolve_url (source, token->string.string);
      gtk_css_token_source_consume_token (source);
      token = gtk_css_token_source_get_token (source);
      if (!gtk_css_token_is (token, GTK_CSS_TOKEN_CLOSE_PARENS))
        {
          gtk_css_token_source_error (source, "Expected closing ')' for url()");
          gtk_css_token_source_consume_all (source);
          g_object_unref (file);
          return NULL;
        }
      gtk_css_token_source_consume_token (source);
      return file;
    }
  else
    {
      gtk_css_token_source_error (source, "Expected url()");
      gtk_css_token_source_consume_all (source);
      return NULL;
    }
}

GtkCssTokenType
gtk_css_token_get_pending_block (GtkCssTokenSource *source)
{
  if (!source->blocks)
    return GTK_CSS_TOKEN_EOF;

  return GPOINTER_TO_UINT(source->blocks->data);
}

void
gtk_css_token_source_emit_error (GtkCssTokenSource *source,
                                 const GError      *error)
{
  source->klass->error (source, error);
}

void
gtk_css_token_source_error (GtkCssTokenSource *source,
                            const char        *format,
                            ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PROVIDER_ERROR,
                              GTK_CSS_PROVIDER_ERROR_SYNTAX,
                              format, args);
  gtk_css_token_source_emit_error (source, error);
  g_error_free (error);
  va_end (args);
}

void
gtk_css_token_source_unknown (GtkCssTokenSource *source,
                              const char        *format,
                              ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PROVIDER_ERROR,
                              GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                              format, args);
  gtk_css_token_source_emit_error (source, error);
  g_error_free (error);
  va_end (args);
}

void
gtk_css_token_source_deprecated (GtkCssTokenSource *source,
                                 const char        *format,
                                 ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PROVIDER_ERROR,
                              GTK_CSS_PROVIDER_ERROR_DEPRECATED,
                              format, args);
  gtk_css_token_source_emit_error (source, error);
  g_error_free (error);
  va_end (args);
}

GFile *
gtk_css_token_source_get_location (GtkCssTokenSource *source)
{
  return source->klass->get_location (source);
}

GObject *
gtk_css_token_source_get_consumer (GtkCssTokenSource *source)
{
  return source->consumer;
}

void
gtk_css_token_source_set_consumer (GtkCssTokenSource *source,
                                   GObject           *consumer)
{
  g_set_object (&source->consumer, consumer);
}

