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
#include "gtkrenderbackgroundprivate.h"
#include "gtkrenderborderprivate.h"
#include "gtkrendericonprivate.h"
#include "gtkstylecontextprivate.h"

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
 * The node at the top of the stack is the the one that gtk_snapshot_append()
 * operates on. Use the gtk_snapshot_push() and gtk_snapshot_pop() functions to
 * change the current node.
 *
 * The only way to obtain a #GtkSnapshot object is as an argument to
 * the #GtkWidget::snapshot vfunc.
 */

static GskRenderNode *
gtk_snapshot_collect_default (GtkSnapshot      *snapshot,
                              GtkSnapshotState *state,
                              GskRenderNode **nodes,
                              guint           n_nodes,
                              const char     *name)
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
      if (name)
        gsk_render_node_set_name (node, name);
    }

  return node;
}

static GtkSnapshotState *
gtk_snapshot_push_state (GtkSnapshot            *snapshot,
                         char                   *name,
                         cairo_region_t         *clip,
                         int                     translate_x,
                         int                     translate_y,
                         GtkSnapshotCollectFunc  collect_func)
{
  GtkSnapshotState *state;

  g_array_set_size (snapshot->state_stack, snapshot->state_stack->len + 1);
  state = &g_array_index (snapshot->state_stack, GtkSnapshotState, snapshot->state_stack->len - 1);

  state->name = name;
  if (clip)
    state->clip_region = cairo_region_reference (clip);
  state->translate_x = translate_x;
  state->translate_y = translate_y;
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
  g_clear_pointer (&state->clip_region, cairo_region_destroy);
  g_clear_pointer (&state->name, g_free);
}

void
gtk_snapshot_init (GtkSnapshot          *snapshot,
                   GskRenderer          *renderer,
                   gboolean              record_names,
                   const cairo_region_t *clip,
                   const char           *name,
                   ...)
{
  char *str;

  snapshot->record_names = record_names;
  snapshot->renderer = renderer;
  snapshot->state_stack = g_array_new (FALSE, TRUE, sizeof (GtkSnapshotState));
  g_array_set_clear_func (snapshot->state_stack, (GDestroyNotify)gtk_snapshot_state_clear);
  snapshot->nodes = g_ptr_array_new_with_free_func ((GDestroyNotify)gsk_render_node_unref);

  if (name && record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  gtk_snapshot_push_state (snapshot,
                           str,
                           (cairo_region_t *) clip,
                           0, 0,
                           gtk_snapshot_collect_default);
}

/**
 * gtk_snapshot_push:
 * @snapshot: a #GtkSnapshot
 * @keep_coordinates: If %TRUE, the current offset and clip will be kept.
 *     Otherwise, the clip will be unset and the offset will be reset to
 *     (0, 0).
 * @name: (transfer none): a printf() style format string for the name for the new node
 * @...: arguments to insert into the format string
 *
 * Creates a new render node, appends it to the current render
 * node of @snapshot, and makes it the new current render node.
 *
 * Since: 3.90
 */
void
gtk_snapshot_push (GtkSnapshot           *snapshot,
                   gboolean               keep_coordinates,
                   const char            *name,
                   ...)
{
  char *str;

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  if (keep_coordinates)
    {
      GtkSnapshotState *state = gtk_snapshot_get_current_state (snapshot);

      gtk_snapshot_push_state (snapshot,
                               g_strdup (str),
                               state->clip_region,
                               state->translate_x,
                               state->translate_y,
                               gtk_snapshot_collect_default);

    }
  else
    {
      gtk_snapshot_push_state (snapshot,
                               g_strdup (str),
                               NULL, 0, 0,
                               gtk_snapshot_collect_default);
    }
}

static GskRenderNode *
gtk_snapshot_collect_transform (GtkSnapshot      *snapshot,
                                GtkSnapshotState *state,
                                GskRenderNode **nodes,
                                guint           n_nodes,
                                const char     *name)
{
  GskRenderNode *node, *transform_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);
  if (node == NULL)
    return NULL;

  transform_node = gsk_transform_node_new (node, &state->data.transform.transform);
  if (name)
    gsk_render_node_set_name (transform_node, name);

  gsk_render_node_unref (node);

  return transform_node;
}

