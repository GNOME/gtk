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

#include "gskstrokenode.h"

#include "gskcolornodeprivate.h"
#include "gskrendernodeprivate.h"
#include "gskpathprivate.h"
#include "gskcontourprivate.h"
#include "gskrectprivate.h"
#include "gskrenderreplay.h"
#include "gskstrokeprivate.h"

#include "gdk/gdkcairoprivate.h"

/**
 * GskStrokeNode:
 *
 * A render node that will fill the area determined by stroking the the given
 * [struct@Gsk.Path] using the [struct@Gsk.Stroke] attributes.
 *
 * Since: 4.14
 */

struct _GskStrokeNode
{
  GskRenderNode render_node;

  GskRenderNode *child;
  GskPath *path;
  GskStroke stroke;
};

static void
gsk_stroke_node_finalize (GskRenderNode *node)
{
  GskStrokeNode *self = (GskStrokeNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_STROKE_NODE));

  gsk_render_node_unref (self->child);
  gsk_path_unref (self->path);
  gsk_stroke_clear (&self->stroke);

  parent_class->finalize (node);
}

void
gsk_cairo_stroke_path (cairo_t   *cr,
                       GskPath   *path,
                       GskStroke *stroke)
{
  gsk_stroke_to_cairo (stroke, cr);
  gsk_path_to_cairo (path, cr);
  cairo_stroke (cr);

  /* Cairo draws caps for zero-length subpaths with round caps,
   * but not square. So we do it ourselves.
   */
  if (gsk_stroke_get_line_cap (stroke) == GSK_LINE_CAP_SQUARE)
    {
      double width = gsk_stroke_get_line_width (stroke);

      for (size_t i = 0; i < gsk_path_get_n_contours (path); i++)
        {
          const GskContour *c = gsk_path_get_contour (path, i);

          if ((gsk_contour_get_flags (c) & GSK_PATH_ZERO_LENGTH) != 0)
            {
              GskPathPoint point = { .contour = i, .idx = 0, .t = 0 };
              graphene_point_t p;

              gsk_contour_get_position (c, &point, &p);
              cairo_rectangle (cr,
                               p.x - width / 2,
                               p.y - width / 2,
                               width,
                               width);
              cairo_fill (cr);
            }
        }
    }
}

static void
gsk_stroke_node_draw (GskRenderNode *node,
                      cairo_t       *cr,
                      GskCairoData  *data)
{
  GskStrokeNode *self = (GskStrokeNode *) node;

  if (gsk_render_node_get_node_type (self->child) == GSK_COLOR_NODE &&
      gsk_rect_contains_rect (&self->child->bounds, &node->bounds))
    {
      gdk_cairo_set_source_rgba_ccs (cr, data->ccs, gsk_color_node_get_color (self->child));
    }
  else
    {
      gdk_cairo_rectangle_snap_to_grid (cr, &self->child->bounds);
      cairo_clip (cr);
      if (gdk_cairo_is_all_clipped (cr))
        return;

      cairo_push_group (cr);
      gsk_render_node_draw_full (self->child, cr, data);
      cairo_pop_group_to_source (cr);
    }

  gsk_cairo_stroke_path (cr, self->path, &self->stroke);
}

static void
gsk_stroke_node_diff (GskRenderNode *node1,
                      GskRenderNode *node2,
                      GskDiffData   *data)
{
  GskStrokeNode *self1 = (GskStrokeNode *) node1;
  GskStrokeNode *self2 = (GskStrokeNode *) node2;

  if (self1->path == self2->path &&
      gsk_stroke_equal (&self1->stroke, &self2->stroke))
    {
      cairo_region_t *save;
      cairo_rectangle_int_t clip_rect;

      save = cairo_region_copy (data->region);
      gsk_render_node_diff (self1->child, self2->child, data);
      gsk_rect_to_cairo_grow (&node1->bounds, &clip_rect);
      cairo_region_intersect_rectangle (data->region, &clip_rect);
      cairo_region_union (data->region, save);
      cairo_region_destroy (save);
    }
  else
    {
      gsk_render_node_diff_impossible (node1, node2, data);
    }
}

