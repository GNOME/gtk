/* gtkliststore.c
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

#include <string.h>
#include "gtktreemodel.h"
#include "gtkliststore.h"
#include "gtktreedatalist.h"
#include "gtksignal.h"
#include "gtktreednd.h"
#include <gobject/gvaluecollector.h>

#define G_SLIST(x) ((GSList *) x)
#define GTK_LIST_STORE_IS_SORTED(list) (GTK_LIST_STORE (list)->sort_column_id != -1)

static void         gtk_list_store_init            (GtkListStore      *list_store);
static void         gtk_list_store_class_init      (GtkListStoreClass *class);
static void         gtk_list_store_tree_model_init (GtkTreeModelIface *iface);
static void         gtk_list_store_drag_source_init(GtkTreeDragSourceIface *iface);
static void         gtk_list_store_drag_dest_init  (GtkTreeDragDestIface   *iface);
static void         gtk_list_store_sortable_init   (GtkTreeSortableIface   *iface);
static guint        gtk_list_store_get_flags       (GtkTreeModel      *tree_model);
static gint         gtk_list_store_get_n_columns   (GtkTreeModel      *tree_model);
static GType        gtk_list_store_get_column_type (GtkTreeModel      *tree_model,
						    gint               index);
static gboolean     gtk_list_store_get_iter        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreePath       *path);
static GtkTreePath *gtk_list_store_get_path        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static void         gtk_list_store_get_value       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    gint               column,
						    GValue            *value);
static gboolean     gtk_list_store_iter_next       (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_list_store_iter_children   (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent);
static gboolean     gtk_list_store_iter_has_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gint         gtk_list_store_iter_n_children (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static gboolean     gtk_list_store_iter_nth_child  (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent,
						    gint               n);
static gboolean     gtk_list_store_iter_parent     (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *child);


static void gtk_list_store_set_n_columns   (GtkListStore *list_store,
					    gint          n_columns);
static void gtk_list_store_set_column_type (GtkListStore *list_store,
					    gint          column,
					    GType         type);


/* Drag and Drop */
static gboolean gtk_list_store_drag_data_delete   (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path);
static gboolean gtk_list_store_drag_data_get      (GtkTreeDragSource *drag_source,
                                                   GtkTreePath       *path,
                                                   GtkSelectionData  *selection_data);
static gboolean gtk_list_store_drag_data_received (GtkTreeDragDest   *drag_dest,
                                                   GtkTreePath       *dest,
                                                   GtkSelectionData  *selection_data);
static gboolean gtk_list_store_row_drop_possible  (GtkTreeDragDest   *drag_dest,
                                                   GtkTreeModel      *src_model,
                                                   GtkTreePath       *src_path,
                                                   GtkTreePath       *dest_path);


/* sortable */
static void     gtk_list_store_sort               (GtkListStore           *list_store);
static void     gtk_list_store_sort_iter_changed  (GtkListStore           *list_store,
						   GtkTreeIter            *iter,
						   gint                    column);
static gboolean gtk_list_store_get_sort_column_id (GtkTreeSortable        *sortable,
						   gint                   *sort_column_id,
						   GtkTreeSortOrder       *order);
static void     gtk_list_store_set_sort_column_id (GtkTreeSortable        *sortable,
						   gint                    sort_column_id,
						   GtkTreeSortOrder        order);
static void     gtk_list_store_set_sort_func      (GtkTreeSortable        *sortable,
						   gint                    sort_column_id,
						   GtkTreeIterCompareFunc  func,
						   gpointer                data,
						   GtkDestroyNotify        destroy);


static void
validate_list_store (GtkListStore *list_store)
{
  if (gtk_debug_flags & GTK_DEBUG_TREE)
    {
      g_assert (g_slist_length (list_store->root) == list_store->length);

      g_assert (g_slist_last (list_store->root) == list_store->tail);
    }
}

GtkType
gtk_list_store_get_type (void)
{
  static GType list_store_type = 0;

  if (!list_store_type)
    {
      static const GTypeInfo list_store_info =
      {
	sizeof (GtkListStoreClass),
	NULL,		/* base_init */
	NULL,		/* base_finalize */
        (GClassInitFunc) gtk_list_store_class_init,
	NULL,		/* class_finalize */
	NULL,		/* class_data */
        sizeof (GtkListStore),
	0,
        (GInstanceInitFunc) gtk_list_store_init,
      };

      static const GInterfaceInfo tree_model_info =
      {
	(GInterfaceInitFunc) gtk_list_store_tree_model_init,
	NULL,
	NULL
      };

      static const GInterfaceInfo drag_source_info =
      {
	(GInterfaceInitFunc) gtk_list_store_drag_source_init,
	NULL,
	NULL
      };

      static const GInterfaceInfo drag_dest_info =
      {
	(GInterfaceInitFunc) gtk_list_store_drag_dest_init,
	NULL,
	NULL
      };

      static const GInterfaceInfo sortable_info =
      {
	(GInterfaceInitFunc) gtk_list_store_sortable_init,
	NULL,
	NULL
      };

      list_store_type = g_type_register_static (G_TYPE_OBJECT, "GtkListStore", &list_store_info, 0);
      g_type_add_interface_static (list_store_type,
				   GTK_TYPE_TREE_MODEL,
				   &tree_model_info);
      g_type_add_interface_static (list_store_type,
				   GTK_TYPE_TREE_DRAG_SOURCE,
				   &drag_source_info);
      g_type_add_interface_static (list_store_type,
				   GTK_TYPE_TREE_DRAG_DEST,
				   &drag_dest_info);
      g_type_add_interface_static (list_store_type,
				   GTK_TYPE_TREE_SORTABLE,
				   &sortable_info);
    }

  return list_store_type;
}

