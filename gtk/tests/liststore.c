/* Extensive GtkListStore tests.
 * Copyright (C) 2007  Imendio AB
 * Authors: Kristian Rietveld  <kris@imendio.com>
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

/* To do:
 *  - Test implementations of the interfaces: DnD, sortable, buildable
 *    and the tree model interface itself?
 *  - Need to check if the emitted signals are right.
 *  - Needs analysis with the code coverage tool once it is there.
 */

#include <gtk/gtk.h>

static inline gboolean
iters_equal (GtkTreeIter *a,
	     GtkTreeIter *b)
{
  if (a->stamp != b->stamp)
    return FALSE;

  if (a->user_data != b->user_data)
    return FALSE;

  /* user_data2 and user_data3 are not used in GtkListStore */

  return TRUE;
}

static gboolean
iter_position (GtkListStore *store,
	       GtkTreeIter  *iter,
	       int           n)
{
  gboolean ret = TRUE;
  GtkTreePath *path;

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter);
  if (!path)
    return FALSE;

  if (gtk_tree_path_get_indices (path)[0] != n)
    ret = FALSE;

  gtk_tree_path_free (path);

  return ret;
}

/*
 * Fixture
 */
typedef struct
{
  GtkTreeIter iter[5];
  GtkListStore *store;
} ListStore;

static void
list_store_setup (ListStore     *fixture,
		  gconstpointer  test_data)
{
  int i;

  fixture->store = gtk_list_store_new (1, G_TYPE_INT);

  for (i = 0; i < 5; i++)
    {
      gtk_list_store_insert (fixture->store, &fixture->iter[i], i);
      gtk_list_store_set (fixture->store, &fixture->iter[i], 0, i, -1);
    }
}

static void
list_store_teardown (ListStore     *fixture,
		     gconstpointer  test_data)
{
  g_object_unref (fixture->store);
}

/*
 * The actual tests.
 */

static void
check_model (ListStore *fixture,
	     gint      *new_order,
	     gint       skip)
{
  int i;
  GtkTreePath *path;

  path = gtk_tree_path_new ();
  gtk_tree_path_down (path);

  /* Check validity of the model and validity of the iters-persistent
   * claim.
   */
  for (i = 0; i < 5; i++)
    {
      GtkTreeIter iter;

      if (i == skip)
	continue;

      /* The saved iterator at new_order[i] should match the iterator
       * at i.
       */

      gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store),
			       &iter, path);

      g_assert (gtk_list_store_iter_is_valid (fixture->store, &iter));
      g_assert (iters_equal (&iter, &fixture->iter[new_order[i]]));

      gtk_tree_path_next (path);
    }

  gtk_tree_path_free (path);
}

/* insertion */
static void
list_store_test_insert_high_values (void)
{
  GtkTreeIter iter, iter2;
  GtkTreeIter iter_copy;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_INT);

  gtk_list_store_insert (store, &iter, 1234);
  g_assert (gtk_list_store_iter_is_valid (store, &iter));
  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 1);
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (iter_position (store, &iter, 0));

  gtk_list_store_insert (store, &iter2, 765);
  g_assert (gtk_list_store_iter_is_valid (store, &iter2));
  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 2);

  /* Walk over the model */
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (iter_position (store, &iter, 0));

  g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter2, &iter_copy));
  g_assert (iter_position (store, &iter2, 1));

  g_assert (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));

  g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter_copy, NULL, 1));
  g_assert (iters_equal (&iter2, &iter_copy));

  g_object_unref (store);
}

static void
list_store_test_append (void)
{
  GtkTreeIter iter, iter2;
  GtkTreeIter iter_copy;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_INT);

  gtk_list_store_append (store, &iter);
  g_assert (gtk_list_store_iter_is_valid (store, &iter));
  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 1);
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (iter_position (store, &iter, 0));

  gtk_list_store_append (store, &iter2);
  g_assert (gtk_list_store_iter_is_valid (store, &iter2));
  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 2);

  /* Walk over the model */
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (iter_position (store, &iter, 0));

  g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter2, &iter_copy));
  g_assert (iter_position (store, &iter2, 1));

  g_assert (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));

  g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter_copy, NULL, 1));
  g_assert (iters_equal (&iter2, &iter_copy));

  g_object_unref (store);
}

