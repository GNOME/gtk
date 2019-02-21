/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
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

#include "gtksnapshot.h"
#include "gtksnapshotprivate.h"

#include "gtkcssrgbavalueprivate.h"
#include "gtkcssshadowsvalueprivate.h"
#include "gtkdebug.h"
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkrendericonprivate.h"
#include "gtkrendernodepaintableprivate.h"
#include "gtkstylecontextprivate.h"
#include "gtktransformprivate.h"

#include "gsk/gskrendernodeprivate.h"

#include "gtk/gskpango.h"


/**
 * SECTION:gtksnapshot
 * @Short_description: Auxiliary object for snapshots
 * @Title: GtkSnapshot
 *
 * GtkSnapshot is an auxiliary object that assists in creating #GskRenderNodes
 * in the #GtkWidget::snapshot vfunc. It functions in a similar way to
 * a cairo context, and maintains a stack of render nodes and their associated
 * transformations.
 *
 * The node at the top of the stack is the the one that gtk_snapshot_append_…
 * functions operate on. Use the gtk_snapshot_push_… functions and gtk_snapshot_pop()
 * to change the current node.
 *
 * The typical way to obtain a GtkSnapshot object is as an argument to
 * the #GtkWidget::snapshot vfunc. If you need to create your own GtkSnapshot,
 * use gtk_snapshot_new().
 */

G_DEFINE_TYPE (GtkSnapshot, gtk_snapshot, GDK_TYPE_SNAPSHOT)

static void
gtk_snapshot_dispose (GObject *object)
{
  GtkSnapshot *snapshot = GTK_SNAPSHOT (object);

  if (snapshot->state_stack)
    gsk_render_node_unref (gtk_snapshot_to_node (snapshot));

  g_assert (snapshot->state_stack == NULL);
  g_assert (snapshot->nodes == NULL);

  G_OBJECT_CLASS (gtk_snapshot_parent_class)->dispose (object);
}

static void
gtk_snapshot_class_init (GtkSnapshotClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = gtk_snapshot_dispose;
}

static void
gtk_snapshot_init (GtkSnapshot *self)
{
}

static GskRenderNode *
gtk_snapshot_collect_default (GtkSnapshot       *snapshot,
                              GtkSnapshotState  *state,
                              GskRenderNode    **nodes,
                              guint              n_nodes)
{
  GskRenderNode *node;

  if (n_nodes == 0)
    {
      node = NULL;
    }
  else if (n_nodes == 1)
    {
      node = gsk_render_node_ref (nodes[0]);
    }
  else
    {
      node = gsk_container_node_new (nodes, n_nodes);
    }

  return node;
}

static GtkSnapshotState *
gtk_snapshot_push_state (GtkSnapshot            *snapshot,
                         GtkTransform           *transform,
                         GtkSnapshotCollectFunc  collect_func)
{
  const gsize n_states = snapshot->state_stack->len;
  GtkSnapshotState *state;

  g_array_set_size (snapshot->state_stack, n_states + 1);
  state = &g_array_index (snapshot->state_stack, GtkSnapshotState, n_states);

  state->transform = gtk_transform_ref (transform);
  state->collect_func = collect_func;
  state->start_node_index = snapshot->nodes->len;
  state->n_nodes = 0;

  return state;
}

static GtkSnapshotState *
gtk_snapshot_get_current_state (const GtkSnapshot *snapshot)
{
  g_assert (snapshot->state_stack->len > 0);

  return &g_array_index (snapshot->state_stack, GtkSnapshotState, snapshot->state_stack->len - 1);
}

static GtkSnapshotState *
gtk_snapshot_get_previous_state (const GtkSnapshot *snapshot)
{
  g_assert (snapshot->state_stack->len > 1);

  return &g_array_index (snapshot->state_stack, GtkSnapshotState, snapshot->state_stack->len - 2);
}

static void
gtk_snapshot_state_clear (GtkSnapshotState *state)
{
  gtk_transform_unref (state->transform);
}

/**
 * gtk_snapshot_new:
 *
 * Creates a new #GtkSnapshot.
 *
 * Returns: a newly-allocated #GtkSnapshot
 */
GtkSnapshot *
gtk_snapshot_new (void)
{
  GtkSnapshot *snapshot;

  snapshot = g_object_new (GTK_TYPE_SNAPSHOT, NULL);

  snapshot->from_parent = FALSE;
  snapshot->state_stack = g_array_new (FALSE, TRUE, sizeof (GtkSnapshotState));
  g_array_set_clear_func (snapshot->state_stack, (GDestroyNotify)gtk_snapshot_state_clear);
  snapshot->nodes = g_ptr_array_new_with_free_func ((GDestroyNotify)gsk_render_node_unref);

  gtk_snapshot_push_state (snapshot,
                           NULL,
                           gtk_snapshot_collect_default);

  return snapshot;
}

/* Private. Does the same as _new but does not allocate a NEW
 * state/node stack. */
GtkSnapshot *
gtk_snapshot_new_with_parent (GtkSnapshot *parent_snapshot)
{
  GtkSnapshot *snapshot = g_object_new (GTK_TYPE_SNAPSHOT, NULL);

  snapshot->state_stack = parent_snapshot->state_stack;
  snapshot->nodes = parent_snapshot->nodes;
  snapshot->from_parent = TRUE;

  gtk_snapshot_push_state (snapshot,
                           NULL,
                           gtk_snapshot_collect_default);

  return snapshot;
}

