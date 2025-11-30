/* GSK - The GTK Scene Kit
 *
 * Copyright 2025  Benjamin Otte
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

#include "gskcompositenode.h"

#include "gskcontainernodeprivate.h"
#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

#include "gdk/gdkcairoprivate.h"

/**
 * GskCompositeNode:
 *
 * A render node that uses Porter/Duff compositing operators to combine
 * its child with the background.
 *
 * Since: 4.22
 */
struct _GskCompositeNode
{
  GskRenderNode render_node;

  union {
    GskRenderNode *children[2];
    struct {
      GskRenderNode *child;
      GskRenderNode *mask;
    };
  };
  GskPorterDuff op;
};

static gboolean
gsk_porter_duff_is_bound_by_source (GskPorterDuff porter_duff)
{
  switch (porter_duff)
    {
      case GSK_PORTER_DUFF_DEST: /* this is a no-op */
      case GSK_PORTER_DUFF_SOURCE_OVER_DEST:
      case GSK_PORTER_DUFF_DEST_OVER_SOURCE:
      case GSK_PORTER_DUFF_DEST_OUT_SOURCE:
      case GSK_PORTER_DUFF_SOURCE_ATOP_DEST:
      case GSK_PORTER_DUFF_XOR:
        return TRUE;

      case GSK_PORTER_DUFF_SOURCE:
      case GSK_PORTER_DUFF_SOURCE_IN_DEST:
      case GSK_PORTER_DUFF_DEST_IN_SOURCE:
      case GSK_PORTER_DUFF_SOURCE_OUT_DEST:
      case GSK_PORTER_DUFF_DEST_ATOP_SOURCE:
      case GSK_PORTER_DUFF_CLEAR:
        return FALSE;

      default:
        g_return_val_if_reached (TRUE);
    }
}

static cairo_operator_t
gsk_porter_duff_to_cairo_operator (GskPorterDuff porter_duff)
{
  switch (porter_duff)
    {
      case GSK_PORTER_DUFF_SOURCE:
        return CAIRO_OPERATOR_SOURCE;
      case GSK_PORTER_DUFF_DEST:
        return CAIRO_OPERATOR_DEST;
      case GSK_PORTER_DUFF_SOURCE_OVER_DEST:
        return CAIRO_OPERATOR_OVER;
      case GSK_PORTER_DUFF_DEST_OVER_SOURCE:
        return CAIRO_OPERATOR_DEST_OVER;
      case GSK_PORTER_DUFF_SOURCE_IN_DEST:
        return CAIRO_OPERATOR_IN;
      case GSK_PORTER_DUFF_DEST_IN_SOURCE:
        return CAIRO_OPERATOR_DEST_IN;
      case GSK_PORTER_DUFF_SOURCE_OUT_DEST:
        return CAIRO_OPERATOR_OUT;
      case GSK_PORTER_DUFF_DEST_OUT_SOURCE:
        return CAIRO_OPERATOR_DEST_OUT;
      case GSK_PORTER_DUFF_SOURCE_ATOP_DEST:
        return CAIRO_OPERATOR_ATOP;
      case GSK_PORTER_DUFF_DEST_ATOP_SOURCE:
        return CAIRO_OPERATOR_DEST_ATOP;
      case GSK_PORTER_DUFF_XOR:
        return CAIRO_OPERATOR_XOR;
      case GSK_PORTER_DUFF_CLEAR:
        return CAIRO_OPERATOR_CLEAR;
      default:
        g_return_val_if_reached (CAIRO_OPERATOR_OVER);
    }
}

static cairo_operator_t
gsk_porter_duff_clears_background (GskPorterDuff porter_duff)
{
  switch (porter_duff)
    {
      case GSK_PORTER_DUFF_SOURCE:
      case GSK_PORTER_DUFF_DEST_OVER_SOURCE:
      case GSK_PORTER_DUFF_SOURCE_IN_DEST:
      case GSK_PORTER_DUFF_DEST_IN_SOURCE:
      case GSK_PORTER_DUFF_SOURCE_OUT_DEST:
      case GSK_PORTER_DUFF_DEST_OUT_SOURCE:
      case GSK_PORTER_DUFF_SOURCE_ATOP_DEST:
      case GSK_PORTER_DUFF_DEST_ATOP_SOURCE:
      case GSK_PORTER_DUFF_XOR:
      case GSK_PORTER_DUFF_CLEAR:
        return TRUE;

      case GSK_PORTER_DUFF_DEST:
      case GSK_PORTER_DUFF_SOURCE_OVER_DEST:
        return FALSE;

      default:
        g_return_val_if_reached (TRUE);
    }
}

