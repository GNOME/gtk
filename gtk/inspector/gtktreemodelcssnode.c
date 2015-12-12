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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtktreemodelcssnode.h"
#include "gtk/gtkcsstransientnodeprivate.h"

struct _GtkTreeModelCssNodePrivate
{
  GtkTreeModelCssNodeGetFunc    get_func;
  gint                          n_columns;
  GType                        *column_types;

  GtkCssNode                   *root;
};

static void gtk_tree_model_css_node_connect_node    (GtkTreeModelCssNode *model,
                                                     GtkCssNode          *node,
                                                     gboolean             emit_signal);

static void gtk_tree_model_css_node_disconnect_node (GtkTreeModelCssNode *model,
                                                     GtkCssNode          *node,
                                                     gboolean             emit_signal,
                                                     GtkCssNode          *parent,
                                                     GtkCssNode          *previous);

static void gtk_tree_model_css_node_tree_model_init (GtkTreeModelIface   *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTreeModelCssNode, gtk_tree_model_css_node, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkTreeModelCssNode)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						gtk_tree_model_css_node_tree_model_init))

static GtkCssNode *
get_nth_child (GtkCssNode *node,
               gint        i)
{
  for (node = gtk_css_node_get_first_child (node);
       node != NULL && i > 0;
       node = gtk_css_node_get_next_sibling (node))
    i--;

  return node;
}

static int
get_node_index (GtkCssNode *node)
{
  int result = 0;

  while ((node = gtk_css_node_get_previous_sibling (node)))
    result++;

  return result;
}

static GtkTreeModelFlags
gtk_tree_model_css_node_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint
gtk_tree_model_css_node_get_n_columns (GtkTreeModel *tree_model)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;

  return priv->n_columns;
}

static GType
gtk_tree_model_css_node_get_column_type (GtkTreeModel *tree_model,
                                         gint          column)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;

  g_return_val_if_fail (column < priv->n_columns, G_TYPE_INVALID);

  return priv->column_types[column];
}

static gboolean
gtk_tree_model_css_node_get_iter (GtkTreeModel *tree_model,
                                  GtkTreeIter  *iter,
                                  GtkTreePath  *path)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;
  GtkCssNode *node;
  int *indices;
  int depth, i;

  if (priv->root == NULL)
    return FALSE;

  indices = gtk_tree_path_get_indices (path);
  depth = gtk_tree_path_get_depth (path);

  if (depth < 1 || indices[0] != 0)
    return FALSE;

  node = priv->root;
  for (i = 1; i < depth; i++)
    {
      node = get_nth_child (node, indices[i]);
      if (node == NULL)
        return FALSE;
    }

  gtk_tree_model_css_node_get_iter_from_node (nodemodel, iter, node);
  return TRUE;
}

static GtkTreePath *
gtk_tree_model_css_node_get_path (GtkTreeModel *tree_model,
			          GtkTreeIter  *iter)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;
  GtkCssNode *node;
  GtkTreePath *path;

  g_return_val_if_fail (priv->root != NULL, NULL);

  path = gtk_tree_path_new ();
  node = gtk_tree_model_css_node_get_node_from_iter (nodemodel, iter);

  while (node != priv->root)
    {
      gtk_tree_path_prepend_index (path, get_node_index (node));
      node = gtk_css_node_get_parent (node);
    }

  gtk_tree_path_prepend_index (path, 0);

  return path;
}

static void
gtk_tree_model_css_node_get_value (GtkTreeModel *tree_model,
                                   GtkTreeIter  *iter,
                                   gint          column,
                                   GValue       *value)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;

  g_value_init (value, priv->column_types[column]);
  priv->get_func (nodemodel,
                  gtk_tree_model_css_node_get_node_from_iter (nodemodel, iter),
                  column,
                  value);
}

static gboolean
gtk_tree_model_css_node_iter_next (GtkTreeModel  *tree_model,
			           GtkTreeIter   *iter)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;
  GtkCssNode *node;

  node = gtk_tree_model_css_node_get_node_from_iter (nodemodel, iter);
  if (node == priv->root)
    return FALSE;
  
  node = gtk_css_node_get_next_sibling (node);
  if (node == NULL)
    return FALSE;

  gtk_tree_model_css_node_get_iter_from_node (nodemodel, iter, node);
  return TRUE;
}

