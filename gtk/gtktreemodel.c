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
#include <glib.h>
#include <gobject/gvaluecollector.h>
#include "gtktreemodel.h"
#include "gtktreeview.h"
#include "gtktreeprivate.h"
#include "gtksignal.h"


struct _GtkTreePath
{
  gint depth;
  gint *indices;
};

static void gtk_tree_model_base_init (gpointer g_class);


GtkType
gtk_tree_model_get_type (void)
{
  static GtkType tree_model_type = 0;

  if (! tree_model_type)
    {
      static const GTypeInfo tree_model_info =
      {
        sizeof (GtkTreeModelIface), /* class_size */
	gtk_tree_model_base_init,   /* base_init */
	NULL,		/* base_finalize */
	NULL,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
	0,
	0,              /* n_preallocs */
	NULL
      };

      tree_model_type = g_type_register_static (G_TYPE_INTERFACE, "GtkTreeModel", &tree_model_info, 0);
      g_type_interface_add_prerequisite (tree_model_type, G_TYPE_OBJECT);
    }

  return tree_model_type;
}

static void
gtk_tree_model_base_init (gpointer g_class)
{
  static gboolean initialized = FALSE;

  if (! initialized)
    {
      g_signal_newc ("range_changed",
		     GTK_TYPE_TREE_MODEL,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET (GtkTreeModelIface, range_changed),
		     NULL, NULL,
		     gtk_marshal_VOID__BOXED_BOXED_BOXED_BOXED,
		     G_TYPE_NONE, 4,
		     GTK_TYPE_TREE_PATH,
		     GTK_TYPE_TREE_ITER,
		     GTK_TYPE_TREE_PATH,
		     GTK_TYPE_TREE_ITER);
      g_signal_newc ("inserted",
		     GTK_TYPE_TREE_MODEL,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET (GtkTreeModelIface, inserted),
		     NULL, NULL,
		     gtk_marshal_VOID__BOXED_BOXED,
		     G_TYPE_NONE, 2,
		     GTK_TYPE_TREE_PATH,
		     GTK_TYPE_TREE_ITER);
      g_signal_newc ("has_child_toggled",
		     GTK_TYPE_TREE_MODEL,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET (GtkTreeModelIface, has_child_toggled),
		     NULL, NULL,
		     gtk_marshal_VOID__BOXED_BOXED,
		     G_TYPE_NONE, 2,
		     GTK_TYPE_TREE_PATH,
		     GTK_TYPE_TREE_ITER);
      g_signal_newc ("deleted",
		     GTK_TYPE_TREE_MODEL,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET (GtkTreeModelIface, deleted),
		     NULL, NULL,
		     gtk_marshal_VOID__BOXED,
		     G_TYPE_NONE, 1,
		     GTK_TYPE_TREE_PATH);
      g_signal_newc ("reordered",
		     GTK_TYPE_TREE_MODEL,
		     G_SIGNAL_RUN_LAST,
		     G_STRUCT_OFFSET (GtkTreeModelIface, reordered),
		     NULL, NULL,
		     gtk_marshal_VOID__BOXED_BOXED_POINTER,
		     G_TYPE_NONE, 3,
		     GTK_TYPE_TREE_PATH,
		     GTK_TYPE_TREE_ITER,
		     G_TYPE_POINTER);
      initialized = TRUE;
    }
}

/**
 * gtk_tree_path_new:
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
 * create a path of depth 3 pointing to the 11th child of the root node, the 5th
 * child of that 11th child, and the 1st child of that 5th child.  If an invalid
 * path is past in, NULL is returned.
 *
 * Return value: A newly created #GtkTreePath, or NULL
 **/
