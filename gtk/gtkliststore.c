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

      list_store_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkListStore", &list_store_info, 0);
      g_type_add_interface_static (list_store_type,
				   GTK_TYPE_TREE_MODEL,
				   &tree_model_info);
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


  gtk_object_class_add_signals (object_class, list_store_signals, LAST_SIGNAL);
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
gtk_list_store_init (GtkListStore *list_store)
{
  list_store->root = NULL;
  list_store->stamp = g_random_int ();
}

GtkListStore *
gtk_list_store_new (void)
{
  return GTK_LIST_STORE (gtk_type_new (gtk_list_store_get_type ()));
}

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
  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), FALSE);
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, FALSE);
  
  iter->stamp = GTK_LIST_STORE (tree_model)->stamp;
  iter->tree_node = g_slist_nth (G_SLIST (GTK_LIST_STORE (tree_model)->root),
				 gtk_tree_path_get_indices (path)[0]);

  return iter->tree_node != NULL;
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
      if (list == G_SLIST (iter->tree_node))
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

  list = G_SLIST (iter->tree_node)->data;

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

  iter->tree_node = G_SLIST (iter->tree_node)->next;

  return (iter->tree_node != NULL);
}

static gboolean
gtk_list_store_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
  iter->stamp = 0;
  iter->tree_node = NULL;

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
    return g_slist_length (G_SLIST (GTK_LIST_STORE (tree_model)->root));

  return 0;
}

static gboolean
gtk_list_store_iter_nth_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       GtkTreeIter  *parent,
			       gint          n)
{
  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), FALSE);

  if (parent)
    {
      g_return_val_if_fail (iter->stamp == GTK_LIST_STORE (tree_model)->stamp, FALSE);
      iter->stamp = 0;
      iter->tree_node = NULL;

      return FALSE;
    }

  iter->tree_node = g_slist_nth (G_SLIST (GTK_LIST_STORE (tree_model)->root), n);
  if (iter->tree_node)
    iter->stamp = GTK_LIST_STORE (tree_model)->stamp;
  else
    iter->stamp = 0;

  return (iter->tree_node != NULL);
}

static gboolean
gtk_list_store_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  iter->stamp = 0;
  iter->tree_node = NULL;

  return FALSE;
}

/* Public accessors */
/* This is a somewhat inelegant function that does a lot of list
 * manipulations on it's own.
 */
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

  prev = list = G_SLIST (iter->tree_node)->data;

  while (list != NULL)
    {
      if (column == 0)
	{
	  _gtk_tree_data_list_value_to_node (list, value);
	  return;
	}

      column--;
      prev = list;
      list = list->next;
    }

  if (G_SLIST (iter->tree_node)->data == NULL)
    {
      G_SLIST (iter->tree_node)->data = list = _gtk_tree_data_list_alloc ();
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

void
gtk_list_store_remove (GtkListStore *list_store,
		       GtkTreeIter  *iter)
{
  GtkTreePath *path;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));

  if (G_SLIST (iter->tree_node)->data)
    _gtk_tree_data_list_free ((GtkTreeDataList *) G_SLIST (iter->tree_node)->data,
			      list_store->column_headers);

  path = gtk_list_store_get_path (GTK_TREE_MODEL (list_store), iter);
  list_store->root = g_slist_remove_link (G_SLIST (list_store->root),
					  G_SLIST (iter->tree_node));
  list_store->stamp ++;
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "deleted",
			   path);
  gtk_tree_path_free (path);
}

void
gtk_list_store_insert (GtkListStore *list_store,
		       GtkTreeIter  *iter,
		       gint          position)
{
  GSList *list;
  GtkTreePath *path;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (position < 0);

  if (position == 0)
    {
      gtk_list_store_prepend (list_store, iter);
      return;
    }

  iter->stamp = list_store->stamp;
  iter->tree_node = g_slist_alloc ();

  list = g_slist_nth (G_SLIST (list_store->root), position - 1);
  if (list)
    {
      G_SLIST (iter->tree_node)->next = list->next;
      list->next = G_SLIST (iter->tree_node)->next;
    }
  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, position);
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "inserted",
			   path, iter);
  gtk_tree_path_free (path);
}

void
gtk_list_store_insert_before (GtkListStore *list_store,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *sibling)
{
  GtkTreePath *path;
  GSList *list, *prev;
  gint i = 0;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (G_SLIST (iter)->next == NULL);

  if (sibling == NULL)
    {
      gtk_list_store_append (list_store, iter);
      return;
    }

  iter->stamp = list_store->stamp;
  iter->tree_node = g_slist_alloc ();

  prev = list = list_store->root;
  while (list && list != sibling->tree_node)
    {
      prev = list;
      list = list->next;
      i++;
    }
  
  if (prev)
    {
      prev->next = iter->tree_node;
    }
  else
    {
      G_SLIST (iter->tree_node)->next = list_store->root;
      list_store->root = iter->tree_node;
    }

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, i);
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "inserted",
			   path, iter);
  gtk_tree_path_free (path);
}

void
gtk_list_store_insert_after (GtkListStore *list_store,
			     GtkTreeIter  *iter,
			     GtkTreeIter  *sibling)
{
  GtkTreePath *path;
  GSList *list;
  gint i = 0;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter == NULL);
  if (sibling)
    g_return_if_fail (sibling->stamp == list_store->stamp);

  if (sibling == NULL)
    {
      gtk_list_store_prepend (list_store, iter);
      return;
    }

  for (list = list_store->root; list && list != sibling->tree_node; list = list->next)
    i++;

  g_return_if_fail (list != NULL);

  iter->stamp = list_store->stamp;
  iter->tree_node = g_slist_alloc ();

  G_SLIST (iter->tree_node)->next = G_SLIST (sibling->tree_node)->next;
  G_SLIST (sibling)->next = G_SLIST (iter);

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, i);
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "inserted",
			   path, iter);
  gtk_tree_path_free (path);
}

void
gtk_list_store_prepend (GtkListStore *list_store,
			GtkTreeIter  *iter)
{
  GtkTreePath *path;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  iter->stamp = list_store->stamp;
  iter->tree_node = g_slist_alloc ();

  G_SLIST (iter->tree_node)->next = G_SLIST (list_store->root);
  list_store->root = iter->tree_node;

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, 0);
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "inserted",
			   path, iter);
  gtk_tree_path_free (path);
}

void
gtk_list_store_append (GtkListStore *list_store,
		       GtkTreeIter  *iter)
{
  GtkTreePath *path;
  GSList *list, *prev;
  gint i = 0;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (iter != NULL);

  iter->stamp = list_store->stamp;
  iter->tree_node = g_slist_alloc ();

  prev = list = list_store->root;
  while (list)
    {
      prev = list;
      list = list->next;
      i++;
    }
  
  if (prev)
    prev->next = iter->tree_node;
  else
    list_store->root = iter->tree_node;

  path = gtk_tree_path_new ();
  gtk_tree_path_append_index (path, i);
  gtk_signal_emit_by_name (GTK_OBJECT (list_store),
			   "inserted",
			   path, iter);
  gtk_tree_path_free (path);
}
