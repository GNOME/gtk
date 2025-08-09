/*
 * Copyright © 2025 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "path-paintable-private.h"

/* This is an experiment into seeing how much metadata is needed
 * on top of a set of paths to make interesting animated icons.
 *
 * The paths can be stroked or filled (or both), using either
 * fixed or symbolic colors. Stroking takes the CSS font weight
 * into account for adjusting stroke width up or down.
 *
 * The PathPaintable object has a state, and each path is tagged
 * to be drawn in a number of states, and not in others. Whenever
 * the state changes, we transition from the old set of paths to
 * the new set.
 *
 * The transition has two (possibly overlapping) phases:
 * - animating out the paths that are no longer present
 * - animating in the paths that are newly present
 *
 * The duration of the phases, the delay between them and the
 * easing function are settable. There is a number of choices
 * for the effect used to transition each path:
 * - no effect
 * - animated stroke
 * - animated blur
 *
 * For animated strokes, the origin of the stroke can be either
 * at the start or end of the path, or at an arbirary position
 * along the path. When the position is not at the start or end,
 * the animation will proceed in both directions from the origin.
 *
 * Finally, a path can be attached to position on another path.
 * In this case, the attached path will be moved along with its
 * attachment point during transition.
 *
 * Limitations:
 * - fills cannot be animated
 * - attached paths cannot have transition effects
 */

typedef struct
{
  GskPath *path;

  guint64 states;

  StateTransition transition;
  float origin;

  gboolean stroke;
  float stroke_width;
  guint stroke_symbolic;
  GdkRGBA stroke_color;
  GskLineCap stroke_linecap;
  GskLineJoin stroke_linejoin;

  gboolean fill;
  GskFillRule fill_rule;
  guint fill_symbolic;
  GdkRGBA fill_color;

  gsize attach_to;
  float attach_pos;
} PathElt;

typedef struct
{
  GtkSnapshot *snapshot;
  double width;
  double height;
  const GdkRGBA *colors;
  gsize n_colors;
  double weight;
} PaintData;

struct _PathPaintable
{
  GObject parent_instance;
  GArray *paths;

  double width, height;

  graphene_rect_t bounds;

  gboolean animating;
  gint64 start_time;
  float duration;
  float delay;

  EasingFunction easing;

  guint state;
  guint max_state;
  guint old_state;
};

struct _PathPaintableClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_DURATION = 1,
  PROP_DELAY,
  PROP_EASING,
  PROP_STATE,
  PROP_MAX_STATE,
  NUM_PROPERTIES,
};

enum
{
  CHANGED,
  PATHS_CHANGED,
  LAST_SIGNAL,
};

static GParamSpec *properties[NUM_PROPERTIES];
static guint signals[LAST_SIGNAL];

/* {{{ Helpers */

static void
clear_path_elt (gpointer data)
{
  PathElt *elt = data;

  gsk_path_unref (elt->path);
}

static struct {
  EasingFunction easing;
  float params[4];
} easing_funcs[] = {
  { EASING_FUNCTION_LINEAR, { 0, 0, 1, 1 } },
  { EASING_FUNCTION_EASE_IN_OUT, { 0.42, 0, 0.58, 1 } },
  { EASING_FUNCTION_EASE_IN, { 0.42, 0, 1, 1 } },
  { EASING_FUNCTION_EASE_OUT, { 0, 0, 0.58, 1 } },
  { EASING_FUNCTION_EASE, { 0.25, 0.1, 0.25, 1 } },
};

static float
apply_easing (float *params,
              float  progress)
{
  static const float epsilon = 0.00001;
  float tmin, t, tmax;
  float x1, y1, x2, y2;

  x1 = params[0];
  y1 = params[1];
  x2 = params[2];
  y2 = params[3];

  if (progress <= 0)
    return 0;
  if (progress >= 1)
    return 1;

  tmin = 0.0;
  tmax = 1.0;
  t = progress;

  while (tmin < tmax)
    {
      float sample;

      sample = (((1.0 + 3 * x1 - 3 * x2) * t
                +      -6 * x1 + 3 * x2) * t
                +       3 * x1         ) * t;
      if (fabsf (sample - progress) < epsilon)
        break;

      if (progress > sample)
        tmin = t;
      else
        tmax = t;
      t = (tmax + tmin) * .5;
    }

  return (((1.0 + 3 * y1 - 3 * y2) * t
          +      -6 * y1 + 3 * y2) * t
          +       3 * y1         ) * t;
}

/* There's two important corner cases here:
 * - no path is in the 'unset' state
 * - if elt->states is 0xffffffff, the path is inert
 *   and is part of all states (except for the 'unset' one)
 */
static gboolean
path_is_in_state (PathElt *elt,
                  guint    state)
{
  if (state == STATE_UNSET)
    return FALSE;

  if ((elt->states & (1ull << state)) != 0)
    return TRUE;

  return FALSE;
}

static gboolean
has_change_for_out (PathPaintable *self,
                    guint          from,
                    guint          to)
{
  for (guint i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (path_is_in_state (elt, from) &&
          !path_is_in_state (elt, to))
        return TRUE;
    }

  return FALSE;
}

static gboolean
has_change_for_in (PathPaintable *self,
                   guint          from,
                   guint          to)
{
  for (guint i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (!path_is_in_state (elt, from) &&
          path_is_in_state (elt, to))
        return TRUE;
    }

  return FALSE;
}

static GskPath *
path_get_segment (GskPath *path,
                  float    start,
                  float    end)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();
  if (start <= 0 && end >= 1)
    {
      gsk_path_builder_add_path (builder, path);
      return gsk_path_builder_free_to_path (builder);
    }

  if (end > start)
    {
      g_autoptr (GskPathMeasure) measure = NULL;
      float length;
      GskPathPoint start_point, end_point;

      measure = gsk_path_measure_new (path);
      length = gsk_path_measure_get_length (measure);

      if (gsk_path_measure_get_point (measure, start * length, &start_point) &&
          gsk_path_measure_get_point (measure, end * length, &end_point))
        gsk_path_builder_add_segment (builder, path, &start_point, &end_point);
    }

  return gsk_path_builder_free_to_path (builder);
}