/**
 * gtk_snapshot_free_to_node: (skip)
 * @snapshot: (transfer full): a #GtkSnapshot
 *
 * Returns the node that was constructed by @snapshot
 * and frees @snapshot.
 *
 * Returns: (transfer full): a newly-created #GskRenderNode
 */
GskRenderNode *
gtk_snapshot_free_to_node (GtkSnapshot *snapshot)
{
  GskRenderNode *result;

  result = gtk_snapshot_to_node (snapshot);
  g_object_unref (snapshot);

  return result;
}

/**
 * gtk_snapshot_free_to_paintable: (skip)
 * @snapshot: (transfer full): a #GtkSnapshot
 * @size: (allow-none): The size of the resulting paintable
 *     or %NULL to use the bounds of the snapshot
 *
 * Returns a paintable for the node that was
 * constructed by @snapshot and frees @snapshot.
 *
 * Returns: (transfer full): a newly-created #GdkPaintable
 */
GdkPaintable *
gtk_snapshot_free_to_paintable (GtkSnapshot           *snapshot,
                                const graphene_size_t *size)
{
  GdkPaintable *result;

  result = gtk_snapshot_to_paintable (snapshot, size);
  g_object_unref (snapshot);

  return result;
}

static GskRenderNode *
gtk_snapshot_collect_autopush_transform (GtkSnapshot      *snapshot,
                                         GtkSnapshotState *state,
                                         GskRenderNode   **nodes,
                                         guint             n_nodes)
{
  GskRenderNode *node, *transform_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  transform_node = gsk_transform_node_new_with_category (node,
                                                         &state->data.transform.transform,
                                                         state->data.transform.category);

  gsk_render_node_unref (node);

  return transform_node;
}

static void
gtk_snapshot_autopush_transform (GtkSnapshot *snapshot)
{
  GtkSnapshotState *previous_state;
  GtkSnapshotState *state;

  state = gtk_snapshot_push_state (snapshot,
                                   NULL,
                                   gtk_snapshot_collect_autopush_transform);

  previous_state = gtk_snapshot_get_previous_state (snapshot);

  gtk_transform_to_matrix (previous_state->transform, &state->data.transform.transform);
  state->data.transform.category = gtk_transform_categorize (previous_state->transform);
}

static gboolean
gtk_snapshot_state_should_autopop (const GtkSnapshotState *state)
{
  return state->collect_func == NULL ||
         state->collect_func == gtk_snapshot_collect_autopush_transform;
}

static GskRenderNode *
gtk_snapshot_collect_debug (GtkSnapshot      *snapshot,
                            GtkSnapshotState *state,
                            GskRenderNode   **nodes,
                            guint             n_nodes)
{
  GskRenderNode *node, *debug_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  if (state->data.debug.message == NULL)
    return node;

  debug_node = gsk_debug_node_new (node, state->data.debug.message);

  gsk_render_node_unref (node);

  return debug_node;
}

void
gtk_snapshot_push_debug (GtkSnapshot *snapshot,
                         const char  *message,
                         ...)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->transform,
                                   gtk_snapshot_collect_debug);

  if (GTK_DEBUG_CHECK (SNAPSHOT))
    {
      va_list args;

      va_start (args, message);
      state->data.debug.message = g_strdup_vprintf (message, args);
      va_end (args);
    }
  else
    {
      state->data.debug.message = NULL;
    }
}

static GskRenderNode *
gtk_snapshot_collect_transform (GtkSnapshot      *snapshot,
                                GtkSnapshotState *state,
                                GskRenderNode   **nodes,
                                guint             n_nodes)
{
  GskRenderNode *node, *transform_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  transform_node = gsk_transform_node_new_with_category (node,
                                                         &state->data.transform.transform,
                                                         state->data.transform.category);

  gsk_render_node_unref (node);

  return transform_node;
}

void
gtk_snapshot_push_transform (GtkSnapshot             *snapshot,
                             const graphene_matrix_t *transform)
{
  gtk_snapshot_push_transform_with_category (snapshot,
                                             transform,
                                             GSK_MATRIX_CATEGORY_UNKNOWN);
}

void
gtk_snapshot_push_transform_with_category (GtkSnapshot             *snapshot,
                                           const graphene_matrix_t *transform,
                                           GskMatrixCategory        category)
{
  GtkSnapshotState *previous_state;
  GtkSnapshotState *state;
  graphene_matrix_t offset;

  state = gtk_snapshot_push_state (snapshot,
                                   NULL,
                                   gtk_snapshot_collect_transform);

  previous_state = gtk_snapshot_get_previous_state (snapshot);

  gtk_transform_to_matrix (previous_state->transform, &offset);

  graphene_matrix_multiply (transform, &offset, &state->data.transform.transform);
  state->data.transform.category = MIN (gtk_transform_categorize (previous_state->transform), category);
}

static GskRenderNode *
gtk_snapshot_collect_opacity (GtkSnapshot      *snapshot,
                              GtkSnapshotState *state,
                              GskRenderNode   **nodes,
                              guint             n_nodes)
{
  GskRenderNode *node, *opacity_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  if (state->data.opacity.opacity == 1.0)
    {
      opacity_node = node;
    }
  else if (state->data.opacity.opacity == 0.0)
    {
      gsk_render_node_unref (node);
      opacity_node = NULL;
    }
  else
    {
      opacity_node = gsk_opacity_node_new (node, state->data.opacity.opacity);
      gsk_render_node_unref (node);
    }

  return opacity_node;
}

