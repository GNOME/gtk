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
#include "gtkmarshalers.h"
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
      g_signal_new ("row_changed",
                    GTK_TYPE_TREE_MODEL,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkTreeModelIface, row_changed),
                    NULL, NULL,
                    _gtk_marshal_VOID__BOXED_BOXED,
                    G_TYPE_NONE, 2,
                    GTK_TYPE_TREE_PATH,
                    GTK_TYPE_TREE_ITER);
      g_signal_new ("row_inserted",
                    GTK_TYPE_TREE_MODEL,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkTreeModelIface, row_inserted),
                    NULL, NULL,
                    _gtk_marshal_VOID__BOXED_BOXED,
                    G_TYPE_NONE, 2,
                    GTK_TYPE_TREE_PATH,
                    GTK_TYPE_TREE_ITER);
      g_signal_new ("row_has_child_toggled",
                    GTK_TYPE_TREE_MODEL,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkTreeModelIface, row_has_child_toggled),
                    NULL, NULL,
                    _gtk_marshal_VOID__BOXED_BOXED,
                    G_TYPE_NONE, 2,
                    GTK_TYPE_TREE_PATH,
                    GTK_TYPE_TREE_ITER);
      g_signal_new ("row_deleted",
                    GTK_TYPE_TREE_MODEL,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkTreeModelIface, row_deleted),
                    NULL, NULL,
                    _gtk_marshal_VOID__BOXED,
                    G_TYPE_NONE, 1,
                    GTK_TYPE_TREE_PATH);
      g_signal_new ("rows_reordered",
                    GTK_TYPE_TREE_MODEL,
                    G_SIGNAL_RUN_LAST,
                    G_STRUCT_OFFSET (GtkTreeModelIface, rows_reordered),
                    NULL, NULL,
                    _gtk_marshal_VOID__BOXED_BOXED_POINTER,
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
 * Creates a new #GtkTreePath.  This structure refers to a row 
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
 * path string is passed in, %NULL is returned.
 *
 * Return value: A newly-created #GtkTreePath, or %NULL
 **/
GtkTreePath *
gtk_tree_path_new_from_string (const gchar *path)
{
  GtkTreePath *retval;
  const gchar *orig_path = path;
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
	  g_warning (G_STRLOC ": Negative numbers in path %s passed to gtk_tree_path_new_from_string", orig_path);
	  gtk_tree_path_free (retval);
	  return NULL;
	}
      if (*ptr == '\000')
	break;
      if (ptr == path || *ptr != ':')
	{
	  g_warning (G_STRLOC ": Invalid path %s passed to gtk_tree_path_new_from_string", orig_path);
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
 * Return value: A newly-allocated string.  Must be freed with g_free().
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
 * representing a node in a tree.  This value should not be freed.
 *
 * Return value: The current indices, or %NULL.
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
 * Compares two paths.  If @a appears before @b in a tree, then -1 is returned.
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
 * Returns %TRUE if @descendant is a descendant of @path.
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
 * Returns %TRUE if @path is a descendant of @ancestor.
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
 * Return value: %TRUE if @path has a previous node, and the move was made.
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
 * Return value: %TRUE if @path has a parent, and the move was made.
 **/
gboolean
gtk_tree_path_up (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (path->depth == 0)
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
 * the structs by value (<literal>GtkTreeIter new_iter = iter;</literal>).  You
 * must free this iter with gtk_tree_iter_free ().
 *
 * Return value: a newly-allocated copy of @iter.
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
 * Frees an iterator that has been allocated on the heap.  This function is
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
 * combination of #GtkTreeModelFlags.  The flags supported should not change
 * during the lifecycle of the tree_model.
 *
 * Return value: The flags supported by this interface.
 **/
GtkTreeModelFlags
gtk_tree_model_get_flags (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);

  if (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_flags)
    return (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_flags) (tree_model);

  return 0;
}

/**
 * gtk_tree_model_get_n_columns:
 * @tree_model: A #GtkTreeModel.
 *
 * Returns the number of columns supported by @tree_model.
 *
 * Return value: The number of columns.
 **/
gint
gtk_tree_model_get_n_columns (GtkTreeModel *tree_model)
{
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
 * Sets @iter to a valid iterator pointing to @path.
 *
 * Return value: %TRUE, if @iter was set.
 **/
gboolean
gtk_tree_model_get_iter (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter,
			 GtkTreePath  *path)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (path != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_iter != NULL, FALSE);
  g_return_val_if_fail (path->depth > 0, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_iter) (tree_model, iter, path);
}

/**
 * gtk_tree_model_get_iter_from_string:
 * @tree_model: A #GtkTreeModel.
 * @iter: An uninitialized #GtkTreeIter.
 * @path_string: A string representation of a #GtkTreePath.
 *
 * Sets @iter to a valid iterator pointing to @path_string, if it
 * exists. Otherwise, @iter is left invalid and %FALSE is returned.
 *
 * Return value: %TRUE, if @iter was set.
 **/
gboolean
gtk_tree_model_get_iter_from_string (GtkTreeModel *tree_model,
				     GtkTreeIter  *iter,
				     const gchar  *path_string)
{
  gboolean retval;
  GtkTreePath *path;

  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (path_string != NULL, FALSE);
  
  path = gtk_tree_path_new_from_string (path_string);
  
  g_return_val_if_fail (path != NULL, FALSE);

  retval = gtk_tree_model_get_iter (tree_model, iter, path);
  gtk_tree_path_free (path);
  
  return retval;
}


/**
 * gtk_tree_model_get_iter_root:
 * @tree_model: A #GtkTreeModel.
 * @iter: The uninitialized #GtkTreeIter.
 * 
 * Initializes @iter with the root iterator in the tree (the one at the root
 * path) and returns %TRUE.   Returns %FALSE if the tree is empty.
 * 
 * Return value: %TRUE, if @iter was set.
 **/
gboolean
gtk_tree_model_get_iter_root (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter)
{
  GtkTreePath *path;
  gboolean retval;

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
 * Returns a newly-created #GtkTreePath referenced by @iter.  This path should
 * be freed with gtk_tree_path_free().
 *
 * Return value: a newly-created #GtkTreePath.
 **/
GtkTreePath *
gtk_tree_model_get_path (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);
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
 * Sets initializes and sets @value to that at @column.  When done with @value,
 * g_value_unset() needs to be called to free any allocated memory.
 **/
void
gtk_tree_model_get_value (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);
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
 * is no next @iter, %FALSE is returned and @iter is set to be invalid.
 *
 * Return value: %TRUE if @iter has been changed to the next node.
 **/
gboolean
gtk_tree_model_iter_next (GtkTreeModel  *tree_model,
			  GtkTreeIter   *iter)
{
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
 * %FALSE is returned and @iter is set to be invalid.  @parent will remain a valid
 * node after this function has been called.
 *
 * Return value: %TRUE, if @child has been set to the first child.
 **/
gboolean
gtk_tree_model_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
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
 * Returns %TRUE if @iter has children, %FALSE otherwise.
 *
 * Return value: %TRUE if @iter has children.
 **/
gboolean
gtk_tree_model_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_has_child != NULL, FALSE);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_has_child) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_n_children:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter, or %NULL.
 *
 * Returns the number of children that @iter has.  As a special case, if @iter
 * is %NULL, then the number of toplevel nodes is returned.
 *
 * Return value: The number of children of @iter.
 **/
gint
gtk_tree_model_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL (tree_model), 0);
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_n_children != NULL, 0);

  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->iter_n_children) (tree_model, iter);
}

