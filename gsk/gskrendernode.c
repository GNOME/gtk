/* GSK - The GTK Scene Kit
 *
 * Copyright 2016  Endless
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:GskRenderNode
 * @Title: GskRenderNode
 * @Short_description: Simple scene graph element
 *
 * #GskRenderNode is the basic block in a scene graph to be
 * rendered using #GskRenderer.
 *
 * Each node has a parent, except the top-level node; each node may have
 * children nodes.
 *
 * Each node has an associated drawing surface, which has the size of
 * the rectangle set using gsk_render_node_set_bounds(). Nodes have an
 * associated transformation matrix, which is used to position and
 * transform the node on the scene graph; additionally, they also have
 * a child transformation matrix, which will be applied to each child.
 *
 * Render nodes are meant to be transient; once they have been associated
 * to a #GskRenderer it's safe to release any reference you have on them.
 * Once a #GskRenderNode has been rendered, it is marked as immutable, and
 * cannot be modified.
 */

#include "config.h"

#include "gskrendernodeprivate.h"

#include "gskdebugprivate.h"
#include "gskrendernodeiter.h"
#include "gskrendererprivate.h"
#include "gsktexture.h"

#include <graphene-gobject.h>

#include <math.h>

#include <gobject/gvaluecollector.h>

/**
 * GskRenderNode: (ref-func gsk_render_node_ref) (unref-func gsk_render_node_unref) (set-value-func gsk_value_set_render_node) (get-value-func gsk_value_get_render_node)
 *
 * The `GskRenderNode` structure contains only private data.
 *
 * Since: 3.90
 */

static void
value_render_node_init (GValue *value)
{
  value->data[0].v_pointer = NULL;
}

static void
value_render_node_free (GValue *value)
{
  if (value->data[0].v_pointer != NULL)
    gsk_render_node_unref (value->data[0].v_pointer);
}

static void
value_render_node_copy (const GValue *src,
                        GValue       *dst)
{
  if (src->data[0].v_pointer != NULL)
    dst->data[0].v_pointer = gsk_render_node_ref (src->data[0].v_pointer);
  else
    dst->data[0].v_pointer = NULL;
}

static gpointer
value_render_node_peek_pointer (const GValue *value)
{
  return value->data[0].v_pointer;
}

static gchar *
value_render_node_collect_value (GValue      *value,
                                 guint        n_collect_values,
                                 GTypeCValue *collect_values,
                                 guint        collect_flags)
{
  GskRenderNode *node;

  node = collect_values[0].v_pointer;

  if (node == NULL)
    {
      value->data[0].v_pointer = NULL;
      return NULL;
    }

  if (node->parent_instance.g_class == NULL)
    return g_strconcat ("invalid unclassed GskRenderNode pointer for "
                        "value type '",
                        G_VALUE_TYPE_NAME (value),
                        "'",
                        NULL);

  value->data[0].v_pointer = gsk_render_node_ref (node);

  return NULL;
}

static gchar *
value_render_node_lcopy_value (const GValue *value,
                               guint         n_collect_values,
                               GTypeCValue  *collect_values,
                               guint         collect_flags)
{
  GskRenderNode **node_p = collect_values[0].v_pointer;

  if (node_p == NULL)
    return g_strconcat ("value location for '",
                        G_VALUE_TYPE_NAME (value),
                        "' passed as NULL",
                        NULL);

  if (value->data[0].v_pointer == NULL)
    *node_p = NULL;
  else if (collect_flags & G_VALUE_NOCOPY_CONTENTS)
    *node_p = value->data[0].v_pointer;
  else
    *node_p = gsk_render_node_ref (value->data[0].v_pointer);

  return NULL;
}

static void
gsk_render_node_finalize (GskRenderNode *self)
{
  GskRenderNodeIter iter;

  self->is_mutable = TRUE;

  g_clear_pointer (&self->surface, cairo_surface_destroy);
  g_clear_pointer (&self->name, g_free);

  gsk_render_node_iter_init (&iter, self);
  while (gsk_render_node_iter_next (&iter, NULL))
    gsk_render_node_iter_remove (&iter);

  g_type_free_instance ((GTypeInstance *) self);
}

static void
gsk_render_node_class_base_init (GskRenderNodeClass *klass)
{
}

static void
gsk_render_node_class_base_finalize (GskRenderNodeClass *klass)
{
}

static void
gsk_render_node_class_init (GskRenderNodeClass *klass)
{
  klass->finalize = gsk_render_node_finalize;
}

static void
gsk_render_node_init (GskRenderNode *self)
{
  self->ref_count = 1;

  graphene_rect_init_from_rect (&self->bounds, graphene_rect_zero ());

  graphene_matrix_init_identity (&self->transform);

  graphene_point3d_init (&self->anchor_point, 0.f, 0.f, 0.f);

  self->opacity = 1.0;

  self->min_filter = GSK_SCALING_FILTER_NEAREST;
  self->mag_filter = GSK_SCALING_FILTER_NEAREST;

  self->is_mutable = TRUE;
  self->needs_world_matrix_update = TRUE;
}