static gboolean
gtk_tree_model_css_node_iter_previous (GtkTreeModel  *tree_model,
			               GtkTreeIter   *iter)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;
  GtkCssNode *node;

  node = gtk_tree_model_css_node_get_node_from_iter (nodemodel, iter);
  if (node == priv->root)
    return FALSE;
  
  node = gtk_css_node_get_previous_sibling (node);
  if (node == NULL)
    return FALSE;

  gtk_tree_model_css_node_get_iter_from_node (nodemodel, iter, node);
  return TRUE;
}

static gboolean
gtk_tree_model_css_node_iter_children (GtkTreeModel *tree_model,
                                       GtkTreeIter  *iter,
                                       GtkTreeIter  *parent)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;
  GtkCssNode *node;

  if (parent == NULL)
    {
      node = priv->root;
    }
  else
    {
      node = gtk_tree_model_css_node_get_node_from_iter (nodemodel, parent);
      node = gtk_css_node_get_first_child (node);
    }
  if (node == NULL)
    return FALSE;

  gtk_tree_model_css_node_get_iter_from_node (nodemodel, iter, node);
  return TRUE;
}

static gboolean
gtk_tree_model_css_node_iter_has_child (GtkTreeModel *tree_model,
			                GtkTreeIter  *iter)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkCssNode *node;

  node = gtk_tree_model_css_node_get_node_from_iter (nodemodel, iter);

  return gtk_css_node_get_first_child (node) != NULL;
}

static gint
gtk_tree_model_css_node_iter_n_children (GtkTreeModel *tree_model,
                                         GtkTreeIter  *iter)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;
  GtkCssNode *node;

  if (iter == NULL)
    return priv->root ? 1 : 0;

  node = gtk_tree_model_css_node_get_node_from_iter (nodemodel, iter);

  node = gtk_css_node_get_last_child (node);
  if (node == NULL)
    return 0;

  return get_node_index (node) + 1;
}

static gboolean
gtk_tree_model_css_node_iter_nth_child (GtkTreeModel *tree_model,
                                        GtkTreeIter  *iter,
                                        GtkTreeIter  *parent,
                                        gint          n)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;
  GtkCssNode *node;

  if (parent == NULL)
    {
      if (n > 0)
        return FALSE;
      
      node = priv->root;
    }
  else
    {
      node = gtk_tree_model_css_node_get_node_from_iter (nodemodel, parent);
      node = get_nth_child (node, n);
    }

  if (node == NULL)
    return FALSE;
  
  gtk_tree_model_css_node_get_iter_from_node (nodemodel, iter, node);
  return TRUE;
}

static gboolean
gtk_tree_model_css_node_iter_parent (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     GtkTreeIter  *child)
{
  GtkTreeModelCssNode *nodemodel = GTK_TREE_MODEL_CSS_NODE (tree_model);
  GtkTreeModelCssNodePrivate *priv = nodemodel->priv;
  GtkCssNode *node;

  node = gtk_tree_model_css_node_get_node_from_iter (nodemodel, child);
  if (node == priv->root)
    return FALSE;

  node = gtk_css_node_get_parent (node);

  gtk_tree_model_css_node_get_iter_from_node (nodemodel, iter, node);
  return TRUE;
}

static void
gtk_tree_model_css_node_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_tree_model_css_node_get_flags;
  iface->get_n_columns = gtk_tree_model_css_node_get_n_columns;
  iface->get_column_type = gtk_tree_model_css_node_get_column_type;
  iface->get_iter = gtk_tree_model_css_node_get_iter;
  iface->get_path = gtk_tree_model_css_node_get_path;
  iface->get_value = gtk_tree_model_css_node_get_value;
  iface->iter_next = gtk_tree_model_css_node_iter_next;
  iface->iter_previous = gtk_tree_model_css_node_iter_previous;
  iface->iter_children = gtk_tree_model_css_node_iter_children;
  iface->iter_has_child = gtk_tree_model_css_node_iter_has_child;
  iface->iter_n_children = gtk_tree_model_css_node_iter_n_children;
  iface->iter_nth_child = gtk_tree_model_css_node_iter_nth_child;
  iface->iter_parent = gtk_tree_model_css_node_iter_parent;
}