void
gtk_snapshot_push_opacity (GtkSnapshot *snapshot,
                           double       opacity)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->transform,
                                   gtk_snapshot_collect_opacity);
  state->data.opacity.opacity = CLAMP (opacity, 0.0, 1.0);
}

static GskRenderNode *
gtk_snapshot_collect_blur (GtkSnapshot      *snapshot,
                           GtkSnapshotState *state,
                           GskRenderNode   **nodes,
                           guint             n_nodes)
{
  GskRenderNode *node, *blur_node;
  double radius;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  radius = state->data.blur.radius;

  if (radius == 0.0)
    return node;

  blur_node = gsk_blur_node_new (node, radius);

  gsk_render_node_unref (node);

  return blur_node;
}

void
gtk_snapshot_push_blur (GtkSnapshot *snapshot,
                        double       radius)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->transform,
                                   gtk_snapshot_collect_blur);
  state->data.blur.radius = radius;
}

static GskRenderNode *
gtk_snapshot_collect_color_matrix (GtkSnapshot      *snapshot,
                                   GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes)
{
  GskRenderNode *node, *color_matrix_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  if (gsk_render_node_get_node_type (node) == GSK_COLOR_MATRIX_NODE)
    {
      GskRenderNode *child = gsk_render_node_ref (gsk_color_matrix_node_get_child (node));
      const graphene_matrix_t *mat1 = gsk_color_matrix_node_peek_color_matrix (node);
      graphene_matrix_t mat2;
      graphene_vec4_t offset2;

      /* color matrix node: color = mat * p + offset; for a pixel p.
       * color =  mat1 * (mat2 * p + offset2) + offset1;
       *       =  mat1 * mat2  * p + offset2 * mat1 + offset1
       *       = (mat1 * mat2) * p + (offset2 * mat1 + offset1)
       * Which this code does.
       * mat1 and offset1 come from @child.
       */

      mat2 = state->data.color_matrix.matrix;
      offset2 = state->data.color_matrix.offset;
      graphene_matrix_transform_vec4 (mat1, &offset2, &offset2);
      graphene_vec4_add (&offset2, gsk_color_matrix_node_peek_color_offset (node), &offset2);

      graphene_matrix_multiply (mat1, &mat2, &mat2);

      gsk_render_node_unref (node);
      node = NULL;
      color_matrix_node = gsk_color_matrix_node_new (child, &mat2, &offset2);

      gsk_render_node_unref (child);
    }
  else
    {
      color_matrix_node = gsk_color_matrix_node_new (node,
                                                     &state->data.color_matrix.matrix,
                                                     &state->data.color_matrix.offset);
      gsk_render_node_unref (node);
    }

  return color_matrix_node;
}

void
gtk_snapshot_push_color_matrix (GtkSnapshot             *snapshot,
                                const graphene_matrix_t *color_matrix,
                                const graphene_vec4_t   *color_offset)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->transform,
                                   gtk_snapshot_collect_color_matrix);

  graphene_matrix_init_from_matrix (&state->data.color_matrix.matrix, color_matrix);
  graphene_vec4_init_from_vec4 (&state->data.color_matrix.offset, color_offset);
}

static GskRenderNode *
gtk_snapshot_collect_repeat (GtkSnapshot      *snapshot,
                             GtkSnapshotState *state,
                             GskRenderNode   **nodes,
                             guint             n_nodes)
{
  GskRenderNode *node, *repeat_node;
  graphene_rect_t *bounds = &state->data.repeat.bounds;
  graphene_rect_t *child_bounds = &state->data.repeat.child_bounds;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  repeat_node = gsk_repeat_node_new (bounds,
                                     node,
                                     child_bounds->size.width > 0 ? child_bounds : NULL);

  gsk_render_node_unref (node);

  return repeat_node;
}

static void
gtk_graphene_rect_scale_affine (const graphene_rect_t *rect,
                                float                  scale_x,
                                float                  scale_y,
                                float                  dx,
                                float                  dy,
                                graphene_rect_t       *res)
{
  res->origin.x = scale_x * rect->origin.x + dx;
  res->origin.y = scale_y * rect->origin.y + dy;
  res->size.width = scale_x * rect->size.width;
  res->size.height = scale_y * rect->size.height;
  /* necessary when scale_x or scale_y are < 0 */
  graphene_rect_normalize (res);
}

static void
gtk_rounded_rect_scale_affine (GskRoundedRect       *dest,
                               const GskRoundedRect *src,
                               float                 scale_x,
                               float                 scale_y,
                               float                 dx,
                               float                 dy)
{
  guint flip;
  guint i;

  g_assert (dest != src);

  gtk_graphene_rect_scale_affine (&src->bounds, scale_x, scale_y, dx, dy, &dest->bounds);
  flip = ((scale_x < 0) ? 1 : 0) + (scale_y < 0 ? 2 : 0);

  for (i = 0; i < 4; i++)
    {
      dest->corner[i].width = src->corner[i ^ flip].width * fabsf (scale_x);
      dest->corner[i].height = src->corner[i ^ flip].height * fabsf (scale_y);
    }
}