void
gtk_snapshot_push_transform (GtkSnapshot             *snapshot,
                             const graphene_matrix_t *transform,
                             const char              *name,
                             ...)
{
  GtkSnapshotState *previous_state;
  GtkSnapshotState *state;
  graphene_matrix_t offset;
  char *str;

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  state = gtk_snapshot_push_state (snapshot,
                                   str,
                                   NULL,
                                   0, 0,
                                   gtk_snapshot_collect_transform);

  previous_state = gtk_snapshot_get_previous_state (snapshot);

  graphene_matrix_init_translate (&offset,
                                  &GRAPHENE_POINT3D_INIT(
                                      previous_state->translate_x,
                                      previous_state->translate_y,
                                      0
                                  ));

  graphene_matrix_multiply (transform, &offset, &state->data.transform.transform);
}

static GskRenderNode *
gtk_snapshot_collect_opacity (GtkSnapshot      *snapshot,
                              GtkSnapshotState *state,
                              GskRenderNode **nodes,
                              guint           n_nodes,
                              const char     *name)
{
  GskRenderNode *node, *opacity_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);
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
      if (name)
        gsk_render_node_set_name (opacity_node, name);
      gsk_render_node_unref (node);
    }

  return opacity_node;
}

void
gtk_snapshot_push_opacity (GtkSnapshot *snapshot,
                           double       opacity,
                           const char  *name,
                           ...)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;
  char *str;

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  state = gtk_snapshot_push_state (snapshot,
                                   str,
                                   current_state->clip_region,
                                   current_state->translate_x,
                                   current_state->translate_y,
                                   gtk_snapshot_collect_opacity);
  state->data.opacity.opacity = CLAMP (opacity, 0.0, 1.0);
}

static GskRenderNode *
gtk_snapshot_collect_blur (GtkSnapshot      *snapshot,
                           GtkSnapshotState *state,
                           GskRenderNode **nodes,
                           guint           n_nodes,
                           const char     *name)
{
  GskRenderNode *node, *blur_node;
  double radius;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);
  if (node == NULL)
    return NULL;

  radius = state->data.blur.radius;

  if (radius == 0.0)
    return node;

  blur_node = gsk_blur_node_new (node, radius);
  if (name)
    gsk_render_node_set_name (blur_node, name);

  gsk_render_node_unref (node);

  return blur_node;
}

void
gtk_snapshot_push_blur (GtkSnapshot *snapshot,
                        double       radius,
                        const char  *name,
                        ...)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;
  char *str;

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  state = gtk_snapshot_push_state (snapshot,
                                   str,
                                   current_state->clip_region,
                                   current_state->translate_x,
                                   current_state->translate_y,
                                   gtk_snapshot_collect_blur);
  state->data.blur.radius = radius;
  current_state = state;
}

static GskRenderNode *
gtk_snapshot_collect_color_matrix (GtkSnapshot      *snapshot,
                                   GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes,
                                   const char       *name)
{
  GskRenderNode *node, *color_matrix_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);
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

  if (name)
    gsk_render_node_set_name (color_matrix_node, name);

  return color_matrix_node;
}

void
gtk_snapshot_push_color_matrix (GtkSnapshot             *snapshot,
                                const graphene_matrix_t *color_matrix,
                                const graphene_vec4_t   *color_offset,
                                const char              *name,
                                ...)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;
  char *str;

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  state = gtk_snapshot_push_state (snapshot,
                                  str,
                                  current_state->clip_region,
                                  current_state->translate_x,
                                  current_state->translate_y,
                                  gtk_snapshot_collect_color_matrix);

  graphene_matrix_init_from_matrix (&state->data.color_matrix.matrix, color_matrix);
  graphene_vec4_init_from_vec4 (&state->data.color_matrix.offset, color_offset);
  current_state = state;
}

