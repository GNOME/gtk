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

static void gtk_tree_selection_init              (GtkTreeSelection      *selection);
static void gtk_tree_selection_class_init        (GtkTreeSelectionClass *class);
static gint gtk_tree_selection_real_select_all   (GtkTreeSelection      *selection);
static gint gtk_tree_selection_real_unselect_all (GtkTreeSelection      *selection);
static gint gtk_tree_selection_real_select_node  (GtkTreeSelection      *selection,
						  GtkRBTree             *tree,
						  GtkRBNode             *node,
						  gboolean               select);

enum
{
  SELECTION_CHANGED,
  LAST_SIGNAL
};

static GtkObjectClass *parent_class = NULL;
static guint tree_selection_signals[LAST_SIGNAL] = { 0 };

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

      selection_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkTreeSelection", &selection_info, 0);
    }

  return selection_type;
}

static void
gtk_tree_selection_class_init (GtkTreeSelectionClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;
  parent_class = g_type_class_peek_parent (class);

  class->selection_changed = NULL;

  tree_selection_signals[SELECTION_CHANGED] =
    gtk_signal_new ("selection_changed",
		    GTK_RUN_FIRST,
		    GTK_CLASS_TYPE (object_class),
		    GTK_SIGNAL_OFFSET (GtkTreeSelectionClass, selection_changed),
		    gtk_marshal_VOID__VOID,
		    GTK_TYPE_NONE, 0);
}

static void
gtk_tree_selection_init (GtkTreeSelection *selection)
{
  selection->type = GTK_TREE_SELECTION_SINGLE;
}

/**
 * _gtk_tree_selection_new:
 * 
 * Creates a new #GtkTreeSelection object.  This function should not be invoked,
 * as each #GtkTreeView will create it's own #GtkTreeSelection.
 * 
 * Return value: A newly created #GtkTreeSelection object.
 **/
GtkTreeSelection*
_gtk_tree_selection_new (void)
{
  GtkTreeSelection *selection;

  selection = GTK_TREE_SELECTION (gtk_type_new (GTK_TYPE_TREE_SELECTION));

  return selection;
}

/**
 * _gtk_tree_selection_new_with_tree_view:
 * @tree_view: The #GtkTreeView.
 * 
 * Creates a new #GtkTreeSelection object.  This function should not be invoked,
 * as each #GtkTreeView will create it's own #GtkTreeSelection.
 * 
 * Return value: A newly created #GtkTreeSelection object.
 **/
GtkTreeSelection*
_gtk_tree_selection_new_with_tree_view (GtkTreeView *tree_view)
{
  GtkTreeSelection *selection;

  g_return_val_if_fail (tree_view != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_VIEW (tree_view), NULL);

  selection = _gtk_tree_selection_new ();
  _gtk_tree_selection_set_tree_view (selection, tree_view);

  return selection;
}

/**
 * _gtk_tree_selection_set_tree_view:
 * @selection: A #GtkTreeSelection.
 * @tree_view: The #GtkTreeView.
 * 
 * Sets the #GtkTreeView of @selection.  This function should not be invoked, as
 * it is used internally by #GtkTreeView.
 **/
void
_gtk_tree_selection_set_tree_view (GtkTreeSelection *selection,
                                   GtkTreeView      *tree_view)
{
  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  if (tree_view != NULL)
    g_return_if_fail (GTK_IS_TREE_VIEW (tree_view));

  selection->tree_view = tree_view;
}

/* FIXME explain what the anchor is */
/**
 * gtk_tree_selection_set_mode:
 * @selection: A #GtkTreeSelection.
 * @type: The selection type.
 * 
 * Sets the selection type of the @selection.  If the previous type was
 * #GTK_TREE_SELECTION_MULTI and @type is #GTK_TREE_SELECTION_SINGLE, then
 * the anchor is kept selected, if it was previously selected.
 **/
