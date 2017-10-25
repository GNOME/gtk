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

#include "gskcodesource.h"
#include "gskslcompilerprivate.h"
#include "gsksldefineprivate.h"
#include "gskslenvironmentprivate.h"
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
  GskSlEnvironment *environment;
  GskSlTokenizer *tokenizer;
  GSList *pending_tokenizers;
  GArray *tokens;
  GHashTable *defines;
  gboolean fatal_error;
  GSList *conditionals;
};

typedef enum {
  /* ignore this part, the last conditional check didn't match */
  GSK_COND_IGNORE = (1 << 0),
  /* we're inside the else block, so no more elif */
  GSK_COND_ELSE = (1 << 1),
  /* we've had a match in one of the previous blocks (or this one matches) */
  GSK_COND_MATCH = (1 << 2)
} GskConditional;

/* API */

static void
gsk_sl_preprocessor_error_func (GskSlTokenizer        *parser,
                                gboolean               fatal,
                                const GskCodeLocation *location,
                                const GskSlToken      *token,
                                const GError          *error,
                                gpointer               user_data)
{
  gsk_sl_preprocessor_emit_error (user_data, TRUE, location, error);
}

static void
gsk_sl_preprocessor_clear_token (gpointer data)
{
  GskSlPpToken *pp = data;

  gsk_sl_token_clear (&pp->token);
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

  g_slist_free (preproc->conditionals);
  g_hash_table_destroy (preproc->defines);
  g_slist_free_full (preproc->pending_tokenizers, (GDestroyNotify) gsk_sl_tokenizer_unref);
  gsk_sl_tokenizer_unref (preproc->tokenizer);
  gsk_sl_environment_unref (preproc->environment);
  g_object_unref (preproc->compiler);
  g_array_free (preproc->tokens, TRUE);

  g_slice_free (GskSlPreprocessor, preproc);
}

gboolean
gsk_sl_preprocessor_has_fatal_error (GskSlPreprocessor *preproc)
{
  return preproc->fatal_error;
}

GskSlEnvironment *
gsk_sl_preprocessor_get_environment (GskSlPreprocessor *preproc)
{
  return preproc->environment;
}

static void
gsk_sl_preprocessor_push_conditional (GskSlPreprocessor *preproc,
                                      GskConditional     cond)
{
  preproc->conditionals = g_slist_prepend (preproc->conditionals, GUINT_TO_POINTER (cond));
}

static GskConditional
gsk_sl_preprocessor_pop_conditional (GskSlPreprocessor *preproc)
{
  GskConditional cond = GPOINTER_TO_UINT (preproc->conditionals->data);

  preproc->conditionals = g_slist_remove (preproc->conditionals, preproc->conditionals->data);

  return cond;
}

static gboolean
gsk_sl_preprocessor_has_conditional (GskSlPreprocessor *preproc)
{
  return preproc->conditionals != NULL;
}

static gboolean
gsk_sl_preprocessor_in_ignored_conditional (GskSlPreprocessor *preproc)
{
  GSList *l;

  for (l = preproc->conditionals; l; l = l->next)
    {
      if (GPOINTER_TO_UINT (l->data) & GSK_COND_IGNORE)
        return TRUE;
    }

  return FALSE;
}

static void
gsk_sl_preprocessor_handle_version (GskSlPreprocessor *preproc,
                                    GskCodeLocation   *location,
                                    int                version,
                                    const char        *profile_name,
                                    gboolean           first_token_ever)
{
  GskSlEnvironment *new_environment;
  GskSlProfile profile;
  GError *error = NULL;

  if (version <= 0)
    {
      gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, location, "version must be a positive number.");
      return;
    }
  if (profile_name == NULL)
    profile = GSK_SL_PROFILE_NONE;
  else if (g_str_equal (profile_name, "core"))
    profile = GSK_SL_PROFILE_CORE;
  else if (g_str_equal (profile_name, "compatibility"))
    profile = GSK_SL_PROFILE_COMPATIBILITY;
  else if (g_str_equal (profile_name, "es"))
    profile = GSK_SL_PROFILE_ES;
  else
    {
      gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, location, "Unknown #version profile \"%s\".", profile_name);
      return;
    }
  if (!first_token_ever)
    {
      gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, location, "#version directive must be first in compilation.");
      return;
    }

  new_environment = gsk_sl_environment_new_similar (preproc->environment, profile, version, &error);
  if (new_environment == NULL)
    {
      gsk_sl_preprocessor_emit_error (preproc, TRUE, location, error);
      g_clear_error (&error);
    }
  else
    {
      gsk_sl_environment_unref (preproc->environment);
      preproc->environment = new_environment;
    }
}

