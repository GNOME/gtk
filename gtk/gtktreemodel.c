/* gtktreemodel.c
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "gtktreemodel.h"

struct _GtkTreePath
{
  gint depth;
  gint *indices;
};

GtkType
gtk_tree_model_get_type (void)
{
  static GtkType tree_model_type = 0;

  if (!tree_model_type)
    {
      static const GTypeInfo tree_model_info =
      {
        sizeof (GtkTreeModelIface), /* class_size */
	NULL,		/* base_init */
	NULL,		/* base_finalize */
      };

      tree_model_type = g_type_register_static (G_TYPE_INTERFACE, "GtkTreeModel", &tree_model_info);
    }

  return tree_model_type;
}

/**
 * gtk_tree_path_new:
 * @void: 
 * 
 * Creates a new #GtkTreePath.
 * 
 * Return value: A newly created #GtkTreePath.
 **/
/* GtkTreePath Operations */
GtkTreePath *
gtk_tree_path_new (void)
{
  GtkTreePath *retval;
  retval = (GtkTreePath *) g_new (GtkTreePath, 1);
  retval->depth = 0;
  retval->indices = NULL;

  return retval;
}

/**
 * gtk_tree_path_new_from_string:
 * @path: The string representation of a path.
 * 
 * Creates a new #GtkTreePath initialized to @path.  @path is expected to be a
 * colon separated list of numbers.  For example, the string "10:4:0" would
 * create a path of depth 3.
 * 
 * Return value: A newly created #GtkTreePath.
 **/
GtkTreePath *
gtk_tree_path_new_from_string (gchar *path)
{
  GtkTreePath *retval;
  gchar *ptr;
  gint i;

  g_return_val_if_fail (path != NULL, gtk_tree_path_new ());

  retval = gtk_tree_path_new ();

  while (1)
    {
      i = strtol (path, &ptr, 10);
      gtk_tree_path_append_index (retval, i);

      if (*ptr == '\000')
	break;
      /* FIXME: should we error out if this is not a ':', or should we be tolerant? */
      path = ptr + 1;
    }

  return retval;
}

/**
 * gtk_tree_path_to_string:
 * @path: A #GtkTreePath
 * 
 * Generates a string representation of the path.  This string is a ':'
 * separated list of numbers.  For example, "4:10:0:3" would be an acceptable return value for this string.
 * 
 * Return value: A newly allocated string.  Must be freed with #g_free.
 **/
gchar *
gtk_tree_path_to_string (GtkTreePath *path)
{
  gchar *retval, *ptr;
  gint i;

  if (path->depth == 0)
    return NULL;

  ptr = retval = (gchar *) g_new0 (char *, path->depth*8);
  sprintf (retval, "%d", path->indices[0]);
  while (*ptr != '\000')
    ptr++;

  for (i = 1; i < path->depth; i++)
    {
      sprintf (ptr, ":%d", path->indices[i]);
      while (*ptr != '\000')
	ptr++;
    }

  return retval;
}

/**
 * gtk_tree_path_new_root:
 * @void: 
 * 
 * Creates a new root #GtkTreePath.  The string representation of this path is
 * "0"
 * 
 * Return value: A new #GtkTreePath.
 **/
GtkTreePath *
gtk_tree_path_new_root (void)
{
  GtkTreePath *retval;

  retval = gtk_tree_path_new ();
  gtk_tree_path_append_index (retval, 0);

  return retval;
}

/**
 * gtk_tree_path_append_index:
 * @path: A #GtkTreePath.
 * @index: The index.
 * 
 * Appends a new index to a path.  As a result, the depth of the path is
 * increased.
 **/
void
gtk_tree_path_append_index (GtkTreePath *path,
			    gint         index)
{
  gint *new_indices;

  g_return_if_fail (path != NULL);
  g_return_if_fail (index >= 0);

  new_indices = g_new (gint, ++path->depth);
  if (path->indices == NULL)
    {
      path->indices = new_indices;
      path->indices[0] = index;
      return;
    }

  memcpy (new_indices, path->indices, (path->depth - 1)*sizeof (gint));
  g_free (path->indices);
  path->indices = new_indices;
  path->indices[path->depth - 1] = index;
}

/**
 * gtk_tree_path_prepend_index:
 * @path: A #GtkTreePath.
 * @index: The index.
 * 
 * Prepends a new index to a path.  As a result, the depth of the path is
 * increased.
 **/