GType
gsk_render_node_get_type (void)
{
  static volatile gsize gsk_render_node_type__volatile;

  if (g_once_init_enter (&gsk_render_node_type__volatile))
    {
      static const GTypeFundamentalInfo finfo = {
        (G_TYPE_FLAG_CLASSED |
         G_TYPE_FLAG_INSTANTIATABLE |
         G_TYPE_FLAG_DERIVABLE |
         G_TYPE_FLAG_DEEP_DERIVABLE),
      };
      static const GTypeValueTable render_node_value_table = {
        value_render_node_init,
        value_render_node_free,
        value_render_node_copy,
        value_render_node_peek_pointer,
        "p", value_render_node_collect_value,
        "p", value_render_node_lcopy_value,
      };
      const GTypeInfo render_node_info = {
        sizeof (GskRenderNodeClass),

        (GBaseInitFunc) gsk_render_node_class_base_init,
        (GBaseFinalizeFunc) gsk_render_node_class_base_finalize,
        (GClassInitFunc) gsk_render_node_class_init,
        (GClassFinalizeFunc) NULL,
        NULL,

        sizeof (GskRenderNode), 16,
        (GInstanceInitFunc) gsk_render_node_init,

        &render_node_value_table,
      };
      GType gsk_render_node_type =
        g_type_register_fundamental (g_type_fundamental_next (),
                                     g_intern_static_string ("GskRenderNode"),
                                     &render_node_info,
                                     &finfo,
                                     0);

      g_once_init_leave (&gsk_render_node_type__volatile, gsk_render_node_type);
    }

  return gsk_render_node_type__volatile;
}

/*< private >
 * gsk_render_node_new:
 * @renderer: a #GskRenderer
 *
 * Returns: (transfer full): the newly created #GskRenderNode
 */
GskRenderNode *
gsk_render_node_new (void)
{
  GskRenderNode *res = (GskRenderNode *) g_type_create_instance (GSK_TYPE_RENDER_NODE);

  return res;
}

/**
 * gsk_render_node_ref:
 * @node: a #GskRenderNode
 *
 * Acquires a reference on the given #GskRenderNode.
 *
 * Returns: (transfer none): the #GskRenderNode with an additional reference
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_ref (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  g_atomic_int_inc (&node->ref_count);

  return node;
}

/**
 * gsk_render_node_unref:
 * @node: a #GskRenderNode
 *
 * Releases a reference on the given #GskRenderNode.
 *
 * If the reference was the last, the resources associated to the @node are
 * freed.
 *
 * Since: 3.90
 */
void
gsk_render_node_unref (GskRenderNode *node)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  if (g_atomic_int_dec_and_test (&node->ref_count))
    GSK_RENDER_NODE_GET_CLASS (node)->finalize (node);
}

