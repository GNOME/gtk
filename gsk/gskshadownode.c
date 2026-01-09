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

#include "gskshadownodeprivate.h"

#include "gskrenderreplay.h"

#include "gskcairoblurprivate.h"
#include "gskrectprivate.h"
#include "gskrendernodeprivate.h"

#include "gdk/gdkcairoprivate.h"

G_LOCK_DEFINE_STATIC (rgba);

/**
 * GskShadowNode:
 *
 * A render node drawing one or more shadows behind its single child node.
 */
struct _GskShadowNode
{
  GskRenderNode render_node;

  GskRenderNode *child;

  gsize n_shadows;
  GskShadowEntry *shadows;

  GskShadow *rgba_shadows;
};

static void
gsk_shadow_node_finalize (GskRenderNode *node)
{
  GskShadowNode *self = (GskShadowNode *) node;
  GskRenderNodeClass *parent_class = g_type_class_peek (g_type_parent (GSK_TYPE_SHADOW_NODE));

  gsk_render_node_unref (self->child);

  for (gsize i = 0; i < self->n_shadows; i++)
    gdk_color_finish (&self->shadows[i].color);
  g_free (self->shadows);

  g_free (self->rgba_shadows);

  parent_class->finalize (node);
}

static void
gsk_shadow_node_draw (GskRenderNode *node,
                      cairo_t       *cr,
                      GskCairoData  *data)
{
  GskShadowNode *self = (GskShadowNode *) node;
  gsize i;

  /* clip so the blur area stays small */
  gdk_cairo_rectangle_snap_to_grid (cr, &node->bounds);
  cairo_clip (cr);
  if (gdk_cairo_is_all_clipped (cr))
    return;

  for (i = 0; i < self->n_shadows; i++)
    {
      GskShadowEntry *shadow = &self->shadows[i];
      cairo_pattern_t *pattern;

      /* We don't need to draw invisible shadows */
      if (gdk_color_is_clear (&shadow->color))
        continue;

      cairo_save (cr);
      cr = gsk_cairo_blur_start_drawing (cr, 0.5 * shadow->radius, GSK_BLUR_X | GSK_BLUR_Y);

      cairo_save (cr);
      cairo_translate (cr, shadow->offset.x, shadow->offset.y);
      cairo_push_group (cr);
      gsk_render_node_draw_full (self->child, cr, data);
      pattern = cairo_pop_group (cr);
      cairo_reset_clip (cr);
      gdk_cairo_set_source_color (cr, data->ccs, &shadow->color);
      cairo_mask (cr, pattern);
      cairo_pattern_destroy (pattern);
      cairo_restore (cr);

      cr = gsk_cairo_blur_finish_drawing (cr, data->ccs, 0.5 * shadow->radius, &shadow->color, GSK_BLUR_X | GSK_BLUR_Y);
      cairo_restore (cr);
    }

  gsk_render_node_draw_full (self->child, cr, data);
}

static void
gsk_shadow_node_diff (GskRenderNode *node1,
                      GskRenderNode *node2,
                      GskDiffData   *data)
{
  GskShadowNode *self1 = (GskShadowNode *) node1;
  GskShadowNode *self2 = (GskShadowNode *) node2;
  int top = 0, right = 0, bottom = 0, left = 0;
  cairo_region_t *sub;
  cairo_rectangle_int_t rect;
  gsize i, n;

  if (self1->n_shadows != self2->n_shadows)
    {
      gsk_render_node_diff_impossible (node1, node2, data);
      return;
    }

  for (i = 0; i < self1->n_shadows; i++)
    {
      GskShadowEntry *shadow1 = &self1->shadows[i];
      GskShadowEntry *shadow2 = &self2->shadows[i];
      float clip_radius;

      if (!gdk_color_equal (&shadow1->color, &shadow2->color) ||
          !graphene_point_equal (&shadow1->offset, &shadow2->offset) ||
          shadow1->radius != shadow2->radius)
        {
          gsk_render_node_diff_impossible (node1, node2, data);
          return;
        }

      clip_radius = gsk_cairo_blur_compute_pixels (shadow1->radius / 2.0);
      top = MAX (top, ceil (clip_radius - shadow1->offset.y));
      right = MAX (right, ceil (clip_radius + shadow1->offset.x));
      bottom = MAX (bottom, ceil (clip_radius + shadow1->offset.y));
      left = MAX (left, ceil (clip_radius - shadow1->offset.x));
    }

  sub = cairo_region_create ();
  gsk_render_node_diff (self1->child, self2->child, &(GskDiffData) { sub, data->copies, data->surface });

  n = cairo_region_num_rectangles (sub);
  for (i = 0; i < n; i++)
    {
      cairo_region_get_rectangle (sub, i, &rect);
      rect.x -= left;
      rect.y -= top;
      rect.width += left + right;
      rect.height += top + bottom;
      cairo_region_union_rectangle (data->region, &rect);
    }
  cairo_region_destroy (sub);
}

