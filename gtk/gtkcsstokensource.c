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

#include "gtkcssprovider.h"

typedef struct _GtkCssTokenSourceTokenizer GtkCssTokenSourceTokenizer;
struct _GtkCssTokenSourceTokenizer {
  GtkCssTokenSource parent;
  GtkCssTokenizer *tokenizer;
  GtkCssToken current_token;
};

static void
gtk_css_token_source_tokenizer_finalize (GtkCssTokenSource *source)
{
  GtkCssTokenSourceTokenizer *tok = (GtkCssTokenSourceTokenizer *) source;

  gtk_css_token_clear (&tok->current_token);
  gtk_css_tokenizer_unref (tok->tokenizer);
}

static void
gtk_css_token_source_tokenizer_consume_token (GtkCssTokenSource *source,
                                              GObject           *consumer)
{
  GtkCssTokenSourceTokenizer *tok = (GtkCssTokenSourceTokenizer *) source;

  gtk_css_token_clear (&tok->current_token);
}

static const GtkCssToken *
gtk_css_token_source_tokenizer_get_token (GtkCssTokenSource   *source)
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

const GtkCssTokenSourceClass GTK_CSS_TOKEN_SOURCE_TOKENIZER = {
  gtk_css_token_source_tokenizer_finalize,
  gtk_css_token_source_tokenizer_consume_token,
  gtk_css_token_source_tokenizer_get_token,
  gtk_css_token_source_tokenizer_error,
};

GtkCssTokenSource *
gtk_css_token_source_new_for_tokenizer (GtkCssTokenizer *tokenizer)
{
  GtkCssTokenSourceTokenizer *source;

  g_return_val_if_fail (tokenizer != NULL, NULL);

  source = gtk_css_token_source_new (GtkCssTokenSourceTokenizer, &GTK_CSS_TOKEN_SOURCE_TOKENIZER);
  source->tokenizer = gtk_css_tokenizer_ref (tokenizer);

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
      token = gtk_css_token_source_get_token (part->source);
      if (gtk_css_token_is (token, part->end_type))
        return;
    }

  gtk_css_token_source_consume_token_as (part->source, consumer);
}

static const GtkCssToken *
gtk_css_token_source_part_get_token (GtkCssTokenSource *source)
{
  GtkCssTokenSourcePart *part = (GtkCssTokenSourcePart *) source;
  static const GtkCssToken eof_token = { GTK_CSS_TOKEN_EOF };
  const GtkCssToken *token;

  token = gtk_css_token_source_get_token (part->source);
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

const GtkCssTokenSourceClass GTK_CSS_TOKEN_SOURCE_PART = {
  gtk_css_token_source_part_finalize,
  gtk_css_token_source_part_consume_token,
  gtk_css_token_source_part_get_token,
  gtk_css_token_source_part_error,
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
      token = gtk_css_token_source_get_token (source);
      if (gtk_css_token_is (token, GPOINTER_TO_UINT (source->blocks->data)))
        source->blocks = g_slist_remove (source->blocks, source->blocks->data);
    }

  source->klass->consume_token (source, consumer);

  token = gtk_css_token_source_get_token (source);
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
gtk_css_token_source_get_token (GtkCssTokenSource *source)
{
  const GtkCssToken *token;

  for (token = source->klass->get_token (source);
       gtk_css_token_is (token, GTK_CSS_TOKEN_COMMENT);
       token = source->klass->get_token (source))
    {
      /* use vfunc here to avoid infloop */
      source->klass->consume_token (source, source->consumer);
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

  for (token = gtk_css_token_source_get_token (source);
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_token_source_get_token (source))
    {
      gtk_css_token_print (token, string);
      gtk_css_token_source_consume_token (source);
    }

  return g_string_free (string, FALSE);
}

gboolean
gtk_css_token_source_consume_whitespace (GtkCssTokenSource *source)
{
  const GtkCssToken *token;
  gboolean seen_whitespace = FALSE;

  for (token = gtk_css_token_source_get_token (source);
       gtk_css_token_is (token, GTK_CSS_TOKEN_WHITESPACE);
       token = gtk_css_token_source_get_token (source))
    {
      seen_whitespace = TRUE;
      gtk_css_token_source_consume_token (source);
    }

  return seen_whitespace;
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

