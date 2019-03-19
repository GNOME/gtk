/*
 * Copyright © 2019 Benjamin Otte
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

#include "gtkcssparserprivate.h"

#include "gtkcssenums.h"
#include "gtkcsserror.h"

struct _GtkCssParser
{
  volatile int ref_count;

  GtkCssParserErrorFunc error_func;
  gpointer user_data;
  GDestroyNotify user_destroy;

  GSList *sources;
  GSList *blocks;
  GtkCssLocation location;
  GtkCssToken token;
};

GtkCssParser *
gtk_css_parser_new (GtkCssParserErrorFunc error_func,
                    gpointer              user_data,
                    GDestroyNotify        user_destroy)
{
  GtkCssParser *self;

  self = g_slice_new0 (GtkCssParser);

  self->ref_count = 1;
  self->error_func = error_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  return self;
}

static void
gtk_css_parser_finalize (GtkCssParser *self)
{
  g_slist_free_full (self->sources, (GDestroyNotify) gtk_css_tokenizer_unref);

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  g_slice_free (GtkCssParser, self);
}

GtkCssParser *
gtk_css_parser_ref (GtkCssParser *self)
{
  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
gtk_css_parser_unref (GtkCssParser *self)
{
  if (g_atomic_int_dec_and_test (&self->ref_count))
    gtk_css_parser_finalize (self);
}

void
gtk_css_parser_add_tokenizer (GtkCssParser    *self,
                              GtkCssTokenizer *tokenizer)
{
  self->sources = g_slist_prepend (self->sources, gtk_css_tokenizer_ref (tokenizer));
}

void
gtk_css_parser_add_bytes (GtkCssParser *self,
                          GBytes       *bytes)
{
  GtkCssTokenizer *tokenizer;
  
  tokenizer = gtk_css_tokenizer_new (bytes);
  gtk_css_parser_add_tokenizer (self, tokenizer);
  gtk_css_tokenizer_unref (tokenizer);
}

static void
gtk_css_parser_ensure_token (GtkCssParser *self)
{
  GtkCssTokenizer *tokenizer;
  GError *error = NULL;

  if (!gtk_css_token_is (&self->token, GTK_CSS_TOKEN_EOF))
    return;

  if (self->sources == NULL)
    return;

  tokenizer = self->sources->data;

  self->location = *gtk_css_tokenizer_get_location (tokenizer);
  if (!gtk_css_tokenizer_read_token (tokenizer, &self->token, &error))
    {
      g_clear_error (&error);
    }
}

const GtkCssToken *
gtk_css_parser_peek_token (GtkCssParser *self)
{
  static const GtkCssToken eof_token = { GTK_CSS_TOKEN_EOF, };

  gtk_css_parser_ensure_token (self);

  if (self->blocks && gtk_css_token_is (&self->token, GPOINTER_TO_UINT (self->blocks->data)))
    return &eof_token;

  return &self->token;
}

const GtkCssToken *
gtk_css_parser_get_token (GtkCssParser *self)
{
  const GtkCssToken *token;

  for (token = gtk_css_parser_peek_token (self);
       gtk_css_token_is (token, GTK_CSS_TOKEN_COMMENT) ||
       gtk_css_token_is (token, GTK_CSS_TOKEN_WHITESPACE);
       token = gtk_css_parser_peek_token (self))
    {
      gtk_css_parser_consume_token (self);
    }

  return token;
}

void
gtk_css_parser_consume_token (GtkCssParser *self)
{
  gtk_css_parser_ensure_token (self);

  /* unpreserved tokens MUST be consumed via start_block() */
  g_assert (gtk_css_token_is_preserved (&self->token, NULL));

  gtk_css_token_clear (&self->token);
}

void
gtk_css_parser_start_block (GtkCssParser *self)
{
  GtkCssTokenType end_token_type;

  gtk_css_parser_ensure_token (self);

  if (gtk_css_token_is_preserved (&self->token, &end_token_type))
    {
      g_critical ("gtk_css_parser_start_block() may only be called for non-preserved tokens");
      return;
    }

  self->blocks = g_slist_prepend (self->blocks, GUINT_TO_POINTER (end_token_type));

  gtk_css_token_clear (&self->token);
}

void
gtk_css_parser_end_block (GtkCssParser *self)
{
  g_return_if_fail (self->blocks != NULL);

  gtk_css_parser_skip_until (self, GTK_CSS_TOKEN_EOF);

  if (gtk_css_token_is (&self->token, GTK_CSS_TOKEN_EOF))
    gtk_css_parser_warn_syntax (self, "Unterminated block at end of document");

  self->blocks = g_slist_remove (self->blocks, self->blocks->data);
  gtk_css_token_clear (&self->token);
}

/*
 * gtk_css_parser_skip:
 * @self: a #GtkCssParser
 *
 * Skips a component value.
 *
 * This means that if the token is a preserved token, only
 * this token will be skipped. If the token starts a block,
 * the whole block will be skipped.
 **/
void
gtk_css_parser_skip (GtkCssParser *self)
{
  const GtkCssToken *token;
  
  token = gtk_css_parser_get_token (self);
  if (gtk_css_token_is_preserved (token, NULL))
    {
      gtk_css_parser_consume_token (self);
    }
  else
    {
      gtk_css_parser_start_block (self);
      gtk_css_parser_end_block (self);
    }
}

