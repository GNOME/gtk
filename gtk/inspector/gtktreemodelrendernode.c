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

#include "gtktreemodelrendernode.h"

#include "gsk/gskrendernodeprivate.h"

typedef struct _TreeElement TreeElement;

/* This is an array of all nodes and the index of their parent. When adding a node,
 * we first add the node itself, and then all their children pointing the parent to
 * this element.
 */
struct _TreeElement
{
  GskRenderNode *node;
  int            parent;
};

struct _GtkTreeModelRenderNodePrivate
{
  GtkTreeModelRenderNodeGetFunc  get_func;
  gint                           n_columns;
  GType                         *column_types;

  GskRenderNode                 *root;
  GArray                        *nodes;
};

static void gtk_tree_model_render_node_tree_model_init  (GtkTreeModelIface   *iface);

G_DEFINE_TYPE_WITH_CODE (GtkTreeModelRenderNode, gtk_tree_model_render_node, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GtkTreeModelRenderNode)
			 G_IMPLEMENT_INTERFACE (GTK_TYPE_TREE_MODEL,
						gtk_tree_model_render_node_tree_model_init))

static gint
element_from_iter (GtkTreeModelRenderNode *model,
                   GtkTreeIter            *iter)
{
  return GPOINTER_TO_INT (iter->user_data2);
}

static void
iter_from_element (GtkTreeModelRenderNode *model,
                   GtkTreeIter            *iter,
                   gint                    elt)
{
  iter->user_data = model;
  iter->user_data2 = GINT_TO_POINTER (elt);
}

static GskRenderNode *
node_from_element (GtkTreeModelRenderNode *model,
                   gint                    elt)
{
  return g_array_index (model->priv->nodes, TreeElement, elt).node;
}

static gint
parent_element (GArray *nodes,
        gint    idx)
{
  return g_array_index (nodes, TreeElement, idx).parent;
}

static gint
get_nth_child (GArray *nodes,
               gint    elt,
               gint    nth)
{
  guint i, count;

  count = 0;
  for (i = elt + 1; i < nodes->len; i++)
    {
      gint parent = parent_element (nodes, i);

      if (parent < elt)
        return FALSE;

      if (parent != elt)
        continue;

      if (count == nth)
        return i;

      count++;
    }

  return -1;
}

static gint
get_node_index (GArray *nodes,
                gint    elt)
{
  gint parent, i, idx;

  if (elt == 0)
    return 0;

  parent = parent_element (nodes, elt);
  idx = 0;

  for (i = elt; i > parent; i--)
    {
      if (parent_element (nodes, i) == parent)
        idx++;
    }

  return idx;
}

static GtkTreeModelFlags
gtk_tree_model_render_node_get_flags (GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_ITERS_PERSIST;
}

static gint
gtk_tree_model_render_node_get_n_columns (GtkTreeModel *tree_model)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;

  return priv->n_columns;
}

static GType
gtk_tree_model_render_node_get_column_type (GtkTreeModel *tree_model,
                                            gint          column)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;

  g_return_val_if_fail (column < priv->n_columns, G_TYPE_INVALID);

  return priv->column_types[column];
}

static gboolean
gtk_tree_model_render_node_get_iter (GtkTreeModel *tree_model,
                                     GtkTreeIter  *iter,
                                     GtkTreePath  *path)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;
  int *indices;
  int depth, i;
  int elt;

  if (priv->root == NULL)
    return FALSE;

  indices = gtk_tree_path_get_indices (path);
  depth = gtk_tree_path_get_depth (path);

  if (depth < 1 || indices[0] != 0)
    return FALSE;

  elt = 0;
  for (i = 1; i < depth; i++)
    {
      elt = get_nth_child (priv->nodes, elt, indices[i]);
      if (elt < 0)
        return FALSE;
    }

  iter_from_element (nodemodel, iter, elt);
  return TRUE;
}

static GtkTreePath *
gtk_tree_model_render_node_get_path (GtkTreeModel *tree_model,
			             GtkTreeIter  *iter)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;
  GtkTreePath *path;
  gint elt;

  g_return_val_if_fail (priv->root != NULL, NULL);

  path = gtk_tree_path_new ();

  for (elt = element_from_iter (nodemodel, iter); elt >= 0; elt = parent_element (priv->nodes, elt))
    {
      gtk_tree_path_prepend_index (path, get_node_index (priv->nodes, elt));
    }

  gtk_tree_path_prepend_index (path, 0);

  return path;
}

