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

#include "gskisolationnode.h"

#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

#include "gdk/gdkcairoprivate.h"

/**
 * GskIsolationNode:
 *
 * A render node that isolates its child from surrounding rendernodes.
 *
 * Since: 4.22
 */
struct _GskIsolationNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  GskIsolation isolations;
};

static gboolean
gsk_isolation_node_is_isolating (GskIsolationNode *self,
                                 GskIsolation      feature)
{
  return (self->isolations & feature) == feature;
}

static void
gsk_isolation_node_finalize (GskRenderNode *node)
{
  GskIsolationNode *self = (GskIsolationNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_ISOLATION_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
gsk_isolation_node_draw (GskRenderNode *node,
                         cairo_t       *cr,
                         GskCairoData  *data)
{
  GskIsolationNode *self = (GskIsolationNode *) node;

  if (gsk_isolation_node_is_isolating (self, GSK_ISOLATION_BACKGROUND))
    {
      gdk_cairo_rectangle_snap_to_grid (cr, &node->bounds);
      cairo_clip (cr);
      cairo_push_group (cr);
    }

  gsk_render_node_draw_full (self->child, cr, data);

  if (gsk_isolation_node_is_isolating (self, GSK_ISOLATION_BACKGROUND))
    {
      cairo_pop_group_to_source (cr);
      cairo_paint (cr);
    }
}

static void
gsk_isolation_node_diff (GskRenderNode *node1,
                         GskRenderNode *node2,
                         GskDiffData   *data)
{
  GskIsolationNode *self1 = (GskIsolationNode *) node1;
  GskIsolationNode *self2 = (GskIsolationNode *) node2;
  GskDiffData child_data = {
    .surface = data->surface
  };

  if (self1->isolations != self2->isolations)
    {
      gsk_render_node_diff_impossible (node1, node2, data);
      return;
    }

  if (gsk_isolation_node_is_isolating (self1, GSK_ISOLATION_BACKGROUND))
    child_data.region = cairo_region_create ();
  else
    child_data.region = data->region;

  if (gsk_isolation_node_is_isolating (self1, GSK_ISOLATION_COPY_PASTE))
    child_data.copies = NULL;
  else
    child_data.copies = data->copies;

  gsk_render_node_diff (self1->child, self2->child, &child_data);

  if (gsk_isolation_node_is_isolating (self1, GSK_ISOLATION_BACKGROUND))
    {
      cairo_region_union (data->region, child_data.region);
      cairo_region_destroy (child_data.region);
    }
}

static GskRenderNode **
gsk_isolation_node_get_children (GskRenderNode *node,
                                 gsize         *n_children)
{
  GskIsolationNode *self = (GskIsolationNode *) node;

  *n_children = 1;

  return &self->child;
}

static void
gsk_isolation_node_render_opacity (GskRenderNode  *node,
                                   GskOpacityData *data)
{
  GskIsolationNode *self = (GskIsolationNode *) node;
  GskOpacityData child_data = GSK_OPACITY_DATA_INIT_EMPTY (NULL);

  if (!gsk_isolation_node_is_isolating (self, GSK_ISOLATION_BACKGROUND))
    child_data.opaque = data->opaque;

  if (!gsk_isolation_node_is_isolating (self, GSK_ISOLATION_COPY_PASTE))
    child_data.copies = data->copies;

  gsk_render_node_render_opacity (self->child, &child_data);

  if (gsk_isolation_node_is_isolating (self, GSK_ISOLATION_BACKGROUND))
    {
      if (gsk_rect_is_empty (&data->opaque))
        data->opaque = child_data.opaque;
      else
        gsk_rect_coverage (&data->opaque, &child_data.opaque, &data->opaque);
    }
  else
    {
      data->opaque = child_data.opaque;
    }
}

static GskRenderNode *
gsk_isolation_node_replay (GskRenderNode   *node,
                           GskRenderReplay *replay)
{
  GskIsolationNode *self = (GskIsolationNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_isolation_node_new (child, self->isolations);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_isolation_node_class_init (gpointer g_class,
                               gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_ISOLATION_NODE;

  node_class->finalize = gsk_isolation_node_finalize;
  node_class->draw = gsk_isolation_node_draw;
  node_class->diff = gsk_isolation_node_diff;
  node_class->get_children = gsk_isolation_node_get_children;
  node_class->replay = gsk_isolation_node_replay;
  node_class->render_opacity = gsk_isolation_node_render_opacity;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskIsolationNode, gsk_isolation_node)

/**
 * gsk_isolation_node_new:
 * @child: The child
 * @isolations: features to isolate
 *
 * Creates a `GskRenderNode` that isolates the drawing operations of
 * the child from surrounding ones.
 *
 * You can express "everything but these flags" in a forward compatible
 * way by using bit math:
 * `GSK_ISOLATION_ALL & ~(GSK_ISOLATION_BACKGROUND | GSK_ISOLATION_COPY_PASTE)`
 * will isolate everything but background and copy/paste.
 *
 * For the available isolations, see [flags@Gsk.Isolation].
 *
 * Returns: (transfer full) (type GskIsolationNode): A new `GskRenderNode`
 *
 * Since: 4.22
 */
GskRenderNode *
gsk_isolation_node_new (GskRenderNode *child,
                        GskIsolation   isolations)
{
  GskIsolationNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_ISOLATION_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = child->fully_opaque;

  self->child = gsk_render_node_ref (child);
  self->isolations = isolations;

  gsk_rect_init_from_rect (&node->bounds, &child->bounds);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  if (!gsk_isolation_node_is_isolating (self, GSK_ISOLATION_BACKGROUND))
    {
      node->clears_background = gsk_render_node_clears_background (child);
      node->copy_mode = gsk_render_node_get_copy_mode (child);
    }
  if (!gsk_isolation_node_is_isolating (self, GSK_ISOLATION_COPY_PASTE))
    node->contains_paste_node = gsk_render_node_contains_paste_node (child);
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);

  return node;
}

/**
 * gsk_isolation_node_get_child:
 * @node: (type GskIsolationNode): an isolation `GskRenderNode`
 *
 * Gets the child node that is getting drawn by the given @node.
 *
 * Returns: (transfer none): the child `GskRenderNode`
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_isolation_node_get_child (const GskRenderNode *node)
{
  const GskIsolationNode *self = (const GskIsolationNode *) node;

  return self->child;
}

/**
 * gsk_isolation_node_get_isolations:
 * @node: (type GskIsolationNode): an isolation `GskRenderNode`
 *
 * Gets the isolation features that are enforced by this node.
 *
 * Returns: the isolation features
 *
 * Since: 4.22
 **/
GskIsolation
gsk_isolation_node_get_isolations (const GskRenderNode *node)
{
  const GskIsolationNode *self = (const GskIsolationNode *) node;

  return self->isolations;
}

