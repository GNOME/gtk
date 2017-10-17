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
                             GskSlPpToken      *pp,
                             gboolean           include_local)
{
  GskCodeSource *source;
  GError *error = NULL;
  gboolean was_newline;
    
  source = gsk_sl_compiler_resolve_include (preproc->compiler,
                                            gsk_sl_tokenizer_get_location (preproc->tokenizer)->source,
                                            include_local,
                                            pp->token.str,
                                            &error);
  if (source == NULL)
    {
      gsk_sl_preprocessor_emit_error (preproc, TRUE, &pp->location, error);
      gsk_sl_preprocessor_clear_token (pp);
      g_error_free (error);
      return FALSE;
    }

  if (g_slist_length (preproc->pending_tokenizers) > 20)
    {
      gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp->location, "#include nested too deeply.");
      gsk_sl_preprocessor_clear_token (pp);
      return FALSE;
    }

  gsk_sl_preprocessor_clear_token (pp);
  pp->location = *gsk_sl_tokenizer_get_location (preproc->tokenizer);
  gsk_sl_tokenizer_read_token (preproc->tokenizer, &pp->token);
  if (!gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_NEWLINE) &&
      !gsk_sl_token_is (&pp->token, GSK_SL_TOKEN_EOF))
    {
      gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp->location, "Extra content after #include directive");
      gsk_sl_preprocessor_clear_token (pp);
      return FALSE;
    }

  gsk_sl_preprocessor_clear_token (pp);
  preproc->pending_tokenizers = g_slist_prepend (preproc->pending_tokenizers, preproc->tokenizer);
  preproc->tokenizer = gsk_sl_tokenizer_new (source, 
                                             gsk_sl_preprocessor_error_func,
                                             preproc,
                                             NULL);

  /* process first token, so we can ensure it assumes a newline */
  gsk_sl_preprocessor_next_token (preproc, pp, &was_newline);
  gsk_sl_preprocessor_handle_token (preproc, pp, TRUE, FALSE);

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

static void
gsk_sl_preprocessor_handle_preprocessor_directive (GskSlPreprocessor *preproc,
                                                   gboolean           first_token_ever)
{
  GskSlPpToken pp;
  gboolean was_newline;
  
  if (gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
    {
      /* empty # line */
      gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
      return;
    }

  if (gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_IDENTIFIER))
    {
      if (g_str_equal (pp.token.str, "else"))
        {
          if (gsk_sl_preprocessor_has_conditional (preproc))
            {
              GskConditional cond = gsk_sl_preprocessor_pop_conditional (preproc);

              if (cond & GSK_COND_ELSE)
                {
                  gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "#else after #else.");
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
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "#else without #if.");
            }
          gsk_sl_preprocessor_clear_token (&pp);

          if (!gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected newline after #else.");
              gsk_sl_preprocessor_clear_token (&pp);
            }
          else
            {
              gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
            }
          return;
        }
#if 0
      else if (g_str_equal (pp.token.str, "elif"))
        {
        }
#endif
      else if (g_str_equal (pp.token.str, "endif"))
        {
          if (gsk_sl_preprocessor_has_conditional (preproc))
            {
              gsk_sl_preprocessor_pop_conditional (preproc);
            }
          else
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "#endif without #if.");
            }
          gsk_sl_preprocessor_clear_token (&pp);

          if (!gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected newline after #endif.");
              gsk_sl_preprocessor_clear_token (&pp);
            }
          else
            {
              gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
            }
          return;
        }
#if 0
      else if (g_str_equal (pp.token.str, "if"))
        {
        }