GtkTreePath *
gtk_tree_path_new_from_string (gchar *path)
{
  GtkTreePath *retval;
  gchar *orig_path = path;
  gchar *ptr;
  gint i;

  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (*path != '\000', NULL);

  retval = gtk_tree_path_new ();

  while (1)
    {
      i = strtol (path, &ptr, 10);
      gtk_tree_path_append_index (retval, i);

      if (i < 0)
	{
	  g_warning (G_STRLOC"Negative numbers in path %s passed to gtk_tree_path_new_from_string", orig_path);
	  gtk_tree_path_free (retval);
	  return NULL;
	}
      if (*ptr == '\000')
	break;
      if (ptr == path || *ptr != ':')
	{
	  g_warning (G_STRLOC"Invalid path %s passed to gtk_tree_path_new_from_string", orig_path);
	  gtk_tree_path_free (retval);
	  return NULL;
	}
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

  g_return_val_if_fail (path != NULL, NULL);

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
  g_return_if_fail (path != NULL);
  g_return_if_fail (index >= 0);

  path->depth += 1;
  path->indices = g_realloc (path->indices, path->depth * sizeof(gint));
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
  g_return_if_fail (path != NULL);

  g_free (path->indices);
  g_free (path);
}

/**
 * gtk_tree_path_copy:
 * @path: A #GtkTreePath.
 *
 * Creates a new #GtkTreePath as a copy of @path.
 *
 * Return value: A new #GtkTreePath.
 **/
GtkTreePath *
gtk_tree_path_copy (GtkTreePath *path)
{
  GtkTreePath *retval;

  g_return_val_if_fail (path != NULL, NULL);

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
 * Compares two paths.  If @a appears before @b in a tree, then -1, is returned.
 * If @b appears before @a, then 1 is returned.  If the two nodes are equal,
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
      return (a->indices[p] < b->indices[q]?-1:1);
    }
  while (++p < a->depth && ++q < b->depth);
  if (a->depth == b->depth)
    return 0;
  return (a->depth < b->depth?-1:1);
}

/**
 * gtk_tree_path_is_ancestor:
 * @path: a #GtkTreePath
 * @descendant: another #GtkTreePath
 *
 *
 *
 * Return value: %TRUE if @descendant is contained inside @path
 **/
gboolean
gtk_tree_path_is_ancestor (GtkTreePath *path,
                           GtkTreePath *descendant)
{
  gint i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (descendant != NULL, FALSE);

  /* can't be an ancestor if we're deeper */
  if (path->depth >= descendant->depth)
    return FALSE;

  i = 0;
  while (i < path->depth)
    {
      if (path->indices[i] != descendant->indices[i])
        return FALSE;
      ++i;
    }

  return TRUE;
}

/**
 * gtk_tree_path_is_descendant:
 * @path: a #GtkTreePath
 * @ancestor: another #GtkTreePath
 *
 *
 *
 * Return value: %TRUE if @ancestor contains @path somewhere below it
 **/
