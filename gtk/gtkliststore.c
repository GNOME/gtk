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

enum {
  CHANGED,
  INSERTED,
  CHILD_TOGGLED,
  DELETED,
  LAST_SIGNAL
};

static guint list_store_signals[LAST_SIGNAL] = { 0 };

static void         gtk_list_store_init            (GtkListStore      *list_store);
static void         gtk_list_store_class_init      (GtkListStoreClass *class);
static void         gtk_list_store_tree_model_init (GtkTreeModelIface *iface);
static void         gtk_list_store_drag_source_init(GtkTreeDragSourceIface *iface);
static void         gtk_list_store_drag_dest_init  (GtkTreeDragDestIface   *iface);
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
  static GtkType list_store_type = 0;

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
      
      list_store_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkListStore", &list_store_info, 0);
      g_type_add_interface_static (list_store_type,
				   GTK_TYPE_TREE_MODEL,
				   &tree_model_info);
      g_type_add_interface_static (list_store_type,
				   GTK_TYPE_TREE_DRAG_SOURCE,
				   &drag_source_info);
      g_type_add_interface_static (list_store_type,
				   GTK_TYPE_TREE_DRAG_DEST,
				   &drag_dest_info);
    }

  return list_store_type;
}

static void
gtk_list_store_class_init (GtkListStoreClass *class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass*) class;

  list_store_signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListStoreClass, changed),
                    gtk_marshal_VOID__BOXED_BOXED,
                    G_TYPE_NONE, 2,
		    GTK_TYPE_TREE_PATH,
		    GTK_TYPE_TREE_ITER);
  list_store_signals[INSERTED] =
    gtk_signal_new ("inserted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListStoreClass, inserted),
                    gtk_marshal_VOID__BOXED_BOXED,
                    G_TYPE_NONE, 2,
		    GTK_TYPE_TREE_PATH,
		    GTK_TYPE_TREE_ITER);
  list_store_signals[CHILD_TOGGLED] =
    gtk_signal_new ("child_toggled",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListStoreClass, child_toggled),
                    gtk_marshal_VOID__BOXED_BOXED,
                    G_TYPE_NONE, 2,
		    GTK_TYPE_TREE_PATH,
		    GTK_TYPE_TREE_ITER);
  list_store_signals[DELETED] =
    gtk_signal_new ("deleted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListStoreClass, deleted),
                    gtk_marshal_VOID__BOXED,
                    G_TYPE_NONE, 1,
		    GTK_TYPE_TREE_PATH);
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
gtk_list_store_drag_dest_init   (GtkTreeDragDestIface   *iface)
{
  iface->drag_data_received = gtk_list_store_drag_data_received;
  iface->row_drop_possible = gtk_list_store_row_drop_possible;
}

static void
gtk_list_store_init (GtkListStore *list_store)
{
  list_store->root = NULL;
  list_store->tail = NULL;
  list_store->stamp = g_random_int ();
  list_store->length = 0;
}

/**
 * gtk_list_store_new:
 *
 * Creates a new #GtkListStore. A #GtkListStore implements the
 * #GtkTreeModel interface, and stores a linked list of
 * rows; each row can have any number of columns. Columns are of uniform type,
 * i.e. all cells in a column have the same type such as #G_TYPE_STRING or
 * #GDK_TYPE_PIXBUF. Use #GtkListStore to store data to be displayed in a
 * #GtkTreeView.
 * 
 * Return value: a new #GtkListStore
 **/
GtkListStore *
gtk_list_store_new (void)
{
  return GTK_LIST_STORE (gtk_type_new (gtk_list_store_get_type ()));
}

/**
 * gtk_list_store_new_with_types:
 * @n_columns: number of columns in the list store
 * @Varargs: pairs of column number and #GType
 *
 * Creates a new list store as with gtk_list_store_new(),
 * simultaneously setting up the columns and column types as with
 * gtk_list_store_set_n_columns() and
 * gtk_list_store_set_column_type().
 * 
 * 
 * Return value: a new #GtkListStore
 **/
GtkListStore *
gtk_list_store_new_with_types (gint n_columns,
			       ...)
{
  GtkListStore *retval;
  va_list args;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = gtk_list_store_new ();
  gtk_list_store_set_n_columns (retval, n_columns);

  va_start (args, n_columns);

  for (i = 0; i < n_columns; i++)
    gtk_list_store_set_column_type (retval, i, va_arg (args, GType));

  va_end (args);

  return retval;
}

/**
 * gtk_list_store_set_n_columns:
 * @store: a #GtkListStore
 * @n_columns: number of columns
 *
 * Sets the number of columns in the #GtkListStore.
 * 
 **/
void
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

  list_store->column_headers = new_columns;
  list_store->n_columns = n_columns;
}

