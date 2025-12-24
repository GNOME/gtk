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

#include "gskcomponenttransfernode.h"

#include "gskcomponenttransferprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"
#include "gskrectprivate.h"

#include "gdk/gdkcairoprivate.h"

/**
 * GskComponentTransferNode:
 *
 * A render node for applying a `GskComponentTransfer` for each color
 * component of the child node.
 *
 * Since: 4.20
 */
struct _GskComponentTransferNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  GskComponentTransfer transfer[4];
};

static void
gsk_component_transfer_node_finalize (GskRenderNode *node)
{
  GskComponentTransferNode *self = (GskComponentTransferNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_COMPONENT_TRANSFER_NODE));

  gsk_render_node_unref (self->child);

  gsk_component_transfer_clear (&self->transfer[0]);
  gsk_component_transfer_clear (&self->transfer[1]);
  gsk_component_transfer_clear (&self->transfer[2]);
  gsk_component_transfer_clear (&self->transfer[3]);

  parent_class->finalize (node);
}

static void
gsk_component_transfer_node_draw (GskRenderNode *node,
                                  cairo_t       *cr,
                                  GskCairoData  *data)
{
  GskComponentTransferNode *self = (GskComponentTransferNode *) node;
  int width, height;
  cairo_surface_t *surface;
  cairo_t *cr2;
  guchar *pixels;
  gsize stride;
  guint32 pixel;
  float r, g, b, a;
  cairo_pattern_t *pattern;

  width = ceil (node->bounds.size.width);
  height = ceil (node->bounds.size.height);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, width, height);

  cr2 = cairo_create (surface);
  gsk_render_node_draw_full (self->child, cr2, data);
  cairo_destroy (cr2);

  pixels = cairo_image_surface_get_data (surface);
  stride = cairo_image_surface_get_stride (surface);

  for (guint y = 0; y < height; y++)
    {
      for (guint x = 0; x < width; x++)
        {
          pixel = *(guint32 *)(pixels + y * stride + 4 * x);

          a = ((pixel >> 24) & 0xff) / 255.;
          r = ((pixel >> 16) & 0xff) / 255.;
          g = ((pixel >> 8) & 0xff) / 255.;
          b = ((pixel >> 0) & 0xff) / 255.;

          if (a != 0)
            {
              r /= a;
              g /= a;
              b /= a;
            }

          r = gsk_component_transfer_apply (&self->transfer[0], r);
          g = gsk_component_transfer_apply (&self->transfer[1], g);
          b = gsk_component_transfer_apply (&self->transfer[2], b);
          a = gsk_component_transfer_apply (&self->transfer[3], a);

          r *= a;
          g *= a;
          b *= a;

          pixel = CLAMP ((int) roundf (a * 255), 0, 255) << 24 |
                  CLAMP ((int) roundf (r * 255), 0, 255) << 16 |
                  CLAMP ((int) roundf (g * 255), 0, 255) << 8 |
                  CLAMP ((int) roundf (b * 255), 0, 255) << 0;

          *(guint32 *)(pixels + y * stride + 4 * x) = pixel;
        }
    }

  cairo_surface_mark_dirty (surface);

  pattern = cairo_pattern_create_for_surface (surface);
  cairo_pattern_set_extend (pattern, CAIRO_EXTEND_PAD);

  cairo_set_source (cr, pattern);
  cairo_pattern_destroy (pattern);
  cairo_surface_destroy (surface);

  gdk_cairo_rect (cr, &node->bounds);
  cairo_fill (cr);
}

static gboolean
gsk_component_transfer_node_can_diff (const GskRenderNode *node1,
                                      const GskRenderNode *node2)
{
  GskComponentTransferNode *self1 = (GskComponentTransferNode *) node1;
  GskComponentTransferNode *self2 = (GskComponentTransferNode *) node2;

  return gsk_component_transfer_equal (&self1->transfer[0], &self2->transfer[0]) &&
         gsk_component_transfer_equal (&self1->transfer[1], &self2->transfer[1]) &&
         gsk_component_transfer_equal (&self1->transfer[2], &self2->transfer[2]) &&
         gsk_component_transfer_equal (&self1->transfer[3], &self2->transfer[3]);
}

