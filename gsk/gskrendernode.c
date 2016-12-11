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
#include "gskrendererprivate.h"
#include "gsktexture.h"

#include <graphene-gobject.h>

#include <math.h>

#include <gobject/gvaluecollector.h>

/**
 * GskRenderNode: (ref-func gsk_render_node_ref) (unref-func gsk_render_node_unref)
 *
 * The `GskRenderNode` structure contains only private data.
 *
 * Since: 3.90
 */

G_DEFINE_BOXED_TYPE (GskRenderNode, gsk_render_node,
                     gsk_render_node_ref,
                     gsk_render_node_unref)

static GskRenderNode *
gsk_render_node_remove_child (GskRenderNode *node,
                              GskRenderNode *child);

static void
gsk_render_node_finalize (GskRenderNode *self)
{
  self->is_mutable = TRUE;

  self->node_class->finalize (self);

  g_clear_pointer (&self->name, g_free);

  while (self->first_child)
    gsk_render_node_remove_child (self, self->first_child);

  g_slice_free1 (self->node_class->struct_size, self);
}

/*< private >
 * gsk_render_node_new:
 * @node_class: class structure for this node
 *
 * Returns: (transfer full): the newly created #GskRenderNode
 */
GskRenderNode *
gsk_render_node_new (const GskRenderNodeClass *node_class)
{
  GskRenderNode *self;
  
  g_return_val_if_fail (node_class != NULL, NULL);
  g_return_val_if_fail (node_class->node_type != GSK_NOT_A_RENDER_NODE, NULL);

  self = g_slice_alloc0 (node_class->struct_size);

  self->node_class = node_class;

  self->ref_count = 1;

  graphene_rect_init_from_rect (&self->bounds, graphene_rect_zero ());

  graphene_matrix_init_identity (&self->transform);

  self->opacity = 1.0;

  self->min_filter = GSK_SCALING_FILTER_NEAREST;
  self->mag_filter = GSK_SCALING_FILTER_NEAREST;

  self->is_mutable = TRUE;
  self->needs_world_matrix_update = TRUE;

  return self;
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
    gsk_render_node_finalize (node);
}

/**
 * gsk_render_node_get_node_type:
 * @node: a #GskRenderNode
 *
 * Returns the type of the @node.
 *
 * Returns: the type of the #GskRenderNode
 *
 * Since: 3.90
 */
GskRenderNodeType
gsk_render_node_get_node_type (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), GSK_NOT_A_RENDER_NODE);

  return node->node_class->node_type;
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
  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CONTAINER_NODE), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), node);
  g_return_val_if_fail (node->is_mutable, node);

  gsk_render_node_insert_child_internal (node, child,
					 insert_child_at_pos,
					 GINT_TO_POINTER (node->n_children));

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
static GskRenderNode *
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
  g_return_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CONTAINER_NODE));
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

          graphene_matrix_multiply (&tmp, &parent->world_matrix, &node->world_matrix);
        }

      node->needs_world_matrix_update = FALSE;
    }

  for (child = gsk_render_node_get_first_child (node);
       child != NULL;
       child = gsk_render_node_get_next_sibling (child))
    {
      gsk_render_node_update_world_matrix (child, TRUE);
    }
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

/*< private >
 * gsk_render_node_make_immutable:
 * @node: a #GskRenderNode
 *
 * Marks @node, and all its children, as immutable.
 */
void
gsk_render_node_make_immutable (GskRenderNode *node)
{
  GskRenderNode *child;

  if (!node->is_mutable)
    return;

  node->is_mutable = FALSE;

  for (child = gsk_render_node_get_first_child (node);
       child != NULL;
       child = gsk_render_node_get_next_sibling (child))
    {
      gsk_render_node_make_immutable (child);
    }
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
  GskRenderNode *child;
  int res;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (root), 0);

  res = 1;
  for (child = gsk_render_node_get_first_child (root);
       child != NULL;
       child = gsk_render_node_get_next_sibling (child))
    {
      res += gsk_render_node_get_size (child);
    }

  return res;
}