#define token_array_get_token(array, i) (&g_array_index ((array), GskSlPpToken, (i)).token)
#define token_array_get_location(array, i) (&g_array_index ((array), GskSlPpToken, (i)).location)
#define token_array_error(preproc, array, i, ...) gsk_sl_preprocessor_error_full ((preproc), PREPROCESSOR, token_array_get_location (array, i), __VA_ARGS__)

static int
gsk_sl_preprocessor_handle_defined_expression (GskSlPreprocessor *preproc,
                                               GArray            *tokens,
                                               gint              *index)
{
  const GskSlToken *token;
  gboolean paren = FALSE;
  int result;

  (*index)++;

  if (*index >= tokens->len)
    {
      token_array_error (preproc, tokens, tokens->len - 1, "\"defined\" without argument.");
      return 0;
    }

  token = token_array_get_token (tokens, *index);
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
    {
      paren = TRUE;
      (*index)++;
      if (*index >= tokens->len)
        {
          token_array_error (preproc, tokens, tokens->len - 1, "\"defined()\" without argument.");
          return 0;
        }
      token = token_array_get_token (tokens, *index);
    }
  if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {
      (*index)++;
      if (g_hash_table_lookup (preproc->defines, token->str))
        result = 1;
      else
        result = 0;
    }
  else
    {
      token_array_error (preproc, tokens, *index, "Expected identifier after \"defined\".");
    }
  if (paren)
    {
      if (*index >= tokens->len ||
          !gsk_sl_token_is (token_array_get_token (tokens, *index), GSK_SL_TOKEN_RIGHT_PAREN))
        {
          token_array_error (preproc, tokens, *index, "Expected closing \")\" for \"defined()\".");
          return 0;
        }
      else
        {
          (*index)++;
        }
    }
  return result;
}

static int
gsk_sl_preprocessor_handle_primary_expression (GskSlPreprocessor *preproc,
                                               GArray            *tokens,
                                               gint              *index)
{
  const GskSlToken *token;

  if (*index >= tokens->len)
    {
      token_array_error (preproc, tokens, tokens->len - 1, "Expected value.");
      return 0;
    }

  token = token_array_get_token (tokens, (*index));

  if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {
      if (g_str_equal (token->str, "defined"))
        {
          return gsk_sl_preprocessor_handle_defined_expression (preproc, tokens, index);
        }
      else
        {
          token_array_error (preproc, tokens, *index, "Unexpected identifier \"%s\".", token->str);
          (*index)++;
          return 0;
        }
    }
  else if (gsk_sl_token_is (token, GSK_SL_TOKEN_INTCONSTANT))
    {
      (*index)++;
      return token->i32;
    }
  else if (gsk_sl_token_is (token, GSK_SL_TOKEN_UINTCONSTANT))
    {
      (*index)++;
      return token->u32;
    }
  else
    {
      token_array_error (preproc, tokens, *index, "Unexpected token in #if statement.");
      (*index)++;
      return 0;
    }
}

static int
gsk_sl_preprocessor_handle_expression (GskSlPreprocessor *preproc,
                                       GArray            *tokens,
                                       gint              *index)
{
  int result;

  result = gsk_sl_preprocessor_handle_primary_expression (preproc, tokens, index);

  if (*index < tokens->len)
    {
      token_array_error (preproc, tokens, *index, "Expected newline after expression.");
    }

  return result;
}

static gboolean
gsk_sl_preprocessor_next_token (GskSlPreprocessor *preproc,
                                GskSlPpToken      *pp,
                                gboolean          *last_was_newline)
{
  gboolean contained_newline = FALSE;
  
  pp->token = (GskSlToken) { 0, };

  do 
    {
      pp->location = *gsk_sl_tokenizer_get_location (preproc->tokenizer);
      *last_was_newline = gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_NEWLINE);
      contained_newline |= gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_NEWLINE);
      gsk_sl_tokenizer_read_token (preproc->tokenizer, &pp->token);
    }
  while (gsk_sl_token_is_skipped (&pp->token));

  return contained_newline || gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_EOF);
}