/**
 * gtk_tree_model_iter_nth_child:
 * @tree_model: A #GtkTreeModel.
 * @iter: The #GtkTreeIter to set to the nth child.
 * @parent: The #GtkTreeIter to get the child from, or %NULL.
 * @n: Then index of the desired child.
 *
 * Sets @iter to be the child of @parent, using the given index.  The first
 * index is 0.  If @index is too big, or @parent has no children, @iter is set
 * to an invalid iterator and %FALSE is returned.  @parent will remain a valid
 * node after this function has been called.  As a special case, if @parent is
 * %NULL, then the @n<!>th root node is set.
 *
 * Return value: %TRUE, if @parent has an @n<!>th child.
 **/
gboolean
gtk_tree_model_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
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
 * doesn't have a parent, then @iter is set to an invalid iterator and %FALSE
 * is returned.  @child will remain a valid node after this function has been
 * called.
 *
 * Return value: %TRUE, if @iter is set to the parent of @child.
 **/
gboolean
gtk_tree_model_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
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
 * entire file-hierarchy in memory, just the sections that are currently being
 * displayed by every current view.
 *
 * A model should be expected to be able to get an iter independent of it's
 * reffed state.
 **/
void
gtk_tree_model_ref_node (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
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
 * For more information on what this means, see gtk_tree_model_ref_node().
 * Please note that nodes that are deleted are not unreffed.
 **/
void
gtk_tree_model_unref_node (GtkTreeModel *tree_model,
			   GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

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
 * write: <literal>gtk_tree_model_get (model, iter, 0, &amp;place_string_here, -1)</literal>,
 * where <literal>place_string_here</literal> is a <type>gchar*</type> to be 
 * filled with the string.
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
  g_return_if_fail (iter != NULL);

  va_start (var_args, iter);
  gtk_tree_model_get_valist (tree_model, iter, var_args);
  va_end (var_args);
}

/**
 * gtk_tree_model_get_valist:
 * @tree_model: a #GtkTreeModel
 * @iter: a row in @tree_model
 * @var_args: <type>va_list</type> of column/return location pairs
 *
 * See gtk_tree_model_get(), this version takes a <type>va_list</type> 
 * for language bindings to use.
 **/
void
gtk_tree_model_get_valist (GtkTreeModel *tree_model,
                           GtkTreeIter  *iter,
                           va_list	var_args)
{
  gint column;

  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (iter != NULL);

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

/**
 * gtk_tree_model_row_changed:
 * @tree_model: A #GtkTreeModel
 * @path: A #GtkTreePath pointing to the changed row
 * @iter: A valid #GtkTreeIter pointing to the changed row
 * 
 * Emits the "row_changed" signal on @tree_model.
 **/
void
gtk_tree_model_row_changed (GtkTreeModel *tree_model,
			    GtkTreePath  *path,
			    GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit_by_name (tree_model, "row_changed", path, iter);
}

/**
 * gtk_tree_model_row_inserted:
 * @tree_model: A #GtkTreeModel
 * @path: A #GtkTreePath pointing to the inserted row
 * @iter: A valid #GtkTreeIter pointing to the inserted row
 * 
 * Emits the "row_inserted" signal on @tree_model
 **/
void
gtk_tree_model_row_inserted (GtkTreeModel *tree_model,
			     GtkTreePath  *path,
			     GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit_by_name (tree_model, "row_inserted", path, iter);
}

/**
 * gtk_tree_model_row_has_child_toggled:
 * @tree_model: A #GtkTreeModel
 * @path: A #GtkTreePath pointing to the changed row
 * @iter: A valid #GtkTreeIter pointing to the changed row
 * 
 * Emits the "row_has_child_toggled" signal on @tree_model.  This should be
 * called by models after the child state of a node changes.
 **/
void
gtk_tree_model_row_has_child_toggled (GtkTreeModel *tree_model,
				      GtkTreePath  *path,
				      GtkTreeIter  *iter)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);
  g_return_if_fail (iter != NULL);

  g_signal_emit_by_name (tree_model, "row_has_child_toggled", path, iter);
}

