/* gtktreestore.c
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

#include "gtktreemodel.h"
#include "gtktreestore.h"
#include "gtktreedatalist.h"
#include "gtktreednd.h"
#include <string.h>
#include <gobject/gvaluecollector.h>

#define G_NODE(node) ((GNode *)node)

static void         gtk_tree_store_init            (GtkTreeStore      *tree_store);
static void         gtk_tree_store_class_init      (GtkTreeStoreClass *tree_store_class);
static void         gtk_tree_store_tree_model_init (GtkTreeModelIface *iface);
static void         gtk_tree_store_drag_source_init(GtkTreeDragSourceIface *iface);
static void         gtk_tree_store_drag_dest_init  (GtkTreeDragDestIface   *iface);
static void         gtk_tree_store_sortable_init   (GtkTreeSortableIface   *iface);
static guint        gtk_tree_store_get_flags       (GtkTreeModel      *tree_model);
static gint         gtk_tree_store_get_n_columns   (GtkTreeModel      *tree_model);
static GType        gtk_tree_store_get_column_type (GtkTreeModel      *tree_model,
						    gint               index);
static GtkTreePath *gtk_tree_store_get_path        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static void         gtk_tree_store_get_value       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    gint               column,
						    GValue            *value);
static gboolean     gtk_tree_store_iter_next       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_tree_store_iter_children   (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent);
static gboolean     gtk_tree_store_iter_has_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gint         gtk_tree_store_iter_n_children (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_tree_store_iter_nth_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent,
						    gint               n);
static gboolean     gtk_tree_store_iter_parent     (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *child);


/* DND interfaces */
static gboolean gtk_tree_store_drag_data_delete   (GtkTreeDragSource *drag_source,
						   GtkTreePath       *path);
static gboolean gtk_tree_store_drag_data_get      (GtkTreeDragSource *drag_source,
						   GtkTreePath       *path,
						   GtkSelectionData  *selection_data);
static gboolean gtk_tree_store_drag_data_received (GtkTreeDragDest   *drag_dest,
						   GtkTreePath       *dest,
						   GtkSelectionData  *selection_data);
static gboolean gtk_tree_store_row_drop_possible  (GtkTreeDragDest   *drag_dest,
						   GtkTreeModel      *src_model,
						   GtkTreePath       *src_path,
						   GtkTreePath       *dest_path);

/* Sortable Interfaces */

static void gtk_tree_store_get_sort_column_id      (GtkTreeSortable        *sortable,
						    gint                   *sort_column_id,
						    GtkTreeSortOrder       *order);
static void gtk_tree_store_set_sort_column_id      (GtkTreeSortable        *sortable,
						    gint                    sort_column_id,
						    GtkTreeSortOrder        order);
static void gtk_tree_store_sort_column_id_set_func (GtkTreeSortable        *sortable,
						    gint                    sort_column_id,
						    GtkTreeIterCompareFunc  func,
						    gpointer                data,
						    GtkDestroyNotify        destroy);



static void     validate_gnode                    (GNode *node);

static inline void
validate_tree (GtkTreeStore *tree_store)
{
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    {
      g_assert (G_NODE (tree_store->root)->parent == NULL);

      validate_gnode (G_NODE (tree_store->root));
    }
}

GtkType
gtk_tree_store_get_type (void)
{
  static GType tree_store_type = 0;

  if (!tree_store_type)
    {
      static const GTypeInfo tree_store_info =
      {
        sizeof (GtkTreeStoreClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_tree_store_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkTreeStore),
	0,              /* n_preallocs */
        (GInstanceInitFunc) gtk_tree_store_init
      };

      static const GInterfaceInfo tree_model_info =
      {
	(GInterfaceInitFunc) gtk_tree_store_tree_model_init,
	NULL,
	NULL
      };

      static const GInterfaceInfo drag_source_info =
      {
	(GInterfaceInitFunc) gtk_tree_store_drag_source_init,
	NULL,
	NULL
      };

      static const GInterfaceInfo drag_dest_info =
      {
	(GInterfaceInitFunc) gtk_tree_store_drag_dest_init,
	NULL,
	NULL
      };

      static const GInterfaceInfo sortable_info =
      {
	(GInterfaceInitFunc) gtk_tree_store_sortable_init,
	NULL,
	NULL
      };

      tree_store_type = g_type_register_static (G_TYPE_OBJECT, "GtkTreeStore", &tree_store_info, 0);

      g_type_add_interface_static (tree_store_type,
				   GTK_TYPE_TREE_MODEL,
				   &tree_model_info);
      g_type_add_interface_static (tree_store_type,
				   GTK_TYPE_TREE_DRAG_SOURCE,
				   &drag_source_info);
      g_type_add_interface_static (tree_store_type,
				   GTK_TYPE_TREE_DRAG_DEST,
				   &drag_dest_info);
      g_type_add_interface_static (tree_store_type,
				   GTK_TYPE_TREE_SORTABLE,
				   &sortable_info);

    }

  return tree_store_type;
}