static void
gsk_sl_preprocessor_handle_token (GskSlPreprocessor *preproc,
                                  GskSlPpToken      *pp,
                                  gboolean           was_newline,
                                  gboolean           was_start_of_document);

static gboolean
gsk_sl_preprocessor_include (GskSlPreprocessor *preproc,
                             GArray            *tokens,
                             gboolean           include_local)
{
  GskCodeSource *source;
  GError *error = NULL;
    
  source = gsk_sl_compiler_resolve_include (preproc->compiler,
                                            gsk_sl_tokenizer_get_location (preproc->tokenizer)->source,
                                            include_local,
                                            token_array_get_token (tokens, 1)->str,
                                            &error);
  if (source == NULL)
    {
      gsk_sl_preprocessor_emit_error (preproc, TRUE, token_array_get_location (tokens, 1), error);
      g_error_free (error);
      return FALSE;
    }

  if (g_slist_length (preproc->pending_tokenizers) > 20)
    {
      token_array_error (preproc, tokens, 1, "#include nested too deeply.");
      return FALSE;
    }

  if (tokens->len > 2)
    {
      token_array_error (preproc, tokens, 2, "Extra content after #include directive");
      return FALSE;
    }

  preproc->pending_tokenizers = g_slist_prepend (preproc->pending_tokenizers, preproc->tokenizer);
  preproc->tokenizer = gsk_sl_tokenizer_new (source, 
                                             gsk_sl_preprocessor_error_func,
                                             preproc,
                                             NULL);

  return TRUE;
}

static void
gsk_sl_preprocessor_append_token (GskSlPreprocessor *preproc,
                                  GskSlPpToken      *pp,
                                  GSList            *used_defines)
{
  if (gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_EOF) &&
      preproc->pending_tokenizers)
    {
      gboolean was_newline;
      gsk_sl_tokenizer_unref (preproc->tokenizer);
      preproc->tokenizer = preproc->pending_tokenizers->data;
      preproc->pending_tokenizers = g_slist_remove (preproc->pending_tokenizers, preproc->tokenizer);
      gsk_sl_preprocessor_clear_token (pp);
      gsk_sl_preprocessor_next_token (preproc, pp, &was_newline);
      gsk_sl_preprocessor_handle_token (preproc, pp, TRUE, FALSE);
      return;
    }

  if (gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_EOF))
    {
      while (gsk_sl_preprocessor_has_conditional (preproc))
        {
          gsk_sl_preprocessor_pop_conditional (preproc);
          gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp->location, "Missing #endif.");
        }
    }

  if (gsk_sl_preprocessor_in_ignored_conditional (preproc))
    {
      gsk_sl_preprocessor_clear_token (pp);
      return;
    }

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
  else if (gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_STRING))
    {
      gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp->location, "Unexpected string.");
      gsk_sl_preprocessor_clear_token (pp);
      return;
    }

  g_array_append_val (preproc->tokens, *pp);
}

static GArray *
gsk_sl_preprocessor_read_line (GskSlPreprocessor *preproc)
{
  GskSlPpToken pp;
  GArray *tokens;

  tokens = g_array_new (FALSE, FALSE, sizeof (GskSlPpToken));
  g_array_set_clear_func (tokens, gsk_sl_preprocessor_clear_token);

  while (TRUE)
    {
      pp.location = *gsk_sl_tokenizer_get_location (preproc->tokenizer);
      gsk_sl_tokenizer_read_token (preproc->tokenizer, &pp.token);
      if (gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_EOF) ||
          gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_NEWLINE))
        break;
      if (!gsk_sl_token_is_skipped (&pp.token))
        g_array_append_val (tokens, pp);
    }

  return tokens;
}

