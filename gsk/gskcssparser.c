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

#include "gskcssparserprivate.h"

#define GTK_COMPILATION
#include "gtk/gtkcssprovider.h"

struct _GskCssParser
{
  volatile int ref_count;

  GskCssParserErrorFunc error_func;
  gpointer user_data;
  GDestroyNotify user_destroy;

  GSList *sources;
  GSList *blocks;
  GskCssLocation location;
  GskCssToken token;
};

GskCssParser *
gsk_css_parser_new (GskCssParserErrorFunc error_func,
                    gpointer              user_data,
                    GDestroyNotify        user_destroy)
{
  GskCssParser *self;

  self = g_slice_new0 (GskCssParser);

  self->ref_count = 1;
  self->error_func = error_func;
  self->user_data = user_data;
  self->user_destroy = user_destroy;

  return self;
}

static void
gsk_css_parser_finalize (GskCssParser *self)
{
  g_slist_free_full (self->sources, (GDestroyNotify) gsk_css_tokenizer_unref);

  if (self->user_destroy)
    self->user_destroy (self->user_data);

  g_slice_free (GskCssParser, self);
}

GskCssParser *
gsk_css_parser_ref (GskCssParser *self)
{
  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
gsk_css_parser_unref (GskCssParser *self)
{
  if (g_atomic_int_dec_and_test (&self->ref_count))
    gsk_css_parser_finalize (self);
}

void
gsk_css_parser_add_tokenizer (GskCssParser    *self,
                              GskCssTokenizer *tokenizer)
{
  self->sources = g_slist_prepend (self->sources, gsk_css_tokenizer_ref (tokenizer));
}

void
gsk_css_parser_add_bytes (GskCssParser *self,
                          GBytes       *bytes)
{
  GskCssTokenizer *tokenizer;
  
  tokenizer = gsk_css_tokenizer_new (bytes);
  gsk_css_parser_add_tokenizer (self, tokenizer);
  gsk_css_tokenizer_unref (tokenizer);
}

static void
gsk_css_parser_ensure_token (GskCssParser *self)
{
  GskCssTokenizer *tokenizer;
  GError *error = NULL;

  if (!gsk_css_token_is (&self->token, GSK_CSS_TOKEN_EOF))
    return;

  if (self->sources == NULL)
    return;

  tokenizer = self->sources->data;

  self->location = *gsk_css_tokenizer_get_location (tokenizer);
  if (!gsk_css_tokenizer_read_token (tokenizer, &self->token, &error))
    {
      g_clear_error (&error);
    }
}

const GskCssToken *
gsk_css_parser_peek_token (GskCssParser *self)
{
  static const GskCssToken eof_token = { GSK_CSS_TOKEN_EOF, };

  gsk_css_parser_ensure_token (self);

  if (self->blocks && gsk_css_token_is (&self->token, GPOINTER_TO_UINT (self->blocks->data)))
    return &eof_token;

  return &self->token;
}

const GskCssToken *
gsk_css_parser_get_token (GskCssParser *self)
{
  const GskCssToken *token;

  for (token = gsk_css_parser_peek_token (self);
       gsk_css_token_is (token, GSK_CSS_TOKEN_COMMENT) ||
       gsk_css_token_is (token, GSK_CSS_TOKEN_WHITESPACE);
       token = gsk_css_parser_peek_token (self))
    {
      gsk_css_parser_consume_token (self);
    }

  return token;
}

void
gsk_css_parser_consume_token (GskCssParser *self)
{
  gsk_css_parser_ensure_token (self);

  /* unpreserved tokens MUST be consumed via start_block() */
  g_assert (gsk_css_token_is_preserved (&self->token, NULL));

  gsk_css_token_clear (&self->token);
}

void
gsk_css_parser_start_block (GskCssParser *self)
{
  GskCssTokenType end_token_type;

  gsk_css_parser_ensure_token (self);

  if (gsk_css_token_is_preserved (&self->token, &end_token_type))
    {
      g_critical ("gsk_css_parser_start_block() may only be called for non-preserved tokens");
      return;
    }

  self->blocks = g_slist_prepend (self->blocks, GUINT_TO_POINTER (end_token_type));

  gsk_css_token_clear (&self->token);
}

void
gsk_css_parser_end_block (GskCssParser *self)
{
  g_return_if_fail (self->blocks != NULL);

  gsk_css_parser_skip_until (self, GSK_CSS_TOKEN_EOF);

  if (gsk_css_token_is (&self->token, GSK_CSS_TOKEN_EOF))
    gsk_css_parser_warn_syntax (self, "Unterminated block at end of document");

  self->blocks = g_slist_remove (self->blocks, self->blocks->data);
  gsk_css_token_clear (&self->token);
}

/*
 * gsk_css_parser_skip:
 * @self: a #GskCssParser
 *
 * Skips a component value.
 *
 * This means that if the token is a preserved token, only
 * this token will be skipped. If the token starts a block,
 * the whole block will be skipped.
 **/
void
gsk_css_parser_skip (GskCssParser *self)
{
  const GskCssToken *token;
  
  token = gsk_css_parser_get_token (self);
  if (gsk_css_token_is_preserved (token, NULL))
    {
      gsk_css_parser_consume_token (self);
    }
  else
    {
      gsk_css_parser_start_block (self);
      gsk_css_parser_end_block (self);
    }
}

/*
 * gsk_css_parser_skip_until:
 * @self: a #GskCssParser
 * @token_type: type of token to skip to
 *
 * Repeatedly skips a token until a certain type is reached.
 * After this called, gsk_css_parser_get_token() will either
 * return a token of this type or the eof token.
 *
 * This function is useful for resyncing a parser after encountering
 * an error.
 *
 * If you want to skip until the end, use %GSK_TOKEN_TYPE_EOF
 * as the token type.
 **/
void
gsk_css_parser_skip_until (GskCssParser    *self,
                           GskCssTokenType  token_type)
{
  const GskCssToken *token;
  
  for (token = gsk_css_parser_get_token (self);
       !gsk_css_token_is (token, token_type) &&
       !gsk_css_token_is (token, GSK_CSS_TOKEN_EOF);
       token = gsk_css_parser_get_token (self))
    {
      gsk_css_parser_skip (self);
    }
}

void
gsk_css_parser_emit_error (GskCssParser *self,
                           const GError *error)
{
  self->error_func (self,
                    &self->location,
                    &self->token,
                    error,
                    self->user_data);
}

void
gsk_css_parser_error_syntax (GskCssParser *self,
                             const char   *format,
                             ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PROVIDER_ERROR,
                              GTK_CSS_PROVIDER_ERROR_SYNTAX,
                              format, args);
  gsk_css_parser_emit_error (self, error);
  g_error_free (error);
  va_end (args);
}