static void
rectangle_init_from_graphene (cairo_rectangle_int_t *cairo,
                              const graphene_rect_t *graphene)
{
  cairo->x = floorf (graphene->origin.x);
  cairo->y = floorf (graphene->origin.y);
  cairo->width = ceilf (graphene->origin.x + graphene->size.width) - cairo->x;
  cairo->height = ceilf (graphene->origin.y + graphene->size.height) - cairo->y;
}

static GskRenderNode *
gtk_snapshot_collect_repeat (GtkSnapshot      *snapshot,
                             GtkSnapshotState *state,
                             GskRenderNode   **nodes,
                             guint             n_nodes,
                             const char       *name)
{
  GskRenderNode *node, *repeat_node;
  graphene_rect_t *bounds = &state->data.repeat.bounds;
  graphene_rect_t *child_bounds = &state->data.repeat.child_bounds;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);
  if (node == NULL)
    return NULL;

  repeat_node = gsk_repeat_node_new (bounds,
                                     node,
                                     child_bounds->size.width > 0 ? child_bounds : NULL);
  if (name)
    gsk_render_node_set_name (repeat_node, name);

  gsk_render_node_unref (node);

  return repeat_node;
}

void
gtk_snapshot_push_repeat (GtkSnapshot           *snapshot,
                          const graphene_rect_t *bounds,
                          const graphene_rect_t *child_bounds,
                          const char            *name,
                          ...)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;
  cairo_region_t *clip = NULL;
  graphene_rect_t real_child_bounds = { { 0 } };
  char *str;

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  if (child_bounds)
    {
      cairo_rectangle_int_t rect;
      graphene_rect_offset_r (child_bounds, current_state->translate_x, current_state->translate_y, &real_child_bounds);
      rectangle_init_from_graphene (&rect, &real_child_bounds);
      clip = cairo_region_create_rectangle (&rect);
    }

  state = gtk_snapshot_push_state (snapshot,
                                   str,
                                   clip,
                                   current_state->translate_x,
                                   current_state->translate_y,
                                   gtk_snapshot_collect_repeat);

  current_state = gtk_snapshot_get_previous_state (snapshot);

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &state->data.repeat.bounds);
  state->data.repeat.child_bounds = real_child_bounds;

  current_state = state;

  if (clip)
    cairo_region_destroy (clip);
}

static GskRenderNode *
gtk_snapshot_collect_clip (GtkSnapshot      *snapshot,
                           GtkSnapshotState *state,
                           GskRenderNode   **nodes,
                           guint             n_nodes,
                           const char       *name)
{
  GskRenderNode *node, *clip_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);
  if (node == NULL)
    return NULL;

  /* Check if the child node will even be clipped */
  if (graphene_rect_contains_rect (&state->data.clip.bounds, &node->bounds))
    return node;

  clip_node = gsk_clip_node_new (node, &state->data.clip.bounds);
  if (name)
    gsk_render_node_set_name (clip_node, name);

  gsk_render_node_unref (node);

  return clip_node;
}

void
gtk_snapshot_push_clip (GtkSnapshot           *snapshot,
                        const graphene_rect_t *bounds,
                        const char            *name,
                        ...)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;
  graphene_rect_t real_bounds;
  cairo_region_t *clip;
  cairo_rectangle_int_t rect;
  char *str;

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &real_bounds);

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  rectangle_init_from_graphene (&rect, &real_bounds);
  if (current_state->clip_region)
    {
      clip = cairo_region_copy (current_state->clip_region);
      cairo_region_intersect_rectangle (clip, &rect);
    }
  else
    {
      clip = cairo_region_create_rectangle (&rect);
    }
  state = gtk_snapshot_push_state (snapshot,
                                  str,
                                  clip,
                                  current_state->translate_x,
                                  current_state->translate_y,
                                  gtk_snapshot_collect_clip);

  state->data.clip.bounds = real_bounds;

  cairo_region_destroy (clip);
}

static GskRenderNode *
gtk_snapshot_collect_rounded_clip (GtkSnapshot      *snapshot,
                                   GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes,
                                   const char       *name)
{
  GskRenderNode *node, *clip_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);
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

  if (name)
    gsk_render_node_set_name (clip_node, name);

  gsk_render_node_unref (node);

  return clip_node;
}

