/* gtktreeselection.h
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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "gtktreeselection.h"
#include "gtktreeprivate.h"
#include "gtkrbtree.h"
#include "gtksignal.h"

static void     gtk_tree_selection_init           (GtkTreeSelection       *selection);
static void     gtk_tree_selection_class_init     (GtkTreeSelectionClass  *class);

enum {
  ROW_SELECTED,
  ROW_UNSELECTED,
  LAST_SIGNAL
};

static GtkObjectClass *parent_class = NULL;
static guint tree_selection_signals[LAST_SIGNAL] = { 0 };

static void
gtk_tree_selection_real_select_node (GtkTreeSelection *selection, GtkRBTree *tree, GtkRBNode *node, gboolean select)
{
  gboolean selected = FALSE;
  GtkTreePath *path = NULL;

  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED) != select)
    {
      path = _gtk_tree_view_find_path (selection->tree_view, tree, node);
      if (selection->user_func)
	{
	  if ((*selection->user_func) (selection, selection->tree_view->priv->model, path, selection->user_data))
	    selected = TRUE;
	}
      else
	selected = TRUE;
    }
  if (selected == TRUE)
    {
      GtkTreeNode tree_node;
      tree_node = gtk_tree_model_get_node (selection->tree_view->priv->model, path);

      node->flags ^= GTK_RBNODE_IS_SELECTED;
      if (select)
	gtk_signal_emit (GTK_OBJECT (selection), tree_selection_signals[ROW_SELECTED], selection->tree_view->priv->model, tree_node);
      else
	gtk_signal_emit (GTK_OBJECT (selection), tree_selection_signals[ROW_UNSELECTED], selection->tree_view->priv->model, tree_node);
      gtk_widget_queue_draw (GTK_WIDGET (selection->tree_view));
    }
}

GtkType
gtk_tree_selection_get_type (void)
{
  static GtkType selection_type = 0;

  if (!selection_type)
    {
      static const GTypeInfo selection_info =
      {
        sizeof (GtkTreeSelectionClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_tree_selection_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkTreeSelection),
	0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_tree_selection_init
      };

      selection_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkTreeSelection", &selection_info);
    }

  return selection_type;
}

static void
gtk_tree_selection_class_init (GtkTreeSelectionClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;
  parent_class = g_type_class_peek_parent (class);

  tree_selection_signals[ROW_SELECTED] =
    gtk_signal_new ("row_selected",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeSelectionClass, row_selected),
		    gtk_marshal_NONE__POINTER_POINTER,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);

  tree_selection_signals[ROW_UNSELECTED] =
    gtk_signal_new ("row_unselected",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeSelectionClass, row_unselected),
		    gtk_marshal_NONE__POINTER_POINTER,
		    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);

  gtk_object_class_add_signals (object_class, tree_selection_signals, LAST_SIGNAL);

  class->row_selected = NULL;
  class->row_unselected = NULL;
}

static void
gtk_tree_selection_init (GtkTreeSelection *selection)
{
  selection->type = GTK_TREE_SELECTION_MULTI;
  selection->user_func = NULL;
  selection->user_data = NULL;
  selection->user_func = NULL;
  selection->tree_view = NULL;
}

GtkObject *
gtk_tree_selection_new (void)
{
  GtkObject *selection;

  selection = GTK_OBJECT (gtk_type_new (GTK_TYPE_TREE_SELECTION));

  return selection;
}

GtkObject *
gtk_tree_selection_new_with_tree_view (GtkTreeView *tree_view)
{
  GtkObject *selection;

  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  selection = gtk_tree_selection_new ();
  gtk_tree_selection_set_tree_view (GTK_TREE_SELECTION (selection), tree_view);

  return selection;
}

void
gtk_tree_selection_set_tree_view (GtkTreeSelection *selection,
				  GtkTreeView      *tree_view)
{
  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  if (tree_view != NULL)
    g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  selection->tree_view = tree_view;
  tree_view->priv->selection = selection;
}

void
gtk_tree_selection_set_type (GtkTreeSelection     *selection,
			     GtkTreeSelectionType  type)
{
  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));

  if (selection->type == type)
    return;

  if (type == GTK_TREE_SELECTION_SINGLE)
    {
      GtkRBTree *tree = NULL;
      GtkRBNode *node = NULL;
      gint selected = FALSE;

      if (selection->tree_view->priv->anchor)
	{
	  _gtk_tree_view_find_node (selection->tree_view,
				    selection->tree_view->priv->anchor,
				    &tree,
				    &node);

	  if (node && GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	    selected = TRUE;
	}
      gtk_tree_selection_unselect_all (selection);
      if (node && selected)
	GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_SELECTED);
    }
  selection->type = type;
}

void
gtk_tree_selection_set_select_function (GtkTreeSelection     *selection,
					GtkTreeSelectionFunc  func,
					gpointer            data)
{
  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (func != NULL);

  selection->user_func = func;
  selection->user_data = data;
}

gpointer
gtk_tree_selection_get_user_data (GtkTreeSelection *selection)
{
  g_return_val_if_fail (selection != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), NULL);

  return selection->user_data;
}

GtkTreeNode *
gtk_tree_selection_get_selected (GtkTreeSelection *selection)
{
  GtkTreeNode *retval;
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_val_if_fail (selection != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), NULL);

  if (selection->tree_view->priv->anchor == NULL)
    return NULL;

  g_return_val_if_fail (selection->tree_view != NULL, NULL);
  g_return_val_if_fail (selection->tree_view->priv->model != NULL, NULL);

  if (!_gtk_tree_view_find_node (selection->tree_view,
				selection->tree_view->priv->anchor,
				&tree,
				&node) &&
      ! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    /* We don't want to return the anchor if it isn't actually selected.
     */

      return NULL;

  retval = gtk_tree_model_get_node (selection->tree_view->priv->model,
				    selection->tree_view->priv->anchor);
  return retval;
}

