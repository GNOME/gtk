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

#include "gskslpreprocessorprivate.h"

#include "gsksltokenizerprivate.h"

struct _GskSlPreprocessor
{
  int ref_count;

  GskSlTokenizer *tokenizer;
  GskCodeLocation location;
  GskSlToken token;
};

/* API */

static void
gsk_sl_preprocessor_error_func (GskSlTokenizer        *parser,
                                gboolean               fatal,
                                const GskCodeLocation *location,
                                const GskSlToken      *token,
                                const GError          *error,
                                gpointer               user_data)
{
  g_printerr ("%3zu:%2zu: error: %3u: %s: %s\n",
              location->lines + 1, location->line_bytes,
              token->type, gsk_sl_token_to_string (token),
              error->message);
}

GskSlPreprocessor *
gsk_sl_preprocessor_new (GBytes *source)
{
  GskSlPreprocessor *preproc;

  preproc = g_slice_new0 (GskSlPreprocessor);

  preproc->ref_count = 1;
  preproc->tokenizer = gsk_sl_tokenizer_new (source, 
                                            gsk_sl_preprocessor_error_func,
                                            preproc,
                                            NULL);

  return preproc;
}

GskSlPreprocessor *
gsk_sl_preprocessor_ref (GskSlPreprocessor *preproc)
{
  g_return_val_if_fail (preproc != NULL, NULL);

  preproc->ref_count += 1;

  return preproc;
}

void
gsk_sl_preprocessor_unref (GskSlPreprocessor *preproc)
{
  if (preproc == NULL)
    return;

  preproc->ref_count -= 1;
  if (preproc->ref_count > 0)
    return;

  gsk_sl_tokenizer_unref (preproc->tokenizer);
  gsk_sl_token_clear (&preproc->token);

  g_slice_free (GskSlPreprocessor, preproc);
}

static gboolean
gsk_sl_token_is_skipped (const GskSlToken *token)
{
  return gsk_sl_token_is (token, GSK_SL_TOKEN_ERROR)
      || gsk_sl_token_is (token, GSK_SL_TOKEN_NEWLINE)
      || gsk_sl_token_is (token, GSK_SL_TOKEN_WHITESPACE)
      || gsk_sl_token_is (token, GSK_SL_TOKEN_COMMENT)
      || gsk_sl_token_is (token, GSK_SL_TOKEN_SINGLE_LINE_COMMENT);
}

static gboolean
gsk_sl_preprocessor_next_token (GskSlPreprocessor *preproc)
{
  gboolean was_newline;
  
  do 
    {
      preproc->location = *gsk_sl_tokenizer_get_location (preproc->tokenizer);
      was_newline = gsk_sl_token_is (&preproc->token, GSK_SL_TOKEN_NEWLINE);
      gsk_sl_tokenizer_read_token (preproc->tokenizer, &preproc->token);
    }
  while (gsk_sl_token_is_skipped (&preproc->token));

  return was_newline;
}

static void
gsk_sl_preprocessor_handle_preprocessor_directive (GskSlPreprocessor *preproc)
{
  gboolean was_newline = gsk_sl_preprocessor_next_token (preproc);

  /* empty # line */
  if (was_newline)
    return;

  if (gsk_sl_token_is (&preproc->token, GSK_SL_TOKEN_IDENTIFIER))
    {
      if (g_str_equal (preproc->token.str, "define"))
        {
          gsk_sl_preprocessor_error (preproc, "Unknown preprocessor directive #define.");
        }
#if 0
      else if (g_str_equal (preproc->token.str, "else"))
        {
        }
      else if (g_str_equal (preproc->token.str, "elif"))
        {
        }
      else if (g_str_equal (preproc->token.str, "endif"))
        {
        }
      else if (g_str_equal (preproc->token.str, "error"))
        {
        }
      else if (g_str_equal (preproc->token.str, "extension"))
        {
        }
      else if (g_str_equal (preproc->token.str, "if"))
        {
        }
      else if (g_str_equal (preproc->token.str, "ifdef"))
        {
        }
      else if (g_str_equal (preproc->token.str, "ifndef"))
        {
        }
      else if (g_str_equal (preproc->token.str, "line"))
        {
        }
      else if (g_str_equal (preproc->token.str, "pragma"))
        {
        }
      else if (g_str_equal (preproc->token.str, "version"))
        {
        }
#endif
      else
        {
          gsk_sl_preprocessor_error (preproc, "Unknown preprocessor directive #%s.", preproc->token.str);
        }
    }
  else if (gsk_sl_token_is (&preproc->token, GSK_SL_TOKEN_ELSE))
    {
      gsk_sl_preprocessor_error (preproc, "Unknown preprocessor directive #else.");
    }
  else
    {
      gsk_sl_preprocessor_error (preproc, "Missing identifier for preprocessor directive.");
    }

  while (!gsk_sl_preprocessor_next_token (preproc));
}

static void
gsk_sl_preprocessor_ensure (GskSlPreprocessor *preproc)
{
  gboolean was_newline = FALSE;

  if (!gsk_sl_token_is (&preproc->token, GSK_SL_TOKEN_EOF))
    return;

  was_newline = gsk_sl_preprocessor_next_token (preproc);

  while (TRUE)
    {
      if (gsk_sl_token_is (&preproc->token, GSK_SL_TOKEN_HASH))
        {
          if (!was_newline &&
              preproc->location.bytes != 0)
            {
              gsk_sl_preprocessor_error (preproc, "Unexpected \"#\" - preprocessor directives must be at start of line.");
              was_newline = gsk_sl_preprocessor_next_token (preproc);
            }
          else
            {
              gsk_sl_preprocessor_handle_preprocessor_directive (preproc);
              was_newline = TRUE;
            }
        }
      else
        {
          break;
        }
    }
}

const GskSlToken *
gsk_sl_preprocessor_get (GskSlPreprocessor *preproc)
{
  gsk_sl_preprocessor_ensure (preproc);

  return &preproc->token;
}

const GskCodeLocation *
gsk_sl_preprocessor_get_location (GskSlPreprocessor *preproc)
{
  gsk_sl_preprocessor_ensure (preproc);

  return &preproc->location;
}

void
gsk_sl_preprocessor_consume (GskSlPreprocessor *preproc,
                             GskSlNode         *consumer)
{
  gsk_sl_preprocessor_ensure (preproc);

  gsk_sl_token_clear (&preproc->token);
}

void
gsk_sl_preprocessor_error (GskSlPreprocessor *preproc,
                           const char        *format,
                           ...)
{
  GError *error;
  va_list args;

  va_start (args, format);
  error = g_error_new_valist (G_FILE_ERROR,
                              G_FILE_ERROR_FAILED,
                              format,
                              args);
  va_end (args);

  gsk_sl_preprocessor_ensure (preproc);
  gsk_sl_preprocessor_error_func (preproc->tokenizer,
                                  TRUE,
                                  &preproc->location,
                                  &preproc->token,
                                  error,
                                  NULL);

  g_error_free (error);
}