/**
 * gsk_render_node_get_parent:
 * @node: a #GskRenderNode
 *
 * Returns the parent of the @node.
 *
 * Returns: (transfer none): the parent of the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_get_parent (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return node->parent;
}

/**
 * gsk_render_node_get_first_child:
 * @node: a #GskRenderNode
 *
 * Returns the first child of @node.
 *
 * Returns: (transfer none): the first child of the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_get_first_child (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return node->first_child;
}

/**
 * gsk_render_node_get_last_child:
 * @node: a #GskRenderNode
 *
 * Returns the last child of @node.
 *
 * Returns: (transfer none): the last child of the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_get_last_child (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return node->last_child;
}

/**
 * gsk_render_node_get_next_sibling:
 * @node: a #GskRenderNode
 *
 * Returns the next sibling of @node.
 *
 * Returns: (transfer none): the next sibling of the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_get_next_sibling (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return node->next_sibling;
}

/**
 * gsk_render_node_get_previous_sibling:
 * @node: a #GskRenderNode
 *
 * Returns the previous sibling of @node.
 *
 * Returns: (transfer none): the previous sibling of the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_get_previous_sibling (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return node->prev_sibling;
}

typedef void (* InsertChildFunc) (GskRenderNode *node,
                                  GskRenderNode *child,
                                  gpointer       user_data);

static void
gsk_render_node_insert_child_internal (GskRenderNode   *node,
                                       GskRenderNode   *child,
                                       InsertChildFunc  insert_func,
                                       gpointer         insert_func_data)
{
  if (node == child)
    {
      g_critical ("The render node of type '%s' cannot be added to itself.",
                  G_OBJECT_TYPE_NAME (node));
      return;
    }

  if (child->parent != NULL)
    {
      g_critical ("The render node of type '%s' already has a parent of type '%s'; "
                  "render nodes cannot be added to multiple parents.",
		  G_OBJECT_TYPE_NAME (child),
		  G_OBJECT_TYPE_NAME (node));
      return;
    }

  if (!node->is_mutable)
    {
      g_critical ("The render node of type '%s' is immutable.",
                  G_OBJECT_TYPE_NAME (node));
      return;
    }

  insert_func (node, child, insert_func_data);

  gsk_render_node_ref (child);

  child->parent = node;
  child->age = 0;
  child->needs_world_matrix_update = TRUE;

  node->n_children += 1;
  node->age += 1;
  node->needs_world_matrix_update = TRUE;

  if (child->prev_sibling == NULL)
    node->first_child = child;
  if (child->next_sibling == NULL)
    node->last_child = child;
}

static void
insert_child_at_pos (GskRenderNode *node,
                     GskRenderNode *child,
                     gpointer       user_data)
{
  int pos = GPOINTER_TO_INT (user_data);

  if (pos == 0)
    {
      GskRenderNode *tmp = node->first_child;

      if (tmp != NULL)
	tmp->prev_sibling = child;

      child->prev_sibling = NULL;
      child->next_sibling = tmp;

      return;
    }

  if (pos < 0 || pos >= node->n_children)
    {
      GskRenderNode *tmp = node->last_child;

      if (tmp != NULL)
	tmp->next_sibling = child;

      child->prev_sibling = tmp;
      child->next_sibling = NULL;

      return;
    }

  {
    GskRenderNode *iter;
    int i;

    for (iter = node->first_child, i = 0;
	 iter != NULL;
	 iter = iter->next_sibling, i++)
      {
	if (i == pos)
	  {
	    GskRenderNode *tmp = iter->prev_sibling;

	    child->prev_sibling = tmp;
	    child->next_sibling = iter;

	    iter->prev_sibling = child;

	    if (tmp != NULL)
	      tmp->next_sibling = child;

	    break;
	  }
      }
  }
}

/**
 * gsk_render_node_append_child:
 * @node: a #GskRenderNode
 * @child: a #GskRenderNode
 *
 * Appends @child to the list of children of @node.
 *
 * This function acquires a reference on @child.
 *
 * Returns: (transfer none): the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_append_child (GskRenderNode *node,
                              GskRenderNode *child)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), node);
  g_return_val_if_fail (node->is_mutable, node);

  gsk_render_node_insert_child_internal (node, child,
					 insert_child_at_pos,
					 GINT_TO_POINTER (node->n_children));

  return node;
}

/**
 * gsk_render_node_prepend_child:
 * @node: a #GskRenderNode
 * @child: a #GskRenderNode
 *
 * Prepends @child to the list of children of @node.
 *
 * This function acquires a reference on @child.
 *
 * Returns: (transfer none): the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_prepend_child (GskRenderNode *node,
                               GskRenderNode *child)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), node);
  g_return_val_if_fail (node->is_mutable, node);

  gsk_render_node_insert_child_internal (node, child,
                                         insert_child_at_pos,
                                         GINT_TO_POINTER (0));

  return node;
}

/**
 * gsk_render_node_insert_child_at_pos:
 * @node: a #GskRenderNode
 * @child: a #GskRenderNode
 * @index_: the index in the list of children where @child should be inserted at
 *
 * Inserts @child into the list of children of @node, using the given @index_.
 *
 * If @index_ is 0, the @child will be prepended to the list of children.
 *
 * If @index_ is less than zero, or equal to the number of children, the @child
 * will be appended to the list of children.
 *
 * This function acquires a reference on @child.
 *
 * Returns: (transfer none): the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_insert_child_at_pos (GskRenderNode *node,
                                     GskRenderNode *child,
                                     int            index_)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), node);
  g_return_val_if_fail (node->is_mutable, node);

  gsk_render_node_insert_child_internal (node, child,
					 insert_child_at_pos,
					 GINT_TO_POINTER (index_));

  return node;
}

static void
insert_child_before (GskRenderNode *node,
                     GskRenderNode *child,
                     gpointer       user_data)
{
  GskRenderNode *sibling = user_data;

  if (sibling == NULL)
    sibling = node->first_child;

  child->next_sibling = sibling;

  if (sibling != NULL)
    {
      GskRenderNode *tmp = sibling->prev_sibling;

      child->prev_sibling = tmp;

      if (tmp != NULL)
	tmp->next_sibling = child;

      sibling->prev_sibling = child;
    }
  else
    child->prev_sibling = NULL;
}

/**
 * gsk_render_node_insert_child_before:
 * @node: a #GskRenderNode
 * @child: a #GskRenderNode
 * @sibling: (nullable): a #GskRenderNode, or %NULL
 *
 * Inserts @child in the list of children of @node, before @sibling.
 *
 * If @sibling is %NULL, the @child will be inserted at the beginning of the
 * list of children.
 *
 * This function acquires a reference of @child.
 *
 * Returns: (transfer none): the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_insert_child_before (GskRenderNode *node,
                                     GskRenderNode *child,
                                     GskRenderNode *sibling)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), node);
  g_return_val_if_fail (sibling == NULL || GSK_IS_RENDER_NODE (sibling), node);
  g_return_val_if_fail (node->is_mutable, node);

  gsk_render_node_insert_child_internal (node, child, insert_child_before, sibling);

  return node;
}

static void
insert_child_after (GskRenderNode *node,
                    GskRenderNode *child,
                    gpointer       user_data)
{
  GskRenderNode *sibling = user_data;

  if (sibling == NULL)
    sibling = node->last_child;

  child->prev_sibling = sibling;

  if (sibling != NULL)
    {
      GskRenderNode *tmp = sibling->next_sibling;

      child->next_sibling = tmp;

      if (tmp != NULL)
	tmp->prev_sibling = child;

      sibling->next_sibling = child;
    }
  else
    child->next_sibling = NULL;
}

/**
 * gsk_render_node_insert_child_after:
 * @node: a #GskRenderNode
 * @child: a #GskRenderNode
 * @sibling: (nullable): a #GskRenderNode, or %NULL
 *
 * Inserts @child in the list of children of @node, after @sibling.
 *
 * If @sibling is %NULL, the @child will be inserted at the end of the list
 * of children.
 *
 * This function acquires a reference of @child.
 *
 * Returns: (transfer none): the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_insert_child_after (GskRenderNode *node,
                                    GskRenderNode *child,
                                    GskRenderNode *sibling)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), node);
  g_return_val_if_fail (sibling == NULL || GSK_IS_RENDER_NODE (sibling), node);
  g_return_val_if_fail (node->is_mutable, node);

  if (sibling != NULL)
    g_return_val_if_fail (sibling->parent == node, node);

  gsk_render_node_insert_child_internal (node, child, insert_child_after, sibling);

  return node;
}

typedef struct {
  GskRenderNode *prev_sibling;
  GskRenderNode *next_sibling;
} InsertBetween;

static void
insert_child_between (GskRenderNode *node,
                      GskRenderNode *child,
                      gpointer       data_)
{
  InsertBetween *data = data_;

  child->prev_sibling = data->prev_sibling;
  child->next_sibling = data->next_sibling;

  if (data->prev_sibling != NULL)
    data->prev_sibling->next_sibling = child;

  if (data->next_sibling != NULL)
    data->next_sibling->prev_sibling = child;
}

/**
 * gsk_render_node_replace_child:
 * @node: a #GskRenderNode
 * @new_child: the #GskRenderNode to add
 * @old_child: the #GskRenderNode to replace
 *
 * Replaces @old_child with @new_child in the list of children of @node.
 *
 * This function acquires a reference to @new_child, and releases a reference
 * of @old_child.
 *
 * Returns: (transfer none): the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_replace_child (GskRenderNode *node,
                               GskRenderNode *new_child,
                               GskRenderNode *old_child)
{
  InsertBetween clos;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (new_child), node);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (old_child), node);

  g_return_val_if_fail (new_child->parent == NULL, node);
  g_return_val_if_fail (old_child->parent == node, node);

  g_return_val_if_fail (node->is_mutable, node);

  clos.prev_sibling = old_child->prev_sibling;
  clos.next_sibling = old_child->next_sibling;
  gsk_render_node_remove_child (node, old_child);

  gsk_render_node_insert_child_internal (node, new_child, insert_child_between, &clos);

  return node;
}

/**
 * gsk_render_node_remove_child:
 * @node: a #GskRenderNode
 * @child: a #GskRenderNode child of @node
 *
 * Removes @child from the list of children of @node.
 *
 * This function releases the reference acquired when adding @child to the
 * list of children.
 *
 * Returns: (transfer none): the #GskRenderNode
 */