static void
gtk_snapshot_ensure_affine (GtkSnapshot *snapshot,
                            float       *scale_x,
                            float       *scale_y,
                            float       *dx,
                            float       *dy)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);

  if (gtk_transform_to_affine (current_state->transform, scale_x, scale_y, dx, dy))
    return;

  gtk_snapshot_autopush_transform (snapshot);

  *scale_x = *scale_y = 1;
  *dx = *dy = 0;
}

static void
gtk_snapshot_ensure_identity (GtkSnapshot *snapshot)
{
  const GtkSnapshotState *state = gtk_snapshot_get_current_state (snapshot);

  if (state->transform == NULL)
    return;

  gtk_snapshot_autopush_transform (snapshot);
}

void
gtk_snapshot_push_repeat (GtkSnapshot           *snapshot,
                          const graphene_rect_t *bounds,
                          const graphene_rect_t *child_bounds)
{
  GtkSnapshotState *state;
  graphene_rect_t real_child_bounds = { { 0 } };
  float scale_x, scale_y, dx, dy;

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);

  if (child_bounds)
    gtk_graphene_rect_scale_affine (child_bounds, scale_x, scale_y, dx, dy, &real_child_bounds);

  state = gtk_snapshot_push_state (snapshot,
                                   gtk_snapshot_get_current_state (snapshot)->transform,
                                   gtk_snapshot_collect_repeat);

  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &state->data.repeat.bounds);
  state->data.repeat.child_bounds = real_child_bounds;
}

static GskRenderNode *
gtk_snapshot_collect_clip (GtkSnapshot      *snapshot,
                           GtkSnapshotState *state,
                           GskRenderNode   **nodes,
                           guint             n_nodes)
{
  GskRenderNode *node, *clip_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  /* Check if the child node will even be clipped */
  if (graphene_rect_contains_rect (&state->data.clip.bounds, &node->bounds))
    return node;

  if (state->data.clip.bounds.size.width == 0 ||
      state->data.clip.bounds.size.height == 0)
    return NULL;

  clip_node = gsk_clip_node_new (node, &state->data.clip.bounds);

  gsk_render_node_unref (node);

  return clip_node;
}

void
gtk_snapshot_push_clip (GtkSnapshot           *snapshot,
                        const graphene_rect_t *bounds)
{
  GtkSnapshotState *state;
  float scale_x, scale_y, dx, dy;
 
  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);

  state = gtk_snapshot_push_state (snapshot,
                                   gtk_snapshot_get_current_state (snapshot)->transform,
                                   gtk_snapshot_collect_clip);

  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &state->data.clip.bounds);
}

static GskRenderNode *
gtk_snapshot_collect_rounded_clip (GtkSnapshot      *snapshot,
                                   GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes)
{
  GskRenderNode *node, *clip_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  /* If the given radius is 0 in all corners, we can just create a normal clip node */
  if (gsk_rounded_rect_is_rectilinear (&state->data.rounded_clip.bounds))
    {
      /* ... and do the same optimization */
      if (graphene_rect_contains_rect (&state->data.rounded_clip.bounds.bounds, &node->bounds))
        return node;

      clip_node = gsk_clip_node_new (node, &state->data.rounded_clip.bounds.bounds);
    }
  else
    {
      if (gsk_rounded_rect_contains_rect (&state->data.rounded_clip.bounds, &node->bounds))
        return node;

      clip_node = gsk_rounded_clip_node_new (node, &state->data.rounded_clip.bounds);
    }

  if (clip_node->bounds.size.width == 0 ||
      clip_node->bounds.size.height == 0)
    {
      gsk_render_node_unref (node);
      gsk_render_node_unref (clip_node);
      return NULL;
    }

  gsk_render_node_unref (node);

  return clip_node;
}

void
gtk_snapshot_push_rounded_clip (GtkSnapshot          *snapshot,
                                const GskRoundedRect *bounds)
{
  GtkSnapshotState *state;
  float scale_x, scale_y, dx, dy;

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);

  state = gtk_snapshot_push_state (snapshot,
                                   gtk_snapshot_get_current_state (snapshot)->transform,
                                   gtk_snapshot_collect_rounded_clip);

  gtk_rounded_rect_scale_affine (&state->data.rounded_clip.bounds, bounds, scale_x, scale_y, dx, dy);
}

static GskRenderNode *
gtk_snapshot_collect_shadow (GtkSnapshot      *snapshot,
                             GtkSnapshotState *state,
                             GskRenderNode   **nodes,
                             guint             n_nodes)
{
  GskRenderNode *node, *shadow_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  shadow_node = gsk_shadow_node_new (node,
                                     state->data.shadow.shadows != NULL ?
                                     state->data.shadow.shadows :
                                     &state->data.shadow.a_shadow,
                                     state->data.shadow.n_shadows);

  gsk_render_node_unref (node);
  g_free (state->data.shadow.shadows);

  return shadow_node;
}

void
gtk_snapshot_push_shadow (GtkSnapshot     *snapshot,
                          const GskShadow *shadow,
                          gsize            n_shadows)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->transform,
                                   gtk_snapshot_collect_shadow);

  state->data.shadow.n_shadows = n_shadows;
  if (n_shadows == 1)
    {
      state->data.shadow.shadows = NULL;
      memcpy (&state->data.shadow.a_shadow, shadow, sizeof (GskShadow));
    }
  else
    {
      state->data.shadow.shadows = g_malloc (sizeof (GskShadow) * n_shadows);
      memcpy (state->data.shadow.shadows, shadow, sizeof (GskShadow) * n_shadows);
    }

}

