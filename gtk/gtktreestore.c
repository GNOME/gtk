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
#include "gtksignal.h"
#include <string.h>

#define G_NODE(node) ((GNode *)node)

enum {
  NODE_CHANGED,
  NODE_INSERTED,
  NODE_CHILD_TOGGLED,
  NODE_DELETED,
  LAST_SIGNAL
};

static guint tree_store_signals[LAST_SIGNAL] = { 0 };

static void           gtk_tree_store_init            (GtkTreeStore      *tree_store);
static void           gtk_tree_store_class_init      (GtkTreeStoreClass *tree_store_class);
static void           gtk_tree_store_tree_model_init (GtkTreeModelIface *iface);
static gint           gtk_tree_store_get_n_columns   (GtkTreeModel      *tree_model);
static GtkTreeNode    gtk_tree_store_get_node        (GtkTreeModel      *tree_model,
						      GtkTreePath       *path);
static GtkTreePath   *gtk_tree_store_get_path        (GtkTreeModel      *tree_model,
						      GtkTreeNode        node);
static void           gtk_tree_store_node_get_value  (GtkTreeModel      *tree_model,
						      GtkTreeNode        node,
						      gint               column,
						      GValue            *value);
static gboolean       gtk_tree_store_node_next       (GtkTreeModel      *tree_model,
						      GtkTreeNode       *node);
static GtkTreeNode    gtk_tree_store_node_children   (GtkTreeModel      *tree_model,
						      GtkTreeNode        node);
static gboolean       gtk_tree_store_node_has_child  (GtkTreeModel      *tree_model,
						      GtkTreeNode        node);
static gint           gtk_tree_store_node_n_children (GtkTreeModel      *tree_model,
						      GtkTreeNode        node);
static GtkTreeNode    gtk_tree_store_node_nth_child  (GtkTreeModel      *tree_model,
						      GtkTreeNode        node,
						      gint               n);
static GtkTreeNode    gtk_tree_store_node_parent     (GtkTreeModel      *tree_model,
						      GtkTreeNode        node);



GtkType
gtk_tree_store_get_type (void)
{
  static GtkType tree_store_type = 0;

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

      tree_store_type = g_type_register_static (GTK_TYPE_TREE_MODEL, "GtkTreeStore", &tree_store_info);
      g_type_add_interface_static (tree_store_type,
				   GTK_TYPE_TREE_MODEL,
				   &tree_model_info);
    }

  return tree_store_type;
}

static void
gtk_tree_store_class_init (GtkTreeStoreClass *tree_store_class)
{
  GtkObjectClass *object_class;

  object_class = (GtkObjectClass *) tree_store_class;

  tree_store_signals[NODE_CHANGED] =
    gtk_signal_new ("node_changed",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeStoreClass, node_changed),
                    gtk_marshal_NONE__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_store_signals[NODE_INSERTED] =
    gtk_signal_new ("node_inserted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeStoreClass, node_inserted),
                    gtk_marshal_NONE__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_store_signals[NODE_CHILD_TOGGLED] =
    gtk_signal_new ("node_child_toggled",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeStoreClass, node_child_toggled),
                    gtk_marshal_NONE__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_store_signals[NODE_DELETED] =
    gtk_signal_new ("node_deleted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeStoreClass, node_deleted),
                    gtk_marshal_NONE__POINTER,
                    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);

  gtk_object_class_add_signals (object_class, tree_store_signals, LAST_SIGNAL);
}

static void
gtk_tree_store_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_n_columns = gtk_tree_store_get_n_columns;
  iface->get_node = gtk_tree_store_get_node;
  iface->get_path = gtk_tree_store_get_path;
  iface->node_get_value = gtk_tree_store_node_get_value;
  iface->node_next = gtk_tree_store_node_next;
  iface->node_children = gtk_tree_store_node_children;
  iface->node_has_child = gtk_tree_store_node_has_child;
  iface->node_n_children = gtk_tree_store_node_n_children;
  iface->node_nth_child = gtk_tree_store_node_nth_child;
  iface->node_parent = gtk_tree_store_node_parent;
}

static void
gtk_tree_store_init (GtkTreeStore *tree_store)
{
  tree_store->root = gtk_tree_store_node_new ();
}