static GskStroke *
get_stroke_for_path (PathElt *elt,
                     double   weight)
{
  GskStroke *stroke;

  stroke = gsk_stroke_new (elt->stroke_width * weight / 400.0);
  gsk_stroke_set_line_cap (stroke, elt->stroke_linecap);
  gsk_stroke_set_line_join (stroke, elt->stroke_linejoin);

  return stroke;
}

static void
recompute_bounds (PathPaintable *self)
{
  graphene_rect_t bounds;

  for (guint i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (elt->path != NULL && !gsk_path_is_empty (elt->path))
        {
          graphene_rect_t bd;
          g_autoptr (GskStroke) stroke = get_stroke_for_path (elt, 1000);

          if (gsk_path_get_stroke_bounds (elt->path, stroke, &bd))
            {
              if (i > 0)
                graphene_rect_union (&bd, &bounds, &bounds);
              else
                graphene_rect_init_from_rect (&bounds, &bd);
            }
        }
    }

  if (!graphene_rect_equal (&self->bounds, &bounds))
    {
      graphene_rect_init_from_rect (&self->bounds, &bounds);
      gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
    }
}

static void
get_fill_color_for_path (PathPaintable *self,
                         PathElt       *elt,
                         GdkRGBA       *c,
                         PaintData     *data)
{
  if (elt->fill_symbolic < data->n_colors)
    {
      *c = data->colors[elt->fill_symbolic];
      c->alpha *= elt->fill_color.alpha;
    }
  else
    {
      *c = elt->fill_color;
    }
}

static void
get_stroke_color_for_path (PathPaintable *self,
                           PathElt       *elt,
                           GdkRGBA       *c,
                           PaintData     *data)
{
  if (elt->stroke_symbolic < data->n_colors)
    {
      *c = data->colors[elt->stroke_symbolic];
      c->alpha *= elt->stroke_color.alpha;
    }
  else
    {
      *c = elt->stroke_color;
    }
}

static gboolean
needs_animating (PathPaintable *self)
{
  guint signal_id;

  signal_id = g_signal_lookup ("invalidate-contents", GDK_TYPE_PAINTABLE);
  return g_signal_has_handler_pending (self, signal_id, 0, TRUE);
}

static guint
compute_max_state (PathPaintable *self)
{
  guint max = 0;

  for (gsize idx = 0; idx < self->paths->len; idx++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, idx);

      if (elt->states == ALL_STATES)
        continue;

      max = MAX (max, g_bit_nth_msf (elt->states, -1));
    }

  return max;
}

static gboolean
path_equal (GskPath *p1,
            GskPath *p2)
{
  g_autofree char *s1 = gsk_path_to_string (p1);
  g_autofree char *s2 = gsk_path_to_string (p2);

  return strcmp (s1, s2) == 0;
}

static gboolean
path_elt_equal (PathElt *elt1,
                PathElt *elt2)
{
  if (elt1->states != elt2->states)
    return FALSE;

  if (elt1->transition != elt2->transition ||
      elt1->origin != elt2->origin)
    return FALSE;

  if (elt1->stroke != elt2->stroke ||
      elt1->stroke_width != elt2->stroke_width ||
      elt1->stroke_symbolic != elt2->stroke_symbolic ||
      elt1->stroke_linecap != elt2->stroke_linecap ||
      elt1->stroke_linejoin != elt2->stroke_linejoin)
    return FALSE;

  if (elt1->stroke_symbolic == 0xffff)
    {
      if (!gdk_rgba_equal (&elt1->stroke_color, &elt2->stroke_color))
        return FALSE;
    }

  if (elt1->fill != elt2->fill ||
      elt1->fill_rule != elt2->fill_rule ||
      elt1->fill_symbolic != elt2->fill_symbolic)
    return FALSE;

  if (elt1->fill_symbolic == 0xffff)
    {
      if (!gdk_rgba_equal (&elt1->fill_color, &elt2->fill_color))
        return FALSE;
    }

  if (elt1->attach_to != elt2->attach_to ||
      elt1->attach_pos != elt2->attach_pos)
    return FALSE;

  if (!path_equal (elt1->path, elt2->path))
    return FALSE;

  return TRUE;
}

/* }}} */
/* {{{ Painting */

static void
fill_path (PathPaintable *self,
           PathElt       *elt,
           GskPath       *path,
           PaintData     *data)
{
  GdkRGBA c;

  if (!elt->fill)
    return;

  get_fill_color_for_path (self, elt, &c, data);

  gtk_snapshot_push_fill (data->snapshot, path, elt->fill_rule);
  gtk_snapshot_append_color (data->snapshot, &c, &self->bounds);
  gtk_snapshot_pop (data->snapshot);
}

static void
stroke_path (PathPaintable *self,
             PathElt       *elt,
             GskPath       *path,
             PaintData     *data)
{
  GskStroke *stroke;
  GdkRGBA c;

  if (!elt->stroke)
    return;

  stroke = get_stroke_for_path (elt, data->weight);
  get_stroke_color_for_path (self, elt, &c, data);

  gtk_snapshot_push_stroke (data->snapshot, path, stroke);
  gtk_snapshot_append_color (data->snapshot, &c, &self->bounds);
  gtk_snapshot_pop (data->snapshot);

  gsk_stroke_free (stroke);
}