static void
gsk_sl_preprocessor_handle_preprocessor_directive (GskSlPreprocessor *preproc,
                                                   gboolean           first_token_ever)
{
  GskSlPpToken pp;
  const GskSlToken *token;
  gboolean was_newline;
  GArray *tokens;

  tokens = gsk_sl_preprocessor_read_line (preproc);
  
  if (tokens->len == 0)
    /* empty # line */
    goto out;

  token = &g_array_index (tokens, GskSlPpToken, 0).token;
  if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
    {
      token_array_error (preproc, tokens, 0, "Missing identifier for preprocessor directive.");
      goto out;
    }

  if (g_str_equal (token->str, "else"))
    {
      if (gsk_sl_preprocessor_has_conditional (preproc))
        {
          GskConditional cond = gsk_sl_preprocessor_pop_conditional (preproc);

          if (cond & GSK_COND_ELSE)
            {
              token_array_error (preproc, tokens, 0, "#else after #else.");
              cond |= GSK_COND_IGNORE;
            }
          else if (cond & GSK_COND_MATCH)
            cond |= GSK_COND_IGNORE;
          else
            cond &= ~GSK_COND_IGNORE;

          cond |= GSK_COND_ELSE | GSK_COND_MATCH;

          gsk_sl_preprocessor_push_conditional (preproc, cond);
        }
      else
        {
          token_array_error (preproc, tokens, 0, "#else without #if.");
        }

      if (tokens->len > 1)
        token_array_error (preproc, tokens, 1, "Expected newline after #else.");
    }
  else if (g_str_equal (token->str, "elif"))
    {
      if (gsk_sl_preprocessor_has_conditional (preproc))
        {
          GskConditional cond = gsk_sl_preprocessor_pop_conditional (preproc);

          if (cond & GSK_COND_ELSE)
            {
              token_array_error (preproc, tokens, 0, "#elif after #else.");
              cond |= GSK_COND_IGNORE;
            }
          else
            {
              int expr, index;
              
              index = 1;
              expr = gsk_sl_preprocessor_handle_expression (preproc, tokens, &index);
            
              if (cond & GSK_COND_MATCH)
                {
                  cond |= GSK_COND_IGNORE;
                }
              else if (expr)
                {
                  cond &= ~GSK_COND_IGNORE;
                  cond |= GSK_COND_MATCH;
                }
              else
                {
                  cond |= GSK_COND_IGNORE;
                }
            }

          gsk_sl_preprocessor_push_conditional (preproc, cond);
        }
      else
        {
          token_array_error (preproc, tokens, 0, "#elif without #if.");
        }

    }
  else if (g_str_equal (token->str, "endif"))
    {
      if (gsk_sl_preprocessor_has_conditional (preproc))
        {
          gsk_sl_preprocessor_pop_conditional (preproc);
        }
      else
        {
          token_array_error (preproc, tokens, 0, "#endif without #if.");
        }

      if (tokens->len > 1)
          token_array_error (preproc, tokens, 1, "Expected newline after #endif.");
    }
  else if (g_str_equal (token->str, "if"))
    {
      int expr, index;
      
      index = 1;
      expr = gsk_sl_preprocessor_handle_expression (preproc, tokens, &index);
      if (expr)
        gsk_sl_preprocessor_push_conditional (preproc, GSK_COND_MATCH);
      else
        gsk_sl_preprocessor_push_conditional (preproc, GSK_COND_IGNORE);
    }
  else if (g_str_equal (token->str, "ifdef"))
    {
      if (tokens->len == 1)
        {
          token_array_error (preproc, tokens, 0, "No variable after #ifdef.");
        }
      else
        {
          token = token_array_get_token (tokens, 1);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
            {
              token_array_error (preproc, tokens, 1, "Expected identifier after #ifdef.");
            }
          else
            {
              if (g_hash_table_lookup (preproc->defines, token->str))
                {
                  gsk_sl_preprocessor_push_conditional (preproc, GSK_COND_MATCH);
                }
              else
                {
                  gsk_sl_preprocessor_push_conditional (preproc, GSK_COND_IGNORE);
                }
            }

          if (tokens->len > 2)
            token_array_error (preproc, tokens, 2, "Expected newline after #ifdef.");
        }
    }
  else if (g_str_equal (token->str, "ifndef"))
    {
      if (tokens->len == 1)
        {
          token_array_error (preproc, tokens, 0, "No variable after #ifdef.");
        }
      else
        {
          token = token_array_get_token (tokens, 1);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
            {
              token_array_error (preproc, tokens, 1, "Expected identifier after #ifndef.");
            }
          else
            {
              if (g_hash_table_lookup (preproc->defines, token->str))
                {
                  gsk_sl_preprocessor_push_conditional (preproc, GSK_COND_IGNORE);
                }
              else
                {
                  gsk_sl_preprocessor_push_conditional (preproc, GSK_COND_MATCH);
                }
            }

          if (tokens->len > 2)
            token_array_error (preproc, tokens, 2, "Expected newline after #ifndef.");
        }
    }
  else if (gsk_sl_preprocessor_in_ignored_conditional (preproc))
    {
      /* All checks above are for preprocessor directives that are checked even in
       * ignored parts of code */

      /* Everything below has no effect in ignored parts of the code */
    }
  else if (g_str_equal (token->str, "define"))
    {
      if (tokens->len == 1)
        {
          token_array_error (preproc, tokens, 0, "No variable after #define.");
        }
      else
        {
          token = token_array_get_token (tokens, 1);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
            {
              token_array_error (preproc, tokens, 1, "Expected identifier after #define.");
            }
          else
            {
              GskSlDefine *define;
              guint i;

              if (g_hash_table_lookup (preproc->defines, token->str))
                token_array_error (preproc, tokens, 1, "\"%s\" redefined.", token->str);
          
              define = gsk_sl_define_new (token->str, NULL);

              for (i = 2; i < tokens->len; i++)
                {
                  gsk_sl_define_add_token (define,
                                           token_array_get_location (tokens, i),
                                           token_array_get_token (tokens, i));
                }

              g_hash_table_replace (preproc->defines, (gpointer) gsk_sl_define_get_name (define), define);
            }
        }
    }
  else if (g_str_equal (token->str, "include"))
    {
      if (tokens->len == 1)
        {
          token_array_error (preproc, tokens, 0, "No filename after #include.");
        }
      else
        {
          token = token_array_get_token (tokens, 1);
          if (gsk_sl_token_is (token, GSK_SL_TOKEN_STRING))
            {
              gsk_sl_preprocessor_include (preproc, tokens, TRUE);
            }
          else
            {
              token_array_error (preproc, tokens, 1, "Expected filename after #include.");
            }
        }
    }
#if 0
  else if (g_str_equal (token->str, "line"))
    {
    }
  else if (g_str_equal (token->str, "pragma"))
    {
    }
  else if (g_str_equal (token->str, "error"))
    {
    }
  else if (g_str_equal (token->str, "extension"))
    {
    }
#endif
  else if (g_str_equal (token->str, "undef"))
    {
      if (tokens->len == 1)
        {
          token_array_error (preproc, tokens, 0, "No variable after #undef.");
        }
      else
        {
          token = token_array_get_token (tokens, 1);
          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
            {
              token_array_error (preproc, tokens, 1, "Expected identifier after #undef.");
            }
          else
            {
              g_hash_table_remove (preproc->defines, token->str);
            }
          
          if (tokens->len > 2)
            token_array_error (preproc, tokens, 2, "Expected newline after #undef.");
        }
    }
  else if (g_str_equal (token->str, "version"))
    {
      if (tokens->len == 1)
        {
          token_array_error (preproc, tokens, 0, "No version specified after #version.");
        }
      else
        {
          token = token_array_get_token (tokens, 1);

          if (!gsk_sl_token_is (token, GSK_SL_TOKEN_INTCONSTANT))
            {
              token_array_error (preproc, tokens, 1, "Expected version number.");
            }
          else
            {
              gint version = token->i32;

              if (tokens->len == 2)
                {
                  gsk_sl_preprocessor_handle_version (preproc, token_array_get_location (tokens, 1), version, NULL, first_token_ever);
                }
              else
                {
                  token = token_array_get_token (tokens, 2);
                  if (gsk_sl_token_is (token, GSK_SL_TOKEN_IDENTIFIER))
                    gsk_sl_preprocessor_handle_version (preproc, token_array_get_location (tokens, 1), version, token->str, first_token_ever);
                  else
                    token_array_error (preproc, tokens, 2, "Expected newline after #version.");
                }
            }
        }
    }
  else
    {
      token_array_error (preproc, tokens, 0, "Unknown preprocessor directive #%s.", token->str);
    }
  
out:
  g_array_free (tokens, TRUE);

  /* process first token, so we can ensure it assumes a newline */
  gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline);
  gsk_sl_preprocessor_handle_token (preproc, &pp, TRUE, FALSE);
}

