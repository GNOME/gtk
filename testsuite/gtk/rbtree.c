/* GtkRBTree tests.
 *
 * Copyright (C) 2011, Red Hat, Inc.
 * Authors: Benjamin Otte <otte@gnome.org>
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

#include <locale.h>

#include "../../gtk/gtkrbtree.h"

/* _gtk_rbtree_test */

static guint
get_total_count (GtkRBNode *node)
{
  guint child_total = 0;

  child_total += (guint) node->left->total_count;
  child_total += (guint) node->right->total_count;

  if (node->children)
    child_total += (guint) node->children->root->total_count;

  return child_total + 1;
}

static guint
count_total (GtkRBTree *tree,
             GtkRBNode *node)
{
  guint res;
  
  if (_gtk_rbtree_is_nil (node))
    return 0;
  
  res =
    count_total (tree, node->left) +
    count_total (tree, node->right) +
    (guint)1 +
    (node->children ? count_total (node->children, node->children->root) : 0);

  if (res != node->total_count)
    g_print ("total count incorrect for node\n");

  if (get_total_count (node) != node->total_count)
    g_error ("Node has incorrect total count %u, should be %u", node->total_count, get_total_count (node));
  
  return res;
}

static gint
_count_nodes (GtkRBTree *tree,
              GtkRBNode *node)
{
  gint res;
  if (_gtk_rbtree_is_nil (node))
    return 0;

  g_assert (node->left);
  g_assert (node->right);

  res = (_count_nodes (tree, node->left) +
         _count_nodes (tree, node->right) + 1);

  if (res != node->count)
    g_print ("Tree failed\n");
  return res;
}

static void
_gtk_rbtree_test_height (GtkRBTree *tree,
                         GtkRBNode *node)
{
  gint computed_offset = 0;

  /* This whole test is sort of a useless truism. */
  
  if (!_gtk_rbtree_is_nil (node->left))
    computed_offset += node->left->offset;

  if (!_gtk_rbtree_is_nil (node->right))
    computed_offset += node->right->offset;

  if (node->children && !_gtk_rbtree_is_nil (node->children->root))
    computed_offset += node->children->root->offset;

  if (GTK_RBNODE_GET_HEIGHT (node) + computed_offset != node->offset)
    g_error ("node has broken offset");

  if (!_gtk_rbtree_is_nil (node->left))
    _gtk_rbtree_test_height (tree, node->left);

  if (!_gtk_rbtree_is_nil (node->right))
    _gtk_rbtree_test_height (tree, node->right);

  if (node->children && !_gtk_rbtree_is_nil (node->children->root))
    _gtk_rbtree_test_height (node->children, node->children->root);
}

static void
_gtk_rbtree_test_dirty (GtkRBTree *tree,
			GtkRBNode *node,
			 gint      expected_dirtyness)
{

  if (expected_dirtyness)
    {
      g_assert (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID) ||
		GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID) ||
		GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID) ||
		GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID) ||
		(node->children && GTK_RBNODE_FLAG_SET (node->children->root, GTK_RBNODE_DESCENDANTS_INVALID)));
    }
  else
    {
      g_assert (! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID) &&
		! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID));
      if (!_gtk_rbtree_is_nil (node->left))
	g_assert (! GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID));
      if (!_gtk_rbtree_is_nil (node->right))
	g_assert (! GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID));
      if (node->children != NULL)
	g_assert (! GTK_RBNODE_FLAG_SET (node->children->root, GTK_RBNODE_DESCENDANTS_INVALID));
    }

  if (!_gtk_rbtree_is_nil (node->left))
    _gtk_rbtree_test_dirty (tree, node->left, GTK_RBNODE_FLAG_SET (node->left, GTK_RBNODE_DESCENDANTS_INVALID));
  if (!_gtk_rbtree_is_nil (node->right))
    _gtk_rbtree_test_dirty (tree, node->right, GTK_RBNODE_FLAG_SET (node->right, GTK_RBNODE_DESCENDANTS_INVALID));
  if (node->children != NULL && !_gtk_rbtree_is_nil (node->children->root))
    _gtk_rbtree_test_dirty (node->children, node->children->root, GTK_RBNODE_FLAG_SET (node->children->root, GTK_RBNODE_DESCENDANTS_INVALID));
}

static void _gtk_rbtree_test_structure (GtkRBTree *tree);