void
gtk_tree_selection_selected_foreach (GtkTreeSelection            *selection,
				     GtkTreeSelectionForeachFunc  func,
				     gpointer                     data)
{
  GtkTreePath *path;
  GtkRBTree *tree;
  GtkRBNode *node;
  GtkTreeNode tree_node;

  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (selection->tree_view->priv->model != NULL);

  if (func == NULL ||
      selection->tree_view->priv->tree == NULL ||
      selection->tree_view->priv->tree->root == NULL)
    return;

  tree = selection->tree_view->priv->tree;
  node = selection->tree_view->priv->tree->root;

  while (node->left != tree->nil)
    node = node->left;

  /* find the node internally */
  path = gtk_tree_path_new_root ();
  tree_node = gtk_tree_model_get_node (selection->tree_view->priv->model, path);
  gtk_tree_path_free (path);

  do
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	(* func) (selection->tree_view->priv->model, tree_node, data);
      if (node->children)
	{
	  tree = node->children;
	  node = tree->root;
	  while (node->left != tree->nil)
	    node = node->left;
	  tree_node = gtk_tree_model_node_children (selection->tree_view->priv->model, tree_node);

	  /* Sanity Check! */
	  TREE_VIEW_INTERNAL_ASSERT_VOID (tree_node != NULL);
	}
      else
	{
	  gboolean done = FALSE;
	  do
	    {
	      node = _gtk_rbtree_next (tree, node);
	      if (node != NULL)
		{
		  gtk_tree_model_node_next (selection->tree_view->priv->model, &tree_node);
		  done = TRUE;

		  /* Sanity Check! */
		  TREE_VIEW_INTERNAL_ASSERT_VOID (tree_node != NULL);
		}
	      else
		{
		  node = tree->parent_node;
		  tree = tree->parent_tree;
		  if (tree == NULL)
		    /* we've run out of tree */
		    /* We're done with this function */
		    return;
		  tree_node = gtk_tree_model_node_parent (selection->tree_view->priv->model, tree_node);

		  /* Sanity check */
		  TREE_VIEW_INTERNAL_ASSERT_VOID (tree_node != NULL);
		}
	    }
	  while (!done);
	}
    }
  while (TRUE);
}

