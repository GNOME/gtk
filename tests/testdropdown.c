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

static void
setup_item (GtkSignalListItemFactory *self,
            GtkListItem              *list_item,
            gpointer                  data)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  gtk_label_set_xalign (GTK_LABEL (label), 0.0);
  gtk_list_item_set_child (list_item, label);
}

static void
bind_item (GtkSignalListItemFactory *factory,
           GtkListItem              *list_item)
{
  gpointer item;
  GtkWidget *label;

  item = gtk_list_item_get_item (list_item);
  label = gtk_list_item_get_child (list_item);

  gtk_label_set_label (GTK_LABEL (label), pango_font_family_get_name (PANGO_FONT_FAMILY (item)));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *button, *box, *spin;
  GtkListItemFactory *factory;
  GListModel *model;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "hello world");
  gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);

  button = gtk_drop_down_new ();
  g_object_set (button, "margin", 10, NULL);

  factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_item), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_item), NULL);
  gtk_drop_down_set_factory (GTK_DROP_DOWN (button), factory);
  g_object_unref (factory);

  model = G_LIST_MODEL (pango_cairo_font_map_get_default ());
  gtk_drop_down_set_model (GTK_DROP_DOWN (button), model);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (button), 0);

  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_container_add (GTK_CONTAINER (box), button);

  spin = gtk_spin_button_new_with_range (0, g_list_model_get_n_items (G_LIST_MODEL (model)), 1);
  g_object_set (spin, "margin", 10, NULL);
  g_object_bind_property  (button, "selected", spin, "value", G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);
  gtk_container_add (GTK_CONTAINER (box), spin);

  g_object_unref (model);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
