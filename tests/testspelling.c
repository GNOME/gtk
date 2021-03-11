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

#include <gtk/gtk.h>

int
main (int argc, char *argv[])
{
  GtkSpellChecker *checker;

  gtk_init ();

  checker = gtk_spell_checker_get_default ();

  for (guint i = 1; i < argc; i++)
    {
      const char *word = argv[i];
      GListModel *corrections;
      guint n_items = 0;

      if (gtk_spell_checker_contains_word (checker, word, -1))
        {
          g_print ("Dictionary contains the word “%s”\n", word);
          continue;
        }

      if ((corrections = gtk_spell_checker_list_corrections (checker, word, -1)))
        {
          n_items = g_list_model_get_n_items (G_LIST_MODEL (corrections));

          if (n_items > 0)
            g_print ("Corrections for “%s”:\n", word);

          for (guint j = 0; j < n_items; j++)
            {
              GtkStringObject *correction = g_list_model_get_item (corrections, j);
              const char *text = gtk_string_object_get_string (correction);

              g_print ("  %s\n", text);
            }

          g_object_unref (corrections);
        }

      if (n_items == 0)
        g_print ("No corrections for “%s” were found.\n", word);
    }

  g_assert_finalize_object (checker);

  return 0;
}
