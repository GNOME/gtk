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
      path = ptr + 1;
    }

  return retval;
}

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

GtkTreePath *
gtk_tree_path_new_root (void)
{
  GtkTreePath *retval;

  retval = gtk_tree_path_new ();
  gtk_tree_path_append_index (retval, 0);

  return retval;
}

void
gtk_tree_path_append_index (GtkTreePath *path,
			    gint         index)
{
  gint *new_indices = g_new (gint, ++path->depth);
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

gint
gtk_tree_path_get_depth (GtkTreePath *path)
{
  return path->depth;
}

gint *
gtk_tree_path_get_indices (GtkTreePath *path)
{
  return path->indices;
}

void
gtk_tree_path_free (GtkTreePath *path)
{
  g_free (path->indices);
  g_free (path);
}

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

gint
gtk_tree_path_compare (GtkTreePath  *a,
		       GtkTreePath  *b)
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

void
gtk_tree_path_next (GtkTreePath *path)
{
  g_return_if_fail (path != NULL);

  path->indices[path->depth - 1] ++;
}

gint
gtk_tree_path_prev (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (path->indices[path->depth] == 0)
    return FALSE;

  path->indices[path->depth - 1] --;

  return TRUE;
}

gint
gtk_tree_path_up (GtkTreePath *path)
{
  g_return_val_if_fail (path != NULL, FALSE);

  if (path->depth == 1)
    return FALSE;

  path->depth--;

  return TRUE;
}

void
gtk_tree_path_down (GtkTreePath *path)
{
  g_return_if_fail (path != NULL);

  gtk_tree_path_append_index (path, 0);
}

gint
gtk_tree_model_get_n_columns (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_n_columns != NULL, 0);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_n_columns) (tree_model);
}

/* Node options */
GtkTreeNode
gtk_tree_model_get_node (GtkTreeModel *tree_model,
			 GtkTreePath  *path)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_node != NULL, NULL);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_node) (tree_model, path);
}

GtkTreePath *
gtk_tree_model_get_path (GtkTreeModel *tree_model,
			 GtkTreeNode   node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->get_path != NULL, NULL);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->get_path) (tree_model, node);
}

void
gtk_tree_model_node_get_value (GtkTreeModel *tree_model,
			       GtkTreeNode   node,
			       gint        column,
			       GValue     *value)
{
  g_return_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_get_value != NULL);
  (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_get_value) (tree_model, node, column, value);
}

gboolean
gtk_tree_model_node_next (GtkTreeModel  *tree_model,
			  GtkTreeNode   *node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_next != NULL, FALSE);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_next) (tree_model, node);
}

GtkTreeNode
gtk_tree_model_node_children (GtkTreeModel *tree_model,
			      GtkTreeNode   node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_children != NULL, NULL);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_children) (tree_model, node);
}

gboolean
gtk_tree_model_node_has_child (GtkTreeModel *tree_model,
			       GtkTreeNode   node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_has_child != NULL, FALSE);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_has_child) (tree_model, node);
}

gint
gtk_tree_model_node_n_children (GtkTreeModel *tree_model,
				GtkTreeNode   node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_n_children != NULL, -1);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_n_children) (tree_model, node);
}

GtkTreeNode
gtk_tree_model_node_nth_child (GtkTreeModel *tree_model,
			       GtkTreeNode   node,
			       gint        n)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_nth_child != NULL, NULL);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_nth_child) (tree_model, node, n);
}

GtkTreeNode
gtk_tree_model_node_parent (GtkTreeModel *tree_model,
			    GtkTreeNode   node)
{
  g_return_val_if_fail (GTK_TREE_MODEL_GET_IFACE (tree_model)->node_parent != NULL, NULL);
  return (* GTK_TREE_MODEL_GET_IFACE (tree_model)->node_parent) (tree_model, node);
}