/**
 * gtk_tree_model_row_deleted:
 * @tree_model: A #GtkTreeModel
 * @path: A #GtkTreePath pointing to the previous location of the deleted row.
 * 
 * Emits the "row_deleted" signal on @tree_model.  This should be called by
 * models after a row has been removed.  The location pointed to by @path should
 * be the location that the row previously was at.  It may not be a valid
 * location anymore.
 **/
void
gtk_tree_model_row_deleted (GtkTreeModel *tree_model,
			    GtkTreePath  *path)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (path != NULL);

  g_signal_emit_by_name (tree_model, "row_deleted", path);
}

/**
 * gtk_tree_model_rows_reordered:
 * @tree_model: A #GtkTreeModel
 * @path: A #GtkTreePath pointing to the tree node whose children have been reordered
 * @iter: A valid #GtkTreeIter pointing to the node whose children have been reordered
 * @new_order: an array of integers containing the new indices of the children,
 *      i.e. the former child <literal>n</literal> is now at position
 *      @new_order<literal>[n]</literal>. 
 * 
 * Emits the "rows_reordered" signal on @tree_model.  This should be called by
 * models when their rows have been reordered.  
 **/
void
gtk_tree_model_rows_reordered (GtkTreeModel *tree_model,
			       GtkTreePath  *path,
			       GtkTreeIter  *iter,
			       gint         *new_order)
{
  g_return_if_fail (GTK_IS_TREE_MODEL (tree_model));
  g_return_if_fail (new_order != NULL);

  g_signal_emit_by_name (tree_model, "rows_reordered", path, iter, new_order);
}