#endif
      else if (g_str_equal (pp.token.str, "ifdef"))
        {
          gsk_sl_preprocessor_clear_token (&pp);
          if (gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "No variable after #ifdef.");
              gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
              return;
            }
          if (!gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_IDENTIFIER))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected identifier after #ifdef.");
              gsk_sl_preprocessor_clear_token (&pp);
            }
          else
            {
              if (g_hash_table_lookup (preproc->defines, pp.token.str))
                {
                  gsk_sl_preprocessor_push_conditional (preproc, GSK_COND_MATCH);
                }
              else
                {
                  gsk_sl_preprocessor_push_conditional (preproc, GSK_COND_IGNORE);
                }
              
              if (!gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
                {
                  gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected newline after #ifdef.");
                  gsk_sl_preprocessor_clear_token (&pp);
                }
              else
                {
                  gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
                }
              return;
            }
        }
      else if (g_str_equal (pp.token.str, "ifndef"))
        {
          gsk_sl_preprocessor_clear_token (&pp);
          if (gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "No variable after #ifndef.");
              gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
              return;
            }
          if (!gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_IDENTIFIER))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected identifier after #ifndef.");
              gsk_sl_preprocessor_clear_token (&pp);
            }
          else
            {
              if (g_hash_table_lookup (preproc->defines, pp.token.str))
                {
                  gsk_sl_preprocessor_push_conditional (preproc, GSK_COND_IGNORE);
                }
              else
                {
                  gsk_sl_preprocessor_push_conditional (preproc, GSK_COND_MATCH);
                }
              
              if (!gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
                {
                  gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected newline after #ifndef.");
                  gsk_sl_preprocessor_clear_token (&pp);
                }
              else
                {
                  gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
                }
              return;
            }
        }
      else if (gsk_sl_preprocessor_in_ignored_conditional (preproc))
        {
          /* All checks above are for preprocessor directives that are checked even in
           * ignored parts of code */
          gsk_sl_preprocessor_clear_token (&pp);
          /* Everything below has no effect in ignored parts of the code */
        }
      else if (g_str_equal (pp.token.str, "define"))
        {
          gsk_sl_preprocessor_clear_token (&pp);
          if (gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "No variable after #define.");
              gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
              return;
            }
          if (!gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_IDENTIFIER))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected identifier after #define.");
              gsk_sl_preprocessor_clear_token (&pp);
            }
          else
            {
              GskSlDefine *define;

              if (g_hash_table_lookup (preproc->defines, pp.token.str))
                gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "\"%s\" redefined.", pp.token.str);
              
              define = gsk_sl_define_new (pp.token.str, NULL);
              gsk_sl_preprocessor_clear_token (&pp);

              while (!gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
                {
                  gsk_sl_define_add_token (define, &pp.location, &pp.token);
                  gsk_sl_preprocessor_clear_token (&pp);
                }
              g_hash_table_replace (preproc->defines, (gpointer) gsk_sl_define_get_name (define), define);
              gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
              return;
            }
        }
      else if (g_str_equal (pp.token.str, "include"))
        {
          gsk_sl_preprocessor_clear_token (&pp);
          if (gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "No filename after #include.");
              gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
              return;
            }
          if (gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_STRING))
            {
              if (gsk_sl_preprocessor_include (preproc, &pp, TRUE))
                return;
            }
          else
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected filename after #include.");
              gsk_sl_preprocessor_clear_token (&pp);
            }
        }
#if 0
      else if (g_str_equal (pp.token.str, "line"))
        {
        }
      else if (g_str_equal (pp.token.str, "pragma"))
        {
        }
      else if (g_str_equal (pp.token.str, "error"))
        {
        }
      else if (g_str_equal (pp.token.str, "extension"))
        {
        }
#endif
      else if (g_str_equal (pp.token.str, "undef"))
        {
          gsk_sl_preprocessor_clear_token (&pp);
          if (gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "No variable after #undef.");
              gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
              return;
            }
          if (!gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_IDENTIFIER))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected identifier after #undef.");
              gsk_sl_preprocessor_clear_token (&pp);
            }
          else
            {
              g_hash_table_remove (preproc->defines, pp.token.str);
              
              if (!gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
                {
                  gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected newline after #undef.");
                  gsk_sl_preprocessor_clear_token (&pp);
                }
              else
                {
                  gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
                }
              return;
            }
        }
      else if (g_str_equal (pp.token.str, "version"))
        {
          gsk_sl_preprocessor_clear_token (&pp);
          if (gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "No version specified after #version.");
              gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
              return;
            }
          if (!gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_INTCONSTANT))
            {
              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected version number.");
              gsk_sl_preprocessor_clear_token (&pp);
            }
          else
            {
              GskCodeLocation location = pp.location;
              gint version = pp.token.i32;

              gsk_sl_preprocessor_clear_token (&pp);
              if (gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
                {
                  gsk_sl_preprocessor_handle_version (preproc, &location, version, NULL, first_token_ever);
                  gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
                  return;
                }
              else if (gsk_sl_token_is (&pp.token, GSK_SL_TOKEN_IDENTIFIER))
                {
                  gsk_sl_preprocessor_handle_version (preproc, &pp.location, version, pp.token.str, first_token_ever);
                  gsk_sl_preprocessor_clear_token (&pp);
                  if (gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
                    {
                      gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
                      return;
                    }
                }

              gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Expected newline #version.");
              gsk_sl_preprocessor_clear_token (&pp);
            }
        }
      else
        {
          gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Unknown preprocessor directive #%s.", pp.token.str);
          gsk_sl_preprocessor_clear_token (&pp);
        }
    }
  else
    {
      gsk_sl_preprocessor_error_full (preproc, PREPROCESSOR, &pp.location, "Missing identifier for preprocessor directive.");
      gsk_sl_preprocessor_clear_token (&pp);
    }
  
  while (!gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline))
    gsk_sl_preprocessor_clear_token (&pp);

  gsk_sl_preprocessor_handle_token (preproc, &pp, was_newline, FALSE);
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
  pp.location = *gsk_sl_tokenizer_get_location (preproc->tokenizer);
  gsk_sl_tokenizer_read_token (preproc->tokenizer, &pp.token);
  if (!gsk_sl_token_is_skipped (&pp.token))
    gsk_sl_preprocessor_handle_token (preproc, &pp, TRUE, TRUE);
  else
    {
      gboolean was_newline;
      gsk_sl_preprocessor_next_token (preproc, &pp, &was_newline);
      gsk_sl_preprocessor_handle_token (preproc, &pp, TRUE, FALSE);
    }

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