static cairo_operator_t
gsk_porter_duff_clears_foreground (GskPorterDuff porter_duff)
{
  switch (porter_duff)
    {
      case GSK_PORTER_DUFF_DEST:
      case GSK_PORTER_DUFF_SOURCE_IN_DEST:
      case GSK_PORTER_DUFF_SOURCE_OUT_DEST:
      case GSK_PORTER_DUFF_DEST_OUT_SOURCE:
      case GSK_PORTER_DUFF_SOURCE_ATOP_DEST:
      case GSK_PORTER_DUFF_XOR:
      case GSK_PORTER_DUFF_CLEAR:
        return TRUE;

      case GSK_PORTER_DUFF_SOURCE:
      case GSK_PORTER_DUFF_SOURCE_OVER_DEST:
      case GSK_PORTER_DUFF_DEST_OVER_SOURCE:
      case GSK_PORTER_DUFF_DEST_IN_SOURCE:
      case GSK_PORTER_DUFF_DEST_ATOP_SOURCE:
        return FALSE;

      default:
        g_return_val_if_reached (TRUE);
    }
}

static void
gsk_composite_node_finalize (GskRenderNode *node)
{
  GskCompositeNode *self = (GskCompositeNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_COMPOSITE_NODE));

  gsk_render_node_unref (self->child);
  gsk_render_node_unref (self->mask);

  parent_class->finalize (node);
}

static void
gsk_composite_node_draw (GskRenderNode *node,
                         cairo_t       *cr,
                         GskCairoData  *data)
{
  GskCompositeNode *self = (GskCompositeNode *) node;

  /* clip so the push_group() creates a smaller surface */
  gdk_cairo_rectangle_snap_to_grid (cr, &node->bounds);
  cairo_clip (cr);
  if (gdk_cairo_is_all_clipped (cr))
    return;

  if (gsk_render_node_is_fully_opaque (self->mask))
    {
      gdk_cairo_rect (cr, &self->mask->bounds);
      cairo_clip (cr);
      cairo_push_group (cr);
      gsk_render_node_draw_full (self->child, cr, data);
      cairo_pop_group_to_source (cr);
      cairo_set_operator (cr, gsk_porter_duff_to_cairo_operator (self->op));
      cairo_paint (cr);
    }
  else
    {
      cairo_surface_t *bg = cairo_get_group_target (cr);
      cairo_pattern_t *mask_pattern;

      /* First, copy the current target contents into a new offscreen */
      cairo_push_group (cr);
      cairo_save (cr);
      cairo_identity_matrix (cr);
      cairo_set_source_surface (cr, bg, 0, 0);
      cairo_paint (cr);
      cairo_restore (cr);

      /* Then, draw the child into the offscreen as if no mask existed */
      cairo_push_group (cr);
      gsk_render_node_draw_full (self->child, cr, data);
      cairo_pop_group_to_source (cr);
      cairo_set_operator (cr, gsk_porter_duff_to_cairo_operator (self->op));
      cairo_paint (cr);

      /* Next, clear according to the mask */
      cairo_pop_group_to_source (cr);
      cairo_push_group (cr);
      gsk_render_node_draw_full (self->mask, cr, data);
      mask_pattern = cairo_pop_group (cr);
      cairo_set_operator (cr, CAIRO_OPERATOR_CLEAR);
      cairo_mask (cr, mask_pattern);

      /* Finally, add the offscreen using the mask */
      cairo_set_operator (cr, CAIRO_OPERATOR_ADD);
      cairo_mask (cr, mask_pattern);
    }
}