static void
gtk_tree_store_class_init (GtkTreeStoreClass *tree_store_class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass *) tree_store_class;

}

static void
gtk_tree_store_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_tree_store_get_flags;
  iface->get_n_columns = gtk_tree_store_get_n_columns;
  iface->get_column_type = gtk_tree_store_get_column_type;
  iface->get_path = gtk_tree_store_get_path;
  iface->get_value = gtk_tree_store_get_value;
  iface->iter_next = gtk_tree_store_iter_next;
  iface->iter_children = gtk_tree_store_iter_children;
  iface->iter_has_child = gtk_tree_store_iter_has_child;
  iface->iter_n_children = gtk_tree_store_iter_n_children;
  iface->iter_nth_child = gtk_tree_store_iter_nth_child;
  iface->iter_parent = gtk_tree_store_iter_parent;
}

static void
gtk_tree_store_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->drag_data_delete = gtk_tree_store_drag_data_delete;
  iface->drag_data_get = gtk_tree_store_drag_data_get;
}

static void
gtk_tree_store_drag_dest_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = gtk_tree_store_drag_data_received;
  iface->row_drop_possible = gtk_tree_store_row_drop_possible;
}

static void
gtk_tree_store_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id = NULL;
  iface->set_sort_column_id = NULL;
  iface->sort_column_id_set_func = NULL;
}

static void
gtk_tree_store_init (GtkTreeStore *tree_store)
{
  tree_store->root = g_node_new (NULL);
  tree_store->stamp = g_random_int ();
  tree_store->sort_list = NULL;
}

GtkTreeStore *
gtk_tree_store_new (void)
{
  return GTK_TREE_STORE (g_object_new (gtk_tree_store_get_type (), NULL));
}

GtkTreeStore *
gtk_tree_store_new_with_types (gint n_columns,
			       ...)
{
  GtkTreeStore *retval;
  va_list args;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = gtk_tree_store_new ();
  gtk_tree_store_set_n_columns (retval, n_columns);

  va_start (args, n_columns);

  for (i = 0; i < n_columns; i++)
    {
      GType type = va_arg (args, GType);
      if (! _gtk_tree_data_list_check_type (type))
	{
	  g_warning ("%s: Invalid type %s passed to gtk_tree_store_new_with_types\n", G_STRLOC, g_type_name (type));
	  g_object_unref (G_OBJECT (retval));
	  return NULL;
	}
      gtk_tree_store_set_column_type (retval, i, type);
    }
  va_end (args);

  return retval;
}

/**
 * gtk_tree_store_set_n_columns:
 * @tree_store: 
 * @n_columns: 
 * 
 * As a side effect of calling this function, all sort columns that overlap with
 * the current number of columns will be removed.
 **/
void
gtk_tree_store_set_n_columns (GtkTreeStore *tree_store,
			      gint          n_columns)
{
  GType *new_columns;

  g_return_if_fail (tree_store != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));

  if (tree_store->n_columns == n_columns)
    return;

  new_columns = g_new0 (GType, n_columns);
  if (tree_store->column_headers)
    {
      /* copy the old header orders over */
      if (n_columns >= tree_store->n_columns)
	memcpy (new_columns, tree_store->column_headers, tree_store->n_columns * sizeof (gchar *));
      else
	memcpy (new_columns, tree_store->column_headers, n_columns * sizeof (GType));

      g_free (tree_store->column_headers);
    }

  if (tree_store->sort_list)
    _gtk_tree_data_list_header_free (tree_store->sort_list);

  tree_store->sort_list = _gtk_tree_data_list_header_new (n_columns, tree_store->column_headers);

  tree_store->column_headers = new_columns;
  tree_store->n_columns = n_columns;
}