void
gsk_css_parser_error_value (GskCssParser *self,
                            const char   *format,
                            ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PROVIDER_ERROR,
                              GTK_CSS_PROVIDER_ERROR_UNKNOWN_VALUE,
                              format, args);
  gsk_css_parser_emit_error (self, error);
  g_error_free (error);
  va_end (args);
}

void
gsk_css_parser_warn_syntax (GskCssParser *self,
                            const char   *format,
                            ...)
{
  va_list args;
  GError *error;

  va_start (args, format);
  error = g_error_new_valist (GTK_CSS_PROVIDER_ERROR,
                              GTK_CSS_PROVIDER_WARN_GENERAL,
                              format, args);
  gsk_css_parser_emit_error (self, error);
  g_error_free (error);
  va_end (args);
}

gboolean
gsk_css_parser_consume_function (GskCssParser *self,
                                 guint         min_args,
                                 guint         max_args,
                                 guint (* parse_func) (GskCssParser *, guint, gpointer),
                                 gpointer      data)
{
  const GskCssToken *token;
  gboolean result = FALSE;
  char *function_name;
  guint arg;

  token = gsk_css_parser_get_token (self);
  g_return_val_if_fail (gsk_css_token_is (token, GSK_CSS_TOKEN_FUNCTION), FALSE);

  function_name = g_strdup (token->string.string);
  gsk_css_parser_start_block (self);

  arg = 0;
  while (TRUE)
    {
      guint parse_args = parse_func (self, arg, data);
      if (parse_args == 0)
        break;
      arg += parse_args;
      token = gsk_css_parser_get_token (self);
      if (gsk_css_token_is (token, GSK_CSS_TOKEN_EOF))
        {
          if (arg < min_args)
            {
              gsk_css_parser_error_syntax (self, "%s() requires at least %u arguments", function_name, min_args);
              break;
            }
          else
            {
              result = TRUE;
              break;
            }
        }
      else if (gsk_css_token_is (token, GSK_CSS_TOKEN_COMMA))
        {
          if (arg >= max_args)
            {
              gsk_css_parser_error_syntax (self, "Expected ')' at end of %s()", function_name);
              break;
            }

          gsk_css_parser_consume_token (self);
          continue;
        }
      else
        {
          gsk_css_parser_error_syntax (self, "Unexpected data at end of %s() argument", function_name);
          break;
        }
    }

  gsk_css_parser_end_block (self);
  g_free (function_name);

  return result;
}

/**
 * gsk_css_parser_consume_if:
 * @self: a #GskCssParser
 * @token_type: type of token to check for
 *
 * Consumes the next token if it matches the given @token_type.
 *
 * This function can be used in loops like this:
 * do {
 *   ... parse one element ...
 * } while (gsk_css_parser_consume_if (parser, GSK_CSS_TOKEN_COMMA);
 *
 * Returns: %TRUE if a token was consumed
 **/
gboolean
gsk_css_parser_consume_if (GskCssParser    *self,
                           GskCssTokenType  token_type)
{
  const GskCssToken *token;

  token = gsk_css_parser_get_token (self);

  if (!gsk_css_token_is (token, token_type))
    return FALSE;

  gsk_css_parser_consume_token (self);
  return TRUE;
}

gboolean
gsk_css_parser_consume_number (GskCssParser *self,
                               double       *number)
{
  const GskCssToken *token;

  token = gsk_css_parser_get_token (self);
  if (gsk_css_token_is (token, GSK_CSS_TOKEN_SIGNED_NUMBER) ||
      gsk_css_token_is (token, GSK_CSS_TOKEN_SIGNLESS_NUMBER) ||
      gsk_css_token_is (token, GSK_CSS_TOKEN_SIGNED_INTEGER) ||
      gsk_css_token_is (token, GSK_CSS_TOKEN_SIGNLESS_INTEGER))
    {
      *number = token->number.number;
      gsk_css_parser_consume_token (self);
      return TRUE;
    }

  /* FIXME: Implement calc() */
  return FALSE;
}

gboolean
gsk_css_parser_consume_percentage (GskCssParser *self,
                                   double       *number)
{
  const GskCssToken *token;

  token = gsk_css_parser_get_token (self);
  if (gsk_css_token_is (token, GSK_CSS_TOKEN_PERCENTAGE))
    {
      *number = token->number.number;
      gsk_css_parser_consume_token (self);
      return TRUE;
    }

  /* FIXME: Implement calc() */
  return FALSE;
}