static void
gsk_sl_preprocessor_handle_token (GskSlPreprocessor *preproc,
                                  GskSlPpToken      *pp,
                                  gboolean           was_newline,
                                  gboolean           start_of_document)
{
  if (gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_HASH))
    {
      if (!was_newline)
        {
          gsk_sl_preprocessor_error (preproc, SYNTAX, "Unexpected \"#\" - preprocessor directives must be at start of line.");
          gsk_sl_preprocessor_clear_token (pp);
        }
      else
        {
          gsk_sl_preprocessor_clear_token (pp);
          gsk_sl_preprocessor_handle_preprocessor_directive (preproc, start_of_document);
        }
    }
  else
    {
      gsk_sl_preprocessor_append_token (preproc, pp, NULL);
    }
}

GskSlPreprocessor *
gsk_sl_preprocessor_new (GskSlCompiler    *compiler,
                         GskSlEnvironment *environment,
                         GskCodeSource    *source)
{
  GskSlPreprocessor *preproc;
  GskSlPpToken pp;
  gboolean was_newline;

  preproc = g_slice_new0 (GskSlPreprocessor);

  preproc->ref_count = 1;
  preproc->compiler = g_object_ref (compiler);
  if (environment)
    preproc->environment = gsk_sl_environment_ref (environment);
  preproc->tokenizer = gsk_sl_tokenizer_new (source, 
                                             gsk_sl_preprocessor_error_func,
                                             preproc,
                                             NULL);
  preproc->tokens = g_array_new (FALSE, FALSE, sizeof (GskSlPpToken));
  g_array_set_clear_func (preproc->tokens, gsk_sl_preprocessor_clear_token);
  preproc->defines = gsk_sl_compiler_copy_defines (compiler);

  /* process the first token, so we can parse #version */
  gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline);
  gsk_sl_preprocessor_handle_token (preproc, &pp, TRUE, TRUE);

  return preproc;
}