void
gtk_tree_selection_set_mode (GtkTreeSelection     *selection,
			     GtkTreeSelectionMode  type)
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
      /* FIXME: if user_func is set, then it needs to unconditionally unselect
       * all.
       */
      gtk_tree_selection_unselect_all (selection);

      /* FIXME are we properly emitting the selection_changed signal here? */
      if (node && selected)
	GTK_RBNODE_SET_FLAG (node, GTK_RBNODE_IS_SELECTED);
    }
  selection->type = type;
}

/**
 * gtk_tree_selection_set_select_function:
 * @selection: A #GtkTreeSelection.
 * @func: The selection function.
 * @data: The selection function's data.
 * 
 * Sets the selection function.  If set, this function is called before any node
 * is selected or unselected, giving some control over which nodes are selected.
 **/
void
gtk_tree_selection_set_select_function (GtkTreeSelection     *selection,
					GtkTreeSelectionFunc  func,
					gpointer              data)
{
  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (func != NULL);

  selection->user_func = func;
  selection->user_data = data;
}

/**
 * gtk_tree_selection_get_user_data:
 * @selection: A #GtkTreeSelection.
 * 
 * Returns the user data for the selection function.
 * 
 * Return value: The user data.
 **/
gpointer
gtk_tree_selection_get_user_data (GtkTreeSelection *selection)
{
  g_return_val_if_fail (selection != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), NULL);

  return selection->user_data;
}

/**
 * gtk_tree_selection_get_selected:
 * @selection: A #GtkTreeSelection.
 * @model: A pointer set to the #GtkTreeModel, or NULL.
 * @iter: The #GtkTreeIter, or NULL.
 * 
 * Sets @iter to the currently selected node if @selection is set to
 * #GTK_TREE_SELECTION_SINGLE.  Otherwise, it uses the anchor.  @iter may be
 * NULL if you just want to test if @selection has any selected nodes.  @model
 * is filled with the current model as a convenience.
 * 
 * Return value: TRUE, if there is a selected node.
 **/