static GskRenderNode *
gtk_snapshot_collect_blend_top (GtkSnapshot      *snapshot,
                                GtkSnapshotState *state,
                                GskRenderNode   **nodes,
                                guint             n_nodes)
{
  GskRenderNode *bottom_node, *top_node, *blend_node;
  GdkRGBA transparent = { 0, 0, 0, 0 };

  top_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  bottom_node = state->data.blend.bottom_node;

  g_assert (top_node != NULL || bottom_node != NULL);

  /* XXX: Is this necessary? Do we need a NULL node? */
  if (top_node == NULL)
    top_node = gsk_color_node_new (&transparent, &bottom_node->bounds);
  if (bottom_node == NULL)
    bottom_node = gsk_color_node_new (&transparent, &top_node->bounds);

  blend_node = gsk_blend_node_new (bottom_node, top_node, state->data.blend.blend_mode);

  gsk_render_node_unref (top_node);
  gsk_render_node_unref (bottom_node);

  return blend_node;
}

static GskRenderNode *
gtk_snapshot_collect_blend_bottom (GtkSnapshot      *snapshot,
                                   GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes)
{
  GtkSnapshotState *prev_state = gtk_snapshot_get_previous_state (snapshot);

  g_assert (prev_state->collect_func == gtk_snapshot_collect_blend_top);

  prev_state->data.blend.bottom_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);

  return NULL;
}

/**
 * gtk_snapshot_push_blend:
 * @snapshot: a #GtkSnapshot
 * @blend_mode: blend mode to use
 *
 * Blends together 2 images with the given blend mode.
 *
 * Until the first call to gtk_snapshot_pop(), the bottom image for the
 * blend operation will be recorded. After that call, the top image to
 * be blended will be recorded until the second call to gtk_snapshot_pop().
 *
 * Calling this function requires 2 subsequent calls to gtk_snapshot_pop().
 **/
void
gtk_snapshot_push_blend (GtkSnapshot  *snapshot,
                         GskBlendMode  blend_mode)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *top_state;

  top_state = gtk_snapshot_push_state (snapshot,
                                       current_state->transform,
                                       gtk_snapshot_collect_blend_top);
  top_state->data.blend.blend_mode = blend_mode;

  gtk_snapshot_push_state (snapshot,
                           top_state->transform,
                           gtk_snapshot_collect_blend_bottom);
}

static GskRenderNode *
gtk_snapshot_collect_cross_fade_end (GtkSnapshot      *snapshot,
                                     GtkSnapshotState *state,
                                     GskRenderNode   **nodes,
                                     guint             n_nodes)
{
  GskRenderNode *start_node, *end_node, *node;

  end_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  start_node = state->data.cross_fade.start_node;

  if (state->data.cross_fade.progress <= 0.0)
    {
      node = start_node;

      if (end_node)
        gsk_render_node_unref (end_node);
    }
  else if (state->data.cross_fade.progress >= 1.0)
    {
      node = end_node;

      if (start_node)
        gsk_render_node_unref (start_node);
    }
  else if (start_node && end_node)
    {
      node = gsk_cross_fade_node_new (start_node, end_node, state->data.cross_fade.progress);

      gsk_render_node_unref (start_node);
      gsk_render_node_unref (end_node);
    }
  else if (start_node)
    {
      node = gsk_opacity_node_new (start_node, 1.0 - state->data.cross_fade.progress);

      gsk_render_node_unref (start_node);
    }
  else if (end_node)
    {
      node = gsk_opacity_node_new (end_node, state->data.cross_fade.progress);

      gsk_render_node_unref (end_node);
    }
  else
    {
      node = NULL;
    }

  return node;
}

static GskRenderNode *
gtk_snapshot_collect_cross_fade_start (GtkSnapshot      *snapshot,
                                       GtkSnapshotState *state,
                                       GskRenderNode   **nodes,
                                       guint             n_nodes)
{
  GtkSnapshotState *prev_state = gtk_snapshot_get_previous_state (snapshot);

  g_assert (prev_state->collect_func == gtk_snapshot_collect_cross_fade_end);

  prev_state->data.cross_fade.start_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);

  return NULL;
}

/**
 * gtk_snapshot_push_cross_fade:
 * @snapshot: a #GtkSnapshot
 * @progress: progress between 0.0 and 1.0
 *
 * Snapshots a cross-fade operation between two images with the
 * given @progress.
 *
 * Until the first call to gtk_snapshot_pop(), the start image
 * will be snapshot. After that call, the end image will be recorded
 * until the second call to gtk_snapshot_pop().
 *
 * Calling this function requires 2 calls to gtk_snapshot_pop().
 **/
void
gtk_snapshot_push_cross_fade (GtkSnapshot *snapshot,
                              double       progress)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *end_state;

  end_state = gtk_snapshot_push_state (snapshot,
                                       current_state->transform,
                                       gtk_snapshot_collect_cross_fade_end);
  end_state->data.cross_fade.progress = progress;

  gtk_snapshot_push_state (snapshot,
                           end_state->transform,
                           gtk_snapshot_collect_cross_fade_start);
}

