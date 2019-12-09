/* simple.c
 * Copyright (C) 2017  Red Hat, Inc
 * Author: Benjamin Otte
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

static char *
get_family_name (gpointer item)
{
  return g_strdup (pango_font_family_get_name (PANGO_FONT_FAMILY (item)));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *button, *box, *spin, *check;
  GListModel *model;
  GtkExpression *expression;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "hello world");
  gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  button = gtk_drop_down_new ();
  g_object_set (button, "margin", 10, NULL);

  model = G_LIST_MODEL (pango_cairo_font_map_get_default ());
  gtk_drop_down_set_model (GTK_DROP_DOWN (button), model);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (button), 0);

  expression = gtk_cclosure_expression_new (G_TYPE_STRING, NULL,
                                            0, NULL,
                                            (GCallback)get_family_name,
                                            NULL, NULL);
  gtk_drop_down_set_expression (GTK_DROP_DOWN (button), expression);
  gtk_expression_unref (expression);

  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), button);

  spin = gtk_spin_button_new_with_range (-1, g_list_model_get_n_items (G_LIST_MODEL (model)), 1);
  g_object_set (spin, "margin", 10, NULL);
  g_object_bind_property  (button, "selected", spin, "value", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_container_add (GTK_CONTAINER (box), spin);

  check = gtk_check_button_new_with_label ("Enable search");
  g_object_set (check, "margin", 10, NULL);
  g_object_bind_property  (button, "enable-search", check, "active", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_container_add (GTK_CONTAINER (box), check);

  g_object_unref (model);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