static void
paint_elt (PathPaintable *self,
           PathElt       *elt,
           float          t_out,
           float          t_in,
           PaintData     *data)
{
  PathElt *base = &g_array_index (self->paths, PathElt, elt->attach_to);
  gboolean in_old_state;
  gboolean in_state;
  float pos;
  g_autoptr (GskPathMeasure) measure = NULL;
  float length;
  GskPathPoint point;
  graphene_point_t orig_pos, adjusted_pos;
  float orig_angle, adjusted_angle;
  g_autoptr (GskTransform) transform = NULL;

  if (elt->attach_to == (gsize) -1)
    {
      fill_path (self, elt, elt->path, data);
      stroke_path (self, elt, elt->path, data);
      return;
    }

  in_old_state = path_is_in_state (base, self->old_state);
  in_state = path_is_in_state (base, self->state);

  if (in_old_state && !in_state)
    {
      /* base is disappearing */
      switch (base->transition)
        {
        case STATE_TRANSITION_NONE:
        case STATE_TRANSITION_BLUR:
          pos = elt->attach_pos;
          break;
        case STATE_TRANSITION_ANIMATE:
          if (base->origin == 0)
            pos = elt->attach_pos * (1 - t_out);
          else if (base->origin == 1)
            pos = 1 - (1 - t_out) * (1 - elt->attach_pos);
          else if (elt->attach_pos > base->origin)
            pos = base->origin + (elt->attach_pos - base->origin) * (1 - t_out);
          else
            pos = base->origin - (base->origin - elt->attach_pos) * (1 - t_out);
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else if (!in_old_state && in_state)
    {
      /* base is appearing */
      switch (base->transition)
        {
        case STATE_TRANSITION_NONE:
        case STATE_TRANSITION_BLUR:
          pos = elt->attach_pos;
          break;
        case STATE_TRANSITION_ANIMATE:
          if (base->origin == 0)
            pos = elt->attach_pos * t_in;
          else if (base->origin == 1)
            pos = 1 - t_in * (1 - elt->attach_pos);
          else if (elt->attach_pos > base->origin)
            pos = base->origin + (elt->attach_pos - base->origin) * t_in;
          else
            pos = base->origin - (base->origin - elt->attach_pos) * t_in;
          break;
        default:
          g_assert_not_reached ();
        }
    }
  else
    pos = elt->attach_pos;

  /* Now pos is elt->attach_pos, adjusted for t_out/t_in */

  gsk_path_get_start_point (elt->path, &point);
  gsk_path_point_get_position (&point, elt->path, &orig_pos);
  orig_angle = 0; /* FIXME */

  measure = gsk_path_measure_new (base->path);
  length = gsk_path_measure_get_length (measure);

  gsk_path_measure_get_point (measure, length * pos, &point);
  gsk_path_point_get_position (&point, base->path, &adjusted_pos);
  adjusted_angle = gsk_path_point_get_rotation (&point, base->path, GSK_PATH_TO_END);

  /* Now determine that transform that moves orig_pos to adjusted_pos
   * and rotates orig_dir to adjusted_dir
   */

  transform =
    gsk_transform_translate (
        gsk_transform_rotate (
            gsk_transform_translate (NULL, &adjusted_pos),
            adjusted_angle - orig_angle),
        &GRAPHENE_POINT_INIT (-orig_pos.x, -orig_pos.y));

  gtk_snapshot_save (data->snapshot);

  gtk_snapshot_transform (data->snapshot, transform);

  fill_path (self, elt, elt->path, data);
  stroke_path (self, elt, elt->path, data);

  gtk_snapshot_restore (data->snapshot);
}

static void
paint_elt_partial (PathPaintable *self,
                   PathElt       *elt,
                   float          start,
                   float          end,
                   PaintData     *data)
{
  g_autoptr (GskPath) path = NULL;

  path = path_get_segment (elt->path, start, end);
  /* TODO: fill */
  stroke_path (self, elt, path, data);
}

/* This is doing an animated blur together with alpha
 * thresholding to achieve a 'blobbing' effect.
 *
 * If we transition to or from the 'unset' state,
 * we also add a fade out.
 */
static void
paint_elt_with_blur_effect (PathPaintable *self,
                            PathElt       *elt,
                            float          t_linear,
                            float          t,
                            float          t_overlap,
                            PaintData     *data)
{
  g_autoptr (GskComponentTransfer) identity = NULL;
  g_autoptr (GskComponentTransfer) alpha = NULL;
  float blur = t * CLAMP (MAX (data->width, data->height) / 2, 0, 64);

  identity = gsk_component_transfer_new_identity ();
  alpha = gsk_component_transfer_new_discrete (5, (float []) { 0, 1, 1, 1, 1 });

#if 0
  if (self->state == STATE_UNSET || self->old_state == STATE_UNSET)
    {
      float params[] = { .19, .99, .08, .99 };

      gtk_snapshot_push_opacity (data->snapshot, apply_easing (params, 1 - t_linear));
    }
  else
#endif
    gtk_snapshot_push_opacity (data->snapshot, 1 - t_overlap);

  gtk_snapshot_push_component_transfer (data->snapshot, identity, identity, identity, alpha);
  gtk_snapshot_push_blur (data->snapshot, blur);
  paint_elt (self, elt, 0, 1, data);
  gtk_snapshot_pop (data->snapshot);
  gtk_snapshot_pop (data->snapshot);
  gtk_snapshot_pop (data->snapshot);
}

static void
paint_for_transition (PathPaintable *self,
                      PaintData     *data)
{
  gint64 now;
  float t_out_linear, t_out;
  float t_in_linear, t_in;
  float t_overlap;

  now = g_get_monotonic_time ();

  t_out_linear = (now - self->start_time) / (self->duration * G_TIME_SPAN_SECOND);
  t_in_linear = (now - self->start_time - (self->delay * G_TIME_SPAN_SECOND)) / (self->duration * G_TIME_SPAN_SECOND);

  t_overlap = ((now - self->start_time) - (self->delay * G_TIME_SPAN_SECOND)) / ((self->duration - self->delay) * G_TIME_SPAN_SECOND);

  t_out_linear = CLAMP (t_out_linear, 0, 1);
  t_in_linear = CLAMP (t_in_linear, 0, 1);
  t_overlap = CLAMP (t_overlap, 0, 1);

  t_out = apply_easing (easing_funcs[self->easing].params, t_out_linear);
  t_in = apply_easing (easing_funcs[self->easing].params, t_in_linear);

  for (guint i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);
      gboolean in_old_state = path_is_in_state (elt, self->old_state);
      gboolean in_new_state = path_is_in_state (elt, self->state);

      if (in_old_state && in_new_state)
        {
          /* not animating */
          paint_elt (self, elt, 0, 1, data);
        }
      else if (in_old_state && !in_new_state && t_out <= 1)
        {
          /* disappearing */
          switch (elt->transition)
            {
            case STATE_TRANSITION_NONE:
              if (t_out < 1)
                paint_elt (self, elt, t_out, t_in, data);
              break;
            case STATE_TRANSITION_BLUR:
              if (t_out < 1)
                paint_elt_with_blur_effect (self, elt, t_out_linear, t_out, t_overlap, data);
              break;
            case STATE_TRANSITION_ANIMATE:
              if (elt->origin == 0)
                paint_elt_partial (self, elt, 0, 1 - t_out, data);
              else if (elt->origin == 1)
                paint_elt_partial (self, elt, t_out, 1, data);
              else
                paint_elt_partial (self, elt, elt->origin * t_out, 1 - (1 - elt->origin) * t_out, data);
              break;
            default:
              g_assert_not_reached ();
            }
        }
      else if (!in_old_state && in_new_state && t_in >= 0)
        {
          /* appearing */
          switch (elt->transition)
            {
            case STATE_TRANSITION_NONE:
              if (t_in > 0)
                paint_elt (self, elt, t_out, t_in, data);
              break;
            case STATE_TRANSITION_BLUR:
              if (t_in > 0)
                paint_elt_with_blur_effect (self, elt, 1 - t_in_linear, 1 - t_in, 1 - t_overlap, data);
              break;
            case STATE_TRANSITION_ANIMATE:
              if (elt->origin == 0)
                paint_elt_partial (self, elt, 0, t_in, data);
              else if (elt->origin == 1)
                paint_elt_partial (self, elt, 1 - t_in, 1, data);
              else
                paint_elt_partial (self, elt, (1 - t_in) * elt->origin, 1 - (1 - t_in) * (1 - elt->origin), data);
              break;
            default:
              g_assert_not_reached ();
            }
        }
    }

  if (self->animating)
    g_idle_add_once ((GSourceOnceFunc) gdk_paintable_invalidate_contents, self);

  if (t_out == 1)
    {
      /* If nothing is animating out, we're done */
      if (!has_change_for_in (self, self->old_state, self->state))
        self->animating = FALSE;
    }

  if (t_in == 1)
    self->animating = FALSE;
}

static void
paint_for_state (PathPaintable *self,
                 PaintData     *data)
{
  for (guint i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (path_is_in_state (elt, self->state))
        paint_elt (self, elt, 0, 1, data);
    }
}

/* }}} */
/* {{{ GtkSymbolicPaintable implementation */

static void
path_paintable_snapshot_with_weight (GtkSymbolicPaintable  *paintable,
                                     GtkSnapshot           *snapshot,
                                     double                 width,
                                     double                 height,
                                     const GdkRGBA         *colors,
                                     gsize                  n_colors,
                                     double                 weight)
{
  PathPaintable *self = PATH_PAINTABLE (paintable);
  PaintData data;
  float scale;

  data.snapshot = snapshot;
  data.width = width;
  data.height = height;
  data.colors = colors;
  data.n_colors = n_colors;
  data.weight = weight;

  scale = MIN (width / self->width, height / self->height);

  gtk_snapshot_save (snapshot);

  gtk_snapshot_scale (snapshot, scale, scale);

  if (self->animating)
    paint_for_transition (self, &data);
  else
    paint_for_state (self, &data);

  gtk_snapshot_restore (snapshot);
}

static void
path_paintable_snapshot_symbolic (GtkSymbolicPaintable  *paintable,
                                  GtkSnapshot           *snapshot,
                                  double                 width,
                                  double                 height,
                                  const GdkRGBA         *colors,
                                  gsize                  n_colors)
{
  path_paintable_snapshot_with_weight (paintable,
                                       snapshot,
                                       width, height,
                                       colors, n_colors,
                                       400);
}

static void
path_paintable_init_symbolic_paintable_interface (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = path_paintable_snapshot_symbolic;
  iface->snapshot_with_weight = path_paintable_snapshot_with_weight;
}

/* }}} */
/* {{{ GdkPaintable implementation */

static void
path_paintable_snapshot (GdkPaintable  *paintable,
                         GtkSnapshot   *snapshot,
                         double         width,
                         double         height)
{
  path_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable),
                                    snapshot,
                                    width, height,
                                    NULL, 0);
}