gboolean
gtk_tree_path_is_descendant (GtkTreePath *path,
                             GtkTreePath *ancestor)
{
  gint i;

  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (ancestor != NULL, FALSE);

  /* can't be a descendant if we're shallower in the tree */
  if (path->depth <= ancestor->depth)
    return FALSE;

  i = 0;
  while (i < ancestor->depth)
    {
      if (path->indices[i] != ancestor->indices[i])
        return FALSE;
      ++i;
    }

  return TRUE;
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
 *
 * Return value: TRUE if @path has a previous node, and the move was made.
 **/
gboolean
gtk_tree_path_prev (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (path->indices[path->depth - 1] == 0)
    return FALSE;

  path->indices[path->depth - 1] -= 1;

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
gboolean
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
 * gtk_tree_iter_copy:
 * @iter: A #GtkTreeIter.
 *
 * Creates a dynamically allocated tree iterator as a copy of @iter.  This
 * function is not intended for use in applications, because you can just copy
 * the structs by value (GtkTreeIter new_iter = iter;).  You
 * must free this iter with gtk_tree_iter_free ().
 *
 * Return value: a newly allocated copy of @iter.
 **/
GtkTreeIter *
gtk_tree_iter_copy (GtkTreeIter *iter)
{
  GtkTreeIter *retval;

  g_return_val_if_fail (iter != NULL, NULL);

  retval = g_new (GtkTreeIter, 1);
  *retval = *iter;

  return retval;
}

/**
 * gtk_tree_iter_free:
 * @iter: A dynamically allocated tree iterator.
 *
 * Free an iterator that has been allocated on the heap.  This function is
 * mainly used for language bindings.
 **/
void
gtk_tree_iter_free (GtkTreeIter *iter)
{
  g_return_if_fail (iter != NULL);

  g_free (iter);
}

/**
 * gtk_tree_model_get_flags:
 * @tree_model: A #GtkTreeModel.
 *
 * Returns a set of flags supported by this interface.  The flags are a bitwise
 * combination of #GtkTreeModelFlags.  It is expected that the flags supported
 * do not change for an interface.
 *
 * Return value: The flags supported by this interface.
 **/
GtkTreeModelFlags
gtk_tree_model_get_flags (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (tree_model != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);

  if (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_flags)
    return (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_flags) (tree_model);

  return 0;
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
  g_return_val_if_fail (tree_model != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_n_columns != NULL, 0);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_n_columns) (tree_model);
}

/**
 * gtk_tree_model_get_column_type:
 * @tree_model: A #GtkTreeModel.
 * @index: The column index.
 *
 * Returns the type of the column.
 *
 * Return value: The type of the column.
 **/
GType
gtk_tree_model_get_column_type (GtkTreeModel *tree_model,
				gint          index)
{
  g_return_val_if_fail (tree_model != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), G_TYPE_INVALID);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_column_type != NULL, G_TYPE_INVALID);
  g_return_val_if_fail (index >= 0, G_TYPE_INVALID);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_column_type) (tree_model, index);
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

  g_return_val_if_fail (tree_model != NULL, FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);

  if (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_iter != NULL)
    return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_iter) (tree_model, iter, path);

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
 * gtk_tree_model_get_first:
 * @tree_model: a #GtkTreeModel
 * @iter: iterator to initialize
 *
 * Initialized @iter with the first iterator in the tree (the one at the
 * root path) and returns %TRUE, or returns %FALSE if there are no
 * iterable locations in the model (i.e. the tree is empty).
 *
 * Return value: %TRUE if @iter was initialized
 **/
gboolean
gtk_tree_model_get_first (GtkTreeModel *tree_model,
                          GtkTreeIter  *iter)
{
  gboolean retval;
  GtkTreePath *path;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);

  path = gtk_tree_path_new_root ();

  retval = gtk_tree_model_get_iter (tree_model, iter, path);

  gtk_tree_path_free (path);

  return retval;
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
  g_return_val_if_fail (tree_model != NULL, NULL);
  g_return_val_if_fail (iter != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), NULL);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_path != NULL, NULL);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_path) (tree_model, iter);
}

/**
 * gtk_tree_model_get_value:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter.
 * @column: The column to lookup the value at.
 * @value: An empty #GValue to set.
 *
 * Sets initializes and sets @value to that at @column.  When done with value,
 * #g_value_unset needs to be called on it.
 **/
