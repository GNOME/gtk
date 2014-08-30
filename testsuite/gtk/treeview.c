/* Basic GtkTreeView unit tests.
 * Copyright (C) 2009  Kristian Rietveld  <kris@gtk.org>
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

static void
test_bug_546005 (void)
{
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkTreePath *cursor_path;
  GtkListStore *list_store;
  GtkWidget *view;

  g_test_bug ("546005");

  /* Tests provided by Bjorn Lindqvist, Paul Pogonyshev */
  view = gtk_tree_view_new ();

  /* Invalid path on tree view without model */
  path = gtk_tree_path_new_from_indices (1, -1);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path,
                            NULL, FALSE);
  gtk_tree_path_free (path);

  list_store = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (view),
                           GTK_TREE_MODEL (list_store));

  /* Invalid path on tree view with empty model */
  path = gtk_tree_path_new_from_indices (1, -1);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path,
                            NULL, FALSE);
  gtk_tree_path_free (path);

  /* Valid path */
  gtk_list_store_insert_with_values (list_store, &iter, 0,
                                     0, "hi",
                                     -1);

  path = gtk_tree_path_new_from_indices (0, -1);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path,
                            NULL, FALSE);

  gtk_tree_view_get_cursor (GTK_TREE_VIEW (view), &cursor_path, NULL);
  //gtk_assert_cmptreepath (cursor_path, ==, path);

  gtk_tree_path_free (path);
  gtk_tree_path_free (cursor_path);

  /* Invalid path on tree view with model */
  path = gtk_tree_path_new_from_indices (1, -1);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path,
                            NULL, FALSE);
  gtk_tree_path_free (path);

  gtk_widget_destroy (view);
}

static void
test_bug_539377 (void)
{
  GtkWidget *view;
  GtkTreePath *path;
  GtkListStore *list_store;

  g_test_bug ("539377");

  /* Test provided by Bjorn Lindqvist */

  /* Non-realized view, no model */
  view = gtk_tree_view_new ();
  g_assert (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view), 10, 10, &path,
                                           NULL, NULL, NULL) == FALSE);
  g_assert (gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (view), 10, 10,
                                               &path, NULL) == FALSE);

  /* Non-realized view, with model */
  list_store = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_tree_view_set_model (GTK_TREE_VIEW (view),
                           GTK_TREE_MODEL (list_store));

  g_assert (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (view), 10, 10, &path,
                                           NULL, NULL, NULL) == FALSE);
  g_assert (gtk_tree_view_get_dest_row_at_pos (GTK_TREE_VIEW (view), 10, 10,
                                               &path, NULL) == FALSE);

  gtk_widget_destroy (view);
}

static void
test_select_collapsed_row (void)
{
  GtkTreeIter child, parent;
  GtkTreePath *path;
  GtkTreeStore *tree_store;
  GtkTreeSelection *selection;
  GtkWidget *view;

  /* Reported by Michael Natterer */
  tree_store = gtk_tree_store_new (1, G_TYPE_STRING);
  view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (tree_store));

  gtk_tree_store_insert_with_values (tree_store, &parent, NULL, 0,
                                     0, "Parent",
                                     -1);

  gtk_tree_store_insert_with_values (tree_store, &child, &parent, 0,
                                     0, "Child",
                                     -1);
  gtk_tree_store_insert_with_values (tree_store, &child, &parent, 0,
                                     0, "Child",
                                     -1);


  /* Try to select a child path. */
  path = gtk_tree_path_new_from_indices (0, 1, -1);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, NULL, FALSE);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));

  /* Check that the parent is not selected. */
  gtk_tree_path_up (path);
  g_return_if_fail (gtk_tree_selection_path_is_selected (selection, path) == FALSE);

  /* Nothing should be selected at this point. */
  g_return_if_fail (gtk_tree_selection_count_selected_rows (selection) == 0);

  /* Check that selection really still works. */
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, NULL, FALSE);
  g_return_if_fail (gtk_tree_selection_path_is_selected (selection, path) == TRUE);
  g_return_if_fail (gtk_tree_selection_count_selected_rows (selection) == 1);

  /* Expand and select child node now. */
  gtk_tree_path_append_index (path, 1);
  gtk_tree_view_expand_all (GTK_TREE_VIEW (view));

  gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path, NULL, FALSE);
  g_return_if_fail (gtk_tree_selection_path_is_selected (selection, path) == TRUE);
  g_return_if_fail (gtk_tree_selection_count_selected_rows (selection) == 1);

  gtk_tree_path_free (path);

  gtk_widget_destroy (view);
}