static int
path_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  PathPaintable *self = PATH_PAINTABLE (paintable);

  return ceil (self->width);
}

static int
path_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  PathPaintable *self = PATH_PAINTABLE (paintable);

  return ceil (self->height);
}

static void
path_paintable_init_paintable_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = path_paintable_snapshot;
  iface->get_intrinsic_width = path_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = path_paintable_get_intrinsic_height;
}

/* }}} */
/* {{{ GObject boilerplate */

G_DEFINE_TYPE_WITH_CODE (PathPaintable, path_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                path_paintable_init_paintable_interface)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                path_paintable_init_symbolic_paintable_interface))

static void
path_paintable_init (PathPaintable *self)
{
  self->paths = g_array_new (FALSE, TRUE, sizeof (PathElt));
  g_array_set_clear_func (self->paths, clear_path_elt);

  self->duration = 0.40;
  self->delay = 0.35;
  self->easing = EASING_FUNCTION_EASE_IN;
  self->state = 0;
  self->max_state = STATE_UNSET;
}

static void
path_paintable_dispose (GObject *object)
{
  PathPaintable *self = PATH_PAINTABLE (object);

  g_array_unref (self->paths);

  G_OBJECT_CLASS (path_paintable_parent_class)->dispose (object);
}

static void
path_paintable_get_property (GObject    *object,
                             guint       property_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  PathPaintable *self = PATH_PAINTABLE (object);

  switch (property_id)
    {
    case PROP_DURATION:
      g_value_set_float (value, self->duration);
      break;

    case PROP_DELAY:
      g_value_set_float (value, self->delay);
      break;

    case PROP_EASING:
      g_value_set_uint (value, self->easing);
      break;

    case PROP_STATE:
      g_value_set_uint (value, self->state);
      break;

    case PROP_MAX_STATE:
      g_value_set_uint (value, self->max_state);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
path_paintable_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PathPaintable *self = PATH_PAINTABLE (object);

  switch (property_id)
    {
    case PROP_DURATION:
      path_paintable_set_duration (self, g_value_get_float (value));
      break;

    case PROP_DELAY:
      path_paintable_set_delay (self, g_value_get_float (value));
      break;

    case PROP_EASING:
      path_paintable_set_easing (self, g_value_get_uint (value));
      break;

    case PROP_STATE:
      path_paintable_set_state (self, g_value_get_uint (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
path_paintable_class_init (PathPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = path_paintable_dispose;
  object_class->get_property = path_paintable_get_property;
  object_class->set_property = path_paintable_set_property;

  /**
   * PathPaintable:duration:
   *
   * The length of each transition phase, in seconds.
   *
   * Note that the transition between two states has
   * two phases:
   *
   * - fading out disappearing paths
   * - fading in appearing paths
   */
  properties[PROP_DURATION] =
    g_param_spec_float ("duration", NULL, NULL,
                        0, G_MAXFLOAT, 0.40,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * PathPaintable:delay:
   *
   * The delay between the two transition phases, in seconds.
   *
   * If the delay is shorter than the duration, there will be
   * some overlap between the fade-out and the fade-in.
   */
  properties[PROP_DELAY] =
    g_param_spec_float ("delay", NULL, NULL,
                        0, G_MAXFLOAT, 0.35,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * PathPaintable:easing:
   *
   * The easing function to use for transitions.
   */
  properties[PROP_EASING] =
    g_param_spec_uint ("easing", NULL, NULL,
                       0, G_MAXUINT, EASING_FUNCTION_EASE_IN,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * PathPaintable:state:
   *
   * The current state of the paintable.
   *
   * This can be a number between 0 and the maximum state
   * of the paintable, or the special value `(guint) -1`
   * to indicate the 'none' state in which nothing is drawn.
   */
  properties[PROP_STATE] =
    g_param_spec_uint ("state", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * PathPaintable:max-state:
   *
   * The maximum state of the paintable.
   */
  properties[PROP_MAX_STATE] =
    g_param_spec_uint ("max-state", NULL, NULL,
                       0, G_MAXUINT - 1, 0,
                       G_PARAM_READABLE | G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  /**
   * PathPaintable::changed:
   *
   * Emitted when the paintable changes in any way that
   * would change the serialization.
   */
  signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

  /**
   * PathPaintable::paths-changed:
   *
   * Emitted when the paintable changes in any way that
   * affects the mapping between indices and paths, i.e.
   * when paths are added, removed or reordered.
   */
  signals[PATHS_CHANGED] =
    g_signal_new ("paths-changed",
                  G_TYPE_FROM_CLASS (object_class),
                  G_SIGNAL_RUN_LAST,
                  0,
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

/* }}} */
 /* {{{ Private API */

PathPaintable *
path_paintable_new (void)
{
  return g_object_new (PATH_PAINTABLE_TYPE, NULL);
}

void
path_paintable_set_size (PathPaintable *self,
                         double         width,
                         double         height)
{
  self->width = width;
  self->height = height;

  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_signal_emit (self, signals[CHANGED], 0);
}

double
path_paintable_get_width (PathPaintable *self)
{
  return self->width;
}

double
path_paintable_get_height (PathPaintable *self)
{
  return self->height;
}

gsize
path_paintable_add_path (PathPaintable *self,
                         GskPath       *path)
{
  PathElt elt;
  graphene_rect_t bounds;
  GskStroke *stroke;

  elt.path = gsk_path_ref (path);

  elt.states = ALL_STATES;

  elt.transition = STATE_TRANSITION_NONE;
  elt.origin = 0;

  elt.fill = FALSE;
  elt.fill_rule = GSK_FILL_RULE_WINDING;
  elt.fill_symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
  elt.fill_color = (GdkRGBA) { 0, 0, 0, 1 };

  elt.stroke = TRUE;
  elt.stroke_width = 2;
  elt.stroke_linecap = GSK_LINE_CAP_ROUND;
  elt.stroke_linejoin = GSK_LINE_JOIN_ROUND;
  elt.stroke_symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
  elt.stroke_color = (GdkRGBA) { 0, 0, 0, 1 };

  elt.attach_to = (gsize) -1;
  elt.attach_pos = 0;

  g_array_append_val (self->paths, elt);

  stroke = get_stroke_for_path (&elt, 1000);

  if (gsk_path_get_stroke_bounds (path, stroke, &bounds))
    {
      if (self->paths->len > 1)
        graphene_rect_union (&bounds, &self->bounds, &self->bounds);
      else
        graphene_rect_init_from_rect (&self->bounds, &bounds);

      gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
    }

  gsk_stroke_free (stroke);

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);

  return self->paths->len - 1;
}

void
path_paintable_delete_path (PathPaintable *self,
                            gsize          idx)
{
  for (gsize i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (elt->attach_to == (gsize) -1)
        continue;

      if (elt->attach_to == idx)
        elt->attach_to = (gsize) -1;
      else if (elt->attach_to > idx)
        elt->attach_to -= 1;
    }

  g_array_remove_index (self->paths, idx);

  self->max_state = STATE_UNSET;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_STATE]);

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);
}

void
path_paintable_move_path (PathPaintable *self,
                          gsize          idx,
                          gsize          new_pos)
{
  PathElt tmp;

  g_return_if_fail (idx < self->paths->len);
  g_return_if_fail (new_pos < self->paths->len);

  if (new_pos == idx)
    return;

  for (gsize i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (elt->attach_to == (gsize) -1)
        continue;

      if (elt->attach_to == idx)
        elt->attach_to = new_pos;
      else if (idx < elt->attach_to && elt->attach_to <= new_pos)
        elt->attach_to -= 1;
      else if (elt->attach_to >= new_pos && elt->attach_to < idx)
        elt->attach_to += 1;
    }

  tmp = g_array_index (self->paths, PathElt, idx);
  gsk_path_ref (tmp.path);

  g_array_remove_index (self->paths, idx);
  g_array_insert_val (self->paths, new_pos, tmp);

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);
}

void
path_paintable_set_path (PathPaintable *self,
                         gsize          idx,
                         GskPath       *path)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  g_clear_pointer (&elt->path, gsk_path_unref);
  elt->path = gsk_path_ref (path);

  recompute_bounds (self);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_states (PathPaintable *self,
                                gsize          idx,
                                guint64        states)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->states == states)
    return;

  elt->states = states;
  self->max_state = STATE_UNSET;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_STATE]);
  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_transition (PathPaintable   *self,
                                    gsize            idx,
                                    StateTransition  transition)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->transition == transition)
    return;

  elt->transition = transition;

  if (elt->fill && elt->transition == STATE_TRANSITION_ANIMATE)
    g_warning ("Can't currently transition fills");

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_origin (PathPaintable *self,
                                gsize          idx,
                                float          origin)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->origin == origin)
    return;

  elt->origin = origin;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_fill (PathPaintable   *self,
                              gsize            idx,
                              gboolean         do_fill,
                              GskFillRule      rule,
                              guint            symbolic,
                              const GdkRGBA   *color)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->fill == do_fill &&
      elt->fill_rule == rule &&
      elt->fill_symbolic == symbolic &&
      (symbolic != 0xffff || gdk_rgba_equal (&elt->fill_color, color)))
    return;

  elt->fill = do_fill;
  elt->fill_rule = rule;
  elt->fill_symbolic = symbolic;
  elt->fill_color = *color;

  if (elt->fill && elt->transition == STATE_TRANSITION_ANIMATE)
    g_warning ("Can't currently transition fills");

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_stroke (PathPaintable *self,
                                gsize          idx,
                                gboolean       do_stroke,
                                GskStroke     *stroke,
                                guint          symbolic,
                                const GdkRGBA *color)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->stroke == do_stroke &&
      elt->stroke_width == gsk_stroke_get_line_width (stroke) &&
      elt->stroke_linecap == gsk_stroke_get_line_cap (stroke) &&
      elt->stroke_linejoin == gsk_stroke_get_line_join (stroke) &&
      elt->stroke_symbolic == symbolic &&
      (symbolic != 0xffff || gdk_rgba_equal (&elt->stroke_color, color)))
    return;

  elt->stroke = do_stroke;
  elt->stroke_width = gsk_stroke_get_line_width (stroke);
  elt->stroke_linecap = gsk_stroke_get_line_cap (stroke);
  elt->stroke_linejoin = gsk_stroke_get_line_join (stroke);
  elt->stroke_symbolic = symbolic;
  elt->stroke_color = *color;

  recompute_bounds (self);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_attach_path (PathPaintable *self,
                            gsize          idx,
                            gsize          to,
                            float          pos)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->attach_to == to && elt->attach_pos == pos)
    return;

  elt->attach_to = to;
  elt->attach_pos = pos;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_get_attach_path (PathPaintable *self,
                                gsize          idx,
                                gsize         *to,
                                float         *pos)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  *to = elt->attach_to;
  *pos = elt->attach_pos;
}