void
gtk_tree_model_get_value (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (value != NULL);
  g_return_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_value != NULL);

  (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_value) (tree_model, iter, column, value);
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
  g_return_val_if_fail (tree_model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_next != NULL, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_next) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_children:
 * @tree_model: A #GtkTreeModel.
 * @iter: The new #GtkTreeIter to be set to the child.
 * @parent: The #GtkTreeIter.
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
  g_return_val_if_fail (tree_model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
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
  g_return_val_if_fail (tree_model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_has_child != NULL, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_has_child) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_n_children:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter, or NULL.
 *
 * Returns the number of children that @iter has.  If @iter is NULL, then the
 * number of toplevel nodes is returned.
 *
 * Return value: The number of children of @iter.
 **/
gint
gtk_tree_model_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  g_return_val_if_fail (tree_model != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_n_children != NULL, 0);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_n_children) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_nth_child:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter to set to the nth child.
 * @parent: The #GtkTreeIter to get the child from, or NULL.
 * @n: Then index of the desired child.
 *
 * Sets @iter to be the child of @parent, using the given index.  The first
 * index is 0.  If the index is too big, or @parent has no children, @iter is
 * set to an invalid iterator and FALSE is returned.  @parent will remain a
 * valid node after this function has been called.  If @parent is NULL, then the
 * root node is assumed.
 *
 * Return value: TRUE, if @parent has an nth child.
 **/
gboolean
gtk_tree_model_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
  g_return_val_if_fail (tree_model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (n >= 0, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_nth_child != NULL, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_nth_child) (tree_model, iter, parent, n);
}

/**
 * gtk_tree_model_iter_parent:
 * @tree_model: A #GtkTreeModel
 * @iter: The new #GtkTreeIter to set to the parent.
 * @child: The #GtkTreeIter.
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
  g_return_val_if_fail (tree_model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (child != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_parent != NULL, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_parent) (tree_model, iter, child);
}

/**
 * gtk_tree_model_ref_node:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter.
 *
 * Lets the tree ref the node.  This is an optional method for models to
 * implement.  To be more specific, models may ignore this call as it exists
 * primarily for performance reasons.
 * 
 * This function is primarily meant as a way for views to let caching model know
 * when nodes are being displayed (and hence, whether or not to cache that
 * node.)  For example, a file-system based model would not want to keep the
 * entire file-heirarchy in memory, just the sections that are currently being
 * displayed by every current view.
 **/
void
gtk_tree_model_ref_node (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));

  if (GTK_TREE_MODEL_GET_IFACE (tree_model)->ref_node)
    (* GTK_TREE_MODEL_GET_IFACE (tree_model)->ref_node) (tree_model, iter);
}

/**
 * gtk_tree_model_unref_node:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter.
 *
 * Lets the tree unref the node.  This is an optional method for models to
 * implement.  To be more specific, models may ignore this call as it exists
 * primarily for performance reasons.
 *
 * For more information on what this means, please see #gtk_tree_model_ref_node.
 * Please note that nodes that are deleted are not unreffed.
 **/
void
gtk_tree_model_unref_node (GtkTreeModel *tree_model,
			   GtkTreeIter  *iter)
{
  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));

  if (GTK_TREE_MODEL_GET_IFACE (tree_model)->unref_node)
    (* GTK_TREE_MODEL_GET_IFACE (tree_model)->unref_node) (tree_model, iter);
}

/**
 * gtk_tree_model_get:
 * @tree_model: a #GtkTreeModel
 * @iter: a row in @tree_model
 * @Varargs: pairs of column number and value return locations, terminated by -1
 *
 * Gets the value of one or more cells in the row referenced by @iter.
 * The variable argument list should contain integer column numbers,
 * each column number followed by a place to store the value being
 * retrieved.  The list is terminated by a -1. For example, to get a
 * value from column 0 with type %G_TYPE_STRING, you would
 * write: gtk_tree_model_set (model, iter, 0, &place_string_here, -1),
 * where place_string_here is a gchar* to be filled with the string.
 * If appropriate, the returned values have to be freed or unreferenced.
 *
 **/
void
gtk_tree_model_get (GtkTreeModel *tree_model,
		    GtkTreeIter  *iter,
		    ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));

  va_start (var_args, iter);
  gtk_tree_model_get_valist (tree_model, iter, var_args);
  va_end (var_args);
}

