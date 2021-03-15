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

#ifndef __GTK_SPELL_CHECK_PRIVATE_H__
#define __GTK_SPELL_CHECK_PRIVATE_H__

#include "gtkspellcheck.h"

G_BEGIN_DECLS

typedef struct _GtkSpellProvider GtkSpellProvider;
typedef struct _GtkSpellLanguage GtkSpellLanguage;

struct _GtkSpellProvider
{
  const char    *name;
  gboolean     (*supports)         (const char       *code);
  char       **(*list_languages)   (void);
  GListModel  *(*list_corrections) (GtkSpellLanguage *language,
                                    const char       *word,
                                    gssize            word_length);
  gboolean     (*init_language)    (GtkSpellLanguage *language);
  void         (*fini_language)    (GtkSpellLanguage *language);
  gboolean     (*contains_word)    (GtkSpellLanguage *language,
                                    const char       *word,
                                    gssize            word_length);
};

struct _GtkSpellLanguage
{
  const GtkSpellProvider *provider;
  char *code;
  gpointer native;
};

struct _GtkSpellChecker
{
  GObject    parent_instance;
  GPtrArray *languages;
};

struct _GtkSpellCorrection
{
  GObject parent_instance;
  char *text;
};

static inline gboolean
_gtk_spell_language_contains_word (GtkSpellLanguage *language,
                                   const char       *word,
                                   gssize            word_length)
{
  return language->provider->contains_word (language, word, word_length);
}

static inline gboolean
_gtk_spell_checker_contains_word (GtkSpellChecker *checker,
                                  const char      *word,
                                  gssize           word_length)
{
  if (word_length < 0)
    word_length = strlen (word);

  for (guint i = 0; i < checker->languages->len; i++)
    {
      GtkSpellLanguage *language = g_ptr_array_index (checker->languages, i);

      if (_gtk_spell_language_contains_word (language, word, word_length))
        return TRUE;
    }

  return FALSE;
}

G_END_DECLS

#endif /* __GTK_SPELL_CHECKER_PRIVATE_H__ */