/**
 * gtk_tree_store_set_column_type:
 * @tree_store: a #GtkTreeStore
 * @column: column number
 * @type: type of the data to be stored in @column
 *
 * Supported types include: %G_TYPE_UINT, %G_TYPE_INT, %G_TYPE_UCHAR,
 * %G_TYPE_CHAR, %G_TYPE_BOOLEAN, %G_TYPE_POINTER, %G_TYPE_FLOAT,
 * %G_TYPE_DOUBLE, %G_TYPE_STRING, %G_TYPE_OBJECT, and %G_TYPE_BOXED, along with
 * subclasses of those types such as %GDK_TYPE_PIXBUF.
 *
 **/
void
gtk_tree_store_set_column_type (GtkTreeStore *tree_store,
				gint          column,
				GType         type)
{
  g_return_if_fail (tree_store != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));
  g_return_if_fail (column >=0 && column < tree_store->n_columns);
  if (!_gtk_tree_data_list_check_type (type))
    {
      g_warning ("%s: Invalid type %s passed to gtk_tree_store_new_with_types\n", G_STRLOC, g_type_name (type));
      return;
    }
  tree_store->column_headers[column] = type;
}

/* fulfill the GtkTreeModel requirements */
/* NOTE: GtkTreeStore::root is a GNode, that acts as the parent node.  However,
 * it is not visible to the tree or to the user., and the path "0" refers to the
 * first child of GtkTreeStore::root.
 */


static guint
gtk_tree_store_get_flags (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), 0);

  return GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint
gtk_tree_store_get_n_columns (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), 0);

  return GTK_TREE_STORE (tree_model)->n_columns;
}

static GType
gtk_tree_store_get_column_type (GtkTreeModel *tree_model,
				gint          index)
{
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), G_TYPE_INVALID);
  g_return_val_if_fail (index < GTK_TREE_STORE (tree_model)->n_columns &&
			index >= 0, G_TYPE_INVALID);

  return GTK_TREE_STORE (tree_model)->column_headers[index];
}

static GtkTreePath *
gtk_tree_store_get_path (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  GtkTreePath *retval;
  GNode *tmp_node;
  gint i = 0;

  g_return_val_if_fail (tree_model != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);
  g_return_val_if_fail (iter->user_data != NULL, NULL);

  validate_tree ((GtkTreeStore*)tree_model);

  g_assert (G_NODE (iter->user_data)->parent != NULL);

  if (G_NODE (iter->user_data)->parent == G_NODE (GTK_TREE_STORE (tree_model)->root))
    {
      retval = gtk_tree_path_new ();
      tmp_node = G_NODE (GTK_TREE_STORE (tree_model)->root)->children;
    }
  else
    {
      GtkTreeIter tmp_iter = *iter;

      tmp_iter.user_data = G_NODE (iter->user_data)->parent;

      retval = gtk_tree_store_get_path (tree_model,
					&tmp_iter);
      tmp_node = G_NODE (iter->user_data)->parent->children;
    }

  if (retval == NULL)
    return NULL;

  if (tmp_node == NULL)
    {
      gtk_tree_path_free (retval);
      return NULL;
    }

  for (; tmp_node; tmp_node = tmp_node->next)
    {
      if (tmp_node == G_NODE (iter->user_data))
	break;
      i++;
    }

  if (tmp_node == NULL)
    {
      /* We couldn't find node, meaning it's prolly not ours */
      /* Perhaps I should do a g_return_if_fail here. */
      gtk_tree_path_free (retval);
      return NULL;
    }

  gtk_tree_path_append_index (retval, i);

  return retval;
}


static void
gtk_tree_store_get_value (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  GtkTreeDataList *list;
  gint tmp_column = column;

  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (tree_model));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (column < GTK_TREE_STORE (tree_model)->n_columns);

  list = G_NODE (iter->user_data)->data;

  while (tmp_column-- > 0 && list)
    list = list->next;

  if (list)
    {
      _gtk_tree_data_list_node_to_value (list,
					 GTK_TREE_STORE (tree_model)->column_headers[column],
					 value);
    }
  else
    {
      /* We want to return an initialized but empty (default) value */
      g_value_init (value, GTK_TREE_STORE (tree_model)->column_headers[column]);
    }
}