static void
gtk_tree_model_render_node_get_value (GtkTreeModel *tree_model,
                                      GtkTreeIter  *iter,
                                      gint          column,
                                      GValue       *value)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;

  g_value_init (value, priv->column_types[column]);
  priv->get_func (nodemodel,
                  node_from_element (nodemodel, element_from_iter (nodemodel, iter)),
                  column,
                  value);
}

static gboolean
gtk_tree_model_render_node_iter_next (GtkTreeModel *tree_model,
			              GtkTreeIter  *iter)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;
  gint i, parent;

  i = element_from_iter (nodemodel, iter);
  parent = parent_element (priv->nodes, i);

  if (parent < 0)
    return FALSE;

  for (i = i + 1; i < priv->nodes->len; i++)
    {
      if (parent_element (priv->nodes, i) < parent)
        return FALSE;

      if (parent_element (priv->nodes, i) == parent)
        {
          iter_from_element (nodemodel, iter, i);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gtk_tree_model_render_node_iter_previous (GtkTreeModel  *tree_model,
			                  GtkTreeIter   *iter)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;
  gint i, parent;

  i = element_from_iter (nodemodel, iter);
  parent = parent_element (priv->nodes, i);

  if (parent < 0)
    return FALSE;

  for (i = i - 1; i > parent; i--)
    {
      if (parent_element (priv->nodes, i) == parent)
        {
          iter_from_element (nodemodel, iter, i);
          return TRUE;
        }
    }

  return FALSE;
}

static gboolean
gtk_tree_model_render_node_iter_children (GtkTreeModel *tree_model,
                                          GtkTreeIter  *iter,
                                          GtkTreeIter  *parent)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;
  gint elt;

  if (parent == NULL)
    {
      if (!priv->root)
        return FALSE;

      iter_from_element (nodemodel, iter, 0);
      return TRUE;
    }
  else
    {
      elt = element_from_iter (nodemodel, parent);
      if (elt + 1 >= priv->nodes->len)
        return FALSE;
      if (parent_element (priv->nodes, elt + 1) != elt)
        return FALSE;

      iter_from_element (nodemodel, iter, elt + 1);
      return TRUE;
    }
}

static gboolean
gtk_tree_model_render_node_iter_has_child (GtkTreeModel *tree_model,
			                   GtkTreeIter  *iter)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;
  gint elt;

  elt = element_from_iter (nodemodel, iter);
  if (elt + 1 >= priv->nodes->len)
    return FALSE;
  if (parent_element (priv->nodes, elt + 1) != elt)
    return FALSE;

  return TRUE;
}

static gint
gtk_tree_model_render_node_iter_n_children (GtkTreeModel *tree_model,
                                            GtkTreeIter  *iter)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;
  gint elt, i, count;

  if (iter == NULL)
    return priv->root ? 1 : 0;

  elt = element_from_iter (nodemodel, iter);
  count = 0;

  for (i = elt + 1; i < priv->nodes->len; i++)
    {
      int parent = parent_element (priv->nodes, i);

      if (parent < elt)
        break;

      if (parent == elt)
        count++;
    }

  return count;
}

static gboolean
gtk_tree_model_render_node_iter_nth_child (GtkTreeModel *tree_model,
                                           GtkTreeIter  *iter,
                                           GtkTreeIter  *parent,
                                           gint          n)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;
  gint elt;

  if (parent == NULL)
    {
      if (n > 0)
        return FALSE;
      
      iter_from_element (nodemodel, iter, 0);
      return TRUE;
    }
  else
    {
      gint nth;

      elt = element_from_iter (nodemodel, parent);
      nth = get_nth_child (priv->nodes, elt, n);
      if (nth < 0)
        return FALSE;

      iter_from_element (nodemodel, iter, nth);
      return TRUE;
    }
}