gboolean
gtk_tree_selection_get_selected (GtkTreeSelection  *selection,
				 GtkTreeModel     **model,
				 GtkTreeIter       *iter)
{
  GtkRBTree *tree;
  GtkRBNode *node;

  g_return_val_if_fail (selection != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_SELECTION (selection), FALSE);

  if (model)
    *model = selection->tree_view->priv->model;
  
  if (selection->tree_view->priv->anchor == NULL)
    return FALSE;
  else if (iter == NULL)
    return TRUE;

  g_return_val_if_fail (selection->tree_view != NULL, FALSE);
  g_return_val_if_fail (selection->tree_view->priv->model != NULL, FALSE);

  if (!_gtk_tree_view_find_node (selection->tree_view,
				selection->tree_view->priv->anchor,
				&tree,
				&node) &&
      ! GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
    /* We don't want to return the anchor if it isn't actually selected.
     */
      return FALSE;

  return gtk_tree_model_get_iter (selection->tree_view->priv->model,
				  iter,
				  selection->tree_view->priv->anchor);
}

/**
 * gtk_tree_selection_selected_foreach:
 * @selection: A #GtkTreeSelection.
 * @func: The function to call for each selected node.
 * @data: user data to pass to the function.
 * 
 * Calls a function for each selected node.
 **/
void
gtk_tree_selection_selected_foreach (GtkTreeSelection            *selection,
				     GtkTreeSelectionForeachFunc  func,
				     gpointer                     data)
{
  GtkTreePath *path;
  GtkRBTree *tree;
  GtkRBNode *node;
  GtkTreeIter iter;

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
  gtk_tree_model_get_iter (selection->tree_view->priv->model,
			   &iter, path);
  gtk_tree_path_free (path);

  do
    {
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	(* func) (selection->tree_view->priv->model, &iter, data);
      if (node->children)
	{
	  gboolean has_child;
	  GtkTreeIter tmp;

	  tree = node->children;
	  node = tree->root;
	  while (node->left != tree->nil)
	    node = node->left;
	  tmp = iter;
	  has_child = gtk_tree_model_iter_children (selection->tree_view->priv->model, &iter, &tmp);

	  /* Sanity Check! */
	  TREE_VIEW_INTERNAL_ASSERT_VOID (has_child);
	}
      else
	{
	  gboolean done = FALSE;
	  do
	    {
	      node = _gtk_rbtree_next (tree, node);
	      if (node != NULL)
		{
		  gboolean has_next;

		  has_next = gtk_tree_model_iter_next (selection->tree_view->priv->model, &iter);
		  done = TRUE;

		  /* Sanity Check! */
		  TREE_VIEW_INTERNAL_ASSERT_VOID (has_next);
		}
	      else
		{
		  gboolean has_parent;
		  GtkTreeIter tmp_iter = iter;

		  node = tree->parent_node;
		  tree = tree->parent_tree;
		  if (tree == NULL)
		    /* we've run out of tree */
		    /* We're done with this function */
		    return;
		  has_parent = gtk_tree_model_iter_parent (selection->tree_view->priv->model, &iter, &tmp_iter);

		  /* Sanity check */
		  TREE_VIEW_INTERNAL_ASSERT_VOID (has_parent);
		}
	    }
	  while (!done);
	}
    }
  while (TRUE);
}

/**
 * gtk_tree_selection_select_path:
 * @selection: A #GtkTreeSelection.
 * @path: The #GtkTreePath to be selected.
 * 
 * Select the row at @path.
 **/
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

/**
 * gtk_tree_selection_unselect_path:
 * @selection: A #GtkTreeSelection.
 * @path: The #GtkTreePath to be unselected.
 * 
 * Unselects the row at @path.
 **/
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

/**
 * gtk_tree_selection_select_iter:
 * @selection: A #GtkTreeSelection.
 * @iter: The #GtkTreeIter to be selected.
 * 
 * Selects the specified iterator.
 **/
void
gtk_tree_selection_select_iter (GtkTreeSelection *selection,
				GtkTreeIter      *iter)
{
  GtkTreePath *path;

  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (selection->tree_view->priv->model != NULL);
  g_return_if_fail (iter == NULL);

  path = gtk_tree_model_get_path (selection->tree_view->priv->model,
				  iter);

  if (path == NULL)
    return;

  gtk_tree_selection_select_path (selection, path);
  gtk_tree_path_free (path);
}


/**
 * gtk_tree_selection_unselect_iter:
 * @selection: A #GtkTreeSelection.
 * @iter: The #GtkTreeIter to be unselected.
 * 
 * Unselects the specified iterator.
 **/
void
gtk_tree_selection_unselect_iter (GtkTreeSelection *selection,
				  GtkTreeIter      *iter)
{
  GtkTreePath *path;

  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (selection->tree_view->priv->model != NULL);
  g_return_if_fail (iter == NULL);

  path = gtk_tree_model_get_path (selection->tree_view->priv->model,
				  iter);

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
		   gpointer    data)
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
      tuple->dirty = gtk_tree_selection_real_select_node (tuple->selection, tree, node, TRUE) || tuple->dirty;
    }
}


/* We have a real_{un,}select_all function that doesn't emit the signal, so we
 * can use it in other places without fear of the signal being emitted.
 */
static gint
gtk_tree_selection_real_select_all (GtkTreeSelection *selection)
{
  struct _TempTuple *tuple;
  if (selection->tree_view->priv->tree == NULL)
    return FALSE;

  if (selection->type == GTK_TREE_SELECTION_SINGLE)
    {
      GtkRBTree *tree;
      GtkRBNode *node;
      gint dirty;

      /* Just select the last row */
      
      dirty = gtk_tree_selection_real_unselect_all (selection);

      tree = selection->tree_view->priv->tree;
      node = tree->root;
      do
	{
	  while (node->right != selection->tree_view->priv->tree->nil)
	    node = node->right;

	  if (node->children)
	    {
	      tree = node->children;
	      node = tree->root;
	    }
	  else
	    break;
	} while (TRUE);

      dirty |= gtk_tree_selection_real_select_node (selection, tree, node, TRUE);

      return dirty;
    }
  else
    {
      /* Mark all nodes selected */
      
      tuple = g_new (struct _TempTuple, 1);
      tuple->selection = selection;
      tuple->dirty = FALSE;

      _gtk_rbtree_traverse (selection->tree_view->priv->tree,
                            selection->tree_view->priv->tree->root,
                            G_PRE_ORDER,
                            select_all_helper,
                            tuple);
      if (tuple->dirty)
        {
          g_free (tuple);
          return TRUE;
        }
      g_free (tuple);
      return FALSE;
    }
}