static gboolean
gtk_tree_store_iter_next (GtkTreeModel  *tree_model,
			  GtkTreeIter   *iter)
{
  g_return_val_if_fail (iter->user_data != NULL, FALSE);

  if (G_NODE (iter->user_data)->next)
    {
      iter->user_data = G_NODE (iter->user_data)->next;
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_tree_store_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
  GNode *children;

  g_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);

  if (parent)
    children = G_NODE (parent->user_data)->children;
  else
    children = G_NODE (GTK_TREE_STORE (tree_model)->root)->children;

  if (children)
    {
      iter->stamp = GTK_TREE_STORE (tree_model)->stamp;
      iter->user_data = children;
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_tree_store_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  g_return_val_if_fail (tree_model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (iter->user_data != NULL, FALSE);

  return G_NODE (iter->user_data)->children != NULL;
}

static gint
gtk_tree_store_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  GNode *node;
  gint i = 0;

  g_return_val_if_fail (tree_model != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), 0);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (iter->user_data != NULL, FALSE);

  if (iter == NULL)
    node = G_NODE (GTK_TREE_STORE (tree_model)->root)->children;
  else
    node = G_NODE (iter->user_data)->children;

  while (node)
    {
      i++;
      node = node->next;
    }

  return i;
}

static gboolean
gtk_tree_store_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
  GNode *parent_node;
  GNode *child;

  g_return_val_if_fail (tree_model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), FALSE);
  g_return_val_if_fail (parent == NULL || parent->user_data != NULL, FALSE);

  if (parent == NULL)
    parent_node = GTK_TREE_STORE (tree_model)->root;
  else
    parent_node = parent->user_data;

  child = g_node_nth_child (parent_node, n);

  if (child)
    {
      iter->user_data = child;
      iter->stamp = GTK_TREE_STORE (tree_model)->stamp;
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_tree_store_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  GNode *parent;

  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (iter->user_data != NULL, FALSE);

  parent = G_NODE (child->user_data)->parent;

  g_assert (parent != NULL);

  if (parent != GTK_TREE_STORE (tree_model)->root)
    {
      iter->user_data = parent;
      iter->stamp = GTK_TREE_STORE (tree_model)->stamp;
      return TRUE;
    }
  else
    return FALSE;
}

/*
 * This is a somewhat inelegant function that does a lot of list
 * manipulations on it's own.
 */
void
gtk_tree_store_set_value (GtkTreeStore *tree_store,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  GtkTreeDataList *list;
  GtkTreeDataList *prev;
  GtkTreePath *path = NULL;
  GValue real_value = {0, };
  gboolean converted = FALSE;

  g_return_if_fail (tree_store != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));
  g_return_if_fail (column >= 0 && column < tree_store->n_columns);
  g_return_if_fail (G_IS_VALUE (value));

  if (! g_type_is_a (G_VALUE_TYPE (value), tree_store->column_headers[column]))
    {
      if (! (g_value_type_compatible (G_VALUE_TYPE (value), tree_store->column_headers[column]) &&
	     g_value_type_compatible (tree_store->column_headers[column], G_VALUE_TYPE (value))))
	{
	  g_warning ("%s: Unable to convert from %s to %s\n",
		     G_STRLOC,
		     g_type_name (G_VALUE_TYPE (value)),
		     g_type_name (tree_store->column_headers[column]));
	  return;
	}
      if (!g_value_transform (value, &real_value))
	{
	  g_warning ("%s: Unable to make conversion from %s to %s\n",
		     G_STRLOC,
		     g_type_name (G_VALUE_TYPE (value)),
		     g_type_name (tree_store->column_headers[column]));
	  g_value_unset (&real_value);
	  return;
	}
      converted = TRUE;
    }

  prev = list = G_NODE (iter->user_data)->data;

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (tree_store), iter);

  while (list != NULL)
    {
      if (column == 0)
	{
	  if (converted)
	    _gtk_tree_data_list_value_to_node (list, &real_value);
	  else
	    _gtk_tree_data_list_value_to_node (list, value);
	  gtk_tree_model_changed (GTK_TREE_MODEL (tree_store), path, iter);
	  gtk_tree_path_free (path);
	  if (converted)
	    g_value_unset (&real_value);
	  return;
	}

      column--;
      prev = list;
      list = list->next;
    }

  if (G_NODE (iter->user_data)->data == NULL)
    {
      G_NODE (iter->user_data)->data = list = _gtk_tree_data_list_alloc ();
      list->next = NULL;
    }
  else
    {
      list = prev->next = _gtk_tree_data_list_alloc ();
      list->next = NULL;
    }

  while (column != 0)
    {
      list->next = _gtk_tree_data_list_alloc ();
      list = list->next;
      list->next = NULL;
      column --;
    }
  if (converted)
    _gtk_tree_data_list_value_to_node (list, &real_value);
  else
    _gtk_tree_data_list_value_to_node (list, value);
  gtk_tree_model_changed (GTK_TREE_MODEL (tree_store), path, iter);
  gtk_tree_path_free (path);
  if (converted)
    g_value_unset (&real_value);
}

