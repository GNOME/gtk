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

#include "gsk/gskrendernodeprivate.h"

#include "gsk/gskallocprivate.h"

#include "gtk/gskpango.h"


static void gtk_snapshot_state_clear (GtkSnapshotState *state);

/* Returns the smallest power of 2 greater than n, or n if
 * such power does not fit in a guint
 */
static guint
g_nearest_pow (gint num)
{
  guint n = 1;

  while (n < num && n > 0)
    n <<= 1;

  return n ? n : num;
}

typedef struct _GtkRealSnapshotStateArray  GtkRealSnapshotStateArray;

struct _GtkRealSnapshotStateArray
{
  GtkSnapshotState *data;
  guint   len;
  guint   alloc;
  gint    ref_count;
};

static GtkSnapshotStateArray*
gtk_snapshot_state_array_new (void)
{
  GtkRealSnapshotStateArray *array;

  g_return_val_if_fail (sizeof (GtkSnapshotState) % 16 == 0, NULL);

  array = g_slice_new (GtkRealSnapshotStateArray);

  array->data      = NULL;
  array->len       = 0;
  array->alloc     = 0;
  array->ref_count = 1;

  return (GtkSnapshotStateArray *) array;
}

static GtkSnapshotState *
gtk_snapshot_state_array_free (GtkSnapshotStateArray *farray)
{
  GtkRealSnapshotStateArray *array = (GtkRealSnapshotStateArray*) farray;
  guint i;

  g_return_val_if_fail (array, NULL);

  for (i = 0; i < array->len; i++)
    gtk_snapshot_state_clear (&array->data[i]);

  gsk_aligned_free (array->data);

  g_slice_free1 (sizeof (GtkRealSnapshotStateArray), array);

  return NULL;
}

#define MIN_ARRAY_SIZE 16

static void
gtk_snapshot_state_array_maybe_expand (GtkRealSnapshotStateArray *array,
                                       gint                       len)
{
  guint want_alloc = sizeof (GtkSnapshotState) * (array->len + len);
  GtkSnapshotState *new_data;

  if (want_alloc <= array->alloc)
    return;

  want_alloc = g_nearest_pow (want_alloc);
  want_alloc = MAX (want_alloc, MIN_ARRAY_SIZE);
  new_data = gsk_aligned_alloc0 (want_alloc, 1, 16);
  memcpy (new_data, array->data, sizeof (GtkSnapshotState) * array->len);
  gsk_aligned_free (array->data);
  array->data = new_data;
  array->alloc = want_alloc;
}

static GtkSnapshotStateArray*
gtk_snapshot_state_array_remove_index (GtkSnapshotStateArray *farray,
                                       guint                  index_)
{
  GtkRealSnapshotStateArray *array = (GtkRealSnapshotStateArray*) farray;

  g_return_val_if_fail (array, NULL);
  g_return_val_if_fail (index_ < array->len, NULL);

  gtk_snapshot_state_clear (&array->data[index_]);

  memmove (&array->data[index_],
           &array->data[index_ + 1],
           (array->len - index_ - 1) * sizeof (GtkSnapshotState));

  array->len -= 1;

  return farray;
}

static GtkSnapshotStateArray*
gtk_snapshot_state_array_remove_range (GtkSnapshotStateArray *farray,
                                       guint                  index_,
                                       guint                  length)
{
  GtkRealSnapshotStateArray *array = (GtkRealSnapshotStateArray*) farray;
  guint i;

  g_return_val_if_fail (array, NULL);
  g_return_val_if_fail (index_ <= array->len, NULL);
  g_return_val_if_fail (index_ + length <= array->len, NULL);

  for (i = 0; i < length; i++)
    gtk_snapshot_state_clear (&array->data[index_ + i]);

  memmove (&array->data[index_],
           &array->data[index_ + length],
           (array->len - (index_ + length)) * sizeof (GtkSnapshotState));

  array->len -= length;

  return farray;
}

static GtkSnapshotStateArray*
gtk_snapshot_state_array_set_size (GtkSnapshotStateArray *farray,
                                   guint                  length)
{
  GtkRealSnapshotStateArray *array = (GtkRealSnapshotStateArray*) farray;

  g_return_val_if_fail (array, NULL);

  if (length > array->len)
    gtk_snapshot_state_array_maybe_expand (array, length - array->len);
  else if (length < array->len)
    gtk_snapshot_state_array_remove_range (farray, length, array->len - length);

  array->len = length;

  return farray;
}

#define gtk_snapshot_state_array_append_val(a,v)	gtk_snapshot_state_array_append_vals (a, &(v), 1)