/**
 * gtk_tree_selection_select_all:
 * @selection: A #GtkTreeSelection.
 * 
 * Selects all the nodes.  If the type of @selection is
 * #GTK_TREE_SELECTION_SINGLE, then the last row is selected.
 **/
void
gtk_tree_selection_select_all (GtkTreeSelection *selection)
{
  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (selection->tree_view->priv->tree != NULL);

  if (gtk_tree_selection_real_select_all (selection))
    gtk_signal_emit (GTK_OBJECT (selection), tree_selection_signals[SELECTION_CHANGED]);
}

static void
unselect_all_helper (GtkRBTree  *tree,
		     GtkRBNode  *node,
		     gpointer    data)
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
      tuple->dirty = gtk_tree_selection_real_select_node (tuple->selection, tree, node, FALSE) || tuple->dirty;
    }
}

static gint
gtk_tree_selection_real_unselect_all (GtkTreeSelection *selection)
{
  struct _TempTuple *tuple;

  if (selection->type == GTK_TREE_SELECTION_SINGLE)
    {
      GtkRBTree *tree = NULL;
      GtkRBNode *node = NULL;
      if (selection->tree_view->priv->anchor == NULL)
	return FALSE;

      _gtk_tree_view_find_node (selection->tree_view,
				selection->tree_view->priv->anchor,
				&tree,
				&node);
      if (GTK_RBNODE_FLAG_SET (node, GTK_RBNODE_IS_SELECTED))
	{
	  gtk_tree_selection_real_select_node (selection, tree, node, FALSE);
	  return TRUE;
	}
      return FALSE;
    }
  else
    {  
      tuple = g_new (struct _TempTuple, 1);
      tuple->selection = selection;
      tuple->dirty = FALSE;
      
      _gtk_rbtree_traverse (selection->tree_view->priv->tree,
                            selection->tree_view->priv->tree->root,
                            G_PRE_ORDER,
                            unselect_all_helper,
                            tuple);
      
      if (tuple->dirty)
        {
          g_free (tuple);
          return TRUE;
        }
      g_free (tuple);
      return FALSE;
    }
}

/**
 * gtk_tree_selection_unselect_all:
 * @selection: A #GtkTreeSelection.
 * 
 * Unselects all the nodes.
 **/
void
gtk_tree_selection_unselect_all (GtkTreeSelection *selection)
{
  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);
  g_return_if_fail (selection->tree_view->priv->tree != NULL);
  if (selection->tree_view->priv->tree == NULL)
    return;

  if (gtk_tree_selection_real_unselect_all (selection))
    gtk_signal_emit (GTK_OBJECT (selection), tree_selection_signals[SELECTION_CHANGED]);
}

static gint
gtk_tree_selection_real_select_range (GtkTreeSelection *selection,
				      GtkTreePath      *start_path,
				      GtkTreePath      *end_path)
{
  GtkRBNode *start_node, *end_node;
  GtkRBTree *start_tree, *end_tree;
  gboolean dirty = FALSE;

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

  g_return_val_if_fail (start_node != NULL, FALSE);
  g_return_val_if_fail (end_node != NULL, FALSE);

  do
    {
      if (GTK_RBNODE_FLAG_SET (start_node, GTK_RBNODE_IS_SELECTED))
	{
	  dirty = gtk_tree_selection_real_select_node (selection, start_tree, start_node, FALSE);
	}

      if (start_node == end_node)
	break;

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
                    /* FIXME should this really be silent, or should it g_warning? */
		    /* we've run out of tree */
		    /* This means we never found end node!! */
		    break;
		}
	    }
	  while (!done);
	}
    }
  while (TRUE);

  return dirty;
}