guint
path_paintable_get_state (PathPaintable *self)
{
  return self->state;
}

void
path_paintable_set_duration (PathPaintable *self,
                             float          duration)
{
  g_return_if_fail (duration >= 0);

  if (self->duration == duration)
    return;

  self->duration = duration;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DURATION]);
  g_signal_emit (self, signals[CHANGED], 0);
}

float
path_paintable_get_duration (PathPaintable *self)
{
  return self->duration;
}

void
path_paintable_set_delay (PathPaintable *self,
                          float          delay)
{
  g_return_if_fail (delay >= 0);

  if (self->delay == delay)
    return;

  self->delay = delay;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_DELAY]);
  g_signal_emit (self, signals[CHANGED], 0);
}

float
path_paintable_get_delay (PathPaintable *self)
{
  return self->delay;
}

void
path_paintable_set_easing (PathPaintable  *self,
                           EasingFunction  easing)
{
  if (self->easing == easing)
    return;

  self->easing = easing;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_EASING]);
  g_signal_emit (self, signals[CHANGED], 0);
}

EasingFunction
path_paintable_get_easing (PathPaintable *self)
{
  return self->easing;
}

gsize
path_paintable_get_n_paths (PathPaintable *self)
{
  return self->paths->len;
}

GskPath *
path_paintable_get_path (PathPaintable *self,
                         gsize          idx)
{
  g_return_val_if_fail (idx < self->paths->len, NULL);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->path;
}