void
gtk_snapshot_push_rounded_clip (GtkSnapshot          *snapshot,
                                const GskRoundedRect *bounds,
                                const char           *name,
                                ...)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;
  GskRoundedRect real_bounds;
  cairo_region_t *clip;
  cairo_rectangle_int_t rect;
  char *str;

  gsk_rounded_rect_init_copy (&real_bounds, bounds);
  gsk_rounded_rect_offset (&real_bounds, current_state->translate_x, current_state->translate_y);

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  rectangle_init_from_graphene (&rect, &real_bounds.bounds);
  if (current_state->clip_region)
    {
      clip = cairo_region_copy (current_state->clip_region);
      cairo_region_intersect_rectangle (clip, &rect);
    }
  else
    {
      clip = cairo_region_create_rectangle (&rect);
    }

  state = gtk_snapshot_push_state (snapshot,
                                  str,
                                  clip,
                                  current_state->translate_x,
                                  current_state->translate_y,
                                  gtk_snapshot_collect_rounded_clip);


  state->data.rounded_clip.bounds = real_bounds;

  current_state = state;
  cairo_region_destroy (clip);
}

static GskRenderNode *
gtk_snapshot_collect_shadow (GtkSnapshot      *snapshot,
                             GtkSnapshotState *state,
                             GskRenderNode   **nodes,
                             guint             n_nodes,
                             const char       *name)
{
  GskRenderNode *node, *shadow_node;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);
  if (node == NULL)
    return NULL;

  shadow_node = gsk_shadow_node_new (node,
                                     state->data.shadow.shadows != NULL ?
                                     state->data.shadow.shadows :
                                     &state->data.shadow.a_shadow,
                                     state->data.shadow.n_shadows);
  if (name)
    gsk_render_node_set_name (shadow_node, name);

  gsk_render_node_unref (node);
  g_free (state->data.shadow.shadows);

  return shadow_node;
}

void
gtk_snapshot_push_shadow (GtkSnapshot            *snapshot,
                          const GskShadow        *shadow,
                          gsize                   n_shadows,
                          const char             *name,
                          ...)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;
  char *str;

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  state = gtk_snapshot_push_state (snapshot,
                                   str,
                                   current_state->clip_region,
                                   current_state->translate_x,
                                   current_state->translate_y,
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
                                guint             n_nodes,
                                const char       *name)
{
  GskRenderNode *bottom_node, *top_node, *blend_node;
  GdkRGBA transparent = { 0, 0, 0, 0 };

  top_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);
  bottom_node = state->data.blend.bottom_node;

  g_assert (top_node != NULL || bottom_node != NULL);

  /* XXX: Is this necessary? Do we need a NULL node? */
  if (top_node == NULL)
    top_node = gsk_color_node_new (&transparent, &bottom_node->bounds);
  if (bottom_node == NULL)
    bottom_node = gsk_color_node_new (&transparent, &top_node->bounds);

  blend_node = gsk_blend_node_new (bottom_node, top_node, state->data.blend.blend_mode);
  gsk_render_node_set_name (blend_node, name);

  gsk_render_node_unref (top_node);
  gsk_render_node_unref (bottom_node);

  return blend_node;
}

static GskRenderNode *
gtk_snapshot_collect_blend_bottom (GtkSnapshot      *snapshot,
                                   GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes,
                                   const char       *name)
{
  GtkSnapshotState *prev_state = gtk_snapshot_get_previous_state (snapshot);

  g_assert (prev_state->collect_func == gtk_snapshot_collect_blend_top);

  prev_state->data.blend.bottom_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);

  return NULL;
}

