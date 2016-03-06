/* gtkrbtree.c
 * Copyright (C) 2000  Red Hat, Inc.,  Jonathan Blandford <jrb@redhat.com>
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

#include "config.h"

#include "gtkcssrbtreeprivate.h"

#include "gtkdebug.h"

typedef struct _GtkCssRbNode GtkCssRbNode;

struct _GtkCssRbTree
{
  guint ref_count;

  gsize element_size;
  gsize augment_size;
  GtkCssRbTreeAugmentFunc augment_func;
  GDestroyNotify clear_func;
  GDestroyNotify clear_augment_func;

  GtkCssRbNode *root;
};

struct _GtkCssRbNode
{
  guint red :1;
  guint dirty :1;

  GtkCssRbNode *left;
  GtkCssRbNode *right;
  GtkCssRbNode *parent;
};

#define NODE_FROM_POINTER(ptr) ((GtkCssRbNode *) ((ptr) ? (((guchar *) (ptr)) - sizeof (GtkCssRbNode)) : NULL))
#define NODE_TO_POINTER(node) ((gpointer) ((node) ? (((guchar *) (node)) + sizeof (GtkCssRbNode)) : NULL))
#define NODE_TO_AUG_POINTER(tree, node) ((gpointer) ((node) ? (((guchar *) (node)) + sizeof (GtkCssRbNode) + (tree)->element_size) : NULL))

static inline gsize
gtk_css_rb_node_get_size (GtkCssRbTree *tree)
{
  return sizeof (GtkCssRbNode) + tree->element_size + tree->augment_size;
}

static GtkCssRbNode *
gtk_css_rb_node_new (GtkCssRbTree *tree)
{
  GtkCssRbNode *result;

  result = g_slice_alloc0 (gtk_css_rb_node_get_size (tree));

  result->red = TRUE;
  result->dirty = TRUE;

  return result;
}

static void
gtk_css_rb_node_free (GtkCssRbTree *tree,
                      GtkCssRbNode *node)
{
  if (tree->clear_func)
    tree->clear_func (NODE_TO_POINTER (node));
  if (tree->clear_augment_func)
    tree->clear_augment_func (NODE_TO_AUG_POINTER (tree, node));

  g_slice_free1 (gtk_css_rb_node_get_size (tree), node);
}

static void
gtk_css_rb_node_free_deep (GtkCssRbTree *tree,
                           GtkCssRbNode *node)
{
  GtkCssRbNode *right = node->right;

  if (node->left)
    gtk_css_rb_node_free_deep (tree, node->left);

  gtk_css_rb_node_free (tree, node);

  if (right)
    gtk_css_rb_node_free_deep (tree, right);
}

static void
gtk_css_rb_node_mark_dirty (GtkCssRbNode *node,
                            gboolean      mark_parent)
{
  if (node->dirty)
    return;
  
  node->dirty = TRUE;

  if (mark_parent && node->parent)
    gtk_css_rb_node_mark_dirty (node->parent, TRUE);
}

static void
gtk_css_rb_node_clean (GtkCssRbTree *tree,
                       GtkCssRbNode *node)
{
  if (!node->dirty)
    return;

  node->dirty = FALSE;
  if (tree->augment_func)
    tree->augment_func (tree,
                        NODE_TO_AUG_POINTER (tree, node),
                        NODE_TO_POINTER (node),
                        NODE_TO_POINTER (node->left),
                        NODE_TO_POINTER (node->right));
}

static GtkCssRbNode *
gtk_css_rb_node_get_first (GtkCssRbNode *node)
{
  while (node->left)
    node = node->left;

  return node;
}

static GtkCssRbNode *
gtk_css_rb_node_get_last (GtkCssRbNode *node)
{
  while (node->right)
    node = node->right;

  return node;
}

static GtkCssRbNode *
gtk_css_rb_node_get_previous (GtkCssRbNode *node)
{
  GtkCssRbNode *parent;

  if (node->left)
    return gtk_css_rb_node_get_last (node->left);

  for (parent = node->parent; parent != NULL; parent = node->parent)
    {
      if (parent->right == node)
        return parent;

      node = parent;
    }

  return NULL;
}

static GtkCssRbNode *
gtk_css_rb_node_get_next (GtkCssRbNode *node)
{
  GtkCssRbNode *parent;

  if (node->right)
    return gtk_css_rb_node_get_first (node->right);

  for (parent = node->parent; parent != NULL; parent = node->parent)
    {
      if (parent->left == node)
        return parent;

      node = parent;
    }

  return NULL;
}

static void
gtk_css_rb_node_rotate_left (GtkCssRbTree *tree,
                             GtkCssRbNode *node)
{
  GtkCssRbNode *right;

  right = node->right;

  node->right = right->left;
  if (right->left)
    right->left->parent = node;

  right->parent = node->parent;
  if (node->parent)
    {
      if (node == node->parent->left)
	node->parent->left = right;
      else
	node->parent->right = right;
    }
  else
    {
      tree->root = right;
    }

  right->left = node;
  node->parent = right;

  gtk_css_rb_node_mark_dirty (node, FALSE);
  gtk_css_rb_node_mark_dirty (right, FALSE);
}

static void
gtk_css_rb_node_rotate_right (GtkCssRbTree *tree,
                              GtkCssRbNode *node)
{
  GtkCssRbNode *left;

  left = node->left;

  node->left = left->right;
  if (left->right)
    left->right->parent = node;

  left->parent = node->parent;
  if (node->parent)
    {
      if (node == node->parent->right)
	node->parent->right = left;
      else
	node->parent->left = left;
    }
  else
    {
      tree->root = left;
    }

  /* link node and left */
  left->right = node;
  node->parent = left;

  gtk_css_rb_node_mark_dirty (node, FALSE);
  gtk_css_rb_node_mark_dirty (left, FALSE);
}