static void
gsk_sl_preprocessor_ensure (GskSlPreprocessor *preproc)
{
  GskSlPpToken pp;
  gboolean was_newline = FALSE;

  while (preproc->tokens->len <= 0)
    {
      gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline);

      gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
    }
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
                             gpointer           consumer)
{
  gsk_sl_preprocessor_ensure (preproc);

  g_array_remove_index (preproc->tokens, 0);
}

void
gsk_sl_preprocessor_sync (GskSlPreprocessor *preproc,
                          GskSlTokenType     token_type)
{
  const GskSlToken *token;

  for (token = gsk_sl_preprocessor_get (preproc);
       !gsk_sl_token_is (token, GSK_SL_TOKEN_EOF) && !gsk_sl_token_is (token, token_type);
       token = gsk_sl_preprocessor_get (preproc))
    {
      if (gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_BRACE))
        {
          gsk_sl_preprocessor_consume (preproc, NULL);
          gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_RIGHT_BRACE);
        }
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_BRACKET))
        {
          gsk_sl_preprocessor_consume (preproc, NULL);
          gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_RIGHT_BRACKET);
        }
      else if (gsk_sl_token_is (token, GSK_SL_TOKEN_LEFT_PAREN))
        {
          gsk_sl_preprocessor_consume (preproc, NULL);
          gsk_sl_preprocessor_sync (preproc, GSK_SL_TOKEN_RIGHT_PAREN);
        }
      else
        {
          gsk_sl_preprocessor_consume (preproc, NULL);
        }
    }
}

void
gsk_sl_preprocessor_emit_error (GskSlPreprocessor     *preproc,
                                gboolean               fatal,
                                const GskCodeLocation *location,
                                const GError          *error)
{
  preproc->fatal_error |= fatal;

  g_printerr ("%s:%zu:%zu: %s: %s\n",
              gsk_code_source_get_name (location->source),
              location->lines + 1, location->line_bytes,
              fatal ? "error" : "warn",
              error->message);
}