/**
 * gtk_tree_model_get_valist:
 * @tree_model: a #GtkTreeModel
 * @iter: a row in @tree_model
 * @var_args: va_list of column/return location pairs
 *
 * See gtk_tree_model_get(), this version takes a va_list for
 * language bindings to use.
 *
 **/
void
gtk_tree_model_get_valist (GtkTreeModel *tree_model,
                           GtkTreeIter  *iter,
                           va_list	var_args)
{
  gint column;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));

  column = va_arg (var_args, gint);

  while (column != -1)
    {
      GValue value = { 0, };
      gchar *error = NULL;

      if (column >= gtk_tree_model_get_n_columns (tree_model))
	{
	  g_warning ("%s: Invalid column number %d accessed (remember to end your list of columns with a -1)", G_STRLOC, column);
	  break;
	}

      gtk_tree_model_get_value (GTK_TREE_MODEL (tree_model), iter, column, &value);

      G_VALUE_LCOPY (&value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

 	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  break;
	}

      g_value_unset (&value);

      column = va_arg (var_args, gint);
    }
}

void
gtk_tree_model_range_changed (GtkTreeModel *tree_model,
			      GtkTreePath  *start_path,
			      GtkTreeIter  *start_iter,
			      GtkTreePath  *end_path,
			      GtkTreeIter  *end_iter)
{
  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (start_path != NULL);
  g_return_if_fail (start_iter != NULL);
  g_return_if_fail (end_path != NULL);
  g_return_if_fail (end_iter != NULL);

  g_signal_emit_by_name (tree_model, "range_changed",
			 start_path, start_iter,
			 end_path, end_iter);
}

void
gtk_tree_model_inserted (GtkTreeModel *tree_model,
			 GtkTreePath  *path,
			 GtkTreeIter  *iter)
{
  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit_by_name (tree_model, "inserted", path, iter);
}

void
gtk_tree_model_has_child_toggled (GtkTreeModel *tree_model,
				  GtkTreePath  *path,
				  GtkTreeIter  *iter)
{
  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit_by_name (tree_model, "has_child_toggled", path, iter);
}

void
gtk_tree_model_deleted (GtkTreeModel *tree_model,
			GtkTreePath  *path)
{
  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);

  g_signal_emit_by_name (tree_model, "deleted", path);
}

void
gtk_tree_model_reordered (GtkTreeModel *tree_model,
			  GtkTreePath  *path,
			  GtkTreeIter  *iter,
			  gint         *new_order)
{
  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (new_order != NULL);

  g_signal_emit_by_name (tree_model, "reordered", path, iter, new_order);
}


/*
 * GtkTreeRowReference
 */

#define ROW_REF_DATA_STRING "gtk-tree-row-refs"

struct _GtkTreeRowReference
{
  GObject *proxy;
  GtkTreeModel *model;
  GtkTreePath *path;
};

typedef struct
{
  GSList *list;
} RowRefList;


static void
release_row_references (gpointer data)
{
  RowRefList *refs = data;
  GSList *tmp_list = NULL;

  tmp_list = refs->list;
  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      if (reference->proxy == (GObject *)reference->model)
	reference->model = NULL;
      reference->proxy = NULL;

      /* we don't free the reference, users are responsible for that. */

      tmp_list = g_slist_next (tmp_list);
    }

  g_slist_free (refs->list);
  g_free (refs);
}

