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

#include "gskblendnode.h"

#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"
#include "gskcontainernode.h"

#include "gdk/gdkcairoprivate.h"

/**
 * GskBlendNode:
 *
 * A render node applying a blending function between its two child nodes.
 */
struct _GskBlendNode
{
  GskRenderNode render_node;

  GskRenderNode *bottom;
  GskRenderNode *top;
  GskBlendMode blend_mode;
};

static cairo_operator_t
gsk_blend_mode_to_cairo_operator (GskBlendMode blend_mode)
{
  switch (blend_mode)
    {
    default:
      g_assert_not_reached ();
    case GSK_BLEND_MODE_DEFAULT:
      return CAIRO_OPERATOR_OVER;
    case GSK_BLEND_MODE_MULTIPLY:
      return CAIRO_OPERATOR_MULTIPLY;
    case GSK_BLEND_MODE_SCREEN:
      return CAIRO_OPERATOR_SCREEN;
    case GSK_BLEND_MODE_OVERLAY:
      return CAIRO_OPERATOR_OVERLAY;
    case GSK_BLEND_MODE_DARKEN:
      return CAIRO_OPERATOR_DARKEN;
    case GSK_BLEND_MODE_LIGHTEN:
      return CAIRO_OPERATOR_LIGHTEN;
    case GSK_BLEND_MODE_COLOR_DODGE:
      return CAIRO_OPERATOR_COLOR_DODGE;
    case GSK_BLEND_MODE_COLOR_BURN:
      return CAIRO_OPERATOR_COLOR_BURN;
    case GSK_BLEND_MODE_HARD_LIGHT:
      return CAIRO_OPERATOR_HARD_LIGHT;
    case GSK_BLEND_MODE_SOFT_LIGHT:
      return CAIRO_OPERATOR_SOFT_LIGHT;
    case GSK_BLEND_MODE_DIFFERENCE:
      return CAIRO_OPERATOR_DIFFERENCE;
    case GSK_BLEND_MODE_EXCLUSION:
      return CAIRO_OPERATOR_EXCLUSION;
    case GSK_BLEND_MODE_COLOR:
      return CAIRO_OPERATOR_HSL_COLOR;
    case GSK_BLEND_MODE_HUE:
      return CAIRO_OPERATOR_HSL_HUE;
    case GSK_BLEND_MODE_SATURATION:
      return CAIRO_OPERATOR_HSL_SATURATION;
    case GSK_BLEND_MODE_LUMINOSITY:
      return CAIRO_OPERATOR_HSL_LUMINOSITY;
    }
}

static void
gsk_blend_node_finalize (GskRenderNode *node)
{
  GskBlendNode *self = (GskBlendNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_BLEND_NODE));

  gsk_render_node_unref (self->bottom);
  gsk_render_node_unref (self->top);

  parent_class->finalize (node);
}

static void
gsk_blend_node_draw (GskRenderNode *node,
                     cairo_t       *cr,
                     GdkColorState *ccs)
{
  GskBlendNode *self = (GskBlendNode *) node;

  if (gdk_cairo_is_all_clipped (cr))
    return;

  if (!gdk_color_state_equal (ccs, GDK_COLOR_STATE_SRGB))
    g_warning ("blend node in non-srgb colorstate isn't implemented yet.");

  cairo_push_group (cr);
  gsk_render_node_draw_ccs (self->bottom, cr, ccs);

  cairo_push_group (cr);
  gsk_render_node_draw_ccs (self->top, cr, ccs);

  cairo_pop_group_to_source (cr);
  cairo_set_operator (cr, gsk_blend_mode_to_cairo_operator (self->blend_mode));
  cairo_paint (cr);

  cairo_pop_group_to_source (cr); /* resets operator */
  cairo_paint (cr);
}