static void
gsk_composite_node_diff (GskRenderNode *node1,
                         GskRenderNode *node2,
                         GskDiffData   *data)
{
  GskCompositeNode *self1 = (GskCompositeNode *) node1;
  GskCompositeNode *self2 = (GskCompositeNode *) node2;

  if (self1->op != self2->op)
    {
      gsk_render_node_diff_impossible (node1, node2, data);
      return;
    }

  gsk_render_node_diff (self1->child, self2->child, data);
  gsk_render_node_diff (self1->mask, self2->mask, data);
}

static GskRenderNode **
gsk_composite_node_get_children (GskRenderNode *node,
                                 gsize         *n_children)
{
  GskCompositeNode *self = (GskCompositeNode *) node;

  *n_children = G_N_ELEMENTS (self->children);

  return self->children;
}

static GskRenderNode *
gsk_composite_node_replay (GskRenderNode   *node,
                           GskRenderReplay *replay)
{
  GskCompositeNode *self = (GskCompositeNode *) node;
  GskRenderNode *result, *child, *mask;

  child = gsk_render_replay_filter_node (replay, self->child);
  mask = gsk_render_replay_filter_node (replay, self->mask);

  if (mask == NULL)
    return NULL;
  if (child == NULL)
    child = gsk_container_node_new (NULL, 0);

  if (child == self->child && mask == self->mask)
    result = gsk_render_node_ref (node);
  else
    result = gsk_composite_node_new (child, mask, self->op);

  gsk_render_node_unref (child);
  gsk_render_node_unref (mask);

  return result;
}

static gboolean
gsk_composite_node_get_opaque_rect (GskRenderNode   *node,
                                    graphene_rect_t *out_opaque)
{
  GskCompositeNode *self = (GskCompositeNode *) node;
  graphene_rect_t mask_opaque, child_opaque;

  if (gsk_porter_duff_clears_foreground (self->op))
    return FALSE;

  if (!gsk_render_node_get_opaque_rect (self->child, &child_opaque) ||
      !gsk_render_node_get_opaque_rect (self->mask, &mask_opaque))
    return FALSE;

  return gsk_rect_intersection (&child_opaque, &mask_opaque, out_opaque);
}

static void
gsk_composite_node_render_opacity (GskRenderNode  *node,
                                   GskOpacityData *data)
{
  GskCompositeNode *self = (GskCompositeNode *) node;

  switch (self->op)
    {
      case GSK_PORTER_DUFF_SOURCE:
      case GSK_PORTER_DUFF_DEST_ATOP_SOURCE:

      case GSK_PORTER_DUFF_SOURCE_OVER_DEST:
      case GSK_PORTER_DUFF_DEST_OVER_SOURCE:
        {
          GskOpacityData child_data = GSK_OPACITY_DATA_INIT_EMPTY (data->copies);

          gsk_render_node_render_opacity (self->child, &child_data);

          if (!gsk_rect_contains_rect (&child_data.opaque, &self->mask->bounds))
            {
              GskOpacityData mask_data = GSK_OPACITY_DATA_INIT_EMPTY (data->copies);
              gboolean empty;

              gsk_render_node_render_opacity (self->mask, &mask_data);
              empty = !gsk_rect_intersection (&child_data.opaque, &mask_data.opaque, &child_data.opaque);

              if (!empty &&
                  self->op != GSK_PORTER_DUFF_SOURCE_OVER_DEST &&
                  self->op != GSK_PORTER_DUFF_DEST_OVER_SOURCE)
                {
                  if (!gsk_rect_subtract (&data->opaque, &self->mask->bounds, &data->opaque))
                    data->opaque = GRAPHENE_RECT_INIT (0, 0, 0, 0);
                }
            }
          else
            {
              gsk_rect_intersection (&child_data.opaque, &self->mask->bounds, &child_data.opaque);
            }

          if (gsk_rect_is_empty (&data->opaque))
            data->opaque = child_data.opaque;
          else
            gsk_rect_coverage (&data->opaque, &child_data.opaque, &data->opaque);
        }
        break;

      case GSK_PORTER_DUFF_DEST:
      case GSK_PORTER_DUFF_SOURCE_ATOP_DEST:
        /* no changes */
        break;

      case GSK_PORTER_DUFF_SOURCE_IN_DEST:
      case GSK_PORTER_DUFF_DEST_IN_SOURCE:
      case GSK_PORTER_DUFF_SOURCE_OUT_DEST:
      case GSK_PORTER_DUFF_DEST_OUT_SOURCE:
      case GSK_PORTER_DUFF_XOR:
        /* feel free to implement these. For now: */
      case GSK_PORTER_DUFF_CLEAR:
        if (!gsk_rect_subtract (&data->opaque, &self->mask->bounds, &data->opaque))
          data->opaque = GRAPHENE_RECT_INIT (0, 0, 0, 0);
        break;

      default:
        g_assert_not_reached ();
        break;
    }
}

