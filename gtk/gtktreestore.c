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
#include <gobject/gvaluecollector.h>

#define G_NODE(node) ((GNode *)node)

enum {
  CHANGED,
  INSERTED,
  CHILD_TOGGLED,
  DELETED,
  LAST_SIGNAL
};

static guint tree_store_signals[LAST_SIGNAL] = { 0 };


static void         gtk_tree_store_init            (GtkTreeStore      *tree_store);
static void         gtk_tree_store_class_init      (GtkTreeStoreClass *tree_store_class);
static void         gtk_tree_store_tree_model_init (GtkTreeModelIface *iface);
static gint         gtk_tree_store_get_n_columns   (GtkTreeModel      *tree_model);
static GtkTreePath *gtk_tree_store_get_path        (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter);
static void         gtk_tree_store_iter_get_value  (GtkTreeModel      *tree_model,
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
						    GtkTreeIter       *child,
						    gint               n);
static gboolean     gtk_tree_store_iter_parent     (GtkTreeModel      *tree_model,
						    GtkTreeIter       *iter,
						    GtkTreeIter       *parent);


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

      tree_store_type = g_type_register_static (GTK_TYPE_OBJECT, "GtkTreeStore", &tree_store_info, 0);
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

  tree_store_signals[CHANGED] =
    gtk_signal_new ("changed",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeStoreClass, changed),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_store_signals[INSERTED] =
    gtk_signal_new ("inserted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeStoreClass, inserted),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_store_signals[CHILD_TOGGLED] =
    gtk_signal_new ("child_toggled",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeStoreClass, child_toggled),
                    gtk_marshal_VOID__POINTER_POINTER,
                    GTK_TYPE_NONE, 2,
		    GTK_TYPE_POINTER,
		    GTK_TYPE_POINTER);
  tree_store_signals[DELETED] =
    gtk_signal_new ("deleted",
                    GTK_RUN_FIRST,
                    GTK_CLASS_TYPE (object_class),
                    GTK_SIGNAL_OFFSET (GtkTreeStoreClass, deleted),
                    gtk_marshal_VOID__POINTER,
                    GTK_TYPE_NONE, 1,
		    GTK_TYPE_POINTER);

  gtk_object_class_add_signals (object_class, tree_store_signals, LAST_SIGNAL);
}

static void
gtk_tree_store_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_n_columns = gtk_tree_store_get_n_columns;
  iface->get_path = gtk_tree_store_get_path;
  iface->iter_get_value = gtk_tree_store_iter_get_value;
  iface->iter_next = gtk_tree_store_iter_next;
  iface->iter_children = gtk_tree_store_iter_children;
  iface->iter_has_child = gtk_tree_store_iter_has_child;
  iface->iter_n_children = gtk_tree_store_iter_n_children;
  iface->iter_nth_child = gtk_tree_store_iter_nth_child;
  iface->iter_parent = gtk_tree_store_iter_parent;
}

static void
gtk_tree_store_init (GtkTreeStore *tree_store)
{
  tree_store->root = g_node_new (NULL);
  tree_store->stamp = 1;
}

GtkTreeStore *
gtk_tree_store_new (void)
{
  return GTK_TREE_STORE (gtk_type_new (gtk_tree_store_get_type ()));
}