static guint
_gtk_rbtree_test_structure_helper (GtkRBTree *tree,
				   GtkRBNode *node)
{
  guint left_blacks, right_blacks;

  g_assert (!_gtk_rbtree_is_nil (node));

  g_assert (node->left != NULL);
  g_assert (node->right != NULL);
  g_assert (node->parent != NULL);

  if (!_gtk_rbtree_is_nil (node->left))
    {
      g_assert (node->left->parent == node);
      left_blacks = _gtk_rbtree_test_structure_helper (tree, node->left);
    }
  else
    left_blacks = 0;

  if (!_gtk_rbtree_is_nil (node->right))
    {
      g_assert (node->right->parent == node);
      right_blacks = _gtk_rbtree_test_structure_helper (tree, node->right);
    }
  else
    right_blacks = 0;

  if (node->children != NULL)
    {
      g_assert (node->children->parent_tree == tree);
      g_assert (node->children->parent_node == node);

      _gtk_rbtree_test_structure (node->children);
    }

  g_assert (left_blacks == right_blacks);

  return left_blacks + (GTK_RBNODE_GET_COLOR (node) == GTK_RBNODE_BLACK ? 1 : 0);
}

static void
_gtk_rbtree_test_structure (GtkRBTree *tree)
{
  g_assert (tree->root);
  if (_gtk_rbtree_is_nil (tree->root))
    return;

  g_assert (_gtk_rbtree_is_nil (tree->root->parent));
  _gtk_rbtree_test_structure_helper (tree, tree->root);
}

static void
_gtk_rbtree_test (GtkRBTree *tree)
{
  GtkRBTree *tmp_tree;

  if (tree == NULL)
    return;

  /* Test the entire tree */
  tmp_tree = tree;
  while (tmp_tree->parent_tree)
    tmp_tree = tmp_tree->parent_tree;
  
  if (_gtk_rbtree_is_nil (tmp_tree->root))
    return;

  _gtk_rbtree_test_structure (tmp_tree);

  g_assert ((_count_nodes (tmp_tree, tmp_tree->root->left) +
	     _count_nodes (tmp_tree, tmp_tree->root->right) + 1) == tmp_tree->root->count);
      
  _gtk_rbtree_test_height (tmp_tree, tmp_tree->root);
  _gtk_rbtree_test_dirty (tmp_tree, tmp_tree->root, GTK_RBNODE_FLAG_SET (tmp_tree->root, GTK_RBNODE_DESCENDANTS_INVALID));
  g_assert (count_total (tmp_tree, tmp_tree->root) == tmp_tree->root->total_count);
}

/* gtk_rbtree_print() - unused, for debugging only */

static void
gtk_rbtree_print_node (GtkRBTree *tree,
                       GtkRBNode *node,
                       gint       depth)
{
  gint i;
  for (i = 0; i < depth; i++)
    g_print ("\t");

  g_print ("(%p - %s) (Offset %d) (Parity %d) (Validity %d%d%d)\n",
	   node,
	   (GTK_RBNODE_GET_COLOR (node) == GTK_RBNODE_BLACK)?"BLACK":" RED ",
	   node->offset,
	   node->total_count,
	   (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_DESCENDANTS_INVALID))?1:0,
	   (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_INVALID))?1:0,
	   (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_COLUMN_INVALID))?1:0);
  if (node->children != NULL)
    {
      g_print ("Looking at child.\n");
      gtk_rbtree_print_node (node->children, node->children->root, depth + 1);
      g_print ("Done looking at child.\n");
    }
  if (!_gtk_rbtree_is_nil (node->left))
    {
      gtk_rbtree_print_node (tree, node->left, depth+1);
    }
  if (!_gtk_rbtree_is_nil (node->right))
    {
      gtk_rbtree_print_node (tree, node->right, depth+1);
    }
}

/* not static so the debugger finds it. */
void gtk_rbtree_print (GtkRBTree *tree);

void
gtk_rbtree_print (GtkRBTree *tree)
{
  g_return_if_fail (tree != NULL);

  if (_gtk_rbtree_is_nil (tree->root))
    g_print ("Empty tree...\n");
  else
    gtk_rbtree_print_node (tree, tree->root, 0);
}

/* actual tests */

static guint
append_elements (GtkRBTree *tree,
                 guint      depth,
                 guint      elements_per_depth,
                 gboolean   check,
                 guint      height)
{
  GtkRBNode *node;
  guint i;

  g_assert (depth > 0);

  node = NULL;
  depth--;

  for (i = 0; i < elements_per_depth; i++)
    {
      node = _gtk_rbtree_insert_after (tree, node, ++height, TRUE);
      if (depth)
        {
          node->children = _gtk_rbtree_new ();
          node->children->parent_tree = tree;
          node->children->parent_node = node;
          height = append_elements (node->children, depth, elements_per_depth, check, height);
        }
      if (check)
        _gtk_rbtree_test (tree);
    }

  return height;
}

static GtkRBTree *
create_rbtree (guint depth,
               guint elements_per_depth,
               gboolean check)
{
  GtkRBTree *tree;

  tree = _gtk_rbtree_new ();

  append_elements (tree, depth, elements_per_depth, check, 0);

  _gtk_rbtree_test (tree);

  return tree;
}

static void
test_create (void)
{
  GtkRBTree *tree;

  tree = create_rbtree (5, 5, TRUE);

  _gtk_rbtree_free (tree);
}

