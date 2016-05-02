/* -*- Mode: C; c-file-style: "gnu"; tab-width: 8 -*- */
/*
 * GTK - The GIMP Toolkit
 * Copyright (C) 2006  Carlos Garnacho Parro <carlosg@gnome.org>
 *
 * All rights reserved.
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
#include <gtk/gtk.h>

static gpointer GROUP_A = "GROUP_A";
static gpointer GROUP_B = "GROUP_B";

gchar *tabs1 [] = {
  "aaaaaaaaaa",
  "bbbbbbbbbb",
  "cccccccccc",
  "dddddddddd",
  NULL
};

gchar *tabs2 [] = {
  "1",
  "2",
  "3",
  "4",
  "55555",
  NULL
};

gchar *tabs3 [] = {
  "foo",
  "bar",
  NULL
};

gchar *tabs4 [] = {
  "beer",
  "water",
  "lemonade",
  "coffee",
  "tea",
  NULL
};

static const GtkTargetEntry button_targets[] = {
  { "GTK_NOTEBOOK_TAB", GTK_TARGET_SAME_APP, 0 },
};

static GtkNotebook*
window_creation_function (GtkNotebook *source_notebook,
                          GtkWidget   *child,
                          gint         x,
                          gint         y,
                          gpointer     data)
{
  GtkWidget *window, *notebook;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  notebook = gtk_notebook_new ();
  g_signal_connect (notebook, "create-window",
                    G_CALLBACK (window_creation_function), NULL);

  gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook),
                               gtk_notebook_get_group_name (source_notebook));

  gtk_container_add (GTK_CONTAINER (window), notebook);

  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);
  gtk_window_move (GTK_WINDOW (window), x, y);
  gtk_widget_show_all (window);

  return GTK_NOTEBOOK (notebook);
}

static void
on_page_reordered (GtkNotebook *notebook, GtkWidget *child, guint page_num, gpointer data)
{
  g_print ("page %d reordered\n", page_num);
}

static void
on_notebook_drag_begin (GtkWidget      *widget,
                        GdkDragContext *context,
                        gpointer        data)
{
  GdkPixbuf *pixbuf;
  guint page_num;

  page_num = gtk_notebook_get_current_page (GTK_NOTEBOOK (widget));

  if (page_num > 2)
    {
      GtkIconTheme *icon_theme;
      int width;

      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (widget));
      gtk_icon_size_lookup (GTK_ICON_SIZE_DND, &width, NULL);
      pixbuf = gtk_icon_theme_load_icon (icon_theme,
                                         (page_num % 2) ? "help-browser" : "process-stop",
                                         width,
                                         GTK_ICON_LOOKUP_GENERIC_FALLBACK,
                                         NULL);

      gtk_drag_set_icon_pixbuf (context, pixbuf, 0, 0);
      g_object_unref (pixbuf);
    }
}

static gboolean
remove_in_idle (gpointer data)
{
  GtkWidget *child = data;
  GtkWidget *parent = gtk_widget_get_parent (child);
  GtkWidget *tab_label;

  tab_label = gtk_notebook_get_tab_label (GTK_NOTEBOOK (parent), child);
  g_print ("Removing tab: %s\n", gtk_label_get_text (GTK_LABEL (tab_label)));
  gtk_container_remove (GTK_CONTAINER (parent), child);

  return G_SOURCE_REMOVE;
}

static void
on_button_drag_data_received (GtkWidget        *widget,
                              GdkDragContext   *context,
                              gint              x,
                              gint              y,
                              GtkSelectionData *data,
                              guint             info,
                              guint             time,
                              gpointer          user_data)
{
  GtkWidget **child;

  child = (void*) gtk_selection_data_get_data (data);

  g_idle_add (remove_in_idle, *child);
}

static void
action_clicked_cb (GtkWidget *button,
                   GtkWidget *notebook)
{
  GtkWidget *page, *title;

  page = gtk_entry_new ();
  gtk_entry_set_text (GTK_ENTRY (page), "Addition");
  gtk_widget_show (page);

  title = gtk_label_new ("Addition");

  gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, title);
  gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), page, TRUE);
  gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), page, TRUE);
}

static GtkWidget*
create_notebook (gchar           **labels,
                 const gchar      *group,
                 GtkPositionType   pos)
{
  GtkWidget *notebook, *title, *page, *action_widget;

  notebook = gtk_notebook_new ();
  gtk_widget_set_vexpand (notebook, TRUE);
  gtk_widget_set_hexpand (notebook, TRUE);

  action_widget = gtk_button_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_BUTTON);
  g_signal_connect (action_widget, "clicked", G_CALLBACK (action_clicked_cb), notebook);
  gtk_widget_show (action_widget);
  gtk_notebook_set_action_widget (GTK_NOTEBOOK (notebook), action_widget, GTK_PACK_END);

  g_signal_connect (notebook, "create-window",
                    G_CALLBACK (window_creation_function), NULL);

  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), pos);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
  gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook), group);

  while (*labels)
    {
      page = gtk_entry_new ();
      gtk_entry_set_text (GTK_ENTRY (page), *labels);

      title = gtk_label_new (*labels);

      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, title);
      gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), page, TRUE);
      gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), page, TRUE);

      labels++;
    }

  g_signal_connect (GTK_NOTEBOOK (notebook), "page-reordered",
                    G_CALLBACK (on_page_reordered), NULL);
  g_signal_connect_after (G_OBJECT (notebook), "drag-begin",
                          G_CALLBACK (on_notebook_drag_begin), NULL);
  return notebook;
}

static GtkWidget*
create_notebook_non_dragable_content (gchar           **labels,
                                      const gchar      *group,
                                      GtkPositionType   pos)
{
  GtkWidget *notebook, *title, *page, *action_widget;

  notebook = gtk_notebook_new ();
  gtk_widget_set_vexpand (notebook, TRUE);
  gtk_widget_set_hexpand (notebook, TRUE);

  action_widget = gtk_button_new_from_icon_name ("list-add-symbolic", GTK_ICON_SIZE_BUTTON);
  g_signal_connect (action_widget, "clicked", G_CALLBACK (action_clicked_cb), notebook);
  gtk_widget_show (action_widget);
  gtk_notebook_set_action_widget (GTK_NOTEBOOK (notebook), action_widget, GTK_PACK_END);

  g_signal_connect (notebook, "create-window",
                    G_CALLBACK (window_creation_function), NULL);

  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), pos);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
  gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook), group);

  while (*labels)
    {
      GtkWidget *button;
      button = gtk_button_new_with_label (*labels);
      /* Use GtkListBox since it bubbles up motion notify event, which can
       * experience more issues than GtkBox. */
      page = gtk_list_box_new ();
      gtk_container_add (GTK_CONTAINER (page), button);

      title = gtk_label_new (*labels);

      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, title);
      gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), page, TRUE);
      gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), page, TRUE);

      labels++;
    }

  g_signal_connect (GTK_NOTEBOOK (notebook), "page-reordered",
                    G_CALLBACK (on_page_reordered), NULL);
  g_signal_connect_after (G_OBJECT (notebook), "drag-begin",
                          G_CALLBACK (on_notebook_drag_begin), NULL);
  return notebook;
}