GskRenderNode *
gsk_render_node_remove_child (GskRenderNode *node,
                              GskRenderNode *child)
{
  GskRenderNode *prev_sibling, *next_sibling;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), node);
  g_return_val_if_fail (node->is_mutable, node);

  if (child->parent != node)
    {
      g_critical ("The render node of type '%s' is not a child of the render node of type '%s'",
		  G_OBJECT_TYPE_NAME (child),
		  G_OBJECT_TYPE_NAME (node));
      return node;
    }

  prev_sibling = child->prev_sibling;
  next_sibling = child->next_sibling;

  child->parent = NULL;
  child->prev_sibling = NULL;
  child->next_sibling = NULL;
  child->age = 0;

  if (prev_sibling)
    prev_sibling->next_sibling = next_sibling;
  if (next_sibling)
    next_sibling->prev_sibling = prev_sibling;

  node->age += 1;
  node->n_children -= 1;

  if (node->first_child == child)
    node->first_child = next_sibling;
  if (node->last_child == child)
    node->last_child = prev_sibling;

  gsk_render_node_unref (child);

  return node;
}

/**
 * gsk_render_node_remove_all_children:
 * @node: a #GskRenderNode
 *
 * Removes all children of @node.
 *
 * See also: gsk_render_node_remove_child()
 *
 * Returns: (transfer none): the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_render_node_remove_all_children (GskRenderNode *node)
{
  GskRenderNodeIter iter;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);
  g_return_val_if_fail (node->is_mutable, node);

  if (node->n_children == 0)
    return node;

  gsk_render_node_iter_init (&iter, node);
  while (gsk_render_node_iter_next (&iter, NULL))
    gsk_render_node_iter_remove (&iter);

  g_assert (node->n_children == 0);
  g_assert (node->first_child == NULL);
  g_assert (node->last_child == NULL);

  return node;
}

/**
 * gsk_render_node_get_n_children:
 * @node: a #GskRenderNode
 *
 * Retrieves the number of direct children of @node.
 *
 * Returns: the number of children of the #GskRenderNode
 *
 * Since: 3.90
 */