guint64
path_paintable_get_path_states (PathPaintable *self,
                                gsize          idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->states;
}

StateTransition
path_paintable_get_path_transition (PathPaintable *self,
                                    gsize          idx)
{
  g_return_val_if_fail (idx< self->paths->len, STATE_TRANSITION_NONE);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition;
}

float
path_paintable_get_path_origin (PathPaintable *self,
                                gsize          idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0.0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->origin;
}

gboolean
path_paintable_get_path_fill (PathPaintable *self,
                              gsize          idx,
                              GskFillRule   *rule,
                              guint         *symbolic,
                              GdkRGBA       *color)
{
  g_return_val_if_fail (idx < self->paths->len, FALSE);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  *rule = elt->fill_rule;
  *symbolic = elt->fill_symbolic;
  *color = elt->fill_color;

  return elt->fill;
}

gboolean
path_paintable_get_path_stroke (PathPaintable *self,
                                gsize          idx,
                                GskStroke     *stroke,
                                guint         *symbolic,
                                GdkRGBA       *color)
{
  g_return_val_if_fail (idx < self->paths->len, FALSE);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  gsk_stroke_set_line_width (stroke, elt->stroke_width);
  gsk_stroke_set_line_cap (stroke, elt->stroke_linecap);
  gsk_stroke_set_line_join (stroke, elt->stroke_linejoin);
  *symbolic = elt->stroke_symbolic;
  *color = elt->stroke_color;

  return elt->stroke;
}