static void
list_store_test_prepend (void)
{
  GtkTreeIter iter, iter2;
  GtkTreeIter iter_copy;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_INT);

  gtk_list_store_prepend (store, &iter);
  g_assert (gtk_list_store_iter_is_valid (store, &iter));
  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 1);
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (iter_position (store, &iter, 0));

  gtk_list_store_prepend (store, &iter2);
  g_assert (gtk_list_store_iter_is_valid (store, &iter2));
  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 2);

  /* Walk over the model */
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter2, &iter_copy));
  g_assert (iter_position (store, &iter2, 0));

  g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (iter_position (store, &iter, 1));

  g_assert (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));

  g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter_copy, NULL, 1));
  g_assert (iters_equal (&iter, &iter_copy));

  g_object_unref (store);
}

static void
list_store_test_insert_after (void)
{
  GtkTreeIter iter, iter2, iter3;
  GtkTreeIter iter_copy;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_INT);

  gtk_list_store_append (store, &iter);
  gtk_list_store_append (store, &iter2);

  gtk_list_store_insert_after (store, &iter3, &iter);
  g_assert (gtk_list_store_iter_is_valid (store, &iter3));
  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 3);
  g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter_copy, NULL, 1));
  g_assert (iters_equal (&iter3, &iter_copy));
  g_assert (iter_position (store, &iter3, 1));

  /* Walk over the model */
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (iter_position (store, &iter_copy, 0));

  g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter3, &iter_copy));
  g_assert (iter_position (store, &iter_copy, 1));

  g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter2, &iter_copy));
  g_assert (iter_position (store, &iter_copy, 2));

  g_assert (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));

  g_object_unref (store);
}

static void
list_store_test_insert_after_NULL (void)
{
  GtkTreeIter iter, iter2;
  GtkTreeIter iter_copy;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_INT);

  gtk_list_store_append (store, &iter);

  /* move_after NULL is basically a prepend */
  gtk_list_store_insert_after (store, &iter2, NULL);
  g_assert (gtk_list_store_iter_is_valid (store, &iter2));
  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 2);

  /* Walk over the model */
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter2, &iter_copy));
  g_assert (iter_position (store, &iter2, 0));

  g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (iter_position (store, &iter, 1));

  g_assert (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));

  g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter_copy, NULL, 0));
  g_assert (iters_equal (&iter2, &iter_copy));

  g_object_unref (store);
}

static void
list_store_test_insert_before (void)
{
  GtkTreeIter iter, iter2, iter3;
  GtkTreeIter iter_copy;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_INT);

  gtk_list_store_append (store, &iter);
  gtk_list_store_append (store, &iter2);

  gtk_list_store_insert_before (store, &iter3, &iter2);
  g_assert (gtk_list_store_iter_is_valid (store, &iter3));
  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 3);
  g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter_copy, NULL, 1));
  g_assert (iters_equal (&iter3, &iter_copy));
  g_assert (iter_position (store, &iter3, 1));

  /* Walk over the model */
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (iter_position (store, &iter_copy, 0));

  g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter3, &iter_copy));
  g_assert (iter_position (store, &iter_copy, 1));

  g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter2, &iter_copy));
  g_assert (iter_position (store, &iter_copy, 2));

  g_assert (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));

  g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter_copy, NULL, 1));
  g_assert (iters_equal (&iter3, &iter_copy));

  g_object_unref (store);
}

static void
list_store_test_insert_before_NULL (void)
{
  GtkTreeIter iter, iter2;
  GtkTreeIter iter_copy;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_INT);

  gtk_list_store_append (store, &iter);

  /* move_before NULL is basically an append */
  gtk_list_store_insert_before (store, &iter2, NULL);
  g_assert (gtk_list_store_iter_is_valid (store, &iter2));
  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), NULL) == 2);

  /* Walk over the model */
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (iter_position (store, &iter, 0));

  g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));
  g_assert (iters_equal (&iter2, &iter_copy));
  g_assert (iter_position (store, &iter2, 1));

  g_assert (!gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter_copy));

  g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter_copy, NULL, 1));
  g_assert (iters_equal (&iter2, &iter_copy));

  g_object_unref (store);
}

