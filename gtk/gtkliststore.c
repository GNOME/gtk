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
  NODE_CHANGED,
  NODE_INSERTED,
  NODE_CHILD_TOGGLED,
  NODE_DELETED,
  LAST_SIGNAL
};

static guint list_store_signals[LAST_SIGNAL] = { 0 };

static void           gtk_list_store_init            (GtkListStore      *list_store);
static void           gtk_list_store_class_init      (GtkListStoreClass *class);
static void           gtk_list_store_tree_model_init (GtkTreeModelIface *iface);
static gint           gtk_list_store_get_n_columns   (GtkTreeModel      *tree_model);
static GtkTreeNode    gtk_list_store_get_node        (GtkTreeModel      *tree_model,
						      GtkTreePath       *path);
static GtkTreePath   *gtk_list_store_get_path        (GtkTreeModel      *tree_model,
						      GtkTreeNode        node);
static void           gtk_list_store_node_get_value  (GtkTreeModel      *tree_model,
						      GtkTreeNode        node,
						      gint               column,
						      GValue            *value);
static gboolean       gtk_list_store_node_next       (GtkTreeModel      *tree_model,
						      GtkTreeNode       *node);
static GtkTreeNode    gtk_list_store_node_children   (GtkTreeModel      *tree_model,
						      GtkTreeNode        node);
static gboolean       gtk_list_store_node_has_child  (GtkTreeModel      *tree_model,
						      GtkTreeNode        node);
static gint           gtk_list_store_node_n_children (GtkTreeModel      *tree_model,
						      GtkTreeNode        node);
static GtkTreeNode    gtk_list_store_node_nth_child  (GtkTreeModel      *tree_model,
						      GtkTreeNode        node,
						      gint               n);
static GtkTreeNode    gtk_list_store_node_parent     (GtkTreeModel      *tree_model,
						      GtkTreeNode        node);


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

      list_store_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkListStore", &list_store_info);
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

  list_store_signals[NODE_CHANGED] =
    gtk_signal_new ("node_changed",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListStoreClass, node_changed),
                    gtk_marshal_NONE__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  list_store_signals[NODE_INSERTED] =
    gtk_signal_new ("node_inserted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListStoreClass, node_inserted),
                    gtk_marshal_NONE__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  list_store_signals[NODE_CHILD_TOGGLED] =
    gtk_signal_new ("node_child_toggled",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListStoreClass, node_child_toggled),
                    gtk_marshal_NONE__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  list_store_signals[NODE_DELETED] =
    gtk_signal_new ("node_deleted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkListStoreClass, node_deleted),
                    gtk_marshal_NONE__POINTER,
                    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);


  gtk_object_class_add_signals (object_class, list_store_signals, LAST_SIGNAL);
}

static void
gtk_list_store_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_n_columns = gtk_list_store_get_n_columns;
  iface->get_node = gtk_list_store_get_node;
  iface->get_path = gtk_list_store_get_path;
  iface->node_get_value = gtk_list_store_node_get_value;
  iface->node_next = gtk_list_store_node_next;
  iface->node_children = gtk_list_store_node_children;
  iface->node_has_child = gtk_list_store_node_has_child;
  iface->node_n_children = gtk_list_store_node_n_children;
  iface->node_nth_child = gtk_list_store_node_nth_child;
  iface->node_parent = gtk_list_store_node_parent;
}

static void
gtk_list_store_init (GtkListStore *list_store)
{
  list_store->root = NULL;
}

GtkObject *
gtk_list_store_new (void)
{
  return GTK_OBJECT (gtk_type_new (gtk_list_store_get_type ()));
}

GtkObject *
gtk_list_store_new_with_types (gint n_columns,
			       ...)
{
  GtkObject *retval;
  va_list args;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = gtk_list_store_new ();
  gtk_list_store_set_n_columns (GTK_LIST_STORE (retval),
			      n_columns);

  va_start (args, n_columns);
  for (i = 0; i < n_columns; i++)
    gtk_list_store_set_column_type (GTK_LIST_STORE (retval),
				    i, va_arg (args, GType));

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
static gint
gtk_list_store_get_n_columns (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (tree_model != NULL, 0);
  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), 0);

  return GTK_LIST_STORE (tree_model)->n_columns;
}

static GtkTreeNode
gtk_list_store_get_node (GtkTreeModel *tree_model,
			 GtkTreePath  *path)
{
  g_return_val_if_fail (gtk_tree_path_get_depth (path) > 0, NULL);

  return (GtkTreeNode) g_slist_nth (G_SLIST (GTK_LIST_STORE (tree_model)->root),
				    gtk_tree_path_get_indices (path)[0]);
}

static GtkTreePath *
gtk_list_store_get_path (GtkTreeModel *tree_model,
			 GtkTreeNode   node)
{
  GtkTreePath *retval;
  GSList *list;
  gint i = 0;

  g_return_val_if_fail (GTK_IS_LIST_STORE (tree_model), NULL);

  for (list = G_SLIST (GTK_LIST_STORE (tree_model)->root); list; list = list->next)
    {
      i++;
      if (list == G_SLIST (node))
	break;
    }
  if (list == NULL)
    return NULL;

  retval = gtk_tree_path_new ();
  gtk_tree_path_append_index (retval, i);

  return retval;
}

static void
gtk_list_store_node_get_value (GtkTreeModel *tree_model,
			       GtkTreeNode   node,
			       gint          column,
			       GValue       *value)
{
  GtkTreeDataList *list;
  gint tmp_column = column;

  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (tree_model));
  g_return_if_fail (node != NULL);
  g_return_if_fail (column < GTK_LIST_STORE (tree_model)->n_columns);

  list = G_SLIST (node)->data;

  while (tmp_column-- > 0 && list)
    list = list->next;

  g_return_if_fail (list != NULL);

  gtk_tree_data_list_node_to_value (list,
				    GTK_LIST_STORE (tree_model)->column_headers[column],
				    value);
}