/**
 * gtk_snapshot_push_blend:
 * @snapshot: a #GtkSnapshot
 * @blend_mode: blend mode to use
 * @name: printf format string for name of the pushed node
 * @...: printf-style arguments for the @name string
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
                         GskBlendMode  blend_mode,
                         const char   *name,
                         ...)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *top_state;
  char *str;

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  top_state = gtk_snapshot_push_state (snapshot,
                                       str,
                                       current_state->clip_region,
                                       current_state->translate_x,
                                       current_state->translate_y,
                                       gtk_snapshot_collect_blend_top);
  top_state->data.blend.blend_mode = blend_mode;

  gtk_snapshot_push_state (snapshot,
                           g_strdup (str),
                           top_state->clip_region,
                           top_state->translate_x,
                           top_state->translate_y,
                           gtk_snapshot_collect_blend_bottom);
}

static GskRenderNode *
gtk_snapshot_collect_cross_fade_end (GtkSnapshot      *snapshot,
                                     GtkSnapshotState *state,
                                     GskRenderNode   **nodes,
                                     guint             n_nodes,
                                     const char       *name)
{
  GskRenderNode *start_node, *end_node, *node;

  end_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);
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
      gsk_render_node_set_name (node, name);

      gsk_render_node_unref (start_node);
      gsk_render_node_unref (end_node);
    }
  else if (start_node)
    {
      node = gsk_opacity_node_new (start_node, 1.0 - state->data.cross_fade.progress);
      gsk_render_node_set_name (node, name);

      gsk_render_node_unref (start_node);
    }
  else if (end_node)
    {
      node = gsk_opacity_node_new (end_node, state->data.cross_fade.progress);
      gsk_render_node_set_name (node, name);

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
                                       guint             n_nodes,
                                       const char       *name)
{
  GtkSnapshotState *prev_state = gtk_snapshot_get_previous_state (snapshot);

  g_assert (prev_state->collect_func == gtk_snapshot_collect_cross_fade_end);

  prev_state->data.cross_fade.start_node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes, name);

  return NULL;
}

/**
 * gtk_snapshot_push_cross_fade:
 * @snapshot: a #GtkSnapshot
 * @progress: progress between 0.0 and 1.0
 * @name: printf format string for name of the pushed node
 * @...: printf-style arguments for the @name string
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
                              double       progress,
                              const char  *name,
                              ...)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *end_state;
  char *str;

  if (name && snapshot->record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  end_state = gtk_snapshot_push_state (snapshot,
                                       str,
                                       current_state->clip_region,
                                       current_state->translate_x,
                                       current_state->translate_y,
                                       gtk_snapshot_collect_cross_fade_end);
  end_state->data.cross_fade.progress = progress;

  gtk_snapshot_push_state (snapshot,
                           g_strdup (str),
                           end_state->clip_region,
                           end_state->translate_x,
                           end_state->translate_y,
                           gtk_snapshot_collect_cross_fade_start);
}

static GskRenderNode *
gtk_snapshot_pop_internal (GtkSnapshot *snapshot)
{
  GtkSnapshotState *state;
  guint state_index;
  GskRenderNode *node;

  if (snapshot->state_stack->len == 0)
    {
      g_warning ("Too many gtk_snapshot_pop() calls.");
      return NULL;
    }

  state = gtk_snapshot_get_current_state (snapshot);
  state_index = snapshot->state_stack->len - 1;

  node = state->collect_func (snapshot,
                              state,
                              (GskRenderNode **) snapshot->nodes->pdata + state->start_node_index,
                              state->n_nodes,
                              state->name);

  /* The collect func may not modify the state stack... */
  g_assert (state_index == snapshot->state_stack->len - 1);

  /* Remove all the state's nodes from the list of nodes */
  g_assert (state->start_node_index + state->n_nodes == snapshot->nodes->len);
  g_ptr_array_remove_range (snapshot->nodes,
                            snapshot->nodes->len - state->n_nodes,
                            state->n_nodes);

  g_array_remove_index (snapshot->state_stack, state_index);

  return node;
}

GskRenderNode *
gtk_snapshot_finish (GtkSnapshot *snapshot)
{
  GskRenderNode *result;

  /* We should have exactly our initial state */
  if (snapshot->state_stack->len > 1)
    {
      gint i;

      g_warning ("Too many gtk_snapshot_push() calls. Still there:");
      for (i = snapshot->state_stack->len - 1; i >= 0; i --)
        {
          const GtkSnapshotState *s = &g_array_index (snapshot->state_stack, GtkSnapshotState, i);

          g_warning ("%s", s->name);
        }
    }
  
  result = gtk_snapshot_pop_internal (snapshot);

  g_array_free (snapshot->state_stack, TRUE);
  g_ptr_array_free (snapshot->nodes, TRUE);
  return result;
}