static GtkWidget*
create_notebook_with_notebooks (gchar           **labels,
                                const gchar      *group,
                                GtkPositionType   pos)
{
  GtkWidget *notebook, *title, *page;
  gint count = 0;

  notebook = gtk_notebook_new ();
  g_signal_connect (notebook, "create-window",
                    G_CALLBACK (window_creation_function), NULL);

  gtk_notebook_set_tab_pos (GTK_NOTEBOOK (notebook), pos);
  gtk_notebook_set_scrollable (GTK_NOTEBOOK (notebook), TRUE);
  gtk_container_set_border_width (GTK_CONTAINER (notebook), 6);
  gtk_notebook_set_group_name (GTK_NOTEBOOK (notebook), group);

  while (*labels)
    {
      page = create_notebook (labels, group, pos);
      gtk_notebook_popup_enable (GTK_NOTEBOOK (page));

      title = gtk_label_new (*labels);

      gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page, title);
      gtk_notebook_set_tab_reorderable (GTK_NOTEBOOK (notebook), page, TRUE);
      gtk_notebook_set_tab_detachable (GTK_NOTEBOOK (notebook), page, TRUE);

      count++;
      labels++;
    }

  g_signal_connect (GTK_NOTEBOOK (notebook), "page-reordered",
                    G_CALLBACK (on_page_reordered), NULL);
  g_signal_connect_after (G_OBJECT (notebook), "drag-begin",
                          G_CALLBACK (on_notebook_drag_begin), NULL);
  return notebook;
}

static GtkWidget*
create_trash_button (void)
{
  GtkWidget *button;

  button = gtk_button_new_with_mnemonic ("_Delete");

  gtk_drag_dest_set (button,
                     GTK_DEST_DEFAULT_MOTION | GTK_DEST_DEFAULT_DROP,
                     button_targets,
                     G_N_ELEMENTS (button_targets),
                     GDK_ACTION_MOVE);

  g_signal_connect_after (G_OBJECT (button), "drag-data-received",
                          G_CALLBACK (on_button_drag_data_received), NULL);
  return button;
}

gint
main (gint argc, gchar *argv[])
{
  GtkWidget *window, *grid;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  grid = gtk_grid_new ();

  gtk_grid_attach (GTK_GRID (grid),
                   create_notebook_non_dragable_content (tabs1, GROUP_A, GTK_POS_TOP),
                   0, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (grid),
                   create_notebook (tabs2, GROUP_B, GTK_POS_BOTTOM),
                   0, 1, 1, 1);

  gtk_grid_attach (GTK_GRID (grid),
                   create_notebook (tabs3, GROUP_B, GTK_POS_LEFT),
                   1, 0, 1, 1);

  gtk_grid_attach (GTK_GRID (grid),
                   create_notebook_with_notebooks (tabs4, GROUP_A, GTK_POS_RIGHT),
                   1, 1, 1, 1);

  gtk_grid_attach (GTK_GRID (grid),
                   create_trash_button (),
                   1, 2, 1, 1);

  gtk_container_add (GTK_CONTAINER (window), grid);
  gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);
  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