static gboolean
gtk_list_store_node_next (GtkTreeModel  *tree_model,
			  GtkTreeNode   *node)
{
  if (node == NULL || *node == NULL)
    return FALSE;

  *node = (GtkTreeNode) G_SLIST (*node)->next;

  return (*node != NULL);
}

static GtkTreeNode
gtk_list_store_node_children (GtkTreeModel *tree_model,
			      GtkTreeNode   node)
{
  return NULL;
}

static gboolean
gtk_list_store_node_has_child (GtkTreeModel *tree_model,
			       GtkTreeNode   node)
{
  return FALSE;
}

static gint
gtk_list_store_node_n_children (GtkTreeModel *tree_model,
				GtkTreeNode   node)
{
  return 0;
}

static GtkTreeNode
gtk_list_store_node_nth_child (GtkTreeModel *tree_model,
			       GtkTreeNode   node,
			       gint          n)
{
  return NULL;
}

static GtkTreeNode
gtk_list_store_node_parent (GtkTreeModel *tree_model,
			    GtkTreeNode   node)
{
  return NULL;
}

/* Public accessors */
GtkTreeNode
gtk_list_store_node_new (void)
{
  GtkTreeNode retval = (GtkTreeNode) g_slist_alloc ();

  return retval;
}

/* This is a somewhat inelegant function that does a lot of list
 * manipulations on it's own.
 */
void
gtk_list_store_node_set_cell (GtkListStore *list_store,
			      GtkTreeNode   node,
			      gint          column,
			      GValue       *value)
{
  GtkTreeDataList *list;
  GtkTreeDataList *prev;

  g_return_if_fail (list_store != NULL);
  g_return_if_fail (GTK_IS_LIST_STORE (list_store));
  g_return_if_fail (node != NULL);
  g_return_if_fail (column >= 0 && column < list_store->n_columns);

  prev = list = G_SLIST (node)->data;

  while (list != NULL)
    {
      if (column == 0)
	{
	  gtk_tree_data_list_value_to_node (list, value);
	  return;
	}

      column--;
      prev = list;
      list = list->next;
    }

  if (G_SLIST (node)->data == NULL)
    {
      G_SLIST (node)->data = list = gtk_tree_data_list_alloc ();
      list->next = NULL;
    }
  else
    {
      list = prev->next = gtk_tree_data_list_alloc ();
      list->next = NULL;
    }

  while (column != 0)
    {
      list->next = gtk_tree_data_list_alloc ();
      list = list->next;
      list->next = NULL;
      column --;
    }
  gtk_tree_data_list_value_to_node (list, value);
}

void
gtk_list_store_node_remove (GtkListStore *list_store,
			    GtkTreeNode   node)
{
  /* FIXME: implement */
  g_warning ("Remember to implement this function\n");
}

GtkTreeNode
gtk_list_store_node_insert (GtkListStore *list_store,
			    gint          position,
			    GtkTreeNode   node)
{
  GSList *list;

  g_return_val_if_fail (list_store != NULL, node);
  g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), node);
  g_return_val_if_fail (node != NULL, node);
  g_return_val_if_fail (position < 0, node);
  g_return_val_if_fail (G_SLIST (node)->next == NULL, node);

  if (position == 0)
    {
      gtk_list_store_node_prepend (list_store, node);
      return node;
    }

  list = g_slist_nth (G_SLIST (list_store->root), position);
  if (list)
    {
      G_SLIST (node)->next = list->next;
      list->next = G_SLIST (node)->next;
    }

  return node;
}


GtkTreeNode
gtk_list_store_node_insert_before (GtkListStore *list_store,
				   GtkTreeNode   sibling,
				   GtkTreeNode   node)
{
  g_return_val_if_fail (list_store != NULL, node);
  g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), node);
  g_return_val_if_fail (node != NULL, node);

  /* FIXME: This is all wrong.  This is actually insert_after */
  if (sibling == NULL)
    return gtk_list_store_node_prepend (list_store, node);

  G_SLIST (node)->next = G_SLIST (sibling)->next;
  G_SLIST (sibling)->next = G_SLIST (node);
  return node;
}

GtkTreeNode
gtk_list_store_node_prepend (GtkListStore *list_store,
			     GtkTreeNode   node)
{
  g_return_val_if_fail (list_store != NULL, node);
  g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), node);
  g_return_val_if_fail (node != NULL, node);

  G_SLIST (node)->next = G_SLIST (list_store->root);
  list_store->root = node;

  return node;
}

GtkTreeNode
gtk_list_store_node_append (GtkListStore *list_store,
			    GtkTreeNode   node)
{
  GSList *list;

  g_return_val_if_fail (list_store != NULL, node);
  g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), node);
  g_return_val_if_fail (node != NULL, node);
  g_return_val_if_fail (G_SLIST (node)->next == NULL, node);

  list = g_slist_last (G_SLIST (list_store->root));
  if (list == NULL)
    list_store->root = node;
  else
    list->next = G_SLIST (node);

  return node;
}

GtkTreeNode
gtk_list_store_node_get_root (GtkListStore *list_store)
{
  g_return_val_if_fail (list_store != NULL, NULL);
  g_return_val_if_fail (GTK_IS_LIST_STORE (list_store), NULL);

  return (GtkTreeNode *) list_store->root;
}