/* removal */
static void
list_store_test_remove_begin (ListStore     *fixture,
			      gconstpointer  user_data)
{
  int new_order[5] = { -1, 1, 2, 3, 4 };
  GtkTreePath *path;
  GtkTreeIter iter;

  /* Remove node at 0 */
  path = gtk_tree_path_new_from_indices (0, -1);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store), &iter, path);
  gtk_tree_path_free (path);

  g_assert (gtk_list_store_remove (fixture->store, &iter) == TRUE);
  g_assert (!gtk_list_store_iter_is_valid (fixture->store, &fixture->iter[0]));
  g_assert (iters_equal (&iter, &fixture->iter[1]));

  check_model (fixture, new_order, 0);
}

static void
list_store_test_remove_middle (ListStore     *fixture,
			       gconstpointer  user_data)
{
  int new_order[5] = { 0, 1, -1, 3, 4 };
  GtkTreePath *path;
  GtkTreeIter iter;

  /* Remove node at 2 */
  path = gtk_tree_path_new_from_indices (2, -1);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store), &iter, path);
  gtk_tree_path_free (path);

  g_assert (gtk_list_store_remove (fixture->store, &iter) == TRUE);
  g_assert (!gtk_list_store_iter_is_valid (fixture->store, &fixture->iter[2]));
  g_assert (iters_equal (&iter, &fixture->iter[3]));

  check_model (fixture, new_order, 2);
}

static void
list_store_test_remove_end (ListStore     *fixture,
			    gconstpointer  user_data)
{
  int new_order[5] = { 0, 1, 2, 3, -1 };
  GtkTreePath *path;
  GtkTreeIter iter;

  /* Remove node at 4 */
  path = gtk_tree_path_new_from_indices (4, -1);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store), &iter, path);
  gtk_tree_path_free (path);

  g_assert (gtk_list_store_remove (fixture->store, &iter) == FALSE);
  g_assert (!gtk_list_store_iter_is_valid (fixture->store, &fixture->iter[4]));

  check_model (fixture, new_order, 4);
}

static void
list_store_test_clear (ListStore     *fixture,
		       gconstpointer  user_data)
{
  int i;

  gtk_list_store_clear (fixture->store);

  g_assert (gtk_tree_model_iter_n_children (GTK_TREE_MODEL (fixture->store), NULL) == 0);

  for (i = 0; i < 5; i++)
    g_assert (!gtk_list_store_iter_is_valid (fixture->store, &fixture->iter[i]));
}

/* reorder */

static void
list_store_test_reorder (ListStore     *fixture,
			 gconstpointer  user_data)
{
  int new_order[5] = { 4, 1, 0, 2, 3 };

  gtk_list_store_reorder (fixture->store, new_order);
  check_model (fixture, new_order, -1);
}

/* swapping */

static void
list_store_test_swap_begin (ListStore     *fixture,
		            gconstpointer  user_data)
{
  /* We swap nodes 0 and 1 at the beginning */
  int new_order[5] = { 1, 0, 2, 3, 4 };

  GtkTreeIter iter_a;
  GtkTreeIter iter_b;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_a, "0"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_b, "1"));

  gtk_list_store_swap (fixture->store, &iter_a, &iter_b);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_swap_middle_next (ListStore     *fixture,
		                  gconstpointer  user_data)
{
  /* We swap nodes 2 and 3 in the middle that are next to each other */
  int new_order[5] = { 0, 1, 3, 2, 4 };

  GtkTreeIter iter_a;
  GtkTreeIter iter_b;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_a, "2"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_b, "3"));

  gtk_list_store_swap (fixture->store, &iter_a, &iter_b);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_swap_middle_apart (ListStore     *fixture,
		                   gconstpointer  user_data)
{
  /* We swap nodes 1 and 3 in the middle that are apart from each other */
  int new_order[5] = { 0, 3, 2, 1, 4 };

  GtkTreeIter iter_a;
  GtkTreeIter iter_b;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_a, "1"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_b, "3"));

  gtk_list_store_swap (fixture->store, &iter_a, &iter_b);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_swap_end (ListStore     *fixture,
		          gconstpointer  user_data)
{
  /* We swap nodes 3 and 4 at the end */
  int new_order[5] = { 0, 1, 2, 4, 3 };

  GtkTreeIter iter_a;
  GtkTreeIter iter_b;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_a, "3"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter_b, "4"));

  gtk_list_store_swap (fixture->store, &iter_a, &iter_b);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_swap_single (void)
{
  GtkTreeIter iter;
  GtkTreeIter iter_copy;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_INT);

  /* Check if swap on a store with a single node does not corrupt
   * the store.
   */

  gtk_list_store_append (store, &iter);
  iter_copy = iter;

  gtk_list_store_swap (store, &iter, &iter);
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter));
  g_assert (iters_equal (&iter, &iter_copy));

  g_object_unref (store);
}

