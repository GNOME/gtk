/*
 * Copyright (C) 2022, Red Hat, Inc.
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

#include <locale.h>

#include <gtk/gtk.h>

static GQuark changes_quark;

static void
items_changed_cb (GListModel *model,
                  guint       position,
                  guint       removed,
                  guint       added)
{
  GString *changes;

  g_assert_true (removed != 0 || added != 0);

  changes = g_object_get_qdata (G_OBJECT (model), changes_quark);

  if (changes->len)
    g_string_append (changes, ", ");

  if (removed == 1 && added == 0)
    {
      g_string_append_printf (changes, "-%u", position);
    }
  else if (removed == 0 && added == 1)
    {
      g_string_append_printf (changes, "+%u", position);
    }
  else
    {
      g_string_append_printf (changes, "%u", position);
      if (removed > 0)
        g_string_append_printf (changes, "-%u", removed);
      if (added > 0)
        g_string_append_printf (changes, "+%u", added);
    }
}

#define assert_changes(model, expected) G_STMT_START{ \
  GString *changes = g_object_get_qdata (G_OBJECT (model), changes_quark); \
  if (!g_str_equal (changes->str, expected)) \
     g_assertion_message_cmpstr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
         #model " == " #expected, changes->str, "==", expected); \
  g_string_set_size (changes, 0); \
}G_STMT_END

static void
free_changes (gpointer data)
{
  GString *s = data;

  g_string_free (s, TRUE);
}

static void
test_list_list_model (void)
{
  GtkWidget *box;
  GListModel *model;
  GtkWidget *a, *b;
  GObject *item;
  guint n_items;

  box = g_object_ref_sink (gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));

  model = gtk_widget_observe_children (box);
  g_assert_true (g_list_model_get_item_type (model) == G_TYPE_OBJECT);

  g_assert_true (g_list_model_get_n_items (model) == 0);
  g_assert_true (g_list_model_get_item (model, 0) == NULL);

  g_object_set_qdata_full (G_OBJECT (model), changes_quark, g_string_new (""), free_changes);

  g_signal_connect (model, "items-changed", G_CALLBACK (items_changed_cb), NULL);
  a = gtk_label_new ("a");
  b = gtk_label_new ("b");
  gtk_box_append (GTK_BOX (box), a);
  gtk_box_append (GTK_BOX (box), b);

  item = g_list_model_get_item (model, 0);
  g_assert_true (GTK_WIDGET (item) == a);
  g_object_unref (item);
  item = g_list_model_get_item (model, 1);
  g_assert_true (GTK_WIDGET (item) == b);
  g_object_unref (item);

  assert_changes (model, "+0, +1");

  g_object_get (model, "n-items", &n_items, NULL);
  g_assert_true (n_items == 2);

  g_object_unref (model);
  g_object_unref (box);
}

int
main (int argc, char *argv[])
{
  (g_test_init) (&argc, &argv, NULL);
  gtk_init ();
  setlocale (LC_ALL, "C");

  changes_quark = g_quark_from_static_string ("What did I see? Can I believe what I saw?");

  g_test_add_func ("/listlistmodel/change", test_list_list_model);

  return g_test_run ();
}