GtkTreeStore *
gtk_tree_store_new_with_values (gint n_columns,
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
    gtk_tree_store_set_column_type (retval, i, va_arg (args, GType));

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
  g_return_val_if_fail (GTK_TREE_STORE (tree_model)->stamp == iter->stamp, NULL);
  if (iter->tree_node == NULL)
    return NULL;

  if (G_NODE (iter->tree_node)->parent == G_NODE (GTK_TREE_STORE (tree_model)->root))
    {
      retval = gtk_tree_path_new ();
      tmp_node = G_NODE (GTK_TREE_STORE (tree_model)->root)->children;
    }
  else
    {
      GtkTreeIter tmp_iter = *iter;
      tmp_iter.tree_node = G_NODE (tmp_iter.tree_node)->parent;

      retval = gtk_tree_store_get_path (tree_model,
					&tmp_iter);
      tmp_node = G_NODE (iter->tree_node)->parent->children;
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
      if (tmp_node == G_NODE (iter->tree_node))
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
gtk_tree_store_iter_get_value (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter,
			       gint          column,
			       GValue       *value)
{
  GtkTreeDataList *list;
  gint tmp_column = column;

  g_return_if_fail (tree_model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (tree_model));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (iter->stamp == GTK_TREE_STORE (tree_model)->stamp);
  g_return_if_fail (column < GTK_TREE_STORE (tree_model)->n_columns);

  list = G_NODE (iter->tree_node)->data;

  while (tmp_column-- > 0 && list)
    list = list->next;

  if (list)
    {
      gtk_tree_data_list_node_to_value (list,
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
  if (iter->tree_node == NULL)
    return FALSE;

  iter->tree_node = G_NODE (iter->tree_node)->next;

  return (iter->tree_node != NULL);
}

static gboolean
gtk_tree_store_iter_children (GtkTreeModel *tree_model,
			      GtkTreeIter  *iter,
			      GtkTreeIter  *parent)
{
  iter->stamp = GTK_TREE_STORE (tree_model)->stamp;
  iter->tree_node = G_NODE (parent->tree_node)->children;

  return iter->tree_node != NULL;
}

static gboolean
gtk_tree_store_iter_has_child (GtkTreeModel *tree_model,
			       GtkTreeIter  *iter)
{
  g_return_val_if_fail (tree_model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (iter->stamp == GTK_TREE_STORE (tree_model)->stamp, FALSE);

  return G_NODE (iter->tree_node)->children != NULL;
}

static gint
gtk_tree_store_iter_n_children (GtkTreeModel *tree_model,
				GtkTreeIter  *iter)
{
  GNode *node;
  gint i = 0;

  g_return_val_if_fail (tree_model != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), 0);
  g_return_val_if_fail (iter != NULL, 0);
  g_return_val_if_fail (iter->stamp == GTK_TREE_STORE (tree_model)->stamp, 0);

  node = G_NODE (iter->tree_node)->children;
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

  g_return_val_if_fail (tree_model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_STORE (tree_model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  if (parent)
    g_return_val_if_fail (parent->stamp == GTK_TREE_STORE (tree_model)->stamp, FALSE);

  if (parent == NULL)
    parent_node = GTK_TREE_STORE (tree_model)->root;
  else
    parent_node = parent->tree_node;

  iter->tree_node = g_node_nth_child (parent_node, n);

  if (iter->tree_node == NULL)
    iter->stamp = 0;
  else
    iter->stamp = GTK_TREE_STORE (tree_model)->stamp;

  return (iter->tree_node != NULL);
}

static gboolean
gtk_tree_store_iter_parent (GtkTreeModel *tree_model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *child)
{
  iter->stamp = GTK_TREE_STORE (tree_model)->stamp;
  iter->tree_node = G_NODE (child->tree_node)->parent;

  if (iter->tree_node == GTK_TREE_STORE (tree_model)->root)
    {
      iter->stamp = 0;
      return FALSE;
    }

  return TRUE;
}

/*
 * This is a somewhat inelegant function that does a lot of list
 * manipulations on it's own.
 */
void
gtk_tree_store_iter_set_cell (GtkTreeStore *tree_store,
			      GtkTreeIter  *iter,
			      gint          column,
			      GValue       *value)
{
  GtkTreeDataList *list;
  GtkTreeDataList *prev;

  g_return_if_fail (tree_store != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));
  g_return_if_fail (column >= 0 && column < tree_store->n_columns);

  prev = list = G_NODE (iter->tree_node)->data;

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

  if (G_NODE (iter->tree_node)->data == NULL)
    {
      G_NODE (iter->tree_node)->data = list = gtk_tree_data_list_alloc ();
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
gtk_tree_store_iter_setv (GtkTreeStore *tree_store,
			  GtkTreeIter  *iter,
			  va_list	var_args)
{
  gint column;

  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));

  column = va_arg (var_args, gint);

  while (column >= 0)
    {
      GValue value = { 0, };
      gchar *error = NULL;

      if (column >= tree_store->n_columns)
	{
	  g_warning ("Invalid column number %d added to iter", column);
	  break;
	}
      g_value_init (&value, tree_store->column_headers[column]);

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

      gtk_tree_store_iter_set_cell (tree_store,
				    iter,
				    column,
				    &value);

      g_value_unset (&value);

      column = va_arg (var_args, gint);
    }
}

void
gtk_tree_store_iter_set (GtkTreeStore *tree_store,
			 GtkTreeIter  *iter,
			 ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));

  va_start (var_args, iter);
  gtk_tree_store_iter_setv (tree_store, iter, var_args);
  va_end (var_args);
}

void
gtk_tree_store_iter_getv (GtkTreeStore *tree_store,
			  GtkTreeIter  *iter,
			  va_list	var_args)
{
  gint column;

  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));

  column = va_arg (var_args, gint);

  while (column >= 0)
    {
      GValue value = { 0, };
      gchar *error = NULL;

      if (column >= tree_store->n_columns)
	{
	  g_warning ("Invalid column number %d accessed", column);
	  break;
	}

      gtk_tree_store_iter_get_value (GTK_TREE_MODEL (tree_store), iter, column, &value);

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

void
gtk_tree_store_iter_get (GtkTreeStore *tree_store,
			 GtkTreeIter  *iter,
			 ...)
{
  va_list var_args;

  g_return_if_fail (GTK_IS_TREE_STORE (tree_store));

  va_start (var_args, iter);
  gtk_tree_store_iter_getv (tree_store, iter, var_args);
  va_end (var_args);
}

void
gtk_tree_store_iter_remove (GtkTreeStore *model,
			    GtkTreeIter  *iter)
{
  GtkTreePath *path;
  GNode *parent;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));

  parent = G_NODE (iter->tree_node)->parent;

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
  g_node_destroy (G_NODE (iter->tree_node));
  gtk_signal_emit_by_name (GTK_OBJECT (model),
			   "deleted",
			   path);
  if (parent != G_NODE (model->root) && parent->children == NULL)
    {
      gtk_tree_path_up (path);
      gtk_signal_emit_by_name (GTK_OBJECT (model),
			       "child_toggled",
			       path,
			       parent);
    }
  gtk_tree_path_free (path);
}

void
gtk_tree_store_iter_insert (GtkTreeStore *model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *parent,
			    gint          position)
{
  GtkTreePath *path;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));

  if (parent->tree_node == NULL)
    parent->tree_node = model->root;

  iter->stamp = model->stamp;
  iter->tree_node = g_node_new (NULL);
  g_node_insert (G_NODE (parent->tree_node), position, G_NODE (iter->tree_node));
  
  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
  gtk_signal_emit_by_name (GTK_OBJECT (model),
			   "inserted",
			   path, iter);
  gtk_tree_path_free (path);
}