static void
gsk_shadow_node_get_bounds (GskShadowNode *self,
                            graphene_rect_t *bounds)
{
  float top = 0, right = 0, bottom = 0, left = 0;
  gsize i;

  gsk_rect_init_from_rect (bounds, &self->child->bounds);

  for (i = 0; i < self->n_shadows; i++)
    {
      float clip_radius = gsk_cairo_blur_compute_pixels (self->shadows[i].radius / 2.0);
      top = MAX (top, clip_radius - self->shadows[i].offset.y);
      right = MAX (right, clip_radius + self->shadows[i].offset.x);
      bottom = MAX (bottom, clip_radius + self->shadows[i].offset.y);
      left = MAX (left, clip_radius - self->shadows[i].offset.x);
    }

  bounds->origin.x -= left;
  bounds->origin.y -= top;
  bounds->size.width += left + right;
  bounds->size.height += top + bottom;
}

static GskRenderNode **
gsk_shadow_node_get_children (GskRenderNode *node,
                              gsize         *n_children)
{
  GskShadowNode *self = (GskShadowNode *) node;

  *n_children = 1;
  
  return &self->child;
}

static GskRenderNode *
gsk_shadow_node_replay (GskRenderNode   *node,
                        GskRenderReplay *replay)
{
  GskShadowNode *self = (GskShadowNode *) node;
  GskRenderNode *result, *child;

  child = gsk_render_replay_filter_node (replay, self->child);

  if (child == NULL)
    return NULL;

  if (child == self->child)
    result = gsk_render_node_ref (node);
  else
    result = gsk_shadow_node_new2 (child, self->shadows, self->n_shadows);

  gsk_render_node_unref (child);

  return result;
}

static void
gsk_shadow_node_class_init (gpointer g_class,
                            gpointer class_data)
{
  GskRenderNodeClass *node_class = g_class;

  node_class->node_type = GSK_SHADOW_NODE;

  node_class->finalize = gsk_shadow_node_finalize;
  node_class->draw = gsk_shadow_node_draw;
  node_class->diff = gsk_shadow_node_diff;
  node_class->get_children = gsk_shadow_node_get_children;
  node_class->replay = gsk_shadow_node_replay;
}

GSK_DEFINE_RENDER_NODE_TYPE (GskShadowNode, gsk_shadow_node)

/**
 * gsk_shadow_node_new:
 * @child: The node to draw
 * @shadows: (array length=n_shadows): The shadows to apply
 * @n_shadows: number of entries in the @shadows array
 *
 * Creates a `GskRenderNode` that will draw a @child with the given
 * @shadows below it.
 *
 * Returns: (transfer full) (type GskShadowNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_shadow_node_new (GskRenderNode   *child,
                     const GskShadow *shadows,
                     gsize            n_shadows)
{
  GskShadowEntry *shadows2;
  GskRenderNode *node;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (shadows != NULL, NULL);
  g_return_val_if_fail (n_shadows > 0, NULL);

  shadows2 = g_new (GskShadowEntry, n_shadows);
  for (gsize i = 0; i < n_shadows; i++)
    {
      gdk_color_init_from_rgba (&shadows2[i].color, &shadows[i].color);
      graphene_point_init (&shadows2[i].offset, shadows[i].dx, shadows[i].dy);
      shadows2[i].radius = shadows[i].radius;
    }

  node = gsk_shadow_node_new2 (child, shadows2, n_shadows);

  for (gsize i = 0; i < n_shadows; i++)
    gdk_color_finish (&shadows2[i].color);
  g_free (shadows2);

  return node;
}

/*< private >
 * gsk_shadow_node_new2:
 * @child: The node to draw
 * @shadows: (array length=n_shadows): The shadows to apply
 * @n_shadows: number of entries in the @shadows array
 *
 * Creates a `GskRenderNode` that will draw a @child with the given
 * @shadows below it.
 *
 * Returns: (transfer full) (type GskShadowNode): A new `GskRenderNode`
 */
