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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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
  g_assert (gtk_tree_path_compare (cursor_path, path) == 0);

  gtk_tree_path_free (path);
  gtk_tree_path_free (cursor_path);

  /* Invalid path on tree view with model */
  path = gtk_tree_path_new_from_indices (1, -1);
  gtk_tree_view_set_cursor (GTK_TREE_VIEW (view), path,
                            NULL, FALSE);
  gtk_tree_path_free (path);
}

static void
test_bug_539377 (void)
{
  GtkWidget *view;
  GtkTreePath *path;
  GtkListStore *list_store;

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
}

int
main (int    argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/TreeView/cursor/bug-546005", test_bug_546005);
  g_test_add_func ("/TreeView/cursor/bug-539377", test_bug_539377);

  return g_test_run ();
}
