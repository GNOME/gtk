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
 * the rectangle set using gsk_render_node_set_bounds().
 *
 * Render nodes are meant to be transient; once they have been associated
 * to a #GskRenderer it's safe to release any reference you have on them.
 * All #GskRenderNodes are immutable, you can only specify their properties
 * during construction.
 */

#include "config.h"

#include "gskrendernodeprivate.h"

#include "gskdebugprivate.h"
#include "gskrendererprivate.h"
#include "gskrendernodeparserprivate.h"

#include <graphene-gobject.h>

#include <math.h>

#include <gobject/gvaluecollector.h>

/**
 * GskRenderNode: (ref-func gsk_render_node_ref) (unref-func gsk_render_node_unref)
 *
 * The `GskRenderNode` structure contains only private data.
 */

G_DEFINE_BOXED_TYPE (GskRenderNode, gsk_render_node,
                     gsk_render_node_ref,
                     gsk_render_node_unref)

G_DEFINE_QUARK (gsk-serialization-error-quark, gsk_serialization_error)

static void
gsk_render_node_finalize (GskRenderNode *self)
{
  self->node_class->finalize (self);

  g_free (self);
}

/*< private >
 * gsk_render_node_new:
 * @node_class: class structure for this node
 *
 * Returns: (transfer full): the newly created #GskRenderNode
 */
GskRenderNode *
gsk_render_node_new (const GskRenderNodeClass *node_class, gsize extra_size)
{
  GskRenderNode *self;

  g_return_val_if_fail (node_class != NULL, NULL);
  g_return_val_if_fail (node_class->node_type != GSK_NOT_A_RENDER_NODE, NULL);

  self = g_malloc0 (node_class->struct_size + extra_size);

  self->node_class = node_class;

  self->ref_count = 1;

  return self;
}

/**
 * gsk_render_node_ref:
 * @node: a #GskRenderNode
 *
 * Acquires a reference on the given #GskRenderNode.
 *
 * Returns: (transfer full): the #GskRenderNode with an additional reference
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
 * @node: (transfer full): a #GskRenderNode
 *
 * Releases a reference on the given #GskRenderNode.
 *
 * If the reference was the last, the resources associated to the @node are
 * freed.
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
 */
GskRenderNodeType
gsk_render_node_get_node_type (GskRenderNode *node)
{
  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), GSK_NOT_A_RENDER_NODE);

  return node->node_class->node_type;
}

static inline
GskRenderNodeType
_gsk_render_node_get_node_type (GskRenderNode *node)
{
  return node->node_class->node_type;
}

/**
 * gsk_render_node_get_bounds:
 * @node: a #GskRenderNode
 * @bounds: (out caller-allocates): return location for the boundaries
 *
 * Retrieves the boundaries of the @node. The node will not draw outside
 * of its boundaries.
 */
void
gsk_render_node_get_bounds (GskRenderNode   *node,
                            graphene_rect_t *bounds)
{
  g_return_if_fail (GSK_IS_RENDER_NODE (node));
  g_return_if_fail (bounds != NULL);

  graphene_rect_init_from_rect (bounds, &node->bounds);
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
 * the drawing context associated to a #GdkSurface's rendering buffer.
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
  g_return_if_fail (cairo_status (cr) == CAIRO_STATUS_SUCCESS);

  cairo_save (cr);

#ifdef G_ENABLE_DEBUG
  if (!GSK_DEBUG_CHECK (GEOMETRY))
    {
      GSK_NOTE (CAIRO, g_message ("CLIP = { .x = %g, .y = %g, .width = %g, .height = %g }",
                                node->bounds.origin.x, node->bounds.origin.y,
                                node->bounds.size.width, node->bounds.size.height));

      cairo_rectangle (cr, node->bounds.origin.x, node->bounds.origin.y, node->bounds.size.width, node->bounds.size.height);
      cairo_clip (cr);
    }
#endif

  GSK_NOTE (CAIRO, g_message ("Rendering node %s[%p]",
                            node->node_class->type_name, node));

  node->node_class->draw (node, cr);

#ifdef G_ENABLE_DEBUG
  if (GSK_DEBUG_CHECK (GEOMETRY))
    {
      cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
      cairo_rectangle (cr, node->bounds.origin.x - 1, node->bounds.origin.y - 1,
                       node->bounds.size.width + 2, node->bounds.size.height + 2);
      cairo_set_line_width (cr, 2);
      cairo_set_source_rgba (cr, 0, 0, 0, 0.5);
      cairo_stroke (cr);
    }
#endif

  cairo_restore (cr);

  if (cairo_status (cr))
    {
      g_warning ("drawing failure for render node %s: %s",
                 node->node_class->type_name,
                 cairo_status_to_string (cairo_status (cr)));
    }
}