static gboolean
gtk_tree_model_foreach_helper (GtkTreeModel            *model,
			       GtkTreeIter             *iter,
			       GtkTreePath             *path,
			       GtkTreeModelForeachFunc  func,
			       gpointer                 user_data)
{
  gtk_tree_path_append_index (path, 0);

  do
    {
      GtkTreeIter child;

      if (gtk_tree_model_iter_children (model, &child, iter))
	{
	  if (gtk_tree_model_foreach_helper (model, &child, path, func, user_data))
	    return TRUE;
	}

      if ((* func) (model, path, iter, user_data))
	return TRUE;

      gtk_tree_path_next (path);
    }
  while (gtk_tree_model_iter_next (model, iter));

  gtk_tree_path_up (path);
  return FALSE;
}

/**
 * gtk_tree_model_foreach:
 * @model: A #GtkTreeModel
 * @func: A function to be called on each row
 * @user_data: User data to passed to func.
 * 
 * Calls func on each node in model in a depth-first fashion.  If func returns
 * %TRUE, then the tree ceases to be walked, and gtk_tree_model_foreach() returns.
 **/

void
gtk_tree_model_foreach (GtkTreeModel            *model,
			GtkTreeModelForeachFunc  func,
			gpointer                 user_data)
{
  GtkTreePath *path;
  GtkTreeIter iter;

  g_return_if_fail (GTK_IS_TREE_MODEL (model));
  g_return_if_fail (func != NULL);

  path = gtk_tree_path_new_root ();
  if (gtk_tree_model_get_iter (model, &iter, path) == FALSE)
    {
      gtk_tree_path_free (path);
      return;
    }

  gtk_tree_model_foreach_helper (model, &iter, path, func, user_data);
  gtk_tree_path_free (path);
}


/*
 * GtkTreeRowReference
 */

static void gtk_tree_row_reference_unref_path (GtkTreePath  *path,
					       GtkTreeModel *model,
					       gint          depth);


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

      if (reference->path == NULL)
	goto done;

      if (reference->path->depth >= path->depth)
	{
	  gint i;
	  gboolean ancestor = TRUE;

	  for (i = 0; i < path->depth - 1; i ++)
	    {
	      if (path->indices[i] != reference->path->indices[i])
		{
		  ancestor = FALSE;
		  break;
		}
	    }
	  if (ancestor == FALSE)
	    goto done;

	  if (path->indices[path->depth-1] <= reference->path->indices[path->depth-1])
	    reference->path->indices[path->depth-1] += 1;
	}
    done:
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
   */

  tmp_list = refs->list;

  while (tmp_list != NULL)
    {
      GtkTreeRowReference *reference = tmp_list->data;

      if (reference->path)
	{
	  gint i;

	  if (path->depth > reference->path->depth)
	    goto next;
	  for (i = 0; i < path->depth - 1; i++)
	    {
	      if (path->indices[i] != reference->path->indices[i])
		goto next;
	    }

	  /* We know it affects us. */
	  if (path->indices[i] == reference->path->indices[i])
	    {
	      gtk_tree_row_reference_unref_path (reference->path, reference->model, reference->path->depth - 1);
	      gtk_tree_path_free (reference->path);
	      reference->path = NULL;
	    }
	  else if (path->indices[i] < reference->path->indices[i])
	    {
	      reference->path->indices[path->depth-1]-=1;
	    }
	}

next:
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
		      break;
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
  g_signal_connect (G_OBJECT (model),
		    "row_inserted",
		    (GCallback) gtk_tree_row_ref_inserted_callback,
		    model);
  g_signal_connect (G_OBJECT (model),
		    "row_deleted",
		    (GCallback) gtk_tree_row_ref_deleted_callback,
		    model);
  g_signal_connect (G_OBJECT (model),
		    "rows_reordered",
		    (GCallback) gtk_tree_row_ref_reordered_callback,
		    model);
}


