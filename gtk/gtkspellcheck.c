/*
 * Copyright 2021 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#ifdef HAVE_ENCHANT
# include <enchant.h>
#endif

#include "gtkflattenlistmodel.h"
#include "gtkspellcheckprivate.h"
#include "gtkstringlist.h"

#ifdef HAVE_ENCHANT
static char       **gtk_enchant_list_languages   (void);
static gboolean     gtk_enchant_supports         (const char       *code);
static gboolean     gtk_enchant_contains_word    (GtkSpellLanguage *language,
                                                  const char       *word,
                                                  gssize            word_length);
static gboolean     gtk_enchant_init_language    (GtkSpellLanguage *language);
static void         gtk_enchant_fini_language    (GtkSpellLanguage *language);
static GListModel  *gtk_enchant_list_corrections (GtkSpellLanguage *language,
                                                  const char       *word,
                                                  gssize            word_length);
#endif

G_DEFINE_TYPE (GtkSpellChecker, gtk_spell_checker, G_TYPE_OBJECT)

enum {
  PROP_0,
  PROP_LANGUAGES,
  N_PROPS
};

static GParamSpec *properties [N_PROPS];
static const GtkSpellProvider providers[] = {
#ifdef HAVE_ENCHANT
  {
    .name             = "enchant",
    .supports         = gtk_enchant_supports,
    .contains_word    = gtk_enchant_contains_word,
    .list_languages   = gtk_enchant_list_languages,
    .init_language    = gtk_enchant_init_language,
    .fini_language    = gtk_enchant_fini_language,
    .list_corrections = gtk_enchant_list_corrections,
  },
#endif
};

static gboolean
gtk_spell_provider_supports (const GtkSpellProvider *provider,
                             const char             *code)
{
  char **codes;
  gboolean ret = FALSE;

  if (provider->supports != NULL)
    return provider->supports (code);

  /* Fallback based on listing languages */
  if ((codes = provider->list_languages ()))
    {
      for (guint i = 0; codes[i]; i++)
        {
          if (g_strcmp0 (codes[i], code) == 0)
            {
              ret = TRUE;
              break;
            }
        }
    }

  g_strfreev (codes);

  return ret;
}

static void
_gtk_source_language_free (GtkSpellLanguage *language)
{
  if (language->provider->fini_language)
    language->provider->fini_language (language);

  g_clear_pointer (&language->code, g_free);

  language->provider = NULL;
  language->native = NULL;

  g_free (language);
}

static GtkSpellLanguage *
_gtk_spell_language_new (const GtkSpellProvider *provider,
                         const char             *code)
{
  GtkSpellLanguage *language;

  g_assert (provider != NULL);
  g_assert (code != NULL);

  language = g_new0 (GtkSpellLanguage, 1);
  language->provider = provider;
  language->code = g_strdup (code);

  if (provider->init_language != NULL)
    {
      if (!provider->init_language (language))
        {
          g_free (language->code);
          g_free (language);
          return NULL;
        }
    }

  return language;
}

static char **
gtk_spell_checker_get_languages (GtkSpellChecker *self)
{
  GArray *ar = g_array_new (TRUE, FALSE, sizeof (char *));

  for (guint i = 0; i < self->languages->len; i++)
    {
      GtkSpellLanguage *language = g_ptr_array_index (self->languages, i);
      char *code = g_strdup (language->code);
      g_array_append_val (ar, code);
    }

  return (char **)(gpointer)g_array_free (ar, FALSE);
}

static gboolean
gtk_spell_checker_contains_language (GtkSpellChecker  *self,
                                     GtkSpellLanguage *language)
{
  g_assert (GTK_IS_SPELL_CHECKER (self));
  g_assert (language != NULL);
  g_assert (language->code != NULL);
  g_assert (language->provider != NULL);

  for (guint i = 0; i < self->languages->len; i++)
    {
      GtkSpellLanguage *element = g_ptr_array_index (self->languages, i);

      if (g_strcmp0 (language->code, element->code) == 0)
        return TRUE;
    }

  return FALSE;
}

static void
gtk_spell_checker_set_languages (GtkSpellChecker    *self,
                                 const char * const *languages)
{
  for (guint i = 0; languages[i]; i++)
    {
      const char *code = languages[i];

      for (guint j = 0; j < G_N_ELEMENTS (providers); j++)
        {
          const GtkSpellProvider *provider = &providers[j];

          if (gtk_spell_provider_supports (provider, code))
            {
              GtkSpellLanguage *language = _gtk_spell_language_new (provider, code);

              if (language == NULL)
                continue;

              if (!gtk_spell_checker_contains_language (self, language))
                g_ptr_array_add (self->languages, language);

              break;
            }
        }
    }
}