static GskRenderNode *
gtk_snapshot_pop_one (GtkSnapshot *snapshot)
{
  GtkSnapshotState *state;
  guint state_index;
  GskRenderNode *node;

  if (snapshot->state_stack->len == 0 &&
      !snapshot->from_parent)
    {
      g_warning ("Too many gtk_snapshot_pop() calls.");
      return NULL;
    }

  state = gtk_snapshot_get_current_state (snapshot);
  state_index = snapshot->state_stack->len - 1;

  if (state->collect_func)
    {
      node = state->collect_func (snapshot,
                                  state,
                                  (GskRenderNode **) snapshot->nodes->pdata + state->start_node_index,
                                  state->n_nodes);

      /* The collect func may not modify the state stack... */
      g_assert (state_index == snapshot->state_stack->len - 1);

      /* Remove all the state's nodes from the list of nodes */
      g_assert (state->start_node_index + state->n_nodes == snapshot->nodes->len);
      g_ptr_array_remove_range (snapshot->nodes,
                                snapshot->nodes->len - state->n_nodes,
                                state->n_nodes);
    }
  else
    {
      GtkSnapshotState *previous_state;

      node = NULL;

      /* move the nodes to the parent */
      previous_state = gtk_snapshot_get_previous_state (snapshot);
      previous_state->n_nodes += state->n_nodes;
      g_assert (previous_state->start_node_index + previous_state->n_nodes == snapshot->nodes->len);
    }

  g_array_remove_index (snapshot->state_stack, state_index);

  return node;
}

static GskRenderNode *
gtk_snapshot_pop_internal (GtkSnapshot *snapshot)
{
  GtkSnapshotState *state;
  GskRenderNode *node;

  for (state = gtk_snapshot_get_current_state (snapshot);
       gtk_snapshot_state_should_autopop (state);
       state = gtk_snapshot_get_current_state (snapshot))
    {
      node = gtk_snapshot_pop_one (snapshot);
      if (node)
        {
          gtk_snapshot_append_node_internal (snapshot, node);
          gsk_render_node_unref (node);
        }
    }

  return gtk_snapshot_pop_one (snapshot);
}

/**
 * gtk_snapshot_to_node:
 * @snapshot: a #GtkSnapshot
 *
 * Returns the render node that was constructed
 * by @snapshot. After calling this function, it
 * is no longer possible to add more nodes to
 * @snapshot. The only function that should be
 * called after this is gtk_snapshot_unref().
 *
 * Returns: the constructed #GskRenderNode
 */
GskRenderNode *
gtk_snapshot_to_node (GtkSnapshot *snapshot)
{
  GskRenderNode *result;

  result = gtk_snapshot_pop_internal (snapshot);

  /* We should have exactly our initial state */
  if (snapshot->state_stack->len > 0 &&
      !snapshot->from_parent)
    {
      g_warning ("Too many gtk_snapshot_push() calls. %u states remaining.", snapshot->state_stack->len);
    }

  if (!snapshot->from_parent)
    {
      g_array_free (snapshot->state_stack, TRUE);
      g_ptr_array_free (snapshot->nodes, TRUE);
    }

  snapshot->state_stack = NULL;
  snapshot->nodes = NULL;

  return result;
}

/**
 * gtk_snapshot_to_paintable:
 * @snapshot: a #GtkSnapshot
 * @size: (allow-none): The size of the resulting paintable
 *     or %NULL to use the bounds of the snapshot
 *
 * Returns a paintable encapsulating the render node
 * that was constructed by @snapshot. After calling
 * this function, it is no longer possible to add more
 * nodes to @snapshot. The only function that should be
 * called after this is gtk_snapshot_unref().
 *
 * Returns: (transfer full): a new #GdkPaintable
 */
GdkPaintable *
gtk_snapshot_to_paintable (GtkSnapshot           *snapshot,
                           const graphene_size_t *size)
{
  GskRenderNode *node;
  GdkPaintable *paintable;
  graphene_rect_t bounds;

  node = gtk_snapshot_to_node (snapshot);
  if (size)
    {
      graphene_size_init_from_size (&bounds.size, size);
    }
  else
    {
      gsk_render_node_get_bounds (node, &bounds);
      bounds.size.width += bounds.origin.x;
      bounds.size.height += bounds.origin.y;
    }
  bounds.origin.x = 0;
  bounds.origin.y = 0;

  paintable = gtk_render_node_paintable_new (node, &bounds);
  gsk_render_node_unref (node);

  return paintable;
}

/**
 * gtk_snapshot_pop:
 * @snapshot: a #GtkSnapshot
 *
 * Removes the top element from the stack of render nodes,
 * and appends it to the node underneath it.
 */
void
gtk_snapshot_pop (GtkSnapshot *snapshot)
{
  GskRenderNode *node;

  node = gtk_snapshot_pop_internal (snapshot);
  if (node)
    {
      gtk_snapshot_append_node_internal (snapshot, node);
      gsk_render_node_unref (node);
    }
}

/**
 * gtk_snapshot_offset:
 * @snapshot: a #GtkSnapshot
 * @x: horizontal translation
 * @y: vertical translation
 *
 * Appends a translation by (@x, @y) to the current transformation.
 */
void
gtk_snapshot_offset (GtkSnapshot *snapshot,
                     int          x,
                     int          y)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);

  current_state->transform = gtk_transform_translate (current_state->transform, &GRAPHENE_POINT_INIT (x, y));
}

/**
 * gtk_snapshot_get_offset:
 * @snapshot: a #GtkSnapshot
 * @x: (out) (optional): return location for x offset
 * @y: (out) (optional): return location for y offset
 *
 * Queries the offset managed by @snapshot. This offset is the
 * accumulated sum of calls to gtk_snapshot_offset().
 *
 * Use this offset to determine how to offset nodes that you
 * manually add to the snapshot using
 * gtk_snapshot_append().
 *
 * Note that other functions that add nodes for you, such as
 * gtk_snapshot_append_cairo() will add this offset for
 * you.
 **/
