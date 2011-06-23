/*
 * Copyright (C) 2011 Red Hat Inc.
 *
 * Author:
 *      Matthias Clasen <mclasen@redhat.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

typedef struct {
  gint count;
  gint position;
  gint length;
} SignalData;

static void
text_deleted (AtkText *atk_text, gint position, gint length, SignalData *data)
{
  data->count++;
  data->position = position;
  data->length = length;
}

static void
text_inserted (AtkText *atk_text, gint position, gint length, SignalData *data)
{
  data->count++;
  data->position = position;
  data->length = length;
}

static void
set_text (GtkWidget   *widget,
          const gchar *text)
{
  if (GTK_IS_LABEL (widget))
    gtk_label_set_text (GTK_LABEL (widget), text);
  else if (GTK_IS_ENTRY (widget))
    gtk_entry_set_text (GTK_ENTRY (widget), text);
  else if (GTK_IS_TEXT_VIEW (widget))
    gtk_text_buffer_set_text (gtk_text_view_get_buffer (GTK_TEXT_VIEW (widget)), text, -1);
  else
    g_assert_not_reached ();
}

static void
test_text_changed (GtkWidget *widget)
{
  AtkText *atk_text;
  const gchar *text = "Text goes here";
  SignalData delete_data;
  SignalData insert_data;

  atk_text = ATK_TEXT (gtk_widget_get_accessible (widget));

  delete_data.count = 0;
  insert_data.count = 0;

  g_signal_connect (atk_text, "text_changed::delete",
                    G_CALLBACK (text_deleted), &delete_data);
  g_signal_connect (atk_text, "text_changed::insert",
                    G_CALLBACK (text_inserted), &insert_data);

  set_text (widget, text);

  g_assert_cmpint (delete_data.count, >, 0);
  g_assert_cmpint (delete_data.position, ==, 0);
  g_assert_cmpint (delete_data.length, ==, -1);

  g_assert_cmpint (insert_data.count, >, 0);
  g_assert_cmpint (insert_data.position, ==, 0);
  g_assert_cmpint (insert_data.length, ==, -1);

  g_signal_handlers_disconnect_by_func (atk_text, G_CALLBACK (text_deleted), &delete_data);
  g_signal_handlers_disconnect_by_func (atk_text, G_CALLBACK (text_inserted), &insert_data);
}

typedef struct {
  gint offset;
  AtkTextBoundary boundary;
  const gchar *word;
  gint start;
  gint end;
} Word;

static void
test_words (GtkWidget *widget)
{
  AtkText *atk_text;
  const gchar *text = "This is a medium-size test string, including some \303\204\303\226\303\234 and 123 for good measure.";
  const gchar *expected_words[] = {
    "This ",
    "is ",
    "a ",
    "medium-",
    "size ",
    "test ",
    "string, ",
    "including ",
    "some ",
    "\303\204\303\226\303\234 ",
    "and ",
    "123 ",
    "for ",
    "good ",
    "measure.",
    NULL
  };
  gint start, end;
  gchar *word;
  gchar *last_word;
  gint offset;
  gint i;

  atk_text = ATK_TEXT (gtk_widget_get_accessible (widget));

  set_text (widget, text);

  last_word = NULL;
  i = 0;
  for (offset = 0; offset < g_utf8_strlen (text, -1); offset++)
    {
      word = atk_text_get_text_at_offset (atk_text,
                                          offset,
                                          ATK_TEXT_BOUNDARY_WORD_START,
                                          &start, &end);
      if (g_strcmp0 (last_word, word) != 0)
        {
          g_assert_cmpstr (word, ==, expected_words[i]);
          g_free (last_word);
          last_word = word;
          i++;
        }
    }
  g_free (last_word);
  g_assert (expected_words[i] == 0);
}

static void
setup_test (GtkWidget *widget)
{
  set_text (widget, "");
}

static void
add_text_test (const gchar      *prefix,
               GTestFixtureFunc  test_func,
               GtkWidget        *widget)
{
  gchar *path;

  path = g_strdup_printf ("%s/%s", prefix, G_OBJECT_TYPE_NAME (widget));
  g_test_add_vtable (path,
                     0,
                     g_object_ref (widget),
                     (GTestFixtureFunc) setup_test,
                     (GTestFixtureFunc) test_func,
                     (GTestFixtureFunc) g_object_unref);
  g_free (path);
}

static void
add_text_tests (GtkWidget *widget)
{
  g_object_ref_sink (widget);
  add_text_test ("/text/words", (GTestFixtureFunc) test_words, widget);
  add_text_test ("/text/changed", (GTestFixtureFunc) test_text_changed, widget);
  g_object_unref (widget);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  add_text_tests (gtk_text_view_new ());
  add_text_tests (gtk_entry_new ());
  add_text_tests (gtk_label_new (""));

  return g_test_run ();
}
