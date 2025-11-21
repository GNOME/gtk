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

#include "gskcontainernodeprivate.h"

#include "gskdiffprivate.h"
#include "gskrectprivate.h"
#include "gskrenderreplay.h"

/* maximal number of rectangles we keep in a diff region before we throw
 * the towel and just use the bounding box of the parent node.
 * Meant to avoid performance corner cases.
 */
#define MAX_RECTS_IN_DIFF 30

/**
 * GskContainerNode:
 *
 * A render node that can contain other render nodes.
 */
struct _GskContainerNode
{
  GskRenderNode render_node;

  gboolean disjoint;
  graphene_rect_t opaque; /* Can be 0 0 0 0 to mean no opacity */
  guint n_children;
  GskRenderNode **children;
};

static void
gsk_container_node_finalize (GskRenderNode *node)
{
  GskContainerNode *container = (GskContainerNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_CONTAINER_NODE));

  for (guint i = 0; i < container->n_children; i++)
    gsk_render_node_unref (container->children[i]);

  g_free (container->children);

  parent_class->finalize (node);
}

static void
gsk_container_node_draw (GskRenderNode *node,
                         cairo_t       *cr,
                         GdkColorState *ccs)
{
  GskContainerNode *container = (GskContainerNode *) node;
  guint i;

  for (i = 0; i < container->n_children; i++)
    {
      gsk_render_node_draw_ccs (container->children[i], cr, ccs);
    }
}

static int
gsk_container_node_compare_func (gconstpointer elem1, gconstpointer elem2, gpointer data)
{
  return gsk_render_node_can_diff ((const GskRenderNode *) elem1, (const GskRenderNode *) elem2) ? 0 : 1;
}

static GskDiffResult
gsk_container_node_keep_func (gconstpointer elem1, gconstpointer elem2, gpointer user_data)
{
  GskDiffData *data = user_data;
  gsk_render_node_diff ((GskRenderNode *) elem1, (GskRenderNode *) elem2, data);
  if (cairo_region_num_rectangles (data->region) > MAX_RECTS_IN_DIFF)
    return GSK_DIFF_ABORTED;

  return GSK_DIFF_OK;
}

static GskDiffResult
gsk_container_node_change_func (gconstpointer elem, gsize idx, gpointer user_data)
{
  const GskRenderNode *node = elem;
  GskDiffData *data = user_data;
  cairo_rectangle_int_t rect;

  gsk_rect_to_cairo_grow (&node->bounds, &rect);
  cairo_region_union_rectangle (data->region, &rect);
  if (cairo_region_num_rectangles (data->region) > MAX_RECTS_IN_DIFF)
    return GSK_DIFF_ABORTED;

  return GSK_DIFF_OK;
}

static GskDiffSettings *
gsk_container_node_get_diff_settings (void)
{
  static GskDiffSettings *settings = NULL;

  if (G_LIKELY (settings))
    return settings;

  settings = gsk_diff_settings_new (gsk_container_node_compare_func,
                                    gsk_container_node_keep_func,
                                    gsk_container_node_change_func,
                                    gsk_container_node_change_func);
  gsk_diff_settings_set_allow_abort (settings, TRUE);

  return settings;
}

static gboolean
gsk_render_node_diff_multiple (GskRenderNode **nodes1,
                               gsize           n_nodes1,
                               GskRenderNode **nodes2,
                               gsize           n_nodes2,
                               GskDiffData    *data)
{
  return gsk_diff ((gconstpointer *) nodes1, n_nodes1,
                   (gconstpointer *) nodes2, n_nodes2,
                   gsk_container_node_get_diff_settings (),
                   data) == GSK_DIFF_OK;
}

void
gsk_container_node_diff_with (GskRenderNode *container,
                              GskRenderNode *other,
                              GskDiffData   *data)
{
  GskContainerNode *self = (GskContainerNode *) container;

  if (gsk_render_node_diff_multiple (self->children,
                                     self->n_children,
                                     &other,
                                     1,
                                     data))
    return;

  gsk_render_node_diff_impossible (container, other, data);
}

static void
gsk_container_node_diff (GskRenderNode *node1,
                         GskRenderNode *node2,
                         GskDiffData   *data)
{
  GskContainerNode *self1 = (GskContainerNode *) node1;
  GskContainerNode *self2 = (GskContainerNode *) node2;

  if (gsk_render_node_diff_multiple (self1->children,
                                     self1->n_children,
                                     self2->children,
                                     self2->n_children,
                                     data))
    return;

  gsk_render_node_diff_impossible (node1, node2, data);
}

static GskRenderNode *
gsk_container_node_replay (GskRenderNode   *node,
                           GskRenderReplay *replay)
{
  GskContainerNode *self = (GskContainerNode *) node;
  GskRenderNode *result;
  GPtrArray *array;
  gboolean changed;
  guint i;

  array = g_ptr_array_new_with_free_func ((GDestroyNotify) gsk_render_node_unref);
  changed = FALSE;

  for (i = 0; i < self->n_children; i++)
    {
      GskRenderNode *replayed = gsk_render_replay_filter_node (replay, self->children[i]);

      if (replayed != self->children[i])
        changed = TRUE;

      if (replayed != NULL)
        g_ptr_array_add (array, replayed);
    }

  if (!changed)
    result = gsk_render_node_ref (node);
  else
    result = gsk_container_node_new ((GskRenderNode **) array->pdata, array->len);

  g_ptr_array_unref (array);

  return result;
}