static gboolean
is_red (GtkCssRbNode *node_or_null)
{
  if (node_or_null == NULL)
    return FALSE;
  else
    return node_or_null->red;
}

static inline gboolean
is_black (GtkCssRbNode *node_or_null)
{
  return !is_red (node_or_null);
}

static void
set_black (GtkCssRbNode *node_or_null)
{
  if (node_or_null == NULL)
    return;

  node_or_null->red = FALSE;
}

static void
set_red (GtkCssRbNode *node_or_null)
{
  if (node_or_null == NULL)
    return;

  node_or_null->red = TRUE;
}

static void
gtk_css_rb_tree_insert_fixup (GtkCssRbTree *tree,
                              GtkCssRbNode *node)
{

  /* check Red-Black properties */
  while (node->parent && is_red (node->parent))
    {
      /* we have a violation */
      g_assert (node->parent->parent);

      if (node->parent == node->parent->parent->left)
	{
	  GtkCssRbNode *uncle = node->parent->parent->right;

	  if (is_red (uncle))
	    {
	      /* uncle is red */
	      set_black (node->parent);
              set_black (uncle);
              set_red (node->parent->parent);
	      node = node->parent->parent;
	    }
	  else
	    {
	      /* uncle is black */
	      if (node == node->parent->right)
		{
		  /* make node a left child */
		  node = node->parent;
		  gtk_css_rb_node_rotate_left (tree, node);
		}
	      /* recolor and rotate */
              set_black (node->parent);
              set_red (node->parent->parent);
	      gtk_css_rb_node_rotate_right (tree, node->parent->parent);
	    }
	}
      else
	{
	  /* mirror image of above code */
	  GtkCssRbNode *uncle = node->parent->parent->left;

	  if (is_red (uncle))
	    {
	      /* uncle is red */
              set_black (node->parent);
              set_black (uncle);
              set_red (node->parent->parent);
	      node = node->parent->parent;
	    }
	  else
	    {
              /* uncle is black */
	      if (node == node->parent->left)
		{
		  node = node->parent;
		  gtk_css_rb_node_rotate_right (tree, node);
		}
	      set_black (node->parent);
	      set_red (node->parent->parent);
	      gtk_css_rb_node_rotate_left (tree, node->parent->parent);
	    }
	}
    }

  set_black (tree->root);
}

