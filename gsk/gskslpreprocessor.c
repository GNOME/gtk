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

#include "gskslcompilerprivate.h"
#include "gsksldefineprivate.h"
#include "gsksltokenizerprivate.h"

typedef struct _GskSlPpToken GskSlPpToken;

struct _GskSlPpToken {
  GskCodeLocation location;
  GskSlToken token;
};

struct _GskSlPreprocessor
{
  int ref_count;

  GskSlCompiler *compiler;
  GskSlTokenizer *tokenizer;
  GArray *tokens;
  GHashTable *defines;
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

static void
gsk_sl_preprocessor_clear_token (gpointer data)
{
  GskSlPpToken *pp = data;

  gsk_sl_token_clear (&pp->token);
}

GskSlPreprocessor *
gsk_sl_preprocessor_new (GskSlCompiler *compiler,
                         GBytes        *source)
{
  GskSlPreprocessor *preproc;

  preproc = g_slice_new0 (GskSlPreprocessor);

  preproc->ref_count = 1;
  preproc->compiler = g_object_ref (compiler);
  preproc->tokenizer = gsk_sl_tokenizer_new (source, 
                                            gsk_sl_preprocessor_error_func,
                                            preproc,
                                            NULL);
  preproc->tokens = g_array_new (FALSE, FALSE, sizeof (GskSlPpToken));
  g_array_set_clear_func (preproc->tokens, gsk_sl_preprocessor_clear_token);
  preproc->defines = gsk_sl_compiler_copy_defines (compiler);

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

  g_hash_table_destroy (preproc->defines);
  gsk_sl_tokenizer_unref (preproc->tokenizer);
  g_object_unref (preproc->compiler);
  g_array_free (preproc->tokens, TRUE);

  g_slice_free (GskSlPreprocessor, preproc);
}

static gboolean
gsk_sl_preprocessor_next_token (GskSlPreprocessor *preproc,
                                GskSlPpToken      *pp)
{
  gboolean was_newline;
  
  pp->token = (GskSlToken) { 0, };

  do 
    {
      pp->location = *gsk_sl_tokenizer_get_location (preproc->tokenizer);
      was_newline = gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_NEWLINE);
      gsk_sl_tokenizer_read_token (preproc->tokenizer, &pp->token);
    }
  while (gsk_sl_token_is_skipped (&pp->token));

  return was_newline;
}

static void
gsk_sl_preprocessor_handle_preprocessor_directive (GskSlPreprocessor *preproc)
{
  GskSlPpToken pp;

  gboolean was_newline = gsk_sl_preprocessor_next_token (preproc, &pp);

  /* empty # line */
  if (was_newline)
    {
      gsk_sl_preprocessor_clear_token (&pp);
      return;
    }

  if (gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_IDENTIFIER))
    {
      if (g_str_equal (pp.token.str, "define"))
        {
          gsk_sl_preprocessor_error (preproc, "Unknown preprocessor directive #define.");
          gsk_sl_preprocessor_clear_token (&pp);
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
      else if (g_str_equal (preproc->token.str, "undef"))
        {
        }
      else if (g_str_equal (preproc->token.str, "version"))
        {
        }
#endif
      else
        {
          gsk_sl_preprocessor_error (preproc, "Unknown preprocessor directive #%s.", pp.token.str);
          gsk_sl_preprocessor_clear_token (&pp);
        }
    }
  else
    {
      gsk_sl_preprocessor_error (preproc, "Missing identifier for preprocessor directive.");
      gsk_sl_preprocessor_clear_token (&pp);
    }
  
  while (!gsk_sl_preprocessor_next_token (preproc, &pp))
    gsk_sl_preprocessor_clear_token (&pp);
}

static void
gsk_sl_preprocessor_append_token (GskSlPreprocessor *preproc,
                                  GskSlPpToken      *pp,
                                  GSList            *used_defines)
{
  if (gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_IDENTIFIER))
    {
      GskSlDefine *define;
      char *ident = pp->token.str;

      define = g_hash_table_lookup (preproc->defines, ident);
      if (define &&
          !g_slist_find (used_defines, define))
        {
          GSList new_defines = { define, used_defines };
          GskSlPpToken dpp;
          guint i;

          for (i = 0; i < gsk_sl_define_get_n_tokens (define); i++)
            {
              gsk_sl_define_get_token (define, i, &dpp.location, &dpp.token);
              gsk_sl_preprocessor_append_token (preproc, &dpp, &new_defines);
            }

          gsk_sl_preprocessor_clear_token (pp);
          return;
        }

      gsk_sl_token_init_from_identifier (&pp->token, ident);
      g_free (ident);
    }

  g_array_append_val (preproc->tokens, *pp);
}

static void
gsk_sl_preprocessor_ensure (GskSlPreprocessor *preproc)
{
  GskSlPpToken pp;
  gboolean was_newline = FALSE;

  if (preproc->tokens->len > 0)
    return;

  was_newline = gsk_sl_preprocessor_next_token (preproc, &pp);

  while (TRUE)
    {
      if (gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_HASH))
        {
          if (!was_newline &&
              pp.location.bytes != 0)
            {
              gsk_sl_preprocessor_error (preproc, "Unexpected \"#\" - preprocessor directives must be at start of line.");
              gsk_sl_preprocessor_clear_token (&pp);
              was_newline = gsk_sl_preprocessor_next_token (preproc, &pp);
            }
          else
            {
              gsk_sl_preprocessor_clear_token (&pp);
              gsk_sl_preprocessor_handle_preprocessor_directive (preproc);
              was_newline = TRUE;
            }
        }
      else
        {
          break;
        }
    }

  gsk_sl_preprocessor_append_token (preproc, &pp, NULL);
}

const GskSlToken *
gsk_sl_preprocessor_get (GskSlPreprocessor *preproc)
{
  gsk_sl_preprocessor_ensure (preproc);

  return &g_array_index (preproc->tokens, GskSlPpToken, 0).token;
}

const GskCodeLocation *
gsk_sl_preprocessor_get_location (GskSlPreprocessor *preproc)
{
  gsk_sl_preprocessor_ensure (preproc);

  return &g_array_index (preproc->tokens, GskSlPpToken, 0).location;
}

void
gsk_sl_preprocessor_consume (GskSlPreprocessor *preproc,
                             GskSlNode         *consumer)
{
  gsk_sl_preprocessor_ensure (preproc);

  g_array_remove_index (preproc->tokens, 0);
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
                                  gsk_sl_preprocessor_get_location (preproc),
                                  gsk_sl_preprocessor_get (preproc),
                                  error,
                                  NULL);

  g_error_free (error);
}