static void
gsk_blend_node_diff (GskRenderNode *node1,
                     GskRenderNode *node2,
                     GskDiffData   *data)
{
  GskBlendNode *self1 = (GskBlendNode *) node1;
  GskBlendNode *self2 = (GskBlendNode *) node2;

  if (self1->blend_mode == self2->blend_mode)
    {
      gsk_render_node_diff (self1->top, self2->top, data);
      gsk_render_node_diff (self1->bottom, self2->bottom, data);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

static GskRenderNode *
gsk_blend_node_replay (GskRenderNode   *node,
                       GskRenderReplay *replay)
{
  GskBlendNode *self = (GskBlendNode *) node;
  GskRenderNode *result, *top, *bottom;

  top = gsk_render_replay_filter_node (replay, self->top);
  bottom = gsk_render_replay_filter_node (replay, self->bottom);

  if (top == NULL)
    {
      if (bottom == NULL)
        return NULL;

      top = gsk_container_node_new (NULL, 0);
    }
  else if (bottom == NULL)
    {
      bottom = gsk_container_node_new (NULL, 0);
    }

  if (top == self->top && bottom == self->bottom)
    result = gsk_render_node_ref (node);
  else
    result = gsk_blend_node_new (bottom, top, self->blend_mode);

  gsk_render_node_unref (top);
  gsk_render_node_unref (bottom);

  return result;
}

static void
gsk_blend_node_class_init (gpointer g_class,
                           gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_BLEND_NODE;

  node_class->finalize = gsk_blend_node_finalize;
  node_class->draw = gsk_blend_node_draw;
  node_class->diff = gsk_blend_node_diff;
  node_class->replay = gsk_blend_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskBlendNode, gsk_blend_node)

/**
 * gsk_blend_node_new:
 * @bottom: The bottom node to be drawn
 * @top: The node to be blended onto the @bottom node
 * @blend_mode: The blend mode to use
 *
 * Creates a `GskRenderNode` that will use @blend_mode to blend the @top
 * node onto the @bottom node.
 *
 * Returns: (transfer full) (type GskBlendNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_blend_node_new (GskRenderNode *bottom,
                    GskRenderNode *top,
                    GskBlendMode   blend_mode)
{
  GskBlendNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (bottom), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (top), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_BLEND_NODE);
  node = (GskRenderNode *) self;

  self->bottom = gsk_render_node_ref (bottom);
  self->top = gsk_render_node_ref (top);
  self->blend_mode = blend_mode;

  graphene_rect_union (&bottom->bounds, &top->bounds, &node->bounds);

  node->preferred_depth = gdk_memory_depth_merge (gsk_render_node_get_preferred_depth (bottom),
                                                  gsk_render_node_get_preferred_depth (top));
  node->is_hdr = gsk_render_node_is_hdr (bottom) ||
                 gsk_render_node_is_hdr (top);

  return node;
}

/**
 * gsk_blend_node_get_bottom_child:
 * @node: (type GskBlendNode): a blending `GskRenderNode`
 *
 * Retrieves the bottom `GskRenderNode` child of the @node.
 *
 * Returns: (transfer none): the bottom child node
 */
GskRenderNode *
gsk_blend_node_get_bottom_child (const GskRenderNode *node)
{
  const GskBlendNode *self = (const GskBlendNode *) node;

  return self->bottom;
}

/**
 * gsk_blend_node_get_top_child:
 * @node: (type GskBlendNode): a blending `GskRenderNode`
 *
 * Retrieves the top `GskRenderNode` child of the @node.
 *
 * Returns: (transfer none): the top child node
 */
GskRenderNode *
gsk_blend_node_get_top_child (const GskRenderNode *node)
{
  const GskBlendNode *self = (const GskBlendNode *) node;

  return self->top;
}

/**
 * gsk_blend_node_get_blend_mode:
 * @node: (type GskBlendNode): a blending `GskRenderNode`
 *
 * Retrieves the blend mode used by @node.
 *
 * Returns: the blend mode
 */
GskBlendMode
gsk_blend_node_get_blend_mode (const GskRenderNode *node)
{
  const GskBlendNode *self = (const GskBlendNode *) node;

  return self->blend_mode;
}