static void
gtk_spell_checker_finalize (GObject *object)
{
  GtkSpellChecker *self = (GtkSpellChecker *)object;

  g_clear_pointer (&self->languages, g_ptr_array_unref);

  G_OBJECT_CLASS (gtk_spell_checker_parent_class)->finalize (object);
}

static void
gtk_spell_checker_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  GtkSpellChecker *self = GTK_SPELL_CHECKER (object);

  switch (prop_id)
    {
    case PROP_LANGUAGES:
      g_value_take_boxed (value, gtk_spell_checker_get_languages (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_spell_checker_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  GtkSpellChecker *self = GTK_SPELL_CHECKER (object);

  switch (prop_id)
    {
    case PROP_LANGUAGES:
      gtk_spell_checker_set_languages (self, g_value_get_boxed (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
gtk_spell_checker_class_init (GtkSpellCheckerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gtk_spell_checker_finalize;
  object_class->get_property = gtk_spell_checker_get_property;
  object_class->set_property = gtk_spell_checker_set_property;

  properties [PROP_LANGUAGES] =
    g_param_spec_boxed ("languages",
                        "Languages",
                        "The language codes to support",
                        G_TYPE_STRV,
                        (G_PARAM_READWRITE |
                         G_PARAM_CONSTRUCT_ONLY |
                         G_PARAM_STATIC_STRINGS));

  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
gtk_spell_checker_init (GtkSpellChecker *self)
{
  self->languages = g_ptr_array_new_with_free_func ((GDestroyNotify)_gtk_source_language_free);
}

/**
 * gtk_spell_checker_new_for_language:
 * @language: (nullable): the language to support
 *
 * Creates a new #GtkSpellChecker which uses a dictionary available based
 * on @language.
 *
 * Returns: (transfer full): a #GtkSpellChecker
 *
 * Since: 4.2
 */
GtkSpellChecker *
gtk_spell_checker_new_for_language (const char *language)
{
  const char *languages[] = { language, NULL };

  return gtk_spell_checker_new_for_languages (languages);
}

/**
 * gtk_spell_checker_new_for_languages:
 * @languages: (nullable) (element-type utf8): the language codes to support
 *
 * Creates a new #GtkSpellChecker which uses dictionaries available based
 * on @languages.
 *
 * Returns: (transfer full): a #GtkSpellChecker
 *
 * Since: 4.2
 */
GtkSpellChecker *
gtk_spell_checker_new_for_languages (const char * const * languages)
{
  GtkSpellChecker *ret;

  ret = g_object_new (GTK_TYPE_SPELL_CHECKER,
                      "languages", languages,
                      NULL);

  if (ret != NULL && ret->languages->len == 0)
    g_clear_object (&ret);

  return g_steal_pointer (&ret);
}

const char * const *
gtk_spell_checker_list_languages (void)
{
  static char **languages;

  if (languages == NULL)
    {
      GHashTable *seen = g_hash_table_new (g_str_hash, g_str_equal);
      GArray *ar = g_array_new (TRUE, FALSE, sizeof (char *));

      for (guint i = 0; i < G_N_ELEMENTS (providers); i++)
        {
          const GtkSpellProvider *provider = &providers[i];
          char **found = provider->list_languages ();

          if (found == NULL)
            continue;

          for (guint j = 0; found[j]; j++)
            {
              if (!g_hash_table_contains (seen, found[j]))
                {
                  char *copy = g_strdup (found[j]);
                  g_array_append_val (ar, copy);
                  g_hash_table_add (seen, copy);
                }
            }

          g_strfreev (found);
        }

      languages = (char **)(gpointer)g_array_free (ar, FALSE);
      g_hash_table_unref (seen);
    }

  return (const char * const *)languages;
}

gboolean
gtk_spell_checker_contains_word (GtkSpellChecker *self,
                                 const char      *word,
                                 gssize           word_length)
{
  g_return_val_if_fail (GTK_IS_SPELL_CHECKER (self), FALSE);

  return _gtk_spell_checker_contains_word (self, word, word_length);
}

GListModel *
gtk_spell_checker_list_corrections (GtkSpellChecker *self,
                                    const char      *word,
                                    gssize           word_length)
{
  GtkFlattenListModel *ret;
  GListStore *store;

  g_return_val_if_fail (GTK_IS_SPELL_CHECKER (self), NULL);
  g_return_val_if_fail (word != NULL, NULL);

  if (word_length < 0)
    word_length = strlen (word);

  if (self->languages->len == 0)
    return G_LIST_MODEL (gtk_string_list_new (NULL));

  if (self->languages->len == 1)
    {
      GtkSpellLanguage *language = g_ptr_array_index (self->languages, 0);
      return language->provider->list_corrections (language, word, word_length);
    }

  store = g_list_store_new (G_TYPE_LIST_MODEL);

  for (guint i = 0; i < self->languages->len; i++)
    {
      GtkSpellLanguage *language = g_ptr_array_index (self->languages, i);
      GListModel *model = language->provider->list_corrections (language, word, word_length);

      if (model != NULL)
        {
          g_list_store_append (store, model);
          g_object_unref (model);
        }
    }

  ret = gtk_flatten_list_model_new (G_LIST_MODEL (store));

  return G_LIST_MODEL (ret);
}

GtkSpellChecker *
gtk_spell_checker_get_default (void)
{
  static GtkSpellChecker *instance;

  if (instance == NULL)
    {
      const char * const *langs = g_get_language_names ();

      if (langs != NULL)
        instance = gtk_spell_checker_new_for_languages (langs);

      if (instance == NULL)
        instance = gtk_spell_checker_new_for_language ("en_US");

      if (instance == NULL)
        instance = gtk_spell_checker_new_for_language ("C");

      /* TODO: We might want to have a fallback so that a real object
       *       is always returned from this method.
       */

      if (instance != NULL)
        g_object_add_weak_pointer (G_OBJECT (instance), (gpointer *)&instance);
    }

  return instance;
}

#ifdef HAVE_ENCHANT
static EnchantBroker *
gtk_enchant_get_broker (void)
{
  static EnchantBroker *broker;

  if (broker == NULL)
    broker = enchant_broker_init ();

  return broker;
}

static gboolean
gtk_enchant_supports (const char *code)
{
  return enchant_broker_dict_exists (gtk_enchant_get_broker (), code);
}

static void
gtk_enchant_list_languages_cb (const char * const  lang_tag,
                               const char * const  provider_name,
                               const char * const  provider_desc,
                               const char * const  provider_file,
                               void               *user_data)
{
  GArray *ar = user_data;
  char *code = g_strdup (lang_tag);
  g_array_append_val (ar, code);
}

static char **
gtk_enchant_list_languages (void)
{
  EnchantBroker *broker = gtk_enchant_get_broker ();
  GArray *ar = g_array_new (TRUE, FALSE, sizeof (char *));

  enchant_broker_list_dicts (broker,
                             gtk_enchant_list_languages_cb,
                             ar);

  return (char **)(gpointer)g_array_free (ar, FALSE);
}

static gboolean
gtk_enchant_contains_word (GtkSpellLanguage *language,
                           const char       *word,
                           gssize            word_length)
{
  return enchant_dict_check (language->native, word, word_length) == 0;
}

static GListModel *
gtk_enchant_list_corrections (GtkSpellLanguage *language,
                              const char       *word,
                              gssize            word_length)
{
  size_t count = 0;
  char **ret = enchant_dict_suggest (language->native, word, word_length, &count);
  GtkStringList *model = gtk_string_list_new ((const char * const *)ret);
  enchant_dict_free_string_list (language->native, ret);
  return G_LIST_MODEL (model);
}

static void
gtk_enchant_init_language_cb (const char * const  code,
                              const char * const  provider_name,
                              const char * const  provider_desc,
                              const char * const  provider_file,
                              void               *user_data)
{
  GtkSpellLanguage *language = user_data;

  g_assert (language != NULL);
  g_assert (code != NULL);

  /* Replace the language code so we can deduplicate based on what
   * dictionary was actually loaded. Otherwise we could end up with
   * en_US and en_US.UTF-8 as two separate dictionaries.
   */
  g_free (language->code);
  language->code = g_strdup (code);
}

static gboolean
gtk_enchant_init_language (GtkSpellLanguage *language)
{
  EnchantBroker *broker = gtk_enchant_get_broker ();

  if (!(language->native = enchant_broker_request_dict (broker, language->code)))
    return FALSE;

  enchant_dict_describe (language->native,
                         gtk_enchant_init_language_cb,
                         language);

  return TRUE;
}

static void
gtk_enchant_fini_language (GtkSpellLanguage *language)
{
  if (language->native != NULL)
    {
      enchant_broker_free_dict (gtk_enchant_get_broker (), language->native);
      language->native = NULL;
    }
}
#endif