static void
gtk_tree_row_ref_inserted_callback (GObject     *object,
				    GtkTreePath *path,
				    GtkTreeIter *iter,
				    gpointer     data)
{
  RowRefList *refs = g_object_get_data (data, ROW_REF_DATA_STRING);

  GSList *tmp_list;

  if (refs == NULL)
    return;

  /* This function corrects the path stored in the reference to
   * account for an insertion. Note that it's called _after_ the insertion
   * with the path to the newly-inserted row. Which means that
   * the inserted path is in a different "coordinate system" than
   * the old path (e.g. if the inserted path was just before the old path,
   * then inserted path and old path will be the same, and old path must be
   * moved down one).
   */

  tmp_list = refs->list;

  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      if (reference->path)
	{
	  gint depth = gtk_tree_path_get_depth (path);
	  gint ref_depth = gtk_tree_path_get_depth (reference->path);

	  if (ref_depth >= depth)
	    {
	      gint *indices = gtk_tree_path_get_indices (path);
	      gint *ref_indices = gtk_tree_path_get_indices (reference->path);
	      gint i;

	      /* This is the depth that might affect us. */
	      i = depth - 1;

	      if (indices[i] <= ref_indices[i])
		ref_indices[i] += 1;
	    }
	}

      tmp_list = g_slist_next (tmp_list);
    }
}

static void
gtk_tree_row_ref_deleted_callback (GObject     *object,
				   GtkTreePath *path,
				   gpointer     data)
{
  RowRefList *refs = g_object_get_data (data, ROW_REF_DATA_STRING);
  GSList *tmp_list;

  if (refs == NULL)
    return;

  /* This function corrects the path stored in the reference to
   * account for an deletion. Note that it's called _after_ the
   * deletion with the old path of the just-deleted row. Which means
   * that the deleted path is the same now-defunct "coordinate system"
   * as the path saved in the reference, which is what we want to fix.
   *
   * Note that this is different from the situation in "inserted," so
   * while you might think you can cut-and-paste between these
   * functions, it's not going to work. ;-)
   */

  tmp_list = refs->list;

  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      if (reference->path)
	{
	  gint depth = gtk_tree_path_get_depth (path);
	  gint ref_depth = gtk_tree_path_get_depth (reference->path);

	  if (ref_depth >= depth)
	    {
	      /* Need to adjust path upward */
	      gint *indices = gtk_tree_path_get_indices (path);
	      gint *ref_indices = gtk_tree_path_get_indices (reference->path);
	      gint i;

	      i = depth - 1;
	      if (indices[i] < ref_indices[i])
		ref_indices[i] -= 1;
	      else if (indices[i] == ref_indices[i])
		{
		  /* the referenced node itself, or its parent, was
		   * deleted, mark invalid
		   */

		  gtk_tree_path_free (reference->path);
		  reference->path = NULL;
		}
	    }

	}
      tmp_list = g_slist_next (tmp_list);
    }
}

static void
gtk_tree_row_ref_reordered_callback (GObject     *object,
				     GtkTreePath *path,
				     GtkTreeIter *iter,
				     gint        *new_order,
				     gpointer     data)
{
  RowRefList *refs = g_object_get_data (data, ROW_REF_DATA_STRING);
  GSList *tmp_list;
  gint length;

  if (refs == NULL)
    return;

  tmp_list = refs->list;

  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      length = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (reference->model), iter);

      if (length < 2)
	return;

      if ((reference->path) &&
	  (gtk_tree_path_is_ancestor (path, reference->path)))
	{
	  gint ref_depth = gtk_tree_path_get_depth (reference->path);
	  gint depth = gtk_tree_path_get_depth (path);

	  if (ref_depth > depth)
	    {
	      gint i;
	      gint *indices = gtk_tree_path_get_indices (reference->path);

	      for (i = 0; i < length; i++)
		{
		  if (new_order[i] == indices[depth])
		    {
		      indices[depth] = i;
		      return;
		    }
		}
	    }
	}

      tmp_list = g_slist_next (tmp_list);
    }
  
}
     
static void
connect_ref_callbacks (GtkTreeModel *model)
{
  g_signal_connect_data (G_OBJECT (model),
                         "inserted",
                         (GCallback) gtk_tree_row_ref_inserted_callback,
                         model,
                         NULL,
                         FALSE,
                         FALSE);

  g_signal_connect_data (G_OBJECT (model),
                         "deleted",
                         (GCallback) gtk_tree_row_ref_deleted_callback,
			 model,
                         NULL,
                         FALSE,
                         FALSE);

  g_signal_connect_data (G_OBJECT (model),
                         "reordered",
                         (GCallback) gtk_tree_row_ref_reordered_callback,
			 model,
                         NULL,
                         FALSE,
                         FALSE);
}