/*
 * gsk_render_node_can_diff:
 * @node1: a #GskRenderNode
 * @node2: the #GskRenderNode to compare with
 *
 * Checks if two render nodes can be expected to be compared via
 * gsk_render_node_diff(). The node diffing algorithm uses this function
 * to match up similar nodes to compare when trying to minimize the
 * resulting region.
 *
 * Nodes of different type always return %FALSE here.
 *
 * Returns: %TRUE if @node1 and @node2 can be expected to be compared
 **/
gboolean
gsk_render_node_can_diff (GskRenderNode *node1,
                          GskRenderNode *node2)
{
  if (node1 == node2)
    return TRUE;

  if (_gsk_render_node_get_node_type (node1) != _gsk_render_node_get_node_type (node2))
    return FALSE;

  return node1->node_class->can_diff (node1, node2);
}

static void
rectangle_init_from_graphene (cairo_rectangle_int_t *cairo,
                              const graphene_rect_t *graphene)
{
  cairo->x = floorf (graphene->origin.x);
  cairo->y = floorf (graphene->origin.y);
  cairo->width = ceilf (graphene->origin.x + graphene->size.width) - cairo->x;
  cairo->height = ceilf (graphene->origin.y + graphene->size.height) - cairo->y;
}

void
gsk_render_node_diff_impossible (GskRenderNode  *node1,
                                 GskRenderNode  *node2,
                                 cairo_region_t *region)
{
  cairo_rectangle_int_t rect;

  rectangle_init_from_graphene (&rect, &node1->bounds);
  cairo_region_union_rectangle (region, &rect);
  rectangle_init_from_graphene (&rect, &node2->bounds);
  cairo_region_union_rectangle (region, &rect);
}

/**
 * gsk_render_node_diff:
 * @node1: a #GskRenderNode
 * @node2: the #GskRenderNode to compare with
 * @region: a #cairo_region_t to add the differences to
 *
 * Compares @node1 and @node2 trying to compute the minimal region of changes.
 * In the worst case, this is the union of the bounds of @node1 and @node2.
 *
 * This function is used to compute the area that needs to be redrawn when
 * the previous contents were drawn by @node1 and the new contents should
 * correspond to @node2. As such, it is important that this comparison is
 * faster than the time it takes to actually do the redraw.
 *
 * Note that the passed in @region may already contain previous results from
 * previous node comparisons, so this function call will only add to it.
 **/
void
gsk_render_node_diff (GskRenderNode  *node1,
                      GskRenderNode  *node2,
                      cairo_region_t *region)
{
  if (node1 == node2)
    return;

  if (_gsk_render_node_get_node_type (node1) != _gsk_render_node_get_node_type (node2))
    return gsk_render_node_diff_impossible (node1, node2, region);

  return node1->node_class->diff (node1, node2, region);
}

#define GSK_RENDER_NODE_SERIALIZATION_VERSION 0
#define GSK_RENDER_NODE_SERIALIZATION_ID "GskRenderNode"

/**
 * gsk_render_node_write_to_file:
 * @node: a #GskRenderNode
 * @filename: the file to save it to.
 * @error: Return location for a potential error
 *
 * This function is equivalent to calling gsk_render_node_serialize()
 * followed by g_file_set_contents(). See those two functions for details
 * on the arguments.
 *
 * It is mostly intended for use inside a debugger to quickly dump a render
 * node to a file for later inspection.
 *
 * Returns: %TRUE if saving was successful
 **/
gboolean
gsk_render_node_write_to_file (GskRenderNode *node,
                               const char    *filename,
                               GError       **error)
{
  GBytes *bytes;
  gboolean result;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (node), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  bytes = gsk_render_node_serialize (node);
  result = g_file_set_contents (filename,
                                g_bytes_get_data (bytes, NULL),
                                g_bytes_get_size (bytes),
                                error);
  g_bytes_unref (bytes);

  return result;
}

/**
 * gsk_render_node_deserialize:
 * @bytes: the bytes containing the data
 * @error_func: (nullable) (scope call): Callback on parsing errors or %NULL
 * @user_data: (closure error_func): user_data for @error_func
 *
 * Loads data previously created via gsk_render_node_serialize(). For a
 * discussion of the supported format, see that function.
 *
 * Returns: (nullable) (transfer full): a new #GskRenderNode or %NULL on
 *     error.
 **/
GskRenderNode *
gsk_render_node_deserialize (GBytes            *bytes,
                             GskParseErrorFunc  error_func,
                             gpointer           user_data)
{
  GskRenderNode *node = NULL;

  node = gsk_render_node_deserialize_from_bytes (bytes, error_func, user_data);

  return node;
}