/**
 * gtk_tree_store_set_valist:
 * @tree_store: a #GtkTreeStore
 * @iter: row to set data for
 * @var_args: va_list of column/value pairs
 *
 * See gtk_tree_store_set(); this version takes a va_list for
 * use by language bindings.
 *
 **/
void
gtk_tree_store_set_valist (GtkTreeStore *tree_store,
                           GtkTreeIter  *iter,
                           va_list	var_args)
{
  gint column;

  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));

  column = va_arg (var_args, gint);

  while (column != -1)
    {
      GValue value = { 0, };
      gchar *error = NULL;

      if (column >= tree_store->n_columns)
	{
	  g_warning ("%s: Invalid column number %d added to iter (remember to end your list of columns with a -1)", G_STRLOC, column);
	  break;
	}
      g_value_init (&value, tree_store->column_headers[column]);

      G_VALUE_COLLECT (&value, var_args, 0, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

 	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  break;
	}

      gtk_tree_store_set_value (tree_store,
				iter,
				column,
				&value);

      g_value_unset (&value);

      column = va_arg (var_args, gint);
    }
}

/**
 * gtk_tree_store_set:
 * @tree_store: a #GtkTreeStore
 * @iter: row iterator
 * @Varargs: pairs of column number and value, terminated with -1
 *
 * Sets the value of one or more cells in the row referenced by @iter.
 * The variable argument list should contain integer column numbers,
 * each column number followed by the value to be set. For example,
 * The list is terminated by a -1. For example, to set column 0 with type
 * %G_TYPE_STRING to "Foo", you would write gtk_tree_store_set (store, iter,
 * 0, "Foo", -1).
 **/
void
gtk_tree_store_set (GtkTreeStore *tree_store,
		    GtkTreeIter  *iter,
		    ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));

  va_start (var_args, iter);
  gtk_tree_store_set_valist (tree_store, iter, var_args);
  va_end (var_args);
}

void
gtk_tree_store_remove (GtkTreeStore *model,
		       GtkTreeIter  *iter)
{
  GtkTreePath *path;
  GtkTreeIter new_iter = {0,};
  GNode *parent;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));

  parent = G_NODE (iter->user_data)->parent;

  g_assert (parent != NULL);

  if (G_NODE (iter->user_data)->data)
    _gtk_tree_data_list_free ((GtkTreeDataList *) G_NODE (iter->user_data)->data,
			      model->column_headers);

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
  g_node_destroy (G_NODE (iter->user_data));

  model->stamp++;
  gtk_tree_model_deleted (GTK_TREE_MODEL (model), path);

  if (parent != G_NODE (model->root) && parent->children == NULL)
    {
      gtk_tree_path_up (path);

      new_iter.stamp = model->stamp;
      new_iter.user_data = parent;
      gtk_tree_model_has_child_toggled (GTK_TREE_MODEL (model), path, &new_iter);
    }
  gtk_tree_path_free (path);
}

void
gtk_tree_store_insert (GtkTreeStore *model,
		       GtkTreeIter  *iter,
		       GtkTreeIter  *parent,
		       gint          position)
{
  GtkTreePath *path;
  GNode *parent_node;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));

  if (parent)
    parent_node = parent->user_data;
  else
    parent_node = model->root;

  iter->stamp = model->stamp;
  iter->user_data = g_node_new (NULL);
  g_node_insert (parent_node, position, G_NODE (iter->user_data));

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
  gtk_tree_model_inserted (GTK_TREE_MODEL (model), path, iter);

  gtk_tree_path_free (path);

  validate_tree ((GtkTreeStore*)model);
}