static GtkSnapshotStateArray*
gtk_snapshot_state_array_append_vals (GtkSnapshotStateArray *farray,
                                      gconstpointer          data,
                                      guint                  len)
{
  GtkRealSnapshotStateArray *array = (GtkRealSnapshotStateArray*) farray;

  g_return_val_if_fail (array, NULL);

  if (len == 0)
    return farray;

  gtk_snapshot_state_array_maybe_expand (array, len);

  memcpy (&array->data[array->len], data,
          sizeof (GtkSnapshotState) * len);

  array->len += len;

  return farray;
}


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
                         int                     translate_x,
                         int                     translate_y,
                         GtkSnapshotCollectFunc  collect_func)
{
  GtkSnapshotState state = { 0, };

  state.translate_x = translate_x;
  state.translate_y = translate_y;
  state.collect_func = collect_func;
  state.start_node_index = snapshot->nodes->len;
  state.n_nodes = 0;

  gtk_snapshot_state_array_append_val (snapshot->state_stack, state);

  return &snapshot->state_stack->data[snapshot->state_stack->len - 1];
}

static GtkSnapshotState *
gtk_snapshot_get_current_state (const GtkSnapshot *snapshot)
{
  g_assert (snapshot->state_stack->len > 0);

  return &snapshot->state_stack->data[snapshot->state_stack->len - 1];
}

static GtkSnapshotState *
gtk_snapshot_get_previous_state (const GtkSnapshot *snapshot)
{
  g_assert (snapshot->state_stack->len > 1);

  return &snapshot->state_stack->data[snapshot->state_stack->len - 2];
}

static void
gtk_snapshot_state_clear (GtkSnapshotState *state)
{
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

  snapshot->state_stack = gtk_snapshot_state_array_new ();
  snapshot->nodes = g_ptr_array_new_with_free_func ((GDestroyNotify)gsk_render_node_unref);

  gtk_snapshot_push_state (snapshot,
                           0, 0,
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
                                   current_state->translate_x,
                                   current_state->translate_y,
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

  transform_node = gsk_transform_node_new (node, &state->data.transform.transform);

  gsk_render_node_unref (node);

  return transform_node;
}

void
gtk_snapshot_push_transform (GtkSnapshot             *snapshot,
                             const graphene_matrix_t *transform)
{
  GtkSnapshotState *previous_state;
  GtkSnapshotState *state;
  graphene_matrix_t offset;

  state = gtk_snapshot_push_state (snapshot,
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
gtk_snapshot_collect_offset (GtkSnapshot       *snapshot,
                             GtkSnapshotState  *state,
                             GskRenderNode    **nodes,
                             guint              n_nodes)
{
  GskRenderNode *node, *offset_node;
  GtkSnapshotState  *previous_state;

  node = gtk_snapshot_collect_default (snapshot, state, nodes, n_nodes);
  if (node == NULL)
    return NULL;

  previous_state = gtk_snapshot_get_previous_state (snapshot);
  if (previous_state->translate_x == 0.0 &&
      previous_state->translate_y == 0.0)
    return node;

  if (gsk_render_node_get_node_type (node) == GSK_OFFSET_NODE)
    {
      const float dx = previous_state->translate_x;
      const float dy = previous_state->translate_y;

      offset_node = gsk_offset_node_new (gsk_offset_node_get_child (node),
                                         gsk_offset_node_get_x_offset (node) + dx,
                                         gsk_offset_node_get_y_offset (node) + dy);
    }
  else
    {
      offset_node = gsk_offset_node_new (node,
                                         previous_state->translate_x,
                                         previous_state->translate_y);
    }

  gsk_render_node_unref (node);

  return offset_node;
}

static void
gtk_snapshot_push_offset (GtkSnapshot *snapshot)
{
  gtk_snapshot_push_state (snapshot,
                           0, 0,
                           gtk_snapshot_collect_offset);
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
                                   current_state->translate_x,
                                   current_state->translate_y,
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
                                   current_state->translate_x,
                                   current_state->translate_y,
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
                                   current_state->translate_x,
                                   current_state->translate_y,
                                   gtk_snapshot_collect_color_matrix);

  graphene_matrix_init_from_matrix (&state->data.color_matrix.matrix, color_matrix);
  graphene_vec4_init_from_vec4 (&state->data.color_matrix.offset, color_offset);
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

void
gtk_snapshot_push_repeat (GtkSnapshot           *snapshot,
                          const graphene_rect_t *bounds,
                          const graphene_rect_t *child_bounds)
{
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;
  graphene_rect_t real_child_bounds = { { 0 } };

  if (child_bounds)
    graphene_rect_offset_r (child_bounds, current_state->translate_x, current_state->translate_y, &real_child_bounds);

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->translate_x,
                                   current_state->translate_y,
                                   gtk_snapshot_collect_repeat);

  current_state = gtk_snapshot_get_previous_state (snapshot);

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &state->data.repeat.bounds);
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
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;
  graphene_rect_t real_bounds;
  cairo_rectangle_int_t rect;

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &real_bounds);

  rectangle_init_from_graphene (&rect, &real_bounds);

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->translate_x,
                                   current_state->translate_y,
                                   gtk_snapshot_collect_clip);

  state->data.clip.bounds = real_bounds;
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
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GtkSnapshotState *state;
  GskRoundedRect real_bounds;

  gsk_rounded_rect_init_copy (&real_bounds, bounds);
  gsk_rounded_rect_offset (&real_bounds, current_state->translate_x, current_state->translate_y);

  state = gtk_snapshot_push_state (snapshot,
                                   current_state->translate_x,
                                   current_state->translate_y,
                                   gtk_snapshot_collect_rounded_clip);

  state->data.rounded_clip.bounds = real_bounds;
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
                                       current_state->translate_x,
                                       current_state->translate_y,
                                       gtk_snapshot_collect_blend_top);
  top_state->data.blend.blend_mode = blend_mode;

  gtk_snapshot_push_state (snapshot,
                           top_state->translate_x,
                           top_state->translate_y,
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
                                       current_state->translate_x,
                                       current_state->translate_y,
                                       gtk_snapshot_collect_cross_fade_end);
  end_state->data.cross_fade.progress = progress;

  gtk_snapshot_push_state (snapshot,
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
                              state->n_nodes);

  /* The collect func may not modify the state stack... */
  g_assert (state_index == snapshot->state_stack->len - 1);

  /* Remove all the state's nodes from the list of nodes */
  g_assert (state->start_node_index + state->n_nodes == snapshot->nodes->len);
  g_ptr_array_remove_range (snapshot->nodes,
                            snapshot->nodes->len - state->n_nodes,
                            state->n_nodes);

  gtk_snapshot_state_array_remove_index (snapshot->state_stack, state_index);

  return node;
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

  /* We should have exactly our initial state */
  if (snapshot->state_stack->len > 1)
    {
      g_warning ("Too many gtk_snapshot_push() calls. %u states remaining.", snapshot->state_stack->len);
    }
  
  result = gtk_snapshot_pop_internal (snapshot);

  gtk_snapshot_state_array_free (snapshot->state_stack);
  snapshot->state_stack = NULL;

  g_ptr_array_free (snapshot->nodes, TRUE);
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
  gsk_render_node_get_bounds (node, &bounds);
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