static void
gtk_list_store_class_init (GtkListStoreClass *class)
{
  GObjectClass *object_class;

  object_class = (GObjectClass*) class;
}

static void
gtk_list_store_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_list_store_get_flags;
  iface->get_n_columns = gtk_list_store_get_n_columns;
  iface->get_column_type = gtk_list_store_get_column_type;
  iface->get_iter = gtk_list_store_get_iter;
  iface->get_path = gtk_list_store_get_path;
  iface->get_value = gtk_list_store_get_value;
  iface->iter_next = gtk_list_store_iter_next;
  iface->iter_children = gtk_list_store_iter_children;
  iface->iter_has_child = gtk_list_store_iter_has_child;
  iface->iter_n_children = gtk_list_store_iter_n_children;
  iface->iter_nth_child = gtk_list_store_iter_nth_child;
  iface->iter_parent = gtk_list_store_iter_parent;
}

static void
gtk_list_store_drag_source_init (GtkTreeDragSourceIface *iface)
{
  iface->drag_data_delete = gtk_list_store_drag_data_delete;
  iface->drag_data_get = gtk_list_store_drag_data_get;
}

static void
gtk_list_store_drag_dest_init (GtkTreeDragDestIface *iface)
{
  iface->drag_data_received = gtk_list_store_drag_data_received;
  iface->row_drop_possible = gtk_list_store_row_drop_possible;
}

static void
gtk_list_store_sortable_init (GtkTreeSortableIface *iface)
{
  iface->get_sort_column_id = gtk_list_store_get_sort_column_id;
  iface->set_sort_column_id = gtk_list_store_set_sort_column_id;
  iface->set_sort_func = gtk_list_store_set_sort_func;
}

static void
gtk_list_store_init (GtkListStore *list_store)
{
  list_store->root = NULL;
  list_store->tail = NULL;
  list_store->sort_list = NULL;
  list_store->stamp = g_random_int ();
  list_store->length = 0;
  list_store->sort_column_id = -1;
}

/**
 * gtk_list_store_new:
 * @n_columns: number of columns in the list store
 * @Varargs: all #GType types for the columns, from first to last
 *
 * Creates a new list store as with @n_columns columns each of the types passed
 * in.  As an example, gtk_tree_store_new (3, G_TYPE_INT, G_TYPE_STRING,
 * GDK_TYPE_PIXBUF); will create a new GtkListStore with three columns, of type
 * int, string and GDkPixbuf respectively.
 *
 * Return value: a new #GtkListStore
 **/
GtkListStore *
gtk_list_store_new (gint n_columns,
			       ...)
{
  GtkListStore *retval;
  va_list args;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = GTK_LIST_STORE (g_object_new (gtk_list_store_get_type (), NULL));
  gtk_list_store_set_n_columns (retval, n_columns);

  va_start (args, n_columns);

  for (i = 0; i < n_columns; i++)
    {
      GType type = va_arg (args, GType);
      if (! _gtk_tree_data_list_check_type (type))
	{
	  g_warning ("%s: Invalid type %s passed to gtk_list_store_new_with_types\n", G_STRLOC, g_type_name (type));
	  g_object_unref (G_OBJECT (retval));
	  return NULL;
	}

      gtk_list_store_set_column_type (retval, i, type);
    }

  va_end (args);

  return retval;
}

static void
gtk_list_store_set_n_columns (GtkListStore *list_store,
			      gint          n_columns)
{
  GType *new_columns;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (n_columns > 0);

  if (list_store->n_columns == n_columns)
    return;

  new_columns = g_new0 (GType, n_columns);
  if (list_store->column_headers)
    {
      /* copy the old header orders over */
      if (n_columns >= list_store->n_columns)
	memcpy (new_columns, list_store->column_headers, list_store->n_columns * sizeof (gchar *));
      else
	memcpy (new_columns, list_store->column_headers, n_columns * sizeof (GType));

      g_free (list_store->column_headers);
    }

  if (list_store->sort_list)
    _gtk_tree_data_list_header_free (list_store->sort_list);

  list_store->sort_list = _gtk_tree_data_list_header_new (n_columns, list_store->column_headers);

  list_store->column_headers = new_columns;
  list_store->n_columns = n_columns;
}

static void
gtk_list_store_set_column_type (GtkListStore *list_store,
				gint          column,
				GType         type)
{
  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (column >=0 && column < list_store->n_columns);
  if (!_gtk_tree_data_list_check_type (type))
    {
      g_warning ("%s: Invalid type %s passed to gtk_list_store_new_with_types\n", G_STRLOC, g_type_name (type));
      return;
    }

  list_store->column_headers[column] = type;
}

/* Fulfill the GtkTreeModel requirements */
static guint
gtk_list_store_get_flags (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), 0);

  return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint
gtk_list_store_get_n_columns (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), 0);

  return GTK_LIST_STORE (tree_model)->n_columns;
}