static void
disconnect_ref_callbacks (GtkTreeModel *model)
{
  g_signal_handlers_disconnect_matched (G_OBJECT (model),
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL,
					gtk_tree_row_ref_inserted_callback,
					NULL);
  g_signal_handlers_disconnect_matched (G_OBJECT (model),
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL,
					gtk_tree_row_ref_deleted_callback,
					NULL);
  g_signal_handlers_disconnect_matched (G_OBJECT (model),
                                        G_SIGNAL_MATCH_FUNC,
                                        0, 0, NULL,
					gtk_tree_row_ref_reordered_callback,
					NULL);
}

GtkTreeRowReference *
gtk_tree_row_reference_new (GtkTreeModel *model,
                            GtkTreePath  *path)
{
  g_return_val_if_fail (model != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return gtk_tree_row_reference_new_proxy (G_OBJECT (model), model, path);
}

GtkTreeRowReference *
gtk_tree_row_reference_new_proxy (GObject      *proxy,
				  GtkTreeModel *model,
				  GtkTreePath  *path)
{
  GtkTreeRowReference *reference;
  RowRefList *refs;

  g_return_val_if_fail (G_IS_OBJECT (proxy), NULL);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  reference = g_new (GtkTreeRowReference, 1);

  reference->proxy = proxy;
  reference->model = model;
  reference->path = gtk_tree_path_copy (path);

  refs = g_object_get_data (G_OBJECT (proxy), ROW_REF_DATA_STRING);

  if (refs == NULL)
    {
      refs = g_new (RowRefList, 1);
      refs->list = NULL;

      if (G_OBJECT (model) == proxy)
	connect_ref_callbacks (model);

      g_object_set_data_full (G_OBJECT (proxy),
			      ROW_REF_DATA_STRING,
                              refs, release_row_references);
    }

  refs->list = g_slist_prepend (refs->list, reference);

  return reference;
}

GtkTreePath *
gtk_tree_row_reference_get_path (GtkTreeRowReference *reference)
{
  g_return_val_if_fail (reference != NULL, NULL);

  if (reference->proxy == NULL)
    return NULL;

  if (reference->path == NULL)
    return NULL;

  return gtk_tree_path_copy (reference->path);
}

void
gtk_tree_row_reference_free (GtkTreeRowReference *reference)
{
  RowRefList *refs;

  g_return_if_fail (reference != NULL);

  if (reference->proxy)
    {
      refs = g_object_get_data (G_OBJECT (reference->proxy), ROW_REF_DATA_STRING);

      if (refs == NULL)
        {
          g_warning (G_STRLOC": bad row reference, proxy has no outstanding row references");
          return;
        }

      refs->list = g_slist_remove (refs->list, reference);

      if (refs->list == NULL)
        {
          disconnect_ref_callbacks (reference->model);
          g_object_set_data (G_OBJECT (reference->proxy),
                             ROW_REF_DATA_STRING,
                             NULL);
        }
    }

  if (reference->path)
    gtk_tree_path_free (reference->path);

  g_free (reference);
}

void
gtk_tree_row_reference_inserted (GObject     *proxy,
				 GtkTreePath *path)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_inserted_callback (NULL, path, NULL, proxy);
  
}

void
gtk_tree_row_reference_deleted (GObject     *proxy,
				GtkTreePath *path)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_deleted_callback (NULL, path, proxy);
}

void
gtk_tree_row_reference_reordered (GObject     *proxy,
				  GtkTreePath *path,
				  GtkTreeIter *iter,
				  gint        *new_order)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_reordered_callback (NULL, path, iter, new_order, proxy);
}
  