GtkObject *
gtk_tree_store_new (void)
{
  return GTK_OBJECT (gtk_type_new (gtk_tree_store_get_type ()));
}

GtkObject *
gtk_tree_store_new_with_values (gint n_columns,
				...)
{
  GtkObject *retval;
  va_list args;
  gint i;

  g_return_val_if_fail (n_columns > 0, NULL);

  retval = gtk_tree_store_new ();
  gtk_tree_store_set_n_columns (GTK_TREE_STORE (retval),
				n_columns);

  va_start (args, n_columns);
  for (i = 0; i < n_columns; i++)
    gtk_tree_store_set_column_type (GTK_TREE_STORE (retval),
				    i, va_arg (args, GType));

  va_end (args);

  return retval;
}

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

  tree_store->column_headers = new_columns;
  tree_store->n_columns = n_columns;
}

void
gtk_tree_store_set_column_type (GtkTreeStore *tree_store,
				gint          column,
				GType         type)
{
  g_return_if_fail (tree_store != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));
  g_return_if_fail (column >=0 && column < tree_store->n_columns);

  tree_store->column_headers[column] = type;
}

/* fulfill the GtkTreeModel requirements */
/* NOTE: GtkTreeStore::root is a GNode, that acts as the parent node.  However,
 * it is not visible to the tree or to the user., and the path "1" refers to the
 * first child of GtkTreeStore::root.
 */
static gint
gtk_tree_store_get_n_columns (GtkTreeModel *tree_model)
{
  g_return_val_if_fail (tree_model != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), 0);

  return GTK_TREE_STORE (tree_model)->n_columns;
}

static GtkTreeNode
gtk_tree_store_get_node (GtkTreeModel *tree_model,
			 GtkTreePath  *path)
{
  gint i;
  GtkTreeNode node;
  gint *indices = gtk_tree_path_get_indices (path);

  node = GTK_TREE_STORE (tree_model)->root;

  for (i = 0; i < gtk_tree_path_get_depth (path); i ++)
    {
      node = (GtkTreeNode) gtk_tree_store_node_nth_child (tree_model,
							  (GtkTreeNode) node,
							  indices[i]);
      if (node == NULL)
	return NULL;
    };
  return (GtkTreeNode) node;
}

static GtkTreePath *
gtk_tree_store_get_path (GtkTreeModel *tree_model,
			 GtkTreeNode   node)
{
  GtkTreePath *retval;
  GNode *tmp_node;
  gint i = 0;

  g_return_val_if_fail (tree_model != NULL, NULL);

  if (node == NULL)
    return NULL;
  if (node == G_NODE (GTK_TREE_STORE (tree_model)->root))
    return NULL;

  if (G_NODE (node)->parent == G_NODE (GTK_TREE_STORE (tree_model)->root))
    {
      retval = gtk_tree_path_new ();
      tmp_node = G_NODE (GTK_TREE_STORE (tree_model)->root)->children;
    }
  else
    {
      retval = gtk_tree_store_get_path (tree_model,
					G_NODE (node)->parent);
      tmp_node = G_NODE (node)->parent->children;
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
      if (tmp_node == G_NODE (node))
	break;
      i++;
    }
  if (tmp_node == NULL)
    {
      /* We couldn't find node, meaning it's prolly not ours */
      gtk_tree_path_free (retval);
      return NULL;
    }

  gtk_tree_path_append_index (retval, i);

  return retval;
}


static void
gtk_tree_store_node_get_value (GtkTreeModel *tree_model,
			       GtkTreeNode   node,
			       gint          column,
			       GValue       *value)
{
  GtkTreeDataList *list;
  gint tmp_column = column;

  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (tree_model));
  g_return_if_fail (node != NULL);
  g_return_if_fail (column < GTK_TREE_STORE (tree_model)->n_columns);

  list = G_NODE (node)->data;

  while (tmp_column-- > 0 && list)
    list = list->next;

  g_return_if_fail (list != NULL);

  gtk_tree_data_list_node_to_value (list,
				    GTK_TREE_STORE (tree_model)->column_headers[column],
				    value);
}