/**
 * gtk_snapshot_pop:
 * @snapshot: a #GtkSnapshot
 *
 * Removes the top element from the stack of render nodes,
 * and appends it to the node underneath it.
 *
 * Since: 3.90
 */
void
gtk_snapshot_pop (GtkSnapshot *snapshot)
{
  GskRenderNode *node;

  node = gtk_snapshot_pop_internal (snapshot);
  if (node)
    {
      gtk_snapshot_append_node (snapshot, node);
      gsk_render_node_unref (node);
    }
}

/**
 * gtk_snapshot_get_renderer:
 * @snapshot: a #GtkSnapshot
 *
 * Obtains the #GskRenderer that this snapshot will be
 * rendered with.
 *
 * Returns: (transfer none): the #GskRenderer
 *
 * Since: 3.90
 */
GskRenderer *
gtk_snapshot_get_renderer (const GtkSnapshot *snapshot)
{
  return snapshot->renderer;
}

/**
 * gtk_snapshot_offset:
 * @snapshot: a $GtkSnapshot
 * @x: horizontal translation
 * @y: vertical translation
 *
 * Appends a translation by (@x, @y) to the current transformation.
 *
 * Since: 3.90
 */
void
gtk_snapshot_offset (GtkSnapshot *snapshot,
                     int          x,
                     int          y)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);

  current_state->translate_x += x;
  current_state->translate_y += y;
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
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);

  if (x)
    *x = current_state->translate_x;

  if (y)
    *y = current_state->translate_y;
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
  GtkSnapshotState *current_state;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GSK_IS_RENDER_NODE (node));

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
 * gtk_snapshot_append_cairo:
 * @snapshot: a #GtkSnapshot
 * @bounds: the bounds for the new node
 * @name: (transfer none): a printf() style format string for the name for the new node
 * @...: arguments to insert into the format string
 *
 * Creates a new render node and appends it to the current render
 * node of @snapshot, without changing the current node.
 *
 * Returns: a cairo_t suitable for drawing the contents of the newly
 *     created render node
 *
 * Since: 3.90
 */
cairo_t *
gtk_snapshot_append_cairo (GtkSnapshot           *snapshot,
                           const graphene_rect_t *bounds,
                           const char            *name,
                           ...)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  cairo_t *cr;

  g_return_val_if_fail (snapshot != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &real_bounds);
  node = gsk_cairo_node_new (&real_bounds);

  if (name && snapshot->record_names)
    {
      va_list args;
      char *str;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);

      gsk_render_node_set_name (node, str);

      g_free (str);
    }

  gtk_snapshot_append_node (snapshot, node);
  gsk_render_node_unref (node);

  cr = gsk_cairo_node_get_draw_context (node, snapshot->renderer);

  cairo_translate (cr, current_state->translate_x, current_state->translate_y);

  return cr;
}

/**
 * gtk_snapshot_append_texture:
 * @snapshot: a #GtkSnapshot
 * @texture: the #GdkTexture to render
 * @bounds: the bounds for the new node
 * @name: (transfer none): a printf() style format string for the name for the new node
 * @...: arguments to insert into the format string
 *
 * Creates a new render node drawing the @texture into the given @bounds and appends it
 * to the current render node of @snapshot.
 **/
void
gtk_snapshot_append_texture (GtkSnapshot            *snapshot,
                             GdkTexture             *texture,
                             const graphene_rect_t  *bounds,
                             const char             *name,
                             ...)
{
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GskRenderNode *node;
  graphene_rect_t real_bounds;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GDK_IS_TEXTURE (texture));
  g_return_if_fail (bounds != NULL);

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &real_bounds);
  node = gsk_texture_node_new (texture, &real_bounds);

  if (name && snapshot->record_names)
    {
      va_list args;
      char *str;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);

      gsk_render_node_set_name (node, str);

      g_free (str);
    }

  gtk_snapshot_append_node (snapshot, node);
  gsk_render_node_unref (node);
}

