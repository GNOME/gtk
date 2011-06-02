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
#include "gtktreemodelrefcount.h"


static void
ref_count_single_level (void)
{
  GtkTreeIter iter;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, NULL);

  assert_root_level_unreferenced (ref_model);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  assert_entire_model_referenced (ref_model, 1);

  gtk_widget_destroy (tree_view);

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (sort_model);
  g_object_unref (ref_model);
}

static void
ref_count_two_levels (void)
{
  GtkTreeIter parent1, parent2, iter;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter, &parent2);

  assert_entire_model_unreferenced (ref_model);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  assert_root_level_referenced (ref_model, 1);
  assert_node_ref_count (ref_model, &iter, 0);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter, 1);

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (tree_view));

  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter, 0);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_root_level_referenced (ref_model, 1);
  assert_node_ref_count (ref_model, &iter, 0);

  gtk_widget_destroy (tree_view);

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (sort_model);
  g_object_unref (ref_model);
}

static void
ref_count_three_levels (void)
{
  GtkTreeIter grandparent1, grandparent2, parent1, parent2;
  GtkTreeIter iter_parent1, iter_parent2;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkTreePath *path;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  /* + grandparent1
   * + grandparent2
   *   + parent1
   *     + iter_parent1
   *   + parent2
   *     + iter_parent2
   *     + iter_parent2
   */

  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent1, &parent1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);

  assert_entire_model_unreferenced (ref_model);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  assert_root_level_referenced (ref_model, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 0);
  assert_level_unreferenced (ref_model, &parent1);
  assert_level_unreferenced (ref_model, &parent2);

  path = gtk_tree_path_new_from_indices (1, -1);
  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, FALSE);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, TRUE);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 2);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 1);
  assert_node_ref_count (ref_model, &iter_parent2, 1);

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (tree_view));

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_root_level_referenced (ref_model, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 0);

  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, FALSE);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_path_append_index (path, 1);
  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, FALSE);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 1);

  gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree_view), path);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_path_up (path);
  gtk_tree_view_collapse_row (GTK_TREE_VIEW (tree_view), path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 0);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 0);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_widget_destroy (tree_view);

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (sort_model);
  g_object_unref (ref_model);
}

static void
ref_count_delete_row (void)
{
  GtkTreeIter grandparent1, grandparent2, parent1, parent2;
  GtkTreeIter iter_parent1, iter_parent2;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkTreePath *path;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  /* + grandparent1
   * + grandparent2
   *   + parent1
   *     + iter_parent1
   *   + parent2
   *     + iter_parent2
   *     + iter_parent2
   */

  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent1, &parent1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);

  assert_entire_model_unreferenced (ref_model);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  assert_root_level_referenced (ref_model, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 0);
  assert_level_unreferenced (ref_model, &parent1);
  assert_level_unreferenced (ref_model, &parent2);

  path = gtk_tree_path_new_from_indices (1, -1);
  gtk_tree_view_expand_row (GTK_TREE_VIEW (tree_view), path, TRUE);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 2);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 1);
  assert_node_ref_count (ref_model, &iter_parent2, 1);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &iter_parent2);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 2);
  assert_level_referenced (ref_model, 1, &parent1);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_level_referenced (ref_model, 1, &parent2);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &parent1);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_level_referenced (ref_model, 1, &parent2);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &grandparent2);

  assert_node_ref_count (ref_model, &grandparent1, 1);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_node_ref_count (ref_model, &grandparent1, 1);

  gtk_widget_destroy (tree_view);

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (sort_model);
  g_object_unref (ref_model);
}

static void
ref_count_cleanup (void)
{
  GtkTreeIter grandparent1, grandparent2, parent1, parent2;
  GtkTreeIter iter_parent1, iter_parent2;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  /* + grandparent1
   * + grandparent2
   *   + parent1
   *     + iter_parent1
   *   + parent2
   *     + iter_parent2
   *     + iter_parent2
   */

  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent1, &parent1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  gtk_widget_destroy (tree_view);

  assert_node_ref_count (ref_model, &grandparent1, 0);
  assert_node_ref_count (ref_model, &grandparent2, 1);
  assert_node_ref_count (ref_model, &parent1, 1);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (sort_model));

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (sort_model);
  g_object_unref (ref_model);
}