void
gtk_tree_store_iter_insert_before (GtkTreeStore *model,
				   GtkTreeIter  *iter,
				   GtkTreeIter  *parent,
				   GtkTreeIter  *sibling)
{
  GtkTreePath *path;
  GNode *parent_node = NULL;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));
  g_return_if_fail (iter != NULL);
  if (parent != NULL)
    g_return_if_fail (parent->stamp == model->stamp);
  if (sibling != NULL)
    g_return_if_fail (sibling->stamp == model->stamp);

  iter->stamp = model->stamp;
  iter->tree_node = g_node_new (NULL);

  if (parent == NULL && sibling == NULL)
    parent_node = model->root;
  else if (parent == NULL)
    parent_node = G_NODE (sibling->tree_node)->parent;
  else
    parent_node = G_NODE (parent->tree_node);

  g_node_insert_before (parent_node,
			sibling ? G_NODE (sibling->tree_node) : NULL,
			G_NODE (iter->tree_node));

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
  gtk_signal_emit_by_name (GTK_OBJECT (model),
			   "inserted",
			   path, iter);
  gtk_tree_path_free (path);
}

void
gtk_tree_store_iter_insert_after (GtkTreeStore *model,
				  GtkTreeIter  *iter,
				  GtkTreeIter  *parent,
				  GtkTreeIter  *sibling)
{
  GtkTreePath *path;
  GNode *parent_node;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));
  g_return_if_fail (iter != NULL);
  if (parent != NULL)
    g_return_if_fail (parent->stamp == model->stamp);
  if (sibling != NULL)
    g_return_if_fail (sibling->stamp == model->stamp);

  iter->stamp = model->stamp;
  iter->tree_node = g_node_new (NULL);

  if (parent == NULL && sibling == NULL)
    parent_node = model->root;
  else if (parent == NULL)
    parent_node = G_NODE (sibling->tree_node)->parent;
  else
    parent_node = G_NODE (parent->tree_node);

  g_node_insert_after (parent_node,
		       sibling ? G_NODE (sibling->tree_node) : NULL,
		       G_NODE (iter->tree_node));

  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
  gtk_signal_emit_by_name (GTK_OBJECT (model),
			   "inserted",
			   path, iter);
  gtk_tree_path_free (path);
}