void
gtk_tree_selection_select_path (GtkTreeSelection *selection,
				GtkTreePath      *path)
{
  GtkRBNode *node;
  GtkRBTree *tree;
  GdkModifierType state = 0;

  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (path != NULL);

  _gtk_tree_view_find_node (selection->tree_view,
			    path,
			    &tree,
			    &node);

  if (node == NULL || GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    return;

  if (selection->type == GTK_TREE_SELECTION_MULTI)
    state = GDK_CONTROL_MASK;

  _gtk_tree_selection_internal_select_node (selection,
					    node,
					    tree,
					    path,
					    state);
}

void
gtk_tree_selection_unselect_path (GtkTreeSelection *selection,
				  GtkTreePath      *path)
{
  GtkRBNode *node;
  GtkRBTree *tree;

  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (path != NULL);

  _gtk_tree_view_find_node (selection->tree_view,
			    path,
			    &tree,
			    &node);

  if (node == NULL || !GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    return;

  _gtk_tree_selection_internal_select_node (selection,
					    node,
					    tree,
					    path,
					    GDK_CONTROL_MASK);
}

void
gtk_tree_selection_select_node (GtkTreeSelection *selection,
				GtkTreeNode      *tree_node)
{
  GtkTreePath *path;

  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (selection->tree_view->priv->model != NULL);

  path = gtk_tree_model_get_path (selection->tree_view->priv->model,
				  tree_node);

  if (path == NULL)
    return;

  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);
}


void
gtk_tree_selection_unselect_node (GtkTreeSelection *selection,
				  GtkTreeNode      *tree_node)
{
  GtkTreePath *path;

  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);

  path = gtk_tree_model_get_path (selection->tree_view->priv->model,
				  tree_node);

  if (path == NULL)
    return;

  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);
}

/* Wish I was in python, right now... */
struct _TempTuple {
  GtkTreeSelection *selection;
  gint dirty;
};

static void
select_all_helper (GtkRBTree  *tree,
		   GtkRBNode  *node,
		   gpointer  data)
{
  struct _TempTuple *tuple = data;

  if (node->children)
    _gtk_rbtree_traverse (node->children,
			  node->children->root,
			  G_PRE_ORDER,
			  select_all_helper,
			  data);
  if (!GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    {
      gtk_tree_selection_real_select_node (tuple->selection, tree, node, TRUE);
      tuple->dirty = TRUE;
    }
}

void
gtk_tree_selection_select_all (GtkTreeSelection *selection)
{
  struct _TempTuple *tuple;

  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (selection->tree_view->priv->tree != NULL);

  if (selection->type == GTK_TREE_SELECTION_SINGLE)
    {
      GtkRBNode *node;
      node = selection->tree_view->priv->tree->root;

      while (node->right != selection->tree_view->priv->tree->nil)
	node = node->right;
      return;
    }

  tuple = g_new (struct _TempTuple, 1);
  tuple->selection = selection;
  tuple->dirty = FALSE;

  _gtk_rbtree_traverse (selection->tree_view->priv->tree,
			selection->tree_view->priv->tree->root,
			G_PRE_ORDER,
			select_all_helper,
			tuple);
  if (tuple->dirty)
    gtk_widget_queue_draw (GTK_WIDGET (selection->tree_view));
  g_free (tuple);
}

static void
unselect_all_helper (GtkRBTree  *tree,
		     GtkRBNode  *node,
		     gpointer  data)
{
  struct _TempTuple *tuple = data;

  if (node->children)
    _gtk_rbtree_traverse (node->children,
			  node->children->root,
			  G_PRE_ORDER,
			  unselect_all_helper,
			  data);
  if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    {
      gtk_tree_selection_real_select_node (tuple->selection, tree, node, FALSE);
      tuple->dirty = TRUE;
    }
}

void
gtk_tree_selection_unselect_all (GtkTreeSelection *selection)
{
  struct _TempTuple *tuple;

  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  if (selection->tree_view->priv->tree == NULL)
    return;

  if (selection->type == GTK_TREE_SELECTION_SINGLE)
    {
      GtkRBTree *tree = NULL;
      GtkRBNode *node = NULL;
      if (selection->tree_view->priv->anchor == NULL)
	return;

      _gtk_tree_view_find_node (selection->tree_view,
				selection->tree_view->priv->anchor,
				&tree,
				&node);
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	gtk_tree_selection_real_select_node (selection, tree, node, FALSE);
      return;
    }

  tuple = g_new (struct _TempTuple, 1);
  tuple->selection = selection;
  tuple->dirty = FALSE;

  _gtk_rbtree_traverse (selection->tree_view->priv->tree,
			selection->tree_view->priv->tree->root,
			G_PRE_ORDER,
			unselect_all_helper,
			tuple);
  if (tuple->dirty)
    gtk_widget_queue_draw (GTK_WIDGET (selection->tree_view));
  g_free (tuple);
}

void
gtk_tree_selection_select_range (GtkTreeSelection *selection,
				 GtkTreePath      *start_path,
				 GtkTreePath      *end_path)
{
  GtkRBNode *start_node, *end_node;
  GtkRBTree *start_tree, *end_tree;

  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);

  switch (gtk_tree_path_compare (start_path, end_path))
    {
    case -1:
      _gtk_tree_view_find_node (selection->tree_view,
				end_path,
				&start_tree,
				&start_node);
      _gtk_tree_view_find_node (selection->tree_view,
				start_path,
				&end_tree,
				&end_node);
      break;
    case 0:
      _gtk_tree_view_find_node (selection->tree_view,
				start_path,
				&start_tree,
				&start_node);
      end_tree = start_tree;
      end_node = start_node;
      break;
    case 1:
      _gtk_tree_view_find_node (selection->tree_view,
				start_path,
				&start_tree,
				&start_node);
      _gtk_tree_view_find_node (selection->tree_view,
				end_path,
				&end_tree,
				&end_node);
      break;
    }

  g_return_if_fail (start_node != NULL);
  g_return_if_fail (end_node != NULL);

  do
    {
      gtk_tree_selection_real_select_node (selection, start_tree, start_node, TRUE);

      if (start_node == end_node)
	return;

      if (start_node->children)
	{
	  start_tree = start_node->children;
	  start_node = start_tree->root;
	  while (start_node->left != start_tree->nil)
	    start_node = start_node->left;
	}
      else
	{
	  gboolean done = FALSE;
	  do
	    {
	      start_node = _gtk_rbtree_next (start_tree, start_node);
	      if (start_node != NULL)
		{
		  done = TRUE;
		}
	      else
		{
		  start_node = start_tree->parent_node;
		  start_tree = start_tree->parent_tree;
		  if (start_tree == NULL)
		    /* we've run out of tree */
		    /* This means we never found end node!! */
		    return;
		}
	    }
	  while (!done);
	}
    }
  while (TRUE);
}