static gboolean
gsk_container_node_get_opaque_rect (GskRenderNode   *node,
                                    graphene_rect_t *opaque)
{
  GskContainerNode *self = (GskContainerNode *) node;

  if (self->opaque.size.width <= 0 && self->opaque.size.height <= 0)
    return FALSE;

  *opaque = self->opaque;
  return TRUE;
}

static void
gsk_container_node_class_init (gpointer g_class,
                               gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_CONTAINER_NODE;

  node_class->finalize = gsk_container_node_finalize;
  node_class->draw = gsk_container_node_draw;
  node_class->diff = gsk_container_node_diff;
  node_class->replay = gsk_container_node_replay;
  node_class->get_opaque_rect = gsk_container_node_get_opaque_rect;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskContainerNode, gsk_container_node)

/**
 * gsk_container_node_new:
 * @children: (array length=n_children) (transfer none): The children of the node
 * @n_children: Number of children in the @children array
 *
 * Creates a new `GskRenderNode` instance for holding the given @children.
 *
 * The new node will acquire a reference to each of the children.
 *
 * Returns: (transfer full) (type GskContainerNode): the new `GskRenderNode`
 */
GskRenderNode *
gsk_container_node_new (GskRenderNode **children,
                        guint           n_children)
{
  GskContainerNode *self;
  GskRenderNode *node;

  self = gsk_render_node_alloc (GSK_TYPE_CONTAINER_NODE);
  node = (GskRenderNode *) self;

  self->disjoint = TRUE;
  self->n_children = n_children;

  if (n_children == 0)
    {
      gsk_rect_init_from_rect (&node->bounds, graphene_rect_zero ());
      node->preferred_depth = GDK_MEMORY_NONE;
    }
  else
    {
      graphene_rect_t child_opaque;
      gboolean have_opaque;

      self->children = g_malloc_n (n_children, sizeof (GskRenderNode *));

      self->children[0] = gsk_render_node_ref (children[0]);
      node->preferred_depth = children[0]->preferred_depth;
      gsk_rect_init_from_rect (&node->bounds, &(children[0]->bounds));
      have_opaque = gsk_render_node_get_opaque_rect (children[0], &self->opaque);
      node->is_hdr = gsk_render_node_is_hdr (children[0]);
      node->clears_background = gsk_render_node_clears_background (children[0]);
      node->copy_mode = gsk_render_node_get_copy_mode (children[0]);

      for (guint i = 1; i < n_children; i++)
        {
          self->children[i] = gsk_render_node_ref (children[i]);
          self->disjoint = self->disjoint && !gsk_rect_intersects (&node->bounds, &(children[i]->bounds));
          graphene_rect_union (&node->bounds, &(children[i]->bounds), &node->bounds);
          node->preferred_depth = gdk_memory_depth_merge (node->preferred_depth, children[i]->preferred_depth);
          if (gsk_render_node_clears_background (children[i]))
            {
              node->clears_background  = TRUE;
              if (!children[i]->fully_opaque && have_opaque)
                {
                  if (!gsk_rect_subtract (&self->opaque, &children[i]->bounds, &self->opaque))
                    {
                      have_opaque = FALSE;
                      self->opaque = GRAPHENE_RECT_INIT (0, 0, 0, 0);
                    }
                }
            }
          if (gsk_render_node_get_opaque_rect (children[i], &child_opaque))
            {
              if (have_opaque)
                gsk_rect_coverage (&self->opaque, &child_opaque, &self->opaque);
              else
                {
                  self->opaque = child_opaque;
                  have_opaque = TRUE;
                }
            }

          node->is_hdr |= gsk_render_node_is_hdr (children[i]);
          node->copy_mode = MAX (node->copy_mode, gsk_render_node_get_copy_mode (children[i]));
        }

      node->fully_opaque = have_opaque && graphene_rect_equal (&node->bounds, &self->opaque);
   }

  return node;
}

/**
 * gsk_container_node_get_n_children:
 * @node: (type GskContainerNode): a container `GskRenderNode`
 *
 * Retrieves the number of direct children of @node.
 *
 * Returns: the number of children of the `GskRenderNode`
 */
guint
gsk_container_node_get_n_children (const GskRenderNode *node)
{
  const GskContainerNode *self = (const GskContainerNode *) node;

  return self->n_children;
}

/**
 * gsk_container_node_get_child:
 * @node: (type GskContainerNode): a container `GskRenderNode`
 * @idx: the position of the child to get
 *
 * Gets one of the children of @container.
 *
 * Returns: (transfer none): the @idx'th child of @container
 */
GskRenderNode *
gsk_container_node_get_child (const GskRenderNode *node,
                              guint                idx)
{
  const GskContainerNode *self = (const GskContainerNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_CONTAINER_NODE), NULL);
  g_return_val_if_fail (idx < self->n_children, NULL);

  return self->children[idx];
}

GskRenderNode **
gsk_container_node_get_children (const GskRenderNode *node,
                                 guint               *n_children)
{
  const GskContainerNode *self = (const GskContainerNode *) node;

  *n_children = self->n_children;

  return self->children;
}

/*< private>
 * gsk_container_node_is_disjoint:
 * @node: a container `GskRenderNode`
 *
 * Returns `TRUE` if it is known that the child nodes are not
 * overlapping. There is no guarantee that they do overlap
 * if this function return FALSE.
 *
 * Returns: `TRUE` if children don't overlap
 */
gboolean
gsk_container_node_is_disjoint (const GskRenderNode *node)
{
  const GskContainerNode *self = (const GskContainerNode *) node;

  return self->disjoint;
}