void
gtk_snapshot_append_node_internal (GtkSnapshot   *snapshot,
                                   GskRenderNode *node)
{
  GtkSnapshotState *current_state;

  current_state = gtk_snapshot_get_current_state (snapshot);

  if (current_state)
    {
      if (gsk_render_node_get_node_type (node) == GSK_CONTAINER_NODE)
        {
          guint i, p;

          for (i = 0, p = gsk_container_node_get_n_children (node); i < p; i ++)
            g_ptr_array_add (snapshot->nodes,
                             gsk_render_node_ref (gsk_container_node_get_child (node, i)));

          current_state->n_nodes += p;
          /* Don't unref @node... */
        }
      else
        {
          g_ptr_array_add (snapshot->nodes, gsk_render_node_ref (node));
          current_state->n_nodes ++;
        }
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

  gtk_snapshot_push_offset (snapshot);
  gtk_snapshot_append_node_internal (snapshot, node);
  gtk_snapshot_pop (snapshot);
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
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  cairo_t *cr;

  g_return_val_if_fail (snapshot != NULL, NULL);
  g_return_val_if_fail (bounds != NULL, NULL);

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &real_bounds);

  node = gsk_cairo_node_new (&real_bounds);

  gtk_snapshot_append_node_internal (snapshot, node);
  gsk_render_node_unref (node);

  cr = gsk_cairo_node_get_draw_context (node);

  cairo_translate (cr, current_state->translate_x, current_state->translate_y);

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
  GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GskRenderNode *node;
  graphene_rect_t real_bounds;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (GDK_IS_TEXTURE (texture));
  g_return_if_fail (bounds != NULL);

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &real_bounds);
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
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GskRenderNode *node;
  graphene_rect_t real_bounds;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (color != NULL);
  g_return_if_fail (bounds != NULL);


  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &real_bounds);

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
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  graphene_point_t real_start_point;
  graphene_point_t real_end_point;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (start_point != NULL);
  g_return_if_fail (end_point != NULL);
  g_return_if_fail (stops != NULL);
  g_return_if_fail (n_stops > 1);

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &real_bounds);
  real_start_point.x = start_point->x + current_state->translate_x;
  real_start_point.y = start_point->y + current_state->translate_y;
  real_end_point.x = end_point->x + current_state->translate_x;
  real_end_point.y = end_point->y + current_state->translate_y;

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
  const GtkSnapshotState *current_state = gtk_snapshot_get_current_state (snapshot);
  GskRenderNode *node;
  graphene_rect_t real_bounds;
  graphene_point_t real_start_point;
  graphene_point_t real_end_point;

  g_return_if_fail (snapshot != NULL);
  g_return_if_fail (start_point != NULL);
  g_return_if_fail (end_point != NULL);
  g_return_if_fail (stops != NULL);
  g_return_if_fail (n_stops > 1);

  graphene_rect_offset_r (bounds, current_state->translate_x, current_state->translate_y, &real_bounds);
  real_start_point.x = start_point->x + current_state->translate_x;
  real_start_point.y = start_point->y + current_state->translate_y;
  real_end_point.x = end_point->x + current_state->translate_x;
  real_end_point.y = end_point->y + current_state->translate_y;

  node = gsk_repeating_linear_gradient_node_new (&real_bounds,
                                                 &real_start_point,
                                                 &real_end_point,
                                                 stops,
                                                 n_stops);

  gtk_snapshot_append_node_internal (snapshot, node);
  gsk_render_node_unref (node);
}
