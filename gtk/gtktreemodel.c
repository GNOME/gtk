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

      tree_model_type = g_type_register_static (G_TYPE_INTERFACE, "GtkTreeModel", &tree_model_info, 0);
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
 * gtk_tree_model_get_iter:
 * @tree_model: A #GtkTreeModel.
 * @iter: The uninitialized #GtkTreeIter.
 * @path: The #GtkTreePath.
 * 
 * Sets @iter to a valid iterator pointing to @path.  If the model does not
 * provide an implementation of this function, it is implemented in terms of
 * @gtk_tree_model_iter_nth_child.
 * 
 * Return value: TRUE, if @iter was set.
 **/
gboolean
gtk_tree_model_get_iter (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter,
			 GtkTreePath  *path)
{
  GtkTreeIter parent;
  gint *indices;
  gint depth, i;

  if (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_iter != NULL)
    return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_iter) (tree_model, iter, path);

  g_return_val_if_fail (path != NULL, FALSE);
  indices = gtk_tree_path_get_indices (path);
  depth = gtk_tree_path_get_depth (path);

  g_return_val_if_fail (depth > 0, FALSE);

  if (! gtk_tree_model_iter_nth_child (tree_model, iter, NULL, indices[0]))
    return FALSE;

  for (i = 1; i < depth; i++)
    {
      parent = *iter;
      if (! gtk_tree_model_iter_nth_child (tree_model, iter, &parent, indices[i]))
	return FALSE;
    }

  return TRUE;
}


/**
 * gtk_tree_model_iter_invalid:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter.
 * 
 * Tests the validity of @iter, and returns TRUE if it is invalid.
 * 
 * Return value: TRUE, if @iter is invalid.
 **/
gboolean
gtk_tree_model_iter_invalid (GtkTreeModel *tree_model,
			     GtkTreeIter  *iter)
{
  g_return_val_if_fail (tree_model != NULL, FALSE);
  if (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_invalid)
    return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_invalid) (tree_model, iter);

  /* Default implementation.  Returns false if iter is false, or the stamp is 0 */
  if (iter == NULL || iter->stamp == 0)
    return TRUE;

  return FALSE;
}

/**
 * gtk_tree_model_get_path:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter.
 * 
 * Returns a newly created #GtkTreePath referenced by @iter.  This path should
 * be freed with #gtk_tree_path_free.
 * 
 * Return value: a newly created #GtkTreePath.
 **/
GtkTreePath *
gtk_tree_model_get_path (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_path != NULL, NULL);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_path) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_get_value:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter.
 * @column: The column to lookup the value at.
 * @value: An empty #GValue to set.
 * 
 * Sets initializes and sets @value to that at @column.  When done with value,
 * #g_value_unset needs to be called on it.
 **/
void
gtk_tree_model_iter_get_value (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       gint          column,
			       GValue       *value)
{
  g_return_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_get_value != NULL);

  (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_get_value) (tree_model, iter, column, value);
}

/**
 * gtk_tree_model_iter_next:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter.
 * 
 * Sets @iter to point to the node following it at the current level.  If there
 * is no next @iter, FALSE is returned and @iter is set to be invalid.
 * 
 * Return value: TRUE if @iter has been changed to the next node.
 **/
gboolean
gtk_tree_model_iter_next (GtkTreeModel  *tree_model,
			  GtkTreeIter   *iter)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_next != NULL, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_next) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_children:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter.
 * @child: The new #GtkTreeIter.
 * 
 * Sets @iter to point to the first child of @parent.  If @parent has no children,
 * FALSE is returned and @iter is set to be invalid.  @parent will remain a valid
 * node after this function has been called.
 * 
 * Return value: TRUE, if @child has been set to the first child.
 **/
gboolean
gtk_tree_model_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_children != NULL, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_children) (tree_model, iter, parent);
}

/**
 * gtk_tree_model_iter_has_child:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter to test for children.
 * 
 * Returns TRUE if @iter has children, FALSE otherwise.
 * 
 * Return value: TRUE if @iter has children.
 **/
gboolean
gtk_tree_model_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_has_child != NULL, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_has_child) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_n_children:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter.
 * 
 * Returns the number of children that @iter has.
 * 
 * Return value: The number of children of @iter.
 **/
gint
gtk_tree_model_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_n_children != NULL, -1);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_n_children) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_nth_child:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter to set to the nth child.
 * @parent: The #GtkTreeIter to get the child from, or NULL.
 * @n: Then index of the desired child.
 * 
 * Sets @iter to be the child of @parent, using the given index.  The first index
 * is 0.  If the index is too big, or @parent has no children, @iter is set to an
 * invalid iterator and FALSE is returned.  @parent will remain a valid node after
 * this function has been called.  If @parent is NULL, then the root node is assumed.
 * 
 * Return value: TRUE, if @parent has an nth child.
 **/
gboolean
gtk_tree_model_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_nth_child != NULL, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_nth_child) (tree_model, iter, parent, n);
}

/**
 * gtk_tree_model_iter_parent:
 * @tree_model: A #GtkTreeModel
 * @iter: The #GtkTreeIter.
 * @parent: The #GtkTreeIter to set to the parent
 * 
 * Sets @iter to be the parent of @child.  If @child is at the toplevel, and
 * doesn't have a parent, then @iter is set to an invalid iterator and FALSE
 * is returned.  @child will remain a valid node after this function has been
 * called.
 * 
 * Return value: TRUE, if @iter is set to the parent of @child.
 **/
gboolean
gtk_tree_model_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_parent != NULL, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_parent) (tree_model, iter, child);
}

