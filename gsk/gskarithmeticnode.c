/* GSK - The GTK Scene Kit
 *
 * Copyright 2025 Red Hat, Inc.
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

#include "gskarithmeticnodeprivate.h"

#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"
#include "gskcontainernode.h"

#include "gdk/gdkcairoprivate.h"

/**
 * GskArithmeticNode:
 *
 * A render node applying the 'arithmetic' composite operator,
 * as defined in the
 * [CSS filter effects](https://www.w3.org/TR/filter-effects-1/)
 * spec,
 *
 *     result = k1 * i1 * i1 + k2 * i1 + k3 * i3 + k4
 */
struct _GskArithmeticNode
{
  GskRenderNode render_node;

  union {
    GskRenderNode *children[2];
    struct {
      GskRenderNode *first;
      GskRenderNode *second;
    };
  };
  float factors[4];
};

static void
gsk_arithmetic_node_finalize (GskRenderNode *node)
{
  GskArithmeticNode *self = (GskArithmeticNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_ARITHMETIC_NODE));

  gsk_render_node_unref (self->first);
  gsk_render_node_unref (self->second);

  parent_class->finalize (node);
}

static void
gsk_arithmetic_node_draw (GskRenderNode *node,
                          cairo_t       *cr,
                          GskCairoData  *data)
{
  GskArithmeticNode *self = (GskArithmeticNode *) node;
  cairo_pattern_t *first_pattern, *second_pattern;
  cairo_surface_t *first_surface, *second_surface;
  cairo_surface_t *first_image, *second_image;
  guchar *first_data, *second_data;
  int first_width, first_height, first_stride;
  int second_stride;
  guint32 pixel1, pixel2;
  float r1, g1, b1, a1, r2, g2, b2, a2, r, g, b, a;
  float k1, k2, k3, k4;

  gdk_cairo_rect (cr, &node->bounds);
  cairo_clip (cr);

  if (gdk_cairo_is_all_clipped (cr))
    return;

  k1 = self->factors[0];
  k2 = self->factors[1];
  k3 = self->factors[2];
  k4 = self->factors[3];

  if (!gdk_color_state_equal (data->ccs, GDK_COLOR_STATE_SRGB))
    g_warning ("arithmetic node in non-srgb colorstate isn't implemented yet.");

  cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
  gsk_render_node_draw_full (self->first, cr, data);
  first_pattern = cairo_pop_group (cr);
  cairo_pattern_get_surface (first_pattern, &first_surface);
  first_image = cairo_surface_map_to_image (first_surface, NULL);
  first_data = cairo_image_surface_get_data (first_image);
  first_width = cairo_image_surface_get_width (first_image);
  first_height = cairo_image_surface_get_height (first_image);
  first_stride = cairo_image_surface_get_stride (first_image);

  gdk_cairo_rect (cr, &node->bounds);
  cairo_clip (cr);

  cairo_push_group_with_content (cr, CAIRO_CONTENT_COLOR_ALPHA);
  gsk_render_node_draw_full (self->second, cr, data);
  second_pattern = cairo_pop_group (cr);
  cairo_pattern_get_surface (second_pattern, &second_surface);
  second_image = cairo_surface_map_to_image (second_surface, NULL);
  second_data = cairo_image_surface_get_data (second_image);
  second_stride = cairo_image_surface_get_stride (second_image);

  g_assert (first_width == cairo_image_surface_get_width (second_image));
  g_assert (first_height == cairo_image_surface_get_height (second_image));

  for (guint y = 0; y < first_height; y++)
    {
      for (guint x = 0; x < first_width; x++)
        {
          pixel1 = *(guint32 *)(first_data + y * first_stride + 4 * x);
          a1 = ((pixel1 >> 24) & 0xff) / 255.;

          pixel2 = *(guint32 *)(second_data + y * second_stride + 4 * x);
          a2 = ((pixel2 >> 24) & 0xff) / 255.;

          a = k1 * a1 * a2 + k2 * a1 + k3 * a2 + k4;
          a = CLAMP (a, 0, 1);

          if (a > 0)
            {
              r1 = ((pixel1 >> 16) & 0xff) / 255.;
              g1 = ((pixel1 >> 8) & 0xff) / 255.;
              b1 = ((pixel1 >> 0) & 0xff) / 255.;

              r2 = ((pixel2 >> 16) & 0xff) / 255.;
              g2 = ((pixel2 >> 8) & 0xff) / 255.;
              b2 = ((pixel2 >> 0) & 0xff) / 255.;

              r = k1 * r1 * r2 + k2 * r1 + k3 * r2 + k4;
              g = k1 * g1 * g2 + k2 * g1 + k3 * g2 + k4;
              b = k1 * b1 * b2 + k2 * b1 + k3 * b2 + k4;

              r = CLAMP (r, 0, a);
              g = CLAMP (g, 0, a);
              b = CLAMP (b, 0, a);
            }
          else
            {
              r = 0;
              g = 0;
              b = 0;
            }

          *(guint32 *)(first_data + y * first_stride + 4 * x) =
              CLAMP ((int) roundf (a * 255), 0, 255) << 24 |
              CLAMP ((int) roundf (r * 255), 0, 255) << 16 |
              CLAMP ((int) roundf (g * 255), 0, 255) << 8 |
              CLAMP ((int) roundf (b * 255), 0, 255) << 0;
        }
    }

  cairo_surface_unmap_image (first_surface, first_image);
  cairo_surface_unmap_image (second_surface, second_image);

  cairo_set_source (cr, first_pattern);

  gdk_cairo_rect (cr, &node->bounds);
  cairo_fill (cr);

  cairo_pattern_destroy (first_pattern);
  cairo_pattern_destroy (second_pattern);
}

