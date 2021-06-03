/* state-saving.c
 * Copyright (C) 2021 Red Hat, Inc
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>


static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

static void
save_cb (GtkWidget *window)
{
  GVariant *state;
  char *s;
  GError *error = NULL;

  state = gtk_widget_save_state (window);
  if (!state)
    {
      g_print ("no state\n");
      return;
    }

  s = g_variant_print (state, TRUE);
  g_print ("%s\n", s);
  g_free (s);

  if (!g_file_set_contents ("saved-state",
                            g_variant_get_data (state),
                            g_variant_get_size (state),
                            &error))
    {
      g_error ("Failed to save state: %s", error->message);
      g_error_free (error);
    }

  g_variant_unref (state);
}

static void
restore_cb (GtkWidget *window)
{
  char *contents;
  gsize len;
  GError *error = NULL;
  GVariant *state;

  if (!g_file_get_contents ("saved-state", &contents, &len, &error))
    {
      g_print ("Error loading state: %s\n", error->message);
      g_error_free (error);
      return;
    }

  state = g_variant_new_from_data (G_VARIANT_TYPE_VARDICT, contents, len, FALSE, NULL, NULL);
  gtk_widget_restore_state (window, state);
  g_variant_unref (state);
}

static void
add_to_list (GtkWidget *list)
{
  GtkWidget *entry;
  int i;
  char *s;
  GtkWidget *w;

  entry = gtk_entry_new ();

  gtk_widget_set_save_id (entry, "entry");
  gtk_list_box_append (GTK_LIST_BOX (list), entry);

  for (i = 0, w = gtk_widget_get_first_child (list);
       w;
       w = gtk_widget_get_next_sibling (w)) i++;

  s = g_strdup_printf ("item%d", i);
  gtk_widget_set_save_id (gtk_widget_get_parent (entry), s);
  g_free (s);
}

static gboolean
save_list (GtkWidget    *list,
           GVariantDict *dict,
           gboolean     *save_children,
           gpointer      data)
{
  int n_items;
  GtkWidget *w;

  for (n_items = 0, w = gtk_widget_get_first_child (list);
       w;
       w = gtk_widget_get_next_sibling (w)) n_items++;

  g_variant_dict_insert (dict, "n-items", "i", n_items);
  *save_children =  TRUE;

  return FALSE;
}

static gboolean
restore_list (GtkWidget *list,
              GVariant  *data)
{
  int n_items;

  if (g_variant_lookup (data, "n-items", "i", &n_items))
    {
      for (int i = 0; i < n_items; i++)
        add_to_list (list);
    }

  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *box, *hbox, *button, *scale;
  GtkWidget *entry, *cc, *sw, *stack, *switcher;
  GtkWidget *list;
  gboolean done = FALSE;

  gtk_init ();

  window = gtk_window_new ();
  gtk_widget_set_save_id (window, "window");

  gtk_window_set_title (GTK_WINDOW (window), "hello world");
  gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_save_id (box, "box");

  stack = gtk_stack_new ();
  gtk_widget_set_save_id (stack, "stack");
  switcher = gtk_stack_switcher_new ();
  gtk_stack_switcher_set_stack (GTK_STACK_SWITCHER (switcher), GTK_STACK (stack));

  gtk_box_append (GTK_BOX (box), switcher);
  gtk_box_append (GTK_BOX (box), stack);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_append (GTK_BOX (box), hbox);

  button = gtk_button_new_with_label ("Save");
  gtk_widget_set_hexpand (button, TRUE);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (save_cb), window);
  gtk_box_append (GTK_BOX (hbox), button);

  button = gtk_button_new_with_label ("Restore");
  gtk_widget_set_hexpand (button, TRUE);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (restore_cb), window);
  gtk_box_append (GTK_BOX (hbox), button);

  gtk_window_set_child (GTK_WINDOW (window), box);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_save_id (box, "box");

  scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  gtk_widget_set_save_id (scale, "scale");
  gtk_box_append (GTK_BOX (box), scale);

  entry = gtk_entry_new ();
  gtk_widget_set_save_id (entry, "entry");
  gtk_box_append (GTK_BOX (box), entry);

  cc = gtk_color_chooser_widget_new ();
  gtk_widget_set_save_id (cc, "color");
  gtk_box_append (GTK_BOX (box), cc);

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_widget_set_save_id (hbox, "hbox");
  gtk_box_append (GTK_BOX (box), hbox);

  sw = gtk_switch_new ();
  gtk_widget_set_save_id (sw, "switch");
  gtk_widget_set_valign (sw, GTK_ALIGN_CENTER);
  gtk_box_append (GTK_BOX (hbox), sw);

  button = gtk_check_button_new_with_label ("Check");
  gtk_widget_set_save_id (button, "check");
  gtk_box_append (GTK_BOX (hbox), button);

  button = gtk_toggle_button_new_with_label ("Toggle");
  gtk_widget_set_save_id (button, "toggle");
  gtk_box_append (GTK_BOX (hbox), button);

  entry = gtk_spin_button_new_with_range (0, 100, 1);
  gtk_widget_set_save_id (entry, "spin");
  gtk_box_append (GTK_BOX (hbox), entry);

  entry = gtk_password_entry_new ();
  gtk_password_entry_set_show_peek_icon (GTK_PASSWORD_ENTRY (entry), TRUE);
  gtk_widget_set_save_id (entry, "password");
  gtk_box_append (GTK_BOX (box), entry);

  gtk_stack_add_titled (GTK_STACK (stack), box, "page1", "Page 1");

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_widget_set_save_id (box, "box2");

  list = gtk_list_box_new ();
  gtk_widget_set_save_id (list, "list");
  g_signal_connect (list, "save-state", G_CALLBACK (save_list), NULL);
  g_signal_connect (list, "restore-state", G_CALLBACK (restore_list), NULL);

  button = gtk_button_new_with_label ("Add");
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (add_to_list), list);

  gtk_box_append (GTK_BOX (box), button);
  gtk_box_append (GTK_BOX (box), list);

  gtk_stack_add_titled (GTK_STACK (stack), box, "page2", "Page 2");

  gtk_widget_show (window);

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