PathPaintable *
path_paintable_copy (PathPaintable *self)
{
  PathPaintable *other;
  g_autoptr (GskStroke) stroke = NULL;
  gboolean do_fill, do_stroke;
  GskFillRule rule = GSK_FILL_RULE_WINDING;
  guint symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
  GdkRGBA color = (GdkRGBA) { 0, 0, 0, 1 };
  gsize to = (gsize) -1;
  float pos = 0;

  other = path_paintable_new ();

  path_paintable_set_size (other, self->width, self->height);
  path_paintable_set_duration (other, self->duration);
  path_paintable_set_delay (other, self->delay);
  path_paintable_set_easing (other, self->easing);

  stroke = gsk_stroke_new (1);
  for (gsize i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      path_paintable_add_path (other, elt->path);
      path_paintable_set_path_states (other, i, path_paintable_get_path_states (self, i));
      path_paintable_set_path_transition (other, i, path_paintable_get_path_transition (self, i));
      path_paintable_set_path_origin (other, i, path_paintable_get_path_origin (self, i));

      do_fill = path_paintable_get_path_fill (self, i, &rule, &symbolic, &color);
      path_paintable_set_path_fill (other, i, do_fill, rule, symbolic, &color);

      do_stroke = path_paintable_get_path_stroke (self, i, stroke, &symbolic, &color);
      path_paintable_set_path_stroke (other, i, do_stroke, stroke, symbolic, &color);

      path_paintable_get_attach_path (self, i, &to, &pos);
      path_paintable_attach_path (other, i, to, pos);
    }

  return other;
}

PathPaintable *
path_paintable_combine (PathPaintable *one,
                        PathPaintable *two)
{
  PathPaintable *res;
  guint max_state;
  gsize n_paths;

  res = path_paintable_copy (one);

  max_state = path_paintable_get_max_state (res);
  n_paths = path_paintable_get_n_paths (res);

  for (gsize i = 0; i < path_paintable_get_n_paths (res); i++)
    {
      guint64 states;

      states = path_paintable_get_path_states (res, i);
      if (states == ALL_STATES)
        {
          states = 0;

          for (gsize j = 0; j <= max_state; j++)
            states = states | (1ull << j);

          path_paintable_set_path_states (res, i, states);
        }
    }

  for (gsize i = 0; i < path_paintable_get_n_paths (two); i++)
    {
      gsize idx;
      guint64 states;
      gboolean do_fill;
      GskFillRule rule;
      guint symbolic;
      GdkRGBA color;
      gboolean do_stroke;
      g_autoptr (GskStroke) stroke = gsk_stroke_new (1);
      gsize attach_to;
      float attach_pos;

      idx = path_paintable_add_path (res, path_paintable_get_path (two, i));

      path_paintable_set_path_transition (res, idx,
                                          path_paintable_get_path_transition (two, i));
      path_paintable_set_path_origin (res, idx,
                                      path_paintable_get_path_origin (two, i));
      states = path_paintable_get_path_states (two, i);
      if (states == ALL_STATES)
        {
          guint max2 = path_paintable_get_max_state (two);

          states = 0;
          for (gsize j = 0; j <= max2; j++)
            states |= 1ull << j;
        }
      path_paintable_set_path_states (res, idx, states << (max_state + 1));

      do_fill = path_paintable_get_path_fill (two, i, &rule, &symbolic, &color);
      path_paintable_set_path_fill (res, idx,
                                    do_fill, rule, symbolic, &color);

      do_stroke = path_paintable_get_path_stroke (two, i, stroke, &symbolic, &color);
      path_paintable_set_path_stroke (res, idx,
                                      do_stroke, stroke, symbolic, &color);

      path_paintable_get_attach_path (two, i, &attach_to, &attach_pos);
      path_paintable_attach_path (res, idx, attach_to + n_paths, attach_pos);
    }

  return res;
}

/* }}} */
/* {{{ Serialization */