static void
gtk_css_rb_tree_remove_node_fixup (GtkCssRbTree *tree,
                                   GtkCssRbNode *node,
                                   GtkCssRbNode *parent)
{
  while (node->parent && is_black (node))
    {
      if (node == parent->left)
	{
	  GtkCssRbNode *w = parent->right;

	  if (is_red (w))
	    {
	      set_black (w);
              set_red (parent);
	      gtk_css_rb_node_rotate_left (tree, parent);
	      w = parent->right;
	    }
	  if (is_black (w->left) && is_black (w->right))
	    {
	      set_red (w);
	      node = parent;
	    }
	  else
	    {
	      if (is_black (w->right))
		{
		  set_black (w->left);
		  set_red (w);
		  gtk_css_rb_node_rotate_right (tree, w);
		  w = parent->right;
		}
	      w->red = parent->red;
	      set_black (parent);
              set_black (w->right);
	      gtk_css_rb_node_rotate_left (tree, parent);
	      node = tree->root;
	    }
	}
      else
	{
	  GtkCssRbNode *w = parent->left;
	  if (is_red (w))
	    {
	      set_black (w);
	      set_red (parent);
	      gtk_css_rb_node_rotate_right (tree, parent);
	      w = parent->left;
	    }
	  if (is_black (w->right) && is_black (w->left))
	    {
	      set_red (w);
	      node = parent;
	    }
	  else
	    {
	      if (is_black (w->left))
		{
		  set_black (w->right);
		  set_red (w);
		  gtk_css_rb_node_rotate_left (tree, w);
		  w = parent->left;
		}
	      w->red = parent->red;
	      set_black (parent);
	      set_black (w->left);
	      gtk_css_rb_node_rotate_right (tree, parent);
	      node = tree->root;
	    }
	}

      parent = node->parent;
    }

  set_black (node);
}

GtkCssRbTree *
gtk_css_rb_tree_new_for_size (gsize                   element_size,
                              gsize                   augment_size,
                              GtkCssRbTreeAugmentFunc augment_func,
                              GDestroyNotify          clear_func,
                              GDestroyNotify          clear_augment_func)
{
  GtkCssRbTree *tree;

  tree = g_slice_new0 (GtkCssRbTree);
  tree->ref_count = 1;

  tree->element_size = element_size;
  tree->augment_size = augment_size;
  tree->augment_func = augment_func;
  tree->clear_func = clear_func;
  tree->clear_augment_func = clear_augment_func;

  return tree;
}

GtkCssRbTree *
gtk_css_rb_tree_ref (GtkCssRbTree *tree)
{
  tree->ref_count++;

  return tree;
}

void
gtk_css_rb_tree_unref (GtkCssRbTree *tree)
{
  tree->ref_count--;
  if (tree->ref_count > 0)
    return;

  if (tree->root)
    gtk_css_rb_node_free_deep (tree, tree->root);
    
  g_slice_free (GtkCssRbTree, tree);
}

gpointer
gtk_css_rb_tree_get_first (GtkCssRbTree *tree)
{
  if (tree->root == NULL)
    return NULL;

  return NODE_TO_POINTER (gtk_css_rb_node_get_first (tree->root));
}

gpointer
gtk_css_rb_tree_get_last (GtkCssRbTree *tree)
{
  if (tree->root == NULL)
    return NULL;

  return NODE_TO_POINTER (gtk_css_rb_node_get_last (tree->root));
}

gpointer
gtk_css_rb_tree_get_previous (GtkCssRbTree *tree,
                              gpointer      node)
{
  return NODE_TO_POINTER (gtk_css_rb_node_get_previous (NODE_FROM_POINTER (node)));
}

gpointer
gtk_css_rb_tree_get_next (GtkCssRbTree *tree,
                          gpointer      node)
{
  return NODE_TO_POINTER (gtk_css_rb_node_get_next (NODE_FROM_POINTER (node)));
}

gpointer
gtk_css_rb_tree_get_root (GtkCssRbTree *tree)
{
  return NODE_TO_POINTER (tree->root);
}

gpointer
gtk_css_rb_tree_get_parent (GtkCssRbTree *tree,
                            gpointer      node)
{
  return NODE_TO_POINTER (NODE_FROM_POINTER (node)->parent);
}

gpointer
gtk_css_rb_tree_get_left (GtkCssRbTree *tree,
                          gpointer      node)
{
  return NODE_TO_POINTER (NODE_FROM_POINTER (node)->left);
}

gpointer
gtk_css_rb_tree_get_right (GtkCssRbTree *tree,
                           gpointer      node)
{
  return NODE_TO_POINTER (NODE_FROM_POINTER (node)->right);
}

gpointer
gtk_css_rb_tree_get_augment (GtkCssRbTree *tree,
                             gpointer      node)
{
  GtkCssRbNode *rbnode = NODE_FROM_POINTER (node);

  gtk_css_rb_node_clean (tree, rbnode);

  return NODE_TO_AUG_POINTER (tree, rbnode);
}

void
gtk_css_rb_tree_mark_dirty (GtkCssRbTree *tree,
                            gpointer      node)
{
  gtk_css_rb_node_mark_dirty (NODE_FROM_POINTER (node), TRUE);
}