static void
test_insert_after (void)
{
  guint i;
  GtkRBTree *tree;
  GtkRBNode *node;

  tree = _gtk_rbtree_new ();
  node = NULL;

  for (i = 1; i <= 100; i++)
    {
      node = _gtk_rbtree_insert_after (tree, node, i, TRUE);
      _gtk_rbtree_test (tree);
      g_assert (tree->root->count == i);
      g_assert (tree->root->total_count == i);
      g_assert (tree->root->offset == i * (i + 1) / 2);
    }

  _gtk_rbtree_free (tree);
}

static void
test_insert_before (void)
{
  guint i;
  GtkRBTree *tree;
  GtkRBNode *node;

  tree = _gtk_rbtree_new ();
  node = NULL;

  for (i = 1; i <= 100; i++)
    {
      node = _gtk_rbtree_insert_before (tree, node, i, TRUE);
      _gtk_rbtree_test (tree);
      g_assert (tree->root->count == i);
      g_assert (tree->root->total_count == i);
      g_assert (tree->root->offset == i * (i + 1) / 2);
    }

  _gtk_rbtree_free (tree);
}

static void
test_remove_node (void)
{
  GtkRBTree *tree;

  tree = create_rbtree (3, 16, g_test_thorough ());

  while (tree->root->count > 1)
    {
      GtkRBTree *find_tree;
      GtkRBNode *find_node;
      guint i;
      
      i = g_test_rand_int_range (0, tree->root->total_count);
      if (!_gtk_rbtree_find_index (tree, i, &find_tree, &find_node))
        {
          /* We search an available index, so we mustn't fail. */
          g_assert_not_reached ();
        }
      
      _gtk_rbtree_test (find_tree);

      if (find_tree->root->count == 1)
        {
          _gtk_rbtree_remove (find_tree);
        }
      else
        _gtk_rbtree_remove_node (find_tree, find_node);
      _gtk_rbtree_test (tree);
    }

  _gtk_rbtree_free (tree);
}

static void
test_remove_root (void)
{
  GtkRBTree *tree;
  GtkRBNode *node;

  tree = _gtk_rbtree_new ();
  
  node = _gtk_rbtree_insert_after (tree, NULL, 1, TRUE);
  _gtk_rbtree_insert_after (tree, node, 2, TRUE);
  _gtk_rbtree_insert_before (tree, node, 3, TRUE);

  _gtk_rbtree_remove_node (tree, node);

  _gtk_rbtree_free (tree);
}

static gint *
fisher_yates_shuffle (guint n_items)
{
  gint *list;
  guint i, j;

  list = g_new (gint, n_items);

  for (i = 0; i < n_items; i++)
    {
      j = g_random_int_range (0, i + 1);
      list[i] = list[j];
      list[j] = i;
    }

  return list;
}

static GtkRBTree *
create_unsorted_tree (gint  *order,
                      guint  n)
{
  GtkRBTree *tree;
  GtkRBNode *node;
  guint i;

  tree = _gtk_rbtree_new ();
  node = NULL;

  for (i = 0; i < n; i++)
    {
      node = _gtk_rbtree_insert_after (tree, node, 0, TRUE);
    }

  for (i = 0; i < n; i++)
    {
      node = _gtk_rbtree_find_count (tree, order[i] + 1);
      _gtk_rbtree_node_set_height (tree, node, i);
    }

  _gtk_rbtree_test (tree);

  return tree;
}

static void
test_reorder (void)
{
  guint n = g_test_perf () ? 1000000 : 100;
  GtkRBTree *tree;
  GtkRBNode *node;
  gint *reorder;
  guint i;
  double elapsed;

  reorder = fisher_yates_shuffle (n);
  tree = create_unsorted_tree (reorder, n);

  g_test_timer_start ();

  _gtk_rbtree_reorder (tree, reorder, n);

  elapsed = g_test_timer_elapsed ();
  if (g_test_perf ())
    g_test_minimized_result (elapsed, "reordering rbtree with %u items: %gsec", n, elapsed);

  _gtk_rbtree_test (tree);

  for (node = _gtk_rbtree_first (tree), i = 0;
       node != NULL;
       node = _gtk_rbtree_next (tree, node), i++)
    {
      g_assert (GTK_RBNODE_GET_HEIGHT (node) == i);
    }
  g_assert (i == n);

  _gtk_rbtree_free (tree);

  g_free (reorder);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);
  setlocale (LC_ALL, "C");
  g_test_bug_base ("http://bugzilla.gnome.org/show_bug.cgi?id=%s");

  g_test_add_func ("/rbtree/create", test_create);
  g_test_add_func ("/rbtree/insert_after", test_insert_after);
  g_test_add_func ("/rbtree/insert_before", test_insert_before);
  g_test_add_func ("/rbtree/remove_node", test_remove_node);
  g_test_add_func ("/rbtree/remove_root", test_remove_root);
  g_test_add_func ("/rbtree/reorder", test_reorder);

  return g_test_run ();
}