void
gtk_tree_store_insert_before (GtkTreeStore *model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent,
			      GtkTreeIter  *sibling)
{
  GtkTreePath *path;
  GNode *parent_node = NULL;
  GNode *new_node;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));
  g_return_if_fail (iter != NULL);

  new_node = g_node_new (NULL);

  if (parent == NULL && sibling == NULL)
    parent_node = model->root;
  else if (parent == NULL)
    parent_node = G_NODE (sibling->user_data)->parent;
  else if (sibling == NULL)
    parent_node = G_NODE (parent->user_data);
  else
    {
      g_return_if_fail (G_NODE (sibling->user_data)->parent ==
                        G_NODE (parent->user_data));
      parent_node = G_NODE (parent->user_data);
    }

  g_node_insert_before (parent_node,
			sibling ? G_NODE (sibling->user_data) : NULL,
                        new_node);

  iter->stamp = model->stamp;
  iter->user_data = new_node;

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
  gtk_tree_model_inserted (GTK_TREE_MODEL (model), path, iter);

  gtk_tree_path_free (path);

  validate_tree ((GtkTreeStore*)model);
}

void
gtk_tree_store_insert_after (GtkTreeStore *model,
			     GtkTreeIter  *iter,
			     GtkTreeIter  *parent,
			     GtkTreeIter  *sibling)
{
  GtkTreePath *path;
  GNode *parent_node;
  GNode *new_node;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));
  g_return_if_fail (iter != NULL);

  new_node = g_node_new (NULL);

  if (parent == NULL && sibling == NULL)
    parent_node = model->root;
  else if (parent == NULL)
    parent_node = G_NODE (sibling->user_data)->parent;
  else if (sibling == NULL)
    parent_node = G_NODE (parent->user_data);
  else
    {
      g_return_if_fail (G_NODE (sibling->user_data)->parent ==
                        G_NODE (parent->user_data));
      parent_node = G_NODE (parent->user_data);
    }


  g_node_insert_after (parent_node,
		       sibling ? G_NODE (sibling->user_data) : NULL,
                       new_node);

  iter->stamp = model->stamp;
  iter->user_data = new_node;

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
  gtk_tree_model_inserted (GTK_TREE_MODEL (model), path, iter);

  gtk_tree_path_free (path);

  validate_tree ((GtkTreeStore*)model);
}

void
gtk_tree_store_prepend (GtkTreeStore *model,
			GtkTreeIter  *iter,
			GtkTreeIter  *parent)
{
  GNode *parent_node;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));
  g_return_if_fail (iter != NULL);

  if (parent == NULL)
    parent_node = model->root;
  else
    parent_node = parent->user_data;

  if (parent_node->children == NULL)
    {
      GtkTreePath *path;

      iter->stamp = model->stamp;
      iter->user_data = g_node_new (NULL);

      g_node_prepend (parent_node, iter->user_data);

      if (parent_node != model->root)
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), parent);
	  gtk_tree_model_has_child_toggled (GTK_TREE_MODEL (model), path, parent);
	  gtk_tree_path_append_index (path, 0);
	}
      else
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
	}
      gtk_tree_model_inserted (GTK_TREE_MODEL (model), path, iter);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_tree_store_insert_after (model, iter, parent, NULL);
    }

  validate_tree ((GtkTreeStore*)model);
}

void
gtk_tree_store_append (GtkTreeStore *model,
		       GtkTreeIter  *iter,
		       GtkTreeIter  *parent)
{
  GNode *parent_node;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));
  g_return_if_fail (iter != NULL);

  if (parent == NULL)
    parent_node = model->root;
  else
    parent_node = parent->user_data;

  if (parent_node->children == NULL)
    {
      GtkTreePath *path;

      iter->stamp = model->stamp;
      iter->user_data = g_node_new (NULL);

      g_node_append (parent_node, G_NODE (iter->user_data));

      if (parent_node != model->root)
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), parent);
	  gtk_tree_model_has_child_toggled (GTK_TREE_MODEL (model), path, parent);
	  gtk_tree_path_append_index (path, 0);
	}
      else
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
	}

      gtk_tree_model_inserted (GTK_TREE_MODEL (model), path, iter);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_tree_store_insert_before (model, iter, parent, NULL);
    }

  validate_tree ((GtkTreeStore*)model);
}