/**
 * gtk_snapshot_append_color:
 * @snapshot: a #GtkSnapshot
 * @color: the #GdkRGBA to draw
 * @bounds: the bounds for the new node
 * @name: (transfer none): a printf() style format string for the name for the new node
 * @...: arguments to insert into the format string
 *
 * Creates a new render node drawing the @color into the given @bounds and appends it
 * to the current render node of @snapshot.
 *
 * You should try to avoid calling this function if @color is transparent.
 **/
void
gtk_snapshot_append_color (GtkSnapshot           *snapshot,
                           const GdkRGBA         *color,
                           const graphene_rect_t *bounds,
                           const char            *name,
                           ...)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GskRenderNode *node;
  graphene_rect_t real_bounds;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (color != NULL);
  g_return_if_fail (bounds != NULL);

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &real_bounds);
  node = gsk_color_node_new (color, &real_bounds);

  if (name && snapshot->record_names)
    {
      va_list args;
      char *str;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);

      gsk_render_node_set_name (node, str);

      g_free (str);
    }

  gtk_snapshot_append_node (snapshot, node);
  gsk_render_node_unref (node);
}

/**
 * gtk_snapshot_clips_rect:
 * @snapshot: a #GtkSnapshot
 * @bounds: a rectangle
 *
 * Tests whether the rectangle is entirely outside the clip region of @snapshot.
 *
 * Returns: %TRUE is @bounds is entirely outside the clip region
 *
 * Since: 3.90
 */
gboolean
gtk_snapshot_clips_rect (GtkSnapshot                 *snapshot,
                         const cairo_rectangle_int_t *rect)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  cairo_rectangle_int_t offset_rect;

  if (current_state->clip_region == NULL)
    return FALSE;

  offset_rect.x = rect->x + current_state->translate_x;
  offset_rect.y = rect->y + current_state->translate_y;
  offset_rect.width = rect->width;
  offset_rect.height = rect->height;

  return cairo_region_contains_rectangle (current_state->clip_region, &offset_rect) == CAIRO_REGION_OVERLAP_OUT;
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
 *
 * Since: 3.90
 */
void
gtk_snapshot_render_background (GtkSnapshot     *snapshot,
                                GtkStyleContext *context,
                                gdouble          x,
                                gdouble          y,
                                gdouble          width,
                                gdouble          height)
{
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_snapshot_offset (snapshot, x, y);
  gtk_css_style_snapshot_background (gtk_style_context_lookup_style (context),
                                     snapshot,
                                     width, height);
  gtk_snapshot_offset (snapshot, -x, -y);
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
 *
 * Since: 3.90
 */
void
gtk_snapshot_render_frame (GtkSnapshot     *snapshot,
                           GtkStyleContext *context,
                           gdouble          x,
                           gdouble          y,
                           gdouble          width,
                           gdouble          height)
{
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_snapshot_offset (snapshot, x, y);
  gtk_css_style_snapshot_border (gtk_style_context_lookup_style (context),
                                 snapshot,
                                 width, height);
  gtk_snapshot_offset (snapshot, -x, -y);
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
 *
 * Since: 3.90
 */
void
gtk_snapshot_render_focus (GtkSnapshot     *snapshot,
                           GtkStyleContext *context,
                           gdouble          x,
                           gdouble          y,
                           gdouble          width,
                           gdouble          height)
{
  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));

  gtk_snapshot_offset (snapshot, x, y);
  gtk_css_style_snapshot_outline (gtk_style_context_lookup_style (context),
                                  snapshot,
                                  width, height);
  gtk_snapshot_offset (snapshot, -x, -y);
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
 *
 * Since: 3.90
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

  gsk_pango_show_layout (snapshot, fg_color, layout);

  if (has_shadow)
    gtk_snapshot_pop (snapshot);

  gtk_snapshot_offset (snapshot, -x, -y);
}