static gboolean
gtk_tree_store_node_next (GtkTreeModel  *tree_model,
			  GtkTreeNode   *node)
{
  if (node == NULL || *node == NULL)
    return FALSE;

  *node = (GtkTreeNode) G_NODE (*node)->next;
  return (*node != NULL);
}

static GtkTreeNode
gtk_tree_store_node_children (GtkTreeModel *tree_model,
			      GtkTreeNode   node)
{
  return (GtkTreeNode) G_NODE (node)->children;
}

static gboolean
gtk_tree_store_node_has_child (GtkTreeModel *tree_model,
			       GtkTreeNode   node)
{
  return G_NODE (node)->children != NULL;
}

static gint
gtk_tree_store_node_n_children (GtkTreeModel *tree_model,
				GtkTreeNode   node)
{
  gint i = 0;

  node = (GtkTreeNode) G_NODE (node)->children;
  while (node != NULL)
    {
      i++;
      node = (GtkTreeNode) G_NODE (node)->next;
    }

  return i;
}

static GtkTreeNode
gtk_tree_store_node_nth_child (GtkTreeModel *tree_model,
			       GtkTreeNode   node,
			       gint          n)
{
  g_return_val_if_fail (node != NULL, NULL);

  return (GtkTreeNode) g_node_nth_child (G_NODE (node), n);
}

static GtkTreeNode
gtk_tree_store_node_parent (GtkTreeModel *tree_model,
			    GtkTreeNode   node)
{
  return (GtkTreeNode) G_NODE (node)->parent;
}

/* Public accessors */
GtkTreeNode
gtk_tree_store_node_new (void)
{
  GtkTreeNode retval;

  retval = (GtkTreeNode) g_node_new (NULL);
  return retval;
}

/*
 * This is a somewhat inelegant function that does a lot of list
 * manipulations on it's own.
 */
void
gtk_tree_store_node_set_cell (GtkTreeStore *tree_store,
			      GtkTreeNode   node,
			      gint          column,
			      GValue       *value)
{
  GtkTreeDataList *list;
  GtkTreeDataList *prev;

  g_return_if_fail (tree_store != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));
  g_return_if_fail (node != NULL);
  g_return_if_fail (column >= 0 && column < tree_store->n_columns);

  prev = list = G_NODE (node)->data;

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

  if (G_NODE (node)->data == NULL)
    {
      G_NODE (node)->data = list = gtk_tree_data_list_alloc ();
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
gtk_tree_store_node_remove (GtkTreeStore *model,
			    GtkTreeNode   node)
{
  GtkTreePath *path;
  GNode *parent;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));
  g_return_if_fail (node != NULL);
  /* FIXME: if node is NULL, do I want to free the tree? */

  parent = G_NODE (node)->parent;

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), node);
  g_node_destroy (G_NODE (node));
  gtk_signal_emit_by_name (GTK_OBJECT (model),
			   "node_deleted",
			   path);
  if (parent != G_NODE (model->root) && parent->children == NULL)
    {
      gtk_tree_path_up (path);
      gtk_signal_emit_by_name (GTK_OBJECT (model),
			       "node_child_toggled",
			       path,
			       parent);
    }
  gtk_tree_path_free (path);
}

GtkTreeNode
gtk_tree_store_node_insert (GtkTreeStore *model,
			    GtkTreeNode   parent,
			    gint          position,
			    GtkTreeNode   node)
{
  GtkTreePath *path;

  g_return_val_if_fail (model != NULL, node);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), node);
  g_return_val_if_fail (node != NULL, node);

  if (parent == NULL)
    parent = model->root;

  g_node_insert (G_NODE (parent), position, G_NODE (node));

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), node);
  gtk_signal_emit_by_name (GTK_OBJECT (model),
			   "node_inserted",
			   path, node);
  gtk_tree_path_free (path);

  return node;
}