void
gtk_tree_path_prepend_index (GtkTreePath *path,
			     gint       index)
{
  gint *new_indices = g_new (gint, ++path->depth);
  if (path->indices == NULL)
    {
      path->indices = new_indices;
      path->indices[0] = index;
      return;
    }
  memcpy (new_indices + 1, path->indices, (path->depth - 1)*sizeof (gint));
  g_free (path->indices);
  path->indices = new_indices;
  path->indices[0] = index;
}

/**
 * gtk_tree_path_get_depth:
 * @path: A #GtkTreePath.
 * 
 * Returns the current depth of @path.
 * 
 * Return value: The depth of @path
 **/
gint
gtk_tree_path_get_depth (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, 0);

  return path->depth;
}

/**
 * gtk_tree_path_get_indices:
 * @path: A #GtkTreePath.
 * 
 * Returns the current indices of @path.  This is an array of integers, each
 * representing a node in a tree.
 * 
 * Return value: The current indices, or NULL.
 **/
gint *
gtk_tree_path_get_indices (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, NULL);

  return path->indices;
}

/**
 * gtk_tree_path_free:
 * @path: A #GtkTreePath.
 * 
 * Frees @path.
 **/
void
gtk_tree_path_free (GtkTreePath *path)
{
  g_free (path->indices);
  g_free (path);
}

/**
 * gtk_tree_path_copy:
 * @path: A #GtkTreePath.
 * 
 * Creates a new #GtkTreePath based upon @path.
 * 
 * Return value: A new #GtkTreePath.
 **/
GtkTreePath *
gtk_tree_path_copy (GtkTreePath *path)
{
  GtkTreePath *retval;

  retval = g_new (GtkTreePath, 1);
  retval->depth = path->depth;
  retval->indices = g_new (gint, path->depth);
  memcpy (retval->indices, path->indices, path->depth * sizeof (gint));
  return retval;
}

/**
 * gtk_tree_path_compare:
 * @a: A #GtkTreePath.
 * @b: A #GtkTreePath to compare with.
 * 
 * Compares two paths.  If @a appears before @b in a tree, then 1, is returned.
 * If @b appears before @a, then -1 is returned.  If the two nodes are equal,
 * then 0 is returned.
 * 
 * Return value: The relative positions of @a and @b
 **/
gint
gtk_tree_path_compare (const GtkTreePath *a,
		       const GtkTreePath *b)
{
  gint p = 0, q = 0;

  g_return_val_if_fail (a != NULL, 0);
  g_return_val_if_fail (b != NULL, 0);
  g_return_val_if_fail (a->depth > 0, 0);
  g_return_val_if_fail (b->depth > 0, 0);

  do
    {
      if (a->indices[p] == b->indices[q])
	continue;
      return (a->indices[p] < b->indices[q]?1:-1);
    }
  while (++p < a->depth && ++q < b->depth);
  if (a->depth == b->depth)
    return 0;
  return (a->depth < b->depth?1:-1);
}

/**
 * gtk_tree_path_next:
 * @path: A #GtkTreePath.
 * 
 * Moves the @path to point to the next node at the current depth.
 **/
void
gtk_tree_path_next (GtkTreePath *path)
{
  g_return_if_fail (path != NULL);
  g_return_if_fail (path->depth > 0);

  path->indices[path->depth - 1] ++;
}

/**
 * gtk_tree_path_prev:
 * @path: A #GtkTreePath.
 * 
 * Moves the @path to point to the previous node at the current depth, if it exists.
 **/
gint
gtk_tree_path_prev (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (path->indices[path->depth] == 0)
    return FALSE;

  path->indices[path->depth - 1] --;

  return TRUE;
}

/**
 * gtk_tree_path_up:
 * @path: A #GtkTreePath.
 * 
 * Moves the @path to point to it's parent node, if it has a parent.
 * 
 * Return value: TRUE if @path has a parent, and the move was made.
 **/
gint
gtk_tree_path_up (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (path->depth == 1)
    return FALSE;

  path->depth--;

  return TRUE;
}

/**
 * gtk_tree_path_down:
 * @path: A #GtkTreePath.
 * 
 * Moves @path to point to the first child of the current path.
 **/
void
gtk_tree_path_down (GtkTreePath *path)
{
  g_return_if_fail (path != NULL);

  gtk_tree_path_append_index (path, 0);
}

/**
 * gtk_tree_model_get_n_columns:
 * @tree_model: A #GtkTreeModel.
 * 
 * Returns the number of columns supported by the #tree_model
 * 
 * Return value: The number of columns.
 **/
gint
gtk_tree_model_get_n_columns (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_n_columns != NULL, 0);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_n_columns) (tree_model);
}

/**
 * gtk_tree_model_get_node:
 * @tree_model: A #GtkTreeModel.
 * @path: The @GtkTreePath.
 * 
 * Returns a #GtkTreeNode located at @path.  If such a node does not exist, NULL
 * is returned.
 * 
 * Return value: A #GtkTreeNode located at @path, or NULL.
 **/