void
gtk_tree_store_iter_prepend (GtkTreeStore *model,
			     GtkTreeIter  *iter,
			     GtkTreeIter  *parent)
{
  GNode *parent_node;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));
  g_return_if_fail (iter != NULL);
  if (parent != NULL)
    g_return_if_fail (parent->stamp == model->stamp);

  if (parent == NULL)
    parent_node = model->root;
  else
    parent_node = parent->tree_node;

  iter->stamp = model->stamp;
  iter->tree_node = g_node_new (NULL);

  if (parent_node->children == NULL)
    {
      GtkTreePath *path;

      g_node_prepend (parent_node, G_NODE (iter->tree_node));

      if (parent_node != model->root)
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), parent);
	  gtk_signal_emit_by_name (GTK_OBJECT (model),
				   "child_toggled",
				   path,
				   parent);
	  gtk_tree_path_append_index (path, 0);
	}
      else
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
	}
      gtk_signal_emit_by_name (GTK_OBJECT (model),
			       "inserted",
			       path,
			       iter);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_tree_store_iter_insert_after (model, iter, parent, NULL);
    }
}

void
gtk_tree_store_iter_append (GtkTreeStore *model,
			    GtkTreeIter  *iter,
			    GtkTreeIter  *parent)
{
  GNode *parent_node;

  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));
  g_return_if_fail (iter != NULL);
  if (parent != NULL)
    g_return_if_fail (parent->stamp == model->stamp);

  if (parent == NULL)
    parent_node = model->root;
  else
    parent_node = parent->tree_node;

  iter->stamp = model->stamp;
  iter->tree_node = g_node_new (NULL);

  if (parent_node->children == NULL)
    {
      GtkTreePath *path;

      g_node_append (parent_node, G_NODE (iter->tree_node));

      if (parent_node != model->root)
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), parent);
	  gtk_signal_emit_by_name (GTK_OBJECT (model),
				   "child_toggled",
				   path,
				   parent);
	  gtk_tree_path_append_index (path, 0);
	}
      else
	{
	  path = gtk_tree_store_get_path (GTK_TREE_MODEL (model), iter);
	}
      gtk_signal_emit_by_name (GTK_OBJECT (model),
			       "inserted",
			       path,
			       iter);
      gtk_tree_path_free (path);
    }
  else
    {
      gtk_tree_store_iter_insert_before (model, iter, parent, NULL);
    }
}

void
gtk_tree_store_get_root (GtkTreeStore *model,
			 GtkTreeIter  *iter)
{
  g_return_if_fail (model != NULL);
  g_return_if_fail (GTK_IS_TREE_STORE (model));
  g_return_if_fail (iter != NULL);

  iter->stamp = model->stamp;
  iter->tree_node = G_NODE (model->root)->children;
}


gboolean
gtk_tree_store_iter_is_ancestor (GtkTreeStore *model,
				 GtkTreeIter  *iter,
				 GtkTreeIter  *descendant)
{
  g_return_val_if_fail (model != NULL, FALSE);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), FALSE);
  g_return_val_if_fail (iter != NULL, FALSE);
  g_return_val_if_fail (descendant != NULL, FALSE);
  g_return_val_if_fail (iter->stamp == model->stamp, FALSE);
  g_return_val_if_fail (descendant->stamp == model->stamp, FALSE);

  return g_node_is_ancestor (G_NODE (iter->tree_node),
			     G_NODE (descendant->tree_node));
}


gint
gtk_tree_store_iter_depth (GtkTreeStore *model,
			   GtkTreeIter  *iter)
{
  g_return_val_if_fail (model != NULL, 0);
  g_return_val_if_fail (GTK_IS_TREE_STORE (model), 0);
  g_return_val_if_fail (iter != NULL, 0);
  g_return_val_if_fail (iter->stamp == model->stamp, 0);

  return g_node_depth (G_NODE (iter->tree_node)) - 1;
}