/* We do this recursively so that we can unref children nodes before their parent */
static void
gtk_tree_row_reference_unref_path_helper (GtkTreePath  *path,
					  GtkTreeModel *model,
					  GtkTreeIter  *parent_iter,
					  gint          depth,
					  gint          current_depth)
{
  GtkTreeIter iter;

  if (depth == current_depth)
    return;

  gtk_tree_model_iter_nth_child (model, &iter, parent_iter, path->indices[current_depth]);
  gtk_tree_row_reference_unref_path_helper (path, model, &iter, depth, current_depth + 1);
  gtk_tree_model_unref_node (model, &iter);
}

static void
gtk_tree_row_reference_unref_path (GtkTreePath  *path,
				   GtkTreeModel *model,
				   gint          depth)
{
  GtkTreeIter iter;

  if (depth <= 0)
    return;
  
  gtk_tree_model_iter_nth_child (model, &iter, NULL, path->indices[0]);
  gtk_tree_row_reference_unref_path_helper (path, model, &iter, depth, 1);
  gtk_tree_model_unref_node (model, &iter);
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

/**
 * gtk_tree_row_reference_new:
 * @model: A #GtkTreeModel
 * @path: A valid #GtkTreePath to monitor
 * 
 * Creates a row reference based on @path.  This reference will keep pointing to
 * the node pointed to by @path, so long as it exists.  It listens to all
 * signals emitted by @model, and updates it's path appropriately.  If @path
 * isn't a valid path in @model, then %NULL is returned.
 * 
 * Return value: A newly allocated #GtkTreeRowReference, or %NULL
 **/
GtkTreeRowReference *
gtk_tree_row_reference_new (GtkTreeModel *model,
                            GtkTreePath  *path)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (path != NULL, NULL);

  return gtk_tree_row_reference_new_proxy (G_OBJECT (model), model, path);
}

/**
 * gtk_tree_row_reference_new_proxy:
 * @proxy: A proxy #GObject
 * @model: A #GtkTreeModel
 * @path: A valid #GtkTreePath to monitor
 * 
 * You do not need to use this function.  Creates a row reference based on
 * @path.  This reference will keep pointing to the node pointed to by @path, so
 * long as it exists.  If @path isn't a valid path in @model, then %NULL is
 * returned.  However, unlike references created with
 * gtk_tree_row_reference_new(), it does not listen to the model for changes.
 * The creator of the row reference must do this explicitly using
 * gtk_tree_row_reference_inserted(), gtk_tree_row_reference_deleted(),
 * gtk_tree_row_reference_reordered().  This must be called once per signal per
 * proxy.
 *
 * This type of row reference is primarily meant by structures that need to
 * carefully monitor exactly when a row_reference updates itself, and is not
 * generally needed by most applications.
 *
 * Return value: A newly allocated #GtkTreeRowReference, or %NULL
 **/