/* Called internally by gtktree_view.  It handles actually selecting
 * the tree.  This should almost certainly ever be called by
 * anywhere else */
void
_gtk_tree_selection_internal_select_node (GtkTreeSelection *selection,
					  GtkRBNode        *node,
					  GtkRBTree        *tree,
					  GtkTreePath      *path,
					  GdkModifierType   state)
{
  gint flags;

  if (((state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) && (selection->tree_view->priv->anchor == NULL))
    {
      selection->tree_view->priv->anchor = gtk_tree_path_copy (path);
      gtk_tree_selection_real_select_node (selection, tree, node, TRUE);
    }
  else if ((state & (GDK_CONTROL_MASK|GDK_SHIFT_MASK)) == (GDK_SHIFT_MASK|GDK_CONTROL_MASK))
    {
      gtk_tree_selection_select_range (selection,
				       selection->tree_view->priv->anchor,
				       path);
    }
  else if ((state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK)
    {
      flags = node->flags;
      if (selection->type == GTK_TREE_SELECTION_SINGLE)
	gtk_tree_selection_unselect_all (selection);
      if (selection->tree_view->priv->anchor)
	gtk_tree_path_free (selection->tree_view->priv->anchor);
      selection->tree_view->priv->anchor = gtk_tree_path_copy (path);
      if ((flags & GTK_RBNODE_IS_SELECTED) == GTK_RBNODE_IS_SELECTED)
	gtk_tree_selection_real_select_node (selection, tree, node, FALSE);
      else
	gtk_tree_selection_real_select_node (selection, tree, node, TRUE);
    }
  else if ((state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
    {
      gtk_tree_selection_unselect_all (selection);
      gtk_tree_selection_select_range (selection,
				       selection->tree_view->priv->anchor,
				       path);
    }
  else
    {
      gtk_tree_selection_unselect_all (selection);
      if (selection->tree_view->priv->anchor)
	gtk_tree_path_free (selection->tree_view->priv->anchor);
      selection->tree_view->priv->anchor = gtk_tree_path_copy (path);
      gtk_tree_selection_real_select_node (selection, tree, node, TRUE);
    }
}