static gboolean
gtk_tree_model_render_node_iter_parent (GtkTreeModel *tree_model,
                                        GtkTreeIter  *iter,
                                        GtkTreeIter  *child)
{
  GtkTreeModelRenderNode *nodemodel = GTK_TREE_MODEL_RENDER_NODE (tree_model);
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;
  gint elt, parent;

  elt = element_from_iter (nodemodel, child);
  parent = parent_element (priv->nodes, elt);

  if (parent < 0)
    return FALSE;

  iter_from_element (nodemodel, iter, parent);
  return TRUE;
}

static void
gtk_tree_model_render_node_tree_model_init (GtkTreeModelIface *iface)
{
  iface->get_flags = gtk_tree_model_render_node_get_flags;
  iface->get_n_columns = gtk_tree_model_render_node_get_n_columns;
  iface->get_column_type = gtk_tree_model_render_node_get_column_type;
  iface->get_iter = gtk_tree_model_render_node_get_iter;
  iface->get_path = gtk_tree_model_render_node_get_path;
  iface->get_value = gtk_tree_model_render_node_get_value;
  iface->iter_next = gtk_tree_model_render_node_iter_next;
  iface->iter_previous = gtk_tree_model_render_node_iter_previous;
  iface->iter_children = gtk_tree_model_render_node_iter_children;
  iface->iter_has_child = gtk_tree_model_render_node_iter_has_child;
  iface->iter_n_children = gtk_tree_model_render_node_iter_n_children;
  iface->iter_nth_child = gtk_tree_model_render_node_iter_nth_child;
  iface->iter_parent = gtk_tree_model_render_node_iter_parent;
}

static void
gtk_tree_model_render_node_finalize (GObject *object)
{
  GtkTreeModelRenderNode *model = GTK_TREE_MODEL_RENDER_NODE (object);
  GtkTreeModelRenderNodePrivate *priv = model->priv;

  g_clear_pointer (&priv->root, gsk_render_node_unref);
  g_clear_pointer (&priv->nodes, g_array_unref);

  G_OBJECT_CLASS (gtk_tree_model_render_node_parent_class)->finalize (object);
}

static void
gtk_tree_model_render_node_class_init (GtkTreeModelRenderNodeClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_tree_model_render_node_finalize;
}

static void
gtk_tree_model_render_node_init (GtkTreeModelRenderNode *nodemodel)
{
  nodemodel->priv = gtk_tree_model_render_node_get_instance_private (nodemodel);

  nodemodel->priv->nodes = g_array_new (FALSE, FALSE, sizeof (TreeElement));
}

GtkTreeModel *
gtk_tree_model_render_node_new (GtkTreeModelRenderNodeGetFunc get_func,
                                gint                          n_columns,
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

  result = gtk_tree_model_render_node_newv (get_func, n_columns, types);

  g_free (types);

  return result;
}

GtkTreeModel *
gtk_tree_model_render_node_newv (GtkTreeModelRenderNodeGetFunc  get_func,
                                 gint                           n_columns,
			         GType                         *types)
{
  GtkTreeModelRenderNode *result;
  GtkTreeModelRenderNodePrivate *priv;

  g_return_val_if_fail (get_func != NULL, NULL);
  g_return_val_if_fail (n_columns > 0, NULL);
  g_return_val_if_fail (types != NULL, NULL);

  result = g_object_new (GTK_TYPE_TREE_MODEL_RENDER_NODE, NULL);
  priv = result->priv;

  priv->get_func = get_func;
  priv->n_columns = n_columns;
  priv->column_types = g_memdup (types, sizeof (GType) * n_columns);

  return GTK_TREE_MODEL (result);
}