GtkTreeNode
gtk_tree_model_get_node (GtkTreeModel *tree_model,
			 GtkTreePath  *path)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_node != NULL, NULL);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_node) (tree_model, path);
}

/**
 * gtk_tree_model_get_path:
 * @tree_model: A #GtkTreeModel.
 * @node: The #GtkTreeNode.
 * 
 * Returns a newly created #GtkTreePath that points to @node.  This path should
 * be freed with #gtk_tree_path_free.
 * 
 * Return value: a newly created #GtkTreePath.
 **/
GtkTreePath *
gtk_tree_model_get_path (GtkTreeModel *tree_model,
			 GtkTreeNode   node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_path != NULL, NULL);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_path) (tree_model, node);
}

/**
 * gtk_tree_model_node_get_value:
 * @tree_model: A #GtkTreeModel.
 * @node: The #GtkTreeNode.
 * @column: A column on the node.
 * @value: An empty #GValue to set.
 * 
 * Sets initializes and sets @value to that at @column.  When done with value,
 * #g_value_unset needs to be called on it.
 **/
void
gtk_tree_model_node_get_value (GtkTreeModel *tree_model,
			       GtkTreeNode   node,
			       gint          column,
			       GValue       *value)
{
  g_return_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_get_value != NULL);
  (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_get_value) (tree_model, node, column, value);
}

/**
 * gtk_tree_model_node_next:
 * @tree_model: A #GtkTreeModel.
 * @node: A reference to a #GtkTreeNode.
 * 
 * Sets @node to be the node following it at the current level.  If there is no
 * next @node, FALSE is returned, and *@node is set to NULL.
 * 
 * Return value: TRUE if @node has been changed to the next node.
 **/
gboolean
gtk_tree_model_node_next (GtkTreeModel  *tree_model,
			  GtkTreeNode   *node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_next != NULL, FALSE);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_next) (tree_model, node);
}

/**
 * gtk_tree_model_node_children:
 * @tree_model: A #GtkTreeModel.
 * @node: The #GtkTreeNode to get children from.
 * 
 * Returns the first child node of @node.  If it has no children, then NULL is
 * returned.
 * 
 * Return value: The first child of @node, or NULL.
 **/
GtkTreeNode
gtk_tree_model_node_children (GtkTreeModel *tree_model,
			      GtkTreeNode   node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_children != NULL, NULL);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_children) (tree_model, node);
}

/**
 * gtk_tree_model_node_has_child:
 * @tree_model: A #GtkTreeModel.
 * @node: The #GtkTreeNode to test for children.
 * 
 * Returns TRUE if @node has children, FALSE otherwise.
 * 
 * Return value: TRUE if @node has children.
 **/
gboolean
gtk_tree_model_node_has_child (GtkTreeModel *tree_model,
			       GtkTreeNode   node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_has_child != NULL, FALSE);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_has_child) (tree_model, node);
}

/**
 * gtk_tree_model_node_n_children:
 * @tree_model: A #GtkTreeModel.
 * @node: The #GtkTreeNode.
 * 
 * Returns the number of children that @node has.
 * 
 * Return value: The number of children of @node.
 **/
gint
gtk_tree_model_node_n_children (GtkTreeModel *tree_model,
				GtkTreeNode   node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_n_children != NULL, -1);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_n_children) (tree_model, node);
}

/**
 * gtk_tree_model_node_nth_child:
 * @tree_model: A #GtkTreeModel.
 * @node: The #GtkTreeNode to get the child from.
 * @n: The index of the desired #GtkTreeNode.
 * 
 * Returns a child of @node, using the given index.  The first index is 0.  If
 * the index is too big, NULL is returned.
 * 
 * Return value: the child of @node at index @n.
 **/
GtkTreeNode
gtk_tree_model_node_nth_child (GtkTreeModel *tree_model,
			       GtkTreeNode   node,
			       gint          n)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_nth_child != NULL, NULL);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_nth_child) (tree_model, node, n);
}

/**
 * gtk_tree_model_node_parent:
 * @tree_model: A #GtkTreeModel.
 * @node: The #GtkTreeNode.
 * 
 * Returns the parent of @node.  If @node is at depth 0, then NULL is returned.
 * 
 * Return value: Returns the parent node of @node, or NULL.
 **/
GtkTreeNode
gtk_tree_model_node_parent (GtkTreeModel *tree_model,
			    GtkTreeNode   node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_parent != NULL, NULL);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_parent) (tree_model, node);
}