guint
gsk_render_node_get_n_children (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), 0);

  return node->n_children;
}

/**
 * gsk_render_node_set_bounds:
 * @node: a #GskRenderNode
 * @bounds: (nullable): the boundaries of @node
 *
 * Sets the boundaries of @node, which describe the geometry of the
 * render node, and are used to clip the surface associated to it
 * when rendering.
 *
 * Since: 3.90
 */
void
gsk_render_node_set_bounds (GskRenderNode         *node,
                            const graphene_rect_t *bounds)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (node->is_mutable);

  if (bounds == NULL)
    graphene_rect_init_from_rect (&node->bounds, graphene_rect_zero ());
  else
    graphene_rect_init_from_rect (&node->bounds, bounds);
}

/**
 * gsk_render_node_get_bounds:
 * @node: a #GskRenderNode
 * @bounds: (out caller-allocates): return location for the boundaries
 *
 * Retrieves the boundaries set using gsk_render_node_set_bounds().
 *
 * Since: 3.90
 */
void
gsk_render_node_get_bounds (GskRenderNode   *node,
                            graphene_rect_t *bounds)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (bounds != NULL);

  *bounds = node->bounds;
}

/**
 * gsk_render_node_set_transform:
 * @node: a #GskRenderNode
 * @transform: (nullable): a transformation matrix
 *
 * Sets the transformation matrix used when rendering the @node.
 *
 * Since: 3.90
 */
void
gsk_render_node_set_transform (GskRenderNode           *node,
                               const graphene_matrix_t *transform)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (node->is_mutable);

  if (transform == NULL)
    graphene_matrix_init_identity (&node->transform);
  else
    graphene_matrix_init_from_matrix (&node->transform, transform);

  node->transform_set = !graphene_matrix_is_identity (&node->transform);
}

/**
 * gsk_render_node_get_transform:
 * @node: a #GskRenderNode
 * @mv: (out caller-allocates): return location for the transform matrix
 *
 * Retrieves the transform matrix set using gsk_render_node_set_transform().
 *
 * Since: 3.90
 */
void
gsk_render_node_get_transform (GskRenderNode     *node,
                               graphene_matrix_t *mv)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (mv != NULL);

  graphene_matrix_init_from_matrix (mv, &node->transform);
}

/**
 * gsk_render_node_set_anchor_point:
 * @node: a #GskRenderNode
 * @offset: the anchor point
 *
 * Set the anchor point used when rendering the @node.
 *
 * Since: 3.90
 */
void
gsk_render_node_set_anchor_point (GskRenderNode            *node,
                                  const graphene_point3d_t *offset)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (node->is_mutable);

  graphene_point3d_init_from_point (&node->anchor_point, offset);
}

/**
 * gsk_render_node_set_opacity:
 * @node: a #GskRenderNode
 * @opacity: the opacity of the node, between 0 (fully transparent) and
 *   1 (fully opaque)
 *
 * Sets the opacity of the @node.
 *
 * Since: 3.90
 */
void
gsk_render_node_set_opacity (GskRenderNode *node,
                             double         opacity)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (node->is_mutable);

  node->opacity = CLAMP (opacity, 0.0, 1.0);
}

/**
 * gsk_render_node_get_opacity:
 * @node: a #GskRenderNode
 *
 * Retrieves the opacity set using gsk_render_node_set_opacity().
 *
 * Returns: the opacity of the #GskRenderNode
 *
 * Since: 3.90
 */
double
gsk_render_node_get_opacity (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), 0.0);

  return node->opacity;
}

/**
 * gsk_render_node_set_hidden:
 * @node: a #GskRenderNode
 * @hidden: whether the @node should be hidden or not
 *
 * Sets whether the @node should be hidden.
 *
 * Hidden nodes, and their descendants, are not rendered.
 *
 * Since: 3.90
 */