/**
 * gtk_tree_selection_select_range:
 * @selection: A #GtkTreeSelection.
 * @start_path: The initial node of the range.
 * @end_path: The final node of the range.
 * 
 * Selects a range of nodes, determined by @start_path and @end_path inclusive.
 **/
void
gtk_tree_selection_select_range (GtkTreeSelection *selection,
				 GtkTreePath      *start_path,
				 GtkTreePath      *end_path)
{
  g_return_if_fail (selection != NULL);
  g_return_if_fail (GTK_IS_TREE_SELECTION (selection));
  g_return_if_fail (selection->tree_view != NULL);

  if (gtk_tree_selection_real_select_range (selection, start_path, end_path))
    gtk_signal_emit (GTK_OBJECT (selection), tree_selection_signals[SELECTION_CHANGED]);
}
/* Called internally by gtktreeview.c It handles actually selecting the tree.
 * This should almost certainly ever be called by anywhere else.
 */
void
_gtk_tree_selection_internal_select_node (GtkTreeSelection *selection,
					  GtkRBNode        *node,
					  GtkRBTree        *tree,
					  GtkTreePath      *path,
					  GdkModifierType   state)
{
  gint flags;
  gint dirty = FALSE;

  if (((state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK) && (selection->tree_view->priv->anchor == NULL))
    {
      selection->tree_view->priv->anchor = gtk_tree_path_copy (path);
      dirty = gtk_tree_selection_real_select_node (selection, tree, node, TRUE);
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
	dirty = gtk_tree_selection_real_unselect_all (selection);

      if (selection->tree_view->priv->anchor)
	gtk_tree_path_free (selection->tree_view->priv->anchor);
      selection->tree_view->priv->anchor = gtk_tree_path_copy (path);

      if ((flags & GTK_RBNODE_IS_SELECTED) == GTK_RBNODE_IS_SELECTED)
	dirty |= gtk_tree_selection_real_select_node (selection, tree, node, FALSE);
      else
	dirty |= gtk_tree_selection_real_select_node (selection, tree, node, TRUE);
    }
  else if ((state & GDK_SHIFT_MASK) == GDK_SHIFT_MASK)
    {
      dirty = gtk_tree_selection_real_unselect_all (selection);
      dirty |= gtk_tree_selection_real_select_range (selection,
						     selection->tree_view->priv->anchor,
						     path);
    }
  else
    {
      dirty = gtk_tree_selection_real_unselect_all (selection);
      if (selection->tree_view->priv->anchor)
	gtk_tree_path_free (selection->tree_view->priv->anchor);
      selection->tree_view->priv->anchor = gtk_tree_path_copy (path);
      dirty |= gtk_tree_selection_real_select_node (selection, tree, node, TRUE);
    }

  if (dirty)
    gtk_signal_emit (GTK_OBJECT (selection), tree_selection_signals[SELECTION_CHANGED]);
}

/* NOTE: Any {un,}selection ever done _MUST_ be done through this function!
 */

/* FIXME: user_func can screw up GTK_TREE_SELECTION_SINGLE.  If it prevents
 * unselection of a node, it can keep more then one node selected.
 */
/* Perhaps the correct solution is to prevent selecting the new node, if
 * we fail to unselect the old node.
 */
static gint
gtk_tree_selection_real_select_node (GtkTreeSelection *selection,
				     GtkRBTree        *tree,
				     GtkRBNode        *node,
				     gboolean          select)
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
      gtk_tree_path_free (path);
    }
  if (selected == TRUE)
    {
      node->flags ^= GTK_RBNODE_IS_SELECTED;

      /* FIXME: just draw the one node*/
      gtk_widget_queue_draw (GTK_WIDGET (selection->tree_view));
      return TRUE;
    }

  return FALSE;
}