static void
gtk_tree_model_css_node_finalize (GObject *object)
{
  GtkTreeModelCssNode *model = GTK_TREE_MODEL_CSS_NODE (object);
  GtkTreeModelCssNodePrivate *priv = model->priv;

  if (priv->root)
    {
      gtk_tree_model_css_node_disconnect_node (model, priv->root, FALSE, NULL, NULL);
      priv->root = NULL;
    }

  G_OBJECT_CLASS (gtk_tree_model_css_node_parent_class)->finalize (object);
}

static void
gtk_tree_model_css_node_class_init (GtkTreeModelCssNodeClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_tree_model_css_node_finalize;
}

static void
gtk_tree_model_css_node_init (GtkTreeModelCssNode *nodemodel)
{
  nodemodel->priv = gtk_tree_model_css_node_get_instance_private (nodemodel);
}

GtkTreeModel *
gtk_tree_model_css_node_new (GtkTreeModelCssNodeGetFunc get_func,
                             gint                       n_columns,
			     ...)
{
  GtkTreeModel *result;
  va_list args;
  GType *types;
  gint i;

  g_return_val_if_fail (get_func != NULL, NULL);
  g_return_val_if_fail (n_columns > 0, NULL);

  types = g_new (GType, n_columns);
  va_start (args, n_columns);

  for (i = 0; i < n_columns; i++)
    {
      types[i] = va_arg (args, GType);
    }

  va_end (args);

  result = gtk_tree_model_css_node_newv (get_func, n_columns, types);

  g_free (types);

  return result;
}

GtkTreeModel *
gtk_tree_model_css_node_newv (GtkTreeModelCssNodeGetFunc  get_func,
                              gint                        n_columns,
			      GType                      *types)
{
  GtkTreeModelCssNode *result;
  GtkTreeModelCssNodePrivate *priv;

  g_return_val_if_fail (get_func != NULL, NULL);
  g_return_val_if_fail (n_columns > 0, NULL);
  g_return_val_if_fail (types != NULL, NULL);

  result = g_object_new (GTK_TYPE_TREE_MODEL_CSS_NODE, NULL);
  priv = result->priv;

  priv->get_func = get_func;
  priv->n_columns = n_columns;
  priv->column_types = g_memdup (types, sizeof (GType) * n_columns);

  return GTK_TREE_MODEL (result);
}

static void
child_added_cb (GtkCssNode          *node,
                GtkCssNode          *child,
                GtkCssNode          *previous,
                GtkTreeModelCssNode *model)
{
  gtk_tree_model_css_node_connect_node (model, child, TRUE);
}

static void
child_removed_cb (GtkCssNode          *node,
                  GtkCssNode          *child,
                  GtkCssNode          *previous,
                  GtkTreeModelCssNode *model)
{
  gtk_tree_model_css_node_disconnect_node (model, child, TRUE, node, previous);
}

static void
notify_cb (GtkCssNode          *node,
           GParamSpec          *pspec,
           GtkTreeModelCssNode *model)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  gtk_tree_model_css_node_get_iter_from_node (model, &iter, node);
  path = gtk_tree_model_css_node_get_path (GTK_TREE_MODEL (model), &iter);

  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);

  gtk_tree_path_free (path);
}

static void
style_changed_cb (GtkCssNode          *node,
                  GtkCssStyleChange   *change,
                  GtkTreeModelCssNode *model)
{
  GtkTreeIter iter;
  GtkTreePath *path;

  gtk_tree_model_css_node_get_iter_from_node (model, &iter, node);
  path = gtk_tree_model_css_node_get_path (GTK_TREE_MODEL (model), &iter);

  gtk_tree_model_row_changed (GTK_TREE_MODEL (model), path, &iter);

  gtk_tree_path_free (path);
}

static void
gtk_tree_model_css_node_connect_node (GtkTreeModelCssNode *model,
                                      GtkCssNode          *node,
                                      gboolean             emit_signal)
{
  GtkCssNode *child;

  if (GTK_IS_CSS_TRANSIENT_NODE (node))
    return;

  g_object_ref (node);

  g_signal_connect_after (node, "node-added", G_CALLBACK (child_added_cb), model);
  g_signal_connect_after (node, "node-removed", G_CALLBACK (child_removed_cb), model);
  g_signal_connect_after (node, "notify", G_CALLBACK (notify_cb), model);
  g_signal_connect_after (node, "style-changed", G_CALLBACK (style_changed_cb), model);

  for (child = gtk_css_node_get_first_child (node);
       child;
       child = gtk_css_node_get_next_sibling (child))
    {
      gtk_tree_model_css_node_connect_node (model, child, FALSE);
    }

  if (emit_signal)
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      if (node != model->priv->root &&
          gtk_css_node_get_previous_sibling (node) == NULL &&
          gtk_css_node_get_next_sibling (node) == NULL)
        {
          /* We're the first child of the parent */
          gtk_tree_model_css_node_get_iter_from_node (model, &iter, gtk_css_node_get_parent (node));
          path = gtk_tree_model_css_node_get_path (GTK_TREE_MODEL (model), &iter);
          gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
          gtk_tree_path_free (path);
        }

      gtk_tree_model_css_node_get_iter_from_node (model, &iter, node);
      path = gtk_tree_model_css_node_get_path (GTK_TREE_MODEL (model), &iter);
      gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
      if (gtk_css_node_get_first_child (node))
        gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);

      gtk_tree_path_free (path);
    }
}