void
gsk_render_node_set_hidden (GskRenderNode *node,
                            gboolean       hidden)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (node->is_mutable);

  node->hidden = !!hidden;
}

/**
 * gsk_render_node_is_hidden:
 * @node: a #GskRenderNode
 *
 * Checks whether a @node is hidden.
 *
 * Returns: %TRUE if the #GskRenderNode is hidden
 *
 * Since: 3.90
 */
gboolean
gsk_render_node_is_hidden (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), TRUE);

  return node->hidden;
}

/**
 * gsk_render_node_set_opaque:
 * @node: a #GskRenderNode
 * @opaque: whether the node is fully opaque or not
 *
 * Sets whether the node is known to be fully opaque.
 *
 * Fully opaque nodes will ignore the opacity set using gsk_render_node_set_opacity(),
 * but if their parent is not opaque they may still be rendered with an opacity.
 *
 * Renderers may use this information to optimize the rendering pipeline.
 *
 * Since: 3.90
 */
void
gsk_render_node_set_opaque (GskRenderNode *node,
                            gboolean       opaque)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (node->is_mutable);

  node->opaque = !!opaque;
}

/**
 * gsk_render_node_is_opaque:
 * @node: a #GskRenderNode
 *
 * Retrieves the value set using gsk_render_node_set_opaque().
 *
 * Returns: %TRUE if the #GskRenderNode is fully opaque
 *
 * Since: 3.90
 */
gboolean
gsk_render_node_is_opaque (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), TRUE);

  return node->opaque;
}

/**
 * gsk_render_node_contains:
 * @node: a #GskRenderNode
 * @descendant: a #GskRenderNode
 *
 * Checks whether @node contains @descendant.
 *
 * Returns: %TRUE if the #GskRenderNode contains the given
 *   descendant
 *
 * Since: 3.90
 */
gboolean
gsk_render_node_contains (GskRenderNode *node,
			  GskRenderNode *descendant)
{
  GskRenderNode *tmp;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), FALSE);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (descendant), FALSE);

  for (tmp = descendant; tmp != NULL; tmp = tmp->parent)
    if (tmp == node)
      return TRUE;

  return FALSE;
}

/*< private >
 * gsk_render_node_get_toplevel:
 * @node: a #GskRenderNode
 *
 * Retrieves the top level #GskRenderNode without a parent.
 *
 * Returns: (transfer none): the top level #GskRenderNode
 */
GskRenderNode *
gsk_render_node_get_toplevel (GskRenderNode *node)
{
  GskRenderNode *parent;

  parent = node->parent;
  if (parent == NULL)
    return node;

  while (parent != NULL)
    {
      if (parent->parent == NULL)
        return parent;

      parent = parent->parent;
    }

  return NULL;
}

/*< private >
 * gsk_render_node_update_world_matrix:
 * @node: a #GskRenderNode
 * @force: %TRUE if the update should be forced
 *
 * Updates the cached world matrix of @node and its children, if needed.
 */
void
gsk_render_node_update_world_matrix (GskRenderNode *node,
                                     gboolean       force)
{
  GskRenderNodeIter iter;
  GskRenderNode *child;

  if (force || node->needs_world_matrix_update)
    {
      GSK_NOTE (RENDER_NODE, g_print ("Updating cached world matrix on node %p [parent=%p, t_set=%s]\n",
                                      node,
                                      node->parent != NULL ? node->parent : 0,
                                      node->transform_set ? "y" : "n"));

      if (node->parent == NULL)
        {
          if (node->transform_set)
            graphene_matrix_init_from_matrix (&node->world_matrix, &node->transform);
          else
            graphene_matrix_init_identity (&node->world_matrix);
        }
      else
        {
          GskRenderNode *parent = node->parent;
          graphene_matrix_t tmp;

          graphene_matrix_init_identity (&tmp);

          if (node->transform_set)
            graphene_matrix_multiply (&tmp, &node->transform, &tmp);

          graphene_matrix_translate (&tmp, &node->anchor_point);

          graphene_matrix_multiply (&tmp, &parent->world_matrix, &node->world_matrix);
        }

      node->needs_world_matrix_update = FALSE;
    }

  gsk_render_node_iter_init (&iter, node);
  while (gsk_render_node_iter_next (&iter, &child))
    gsk_render_node_update_world_matrix (child, TRUE);
}

gboolean
gsk_render_node_has_surface (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), FALSE);

  return node->surface != NULL;
}

gboolean
gsk_render_node_has_texture (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), FALSE);

  return node->texture != NULL;
}

GskTexture *
gsk_render_node_get_texture (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), 0);

  return node->texture;
}

/**
 * gsk_render_node_set_texture:
 * @node: a #GskRenderNode
 * @texture: the #GskTexture
 *
 * Associates a #GskTexture to a #GskRenderNode.
 *
 * Since: 3.90
 */
