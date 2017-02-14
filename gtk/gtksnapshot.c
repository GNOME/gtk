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
gtk_snapshot_collect_default (GtkSnapshotState *state,
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
gtk_snapshot_state_new (GtkSnapshotState       *parent,
                        char                   *name,
                        cairo_region_t         *clip,
                        int                     translate_x,
                        int                     translate_y,
                        GtkSnapshotCollectFunc  collect_func)
{
  GtkSnapshotState *state;

  if (parent != NULL && parent->cached_state != NULL)
    {
      state = parent->cached_state;
      parent->cached_state = NULL;
    }
  else
    {
      state = g_slice_new0 (GtkSnapshotState);
      state->nodes = g_ptr_array_new_with_free_func ((GDestroyNotify) gsk_render_node_unref);
      state->parent = parent;
    }

  state->name = name;
  if (clip)
    state->clip_region = cairo_region_reference (clip);
  state->translate_x = translate_x;
  state->translate_y = translate_y;
  state->collect_func = collect_func;

  return state;
}

static void
gtk_snapshot_state_clear (GtkSnapshotState *state)
{
  g_ptr_array_set_size (state->nodes, 0);
  g_clear_pointer (&state->clip_region, cairo_region_destroy);
  g_clear_pointer (&state->name, g_free);
}

static void
gtk_snapshot_state_free (GtkSnapshotState *state)
{
  if (state->cached_state)
    gtk_snapshot_state_free (state->cached_state);
  gtk_snapshot_state_clear (state);
  g_ptr_array_unref (state->nodes);
  g_slice_free (GtkSnapshotState, state);
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

  snapshot->state = NULL;
  snapshot->record_names = record_names;
  snapshot->renderer = renderer;

  if (name && record_names)
    {
      va_list args;

      va_start (args, name);
      str = g_strdup_vprintf (name, args);
      va_end (args);
    }
  else
    str = NULL;

  snapshot->state = gtk_snapshot_state_new (NULL,
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
      snapshot->state = gtk_snapshot_state_new (snapshot->state,
                                                str,
                                                snapshot->state->clip_region,
                                                snapshot->state->translate_x,
                                                snapshot->state->translate_y,
                                                gtk_snapshot_collect_default);
    }
  else
    {
      snapshot->state = gtk_snapshot_state_new (snapshot->state,
                                                str,
                                                NULL,
                                                0, 0,
                                                gtk_snapshot_collect_default);
    }
}

static GskRenderNode *
gtk_snapshot_collect_transform (GtkSnapshotState *state,
                                GskRenderNode **nodes,
                                guint           n_nodes,
                                const char     *name)
{
  GskRenderNode *node, *transform_node;

  node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
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

  state = gtk_snapshot_state_new (snapshot->state,
                                  str,
                                  NULL,
                                  0, 0,
                                  gtk_snapshot_collect_transform);

  graphene_matrix_init_translate (&offset,
                                  &GRAPHENE_POINT3D_INIT(
                                      snapshot->state->translate_x,
                                      snapshot->state->translate_y,
                                      0
                                  ));

  graphene_matrix_multiply (transform, &offset, &state->data.transform.transform);

  snapshot->state = state;
}

static GskRenderNode *
gtk_snapshot_collect_opacity (GtkSnapshotState *state,
                              GskRenderNode **nodes,
                              guint           n_nodes,
                              const char     *name)
{
  GskRenderNode *node, *opacity_node;

  node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
  if (node == NULL)
    return NULL;

  opacity_node = gsk_opacity_node_new (node, state->data.opacity.opacity);
  if (name)
    gsk_render_node_set_name (opacity_node, name);

  gsk_render_node_unref (node);

  return opacity_node;
}

void
gtk_snapshot_push_opacity (GtkSnapshot *snapshot,
                           double       opacity,
                           const char  *name,
                           ...)
{
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

  state = gtk_snapshot_state_new (snapshot->state,
                                  str,
                                  snapshot->state->clip_region,
                                  snapshot->state->translate_x,
                                  snapshot->state->translate_y,
                                  gtk_snapshot_collect_opacity);
  state->data.opacity.opacity = opacity;
  snapshot->state = state;
}

static GskRenderNode *
gtk_snapshot_collect_color_matrix (GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes,
                                   const char       *name)
{
  GskRenderNode *node, *color_matrix_node;

  node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
  if (node == NULL)
    return NULL;

  color_matrix_node = gsk_color_matrix_node_new (node,
                                                 &state->data.color_matrix.matrix,
                                                 &state->data.color_matrix.offset);
  if (name)
    gsk_render_node_set_name (color_matrix_node, name);

  gsk_render_node_unref (node);

  return color_matrix_node;
}

void
gtk_snapshot_push_color_matrix (GtkSnapshot             *snapshot,
                                const graphene_matrix_t *color_matrix,
                                const graphene_vec4_t   *color_offset,
                                const char              *name,
                                ...)
{
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

  state = gtk_snapshot_state_new (snapshot->state,
                                  str,
                                  snapshot->state->clip_region,
                                  snapshot->state->translate_x,
                                  snapshot->state->translate_y,
                                  gtk_snapshot_collect_color_matrix);

  graphene_matrix_init_from_matrix (&state->data.color_matrix.matrix, color_matrix);
  graphene_vec4_init_from_vec4 (&state->data.color_matrix.offset, color_offset);
  snapshot->state = state;
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
gtk_snapshot_collect_repeat (GtkSnapshotState *state,
                             GskRenderNode   **nodes,
                             guint             n_nodes,
                             const char       *name)
{
  GskRenderNode *node, *repeat_node;
  graphene_rect_t *bounds = &state->data.repeat.bounds;
  graphene_rect_t *child_bounds = &state->data.repeat.child_bounds;

  node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
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
      graphene_rect_offset_r (child_bounds, snapshot->state->translate_x, snapshot->state->translate_y, &real_child_bounds);
      rectangle_init_from_graphene (&rect, &real_child_bounds);
      clip = cairo_region_create_rectangle (&rect);
    }

  state = gtk_snapshot_state_new (snapshot->state,
                                  str,
                                  clip,
                                  snapshot->state->translate_x,
                                  snapshot->state->translate_y,
                                  gtk_snapshot_collect_repeat);

  graphene_rect_offset_r (bounds, snapshot->state->translate_x, snapshot->state->translate_y, &state->data.repeat.bounds);
  state->data.repeat.child_bounds = real_child_bounds;

  snapshot->state = state;

  if (clip)
    cairo_region_destroy (clip);
}

static GskRenderNode *
gtk_snapshot_collect_clip (GtkSnapshotState *state,
                           GskRenderNode   **nodes,
                           guint             n_nodes,
                           const char       *name)
{
  GskRenderNode *node, *clip_node;

  node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
  if (node == NULL)
    return NULL;

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
  GtkSnapshotState *state;
  graphene_rect_t real_bounds;
  cairo_region_t *clip;
  cairo_rectangle_int_t rect;
  char *str;

  graphene_rect_offset_r (bounds, snapshot->state->translate_x, snapshot->state->translate_y, &real_bounds);

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
  if (snapshot->state->clip_region)
    {
      clip = cairo_region_copy (snapshot->state->clip_region);
      cairo_region_intersect_rectangle (clip, &rect);
    }
  else
    {
      clip = cairo_region_create_rectangle (&rect);
    }
  state = gtk_snapshot_state_new (snapshot->state,
                                  str,
                                  clip,
                                  snapshot->state->translate_x,
                                  snapshot->state->translate_y,
                                  gtk_snapshot_collect_clip);

  state->data.clip.bounds = real_bounds;

  snapshot->state = state;

  cairo_region_destroy (clip);
}

static GskRenderNode *
gtk_snapshot_collect_rounded_clip (GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes,
                                   const char       *name)
{
  GskRenderNode *node, *clip_node;

  node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
  if (node == NULL)
    return NULL;

  clip_node = gsk_rounded_clip_node_new (node, &state->data.rounded_clip.bounds);
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
  GtkSnapshotState *state;
  GskRoundedRect real_bounds;
  cairo_region_t *clip;
  cairo_rectangle_int_t rect;
  char *str;

  gsk_rounded_rect_init_copy (&real_bounds, bounds);
  gsk_rounded_rect_offset (&real_bounds, snapshot->state->translate_x, snapshot->state->translate_y);

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
  if (snapshot->state->clip_region)
    {
      clip = cairo_region_copy (snapshot->state->clip_region);
      cairo_region_intersect_rectangle (clip, &rect);
    }
  else
    {
      clip = cairo_region_create_rectangle (&rect);
    }

  state = gtk_snapshot_state_new (snapshot->state,
                                  str,
                                  clip,
                                  snapshot->state->translate_x,
                                  snapshot->state->translate_y,
                                  gtk_snapshot_collect_rounded_clip);


  state->data.rounded_clip.bounds = real_bounds;

  snapshot->state = state;
  cairo_region_destroy (clip);
}

static GskRenderNode *
gtk_snapshot_collect_shadow (GtkSnapshotState *state,
                             GskRenderNode   **nodes,
                             guint             n_nodes,
                             const char       *name)
{
  GskRenderNode *node, *shadow_node;

  node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
  if (node == NULL)
    return NULL;

  shadow_node = gsk_shadow_node_new (node, state->data.shadow.shadows, state->data.shadow.n_shadows);
  if (name)
    gsk_render_node_set_name (shadow_node, name);

  gsk_render_node_unref (node);
  if (state->data.shadow.shadows != &state->data.shadow.a_shadow)
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

  state = gtk_snapshot_state_new (snapshot->state,
                                  str,
                                  snapshot->state->clip_region,
                                  snapshot->state->translate_x,
                                  snapshot->state->translate_y,
                                  gtk_snapshot_collect_shadow);

  state->data.shadow.n_shadows = n_shadows;
  if (n_shadows == 1)
    state->data.shadow.shadows = &state->data.shadow.a_shadow;
  else
    state->data.shadow.shadows = g_malloc (sizeof (GskShadow) * n_shadows);
  memcpy (state->data.shadow.shadows, shadow, sizeof (GskShadow) * n_shadows);

  snapshot->state = state;
}

static GskRenderNode *
gtk_snapshot_collect_blend_top (GtkSnapshotState *state,
                                GskRenderNode   **nodes,
                                guint             n_nodes,
                                const char       *name)
{
  GskRenderNode *bottom_node, *top_node, *blend_node;

  top_node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
  bottom_node = state->data.blend.bottom_node;

  /* XXX: Is this necessary? Do we need a NULL node? */
  if (top_node == NULL)
    top_node = gsk_container_node_new (NULL, 0);
  if (bottom_node == NULL)
    bottom_node = gsk_container_node_new (NULL, 0);

  blend_node = gsk_blend_node_new (bottom_node, top_node, state->data.blend.blend_mode);
  gsk_render_node_set_name (blend_node, name);

  gsk_render_node_unref (top_node);
  gsk_render_node_unref (bottom_node);

  return blend_node;
}

static GskRenderNode *
gtk_snapshot_collect_blend_bottom (GtkSnapshotState *state,
                                   GskRenderNode   **nodes,
                                   guint             n_nodes,
                                   const char       *name)
{
  state->parent->data.blend.bottom_node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
  
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

  state = gtk_snapshot_state_new (snapshot->state,
                                  str,
                                  snapshot->state->clip_region,
                                  snapshot->state->translate_x,
                                  snapshot->state->translate_y,
                                  gtk_snapshot_collect_blend_top);
  state->data.blend.blend_mode = blend_mode;
  state->data.blend.bottom_node = NULL;

  state = gtk_snapshot_state_new (state,
                                  g_strdup (str),
                                  state->clip_region,
                                  state->translate_x,
                                  state->translate_y,
                                  gtk_snapshot_collect_blend_bottom);

  snapshot->state = state;
}

static GskRenderNode *
gtk_snapshot_collect_cross_fade_end (GtkSnapshotState *state,
                                     GskRenderNode   **nodes,
                                     guint             n_nodes,
                                     const char       *name)
{
  GskRenderNode *start_node, *end_node, *node;

  end_node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
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
gtk_snapshot_collect_cross_fade_start (GtkSnapshotState *state,
                                       GskRenderNode   **nodes,
                                       guint             n_nodes,
                                       const char       *name)
{
  state->parent->data.cross_fade.start_node = gtk_snapshot_collect_default (state, nodes, n_nodes, name);
  
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

  state = gtk_snapshot_state_new (snapshot->state,
                                  str,
                                  snapshot->state->clip_region,
                                  snapshot->state->translate_x,
                                  snapshot->state->translate_y,
                                  gtk_snapshot_collect_cross_fade_end);
  state->data.cross_fade.progress = progress;
  state->data.cross_fade.start_node = NULL;

  state = gtk_snapshot_state_new (state,
                                  g_strdup (str),
                                  state->clip_region,
                                  state->translate_x,
                                  state->translate_y,
                                  gtk_snapshot_collect_cross_fade_start);

  snapshot->state = state;
}

static GskRenderNode *
gtk_snapshot_pop_internal (GtkSnapshot *snapshot)
{
  GtkSnapshotState *state;
  GskRenderNode *node;

  if (snapshot->state == NULL)
    {
      g_warning ("Too many gtk_snapshot_pop() calls.");
      return NULL;
    }

  state = snapshot->state;
  snapshot->state = state->parent;

  node = state->collect_func (state,
                              (GskRenderNode **) state->nodes->pdata,
                              state->nodes->len,
                              state->name);

  if (snapshot->state == NULL)
    gtk_snapshot_state_free (state);
  else
    {
      gtk_snapshot_state_clear (state);
      snapshot->state->cached_state = state;
    }

  return node;
}

GskRenderNode *
gtk_snapshot_finish (GtkSnapshot *snapshot)
{
  GskRenderNode *result;
  
  result = gtk_snapshot_pop_internal (snapshot);

  if (snapshot->state != NULL)
    {
      g_warning ("Too many gtk_snapshot_push() calls.");
    }

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
  snapshot->state->translate_x += x;
  snapshot->state->translate_y += y;
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
  if (x)
    *x = snapshot->state->translate_x;

  if (y)
    *y = snapshot->state->translate_y;
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

  if (snapshot->state)
    {
      g_ptr_array_add (snapshot->state->nodes, gsk_render_node_ref (node));
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
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  cairo_t *cr;

  g_return_val_if_fail (snapshot != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  graphene_rect_offset_r (bounds, snapshot->state->translate_x, snapshot->state->translate_y, &real_bounds);
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

  cairo_translate (cr, snapshot->state->translate_x, snapshot->state->translate_y);

  return cr;
}

/**
 * gtk_snapshot_append_texture:
 * @snapshot: a #GtkSnapshot
 * @texture: the #GskTexture to render
 * @bounds: the bounds for the new node
 * @name: (transfer none): a printf() style format string for the name for the new node
 * @...: arguments to insert into the format string
 *
 * Creates a new render node drawing the @texture into the given @bounds and appends it
 * to the current render node of @snapshot.
 **/
void
gtk_snapshot_append_texture (GtkSnapshot            *snapshot,
                             GskTexture             *texture,
                             const graphene_rect_t  *bounds,
                             const char             *name,
                             ...)
{
  GskRenderNode *node;
  graphene_rect_t real_bounds;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GSK_IS_TEXTURE (texture));
  g_return_if_fail (bounds != NULL);

  graphene_rect_offset_r (bounds, snapshot->state->translate_x, snapshot->state->translate_y, &real_bounds);
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
  GskRenderNode *node;
  graphene_rect_t real_bounds;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (color != NULL);
  g_return_if_fail (bounds != NULL);

  graphene_rect_offset_r (bounds, snapshot->state->translate_x, snapshot->state->translate_y, &real_bounds);
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
  cairo_rectangle_int_t offset_rect;

  if (snapshot->state->clip_region == NULL)
    return FALSE;

  offset_rect.x = rect->x + snapshot->state->translate_x;
  offset_rect.y = rect->y + snapshot->state->translate_y;
  offset_rect.width = rect->width;
  offset_rect.height = rect->height;

  return cairo_region_contains_rectangle (snapshot->state->clip_region, &offset_rect) == CAIRO_REGION_OVERLAP_OUT;
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
  graphene_rect_t bounds;
  GtkBorder shadow_extents;
  PangoRectangle ink_rect;
  GtkCssValue *shadow;
  cairo_t *cr;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GTK_IS_STYLE_CONTEXT (context));
  g_return_if_fail (PANGO_IS_LAYOUT (layout));

  fg_color = _gtk_css_rgba_value_get_rgba (_gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_COLOR));
  shadow = _gtk_style_context_peek_property (context, GTK_CSS_PROPERTY_TEXT_SHADOW);
  pango_layout_get_pixel_extents (layout, &ink_rect, NULL);
  _gtk_css_shadows_value_get_extents (shadow, &shadow_extents);
  graphene_rect_init (&bounds,
                      ink_rect.x - shadow_extents.left,
                      ink_rect.y - shadow_extents.top,
                      ink_rect.width + shadow_extents.left + shadow_extents.right,
                      ink_rect.height + shadow_extents.top + shadow_extents.bottom);

  gtk_snapshot_offset (snapshot, x, y);

  cr = gtk_snapshot_append_cairo (snapshot, &bounds, "Text<%dchars>", pango_layout_get_character_count (layout));

  _gtk_css_shadows_value_paint_layout (shadow, cr, layout);

  gdk_cairo_set_source_rgba (cr, fg_color);
  pango_cairo_show_layout (cr, layout);

  cairo_destroy (cr);
  gtk_snapshot_offset (snapshot, -x, -y);
}

/**
 * gtk_snapshot_render_icon:
 * @snapshot: a #GtkSnapshot
 * @context: the #GtkStyleContext to use
 * @pixbuf: the #GdkPixbuf to render
 * @x: X origin of the rectangle
 * @y: Y origin of the rectangle
 *
 * Creates a render node for rendering @pixbuf according to the style
 * information in @context, and appends it to the current node of @snapshot,
 * without changing the current node.
 *
 * Since: 3.90
 */
void
gtk_snapshot_render_icon (GtkSnapshot     *snapshot,
                          GtkStyleContext *context,
                          GdkPixbuf       *pixbuf,
                          gdouble          x,
                          gdouble          y)
{
  GskTexture *texture;

  texture = gsk_texture_new_for_pixbuf (pixbuf);
  gtk_snapshot_offset (snapshot, x, y);
  gtk_css_style_snapshot_icon_texture (gtk_style_context_lookup_style (context),
                                       snapshot,
                                       texture,
                                       1);
  gtk_snapshot_offset (snapshot, -x, -y);
  g_object_unref (texture);
}