/* move after */

static void
list_store_test_move_after_from_start (ListStore     *fixture,
				       gconstpointer  user_data)
{
  /* We move node 0 after 2 */
  int new_order[5] = { 1, 2, 0, 3, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "0"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "2"));

  gtk_list_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_after_next (ListStore     *fixture,
			         gconstpointer  user_data)
{
  /* We move node 2 after 3 */
  int new_order[5] = { 0, 1, 3, 2, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "2"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "3"));

  gtk_list_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_after_apart (ListStore     *fixture,
			          gconstpointer  user_data)
{
  /* We move node 1 after 3 */
  int new_order[5] = { 0, 2, 3, 1, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "1"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "3"));

  gtk_list_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_after_end (ListStore     *fixture,
			        gconstpointer  user_data)
{
  /* We move node 2 after 4 */
  int new_order[5] = { 0, 1, 3, 4, 2 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "2"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "4"));

  gtk_list_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_after_from_end (ListStore     *fixture,
			             gconstpointer  user_data)
{
  /* We move node 4 after 1 */
  int new_order[5] = { 0, 1, 4, 2, 3 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "4"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "1"));

  gtk_list_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_after_change_ends (ListStore     *fixture,
			                gconstpointer  user_data)
{
  /* We move 0 after 4, this will cause both the head and tail ends to
   * change.
   */
  int new_order[5] = { 1, 2, 3, 4, 0 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "0"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "4"));

  gtk_list_store_move_after (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_after_NULL (ListStore     *fixture,
			         gconstpointer  user_data)
{
  /* We move node 2, NULL should prepend */
  int new_order[5] = { 2, 0, 1, 3, 4 };

  GtkTreeIter iter;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "2"));

  gtk_list_store_move_after (fixture->store, &iter, NULL);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_after_single (void)
{
  GtkTreeIter iter;
  GtkTreeIter iter_copy;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_INT);

  /* Check if move-after on a store with a single node does not corrupt
   * the store.
   */

  gtk_list_store_append (store, &iter);
  iter_copy = iter;

  gtk_list_store_move_after (store, &iter, NULL);
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter));
  g_assert (iters_equal (&iter, &iter_copy));

  gtk_list_store_move_after (store, &iter, &iter);
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter));
  g_assert (iters_equal (&iter, &iter_copy));

  g_object_unref (store);
}

/* move before */

static void
list_store_test_move_before_next (ListStore     *fixture,
		                  gconstpointer  user_data)
{
  /* We move node 3 before 2 */
  int new_order[5] = { 0, 1, 3, 2, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "3"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "2"));

  gtk_list_store_move_before (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_before_apart (ListStore     *fixture,
				   gconstpointer  user_data)
{
  /* We move node 1 before 3 */
  int new_order[5] = { 0, 2, 1, 3, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "1"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "3"));

  gtk_list_store_move_before (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_before_to_start (ListStore     *fixture,
				      gconstpointer  user_data)
{
  /* We move node 2 before 0 */
  int new_order[5] = { 2, 0, 1, 3, 4 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "2"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "0"));

  gtk_list_store_move_before (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_before_from_end (ListStore     *fixture,
			              gconstpointer  user_data)
{
  /* We move node 4 before 2 (replace end) */
  int new_order[5] = { 0, 1, 4, 2, 3 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "4"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "2"));

  gtk_list_store_move_before (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_before_change_ends (ListStore     *fixture,
				         gconstpointer  user_data)
{
  /* We move node 4 before 0 */
  int new_order[5] = { 4, 0, 1, 2, 3 };

  GtkTreeIter iter;
  GtkTreeIter position;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "4"));
  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &position, "0"));

  gtk_list_store_move_before (fixture->store, &iter, &position);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_before_NULL (ListStore     *fixture,
			          gconstpointer  user_data)
{
  /* We move node 2, NULL should append */
  int new_order[5] = { 0, 1, 3, 4, 2 };

  GtkTreeIter iter;

  g_assert (gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store), &iter, "2"));

  gtk_list_store_move_before (fixture->store, &iter, NULL);
  check_model (fixture, new_order, -1);
}