static GType
gtk_list_store_get_column_type (GtkTreeModel *tree_model,
				gint          index)
{
  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), G_TYPE_INVALID);
  g_return_val_if_fail (index < GTK_LIST_STORE (tree_model)->n_columns &&
			index >= 0, G_TYPE_INVALID);

  return GTK_LIST_STORE (tree_model)->column_headers[index];
}

static gboolean
gtk_list_store_get_iter (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter,
			 GtkTreePath  *path)
{
  GSList *list;
  gint i;

  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);

  i = gtk_tree_path_get_indices (path)[0];

  if (i >= GTK_LIST_STORE (tree_model)->length)
    return FALSE;

  list = g_slist_nth (G_SLIST (GTK_LIST_STORE (tree_model)->root), i);

  /* If this fails, list_store->length has gotten mangled. */
  g_assert (list);

  iter->stamp = GTK_LIST_STORE (tree_model)->stamp;
  iter->user_data = list;
  return TRUE;
}

static GtkTreePath *
gtk_list_store_get_path (GtkTreeModel *tree_model,
			 GtkTreeIter  *iter)
{
  GtkTreePath *retval;
  GSList *list;
  gint i = 0;

  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), NULL);
  g_return_val_if_fail (iter->stamp == GTK_LIST_STORE (tree_model)->stamp, NULL);

  for (list = G_SLIST (GTK_LIST_STORE (tree_model)->root); list; list = list->next)
    {
      if (list == G_SLIST (iter->user_data))
	break;
      i++;
    }
  if (list == NULL)
    return NULL;

  retval = gtk_tree_path_new ();
  gtk_tree_path_append_index (retval, i);
  return retval;
}

static void
gtk_list_store_get_value (GtkTreeModel *tree_model,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  GtkTreeDataList *list;
  gint tmp_column = column;

  g_return_if_fail (GTK_IS_LIST_STORE (tree_model));
  g_return_if_fail (column < GTK_LIST_STORE (tree_model)->n_columns);
  g_return_if_fail (GTK_LIST_STORE (tree_model)->stamp == iter->stamp);

  list = G_SLIST (iter->user_data)->data;

  while (tmp_column-- > 0 && list)
    list = list->next;

  if (list == NULL)
    g_value_init (value, GTK_LIST_STORE (tree_model)->column_headers[column]);
  else
    _gtk_tree_data_list_node_to_value (list,
				       GTK_LIST_STORE (tree_model)->column_headers[column],
				       value);
}

static gboolean
gtk_list_store_iter_next (GtkTreeModel  *tree_model,
			  GtkTreeIter   *iter)
{
  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), FALSE);
  g_return_val_if_fail (GTK_LIST_STORE (tree_model)->stamp == iter->stamp, FALSE);

  iter->user_data = G_SLIST (iter->user_data)->next;

  return (iter->user_data != NULL);
}

static gboolean
gtk_list_store_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
  /* this is a list, nodes have no children */
  if (parent)
    return FALSE;

  /* but if parent == NULL we return the list itself as children of the
   * "root"
   */

  if (GTK_LIST_STORE (tree_model)->root)
    {
      iter->stamp = GTK_LIST_STORE (tree_model)->stamp;
      iter->user_data = GTK_LIST_STORE (tree_model)->root;
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_list_store_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  return FALSE;
}

static gint
gtk_list_store_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  g_return_val_if_fail (GTK_LIST_STORE (tree_model)->stamp == iter->stamp, -1);

  if (iter->user_data == NULL)
    return GTK_LIST_STORE (tree_model)->length;
  else
    return 0;
}

static gboolean
gtk_list_store_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
  GSList *child;

  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), FALSE);

  if (parent)
    return FALSE;

  child = g_slist_nth (G_SLIST (GTK_LIST_STORE (tree_model)->root), n);

  if (child)
    {
      iter->stamp = GTK_LIST_STORE (tree_model)->stamp;
      iter->user_data = child;
      return TRUE;
    }
  else
    return FALSE;
}

static gboolean
gtk_list_store_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  return FALSE;
}

/* Public accessors */
/* This is a somewhat inelegant function that does a lot of list
 * manipulations on it's own.
 */

/**
 * gtk_list_store_set_value:
 * @store: a #GtkListStore
 * @iter: iterator for the row you're modifying
 * @column: column number to modify
 * @value: new value for the cell
 *
 * Sets the data in the cell specified by @iter and @column.
 * The type of @value must be convertible to the type of the
 * column.
 *
 **/
