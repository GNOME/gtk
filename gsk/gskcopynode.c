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

#include "gskcopynode.h"

#include "gskpastenode.h"
#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"
#include "gskrenderreplay.h"

/**
 * GskCopyNode:
 *
 * A render node that copies the current state of the rendering canvas
 * so a [class@Gsk.PasteNode] can draw it.
 *
 * Since: 4.22
 */
struct _GskCopyNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  graphene_rect_t paste_area;
};

static void
gsk_copy_node_finalize (GskRenderNode *node)
{
  GskCopyNode *self = (GskCopyNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_COPY_NODE));

  gsk_render_node_unref (self->child);

  parent_class->finalize (node);
}

static void
gsk_copy_node_draw (GskRenderNode *node,
                    cairo_t       *cr,
                    GskCairoData  *data)
{
  GskCopyNode *self = (GskCopyNode *) node;

  gsk_render_node_draw_full (self->child, cr, data);
}

static void
gsk_copy_node_diff (GskRenderNode *node1,
                    GskRenderNode *node2,
                    GskDiffData   *data)
{
  GskCopyNode *self1 = (GskCopyNode *) node1;
  GskCopyNode *self2 = (GskCopyNode *) node2;
  GSList copy = { cairo_region_copy (data->region), data->copies };

  data->copies = &copy;

  gsk_render_node_diff (self1->child, self2->child, data);

  cairo_region_destroy (copy.data);
  data->copies = copy.next;
}

static GskRenderNode **
gsk_copy_node_get_children (GskRenderNode *node,
                            gsize         *n_children)
{
  GskCopyNode *self = (GskCopyNode *) node;

  *n_children = 1;

  return &self->child;
}

static void
gsk_copy_node_render_opacity (GskRenderNode  *node,
                              GskOpacityData *data)
{
  GskCopyNode *self = (GskCopyNode *) node;
  graphene_rect_t saved = data->opaque;
  GSList copy = { &saved, (GSList *) data->copies };

  data->copies = &copy;

  gsk_render_node_render_opacity (self->child, data);

  data->copies = data->copies->next;
}

static GskRenderNode *
gsk_copy_node_replay (GskRenderNode   *node,
                      GskRenderReplay *replay)
{
  GskCopyNode *self = (GskCopyNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_copy_node_new (child);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_copy_node_class_init (gpointer g_class,
                           gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_COPY_NODE;

  node_class->finalize = gsk_copy_node_finalize;
  node_class->draw = gsk_copy_node_draw;
  node_class->diff = gsk_copy_node_diff;
  node_class->get_children = gsk_copy_node_get_children;
  node_class->replay = gsk_copy_node_replay;
  node_class->render_opacity = gsk_copy_node_render_opacity;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskCopyNode, gsk_copy_node)

static void
gsk_copy_node_analyze_child (GskCopyNode   *self,
                             GskRenderNode *node,
                             guint          depth)
{
  GskRenderNode **children;
  gsize i, n_children;

  if (!gsk_render_node_contains_paste_node (node))
    return;

  children = gsk_render_node_get_children (node, &n_children);

  switch (gsk_render_node_get_node_type (node))
    {
    case GSK_COPY_NODE:
      gsk_copy_node_analyze_child (self, children[0], depth + 1);
      break;

    case GSK_PASTE_NODE:
      if (gsk_paste_node_get_depth (node) == depth)
        {
          if (gsk_rect_is_empty (&self->paste_area))
            self->paste_area = node->bounds;
          else
            graphene_rect_union (&self->paste_area, &node->bounds, &self->paste_area);
        }
      else
        {
          self->render_node.contains_paste_node = TRUE;
        }
      break;

    case GSK_CONTAINER_NODE:
    case GSK_CAIRO_NODE:
    case GSK_COLOR_NODE:
    case GSK_LINEAR_GRADIENT_NODE:
    case GSK_REPEATING_LINEAR_GRADIENT_NODE:
    case GSK_RADIAL_GRADIENT_NODE:
    case GSK_REPEATING_RADIAL_GRADIENT_NODE:
    case GSK_CONIC_GRADIENT_NODE:
    case GSK_BORDER_NODE:
    case GSK_TEXTURE_NODE:
    case GSK_INSET_SHADOW_NODE:
    case GSK_OUTSET_SHADOW_NODE:
    case GSK_TRANSFORM_NODE:
    case GSK_OPACITY_NODE:
    case GSK_COLOR_MATRIX_NODE:
    case GSK_REPEAT_NODE:
    case GSK_CLIP_NODE:
    case GSK_ROUNDED_CLIP_NODE:
    case GSK_SHADOW_NODE:
    case GSK_BLEND_NODE:
    case GSK_CROSS_FADE_NODE:
    case GSK_TEXT_NODE:
    case GSK_BLUR_NODE:
    case GSK_DEBUG_NODE:
    case GSK_GL_SHADER_NODE:
    case GSK_TEXTURE_SCALE_NODE:
    case GSK_MASK_NODE:
    case GSK_FILL_NODE:
    case GSK_STROKE_NODE:
    case GSK_SUBSURFACE_NODE:
    case GSK_COMPONENT_TRANSFER_NODE:
    case GSK_COMPOSITE_NODE:
    case GSK_ISOLATION_NODE:
    case GSK_DISPLACEMENT_NODE:
    case GSK_ARITHMETIC_NODE:
      for (i = 0; i < n_children; i++)
        {
          gsk_copy_node_analyze_child (self, children[i], depth);
        }
      break;

    case GSK_NOT_A_RENDER_NODE:
    default:
      g_assert_not_reached ();
      return;
    }
}

/**
 * gsk_copy_node_new:
 * @child: The child
 *
 * Creates a `GskRenderNode` that copies the current rendering
 * canvas for playback by paste nodes that are part of the child.
 *
 * Returns: (transfer full) (type GskCopyNode): A new `GskRenderNode`
 *
 * Since: 4.22
 */
GskRenderNode *
gsk_copy_node_new (GskRenderNode *child)
{
  GskCopyNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_COPY_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = child->fully_opaque;

  self->child = gsk_render_node_ref (child);

  gsk_rect_init_from_rect (&node->bounds, &child->bounds);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->clears_background = gsk_render_node_clears_background (child);
  node->copy_mode = gsk_render_node_get_copy_mode (child);
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);

  gsk_copy_node_analyze_child (self, child, 0);

  if (!gsk_rect_is_empty (&self->paste_area))
    node->copy_mode = GSK_COPY_ANY;

  return node;
}

/**
 * gsk_copy_node_get_child:
 * @node: (type GskCopyNode): a copy `GskRenderNode`
 *
 * Gets the child node that is getting drawn by the given @node.
 *
 * Returns: (transfer none): the child `GskRenderNode`
 *
 * Since: 4.22
 **/
GskRenderNode *
gsk_copy_node_get_child (const GskRenderNode *node)
{
  const GskCopyNode *self = (const GskCopyNode *) node;

  return self->child;
}