static void
gtk_tree_model_css_node_disconnect_node (GtkTreeModelCssNode *model,
                                         GtkCssNode          *node,
                                         gboolean             emit_signal,
                                         GtkCssNode          *parent,
                                         GtkCssNode          *previous)
{
  GtkCssNode *child;

  if (GTK_IS_CSS_TRANSIENT_NODE (node))
    return;

  g_signal_handlers_disconnect_by_func (node, G_CALLBACK (child_added_cb), model);
  g_signal_handlers_disconnect_by_func (node, G_CALLBACK (child_removed_cb), model);
  g_signal_handlers_disconnect_by_func (node, G_CALLBACK (notify_cb), model);
  g_signal_handlers_disconnect_by_func (node, G_CALLBACK (style_changed_cb), model);

  for (child = gtk_css_node_get_first_child (node);
       child;
       child = gtk_css_node_get_next_sibling (child))
    {
      gtk_tree_model_css_node_disconnect_node (model, child, FALSE, NULL, NULL);
    }

  if (emit_signal)
    {
      GtkTreeIter iter;
      GtkTreePath *path;

      if (parent)
        {
          gtk_tree_model_css_node_get_iter_from_node (model, &iter, parent);
          path = gtk_tree_model_css_node_get_path (GTK_TREE_MODEL (model), &iter);
        }
      else
        {
          path = gtk_tree_path_new ();
        }
      if (previous)
        gtk_tree_path_append_index (path, get_node_index (previous) + 1);
      else
        gtk_tree_path_append_index (path, 0);

      gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);

      if (parent && gtk_css_node_get_first_child (parent) == NULL)
        {
          gtk_tree_path_up (path);
          gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
        }

      gtk_tree_path_free (path);
    }
  
  g_object_unref (node);
}

void
gtk_tree_model_css_node_set_root_node (GtkTreeModelCssNode *model,
                                       GtkCssNode          *node)
{
  GtkTreeModelCssNodePrivate *priv;

  g_return_if_fail (GTK_IS_TREE_MODEL_CSS_NODE (model));
  g_return_if_fail (node == NULL || GTK_IS_CSS_NODE (node));

  priv = model->priv;

  if (priv->root == node)
    return;

  if (priv->root)
    {
      gtk_tree_model_css_node_disconnect_node (model, priv->root, TRUE, NULL, NULL);
      priv->root = NULL;
    }

  if (node)
    {
      priv->root = node;
      gtk_tree_model_css_node_connect_node (model, node, TRUE);
    }
}

GtkCssNode *
gtk_tree_model_css_node_get_root_node (GtkTreeModelCssNode *model)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_CSS_NODE (model), NULL);

  return model->priv->root;
}

GtkCssNode *
gtk_tree_model_css_node_get_node_from_iter (GtkTreeModelCssNode *model,
                                            GtkTreeIter         *iter)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_CSS_NODE (model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);
  g_return_val_if_fail (iter->user_data == model, NULL);
  g_return_val_if_fail (GTK_IS_CSS_NODE (iter->user_data2), NULL);

  return iter->user_data2;
}

void
gtk_tree_model_css_node_get_iter_from_node (GtkTreeModelCssNode *model,
                                            GtkTreeIter         *iter,
                                            GtkCssNode          *node)
{
  g_return_if_fail (GTK_IS_TREE_MODEL_CSS_NODE (model));
  g_return_if_fail (iter != NULL);
  g_return_if_fail (GTK_IS_CSS_NODE (node));

  iter->user_data = model;
  iter->user_data2 = node;
}
