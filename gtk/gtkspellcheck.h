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

#ifndef __GTK_SPELL_CHECK_H__
#define __GTK_SPELL_CHECK_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

#define GTK_TYPE_SPELL_CHECKER (gtk_spell_checker_get_type())

typedef enum _GtkSpellDictionary
{
  GTK_SPELL_DICTIONARY_SESSION,
  GTK_SPELL_DICTIONARY_PERSONAL,
} GtkSpellDictionary;

GDK_AVAILABLE_IN_4_2
G_DECLARE_FINAL_TYPE (GtkSpellChecker, gtk_spell_checker, GTK, SPELL_CHECKER, GObject)

GDK_AVAILABLE_IN_4_2
GtkSpellChecker    *gtk_spell_checker_get_default       (void);
GDK_AVAILABLE_IN_4_2
GtkSpellChecker    *gtk_spell_checker_new_for_language  (const char         *language);
GDK_AVAILABLE_IN_4_2
GtkSpellChecker    *gtk_spell_checker_new_for_languages (const char * const *languages);
GDK_AVAILABLE_IN_4_2
const char * const *gtk_spell_checker_list_languages    (void);
GDK_AVAILABLE_IN_4_2
gboolean            gtk_spell_checker_contains_word     (GtkSpellChecker    *self,
                                                         const char         *word,
                                                         gssize              word_length);
GDK_AVAILABLE_IN_4_2
GListModel         *gtk_spell_checker_list_corrections  (GtkSpellChecker    *self,
                                                         const char         *word,
                                                         gssize              word_length);
GDK_AVAILABLE_IN_4_2
void                gtk_spell_checker_add_word          (GtkSpellChecker    *self,
                                                         GtkSpellDictionary  dictionary,
                                                         const char         *word,
                                                         gssize              word_length);
GDK_AVAILABLE_IN_4_2
void                gtk_spell_checker_set_correction    (GtkSpellChecker    *self,
                                                         GtkSpellDictionary  dictionary,
                                                         const char         *word,
                                                         gssize              word_length,
                                                         const char         *correction,
                                                         gssize              correction_length);

G_END_DECLS

#endif /* __GTK_SPELL_CHECK_H__ */
