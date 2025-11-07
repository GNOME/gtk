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

#include "config.h"

#include "gskcolornodeprivate.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"

#include "gdk/gdkcairoprivate.h"
#include "gdk/gdkcolorprivate.h"

/**
 * GskColorNode:
 *
 * A render node for a solid color.
 */
struct _GskColorNode
{
  GskRenderNode render_node;
  GdkColor color;
};

static void
gsk_color_node_finalize (GskRenderNode *node)
{
  GskColorNode *self = (GskColorNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_COLOR_NODE));

  gdk_color_finish (&self->color);

  parent_class->finalize (node);
}

static void
gsk_color_node_draw (GskRenderNode *node,
                     cairo_t       *cr,
                     GdkColorState *ccs)
{
  GskColorNode *self = (GskColorNode *) node;

  gdk_cairo_set_source_color (cr, ccs, &self->color);
}

static void
gsk_color_node_diff (GskRenderNode *node1,
                     GskRenderNode *node2,
                     GskDiffData   *data)
{
  GskColorNode *self1 = (GskColorNode *) node1;
  GskColorNode *self2 = (GskColorNode *) node2;

  if (gsk_rect_equal (&node1->bounds, &node2->bounds) &&
      gdk_color_equal (&self1->color, &self2->color))
    return;

  gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode *
gsk_color_node_replay (GskRenderNode   *node,
                       GskRenderReplay *replay)
{
  return gsk_render_node_ref (node);
}

static void
gsk_color_node_class_init (gpointer g_class,
                           gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_COLOR_NODE;

  node_class->finalize = gsk_color_node_finalize;
  node_class->draw = gsk_color_node_draw;
  node_class->diff = gsk_color_node_diff;
  node_class->replay = gsk_color_node_replay;
}

GSK_DEFINE_NODE_TYPE (GskColorNode, gsk_color_node)

/**
 * gsk_color_node_get_color:
 * @node: (type GskColorNode): a `GskRenderNode`
 *
 * Retrieves the color of the given @node.
 *
 * The value returned by this function will not be correct
 * if the render node was created for a non-sRGB color.
 *
 * Returns: (transfer none): the color of the node
 */
const GdkRGBA *
gsk_color_node_get_color (const GskRenderNode *node)
{
  GskColorNode *self = (GskColorNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_COLOR_NODE), NULL);

  /* NOTE: This is only correct for nodes with sRGB colors */
  return (const GdkRGBA *) &self->color.values;
}

/*< private >
 * gsk_color_node_get_gdk_color:
 * @node: (type GskColorNode): a `GskRenderNode`
 *
 * Retrieves the color of the given @node.
 *
 * Returns: (transfer none): the color of the node
 */
const GdkColor *
gsk_color_node_get_gdk_color (const GskRenderNode *node)
{
  GskColorNode *self = (GskColorNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_COLOR_NODE), NULL);

  return &self->color;
}

/**
 * gsk_color_node_new:
 * @rgba: a `GdkRGBA` specifying a color
 * @bounds: the rectangle to render the color into
 *
 * Creates a `GskRenderNode` that will render the color specified by @rgba into
 * the area given by @bounds.
 *
 * Returns: (transfer full) (type GskColorNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_color_node_new (const GdkRGBA         *rgba,
                    const graphene_rect_t *bounds)
{
  GdkColor color;
  GskRenderNode *node;

  gdk_color_init_from_rgba (&color, rgba);
  node = gsk_color_node_new2 (&color, bounds);
  gdk_color_finish (&color);

  return node;
}

/*< private >
 * gsk_color_node_new2:
 * @color: a `GdkColor` specifying a color
 * @bounds: the rectangle to render the color into
 *
 * Creates a `GskRenderNode` that will render the color specified by @color
 * into the area given by @bounds.
 *
 * Returns: (transfer full) (type GskColorNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_color_node_new2 (const GdkColor        *color,
                     const graphene_rect_t *bounds)
{
  GskColorNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (color != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  self = gsk_render_node_alloc (GSK_COLOR_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = gdk_color_is_opaque (color);
  node->preferred_depth = GDK_MEMORY_NONE;
  node->is_hdr = !gdk_color_is_srgb (color);

  gdk_color_init_copy (&self->color, color);

  gsk_rect_init_from_rect (&node->bounds, bounds);
  gsk_rect_normalize (&node->bounds);

  return node;
}