/**
 * gtk_list_store_set_column_type:
 * @store: a #GtkListStore
 * @column: column number
 * @type: type of the data stored in @column
 *
 * Supported types include: %G_TYPE_UINT, %G_TYPE_INT, %G_TYPE_UCHAR,
 * %G_TYPE_CHAR, %G_TYPE_BOOLEAN, %G_TYPE_POINTER, %G_TYPE_FLOAT, %G_TYPE_STRING,
 * %G_TYPE_OBJECT, and %G_TYPE_BOXED, along with subclasses of those types such
 * as %GDK_TYPE_PIXBUF.
 * 
 **/
void
gtk_list_store_set_column_type (GtkListStore *list_store,
				gint          column,
				GType         type)
{
  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (column >=0 && column < list_store->n_columns);

  list_store->column_headers[column] = type;
}

/* Fulfill the GtkTreeModel requirements */
static guint
gtk_list_store_get_flags (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), 0);

  return GTK_TREE_MODEL_ITERS_PERSIST;
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
  
  list = g_slist_nth (G_SLIST (GTK_LIST_STORE (tree_model)->root),
                      i);

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

  if (G_SLIST (iter->user_data)->next)
    {
      iter->user_data = G_SLIST (iter->user_data)->next;
      return TRUE;
    }
  else
    return FALSE;
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
  if (iter == NULL)
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
      iter->user_data = child;
      iter->stamp = GTK_LIST_STORE (tree_model)->stamp;
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
 * gtk_list_store_set_cell:
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
gtk_list_store_set_cell (GtkListStore *list_store,
			 GtkTreeIter  *iter,
			 gint          column,
			 GValue       *value)
{
  GtkTreeDataList *list;
  GtkTreeDataList *prev;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (column >= 0 && column < list_store->n_columns);

  prev = list = G_SLIST (iter->user_data)->data;

  while (list != NULL)
    {
      if (column == 0)
	{
	  _gtk_tree_data_list_value_to_node (list, value);
	  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
				   "changed",
				   NULL, iter);
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
  _gtk_tree_data_list_value_to_node (list, value);
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "changed",
			   NULL, iter);
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

      G_VALUE_COLLECT (&value, var_args, &error);
      if (error)
	{
	  g_warning ("%s: %s", G_STRLOC, error);
	  g_free (error);

 	  /* we purposely leak the value here, it might not be
	   * in a sane state if an error condition occoured
	   */
	  break;
	}

      gtk_list_store_set_cell (list_store,
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

/**
 * gtk_list_store_get_valist:
 * @list_store: a #GtkListStore
 * @iter: a row in @list_store
 * @var_args: va_list of column/return location pairs
 *
 * See gtk_list_store_get(), this version takes a va_list for
 * language bindings to use.
 * 
 **/
void
gtk_list_store_get_valist (GtkListStore *list_store,
                           GtkTreeIter  *iter,
                           va_list	var_args)
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
	  g_warning ("%s: Invalid column number %d accessed (remember to end your list of columns with a -1)", G_STRLOC, column);
	  break;
	}

      gtk_list_store_get_value (GTK_TREE_MODEL (list_store), iter, column, &value);

      G_VALUE_LCOPY (&value, var_args, &error);
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
 * gtk_list_store_get:
 * @list_store: a #GtkListStore
 * @iter: a row in @list_store
 * @Varargs: pairs of column number and value return locations, terminated by -1
 *
 * Gets the value of one or more cells in the row referenced by @iter.
 * The variable argument list should contain integer column numbers,
 * each column number followed by a place to store the value being
 * retrieved.  The list is terminated by a -1. For example, to get a
 * value from column 0 with type %G_TYPE_STRING, you would
 * write: gtk_list_store_set (store, iter, 0, &place_string_here, -1),
 * where place_string_here is a gchar* to be filled with the string.
 * If appropriate, the returned values have to be freed or unreferenced.
 * 
 **/
void
gtk_list_store_get (GtkListStore *list_store,
		    GtkTreeIter  *iter,
		    ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  va_start (var_args, iter);
  gtk_list_store_get_valist (list_store, iter, var_args);
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
  
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "deleted",
			   path);
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
  if (sibling == list_store->tail)
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

  if (position == 0)
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
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "inserted",
			   path, iter);
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
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "inserted",
			   path, iter);
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

  if (sibling == NULL)
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
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "inserted",
			   path, iter);
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
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "inserted",
			   path, iter);
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
  gint i = 0;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  iter->stamp = list_store->stamp;
  iter->user_data = g_slist_alloc ();

  if (list_store->tail)
    list_store->tail->next = iter->user_data;
  else
    list_store->root = iter->user_data;

  list_store->tail = iter->user_data;

  list_store->length += 1;

  validate_list_store (list_store);
  
  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, i);
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "inserted",
			   path, iter);
  gtk_tree_path_free (path);
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
      gtk_list_store_remove (GTK_LIST_STORE (drag_source),
                             &iter);
      return TRUE;
    }
  else
    {
      return FALSE;
    }
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
          
          G_SLIST (dest_iter.user_data)->data = copy_head;
          
          gtk_signal_emit_by_name (GTK_OBJECT (tree_model),
                                   "changed",
                                   NULL, &dest_iter);
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