void
gtk_list_store_set_value (GtkListStore *list_store,
			  GtkTreeIter  *iter,
			  gint          column,
			  GValue       *value)
{
  GtkTreeDataList *list;
  GtkTreeDataList *prev;
  GtkTreePath *path;
  GValue real_value = {0, };
  gboolean converted = FALSE;
  gint orig_column = column;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (column >= 0 && column < list_store->n_columns);
  g_return_if_fail (G_IS_VALUE (value));

  if (! g_type_is_a (G_VALUE_TYPE (value), list_store->column_headers[column]))
    {
      if (! (g_value_type_compatible (G_VALUE_TYPE (value), list_store->column_headers[column]) &&
	     g_value_type_compatible (list_store->column_headers[column], G_VALUE_TYPE (value))))
	{
	  g_warning ("%s: Unable to convert from %s to %s\n",
		     G_STRLOC,
		     g_type_name (G_VALUE_TYPE (value)),
		     g_type_name (list_store->column_headers[column]));
	  return;
	}
      if (!g_value_transform (value, &real_value))
	{
	  g_warning ("%s: Unable to make conversion from %s to %s\n",
		     G_STRLOC,
		     g_type_name (G_VALUE_TYPE (value)),
		     g_type_name (list_store->column_headers[column]));
	  g_value_unset (&real_value);
	  return;
	}
      converted = TRUE;
    }

  prev = list = G_SLIST (iter->user_data)->data;

  while (list != NULL)
    {
      if (column == 0)
	{
	  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
	  if (converted)
	    _gtk_tree_data_list_value_to_node (list, &real_value);
	  else
	    _gtk_tree_data_list_value_to_node (list, value);
	  gtk_tree_model_range_changed (GTK_TREE_MODEL (list_store), path, iter, path, iter);
	  gtk_tree_path_free (path);
	  if (converted)
	    g_value_unset (&real_value);
	  return;
	}

      column--;
      prev = list;
      list = list->next;
    }

  if (G_SLIST (iter->user_data)->data == NULL)
    {
      G_SLIST (iter->user_data)->data = list = _gtk_tree_data_list_alloc ();
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

  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
  if (converted)
    _gtk_tree_data_list_value_to_node (list, &real_value);
  else
    _gtk_tree_data_list_value_to_node (list, value);
  gtk_tree_model_range_changed (GTK_TREE_MODEL (list_store), path, iter, path, iter);
  gtk_tree_path_free (path);
  if (converted)
    g_value_unset (&real_value);

  if (GTK_LIST_STORE_IS_SORTED (list_store))
    gtk_list_store_sort_iter_changed (list_store, iter, orig_column);
}

/**
 * gtk_list_store_set_valist:
 * @list_store: a #GtkListStore
 * @iter: row to set data for
 * @var_args: va_list of column/value pairs
 *
 * See gtk_list_store_set(); this version takes a va_list for
 * use by language bindings.
 *
 **/
void
gtk_list_store_set_valist (GtkListStore *list_store,
                           GtkTreeIter  *iter,
                           va_list	 var_args)
{
  gint column;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  column = va_arg (var_args, gint);

  while (column != -1)
    {
      GValue value = { 0, };
      gchar *error = NULL;

      if (column >= list_store->n_columns)
	{
	  g_warning ("%s: Invalid column number %d added to iter (remember to end your list of columns with a -1)", G_STRLOC, column);
	  break;
	}
      g_value_init (&value, list_store->column_headers[column]);

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

      /* FIXME: instead of calling this n times, refactor with above */
      gtk_list_store_set_value (list_store,
				iter,
				column,
				&value);

      g_value_unset (&value);

      column = va_arg (var_args, gint);
    }
}

/**
 * gtk_list_store_set:
 * @list_store: a #GtkListStore
 * @iter: row iterator
 * @Varargs: pairs of column number and value, terminated with -1
 *
 * Sets the value of one or more cells in the row referenced by @iter.
 * The variable argument list should contain integer column numbers,
 * each column number followed by the value to be set.
 * The list is terminated by a -1. For example, to set column 0 with type
 * %G_TYPE_STRING to "Foo", you would write gtk_list_store_set (store, iter,
 * 0, "Foo", -1).
 **/
void
gtk_list_store_set (GtkListStore *list_store,
		    GtkTreeIter  *iter,
		    ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  va_start (var_args, iter);
  gtk_list_store_set_valist (list_store, iter, var_args);
  va_end (var_args);
}

static GSList*
remove_link_saving_prev (GSList  *list,
                         GSList  *link,
                         GSList **prevp)
{
  GSList *tmp;
  GSList *prev;

  prev = NULL;
  tmp = list;

  while (tmp)
    {
      if (tmp == link)
	{
	  if (prev)
	    prev->next = link->next;

	  if (list == link)
	    list = list->next;

	  link->next = NULL;
	  break;
	}

      prev = tmp;
      tmp = tmp->next;
    }

  *prevp = prev;

  return list;
}

static void
gtk_list_store_remove_silently (GtkListStore *list_store,
                                GtkTreeIter  *iter,
                                GtkTreePath  *path)
{
  if (G_SLIST (iter->user_data)->data)
    {
      _gtk_tree_data_list_free ((GtkTreeDataList *) G_SLIST (iter->user_data)->data,
                                list_store->column_headers);
      G_SLIST (iter->user_data)->data = NULL;
    }

  {
    GSList *prev = NULL;

    list_store->root = remove_link_saving_prev (G_SLIST (list_store->root),
                                                G_SLIST (iter->user_data),
                                                &prev);

    list_store->length -= 1;

    if (iter->user_data == list_store->tail)
      list_store->tail = prev;
  }

  list_store->stamp ++;
}

/**
 * gtk_list_store_remove:
 * @store: a #GtkListStore
 * @iter: a row in @list_store
 *
 * Removes the given row from the list store, emitting the
 * "deleted" signal on #GtkTreeModel.
 *
 **/
void
gtk_list_store_remove (GtkListStore *list_store,
		       GtkTreeIter  *iter)
{
  GtkTreePath *path;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter->user_data != NULL);

  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);

  validate_list_store (list_store);

  gtk_list_store_remove_silently (list_store, iter, path);

  validate_list_store (list_store);

  list_store->stamp ++;
  gtk_tree_model_deleted (GTK_TREE_MODEL (list_store), path);
  gtk_tree_path_free (path);
}