GtkTreeNode
gtk_tree_store_node_insert_before (GtkTreeStore *model,
				   GtkTreeNode   parent,
				   GtkTreeNode   sibling,
				   GtkTreeNode   node)
{
  GtkTreePath *path;

  g_return_val_if_fail (model != NULL, node);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), node);
  g_return_val_if_fail (node != NULL, node);

  if (parent == NULL && sibling == NULL)
    parent = model->root;

  if (parent == NULL)
    parent = (GtkTreeNode) G_NODE (sibling)->parent;

  g_node_insert_before (G_NODE (parent), G_NODE (sibling), G_NODE (node));

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), node);
  gtk_signal_emit_by_name (GTK_OBJECT (model),
			   "node_inserted",
			   path, node);
  gtk_tree_path_free (path);

  return node;
}

GtkTreeNode
gtk_tree_store_node_insert_after (GtkTreeStore *model,
				  GtkTreeNode   parent,
				  GtkTreeNode   sibling,
				  GtkTreeNode   node)
{
  GtkTreePath *path;

  g_return_val_if_fail (model != NULL, node);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), node);
  g_return_val_if_fail (node != NULL, node);

  if (parent == NULL && sibling == NULL)
    parent = model->root;

  if (parent == NULL)
    parent = (GtkTreeNode) G_NODE (sibling)->parent;

  g_node_insert_after (G_NODE (parent), G_NODE (sibling), G_NODE (node));

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), node);
  gtk_signal_emit_by_name (GTK_OBJECT (model),
			   "node_inserted",
			   path, node);
  gtk_tree_path_free (path);
  return node;
}

GtkTreeNode
gtk_tree_store_node_prepend (GtkTreeStore *model,
			     GtkTreeNode   parent,
			     GtkTreeNode   node)
{
  g_return_val_if_fail (model != NULL, node);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), node);
  g_return_val_if_fail (node != NULL, node);

  if (parent == NULL)
    parent = model->root;

  if (G_NODE (parent)->children == NULL)
    {
      GtkTreePath *path;
      g_node_prepend (G_NODE (parent), G_NODE (node));
      if (parent != model->root)
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), parent);
	  gtk_signal_emit_by_name (GTK_OBJECT (model),
				   "node_child_toggled",
				   path,
				   parent);
	  gtk_tree_path_append_index (path, 1);
	}
      else
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), G_NODE (parent)->children);
	}
      gtk_signal_emit_by_name (GTK_OBJECT (model),
			       "node_inserted",
			       path,
			       G_NODE (parent)->children);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_tree_store_node_insert_after (model,
					 parent == model->root?NULL:parent,
					 NULL,
					 node);
    }
  return node;
}

GtkTreeNode
gtk_tree_store_node_append (GtkTreeStore *model,
			    GtkTreeNode   parent,
			    GtkTreeNode   node)
{
  g_return_val_if_fail (model != NULL, node);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), node);
  g_return_val_if_fail (node != NULL, node);

  if (parent == NULL)
    parent = model->root;

  if (G_NODE (parent)->children == NULL)
    {
      GtkTreePath *path;
      g_node_append (G_NODE (parent), G_NODE (node));
      if (parent != model->root)
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), parent);
	  gtk_signal_emit_by_name (GTK_OBJECT (model),
				   "node_child_toggled",
				   path,
				   parent);
	  gtk_tree_path_append_index (path, 1);
	}
      else
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), G_NODE (parent)->children);
	}
      gtk_signal_emit_by_name (GTK_OBJECT (model),
			       "node_inserted",
			       path,
			       G_NODE (parent)->children);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_tree_store_node_insert_before (model,
					 parent == model->root?NULL:parent,
					 NULL,
					 node);
    }
  return node;
}

GtkTreeNode
gtk_tree_store_node_get_root (GtkTreeStore *model)
{
  g_return_val_if_fail (model != NULL, NULL);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), NULL);

  return (GtkTreeNode) model->root;
}


gboolean
gtk_tree_store_node_is_ancestor (GtkTreeStore *model,
				 GtkTreeNode   node,
				 GtkTreeNode   descendant)
{
  g_return_val_if_fail (model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), FALSE);
  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (descendant != NULL, FALSE);

  return g_node_is_ancestor (G_NODE (node), G_NODE (descendant));
}


gint
gtk_tree_store_node_depth (GtkTreeStore *model,
			   GtkTreeNode   node)
{
  g_return_val_if_fail (model != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), 0);
  g_return_val_if_fail (node != NULL, 0);

  return g_node_depth (G_NODE (node));
}