static void
gsk_composite_node_class_init (gpointer g_class,
                               gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_COMPOSITE_NODE;

  node_class->finalize = gsk_composite_node_finalize;
  node_class->draw = gsk_composite_node_draw;
  node_class->diff = gsk_composite_node_diff;
  node_class->get_children = gsk_composite_node_get_children;
  node_class->replay = gsk_composite_node_replay;
  node_class->get_opaque_rect = gsk_composite_node_get_opaque_rect;
  node_class->render_opacity = gsk_composite_node_render_opacity;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskCompositeNode, gsk_composite_node)

/**
 * gsk_composite_node_new:
 * @child: The child to composite
 * @mask: The mask where the compositing will apply
 * @op: The compositing operator
 *
 * Creates a `GskRenderNode` that will composite the child onto the
 * background with the given operator wherever the mask is set.
 *
 * Note that various operations can modify the background outside of
 * the child's bounds, so the mask may cause visual changes outside
 * of the child.
 *
 * Returns: (transfer full) (type GskCompositeNode): A new `GskRenderNode`
 *
 * Since: 4.22
 */
GskRenderNode *
gsk_composite_node_new (GskRenderNode *child,
                        GskRenderNode *mask,
                        GskPorterDuff  op)
{
  GskCompositeNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (mask), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_COMPOSITE_NODE);
  node = (GskRenderNode *) self;
  self->child = gsk_render_node_ref (child);
  self->mask = gsk_render_node_ref (mask);
  self->op = op;

  if (gsk_porter_duff_is_bound_by_source (op))
    {
      if (!gsk_rect_intersection (&child->bounds, &mask->bounds, &node->bounds))
        node->bounds = *graphene_rect_zero ();
    }
  else
    {
      node->bounds = mask->bounds;
    }

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->clears_background = gsk_porter_duff_clears_background (op);
  if (gsk_porter_duff_clears_foreground (op) ||
      !gsk_render_node_is_fully_opaque (mask))
    node->fully_opaque = FALSE;
  else
    node->fully_opaque = gsk_render_node_is_fully_opaque (child);
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child) ||
                                   gsk_render_node_contains_subsurface_node (mask);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child) ||
                              gsk_render_node_contains_paste_node (mask);

  return node;
}

/**
 * gsk_composite_node_get_child:
 * @node: (type GskCompositeNode): a composite `GskRenderNode`
 *
 * Gets the child node that is getting composited by the given @node.
 *
 * Returns: (transfer none): the child `GskRenderNode`
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_composite_node_get_child (const GskRenderNode *node)
{
  const GskCompositeNode *self = (const GskCompositeNode *) node;

  return self->child;
}

/**
 * gsk_composite_node_get_mask:
 * @node: (type GskCompositeNode): a composite `GskRenderNode`
 *
 * Gets the mask node that describes the region where the compositing
 * applies.
 *
 * Returns: (transfer none): the mask `GskRenderNode`
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_composite_node_get_mask (const GskRenderNode *node)
{
  const GskCompositeNode *self = (const GskCompositeNode *) node;

  return self->mask;
}

/**
 * gsk_composite_node_get_operator:
 * @node: (type GskCompositeNode): a composite `GskRenderNode`
 *
 * Gets the compositing operator used by this node.
 *
 * Returns: (transfer none): The compositing operator
 *
 * Since: 4.22
 **/
GskPorterDuff
gsk_composite_node_get_operator (const GskRenderNode *node)
{
  const GskCompositeNode *self = (const GskCompositeNode *) node;

  return self->op;
}

G_END_DECLS