static void
insert_after (GtkListStore *list_store,
              GSList       *sibling,
              GSList       *new_list)
{
  g_return_if_fail (sibling != NULL);
  g_return_if_fail (new_list != NULL);

  /* insert new node after list */
  new_list->next = sibling->next;
  sibling->next = new_list;

  /* if list was the tail, the new node is the new tail */
  if (sibling == ((GSList *) list_store->tail))
    list_store->tail = new_list;

  list_store->length += 1;
}

/**
 * gtk_list_store_insert:
 * @store: a #GtkListStore
 * @iter: iterator to initialize with the new row
 * @position: position to insert the new row
 *
 * Creates a new row at @position, initializing @iter to point to the
 * new row, and emitting the "inserted" signal from the #GtkTreeModel
 * interface.
 *
 **/
void
gtk_list_store_insert (GtkListStore *list_store,
		       GtkTreeIter  *iter,
		       gint          position)
{
  GSList *list;
  GtkTreePath *path;
  GSList *new_list;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (position >= 0);

  if (position == 0 ||
      GTK_LIST_STORE_IS_SORTED (list_store))
    {
      gtk_list_store_prepend (list_store, iter);
      return;
    }

  new_list = g_slist_alloc ();

  list = g_slist_nth (G_SLIST (list_store->root), position - 1);

  if (list == NULL)
    {
      g_warning ("%s: position %d is off the end of the list\n", G_STRLOC, position);
      return;
    }

  insert_after (list_store, list, new_list);

  iter->stamp = list_store->stamp;
  iter->user_data = new_list;

  validate_list_store (list_store);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, position);
  gtk_tree_model_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}

/**
 * gtk_list_store_insert_before:
 * @store: a #GtkListStore
 * @iter: iterator to initialize with the new row
 * @sibling: an existing row
 *
 * Inserts a new row before @sibling, initializing @iter to point to
 * the new row, and emitting the "inserted" signal from the
 * #GtkTreeModel interface.
 *
 **/
void
gtk_list_store_insert_before (GtkListStore *list_store,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *sibling)
{
  GtkTreePath *path;
  GSList *list, *prev, *new_list;
  gint i = 0;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  if (GTK_LIST_STORE_IS_SORTED (list_store))
    {
      gtk_list_store_prepend (list_store, iter);
      return;
    }

  if (sibling == NULL)
    {
      gtk_list_store_append (list_store, iter);
      return;
    }

  new_list = g_slist_alloc ();

  prev = NULL;
  list = list_store->root;
  while (list && list != sibling->user_data)
    {
      prev = list;
      list = list->next;
      i++;
    }

  if (list != sibling->user_data)
    {
      g_warning ("%s: sibling iterator invalid? not found in the list", G_STRLOC);
      return;
    }

  /* if there are no nodes, we become the list tail, otherwise we
   * are inserting before any existing nodes so we can't change
   * the tail
   */

  if (list_store->root == NULL)
    list_store->tail = new_list;

  if (prev)
    {
      new_list->next = prev->next;
      prev->next = new_list;
    }
  else
    {
      new_list->next = list_store->root;
      list_store->root = new_list;
    }

  iter->stamp = list_store->stamp;
  iter->user_data = new_list;

  list_store->length += 1;

  validate_list_store (list_store);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, i);
  gtk_tree_model_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}

/**
 * gtk_list_store_insert_after:
 * @store: a #GtkListStore
 * @iter: iterator to initialize with the new row
 * @sibling: an existing row
 *
 * Inserts a new row after @sibling, initializing @iter to point to
 * the new row, and emitting the "inserted" signal from the
 * #GtkTreeModel interface.
 *
 **/
void
gtk_list_store_insert_after (GtkListStore *list_store,
			     GtkTreeIter  *iter,
			     GtkTreeIter  *sibling)
{
  GtkTreePath *path;
  GSList *list, *new_list;
  gint i = 0;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);
  if (sibling)
    g_return_if_fail (sibling->stamp == list_store->stamp);

  if (sibling == NULL ||
      GTK_LIST_STORE_IS_SORTED (list_store))
    {
      gtk_list_store_prepend (list_store, iter);
      return;
    }

  for (list = list_store->root; list && list != sibling->user_data; list = list->next)
    i++;

  g_return_if_fail (list == sibling->user_data);

  new_list = g_slist_alloc ();

  insert_after (list_store, list, new_list);

  iter->stamp = list_store->stamp;
  iter->user_data = new_list;

  validate_list_store (list_store);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, i);
  gtk_tree_model_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}

/**
 * gtk_list_store_prepend:
 * @store: a #GtkListStore
 * @iter: iterator to initialize with new row
 *
 * Prepends a row to @store, initializing @iter to point to the
 * new row, and emitting the "inserted" signal on the #GtkTreeModel
 * interface for the @store.
 *
 **/
void
gtk_list_store_prepend (GtkListStore *list_store,
			GtkTreeIter  *iter)
{
  GtkTreePath *path;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  iter->stamp = list_store->stamp;
  iter->user_data = g_slist_alloc ();

  if (list_store->root == NULL)
    list_store->tail = iter->user_data;

  G_SLIST (iter->user_data)->next = G_SLIST (list_store->root);
  list_store->root = iter->user_data;

  list_store->length += 1;

  validate_list_store (list_store);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, 0);
  gtk_tree_model_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}