static void
gsk_component_transfer_node_diff (GskRenderNode *node1,
                                  GskRenderNode *node2,
                                  GskDiffData   *data)
{
  GskComponentTransferNode *self1 = (GskComponentTransferNode *) node1;
  GskComponentTransferNode *self2 = (GskComponentTransferNode *) node2;

  if (gsk_component_transfer_node_can_diff (node1, node2))
    gsk_render_node_diff (self1->child, self2->child, data);

  gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode **
gsk_component_transfer_node_get_children (GskRenderNode *node,
                                          gsize         *n_children)
{
  GskComponentTransferNode *self = (GskComponentTransferNode *) node;

  *n_children = 1;
  
  return &self->child;
}

static GskRenderNode *
gsk_component_transfer_node_replay (GskRenderNode   *node,
                                    GskRenderReplay *replay)
{
  GskComponentTransferNode *self = (GskComponentTransferNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_component_transfer_node_new (child,
                                              &self->transfer[0],
                                              &self->transfer[1],
                                              &self->transfer[2],
                                              &self->transfer[3]);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_component_transfer_node_class_init (gpointer g_class,
                                        gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_COMPONENT_TRANSFER_NODE;

  node_class->finalize = gsk_component_transfer_node_finalize;
  node_class->draw = gsk_component_transfer_node_draw;
  node_class->can_diff = gsk_component_transfer_node_can_diff;
  node_class->diff = gsk_component_transfer_node_diff;
  node_class->get_children = gsk_component_transfer_node_get_children;
  node_class->replay = gsk_component_transfer_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskComponentTransferNode, gsk_component_transfer_node)

/**
 * gsk_component_transfer_node_new:
 * @child: The child to apply component transfers to
 * @r: transfer for the red component
 * @g: transfer for the green component
 * @b: transfer for the blue component
 * @a: transfer for the alpha component
 *
 * Creates a render node that will apply component
 * transfers to a child node.
 *
 * Returns: (transfer full) (type GskComponentTransferNode): A new `GskRenderNode`
 *
 * Since: 4.20
 */
GskRenderNode *
gsk_component_transfer_node_new (GskRenderNode              *child,
                                 const GskComponentTransfer *r,
                                 const GskComponentTransfer *g,
                                 const GskComponentTransfer *b,
                                 const GskComponentTransfer *a)
{
  GskComponentTransferNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_COMPONENT_TRANSFER_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = FALSE;

  self->child = gsk_render_node_ref (child);

  gsk_component_transfer_init_copy (&self->transfer[0], r);
  gsk_component_transfer_init_copy (&self->transfer[1], g);
  gsk_component_transfer_init_copy (&self->transfer[2], b);
  gsk_component_transfer_init_copy (&self->transfer[3], a);

  gsk_rect_init_from_rect (&node->bounds, &child->bounds);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child);

  return node;
}

/**
 * gsk_component_transfer_node_get_child:
 * @node: (type GskComponentTransferNode): a component transfer `GskRenderNode`
 *
 * Gets the child node that is getting drawn by the given @node.
 *
 * Returns: (transfer none): the child `GskRenderNode`
 *
 * Since: 4.20
 */
GskRenderNode *
gsk_component_transfer_node_get_child (const GskRenderNode *node)
{
  const GskComponentTransferNode *self = (const GskComponentTransferNode *) node;

  return self->child;
}

/**
 * gsk_component_transfer_node_get_transfer:
 * @node: (type GskComponentTransferNode): a component transfer `GskRenderNode`
 * @component: a value between 0 and 3 to indicate the red, green, blue
 *   or alpha component
 *
 * Gets the component transfer for one of the components.
 *
 * Returns: (transfer none): the `GskComponentTransfer`
 *
 * Since: 4.20
 */
const GskComponentTransfer *
gsk_component_transfer_node_get_transfer (const GskRenderNode *node,
                                          guint                component)
{
  const GskComponentTransferNode *self = (const GskComponentTransferNode *) node;

  g_return_val_if_fail (component < 4, NULL);

  return &self->transfer[component];
}