static void
list_store_test_move_before_single (void)
{
  GtkTreeIter iter;
  GtkTreeIter iter_copy;
  GtkListStore *store;

  store = gtk_list_store_new (1, G_TYPE_INT);

  /* Check if move-before on a store with a single node does not corrupt
   * the store.
   */

  gtk_list_store_append (store, &iter);
  iter_copy = iter;

  gtk_list_store_move_before (store, &iter, NULL);
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter));
  g_assert (iters_equal (&iter, &iter_copy));

  gtk_list_store_move_before (store, &iter, &iter);
  g_assert (iters_equal (&iter, &iter_copy));
  g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter));
  g_assert (iters_equal (&iter, &iter_copy));

  g_object_unref (store);
}


/* iter invalidation */

static void
list_store_test_iter_next_invalid (ListStore     *fixture,
                                   gconstpointer  user_data)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  path = gtk_tree_path_new_from_indices (4, -1);
  gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store), &iter, path);
  gtk_tree_path_free (path);

  g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL (fixture->store),
                                      &iter) == FALSE);
  g_assert (gtk_list_store_iter_is_valid (fixture->store, &iter) == FALSE);
  g_assert (iter.stamp == 0);
}

static void
list_store_test_iter_children_invalid (ListStore     *fixture,
                                       gconstpointer  user_data)
{
  GtkTreeIter iter, child;

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (fixture->store), &iter);
  g_assert (gtk_list_store_iter_is_valid (fixture->store, &iter) == TRUE);

  g_assert (gtk_tree_model_iter_children (GTK_TREE_MODEL (fixture->store),
                                          &child, &iter) == FALSE);
  g_assert (gtk_list_store_iter_is_valid (fixture->store, &child) == FALSE);
  g_assert (child.stamp == 0);
}

static void
list_store_test_iter_nth_child_invalid (ListStore     *fixture,
                                        gconstpointer  user_data)
{
  GtkTreeIter iter, child;

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (fixture->store), &iter);
  g_assert (gtk_list_store_iter_is_valid (fixture->store, &iter) == TRUE);

  g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (fixture->store),
                                           &child, &iter, 0) == FALSE);
  g_assert (gtk_list_store_iter_is_valid (fixture->store, &child) == FALSE);
  g_assert (child.stamp == 0);
}

static void
list_store_test_iter_parent_invalid (ListStore     *fixture,
                                     gconstpointer  user_data)
{
  GtkTreeIter iter, child;

  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (fixture->store), &child);
  g_assert (gtk_list_store_iter_is_valid (fixture->store, &child) == TRUE);

  g_assert (gtk_tree_model_iter_parent (GTK_TREE_MODEL (fixture->store),
                                        &iter, &child) == FALSE);
  g_assert (gtk_list_store_iter_is_valid (fixture->store, &iter) == FALSE);
  g_assert (iter.stamp == 0);
}


/* main */