static void
append_node (GtkTreeModelRenderNode *nodemodel,
             GskRenderNode          *node,
             int                     parent_index)
{
  GtkTreeModelRenderNodePrivate *priv = nodemodel->priv;
  TreeElement element = { node, parent_index };

  g_array_append_val (priv->nodes, element);

  switch (gsk_render_node_get_node_type (node))
    {
    default:
    case GSK_NOT_A_RENDER_NODE:
      g_assert_not_reached ();
      break;

    case GSK_CAIRO_NODE:
    case GSK_TEXT_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
      /* no children */
      break;

    case GSK_TRANSFORM_NODE:
      append_node (nodemodel, gsk_transform_node_get_child (node), priv->nodes->len - 1);
      break;

    case GSK_OPACITY_NODE:
      append_node (nodemodel, gsk_opacity_node_get_child (node), priv->nodes->len - 1);
      break;

    case GSK_COLOR_MATRIX_NODE:
      append_node (nodemodel, gsk_color_matrix_node_get_child (node), priv->nodes->len - 1);
      break;

    case GSK_BLUR_NODE:
      append_node (nodemodel, gsk_blur_node_get_child (node), priv->nodes->len - 1);
      break;

    case GSK_REPEAT_NODE:
      append_node (nodemodel, gsk_repeat_node_get_child (node), priv->nodes->len - 1);
      break;

    case GSK_CLIP_NODE:
      append_node (nodemodel, gsk_clip_node_get_child (node), priv->nodes->len - 1);
      break;

    case GSK_ROUNDED_CLIP_NODE:
      append_node (nodemodel, gsk_rounded_clip_node_get_child (node), priv->nodes->len - 1);
      break;

    case GSK_SHADOW_NODE:
      append_node (nodemodel, gsk_shadow_node_get_child (node), priv->nodes->len - 1);
      break;

    case GSK_BLEND_NODE:
      {
        int elt_index = priv->nodes->len - 1;

        append_node (nodemodel, gsk_blend_node_get_bottom_child (node), elt_index);
        append_node (nodemodel, gsk_blend_node_get_top_child (node), elt_index);
      }
      break;

    case GSK_CROSS_FADE_NODE:
      {
        int elt_index = priv->nodes->len - 1;

        append_node (nodemodel, gsk_cross_fade_node_get_start_child (node), elt_index);
        append_node (nodemodel, gsk_cross_fade_node_get_end_child (node), elt_index);
      }
      break;

    case GSK_CONTAINER_NODE:
      {
        gint elt_index;
        guint i;

        elt_index = priv->nodes->len - 1;
        for (i = 0; i < gsk_container_node_get_n_children (node); i++)
          {
            append_node (nodemodel, gsk_container_node_get_child (node, i), elt_index);
          }
      }
      break;
    }
}

void
gtk_tree_model_render_node_set_root_node (GtkTreeModelRenderNode *model,
                                          GskRenderNode          *node)
{
  GtkTreeModelRenderNodePrivate *priv;
  GtkTreePath *path;

  g_return_if_fail (GTK_IS_TREE_MODEL_RENDER_NODE (model));
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  priv = model->priv;

  if (priv->root == node)
    return;

  if (priv->root)
    {
      path = gtk_tree_path_new_first ();
      gtk_tree_model_row_deleted (GTK_TREE_MODEL (model), path);
      gtk_tree_path_free (path);

      gsk_render_node_unref (priv->root);
      g_array_set_size (priv->nodes, 0);
    }

  priv->root = node;

  if (node)
    {
      GtkTreeIter iter;

      gsk_render_node_ref (node);
      append_node (model, node, -1);

      iter_from_element (model, &iter, 0);
      path = gtk_tree_path_new_first ();
      gtk_tree_model_row_inserted (GTK_TREE_MODEL (model), path, &iter);
      if (priv->nodes->len > 1)
        gtk_tree_model_row_has_child_toggled (GTK_TREE_MODEL (model), path, &iter);
      gtk_tree_path_free (path);
    }
}

GskRenderNode *
gtk_tree_model_render_node_get_root_node (GtkTreeModelRenderNode *model)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_RENDER_NODE (model), NULL);

  return model->priv->root;
}

GskRenderNode *
gtk_tree_model_render_node_get_node_from_iter (GtkTreeModelRenderNode *model,
                                               GtkTreeIter            *iter)
{
  g_return_val_if_fail (GTK_IS_TREE_MODEL_RENDER_NODE (model), NULL);
  g_return_val_if_fail (iter != NULL, NULL);
  g_return_val_if_fail (iter->user_data == model, NULL);
  g_return_val_if_fail (GPOINTER_TO_INT (iter->user_data2) < model->priv->nodes->len, NULL);

  return node_from_element (model, element_from_iter (model, iter));
}

