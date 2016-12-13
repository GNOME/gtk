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

static void
gsk_render_node_finalize (GskRenderNode *self)
{
  self->is_mutable = TRUE;

  self->node_class->finalize (self);

  g_clear_pointer (&self->name, g_free);

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

  self->opacity = 1.0;

  self->min_filter = GSK_SCALING_FILTER_NEAREST;
  self->mag_filter = GSK_SCALING_FILTER_NEAREST;

  self->is_mutable = TRUE;

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
 * gsk_render_node_get_bounds:
 * @node: a #GskRenderNode
 * @bounds: (out caller-allocates): return location for the boundaries
 *
 * Retrieves the boundaries of the @node. The node will not draw outside
 * of its boundaries.
 *
 * Since: 3.90
 */
void
gsk_render_node_get_bounds (GskRenderNode   *node,
                            graphene_rect_t *bounds)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (bounds != NULL);

  node->node_class->get_bounds (node, bounds);
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
  if (!node->is_mutable)
    return;

  node->node_class->make_immutable (node);

  node->is_mutable = FALSE;
}

/**
 * gsk_render_node_draw:
 * @node: a #GskRenderNode
 * @cr: cairo context to draw to
 *
 * Draw the contents of @node to the given cairo context.
 *
 * Typically, you'll use this function to implement fallback rendering
 * of #GskRenderNodes on an intermediate Cairo context, instead of using
 * the drawing context associated to a #GdkWindow's rendering buffer.
 *
 * For advanced nodes that cannot be supported using Cairo, in particular
 * for nodes doing 3D operations, this function may fail.
 **/
void
gsk_render_node_draw (GskRenderNode *node,
                      cairo_t       *cr)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (cr != NULL);

  cairo_save (cr);

  if (!GSK_RENDER_MODE_CHECK (GEOMETRY))
    {
      graphene_rect_t frame;

      gsk_render_node_get_bounds (node, &frame);
      GSK_NOTE (CAIRO, g_print ("CLIP = { .x = %g, .y = %g, .width = %g, .height = %g }\n",
                                frame.origin.x, frame.origin.y,
                                frame.size.width, frame.size.height));

      cairo_rectangle (cr, frame.origin.x, frame.origin.y, frame.size.width, frame.size.height);
      cairo_clip (cr);
    }

  GSK_NOTE (CAIRO, g_print ("Rendering node %s[%p]\n",
                            node->name,
                            node));

  node->node_class->draw (node, cr);

  if (GSK_RENDER_MODE_CHECK (GEOMETRY))
    {
      graphene_rect_t frame;

      gsk_render_node_get_bounds (node, &frame);

      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_rectangle (cr, frame.origin.x - 1, frame.origin.y - 1, frame.size.width + 2, frame.size.height + 2);
      cairo_set_line_width (cr, 2);
      cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
      cairo_stroke (cr);
    }

  cairo_restore (cr);
}