/**
 * gtk_list_store_append:
 * @store: a #GtkListStore
 * @iter: iterator to initialize with the new row
 *
 * Appends a row to @store, initializing @iter to point to the
 * new row, and emitting the "inserted" signal on the #GtkTreeModel
 * interface for the @store.
 *
 **/
void
gtk_list_store_append (GtkListStore *list_store,
		       GtkTreeIter  *iter)
{
  GtkTreePath *path;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  if (GTK_LIST_STORE_IS_SORTED (list_store))
    {
      gtk_list_store_prepend (list_store, iter);
      return;
    }

  iter->stamp = list_store->stamp;
  iter->user_data = g_slist_alloc ();

  if (list_store->tail)
    ((GSList *)list_store->tail)->next = iter->user_data;
  else
    list_store->root = iter->user_data;

  list_store->tail = iter->user_data;

  list_store->length += 1;

  validate_list_store (list_store);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, list_store->length - 1);
  gtk_tree_model_inserted (GTK_TREE_MODEL (list_store), path, iter);
  gtk_tree_path_free (path);
}

void
gtk_list_store_clear (GtkListStore *list_store)
{
  GtkTreeIter iter;
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  while (list_store->root)
    {
      iter.stamp = list_store->stamp;
      iter.user_data = list_store->root;
      gtk_list_store_remove (list_store, &iter);
    }
}


static gboolean
gtk_list_store_drag_data_delete (GtkTreeDragSource *drag_source,
                                 GtkTreePath       *path)
{
  GtkTreeIter iter;
  g_return_val_if_fail (GTK_IS_LIST_STORE (drag_source), FALSE);

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_source),
                               &iter,
                               path))
    {
      gtk_list_store_remove (GTK_LIST_STORE (drag_source), &iter);
      return TRUE;
    }
  return FALSE;
}

