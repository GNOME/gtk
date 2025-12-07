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

#include "gsksubsurfacenode.h"

#include "gskrendernodeprivate.h"
#include "gskrectprivate.h"
#include "gskrenderreplay.h"

#include "gdk/gdksubsurfaceprivate.h"

/* }}} */
/* {{{ GSK_SUBSURFACE_NODE */

/**
 * GskSubsurfaceNode:
 *
 * A render node that potentially diverts a part of the scene graph to a subsurface.
 *
 * Since: 4.14
 */
struct _GskSubsurfaceNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  GdkSubsurface *subsurface;
};

static void
gsk_subsurface_node_finalize (GskRenderNode *node)
{
  GskSubsurfaceNode *self = (GskSubsurfaceNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_SUBSURFACE_NODE));

  gsk_render_node_unref (self->child);
  g_clear_object (&self->subsurface);

  parent_class->finalize (node);
}

static void
gsk_subsurface_node_draw (GskRenderNode *node,
                          cairo_t       *cr,
                          GskCairoData  *data)
{
  GskSubsurfaceNode *self = (GskSubsurfaceNode *) node;

  gsk_render_node_draw_full (self->child, cr, data);
}

static gboolean
gsk_subsurface_node_can_diff (const GskRenderNode *node1,
                              const GskRenderNode *node2)
{
  GskSubsurfaceNode *self1 = (GskSubsurfaceNode *) node1;
  GskSubsurfaceNode *self2 = (GskSubsurfaceNode *) node2;

  return self1->subsurface == self2->subsurface;
}

static void
gsk_subsurface_node_diff (GskRenderNode *node1,
                          GskRenderNode *node2,
                          GskDiffData   *data)
{
  GskSubsurfaceNode *self1 = (GskSubsurfaceNode *) node1;
  GskSubsurfaceNode *self2 = (GskSubsurfaceNode *) node2;

  if (self1->subsurface != self2->subsurface)
    {
      /* Shouldn't happen, can_diff() avoids this, but to be sure */
      gsk_render_node_diff_impossible (node1, node2, data);
    }
  else if (self1->subsurface && self1->subsurface->parent != data->surface)
    {
      /* The inspector case */
      gsk_render_node_diff (self1->child, self2->child, data);
    }
  else if (self1->subsurface && gdk_subsurface_get_texture (self1->subsurface) != NULL)
    {
      /* offloaded, no contents to compare */
    }
  else
    {
      /* not offloaded, diff the children */
      gsk_render_node_diff (self1->child, self2->child, data);
    }
}

static gboolean
gsk_subsurface_node_get_opaque_rect (GskRenderNode   *node,
                                     graphene_rect_t *out_opaque)
{
  GskSubsurfaceNode *self = (GskSubsurfaceNode *) node;

  return gsk_render_node_get_opaque_rect (self->child, out_opaque);
}

static GskRenderNode **
gsk_subsurface_node_get_children (GskRenderNode *node,
                                  gsize         *n_children)
{
  GskSubsurfaceNode *self = (GskSubsurfaceNode *) node;

  *n_children = 1;
  
  return &self->child;
}

static GskRenderNode *
gsk_subsurface_node_replay (GskRenderNode   *node,
                            GskRenderReplay *replay)
{
  GskSubsurfaceNode *self = (GskSubsurfaceNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_subsurface_node_new (child, self->subsurface);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_subsurface_node_class_init (gpointer g_class,
                                gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_SUBSURFACE_NODE;

  node_class->finalize = gsk_subsurface_node_finalize;
  node_class->draw = gsk_subsurface_node_draw;
  node_class->can_diff = gsk_subsurface_node_can_diff;
  node_class->diff = gsk_subsurface_node_diff;
  node_class->get_children = gsk_subsurface_node_get_children;
  node_class->replay = gsk_subsurface_node_replay;
  node_class->get_opaque_rect = gsk_subsurface_node_get_opaque_rect;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskSubsurfaceNode, gsk_subsurface_node)

/**
 * gsk_subsurface_node_new: (skip)
 * @child: The child to divert to a subsurface
 * @subsurface: (nullable): the subsurface to use
 *
 * Creates a `GskRenderNode` that will possibly divert the child
 * node to a subsurface.
 *
 * Note: Since subsurfaces are currently private, these nodes cannot
 * currently be created outside of GTK. See
 * [GtkGraphicsOffload](../gtk4/class.GraphicsOffload.html).
 *
 * Returns: (transfer full) (type GskSubsurfaceNode): A new `GskRenderNode`
 *
 * Since: 4.14
 */
GskRenderNode *
gsk_subsurface_node_new (GskRenderNode *child,
                         gpointer       subsurface)
{
  GskSubsurfaceNode *self;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);

  self = gsk_render_node_alloc (GSK_TYPE_SUBSURFACE_NODE);
  node = (GskRenderNode *) self;
  node->fully_opaque = child->fully_opaque;

  self->child = gsk_render_node_ref (child);
  if (subsurface)
    self->subsurface = g_object_ref (subsurface);
  else
    self->subsurface = NULL;

  gsk_rect_init_from_rect (&node->bounds, &child->bounds);

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->clears_background = gsk_render_node_clears_background (child);
  node->copy_mode = gsk_render_node_get_copy_mode (child);
  node->contains_subsurface_node = TRUE;
  node->contains_paste_node = gsk_render_node_contains_paste_node (child);

  return node;
}

/**
 * gsk_subsurface_node_get_child:
 * @node: (type GskSubsurfaceNode): a subsurface `GskRenderNode`
 *
 * Gets the child node that is getting drawn by the given @node.
 *
 * Returns: (transfer none): the child `GskRenderNode`
 *
 * Since: 4.14
 */
GskRenderNode *
gsk_subsurface_node_get_child (const GskRenderNode *node)
{
  const GskSubsurfaceNode *self = (const GskSubsurfaceNode *) node;

  return self->child;
}

/**
 * gsk_subsurface_node_get_subsurface: (skip)
 * @node: (type GskDebugNode): a subsurface `GskRenderNode`
 *
 * Gets the subsurface that was set on this node
 *
 * Returns: (transfer none) (nullable): the subsurface
 *
 * Since: 4.14
 */
gpointer
gsk_subsurface_node_get_subsurface (const GskRenderNode *node)
{
  const GskSubsurfaceNode *self = (const GskSubsurfaceNode *) node;

  return self->subsurface;
}