void
gsk_render_node_set_texture (GskRenderNode *node,
                             GskTexture    *texture)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  if (node->texture == texture)
    return;

  if (node->texture)
    gsk_texture_unref (node->texture);

  node->texture = texture;

  if (texture)
    gsk_texture_ref (texture);
}

/*< private >
 * gsk_render_node_get_surface:
 * @node: a #GskRenderNode
 *
 * Retrieves the surface set using gsk_render_node_set_surface().
 *
 * Returns: (transfer none) (nullable): a Cairo surface
 */
cairo_surface_t *
gsk_render_node_get_surface (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return node->surface;
}

void
gsk_render_node_set_scaling_filters (GskRenderNode    *node,
                                     GskScalingFilter  min_filter,
                                     GskScalingFilter  mag_filter)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  if (node->min_filter != min_filter)
    node->min_filter = min_filter;
  if (node->mag_filter != mag_filter)
    node->mag_filter = mag_filter;
}

/*< private >
 * gsk_render_node_get_world_matrix:
 * @node: a #GskRenderNode
 * @mv: (out caller-allocates): return location for the modelview matrix
 *   in world-relative coordinates
 *
 * Retrieves the modelview matrix in world-relative coordinates.
 */
void
gsk_render_node_get_world_matrix (GskRenderNode     *node,
                                  graphene_matrix_t *mv)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (mv != NULL);

  if (node->needs_world_matrix_update)
    {
      GskRenderNode *tmp = gsk_render_node_get_toplevel (node);

      gsk_render_node_update_world_matrix (tmp, TRUE);

      g_assert (!node->needs_world_matrix_update);
    }

  *mv = node->world_matrix;
}

/**
 * gsk_render_node_set_name:
 * @node: a #GskRenderNode
 * @name: (nullable): a name for the node
 *
 * Sets the name of the node.
 *
 * A name is generally useful for debugging purposes.
 *
 * Since: 3.90
 */
void
gsk_render_node_set_name (GskRenderNode *node,
                          const char    *name)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  g_free (node->name);
  node->name = g_strdup (name);
}

/**
 * gsk_render_node_get_name:
 * @node: a #GskRenderNode
 *
 * Retrieves the name previously set via gsk_render_node_set_name().
 * If no name has been set, %NULL is returned.
 *
 * Returns: (nullable): The name previously set via
 *     gsk_render_node_set_name() or %NULL
 *
 * Since: 3.90
 **/
const char *
gsk_render_node_get_name (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);

  return node->name;
}

/**
 * gsk_render_node_set_blend_mode:
 * @node: a #GskRenderNode
 * @blend_mode: the blend mode to be applied to the node's children
 *
 * Sets the blend mode to be used when rendering the children
 * of the @node.
 *
 * The default value is %GSK_BLEND_MODE_DEFAULT.
 *
 * Since: 3.90
 */
void
gsk_render_node_set_blend_mode (GskRenderNode *node,
                                GskBlendMode   blend_mode)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (node->is_mutable);

  if (node->blend_mode == blend_mode)
    return;

  node->blend_mode = blend_mode;
}

/*
 * gsk_render_node_get_blend_mode:
 * @node: a #GskRenderNode
 *
 * Retrieves the blend mode set by gsk_render_node_set_blend_mode().
 *
 * Returns: the blend mode
 */
GskBlendMode
gsk_render_node_get_blend_mode (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), GSK_BLEND_MODE_DEFAULT);

  return node->blend_mode;
}

/**
 * gsk_render_node_get_draw_context:
 * @node: a #GskRenderNode
 * @renderer: (nullable): Renderer to optimize for or %NULL for any
 *
 * Creates a Cairo context for drawing using the surface associated
 * to the render node.
 * If no surface exists yet, a surface will be created optimized for
 * rendering to @renderer.
 *
 * Returns: (transfer full): a Cairo context used for drawing; use
 *   cairo_destroy() when done drawing
 *
 * Since: 3.90
 */