GskRenderNode *
gsk_shadow_node_new2 (GskRenderNode        *child,
                      const GskShadowEntry *shadows,
                      gsize                 n_shadows)
{
  GskShadowNode *self;
  GskRenderNode *node;
  gsize i;
  gboolean is_hdr;

  g_return_val_if_fail (GSK_IS_RENDER_NODE (child), NULL);
  g_return_val_if_fail (shadows != NULL, NULL);
  g_return_val_if_fail (n_shadows > 0, NULL);

  self = gsk_render_node_alloc (GSK_TYPE_SHADOW_NODE);
  node = (GskRenderNode *) self;

  self->child = gsk_render_node_ref (child);
  self->n_shadows = n_shadows;
  self->shadows = g_new (GskShadowEntry, n_shadows);

  is_hdr = gsk_render_node_is_hdr (child);

  for (i = 0; i < n_shadows; i++)
    {
      gdk_color_init_copy (&self->shadows[i].color, &shadows[i].color);
      graphene_point_init_from_point (&self->shadows[i].offset, &shadows[i].offset);
      self->shadows[i].radius = shadows[i].radius;
      is_hdr = is_hdr || gdk_color_is_srgb (&shadows[i].color);
    }

  node->preferred_depth = gsk_render_node_get_preferred_depth (child);
  node->is_hdr = is_hdr;
  node->contains_subsurface_node = gsk_render_node_contains_subsurface_node (child);
  node->contains_paste_node = gsk_render_node_contains_paste_node (child);

  gsk_shadow_node_get_bounds (self, &node->bounds);

  return node;
}

/**
 * gsk_shadow_node_get_child:
 * @node: (type GskShadowNode): a shadow `GskRenderNode`
 *
 * Retrieves the child `GskRenderNode` of the shadow @node.
 *
 * Returns: (transfer none): the child render node
 */
GskRenderNode *
gsk_shadow_node_get_child (const GskRenderNode *node)
{
  const GskShadowNode *self = (const GskShadowNode *) node;

  return self->child;
}

/**
 * gsk_shadow_node_get_shadow:
 * @node: (type GskShadowNode): a shadow `GskRenderNode`
 * @i: the given index
 *
 * Retrieves the shadow data at the given index @i.
 *
 * Returns: (transfer none): the shadow data
 */
const GskShadow *
gsk_shadow_node_get_shadow (const GskRenderNode *node,
                            gsize                i)
{
  GskShadowNode *self = (GskShadowNode *) node;
  const GskShadow *shadow;

  G_LOCK (rgba);

  if (self->rgba_shadows == NULL)
    {
      self->rgba_shadows = g_new (GskShadow, self->n_shadows);
      for (gsize j = 0; j < self->n_shadows; j++)
        {
          gdk_color_to_float (&self->shadows[j].color,
                              GDK_COLOR_STATE_SRGB,
                              (float *) &self->rgba_shadows[j].color);
          self->rgba_shadows[j].dx = self->shadows[j].offset.x;
          self->rgba_shadows[j].dy = self->shadows[j].offset.y;
          self->rgba_shadows[j].radius = self->shadows[j].radius;
        }
    }

  shadow = &self->rgba_shadows[i];

  G_UNLOCK (rgba);

  return shadow;
}

/*< private >
 * gsk_shadow_node_get_shadow_entry:
 * @node: (type GskShadowNode): a shadow `GskRenderNode`
 * @i: the given index
 *
 * Retrieves the shadow data at the given index @i.
 *
 * Returns: (transfer none): the shadow data
 */
const GskShadowEntry *
gsk_shadow_node_get_shadow_entry (const GskRenderNode *node,
                                  gsize                i)
{
  const GskShadowNode *self = (const GskShadowNode *) node;

  return &self->shadows[i];
}

/**
 * gsk_shadow_node_get_n_shadows:
 * @node: (type GskShadowNode): a shadow `GskRenderNode`
 *
 * Retrieves the number of shadows in the @node.
 *
 * Returns: the number of shadows.
 */
gsize
gsk_shadow_node_get_n_shadows (const GskRenderNode *node)
{
  const GskShadowNode *self = (const GskShadowNode *) node;

  return self->n_shadows;
}