int
main (int    argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  /* insertion */
  g_test_add_func ("/list-store/insert-high-values",
	           list_store_test_insert_high_values);
  g_test_add_func ("/list-store/append",
		   list_store_test_append);
  g_test_add_func ("/list-store/prepend",
		   list_store_test_prepend);
  g_test_add_func ("/list-store/insert-after",
		   list_store_test_insert_after);
  g_test_add_func ("/list-store/insert-after-NULL",
		   list_store_test_insert_after_NULL);
  g_test_add_func ("/list-store/insert-before",
		   list_store_test_insert_before);
  g_test_add_func ("/list-store/insert-before-NULL",
		   list_store_test_insert_before_NULL);

  /* setting values (FIXME) */

  /* removal */
  g_test_add ("/list-store/remove-begin", ListStore, NULL,
	      list_store_setup, list_store_test_remove_begin,
	      list_store_teardown);
  g_test_add ("/list-store/remove-middle", ListStore, NULL,
	      list_store_setup, list_store_test_remove_middle,
	      list_store_teardown);
  g_test_add ("/list-store/remove-end", ListStore, NULL,
	      list_store_setup, list_store_test_remove_end,
	      list_store_teardown);

  g_test_add ("/list-store/clear", ListStore, NULL,
	      list_store_setup, list_store_test_clear,
	      list_store_teardown);

  /* reordering */
  g_test_add ("/list-store/reorder", ListStore, NULL,
	      list_store_setup, list_store_test_reorder,
	      list_store_teardown);

  /* swapping */
  g_test_add ("/list-store/swap-begin", ListStore, NULL,
	      list_store_setup, list_store_test_swap_begin,
	      list_store_teardown);
  g_test_add ("/list-store/swap-middle-next", ListStore, NULL,
	      list_store_setup, list_store_test_swap_middle_next,
	      list_store_teardown);
  g_test_add ("/list-store/swap-middle-apart", ListStore, NULL,
	      list_store_setup, list_store_test_swap_middle_apart,
	      list_store_teardown);
  g_test_add ("/list-store/swap-end", ListStore, NULL,
	      list_store_setup, list_store_test_swap_end,
	      list_store_teardown);
  g_test_add_func ("/list-store/swap-single",
		   list_store_test_swap_single);

  /* moving */
  g_test_add ("/list-store/move-after-from-start", ListStore, NULL,
	      list_store_setup, list_store_test_move_after_from_start,
	      list_store_teardown);
  g_test_add ("/list-store/move-after-next", ListStore, NULL,
	      list_store_setup, list_store_test_move_after_next,
	      list_store_teardown);
  g_test_add ("/list-store/move-after-apart", ListStore, NULL,
	      list_store_setup, list_store_test_move_after_apart,
	      list_store_teardown);
  g_test_add ("/list-store/move-after-end", ListStore, NULL,
	      list_store_setup, list_store_test_move_after_end,
	      list_store_teardown);
  g_test_add ("/list-store/move-after-from-end", ListStore, NULL,
	      list_store_setup, list_store_test_move_after_from_end,
	      list_store_teardown);
  g_test_add ("/list-store/move-after-change-ends", ListStore, NULL,
	      list_store_setup, list_store_test_move_after_change_ends,
	      list_store_teardown);
  g_test_add ("/list-store/move-after-NULL", ListStore, NULL,
	      list_store_setup, list_store_test_move_after_NULL,
	      list_store_teardown);
  g_test_add_func ("/list-store/move-after-single",
		   list_store_test_move_after_single);

  g_test_add ("/list-store/move-before-next", ListStore, NULL,
	      list_store_setup, list_store_test_move_before_next,
	      list_store_teardown);
  g_test_add ("/list-store/move-before-apart", ListStore, NULL,
	      list_store_setup, list_store_test_move_before_apart,
	      list_store_teardown);
  g_test_add ("/list-store/move-before-to-start", ListStore, NULL,
	      list_store_setup, list_store_test_move_before_to_start,
	      list_store_teardown);
  g_test_add ("/list-store/move-before-from-end", ListStore, NULL,
	      list_store_setup, list_store_test_move_before_from_end,
	      list_store_teardown);
  g_test_add ("/list-store/move-before-change-ends", ListStore, NULL,
	      list_store_setup, list_store_test_move_before_change_ends,
	      list_store_teardown);
  g_test_add ("/list-store/move-before-NULL", ListStore, NULL,
	      list_store_setup, list_store_test_move_before_NULL,
	      list_store_teardown);
  g_test_add_func ("/list-store/move-before-single",
		   list_store_test_move_before_single);

  /* iter invalidation */
  g_test_add ("/list-store/iter-next-invalid", ListStore, NULL,
              list_store_setup, list_store_test_iter_next_invalid,
              list_store_teardown);
  g_test_add ("/list-store/iter-children-invalid", ListStore, NULL,
              list_store_setup, list_store_test_iter_children_invalid,
              list_store_teardown);
  g_test_add ("/list-store/iter-nth-child-invalid", ListStore, NULL,
              list_store_setup, list_store_test_iter_nth_child_invalid,
              list_store_teardown);
  g_test_add ("/list-store/iter-parent-invalid", ListStore, NULL,
              list_store_setup, list_store_test_iter_parent_invalid,
              list_store_teardown);

  return g_test_run ();
}