static void
gsk_arithmetic_node_diff (GskRenderNode *node1,
                          GskRenderNode *node2,
                          GskDiffData   *data)
{
  GskArithmeticNode *self1 = (GskArithmeticNode *) node1;
  GskArithmeticNode *self2 = (GskArithmeticNode *) node2;

  if (self1->factors[0] == self2->factors[0] &&
      self1->factors[1] == self2->factors[1] &&
      self1->factors[2] == self2->factors[2] &&
      self1->factors[3] == self2->factors[3])
    {
      gsk_render_node_diff (self1->first, self2->first, data);
      gsk_render_node_diff (self1->second, self2->second, data);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

static GskRenderNode **
gsk_arithmetic_node_get_children (GskRenderNode *node,
                                  gsize         *n_children)
{
  GskArithmeticNode *self = (GskArithmeticNode *) node;

  *n_children = G_N_ELEMENTS (self->children);

  return self->children;
}

static GskRenderNode *
gsk_arithmetic_node_replay (GskRenderNode   *node,
                            GskRenderReplay *replay)
{
  GskArithmeticNode *self = (GskArithmeticNode *) node;
  GskRenderNode *result, *first, *second;

  first = gsk_render_replay_filter_node (replay, self->first);
  second = gsk_render_replay_filter_node (replay, self->second);

  if (first == NULL)
    {
      if (second == NULL)
        return NULL;

      first = gsk_container_node_new (NULL, 0);
    }
  else if (second == NULL)
    {
      second = gsk_container_node_new (NULL, 0);
    }

  if (first == self->first && second == self->second)
    result = gsk_render_node_ref (node);
  else
    result = gsk_arithmetic_node_new (&node->bounds,
                                      first, second,
                                      self->factors[0],
                                      self->factors[1],
                                      self->factors[2],
                                      self->factors[3]);

  gsk_render_node_unref (first);
  gsk_render_node_unref (second);

  return result;
}

static void
gsk_arithmetic_node_class_init (gpointer g_class,
                                gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_ARITHMETIC_NODE;

  node_class->finalize = gsk_arithmetic_node_finalize;
  node_class->draw = gsk_arithmetic_node_draw;
  node_class->diff = gsk_arithmetic_node_diff;
  node_class->get_children = gsk_arithmetic_node_get_children;
  node_class->replay = gsk_arithmetic_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskArithmeticNode, gsk_arithmetic_node)

/*< private >
 * gsk_arithmetic_node_new:
 * @bounds: The bounds for the node
 * @first: The first node to be composited
 * @second: The second node to be composited
 * @k1: first factor
 * @k2: second factor
 * @k3: third factor
 * @k4: fourth factor
 *
 * Creates a `GskRenderNode` that will composite the
 * @first and @second nodes arithmetically.
 *
 * Returns: (transfer full) (type GskArithmeticNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_arithmetic_node_new (const graphene_rect_t *bounds,
                         GskRenderNode         *first,
                         GskRenderNode         *second,
                         float                  k1,
                         float                  k2,
                         float                  k3,
                         float                  k4)
{
  GskArithmeticNode *self;
  GskRenderNode *node;
  graphene_rect_t child_bounds;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (first), NULL);
  g_return_val_if_fail (GSK_IS_RENDER_NODE (second), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_ARITHMETIC_NODE);
  node = (GskRenderNode *) self;

  self->first = gsk_render_node_ref (first);
  self->second = gsk_render_node_ref (second);
  self->factors[0] = k1;
  self->factors[1] = k2;
  self->factors[2] = k3;
  self->factors[3] = k4;

  graphene_rect_union (&first->bounds, &second->bounds, &child_bounds);
  graphene_rect_intersection (bounds, &child_bounds, &node->bounds);

  node->preferred_depth = gdk_memory_depth_merge (gsk_render_node_get_preferred_depth (first),
                                                  gsk_render_node_get_preferred_depth (second));
  node->is_hdr = gsk_render_node_is_hdr (first) ||
                 gsk_render_node_is_hdr (second);

  return node;
}

/*< private >
 * gsk_arithmetic_node_get_first_child:
 * @node: (type GskArithmeticNode): a `GskRenderNode`
 *
 * Retrieves the first `GskRenderNode` child of the @node.
 *
 * Returns: (transfer none): the first child node
 */
GskRenderNode *
gsk_arithmetic_node_get_first_child (const GskRenderNode *node)
{
  const GskArithmeticNode *self = (const GskArithmeticNode *) node;

  return self->first;
}

/*< private >
 * gsk_arithmetic_node_get_second_child:
 * @node: (type GskArithmeticNode): a `GskRenderNode`
 *
 * Retrieves the second `GskRenderNode` child of the @node.
 *
 * Returns: (transfer none): the second child node
 */
GskRenderNode *
gsk_arithmetic_node_get_second_child (const GskRenderNode *node)
{
  const GskArithmeticNode *self = (const GskArithmeticNode *) node;

  return self->second;
}

/*< private >
 * gsk_arithmetic_node_get_factors:
 * @node: (type GskArithmeticNode): a `GskRenderNode`
 * @k1: Return location for first factor
 * @k2: Return location for second factor
 * @k3: Return location for third factor
 * @k4: Return location for fourth factor
 *
 * Retrieves the factors used by @node.
 */
void
gsk_arithmetic_node_get_factors (const GskRenderNode *node,
                                 float               *k1,
                                 float               *k2,
                                 float               *k3,
                                 float               *k4)
{
  const GskArithmeticNode *self = (const GskArithmeticNode *) node;

  *k1 = self->factors[0];
  *k2 = self->factors[1];
  *k3 = self->factors[2];
  *k4 = self->factors[3];
}