static gboolean
test_row_separator_height_func (GtkTreeModel *model,
                                GtkTreeIter  *iter,
                                gpointer      data)
{
  gboolean ret = FALSE;
  GtkTreePath *path;

  path = gtk_tree_model_get_path (model, iter);
  if (gtk_tree_path_get_indices (path)[0] == 2)
    ret = TRUE;
  gtk_tree_path_free (path);

  return ret;
}

static void
test_row_separator_height (void)
{
  int focus_pad, separator_height, height;
  gboolean wide_separators;
  GtkTreeIter iter;
  GtkTreePath *path;
  GtkListStore *store;
  GtkWidget *window;
  GtkWidget *tree_view;
  GdkRectangle rect = { 0, }, cell_rect = { 0, };

  store = gtk_list_store_new (1, G_TYPE_STRING);
  gtk_list_store_insert_with_values (store, &iter, 0, 0, "Row content", -1);
  gtk_list_store_insert_with_values (store, &iter, 1, 0, "Row content", -1);
  gtk_list_store_insert_with_values (store, &iter, 2, 0, "Row content", -1);
  gtk_list_store_insert_with_values (store, &iter, 3, 0, "Row content", -1);
  gtk_list_store_insert_with_values (store, &iter, 4, 0, "Row content", -1);

  window = gtk_offscreen_window_new ();

  tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_view_set_row_separator_func (GTK_TREE_VIEW (tree_view),
                                        test_row_separator_height_func,
                                        NULL,
                                        NULL);

  gtk_tree_view_insert_column_with_attributes (GTK_TREE_VIEW (tree_view),
                                               0,
                                               "Test",
                                               gtk_cell_renderer_text_new (),
                                               "text", 0,
                                               NULL);

  gtk_container_add (GTK_CONTAINER (window), tree_view);
  gtk_widget_show_all (window);


  path = gtk_tree_path_new_from_indices (2, -1);
  gtk_tree_view_get_background_area (GTK_TREE_VIEW (tree_view),
                                     path, NULL, &rect);
  gtk_tree_view_get_cell_area (GTK_TREE_VIEW (tree_view),
                               path, NULL, &cell_rect);
  gtk_tree_path_free (path);

  gtk_widget_style_get (tree_view,
                        "focus-padding", &focus_pad,
                        "wide-separators", &wide_separators,
                        "separator-height", &separator_height,
                        NULL);

  if (wide_separators)
    height = separator_height;
  else
    height = 2;

  g_assert_cmpint (rect.height, ==, height);
  g_assert_cmpint (cell_rect.height, ==, height);

  gtk_widget_destroy (tree_view);
}

static void
test_selection_count (void)
{
  GtkTreePath *path;
  GtkListStore *list_store;
  GtkTreeSelection *selection;
  GtkWidget *view;

  g_test_bug ("702957");

  list_store = gtk_list_store_new (1, G_TYPE_STRING);
  view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (list_store));

  gtk_list_store_insert_with_values (list_store, NULL, 0, 0, "One", -1);
  gtk_list_store_insert_with_values (list_store, NULL, 1, 0, "Two", -1);
  gtk_list_store_insert_with_values (list_store, NULL, 2, 0, "Tree", -1);

  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
  gtk_tree_selection_set_mode (selection, GTK_SELECTION_MULTIPLE);

  g_assert_cmpint (gtk_tree_selection_count_selected_rows (selection), ==, 0);

  path = gtk_tree_path_new_from_indices (0, -1);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);

  g_assert_cmpint (gtk_tree_selection_count_selected_rows (selection), ==, 1);

  path = gtk_tree_path_new_from_indices (2, -1);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);

  g_assert_cmpint (gtk_tree_selection_count_selected_rows (selection), ==, 2);

  path = gtk_tree_path_new_from_indices (2, -1);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);

  g_assert_cmpint (gtk_tree_selection_count_selected_rows (selection), ==, 2);

  path = gtk_tree_path_new_from_indices (1, -1);
  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);

  g_assert_cmpint (gtk_tree_selection_count_selected_rows (selection), ==, 3);

  gtk_tree_selection_unselect_all (selection);

  g_assert_cmpint (gtk_tree_selection_count_selected_rows (selection), ==, 0);

  gtk_widget_destroy (view);
}

int
main (int    argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);
  g_test_bug_base ("http://bugzilla.gnome.org/");

  g_test_add_func ("/TreeView/cursor/bug-546005", test_bug_546005);
  g_test_add_func ("/TreeView/cursor/bug-539377", test_bug_539377);
  g_test_add_func ("/TreeView/cursor/select-collapsed_row",
                   test_select_collapsed_row);
  g_test_add_func ("/TreeView/sizing/row-separator-height",
                   test_row_separator_height);
  g_test_add_func ("/TreeView/selection/count", test_selection_count);

  return g_test_run ();
}