gboolean
gtk_tree_store_is_ancestor (GtkTreeStore *model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *descendant)
{
  g_return_val_if_fail (model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (descendant != NULL, FALSE);

  return g_node_is_ancestor (G_NODE (iter->user_data),
			     G_NODE (descendant->user_data));
}


gint
gtk_tree_store_iter_depth (GtkTreeStore *model,
			   GtkTreeIter  *iter)
{
  g_return_val_if_fail (model != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), 0);
  g_return_val_if_fail (iter != NULL, 0);

  return g_node_depth (G_NODE (iter->user_data)) - 1;
}

/* DND */



static gboolean
gtk_tree_store_drag_data_delete (GtkTreeDragSource *drag_source,
                                 GtkTreePath       *path)
{
  GtkTreeIter iter;

  g_return_val_if_fail (GTK_IS_TREE_STORE (drag_source), FALSE);

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_source),
                               &iter,
                               path))
    {
      gtk_tree_store_remove (GTK_TREE_STORE (drag_source),
                             &iter);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

static gboolean
gtk_tree_store_drag_data_get (GtkTreeDragSource *drag_source,
                              GtkTreePath       *path,
                              GtkSelectionData  *selection_data)
{
  g_return_val_if_fail (GTK_IS_TREE_STORE (drag_source), FALSE);

  /* Note that we don't need to handle the GTK_TREE_MODEL_ROW
   * target, because the default handler does it for us, but
   * we do anyway for the convenience of someone maybe overriding the
   * default handler.
   */

  if (gtk_selection_data_set_tree_row (selection_data,
                                       GTK_TREE_MODEL (drag_source),
                                       path))
    {
      return TRUE;
    }
  else
    {
      /* FIXME handle text targets at least. */
    }

  return FALSE;
}

static void
copy_node_data (GtkTreeStore *tree_store,
                GtkTreeIter  *src_iter,
                GtkTreeIter  *dest_iter)
{
  GtkTreeDataList *dl = G_NODE (src_iter->user_data)->data;
  GtkTreeDataList *copy_head = NULL;
  GtkTreeDataList *copy_prev = NULL;
  GtkTreeDataList *copy_iter = NULL;
  GtkTreePath *path;
  gint col;

  col = 0;
  while (dl)
    {
      copy_iter = _gtk_tree_data_list_node_copy (dl, tree_store->column_headers[col]);

      if (copy_head == NULL)
        copy_head = copy_iter;

      if (copy_prev)
        copy_prev->next = copy_iter;

      copy_prev = copy_iter;

      dl = dl->next;
      ++col;
    }

  G_NODE (dest_iter->user_data)->data = copy_head;

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (tree_store), dest_iter);
  gtk_tree_model_changed (GTK_TREE_MODEL (tree_store), path, dest_iter);
  gtk_tree_path_free (path);
}

static void
recursive_node_copy (GtkTreeStore *tree_store,
                     GtkTreeIter  *src_iter,
                     GtkTreeIter  *dest_iter)
{
  GtkTreeIter child;
  GtkTreeModel *model;

  model = GTK_TREE_MODEL (tree_store);

  copy_node_data (tree_store, src_iter, dest_iter);

  if (gtk_tree_model_iter_children (model,
                                    &child,
                                    src_iter))
    {
      /* Need to create children and recurse. Note our
       * dependence on persistent iterators here.
       */
      do
        {
          GtkTreeIter copy;

          /* Gee, a really slow algorithm... ;-) FIXME */
          gtk_tree_store_append (tree_store,
                                 &copy,
                                 dest_iter);

          recursive_node_copy (tree_store, &child, &copy);
        }
      while (gtk_tree_model_iter_next (model, &child));
    }
}

