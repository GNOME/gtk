/* testcombochange.c
 * Copyright (C) 2004  Red Hat, Inc.
 * Author: Owen Taylor
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

#include "config.h"
#include <gtk/gtk.h>
#include <stdarg.h>

GtkWidget *text_view;
GtkListStore *model;
GArray *contents;

static char next_value = 'A';

static void
test_init (void)
{
  if (g_file_test ("../modules/input/immodules.cache", G_FILE_TEST_EXISTS))
    g_setenv ("GTK_IM_MODULE_FILE", "../modules/input/immodules.cache", TRUE);
}

static void
combochange_log (const char *fmt,
     ...)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (text_view));
  GtkTextIter iter;
  va_list vap;
  char *msg;
  GString *order_string;
  GtkTextMark *tmp_mark;
  int i;

  va_start (vap, fmt);
  
  msg = g_strdup_vprintf (fmt, vap);

  gtk_text_buffer_get_end_iter (buffer, &iter);
  gtk_text_buffer_insert (buffer, &iter, msg, -1);

  order_string = g_string_new ("\n  ");
  for (i = 0; i < contents->len; i++)
    {
      if (i)
	g_string_append_c (order_string, ' ');
      g_string_append_c (order_string, g_array_index (contents, char, i));
    }
  g_string_append_c (order_string, '\n');
  gtk_text_buffer_insert (buffer, &iter, order_string->str, -1);
  g_string_free (order_string, TRUE);

  tmp_mark = gtk_text_buffer_create_mark (buffer, NULL, &iter, FALSE);
  gtk_text_view_scroll_mark_onscreen (GTK_TEXT_VIEW (text_view), tmp_mark);
  gtk_text_buffer_delete_mark (buffer, tmp_mark);

  g_free (msg);
}

static GtkWidget *
create_combo (const char *name,
	      gboolean is_list)
{
  GtkCellRenderer *cell_renderer;
  GtkWidget *combo;
  GtkCssProvider *provider;
  GtkStyleContext *context;
  gchar *css_data;

  combo = gtk_combo_box_new_with_model (GTK_TREE_MODEL (model));
  cell_renderer = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), cell_renderer, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (combo), cell_renderer,
				  "text", 0, NULL);

  gtk_widget_set_name (combo, name);

  context = gtk_widget_get_style_context (combo);

  provider = gtk_css_provider_new ();
  css_data = g_strdup_printf ("#%s { -GtkComboBox-appears-as-list: %s }",
                              name, is_list ? "true" : "false");
  gtk_css_provider_load_from_data (provider, css_data, -1);
  g_free (css_data);

  gtk_style_context_add_provider (context,
                                  GTK_STYLE_PROVIDER (provider),
                                  GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  return combo;
}

static void
on_insert (void)
{
  GtkTreeIter iter;
  
  int insert_pos;
  char new_value[2];

  new_value[0] = next_value++;
  new_value[1] = '\0';

  if (next_value > 'Z')
    next_value = 'A';
  
  if (contents->len)
    insert_pos = g_random_int_range (0, contents->len + 1);
  else
    insert_pos = 0;
  
  gtk_list_store_insert (model, &iter, insert_pos);
  gtk_list_store_set (model, &iter, 0, new_value, -1);

  g_array_insert_val (contents, insert_pos, new_value);

  combochange_log ("Inserted '%c' at position %d", new_value[0], insert_pos);
}

static void
on_delete (void)
{
  GtkTreeIter iter;
  
  int delete_pos;
  char old_val;

  if (!contents->len)
    return;
  
  delete_pos = g_random_int_range (0, contents->len);
  gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (model), &iter, NULL, delete_pos);
  
  gtk_list_store_remove (model, &iter);

  old_val = g_array_index (contents, char, delete_pos);
  g_array_remove_index (contents, delete_pos);
  combochange_log ("Deleted '%c' from position %d", old_val, delete_pos);
}

static void
on_reorder (void)
{
  GArray *new_contents;
  gint *shuffle_array;
  gint i;

  shuffle_array = g_new (int, contents->len);
  
  for (i = 0; i < contents->len; i++)
    shuffle_array[i] = i;

  for (i = 0; i + 1 < contents->len; i++)
    {
      gint pos = g_random_int_range (i, contents->len);
      gint tmp;

      tmp = shuffle_array[i];
      shuffle_array[i] = shuffle_array[pos];
      shuffle_array[pos] = tmp;
    }

  gtk_list_store_reorder (model, shuffle_array);

  new_contents = g_array_new (FALSE, FALSE, sizeof (char));
  for (i = 0; i < contents->len; i++)
    g_array_append_val (new_contents,
			g_array_index (contents, char, shuffle_array[i]));
  g_array_free (contents, TRUE);
  contents = new_contents;

  combochange_log ("Reordered array");
    
  g_free (shuffle_array);
}

static int n_animations = 0;
static int timer = 0;

static gint
animation_timer (gpointer data)
{
  switch (g_random_int_range (0, 3)) 
    {
    case 0: 
      on_insert ();
      break;
    case 1:
      on_delete ();
      break;
    case 2:
      on_reorder ();
      break;
    }

  n_animations--;
  return n_animations > 0;
}

static void
on_animate (void)
{
  n_animations += 20;
 
  timer = g_timeout_add (1000, (GSourceFunc) animation_timer, NULL);
}

int
main (int argc, char **argv)
{
  GtkWidget *content_area;
  GtkWidget *dialog;
  GtkWidget *scrolled_window;
  GtkWidget *hbox;
  GtkWidget *button_vbox;
  GtkWidget *combo_vbox;
  GtkWidget *button;
  GtkWidget *menu_combo;
  GtkWidget *label;
  GtkWidget *list_combo;
  
  test_init ();

  gtk_init ();

  model = gtk_list_store_new (1, G_TYPE_STRING);
  contents = g_array_new (FALSE, FALSE, sizeof (char));
  
  dialog = gtk_dialog_new_with_buttons ("GtkComboBox model changes",
					NULL, 0,
					"_Close", GTK_RESPONSE_CLOSE,
					NULL);

  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));

  hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 12);
  gtk_container_add (GTK_CONTAINER (content_area), hbox);

  combo_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (hbox), combo_vbox);

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), "<b>Menu mode</b>");
  gtk_container_add (GTK_CONTAINER (combo_vbox), label);

  menu_combo = create_combo ("menu-combo", FALSE);
  gtk_widget_set_margin_start (menu_combo, 12);
  gtk_container_add (GTK_CONTAINER (combo_vbox), menu_combo);

  label = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (label), "<b>List mode</b>");
  gtk_container_add (GTK_CONTAINER (combo_vbox), label);

  list_combo = create_combo ("list-combo", TRUE);
  gtk_widget_set_margin_start (list_combo, 12);
  gtk_container_add (GTK_CONTAINER (combo_vbox), list_combo);

  scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (scrolled_window, TRUE);
  gtk_container_add (GTK_CONTAINER (hbox), scrolled_window);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
				  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

  text_view = gtk_text_view_new ();
  gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);
  gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (text_view), FALSE);

  gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);

  button_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add (GTK_CONTAINER (hbox), button_vbox);

  gtk_window_set_default_size (GTK_WINDOW (dialog), 500, 300);

  button = gtk_button_new_with_label ("Insert");
  gtk_container_add (GTK_CONTAINER (button_vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (on_insert), NULL);

  button = gtk_button_new_with_label ("Delete");
  gtk_container_add (GTK_CONTAINER (button_vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (on_delete), NULL);

  button = gtk_button_new_with_label ("Reorder");
  gtk_container_add (GTK_CONTAINER (button_vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (on_reorder), NULL);

  button = gtk_button_new_with_label ("Animate");
  gtk_container_add (GTK_CONTAINER (button_vbox), button);
  g_signal_connect (button, "clicked", G_CALLBACK (on_animate), NULL);

  gtk_widget_show (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));

  return 0;
}