GtkTreeRowReference *
gtk_tree_row_reference_new_proxy (GObject      *proxy,
				  GtkTreeModel *model,
				  GtkTreePath  *path)
{
  GtkTreeRowReference *reference;
  RowRefList *refs;
  GtkTreeIter parent_iter;
  gint i;

  g_return_val_if_fail (G_IS_OBJECT (proxy), NULL);
  g_return_val_if_fail (GTK_IS_TREE_MODEL (model), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (path->depth > 0, NULL);

  /* check that the path is valid */
  if (gtk_tree_model_get_iter (model, &parent_iter, path) == FALSE)
    return NULL;

  /* Now we want to ref every node */
  gtk_tree_model_iter_nth_child (model, &parent_iter, NULL, path->indices[0]);
  gtk_tree_model_ref_node (model, &parent_iter);

  for (i = 1; i < path->depth; i++)
    {
      GtkTreeIter iter;
      gtk_tree_model_iter_nth_child (model, &iter, &parent_iter, path->indices[i]);
      gtk_tree_model_ref_node (model, &iter);
      parent_iter = iter;
    }

  /* Make ther row reference */
  reference = g_new (GtkTreeRowReference, 1);

  g_object_ref (proxy);
  g_object_ref (model);
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

/**
 * gtk_tree_row_reference_get_path:
 * @reference: A #GtkTreeRowReference
 * 
 * Returns a path that the row reference currently points to, or %NULL if the
 * path pointed to is no longer valid.
 * 
 * Return value: A current path, or %NULL.
 **/
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

/**
 * gtk_tree_row_reference_valid:
 * @reference: A #GtkTreeRowReference, or NULL
 * 
 * Returns TRUE if the %reference is non-NULL and refers to a current valid
 * path.
 * 
 * Return value: TRUE if %reference points to a valid path.
 **/
gboolean
gtk_tree_row_reference_valid (GtkTreeRowReference *reference)
{
  if (reference == NULL || reference->path == NULL)
    return FALSE;

  return TRUE;
}

/**
 * gtk_tree_row_reference_free:
 * @reference: A #GtkTreeRowReference, or NULL
 * 
 * Free's %reference.  %reference may be NULL.
 **/
void
gtk_tree_row_reference_free (GtkTreeRowReference *reference)
{
  RowRefList *refs;

  if (reference == NULL)
    return;

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

  if (reference->path)
    {
      gtk_tree_row_reference_unref_path (reference->path, reference->model, reference->path->depth);
      gtk_tree_path_free (reference->path);
    }

  g_object_unref (reference->proxy);
  g_object_unref (reference->model);
  g_free (reference);
}

/**
 * gtk_tree_row_reference_inserted:
 * @proxy: A #GObject
 * @path: The row position that was inserted
 * 
 * Lets a set of row reference created by gtk_tree_row_reference_new_proxy()
 * know that the model emitted the "row_inserted" signal.
 **/
void
gtk_tree_row_reference_inserted (GObject     *proxy,
				 GtkTreePath *path)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_inserted_callback (NULL, path, NULL, proxy);
  
}

/**
 * gtk_tree_row_reference_inserted:
 * @proxy: A #GObject
 * @path: The path position that was deleted
 * 
 * Lets a set of row reference created by gtk_tree_row_reference_new_proxy()
 * know that the model emitted the "row_deleted" signal.
 **/
void
gtk_tree_row_reference_deleted (GObject     *proxy,
				GtkTreePath *path)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_deleted_callback (NULL, path, proxy);
}

/**
 * gtk_tree_row_reference_reordered:
 * @proxy: A #GObject
 * @path: The parent path of the reordered signal
 * @iter: The iter pointing to the parent of the reordered
 * @new_order: The new order of rows
 * 
 * Lets a set of row reference created by gtk_tree_row_reference_new_proxy()
 * know that the model emitted the "rows_reordered" signal.
 **/
void
gtk_tree_row_reference_reordered (GObject     *proxy,
				  GtkTreePath *path,
				  GtkTreeIter *iter,
				  gint        *new_order)
{
  g_return_if_fail (G_IS_OBJECT (proxy));

  gtk_tree_row_ref_reordered_callback (NULL, path, iter, new_order, proxy);
}
  