static gboolean
gtk_tree_store_drag_data_received (GtkTreeDragDest   *drag_dest,
                                   GtkTreePath       *dest,
                                   GtkSelectionData  *selection_data)
{
  GtkTreeModel *tree_model;
  GtkTreeStore *tree_store;
  GtkTreeModel *src_model = NULL;
  GtkTreePath *src_path = NULL;
  gboolean retval = FALSE;

  g_return_val_if_fail (GTK_IS_TREE_STORE (drag_dest), FALSE);

  tree_model = GTK_TREE_MODEL (drag_dest);
  tree_store = GTK_TREE_STORE (drag_dest);

  validate_tree (tree_store);

  if (gtk_selection_data_get_tree_row (selection_data,
                                       &src_model,
                                       &src_path) &&
      src_model == tree_model)
    {
      /* Copy the given row to a new position */
      GtkTreeIter src_iter;
      GtkTreeIter dest_iter;
      GtkTreePath *prev;

      if (!gtk_tree_model_get_iter (src_model,
                                    &src_iter,
                                    src_path))
        {
          goto out;
        }

      /* Get the path to insert _after_ (dest is the path to insert _before_) */
      prev = gtk_tree_path_copy (dest);

      if (!gtk_tree_path_prev (prev))
        {
          GtkTreeIter dest_parent;
          GtkTreePath *parent;
          GtkTreeIter *dest_parent_p;

          /* dest was the first spot at the current depth; which means
           * we are supposed to prepend.
           */

          /* Get the parent, NULL if parent is the root */
          dest_parent_p = NULL;
          parent = gtk_tree_path_copy (dest);
          if (gtk_tree_path_up (parent))
            {
              gtk_tree_model_get_iter (tree_model,
                                       &dest_parent,
                                       parent);
              dest_parent_p = &dest_parent;
            }
          gtk_tree_path_free (parent);
          parent = NULL;

          gtk_tree_store_prepend (GTK_TREE_STORE (tree_model),
                                  &dest_iter,
                                  dest_parent_p);

          retval = TRUE;
        }
      else
        {
          if (gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model),
                                       &dest_iter,
                                       prev))
            {
              GtkTreeIter tmp_iter = dest_iter;
              gtk_tree_store_insert_after (GTK_TREE_STORE (tree_model),
                                           &dest_iter,
                                           NULL,
                                           &tmp_iter);
              retval = TRUE;

            }
        }

      gtk_tree_path_free (prev);

      /* If we succeeded in creating dest_iter, walk src_iter tree branch,
       * duplicating it below dest_iter.
       */

      if (retval)
        {
          recursive_node_copy (tree_store,
                               &src_iter,
                               &dest_iter);
        }
    }
  else
    {
      /* FIXME maybe add some data targets eventually, or handle text
       * targets in the simple case.
       */

    }

 out:

  if (src_path)
    gtk_tree_path_free (src_path);

  return retval;
}

static gboolean
gtk_tree_store_row_drop_possible (GtkTreeDragDest *drag_dest,
                                  GtkTreeModel    *src_model,
                                  GtkTreePath     *src_path,
                                  GtkTreePath     *dest_path)
{
  /* can only drag to ourselves */
  if (src_model != GTK_TREE_MODEL (drag_dest))
    return FALSE;

  /* Can't drop into ourself. */
  if (gtk_tree_path_is_ancestor (src_path,
                                 dest_path))
    return FALSE;

  /* Can't drop if dest_path's parent doesn't exist */
  {
    GtkTreeIter iter;
    GtkTreePath *tmp = gtk_tree_path_copy (dest_path);

    /* if we can't go up, we know the parent exists, the root
     * always exists.
     */
    if (gtk_tree_path_up (tmp))
      {
        if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_dest),
                                      &iter, tmp))
          {
            if (tmp)
              gtk_tree_path_free (tmp);
            return FALSE;
          }
      }

    if (tmp)
      gtk_tree_path_free (tmp);
  }

  /* Can otherwise drop anywhere. */
  return TRUE;
}


static void
gtk_tree_store_get_sort_column_id (GtkTreeSortable  *sortable,
				   gint             *sort_column_id,
				   GtkTreeSortOrder *order)
{

}

static void
gtk_tree_store_set_sort_column_id (GtkTreeSortable  *sortable,
				   gint              sort_column_id,
				   GtkTreeSortOrder  order)
{

}

static void
gtk_tree_store_sort_column_id_set_func (GtkTreeSortable        *sortable,
					gint                    sort_column_id,
					GtkTreeIterCompareFunc  func,
					gpointer                data,
					GtkDestroyNotify        destroy)
{

}

static void
validate_gnode (GNode* node)
{
  GNode *iter;

  iter = node->children;
  while (iter != NULL)
    {
      g_assert (iter->parent == node);
      if (iter->prev)
        g_assert (iter->prev->next == iter);
      validate_gnode (iter);
      iter = iter->next;
    }
}


