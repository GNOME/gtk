/* testtreeview.c
 * Copyright (C) 2011 Red Hat, Inc
 * Author: Benjamin Otte <otte@gnome.org>
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

#include "prop-editor.h"
#include <gtk/gtk.h>

#define MIN_ROWS 50
#define MAX_ROWS 150

typedef void (* DoStuffFunc) (GtkTreeView *treeview);

static guint
count_children (GtkTreeModel *model,
                GtkTreeIter  *parent)
{
  GtkTreeIter iter;
  guint count = 0;
  gboolean valid;

  for (valid = gtk_tree_model_iter_children (model, &iter, parent);
       valid;
       valid = gtk_tree_model_iter_next (model, &iter))
    {
      count += count_children (model, &iter) + 1;
    }

  return count;
}

static void
set_rows (GtkTreeView *treeview, guint i)
{
  g_assert (i == count_children (gtk_tree_view_get_model (treeview), NULL));
  g_object_set_data (G_OBJECT (treeview), "rows", GUINT_TO_POINTER (i));
}

static guint
get_rows (GtkTreeView *treeview)
{
  return GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (treeview), "rows"));
}

static void
delete (GtkTreeView *treeview)
{
  guint n_rows = get_rows (treeview);
  guint i = g_random_int_range (0, n_rows);
  GtkTreeModel *model;
  GtkTreeIter iter, next;

  model = gtk_tree_view_get_model (treeview);
  
  if (!gtk_tree_model_get_iter_first (model, &iter))
    return;


  while (i-- > 0)
    {
      next = iter;
      if (!gtk_tree_model_iter_children (model, &iter, &next) &&
          !gtk_tree_model_iter_next (model, &next) &&
          !gtk_tree_model_iter_parent (model, &next, &iter))
        {
          g_assert_not_reached ();
          return;
        }
      iter = next;
    }

  n_rows -= count_children (model, &iter) + 1;
  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter);
  set_rows (treeview, n_rows);
}

static void
add_one (GtkTreeModel *model,
         GtkTreeIter *iter)
{
  guint n = gtk_tree_model_iter_n_children (model, iter);
  static guint counter = 0;
  
  if (n > 0 && g_random_boolean ())
    {
      GtkTreeIter child;
      gtk_tree_model_iter_nth_child (model, &child, iter, g_random_int_range (0, n));
      add_one (model, &child);
      return;
    }

  gtk_tree_store_insert_with_values (GTK_TREE_STORE (model),
                                     NULL,
                                     iter,
                                     g_random_int_range (-1, n),
                                     0, ++counter,
                                     -1);
}

static void
add (GtkTreeView *treeview)
{
  GtkTreeModel *model;

  model = gtk_tree_view_get_model (treeview);
  add_one (model, NULL);

  set_rows (treeview, get_rows (treeview) + 1);
}

static void
add_or_delete (GtkTreeView *treeview)
{
  guint n_rows = get_rows (treeview);

  if (g_random_int_range (MIN_ROWS, MAX_ROWS) >= n_rows)
    add (treeview);
  else
    delete (treeview);
}

static gboolean
dance (gpointer treeview)
{
  static const DoStuffFunc funcs[] = {
    add_or_delete,
    add_or_delete
  };

  funcs[g_random_int_range (0, G_N_ELEMENTS(funcs))] (treeview);

  return TRUE;
}

int
main (int    argc,
      char **argv)
{
  GtkWidget *window;
  GtkWidget *sw;
  GtkWidget *treeview;
  GtkTreeModel *model;
  guint i;
  
  gtk_init (&argc, &argv);

  if (g_getenv ("RTL"))
    gtk_widget_set_default_direction (GTK_TEXT_DIR_RTL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  gtk_window_set_default_size (GTK_WINDOW (window), 430, 400);

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_hexpand (sw, TRUE);
  gtk_widget_set_vexpand (sw, TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_AUTOMATIC);
  gtk_container_add (GTK_CONTAINER (window), sw);

  model = GTK_TREE_MODEL (gtk_tree_store_new (1, G_TYPE_UINT));
  treeview = gtk_tree_view_new_with_model (model);
  g_object_unref (model);
  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (treeview),
                                               0,
                                               "Counter",
                                               gtk_cell_renderer_text_new (),
                                               "text", 0,
                                               NULL);
  for (i = 0; i < (MIN_ROWS + MAX_ROWS) / 2; i++)
    add (GTK_TREE_VIEW (treeview));
  gtk_container_add (GTK_CONTAINER (sw), treeview);

  create_prop_editor (G_OBJECT (treeview), GTK_TYPE_TREE_VIEW);
  create_prop_editor (G_OBJECT (gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview))), GTK_TYPE_TREE_SELECTION);

  gtk_widget_show_all (window);

  g_timeout_add (50, dance, treeview);
  
  gtk_main ();

  return 0;
}

