/* Extensive GtkTreeModelSort tests.
 * Copyright (C) 2009,2011  Kristian Rietveld  <kris@gtk.org>
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>

#include "treemodel.h"

static void
specific_bug_300089 (void)
{
  /* Test case for GNOME Bugzilla bug 300089.  Written by
   * Matthias Clasen.
   */
  GtkTreeModel *sort_model, *child_model;
  GtkTreePath *path;
  GtkTreeIter iter, iter2, sort_iter;

  child_model = GTK_TREE_MODEL (gtk_tree_store_new (1, G_TYPE_STRING));

  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "A", -1);
  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "B", -1);

  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter2, &iter);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter2, 0, "D", -1);
  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter2, &iter);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter2, 0, "E", -1);

  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "C", -1);


  sort_model = GTK_TREE_MODEL (gtk_tree_model_sort_new_with_model (child_model));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        0, GTK_SORT_ASCENDING);

  path = gtk_tree_path_new_from_indices (1, 1, -1);

  /* make sure a level is constructed */ 
  gtk_tree_model_get_iter (sort_model, &sort_iter, path);

  /* change the "E" row in a way that causes it to change position */ 
  gtk_tree_model_get_iter (child_model, &iter, path);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "A", -1);
}

static void
specific_bug_364946 (void)
{
  /* This is a test case for GNOME Bugzilla bug 364946.  It was written
   * by Andreas Koehler.
   */
  GtkTreeStore *store;
  GtkTreeIter a, aa, aaa, aab, iter;
  GtkTreeModel *s_model;

  store = gtk_tree_store_new (1, G_TYPE_STRING);

  gtk_tree_store_append (store, &a, NULL);
  gtk_tree_store_set (store, &a, 0, "0", -1);

  gtk_tree_store_append (store, &aa, &a);
  gtk_tree_store_set (store, &aa, 0, "0:0", -1);

  gtk_tree_store_append (store, &aaa, &aa);
  gtk_tree_store_set (store, &aaa, 0, "0:0:0", -1);

  gtk_tree_store_append (store, &aab, &aa);
  gtk_tree_store_set (store, &aab, 0, "0:0:1", -1);

  s_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (s_model), 0,
                                        GTK_SORT_ASCENDING);

  gtk_tree_model_get_iter_from_string (s_model, &iter, "0:0:0");

  gtk_tree_store_set (store, &aaa, 0, "0:0:0", -1);
  gtk_tree_store_remove (store, &aaa);
  gtk_tree_store_remove (store, &aab);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (s_model));
}

/* main */

void
register_sort_model_tests (void)
{
  g_test_add_func ("/TreeModelSort/specific/bug-300089",
                   specific_bug_300089);
  g_test_add_func ("/TreeModelSort/specific/bug-364946",
                   specific_bug_364946);
}