/*
 * gtk_css_parser_skip_until:
 * @self: a #GtkCssParser
 * @token_type: type of token to skip to
 *
 * Repeatedly skips a token until a certain type is reached.
 * After this called, gtk_css_parser_get_token() will either
 * return a token of this type or the eof token.
 *
 * This function is useful for resyncing a parser after encountering
 * an error.
 *
 * If you want to skip until the end, use %GSK_TOKEN_TYPE_EOF
 * as the token type.
 **/
void
gtk_css_parser_skip_until (GtkCssParser    *self,
                           GtkCssTokenType  token_type)
{
  const GtkCssToken *token;
  
  for (token = gtk_css_parser_get_token (self);
       !gtk_css_token_is (token, token_type) &&
       !gtk_css_token_is (token, GTK_CSS_TOKEN_EOF);
       token = gtk_css_parser_get_token (self))
    {
      gtk_css_parser_skip (self);
    }
}

void
gtk_css_parser_emit_error (GtkCssParser *self,
                           const GError *error)
{
  self->error_func (self,
                    &self->location,
                    &self->token,
                    error,
                    self->user_data);
}

void
gtk_css_parser_error_syntax (GtkCssParser *self,
                             const char   *format,
                             ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PARSER_ERROR,
                              GTK_CSS_PARSER_ERROR_SYNTAX,
                              format, args);
  gtk_css_parser_emit_error (self, error);
  g_error_free (error);
  va_end (args);
}

void
gtk_css_parser_error_value (GtkCssParser *self,
                            const char   *format,
                            ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PARSER_ERROR,
                              GTK_CSS_PARSER_ERROR_UNKNOWN_VALUE,
                              format, args);
  gtk_css_parser_emit_error (self, error);
  g_error_free (error);
  va_end (args);
}

void
gtk_css_parser_warn_syntax (GtkCssParser *self,
                            const char   *format,
                            ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PARSER_WARNING,
                              GTK_CSS_PARSER_WARNING_SYNTAX,
                              format, args);
  gtk_css_parser_emit_error (self, error);
  g_error_free (error);
  va_end (args);
}

gboolean
gtk_css_parser_consume_function (GtkCssParser *self,
                                 guint         min_args,
                                 guint         max_args,
                                 guint (* parse_func) (GtkCssParser *, guint, gpointer),
                                 gpointer      data)
{
  const GtkCssToken *token;
  gboolean result = FALSE;
  char *function_name;
  guint arg;

  token = gtk_css_parser_get_token (self);
  g_return_val_if_fail (gtk_css_token_is (token, GTK_CSS_TOKEN_FUNCTION), FALSE);

  function_name = g_strdup (token->string.string);
  gtk_css_parser_start_block (self);

  arg = 0;
  while (TRUE)
    {
      guint parse_args = parse_func (self, arg, data);
      if (parse_args == 0)
        break;
      arg += parse_args;
      token = gtk_css_parser_get_token (self);
      if (gtk_css_token_is (token, GTK_CSS_TOKEN_EOF))
        {
          if (arg < min_args)
            {
              gtk_css_parser_error_syntax (self, "%s() requires at least %u arguments", function_name, min_args);
              break;
            }
          else
            {
              result = TRUE;
              break;
            }
        }
      else if (gtk_css_token_is (token, GTK_CSS_TOKEN_COMMA))
        {
          if (arg >= max_args)
            {
              gtk_css_parser_error_syntax (self, "Expected ')' at end of %s()", function_name);
              break;
            }

          gtk_css_parser_consume_token (self);
          continue;
        }
      else
        {
          gtk_css_parser_error_syntax (self, "Unexpected data at end of %s() argument", function_name);
          break;
        }
    }

  gtk_css_parser_end_block (self);
  g_free (function_name);

  return result;
}

/**
 * gtk_css_parser_consume_if:
 * @self: a #GtkCssParser
 * @token_type: type of token to check for
 *
 * Consumes the next token if it matches the given @token_type.
 *
 * This function can be used in loops like this:
 * do {
 *   ... parse one element ...
 * } while (gtk_css_parser_consume_if (parser, GTK_CSS_TOKEN_COMMA);
 *
 * Returns: %TRUE if a token was consumed
 **/
gboolean
gtk_css_parser_consume_if (GtkCssParser    *self,
                           GtkCssTokenType  token_type)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);

  if (!gtk_css_token_is (token, token_type))
    return FALSE;

  gtk_css_parser_consume_token (self);
  return TRUE;
}

gboolean
gtk_css_parser_consume_number (GtkCssParser *self,
                               double       *number)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
      gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      *number = token->number.number;
      gtk_css_parser_consume_token (self);
      return TRUE;
    }

  gtk_css_parser_error_syntax (self, "Expected a number");
  /* FIXME: Implement calc() */
  return FALSE;
}

gboolean
gtk_css_parser_consume_percentage (GtkCssParser *self,
                                   double       *number)
{
  const GtkCssToken *token;

  token = gtk_css_parser_get_token (self);
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_PERCENTAGE))
    {
      *number = token->number.number;
      gtk_css_parser_consume_token (self);
      return TRUE;
    }

  gtk_css_parser_error_syntax (self, "Expected a percentage");
  /* FIXME: Implement calc() */
  return FALSE;
}