static void
ref_count_row_ref (void)
{
  GtkTreeIter grandparent1, grandparent2, parent1, parent2;
  GtkTreeIter iter_parent1, iter_parent2;
  GtkTreeModel *model;
  GtkTreeModelRefCount *ref_model;
  GtkTreeModel *sort_model;
  GtkWidget *tree_view;
  GtkTreePath *path;
  GtkTreeRowReference *row_ref;

  model = gtk_tree_model_ref_count_new ();
  ref_model = GTK_TREE_MODEL_REF_COUNT (model);

  /* + grandparent1
   * + grandparent2
   *   + parent1
   *     + iter_parent1
   *   + parent2
   *     + iter_parent2
   *     + iter_parent2
   */

  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent1, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &grandparent2, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent1, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent1, &parent1);
  gtk_tree_store_append (GTK_TREE_STORE (model), &parent2, &grandparent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);
  gtk_tree_store_append (GTK_TREE_STORE (model), &iter_parent2, &parent2);

  sort_model = gtk_tree_model_sort_new_with_model (model);
  tree_view = gtk_tree_view_new_with_model (sort_model);

  path = gtk_tree_path_new_from_indices (1, 1, 1, -1);
  row_ref = gtk_tree_row_reference_new (sort_model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  /* Referenced because the node is visible, its child level is built
   * and referenced by the row ref.
   */
  assert_node_ref_count (ref_model, &grandparent2, 3);
  assert_node_ref_count (ref_model, &parent1, 0);
  /* Referenced by the row ref and because its child level is built. */
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 1);

  gtk_tree_row_reference_free (row_ref);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 2);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &parent2, 1);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 0);

  path = gtk_tree_path_new_from_indices (1, 1, 1, -1);
  row_ref = gtk_tree_row_reference_new (sort_model, path);
  gtk_tree_path_free (path);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  /* Referenced because the node is visible, its child level is built
   * and referenced by the row ref.
   */
  assert_node_ref_count (ref_model, &grandparent2, 3);
  assert_node_ref_count (ref_model, &parent1, 0);
  /* Referenced by the row ref and because its child level is built. */
  assert_node_ref_count (ref_model, &parent2, 2);
  assert_node_ref_count (ref_model, &iter_parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent2, 1);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &parent2);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent1, 0);

  gtk_tree_row_reference_free (row_ref);

  assert_node_ref_count (ref_model, &grandparent1, 1);
  assert_node_ref_count (ref_model, &grandparent2, 1);
  assert_node_ref_count (ref_model, &parent1, 0);
  assert_node_ref_count (ref_model, &iter_parent1, 0);

  gtk_widget_destroy (tree_view);
  g_object_unref (sort_model);

  assert_entire_model_unreferenced (ref_model);

  g_object_unref (ref_model);
}


static void
specific_bug_300089 (void)
{
  /* Test case for GNOME Bugzilla bug 300089.  Written by
   * Matthias Clasen.
   */
  GtkTreeModel *sort_model, *child_model;
  GtkTreePath *path;
  GtkTreeIter iter, iter2, sort_iter;

  g_test_bug ("300089");

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

  g_test_bug ("364946");

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
  g_test_add_func ("/TreeModelSort/ref-count/single-level",
                   ref_count_single_level);
  g_test_add_func ("/TreeModelSort/ref-count/two-levels",
                   ref_count_two_levels);
  g_test_add_func ("/TreeModelSort/ref-count/three-levels",
                   ref_count_three_levels);
  g_test_add_func ("/TreeModelSort/ref-count/delete-row",
                   ref_count_delete_row);
  g_test_add_func ("/TreeModelSort/ref-count/cleanup",
                   ref_count_cleanup);
  g_test_add_func ("/TreeModelSort/ref-count/row-ref",
                   ref_count_row_ref);

  g_test_add_func ("/TreeModelSort/specific/bug-300089",
                   specific_bug_300089);
  g_test_add_func ("/TreeModelSort/specific/bug-364946",
                   specific_bug_364946);
}