cairo_t *
gsk_render_node_get_draw_context (GskRenderNode *node,
                                  GskRenderer   *renderer)
{
  cairo_t *res;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), NULL);
  g_return_val_if_fail (node->is_mutable, NULL);
  g_return_val_if_fail (renderer == NULL || GSK_IS_RENDERER (renderer), NULL);

  if (node->surface == NULL)
    {
      if (renderer)
        {
          node->surface = gsk_renderer_create_cairo_surface (renderer,
                                                             node->opaque ? CAIRO_FORMAT_RGB24
                                                                          : CAIRO_FORMAT_ARGB32,
                                                             ceilf (node->bounds.size.width),
                                                             ceilf (node->bounds.size.height));
        }
      else
        {
          node->surface = cairo_image_surface_create (node->opaque ? CAIRO_FORMAT_RGB24
                                                                   : CAIRO_FORMAT_ARGB32,
                                                      ceilf (node->bounds.size.width),
                                                      ceilf (node->bounds.size.height));
        }
    }

  res = cairo_create (node->surface);

  cairo_translate (res, -node->bounds.origin.x, -node->bounds.origin.y);

  cairo_rectangle (res,
                   node->bounds.origin.x, node->bounds.origin.y,
                   node->bounds.size.width, node->bounds.size.height);
  cairo_clip (res);

  if (GSK_DEBUG_CHECK (SURFACE))
    {
      const char *prefix;
      prefix = g_getenv ("GSK_DEBUG_PREFIX");
      if (!prefix || g_str_has_prefix (node->name, prefix))
        {
          cairo_save (res);
          cairo_rectangle (res,
                           node->bounds.origin.x + 1, node->bounds.origin.y + 1,
                           node->bounds.size.width - 2, node->bounds.size.height - 2);
          cairo_set_line_width (res, 2);
          cairo_set_source_rgb (res, 1, 0, 0);
          cairo_stroke (res);
          cairo_restore (res);
        }
    }

  return res;
}

/*< private >
 * gsk_render_node_make_immutable:
 * @node: a #GskRenderNode
 *
 * Marks @node, and all its children, as immutable.
 */
void
gsk_render_node_make_immutable (GskRenderNode *node)
{
  GskRenderNodeIter iter;
  GskRenderNode *child;

  if (!node->is_mutable)
    return;

  node->is_mutable = FALSE;

  gsk_render_node_iter_init (&iter, node);
  while (gsk_render_node_iter_next (&iter, &child))
    gsk_render_node_make_immutable (child);
}

/*< private >
 * gsk_render_node_get_size:
 * @root: a #GskRenderNode
 *
 * Computes the total number of children of @root.
 *
 * Returns: the size of the tree
 */
int
gsk_render_node_get_size (GskRenderNode *root)
{
  GskRenderNodeIter iter;
  GskRenderNode *child;
  int res;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (root), 0);

  res = 1;
  gsk_render_node_iter_init (&iter, root);
  while (gsk_render_node_iter_next (&iter, &child))
    res += gsk_render_node_get_size (child);

  return res;
}

/**
 * gsk_value_set_render_node:
 * @value: a #GValue
 * @node: (nullable): a #GskRenderNode
 *
 * Sets the @node into the @value.
 *
 * This function acquires a reference on @node.
 *
 * Since: 3.90
 */
void
gsk_value_set_render_node (GValue        *value,
			   GskRenderNode *node)
{
  GskRenderNode *old_node;

  g_return_if_fail (GSK_VALUE_HOLDS_RENDER_NODE (value));

  old_node = value->data[0].v_pointer;

  if (node != NULL)
    {
      g_return_if_fail (GSK_IS_RENDER_NODE (node));

      value->data[0].v_pointer = gsk_render_node_ref (node);
    }
  else
    value->data[0].v_pointer = NULL;

  if (old_node != NULL)
    gsk_render_node_unref (old_node);
}

/**
 * gsk_value_take_render_node:
 * @value: a #GValue
 * @node: (transfer full) (nullable): a #GskRenderNode
 *
 * Sets the @node into the @value, without taking a reference to it.
 *
 * Since: 3.90
 */
void
gsk_value_take_render_node (GValue        *value,
			    GskRenderNode *node)
{
  GskRenderNode *old_node;

  g_return_if_fail (GSK_VALUE_HOLDS_RENDER_NODE (value));

  old_node = value->data[0].v_pointer;

  if (node != NULL)
    {
      g_return_if_fail (GSK_IS_RENDER_NODE (node));

      /* take over ownership */
      value->data[0].v_pointer = node;
    }
  else
    value->data[0].v_pointer = NULL;

  if (old_node != NULL)
    gsk_render_node_unref (old_node);
}

/**
 * gsk_value_get_render_node:
 * @value: a #GValue
 *
 * Retrieves the #GskRenderNode stored inside the @value.
 *
 * Returns: (transfer none) (nullable): a #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_value_get_render_node (const GValue *value)
{
  g_return_val_if_fail (GSK_VALUE_HOLDS_RENDER_NODE (value), NULL);

  return value->data[0].v_pointer;
}

/**
 * gsk_value_dup_render_node:
 * @value: a #GValue
 *
 * Retrieves the #GskRenderNode stored inside the @value, and
 * acquires a reference to it.
 *
 * Returns: (transfer none) (nullable): a #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNode *
gsk_value_dup_render_node (const GValue *value)
{
  g_return_val_if_fail (GSK_VALUE_HOLDS_RENDER_NODE (value), NULL);

  if (value->data[0].v_pointer != NULL)
    return gsk_render_node_ref (value->data[0].v_pointer);

  return NULL;
}