gpointer
gtk_css_rb_tree_insert_before (GtkCssRbTree *tree,
                              gpointer      node)
{
  GtkCssRbNode *result;

  /* setup new node */
  result = gtk_css_rb_node_new (tree);

  if (tree->root == NULL)
    {
      g_assert (node == NULL);
      tree->root = result;
    }
  else if (node == NULL)
    {
      return gtk_css_rb_tree_insert_after (tree, gtk_css_rb_tree_get_last (tree));
    }
  else
    {
      GtkCssRbNode *current = NODE_FROM_POINTER (node);

      if (current->left)
        {
          current = gtk_css_rb_node_get_last (current);
          current->right = result;
        }
      else
        {
          current->left = result;
        }
      result->parent = current;
      gtk_css_rb_node_mark_dirty (current, TRUE);
    }

  gtk_css_rb_tree_insert_fixup (tree, result);

  return NODE_TO_POINTER (result);
}

gpointer
gtk_css_rb_tree_insert_after (GtkCssRbTree *tree,
                             gpointer      node)
{
  GtkCssRbNode *result;

  /* setup new node */
  result = gtk_css_rb_node_new (tree);

  if (tree->root == NULL)
    {
      g_assert (node == NULL);
      tree->root = result;
    }
  else if (node == NULL)
    {
      return gtk_css_rb_tree_insert_before (tree, gtk_css_rb_tree_get_first (tree));
    }
  else
    {
      GtkCssRbNode *current = NODE_FROM_POINTER (node);

      if (current->right )
        {
          current = gtk_css_rb_node_get_first (current);
          current->left = result;
        }
      else
        {
          current->right = result;
        }
      result->parent = current;
      gtk_css_rb_node_mark_dirty (current, TRUE);
    }

  gtk_css_rb_tree_insert_fixup (tree, result);

  return NODE_TO_POINTER (result);
}

void
gtk_css_rb_tree_remove (GtkCssRbTree *tree,
                        gpointer      node)
{
  GtkCssRbNode *x, *y, *real_node;
  
  real_node = NODE_FROM_POINTER (node);
  y = real_node;
  if (y->left && y->right)
    {
      y = y->right;

      while (y->left)
	y = y->left;
    }

  /* x is y's only child, or nil */
  if (y->left)
    x = y->left;
  else
    x = y->right;

  /* remove y from the parent chain */
  if (x != NULL)
    x->parent = y->parent;
  if (y->parent)
    {
      if (y == y->parent->left)
	y->parent->left = x;
      else
	y->parent->right = x;
    }
  else
    {
      tree->root = x;
    }

  /* We need to clean up the validity of the tree.
   */
  if (x && is_black (y))
    gtk_css_rb_tree_remove_node_fixup (tree, x, y->parent);

  if (y != real_node)
    {
      /* Move the node over */
      if (is_red (real_node) != is_red (y))
	y->red = !y->red;

      y->left = real_node->left;
      if (y->left)
        y->left->parent = y;
      y->right = real_node->right;
      if (y->right)
        y->right->parent = y;
      y->parent = real_node->parent;
      if (y->parent)
        {
          if (y->parent->left == real_node)
            y->parent->left = y;
          else
            y->parent->right = y;
        }
      else
        {
          tree->root = y;
        }
    }

  gtk_css_rb_node_free (tree, real_node);
}


gpointer
gtk_css_rb_tree_find (GtkCssRbTree           *tree,
                      gpointer               *out_before,
                      gpointer               *out_after,
                      GtkCssRbTreeFindFunc    find_func,
                      gpointer                user_data)
{
  GtkCssRbNode *node, *before = NULL, *after = NULL;
  int cmp;

  if (tree->root == NULL)
    {
      if (out_before)
        *out_before = NULL;
      if (out_after)
        *out_after = NULL;

      return NULL;
    }

  node = tree->root;
  for (cmp = find_func (tree, NODE_TO_POINTER (node), user_data);
       cmp != 0;
       cmp = find_func (tree, NODE_TO_POINTER (node), user_data))
    {
      if (cmp < 0)
        {
          before = node;
          node = node->right;
        }
      else /* cmp > 0 */
        {
          after = node;
          node = node->left;
        }
      if (node == NULL)
        {
          if (out_before)
            *out_before = NODE_TO_POINTER (before);
          if (out_after)
            *out_after = NODE_TO_POINTER (after);
          return NULL;;
        }
    }

  if (out_before)
    *out_before = NODE_TO_POINTER (gtk_css_rb_node_get_previous (node));
  if (out_after)
    *out_after = NODE_TO_POINTER (gtk_css_rb_node_get_next (node));

  return NODE_TO_POINTER (node);
}