static gboolean
gtk_list_store_drag_data_get (GtkTreeDragSource *drag_source,
                              GtkTreePath       *path,
                              GtkSelectionData  *selection_data)
{
  g_return_val_if_fail (GTK_IS_LIST_STORE (drag_source), FALSE);

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

static gboolean
gtk_list_store_drag_data_received (GtkTreeDragDest   *drag_dest,
                                   GtkTreePath       *dest,
                                   GtkSelectionData  *selection_data)
{
  GtkTreeModel *tree_model;
  GtkListStore *list_store;
  GtkTreeModel *src_model = NULL;
  GtkTreePath *src_path = NULL;
  gboolean retval = FALSE;

  g_return_val_if_fail (GTK_IS_LIST_STORE (drag_dest), FALSE);

  tree_model = GTK_TREE_MODEL (drag_dest);
  list_store = GTK_LIST_STORE (drag_dest);

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
          /* dest was the first spot in the list; which means we are supposed
           * to prepend.
           */
          gtk_list_store_prepend (GTK_LIST_STORE (tree_model),
                                  &dest_iter);

          retval = TRUE;
        }
      else
        {
          if (gtk_tree_model_get_iter (GTK_TREE_MODEL (tree_model),
                                       &dest_iter,
                                       prev))
            {
              GtkTreeIter tmp_iter = dest_iter;
              gtk_list_store_insert_after (GTK_LIST_STORE (tree_model),
                                           &dest_iter,
                                           &tmp_iter);
              retval = TRUE;
            }
        }

      gtk_tree_path_free (prev);

      /* If we succeeded in creating dest_iter, copy data from src
       */
      if (retval)
        {
          GtkTreeDataList *dl = G_SLIST (src_iter.user_data)->data;
          GtkTreeDataList *copy_head = NULL;
          GtkTreeDataList *copy_prev = NULL;
          GtkTreeDataList *copy_iter = NULL;
	  GtkTreePath *path;
          gint col;

          col = 0;
          while (dl)
            {
              copy_iter = _gtk_tree_data_list_node_copy (dl,
                                                         list_store->column_headers[col]);

              if (copy_head == NULL)
                copy_head = copy_iter;

              if (copy_prev)
                copy_prev->next = copy_iter;

              copy_prev = copy_iter;

              dl = dl->next;
              ++col;
            }

	  dest_iter.stamp = GTK_LIST_STORE (tree_model)->stamp;
          G_SLIST (dest_iter.user_data)->data = copy_head;

	  path = gtk_list_store_get_path (GTK_TREE_MODEL (tree_model), &dest_iter);
	  gtk_tree_model_range_changed (GTK_TREE_MODEL (tree_model), path, &dest_iter, path, &dest_iter);
	  gtk_tree_path_free (path);
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
gtk_list_store_row_drop_possible (GtkTreeDragDest *drag_dest,
                                  GtkTreeModel    *src_model,
                                  GtkTreePath     *src_path,
                                  GtkTreePath     *dest_path)
{
  gint *indices;

  g_return_val_if_fail (GTK_IS_LIST_STORE (drag_dest), FALSE);

  if (src_model != GTK_TREE_MODEL (drag_dest))
    return FALSE;

  if (gtk_tree_path_get_depth (dest_path) != 1)
    return FALSE;

  /* can drop before any existing node, or before one past any existing. */

  indices = gtk_tree_path_get_indices (dest_path);

  if (indices[0] <= GTK_LIST_STORE (drag_dest)->length)
    return TRUE;
  else
    return FALSE;
}


/* Sorting */
typedef struct _SortTuple
{
  gint offset;
  GSList *el;
} SortTuple;

static gint
gtk_list_store_compare_func (gconstpointer a,
			     gconstpointer b,
			     gpointer      user_data)
{
  GtkListStore *list_store = user_data;
  GtkTreeDataSortHeader *header = NULL;
  GSList *el_a; /* Los Angeles? */
  GSList *el_b;
  GtkTreeIter iter_a;
  GtkTreeIter iter_b;
  gint retval;

  header = _gtk_tree_data_list_get_header (list_store->sort_list,
					   list_store->sort_column_id);

  g_return_val_if_fail (header != NULL, 0);
  g_return_val_if_fail (header->func != NULL, 0);

  el_a = ((SortTuple *) a)->el;
  el_b = ((SortTuple *) b)->el;

  iter_a.stamp = list_store->stamp;
  iter_a.user_data = el_a;
  iter_b.stamp = list_store->stamp;
  iter_b.user_data = el_b;

  retval = (* header->func) (GTK_TREE_MODEL (list_store),
			     &iter_a, &iter_b,
			     header->data);

  if (list_store->order == GTK_TREE_SORT_DESCENDING)
    {
      if (retval > 0)
	retval = -1;
      else if (retval < 0)
	retval = 1;
    }
  return retval;
}

static void
gtk_list_store_sort (GtkListStore *list_store)
{
  GtkTreeDataSortHeader *header = NULL;
  GtkTreeIter iter;
  GArray *sort_array;
  gint i;
  gint *new_order;
  GSList *list;
  GtkTreePath *path;

  if (list_store->length <= 1)
    return;

  g_assert (GTK_LIST_STORE_IS_SORTED (list_store));

  header = _gtk_tree_data_list_get_header (list_store->sort_list, list_store->sort_column_id);

  /* We want to make sure that we have a function */
  g_return_if_fail (header != NULL);
  g_return_if_fail (header->func != NULL);

  list = G_SLIST (list_store->root);

  sort_array = g_array_sized_new (FALSE, FALSE,
				  sizeof (SortTuple),
				  list_store->length);

  for (i = 0; i < list_store->length; i++)
    {
      SortTuple tuple;

      /* If this fails, we are in an inconsistent state.  Bad */
      g_return_if_fail (list != NULL);

      tuple.offset = i;
      tuple.el = list;
      g_array_append_val (sort_array, tuple);

      list = list->next;
    }

  g_array_sort_with_data (sort_array, gtk_list_store_compare_func, list_store);

  for (i = 0; i < list_store->length - 1; i++)
      g_array_index (sort_array, SortTuple, i).el->next =
	g_array_index (sort_array, SortTuple, i + 1).el;
  g_array_index (sort_array, SortTuple, list_store->length - 1).el->next = NULL;
  list_store->root = g_array_index (sort_array, SortTuple, 0).el;

  /* Let the world know about our new order */
  new_order = g_new (gint, list_store->length);
  for (i = 0; i < list_store->length; i++)
    new_order[i] = g_array_index (sort_array, SortTuple, i).offset;
  path = gtk_tree_path_new ();
  iter.stamp = list_store->stamp;
  iter.user_data = NULL;
  gtk_tree_model_reordered (GTK_TREE_MODEL (list_store),
			    path, &iter, new_order);
  gtk_tree_path_free (path);
  g_free (new_order);
  g_array_free (sort_array, TRUE);
}

static void
gtk_list_store_sort_iter_changed (GtkListStore *list_store,
				  GtkTreeIter  *iter,
				  gint          column)

{
  GtkTreeDataSortHeader *header;
  GSList *prev = NULL;
  GSList *next = NULL;
  GSList *list = G_SLIST (list_store->root);
  GtkTreePath *tmp_path;
  GtkTreeIter tmp_iter;
  gint cmp_a = 0;
  gint cmp_b = 0;
  gint i;
  gint old_location;
  gint new_location;
  gint *new_order;

  if (list_store->length < 2)
    return;

  tmp_iter.stamp = list_store->stamp;
  header = _gtk_tree_data_list_get_header (list_store->sort_list,
					   list_store->sort_column_id);
  g_return_if_fail (header != NULL);
  g_return_if_fail (header->func != NULL);

  /* If it's the built in function, we don't sort. */
  if (header->func == gtk_tree_data_list_compare_func &&
      list_store->sort_column_id != column)
    return;

  old_location = 0;
  /* First we find the iter, its prev, and its next */
  while (list)
    {
      if (list == G_SLIST (iter->user_data))
	break;
      prev = list;
      list = list->next;
      old_location++;
    }
  g_assert (list != NULL);

  next = list->next;

  /* Check the common case, where we don't need to sort it moved. */
  if (prev != NULL)
    {
      tmp_iter.user_data = prev;
      cmp_a = (* header->func) (GTK_TREE_MODEL (list_store),
				&tmp_iter, iter,
				header->data);
    }

  if (next != NULL)
    {
      tmp_iter.user_data = next;
      cmp_b = (* header->func) (GTK_TREE_MODEL (list_store),
				iter, &tmp_iter,
				header->data);
    }


  if (list_store->order == GTK_TREE_SORT_DESCENDING)
    {
      if (cmp_a < 0)
	cmp_a = 1;
      else if (cmp_a > 0)
	cmp_a = -1;

      if (cmp_b < 0)
	cmp_b = 1;
      else if (cmp_b > 0)
	cmp_b = -1;
    }

  if (prev == NULL && cmp_b <= 0)
    return;
  else if (next == NULL && cmp_a <= 0)
    return;
  else if (prev != NULL && next != NULL &&
	   cmp_a <= 0 && cmp_b <= 0)
    return;

  /* We actually need to sort it */
  /* First, remove the old link. */

  if (prev == NULL)
    list_store->root = next;
  else
    prev->next = next;
  if (next == NULL)
    list_store->tail = prev;
  list->next = NULL;
  
  /* FIXME: as an optimization, we can potentially start at next */
  prev = NULL;
  list = G_SLIST (list_store->root);
  new_location = 0;
  tmp_iter.user_data = list;
  if (list_store->order == GTK_TREE_SORT_DESCENDING)
    cmp_a = (* header->func) (GTK_TREE_MODEL (list_store),
			      &tmp_iter, iter, header->data);
  else
    cmp_a = (* header->func) (GTK_TREE_MODEL (list_store),
			      iter, &tmp_iter, header->data);

  while ((list->next) && (cmp_a > 0))
    {
      prev = list;
      list = list->next;
      new_location++;
      tmp_iter.user_data = list;
      if (list_store->order == GTK_TREE_SORT_DESCENDING)
	cmp_a = (* header->func) (GTK_TREE_MODEL (list_store),
				  &tmp_iter, iter, header->data);
      else
	cmp_a = (* header->func) (GTK_TREE_MODEL (list_store),
				  iter, &tmp_iter, header->data);
    }

  if ((!list->next) && (cmp_a > 0))
    {
      list->next = G_SLIST (iter->user_data);
      list_store->tail = list->next;
    }
  else if (prev)
    {
      prev->next = G_SLIST (iter->user_data);
      G_SLIST (iter->user_data)->next = list;
    }
  else
    {
      G_SLIST (iter->user_data)->next = G_SLIST (list_store->root);
      list_store->root = G_SLIST (iter->user_data);
    }

  /* Emit the reordered signal. */
  new_order = g_new (int, list_store->length);
  if (old_location < new_location)
    for (i = 0; i < list_store->length; i++)
      {
	if (i < old_location ||
	    i > new_location)
	  new_order[i] = i;
	else if (i >= old_location &&
		 i < new_location)
	  new_order[i] = i + 1;
	else if (i == new_location)
	  new_order[i] = old_location;
      }
  else
    for (i = 0; i < list_store->length; i++)
      {
	if (i < new_location ||
	    i > old_location)
	  new_order[i] = i;
	else if (i > new_location &&
		 i <= old_location)
	  new_order[i] = i - 1;
	else if (i == new_location)
	  new_order[i] = old_location;
      }

  tmp_path = gtk_tree_path_new ();
  tmp_iter.user_data = NULL;

  gtk_tree_model_reordered (GTK_TREE_MODEL (list_store),
			    tmp_path, &tmp_iter,
			    new_order);

  gtk_tree_path_free (tmp_path);
  g_free (new_order);
}

static gboolean
gtk_list_store_get_sort_column_id (GtkTreeSortable  *sortable,
				   gint             *sort_column_id,
				   GtkTreeSortOrder *order)
{
  GtkListStore *list_store = (GtkListStore *) sortable;

  g_return_val_if_fail (sortable != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_LIST_STORE (sortable), FALSE);

  if (list_store->sort_column_id == -1)
    return FALSE;

  if (sort_column_id)
    * sort_column_id = list_store->sort_column_id;
  if (order)
    * order = list_store->order;
  return TRUE;
}

static void
gtk_list_store_set_sort_column_id (GtkTreeSortable  *sortable,
				   gint              sort_column_id,
				   GtkTreeSortOrder  order)
{
  GtkListStore *list_store = (GtkListStore *) sortable;
  GList *list;

  g_return_if_fail (sortable != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (sortable));

  for (list = list_store->sort_list; list; list = list->next)
    {
      GtkTreeDataSortHeader *header = (GtkTreeDataSortHeader*) list->data;
      if (header->sort_column_id == sort_column_id)
	break;
    }
  g_return_if_fail (list != NULL);

  if ((list_store->sort_column_id == sort_column_id) &&
      (list_store->order == order))
    return;

  list_store->sort_column_id = sort_column_id;
  list_store->order = order;

  if (list_store->sort_column_id >= 0)
    gtk_list_store_sort (list_store);

  gtk_tree_sortable_sort_column_changed (sortable);
}

static void
gtk_list_store_set_sort_func (GtkTreeSortable        *sortable,
			      gint                    sort_column_id,
			      GtkTreeIterCompareFunc  func,
			      gpointer                data,
			      GtkDestroyNotify        destroy)
{
  GtkListStore *list_store = (GtkListStore *) sortable;
  GtkTreeDataSortHeader *header = NULL;
  GList *list;

  g_return_if_fail (sortable != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (sortable));
  g_return_if_fail (func != NULL);

  for (list = list_store->sort_list; list; list = list->next)
    {
      header = (GtkTreeDataSortHeader*) list->data;
      if (header->sort_column_id == sort_column_id)
	break;
    }

  if (header == NULL)
    {
      header = g_new0 (GtkTreeDataSortHeader, 1);
      header->sort_column_id = sort_column_id;
      list_store->sort_list = g_list_append (list_store->sort_list, header);
    }

  if (header->destroy)
    (* header->destroy) (header->data);

  header->func = func;
  header->data = data;
  header->destroy = destroy;
}