static GskRenderNode **
gsk_stroke_node_get_children (GskRenderNode *node,
                              gsize         *n_children)
{
  GskStrokeNode *self = (GskStrokeNode *) node;

  *n_children = 1;
  
  return &self->child;
}

static GskRenderNode *
gsk_stroke_node_replay (GskRenderNode   *node,
                        GskRenderReplay *replay)
{
  GskStrokeNode *self = (GskStrokeNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_stroke_node_new (child, self->path, &self->stroke);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_stroke_node_class_init (gpointer g_class,
                            gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_STROKE_NODE;

  node_class->finalize = gsk_stroke_node_finalize;
  node_class->draw = gsk_stroke_node_draw;
  node_class->diff = gsk_stroke_node_diff;
  node_class->get_children = gsk_stroke_node_get_children;
  node_class->replay = gsk_stroke_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskStrokeNode, gsk_stroke_node)

/**
 * gsk_stroke_node_new:
 * @child: The node to stroke the area with
 * @path: (transfer none): The path describing the area to stroke
 * @stroke: (transfer none): The stroke attributes to use
 *
 * Creates a #GskRenderNode that will fill the outline generated by stroking
 * the given @path using the attributes defined in @stroke.
 *
 * The area is filled with @child.
 *
 * GSK aims to follow the SVG semantics for stroking paths.
 * E.g. zero-length contours will get round or square line
 * caps drawn, regardless whether they are closed or not.
 *
 * Returns: (transfer none) (type GskStrokeNode): A new #GskRenderNode
 *
 * Since: 4.14
 */
GskRenderNode *
gsk_stroke_node_new (GskRenderNode   *child,
                     GskPath         *path,
                     const GskStroke *stroke)
{
  GskStrokeNode *self;
  GskRenderNode *node;
  graphene_rect_t stroke_bounds;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (path != NULL, NULL);
  g_return_val_if_fail (stroke != NULL, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_STROKE_NODE);
  node = (GskRenderNode *) self;
  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = gsk_render_node_is_hdr (child);
  node->clears_background = gsk_render_node_clears_background (child);
  node->copy_mode = gsk_render_node_get_copy_mode (child);
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child);

  self->child = gsk_render_node_ref (child);
  self->path = gsk_path_ref (path);
  self->stroke = GSK_STROKE_INIT_COPY (stroke);

  if (gsk_path_get_stroke_bounds (self->path, &self->stroke, &stroke_bounds))
    gsk_rect_intersection (&stroke_bounds, &child->bounds, &node->bounds);
  else
    gsk_rect_init (&node->bounds, 0.f, 0.f, 0.f, 0.f);

  return node;
}

/**
 * gsk_stroke_node_get_child:
 * @node: (type GskStrokeNode): a stroke #GskRenderNode
 *
 * Gets the child node that is getting drawn by the given @node.
 *
 * Returns: (transfer none): The child that is getting drawn
 *
 * Since: 4.14
 */
GskRenderNode *
gsk_stroke_node_get_child (const GskRenderNode *node)
{
  const GskStrokeNode *self = (const GskStrokeNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_STROKE_NODE), NULL);

  return self->child;
}

/**
 * gsk_stroke_node_get_path:
 * @node: (type GskStrokeNode): a stroke #GskRenderNode
 *
 * Retrieves the path that will be stroked with the contents of
 * the @node.
 *
 * Returns: (transfer none): a #GskPath
 *
 * Since: 4.14
 */
GskPath *
gsk_stroke_node_get_path (const GskRenderNode *node)
{
  const GskStrokeNode *self = (const GskStrokeNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_STROKE_NODE), NULL);

  return self->path;
}

/**
 * gsk_stroke_node_get_stroke:
 * @node: (type GskStrokeNode): a stroke #GskRenderNode
 *
 * Retrieves the stroke attributes used in this @node.
 *
 * Returns: a #GskStroke
 *
 * Since: 4.14
 */
const GskStroke *
gsk_stroke_node_get_stroke (const GskRenderNode *node)
{
  const GskStrokeNode *self = (const GskStrokeNode *) node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE_TYPE (node, GSK_STROKE_NODE), NULL);

  return &self->stroke;
}