void
gtk_snapshot_get_offset (GtkSnapshot *snapshot,
                         int         *x,
                         int         *y)
{
  /* FIXME: remove this function */
  gtk_snapshot_ensure_identity (snapshot);

  if (x)
    *x = 0;

  if (y)
    *y = 0;
}

void
gtk_snapshot_append_node_internal (GtkSnapshot   *snapshot,
                                   GskRenderNode *node)
{
  GtkSnapshotState *current_state;

  current_state = gtk_snapshot_get_current_state (snapshot);

  if (current_state)
    {
      g_ptr_array_add (snapshot->nodes, gsk_render_node_ref (node));
      current_state->n_nodes ++;
    }
  else
    {
      g_critical ("Tried appending a node to an already finished snapshot.");
    }
}

/**
 * gtk_snapshot_append_node:
 * @snapshot: a #GtkSnapshot
 * @node: a #GskRenderNode
 *
 * Appends @node to the current render node of @snapshot,
 * without changing the current node. If @snapshot does
 * not have a current node yet, @node will become the
 * initial node.
 */
void
gtk_snapshot_append_node (GtkSnapshot   *snapshot,
                          GskRenderNode *node)
{
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

  gtk_snapshot_ensure_identity (snapshot);

  gtk_snapshot_append_node_internal (snapshot, node);
}

/**
 * gtk_snapshot_append_cairo:
 * @snapshot: a #GtkSnapshot
 * @bounds: the bounds for the new node
 *
 * Creates a new render node and appends it to the current render
 * node of @snapshot, without changing the current node.
 *
 * Returns: a cairo_t suitable for drawing the contents of the newly
 *     created render node
 */
cairo_t *
gtk_snapshot_append_cairo (GtkSnapshot           *snapshot,
                           const graphene_rect_t *bounds)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float scale_x, scale_y, dx, dy;
  cairo_t *cr;

  g_return_val_if_fail (snapshot != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);

  node = gsk_cairo_node_new (&real_bounds);

  gtk_snapshot_append_node_internal (snapshot, node);
  gsk_render_node_unref (node);

  cr = gsk_cairo_node_get_draw_context (node);

  cairo_scale (cr, scale_x, scale_y);
  cairo_translate (cr, dx, dy);

  return cr;
}

/**
 * gtk_snapshot_append_texture:
 * @snapshot: a #GtkSnapshot
 * @texture: the #GdkTexture to render
 * @bounds: the bounds for the new node
 *
 * Creates a new render node drawing the @texture into the given @bounds and appends it
 * to the current render node of @snapshot.
 **/
void
gtk_snapshot_append_texture (GtkSnapshot           *snapshot,
                             GdkTexture            *texture,
                             const graphene_rect_t *bounds)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float scale_x, scale_y, dx, dy;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GDK_IS_TEXTURE (texture));
  g_return_if_fail (bounds != NULL);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);
  node = gsk_texture_node_new (texture, &real_bounds);

  gtk_snapshot_append_node_internal (snapshot, node);
  gsk_render_node_unref (node);
}

/**
 * gtk_snapshot_append_color:
 * @snapshot: a #GtkSnapshot
 * @color: the #GdkRGBA to draw
 * @bounds: the bounds for the new node
 *
 * Creates a new render node drawing the @color into the given @bounds and appends it
 * to the current render node of @snapshot.
 *
 * You should try to avoid calling this function if @color is transparent.
 **/
void
gtk_snapshot_append_color (GtkSnapshot           *snapshot,
                           const GdkRGBA         *color,
                           const graphene_rect_t *bounds)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  float scale_x, scale_y, dx, dy;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (color != NULL);
  g_return_if_fail (bounds != NULL);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);

  node = gsk_color_node_new (color, &real_bounds);

  gtk_snapshot_append_node_internal (snapshot, node);
  gsk_render_node_unref (node);
}

/**
 * gtk_snapshot_render_background:
 * @snapshot: a #GtkSnapshot
 * @context: the #GtkStyleContext to use
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Creates a render node for the CSS background according to @context,
 * and appends it to the current node of @snapshot, without changing
 * the current node.
 */
void
gtk_snapshot_render_background (GtkSnapshot     *snapshot,
                                GtkStyleContext *context,
                                gdouble          x,
                                gdouble          y,
                                gdouble          width,
                                gdouble          height)
{
  GtkCssBoxes boxes;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_boxes_init_border_box (&boxes,
                                 gtk_style_context_lookup_style (context),
                                 x, y, width, height);
  gtk_css_style_snapshot_background (&boxes, snapshot);
}

/**
 * gtk_snapshot_render_frame:
 * @snapshot: a #GtkSnapshot
 * @context: the #GtkStyleContext to use
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Creates a render node for the CSS border according to @context,
 * and appends it to the current node of @snapshot, without changing
 * the current node.
 */
void
gtk_snapshot_render_frame (GtkSnapshot     *snapshot,
                           GtkStyleContext *context,
                           gdouble          x,
                           gdouble          y,
                           gdouble          width,
                           gdouble          height)
{
  GtkCssBoxes boxes;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_boxes_init_border_box (&boxes,
                                 gtk_style_context_lookup_style (context),
                                 x, y, width, height);
  gtk_css_style_snapshot_border (&boxes, snapshot);
}