static void
path_paintable_save_path (PathElt *elt,
                          gsize    idx,
                          GString *str)
{
  const char *linecap[] = { "butt", "round", "square" };
  const char *linejoin[] = { "miter", "round", "bevel" };
  const char *sym[] = { "foreground", "error", "warning", "success", "accent" };
  const char *transition[] = { "none", "animate", "blur" };

  g_string_append (str, "  <path d='");
  gsk_path_print (elt->path, str);
  g_string_append (str, "'");

  g_string_append_printf (str, "\n        id='path%lu'", idx);

  if (elt->stroke)
    {
      g_string_append (str, "\n        stroke='rgb(0,0,0)'");
      g_string_append_printf (str, "\n        stroke-width='%g'", elt->stroke_width);
      g_string_append_printf (str, "\n        stroke-linecap='%s'", linecap[elt->stroke_linecap]);
      g_string_append_printf (str, "\n        stroke-linejoin='%s'", linejoin[elt->stroke_linejoin]);
      if (elt->stroke_symbolic == 0xffff)
        {
          char *s = gdk_rgba_to_string (&elt->stroke_color);
          g_string_append_printf (str, "\n        gtk:stroke='%s'", s);
          g_free (s);
        }
      else if (elt->stroke_symbolic <= GTK_SYMBOLIC_COLOR_SUCCESS)
        {
          if (elt->stroke_color.alpha < 1)
            g_string_append_printf (str, "\n        stroke-opacity='%g'", elt->stroke_color.alpha);
          g_string_append_printf (str, "\n        gtk:stroke='%s'", sym[elt->stroke_symbolic]);
        }
    }
  else
    {
      g_string_append (str, "\n        stroke='none'");
    }

  if (elt->fill)
    {
      const char *rule[] = { "winding", "evenodd" };
      g_string_append (str, "\n        fill='rgb(0,0,0)'");
      g_string_append_printf (str, "\n        fill-rule='%s'", rule[elt->fill_rule]);
      if (elt->fill_symbolic == 0xffff)
        {
          char *s = gdk_rgba_to_string (&elt->fill_color);
          g_string_append_printf (str, "\n        gtk:fill='%s'", s);
          g_free (s);
        }
      else if (elt->fill_symbolic <= GTK_SYMBOLIC_COLOR_SUCCESS)
        {
          if (elt->fill_color.alpha < 1)
            g_string_append_printf (str, "\n        fill-opacity='%g'", elt->fill_color.alpha);
          g_string_append_printf (str, "\n        gtk:fill='%s'", sym[elt->fill_symbolic]);
        }
    }
  else
    {
      g_string_append (str, "\n        fill='none'");
    }

  if (elt->states)
    {
      g_autofree char *states = states_to_string (elt->states);
      g_string_append_printf (str, "\n        gtk:states='%s'", states);
    }

  g_string_append_printf (str, "\n        gtk:transition='%s'", transition[elt->transition]);

  if (elt->origin != 0)
    {
      g_autofree char *origin = origin_to_string (elt->origin);
      g_string_append_printf (str, "\n        gtk:origin='%s'", origin);
    }

  if (elt->attach_to != (gsize) -1)
    {
      g_autofree char *attach_pos = origin_to_string (elt->attach_pos);

      g_string_append_printf (str, "\n        gtk:attach-to='path%lu'", elt->attach_to);
      g_string_append_printf (str, "\n        gtk:attach-pos='%s'", attach_pos);
    }

  g_string_append (str, "/>\n");
}

static void
path_paintable_save (PathPaintable *self,
                     GString       *str,
                     guint          state)
{
  const char *easing[] = { "linear", "ease-in-out", "ease-in", "ease-out", "ease" };

  g_string_append_printf (str, "<svg width='%g' height='%g'", self->width, self->height);
  g_string_append (str, "\n     xmlns:gtk='https://www.gtk.org/icons'");

  g_string_append_printf (str, "\n     gtk:easing='%s'", easing[self->easing]);
  g_string_append_printf (str, "\n     gtk:duration='%g'", self->duration);
  g_string_append_printf (str, "\n     gtk:delay='%g'", self->delay);
  g_string_append (str, ">\n");

  for (gsize idx = 0; idx < self->paths->len; idx++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, idx);
      if (state == STATE_UNSET || path_is_in_state (elt, state))
        path_paintable_save_path (elt, idx, str);
    }

  g_string_append (str, "</svg>");
}

/*< private >
 * path_paintable_serialize_state:
 * @self: the paintable
 * @state: the state to serialize
 *
 * Serializes the paintable to SVG, including only
 * the paths that are present in the given state.
 *
 * Returns: (transfer full): SVG data
 */
GBytes *
path_paintable_serialize_state (PathPaintable *self,
                                guint          state)
{
  GString *str = g_string_new ("");

  path_paintable_save (self, str, state);

  return g_string_free_to_bytes (str);
}

/* }}} */
/* {{{ Public API */

/**
 * path_paintable_set_state:
 * @self: a paintable
 * @state: the state to set
 *
 * Sets the state of the paintable.
 */
void
path_paintable_set_state (PathPaintable *self,
                          guint          state)
{
  if (self->state == state)
    return;

  self->old_state = self->state;
  self->state = state;

  if (self->duration > 0 && needs_animating (self))
    self->animating = TRUE;

  /* Skip the delay if nothing is animating out */
  if (has_change_for_out (self, self->old_state, self->state))
    self->start_time = g_get_monotonic_time ();
  else
    self->start_time = g_get_monotonic_time () - self->delay * G_TIME_SPAN_SECOND;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
}

/**
 * path_paintable_get_max_state:
 * @self: a paintable
 *
 * Returns the largest value that occurs among the
 * states of the paths in this paintable.
 *
 * Returns: the largest state of the paintable
 */
guint
path_paintable_get_max_state (PathPaintable *self)
{
  if (self->max_state == STATE_UNSET)
    self->max_state = compute_max_state (self);

  return self->max_state;
}

/**
 * path_paintable_serialize:
 * @self: the paintable
 *
 * Serializes the paintable to SVG.
 *
 * Note that the paths from all the states will
 * be drawn over each other.
 *
 * Returns: (transfer full): SVG data
 */
GBytes *
path_paintable_serialize (PathPaintable *self)
{
  GString *str = g_string_new ("");

  path_paintable_save (self, str, STATE_UNSET);

  return g_string_free_to_bytes (str);
}

/**
 * path_paintable_equal:
 * @self: a paintable
 * @other: another paintable
 *
 * Compares two paintables.
 *
 * Note that this compares the persistent data
 * of the paintable, excluding their states.
 *
 * Returns: true if @self and @other are equal
 */
gboolean
path_paintable_equal (PathPaintable *self,
                      PathPaintable *other)
{
  if (self->width != other->width ||
      self->height != other->height)
    return FALSE;

  if (self->duration != other->duration ||
      self->delay != other->delay ||
      self->easing != other->easing)
    return FALSE;

  if (self->paths->len != other->paths->len)
    return FALSE;

  for (gsize i = 0; i < self->paths->len; i++)
    {
      PathElt *elt1 = &g_array_index (self->paths, PathElt, i);
      PathElt *elt2 = &g_array_index (other->paths, PathElt, i);

      if (!path_elt_equal (elt1, elt2))
        return FALSE;
    }

  return TRUE;
}

/* }}} */

/* vim:set foldmethod=marker: */