/**
 * gtk_snapshot_render_focus:
 * @snapshot: a #GtkSnapshot
 * @context: the #GtkStyleContext to use
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @width: rectangle width
 * @height: rectangle height
 *
 * Creates a render node for the focus outline according to @context,
 * and appends it to the current node of @snapshot, without changing
 * the current node.
 */
void
gtk_snapshot_render_focus (GtkSnapshot     *snapshot,
                           GtkStyleContext *context,
                           gdouble          x,
                           gdouble          y,
                           gdouble          width,
                           gdouble          height)
{
  GtkCssBoxes boxes;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_css_boxes_init_border_box (&boxes,
                                 gtk_style_context_lookup_style (context),
                                 x, y, width, height);
  gtk_css_style_snapshot_outline (&boxes, snapshot);
}

/**
 * gtk_snapshot_render_layout:
 * @snapshot: a #GtkSnapshot
 * @context: the #GtkStyleContext to use
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 * @layout: the #PangoLayout to render
 *
 * Creates a render node for rendering @layout according to the style
 * information in @context, and appends it to the current node of @snapshot,
 * without changing the current node.
 */
void
gtk_snapshot_render_layout (GtkSnapshot     *snapshot,
                            GtkStyleContext *context,
                            gdouble          x,
                            gdouble          y,
                            PangoLayout     *layout)
{
  const GdkRGBA *fg_color;
  GtkCssValue *shadows_value;
  gboolean has_shadow;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  gtk_snapshot_offset (snapshot, x, y);

  fg_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));

  shadows_value = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_TEXT_SHADOW);
  has_shadow = gtk_css_shadows_value_push_snapshot (shadows_value, snapshot);

  gtk_snapshot_append_layout (snapshot, layout, fg_color);

  if (has_shadow)
    gtk_snapshot_pop (snapshot);

  gtk_snapshot_offset (snapshot, -x, -y);
}

/**
 * gtk_snapshot_append_linear_gradient:
 * @snapshot: a #GtkSnapshot
 * @bounds: the rectangle to render the linear gradient into
 * @start_point: the point at which the linear gradient will begin
 * @end_point: the point at which the linear gradient will finish
 * @stops: (array length=n_stops): a pointer to an array of #GskColorStop defining the gradient
 * @n_stops: the number of elements in @stops
 *
 * Appends a linear gradient node with the given stops to @snapshot.
 */
void
gtk_snapshot_append_linear_gradient (GtkSnapshot            *snapshot,
                                     const graphene_rect_t  *bounds,
                                     const graphene_point_t *start_point,
                                     const graphene_point_t *end_point,
                                     const GskColorStop     *stops,
                                     gsize                   n_stops)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  graphene_point_t real_start_point;
  graphene_point_t real_end_point;
  float scale_x, scale_y, dx, dy;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (start_point != NULL);
  g_return_if_fail (end_point != NULL);
  g_return_if_fail (stops != NULL);
  g_return_if_fail (n_stops > 1);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);
  real_start_point.x = scale_x * start_point->x + dx;
  real_start_point.y = scale_y * start_point->y + dy;
  real_end_point.x = scale_x * end_point->x + dx;
  real_end_point.y = scale_y * end_point->y + dy;

  node = gsk_linear_gradient_node_new (&real_bounds,
                                       &real_start_point,
                                       &real_end_point,
                                       stops,
                                       n_stops);

  gtk_snapshot_append_node_internal (snapshot, node);
  gsk_render_node_unref (node);
}

/**
 * gtk_snapshot_append_repeating_linear_gradient:
 * @snapshot: a #GtkSnapshot
 * @bounds: the rectangle to render the linear gradient into
 * @start_point: the point at which the linear gradient will begin
 * @end_point: the point at which the linear gradient will finish
 * @stops: (array length=n_stops): a pointer to an array of #GskColorStop defining the gradient
 * @n_stops: the number of elements in @stops
 *
 * Appends a repeating linear gradient node with the given stops to @snapshot.
 */
void
gtk_snapshot_append_repeating_linear_gradient (GtkSnapshot            *snapshot,
                                               const graphene_rect_t  *bounds,
                                               const graphene_point_t *start_point,
                                               const graphene_point_t *end_point,
                                               const GskColorStop     *stops,
                                               gsize                   n_stops)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  graphene_point_t real_start_point;
  graphene_point_t real_end_point;
  float scale_x, scale_y, dx, dy;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (start_point != NULL);
  g_return_if_fail (end_point != NULL);
  g_return_if_fail (stops != NULL);
  g_return_if_fail (n_stops > 1);

  gtk_snapshot_ensure_affine (snapshot, &scale_x, &scale_y, &dx, &dy);
  gtk_graphene_rect_scale_affine (bounds, scale_x, scale_y, dx, dy, &real_bounds);
  real_start_point.x = scale_x * start_point->x + dx;
  real_start_point.y = scale_y * start_point->y + dy;
  real_end_point.x = scale_x * end_point->x + dx;
  real_end_point.y = scale_y * end_point->y + dy;

  node = gsk_repeating_linear_gradient_node_new (&real_bounds,
                                                 &real_start_point,
                                                 &real_end_point,
                                                 stops,
                                                 n_stops);

  gtk_snapshot_append_node_internal (snapshot, node);
  gsk_render_node_unref (node);
}
