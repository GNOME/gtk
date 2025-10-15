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

#include "config.h"

#include "gtkpathpaintable.h"

#include "gtkenums.h"
#include "gtksymbolicpaintable.h"
#include "gtktypebuiltins.h"

#include <math.h>
#include <stdint.h>

/**
 * GtkPathPaintable:
 *
 * A paintable implementation that renders paths, with animations.
 *
 * `GtkPathPaintable` objects are created by parsing a subset of SVG.
 * The subset is similar to traditional symbolic icons, with
 * [extensions](icon-format.html) to support state transitions
 * and animations.
 *
 * A `GtkPathPaintable` fills or strokes paths with symbolic or fixed
 * colors. It can have multiple states, and paths can be included
 * in a subset of the states. The special 'empty' state is always
 * available.
 *
 * To find out what states a path paintable has, use
 * [method@Gtk.PathPaintable.get_max_state]. To set the current state,
 * use [method@Gtk.PathPaintable.set_state].
 *
 * States can have animation, and the transition between different
 * states can also be animated.
 *
 * <table style="margin-left:auto; margin-right:auto">
 *   <tr>
 *     <td>
 *       <figure>
 *         <video alt="A transition" autoplay controls loop muted>
 *           <source src="path-paintable-transition-dark.webm" media="(prefers-color-scheme: dark)">
 *           <source src="path-paintable-transition-light.webm">
 *         </video>
 *         <figcaption>A state transition</figcaption>
 *       </figure>
 *     </td>
 *     <td>
 *       <figure>
 *         <video alt="An animation" autoplay controls loop muted>
 *           <source src="path-paintable-animation-dark.webm" media="(prefers-color-scheme: dark)">
 *           <source src="path-paintable-animation-light.webm">
 *         </video>
 *         <figcaption>An animated state</figcaption>
 *       </figure>
 *     </td>
 *   </tr>
 * </table>
 *
 * `GtkPathPaintable` objects can be created in UI files by setting
 * the [property@Gtk.PathPaintable:resource] property.
 *
 * Since: 4.22
 */

/* {{{ Easing */

static inline double
lerp (double t, double a, double b)
{
  return a + (b - a) * t;
}

static float
apply_easing_params (double *params,
                     double  progress)
{
  static const double epsilon = 0.00001;
  double tmin, t, tmax;
  double x1, y1, x2, y2;

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
      double sample;

      sample = (((1 + 3 * x1 - 3 * x2) * t
                +    -6 * x1 + 3 * x2) * t
                +     3 * x1         ) * t;
      if (fabs (sample - progress) < epsilon)
        break;

      if (progress > sample)
        tmin = t;
      else
        tmax = t;
      t = (tmax + tmin) * .5;
    }

  return (((1 + 3 * y1 - 3 * y2) * t
          +    -6 * y1 + 3 * y2) * t
          +     3 * y1         ) * t;
}

/* }}} */

typedef enum
{
  GTK_PATH_TRANSITION_TYPE_NONE,
  GTK_PATH_TRANSITION_TYPE_ANIMATE,
  GTK_PATH_TRANSITION_TYPE_MORPH,
  GTK_PATH_TRANSITION_TYPE_FADE,
} GtkPathTransitionType;

typedef enum
{
  GTK_EASING_FUNCTION_LINEAR,
  GTK_EASING_FUNCTION_EASE_IN_OUT,
  GTK_EASING_FUNCTION_EASE_IN,
  GTK_EASING_FUNCTION_EASE_OUT,
  GTK_EASING_FUNCTION_EASE,
} GtkEasingFunction;

typedef enum
{
  GTK_PATH_ANIMATION_TYPE_NONE,
  GTK_PATH_ANIMATION_TYPE_AUTOMATIC,
} GtkPathAnimationType;

typedef enum
{
  GTK_PATH_ANIMATION_DIRECTION_NORMAL,
  GTK_PATH_ANIMATION_DIRECTION_ALTERNATE,
  GTK_PATH_ANIMATION_DIRECTION_REVERSE,
  GTK_PATH_ANIMATION_DIRECTION_REVERSE_ALTERNATE,
  GTK_PATH_ANIMATION_DIRECTION_IN_OUT,
  GTK_PATH_ANIMATION_DIRECTION_IN_OUT_ALTERNATE,
  GTK_PATH_ANIMATION_DIRECTION_IN_OUT_REVERSE,
  GTK_PATH_ANIMATION_DIRECTION_SEGMENT,
  GTK_PATH_ANIMATION_DIRECTION_SEGMENT_ALTERNATE,
} GtkPathAnimationDirection;

typedef enum
{
  GTK_CALC_MODE_LINEAR,
  GTK_CALC_MODE_DISCRETE,
  GTK_CALC_MODE_SPLINE,
} GtkCalcMode;

typedef struct
{
  double time;
  double value;
  double params[4];
} GtkKeyFrame;

static float
compute_value (GtkCalcMode  mode,
               GArray      *keyframes,
               float        t)
{
  size_t i;
  GtkKeyFrame *kf0 = NULL;
  GtkKeyFrame *kf1 = NULL;
  float t_rel;

  if (keyframes->len < 2)
    return 0;

  for (i = 1; i < keyframes->len; i++)
    {
      kf0 = &g_array_index (keyframes, GtkKeyFrame, i - 1);
      kf1 = &g_array_index (keyframes, GtkKeyFrame, i);

      if (t < kf1->time)
        break;
    }

  g_assert (kf0 && kf1);

  t_rel = (t - kf0->time) / (kf1->time - kf0->time);

  switch (mode)
    {
    case GTK_CALC_MODE_DISCRETE:
      return kf0->value;
    case GTK_CALC_MODE_LINEAR:
      return lerp (t_rel, kf0->value, kf1->value);
    case GTK_CALC_MODE_SPLINE:
      return lerp (apply_easing_params (kf0->params, t_rel), kf0->value, kf1->value);
    default:
      g_assert_not_reached ();
    }
}

#define GTK_PATH_PAINTABLE_NO_STATES 0
#define GTK_PATH_PAINTABLE_ALL_STATES G_MAXUINT64

typedef struct
{
  GskPath *path;
  GskPathMeasure *measure;

  uint64_t states;
  double origin;

  struct {
    GtkPathTransitionType type;
    int64_t duration; /* in µs */
    int64_t delay;    /* in µs */
    GtkEasingFunction easing;
  } transition;

  struct {
    GtkPathAnimationType type;
    GtkPathAnimationDirection direction;
    double segment;
    int64_t duration; /* in µs */
    double repeat; /* INFINITY for 'indefinite' */
    GtkCalcMode mode;
    GArray *keyframes;
  } animation;

  struct {
    gboolean enabled;
    float width;
    float min_width;
    float max_width;
    unsigned int symbolic;
    GdkRGBA color;
    GskLineCap linecap;
    GskLineJoin linejoin;
  } stroke;

  struct {
    gboolean enabled;
    GskFillRule rule;
    unsigned int symbolic;
    GdkRGBA color;
  } fill;

  struct {
    size_t to;
    float position;
  } attach;

  /* These are used by attached paths */
  double current_start;
  double current_end;

} PathElt;

typedef struct _GtkPathPaintable GtkPathPaintable;
struct _GtkPathPaintable
{
  GObject parent_instance;
  GArray *paths;

  double width, height;
  graphene_rect_t view_box;
  graphene_rect_t bounds;

  struct {
    gboolean running;
    int64_t start_time;
    int64_t out_duration;
    int64_t in_duration;
    unsigned int old_state;
    unsigned int new_state;
  } transition;

  struct {
    int64_t start_time;
    int64_t end_time;
  } animation;

  float weight;
  unsigned int state;
  unsigned int max_state;

  unsigned int pending_notify;
  unsigned int pending_invalidate;
};

struct _GtkPathPaintableClass
{
  GObjectClass parent_class;
};

enum
{
  PROP_STATE = 1,
  PROP_WEIGHT,
  PROP_RESOURCE,
  NUM_PROPERTIES,
};

static GParamSpec *properties[NUM_PROPERTIES];

/* {{{ Helpers */

typedef struct
{
  GtkSnapshot *snapshot;
  double width;
  double height;
  const GdkRGBA *colors;
  size_t n_colors;
  float weight;
  float t_anim;
  int64_t time;
} PaintData;

static void
clear_path_elt (gpointer data)
{
  PathElt *elt = data;

  gsk_path_unref (elt->path);
  g_clear_pointer (&elt->measure, gsk_path_measure_unref);
}

static struct {
  GtkEasingFunction easing;
  double params[4];
} easing_funcs[] = {
  { GTK_EASING_FUNCTION_LINEAR, { 0, 0, 1, 1 } },
  { GTK_EASING_FUNCTION_EASE_IN_OUT, { 0.42, 0, 0.58, 1 } },
  { GTK_EASING_FUNCTION_EASE_IN, { 0.42, 0, 1, 1 } },
  { GTK_EASING_FUNCTION_EASE_OUT, { 0, 0, 0.58, 1 } },
  { GTK_EASING_FUNCTION_EASE, { 0.25, 0.1, 0.25, 1 } },
};

static float
apply_easing (GtkEasingFunction easing,
              float             progress)
{
  for (int i = 0; i < G_N_ELEMENTS (easing_funcs); i++)
    {
      if (easing_funcs[i].easing == easing)
        return apply_easing_params (easing_funcs[i].params, progress);
    }

  return progress;
}

/* There's two important corner cases here:
 * - no path is in the 'unset' state
 * - if elt->states is 0xffffffff, the path is inert
 *   and is part of all states (except for the 'empty' one)
 */
static gboolean
state_match (uint64_t     states,
             unsigned int state)
{
  if (state == GTK_PATH_PAINTABLE_STATE_EMPTY)
    return FALSE;

  if ((states & (G_GUINT64_CONSTANT (1) << state)) != 0)
    return TRUE;

  return FALSE;
}

static gboolean
path_is_in_state (PathElt      *elt,
                  unsigned int  state)
{
  return state_match (elt->states, state);
}

static int64_t
path_animation_duration (PathElt *elt)
{
  if (elt->animation.type == GTK_PATH_ANIMATION_TYPE_NONE)
    return 0;
  else if (elt->animation.repeat == INFINITY)
    return G_MAXINT64;
  else
    return (int64_t) round (elt->animation.duration * elt->animation.repeat);
}

static GskStroke *
get_stroke_for_path (PathElt *elt,
                     float    weight)
{
  GskStroke *stroke;
  float width;

  if (weight < 1.f)
    {
      g_assert_not_reached ();
    }
  else if (weight < 400.f)
    {
      float f = (400.f - weight) / (400.f - 1.f);
      width = elt->stroke.min_width * f + elt->stroke.width * (1.f - f);
    }
  else if (weight == 400.f)
    {
      width = elt->stroke.width;
    }
  else if (weight <= 1000.f)
    {
      float f = (weight - 400.f) / (1000.f - 400.f);
      width = elt->stroke.max_width * f + elt->stroke.width * (1.f - f);
    }
  else
    {
      g_assert_not_reached ();
    }

  stroke = gsk_stroke_new (width);
  gsk_stroke_set_line_cap (stroke, elt->stroke.linecap);
  gsk_stroke_set_line_join (stroke, elt->stroke.linejoin);

  return stroke;
}

static void
get_fill_color_for_path (GtkPathPaintable *self,
                         PathElt          *elt,
                         GdkRGBA          *c,
                         PaintData        *data)
{
  if (elt->fill.symbolic < data->n_colors)
    {
      *c = data->colors[elt->fill.symbolic];
      c->alpha *= elt->fill.color.alpha;
    }
  else
    {
      *c = elt->fill.color;
    }
}

static gboolean
compute_transition_duration (GtkPathPaintable *self,
                             unsigned int      from,
                             unsigned int      to,
                             int64_t          *out_duration,
                             int64_t          *in_duration)
{
  int64_t out = 0;
  int64_t in = 0;
  gboolean res = FALSE;

  for (unsigned int i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (path_is_in_state (elt, from) &&
          !path_is_in_state (elt, to))
        {
          res = TRUE;
          if (elt->transition.type != GTK_PATH_TRANSITION_TYPE_NONE)
            out = MAX (out, elt->transition.duration + elt->transition.delay);
        }
      else if (!path_is_in_state (elt, from) &&
               path_is_in_state (elt, to))
        {
          res = TRUE;
          if (elt->transition.type != GTK_PATH_TRANSITION_TYPE_NONE)
            in = MAX (in, elt->transition.duration + elt->transition.delay);
        }
    }

  *out_duration = out;
  *in_duration = in;

  return res;
}

static void
compute_bounds (GtkPathPaintable *self)
{
  graphene_rect_t bounds;

  for (unsigned int i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (!gsk_path_is_empty (elt->path))
        {
          graphene_rect_t bd;
          GskStroke *stroke;

          stroke  = get_stroke_for_path (elt, 1000.f);

          if (gsk_path_get_stroke_bounds (elt->path, stroke, &bd))
            {
              if (i > 0)
                graphene_rect_union (&bd, &bounds, &bounds);
              else
                graphene_rect_init_from_rect (&bounds, &bd);
            }

           gsk_stroke_free (stroke);
        }
    }

  if (!graphene_rect_equal (&self->bounds, &bounds))
    {
      graphene_rect_init_from_rect (&self->bounds, &bounds);
      gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
    }
}

static unsigned int
compute_max_state (GtkPathPaintable *self)
{
  unsigned int max = 0;

  for (size_t idx = 0; idx < self->paths->len; idx++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, idx);

      if (elt->states == GTK_PATH_PAINTABLE_ALL_STATES)
        continue;

      max = MAX (max, g_bit_nth_msf (elt->states, -1));
    }

  return max;
}

static int64_t
add_without_wrap (int64_t i1, int64_t i2)
{
  g_assert (i2 >= 0);

  if (i1 > G_MAXINT64 - i2)
    return G_MAXINT64;

  return i1 + i2;
}

static void
compute_animation_end_time (GtkPathPaintable *self,
                            unsigned int      state,
                            int64_t          *end)
{
  *end = self->animation.start_time;

  for (size_t i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (!path_is_in_state (elt, state))
        continue;

      int64_t end2 = add_without_wrap (self->animation.start_time,
                                       path_animation_duration (elt));
      *end = MAX (*end, end2);

      if (*end == G_MAXINT64)
        return;
    }
}

static GskPath *
get_path_segment (PathElt *elt,
                  float    start,
                  float    end)
{
  GskPathBuilder *builder;

  builder = gsk_path_builder_new ();
  if (start <= 0 && end >= 1)
    {
      gsk_path_builder_add_path (builder, elt->path);
      return gsk_path_builder_free_to_path (builder);
    }

  if (end != start)
    {
      float length;
      GskPathPoint start_point, end_point;

      if (!elt->measure)
        elt->measure = gsk_path_measure_new (elt->path);

      length = gsk_path_measure_get_length (elt->measure);

      if (gsk_path_measure_get_point (elt->measure, start * length, &start_point) &&
          gsk_path_measure_get_point (elt->measure, end * length, &end_point))
        gsk_path_builder_add_segment (builder, elt->path, &start_point, &end_point);
    }

  return gsk_path_builder_free_to_path (builder);
}

static void
get_stroke_color_for_path (GtkPathPaintable *self,
                           PathElt          *elt,
                           GdkRGBA          *c,
                           PaintData        *data)
{
  if (elt->stroke.symbolic < data->n_colors)
    {
      *c = data->colors[elt->stroke.symbolic];
      c->alpha *= elt->stroke.color.alpha;
    }
  else
    {
      *c = elt->stroke.color;
    }
}

static void
notify_state (GtkPathPaintable *self)
{
  self->pending_notify = 0;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
}

static void
set_state (GtkPathPaintable *self,
           unsigned int      state,
           gboolean          defer_notify)
{
  if (self->state == state)
    return;

  self->state = state;

  self->animation.start_time = g_get_monotonic_time ();
  compute_animation_end_time (self, state, &self->animation.end_time);

  if (defer_notify)
    {
      if (!self->pending_notify)
        self->pending_notify = g_idle_add_once ((GSourceOnceFunc) notify_state, self);
    }
  else
    notify_state (self);
}

static inline gboolean
g_strv_has (GStrv       strv,
            const char *s)
{
  return g_strv_contains ((const char * const *) strv, s);
}

static GskPath *
circle_path_new (double cx,
                 double cy,
                 double radius)
{
  GskPathBuilder *builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (cx, cy), radius);
  return gsk_path_builder_free_to_path (builder);
}

static GskPath *
rect_path_new (double x,
               double y,
               double width,
               double height,
               double rx,
               double ry)
{
  GskPathBuilder *builder = gsk_path_builder_new ();
  if (rx == 0 && ry == 0)
    gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (x, y, width, height));
  else
    gsk_path_builder_add_rounded_rect (builder,
                                       &(GskRoundedRect) { .bounds = GRAPHENE_RECT_INIT (x, y, width, height),
                                                           .corner = {
                                                             GRAPHENE_SIZE_INIT (rx, ry),
                                                             GRAPHENE_SIZE_INIT (rx, ry),
                                                             GRAPHENE_SIZE_INIT (rx, ry),
                                                             GRAPHENE_SIZE_INIT (rx, ry)
                                                           }
                                                         });
  return gsk_path_builder_free_to_path (builder);
}

/* }}} */
/* {{{ Parser */

static void
markup_filter_attributes (const char *element_name,
                          const char **attribute_names,
                          const char **attribute_values,
                          uint64_t    *handled,
                          const char  *name,
                          ...)
{
  va_list ap;

  va_start (ap, name);
  while (name)
    {
      const char **ptr;

      ptr = va_arg (ap, const char **);

      *ptr = NULL;
      for (unsigned int i = 0; attribute_names[i]; i++)
        {
          if (strcmp (attribute_names[i], name) == 0)
            {
              *ptr = attribute_values[i];
              *handled |= G_GUINT64_CONSTANT(1) << i;
              break;
            }
        }

      name = va_arg (ap, const char *);
    }

  va_end (ap);
}

static void
set_attribute_error (GError     **error,
                     const char  *name,
                     const char  *value)
{
  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
               "Could not handle %s attribute: %s", name, value);
}

static void
set_missing_attribute_error (GError     **error,
                             const char  *name)
{
  g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
               "Missing attribute: %s", name);
}

enum
{
  POSITIVE = 1 << 0,
  LENGTH   = 1 << 1,
  UNIT     = 1 << 2,
};

static gboolean
parse_number (const char    *name,
              const char    *value,
              unsigned int   flags,
              double        *f,
              GError       **error)
{
  char *end;

  *f = g_ascii_strtod (value, &end);
  if ((end && *end != '\0' && ((flags & LENGTH) == 0 || strcmp (end, "px") != 0)) ||
      ((flags & POSITIVE) != 0 && *f < 0) ||
      ((flags & UNIT) != 0 && (*f < 0 || *f > 1)))
    {
      set_attribute_error (error, name, value);
      return FALSE;
    }
  return TRUE;
}

static gboolean
parse_duration (const char    *name,
                const char    *value,
                unsigned int   flags,
                int64_t       *f,
                GError       **error)
{
  double v;
  char *end;

  v = g_ascii_strtod (value, &end);
  if ((flags & POSITIVE) != 0 && value < 0)
    {
      set_attribute_error (error, name, value);
      return FALSE;
    }
  else if (end && *end != '\0')
    {
      if (strcmp (end, "ms") == 0)
        *f = v * G_TIME_SPAN_MILLISECOND;
      else if (strcmp (end, "s") == 0)
        *f = v * G_TIME_SPAN_SECOND;
      else
        {
          set_attribute_error (error, name, value);
          return FALSE;
        }
    }
  else
    *f = v * G_TIME_SPAN_SECOND;

  return TRUE;
}

static gboolean
parse_numbers (const char    *name,
               const char    *value,
               double        *values,
               unsigned int   length,
               unsigned int   flags,
               unsigned int  *n_values,
               GError       **error)
{
  GStrv strv;
  unsigned int len;

  strv = g_strsplit (value, " ", 0);
  len = g_strv_length (strv);

  if (len > length)
    {
      g_strfreev (strv);
      set_attribute_error (error, name, value);
      return FALSE;
    }

  for (unsigned int i = 0; i < len; i++)
    {
      if (!parse_number (name, strv[i], flags, &values[i], error))
        {
          g_strfreev (strv);
          return FALSE;
        }
    }

  g_strfreev (strv);
  *n_values = len;
  return TRUE;
}

static gboolean
parse_enum (const char    *name,
            const char    *value,
            const char   **values,
            size_t          n_values,
            unsigned int  *result,
            GError       **error)
{
  for (unsigned int i = 0; i < n_values; i++)
    {
      if (strcmp (value, values[i]) == 0)
        {
          *result = i;
          return TRUE;
        }
    }

  set_attribute_error (error, name, value);
  return FALSE;
}

static gboolean
parse_states (const char *text,
              uint64_t   *states)
{
  GStrv str = NULL;

  if (strcmp (text, "all") == 0)
    {
      *states = GTK_PATH_PAINTABLE_ALL_STATES;
      return TRUE;
    }

  if (strcmp (text, "none") == 0)
    {
      *states = GTK_PATH_PAINTABLE_NO_STATES;
      return TRUE;
    }

  *states = 0;

  str = g_strsplit (text, " ", 0);
  for (unsigned int i = 0; str[i]; i++)
    {
      unsigned int u;
      char *end;

      u = (unsigned int) g_ascii_strtoull (str[i], &end, 10);
      if ((end && *end != '\0') || (u > 63))
        {
          *states = GTK_PATH_PAINTABLE_ALL_STATES;
          g_strfreev (str);
          return FALSE;
        }

      *states |= (G_GUINT64_CONSTANT(1) << u);
    }

  g_strfreev (str);
  return TRUE;
}

static gboolean
parse_paint (const char    *name,
             const char    *value,
             unsigned int  *symbolic,
             GdkRGBA       *color,
             GError       **error)
{
  const char *sym[] = { "foreground", "error", "warning", "success", "accent" };
  unsigned int i;

  for (i = 0; i < G_N_ELEMENTS (sym); i++)
    {
      if (strcmp (value, sym[i]) == 0)
        {
          *symbolic = (GtkSymbolicColor) i;
          break;
        }
    }
  if (i == G_N_ELEMENTS (sym))
    {
      if (!gdk_rgba_parse (color, value))
        {
          set_attribute_error (error, name, value);
          return FALSE;
        }
    }

  return TRUE;
}

typedef struct {
  char *to;
  float position;
} AttachData;

static void
attach_data_clear (gpointer data)
{
  AttachData *attach = data;
  g_free (attach->to);
}

static GArray *
construct_animation_frames (unsigned int   easing,
                            GError       **error)
{
  GArray *res = g_array_new (FALSE, TRUE, sizeof (GtkKeyFrame));
  GtkKeyFrame frame;

  frame.value = 0;
  frame.time = 0;
  memcpy (frame.params, easing_funcs[easing].params, 4 * sizeof (double));
  g_array_append_val (res, frame);

  frame.value = 1;
  frame.time = 1;
  memcpy (frame.params, easing_funcs[easing].params, 4 * sizeof (double));
  g_array_append_val (res, frame);

  return res;
}

typedef struct
{
  GtkPathPaintable *paintable;
  GHashTable *paths;
  GArray *attach;
  unsigned int state;
  unsigned int version;
} ParserData;

static void
start_element_cb (GMarkupParseContext  *context,
                  const gchar          *element_name,
                  const gchar         **attribute_names,
                  const gchar         **attribute_values,
                  gpointer              user_data,
                  GError              **error)
{
  ParserData *data = user_data;
  uint64_t handled = 0;
  int first_unset;
  const char *ignored = NULL;
  const char *path_attr = NULL;
  const char *stroke_attr = NULL;
  const char *class_attr = NULL;
  const char *stroke_width_attr = NULL;
  const char *gtk_stroke_width_attr = NULL;
  const char *stroke_opacity_attr = NULL;
  const char *stroke_linecap_attr = NULL;
  const char *stroke_linejoin_attr = NULL;
  const char *fill_attr = NULL;
  const char *fill_rule_attr = NULL;
  const char *fill_opacity_attr = NULL;
  const char *states_attr = NULL;
  const char *animation_type_attr = NULL;
  const char *animation_direction_attr = NULL;
  const char *animation_duration_attr = NULL;
  const char *animation_repeat_attr = NULL;
  const char *animation_segment_attr = NULL;
  const char *animation_easing_attr = NULL;
  const char *origin_attr = NULL;
  const char *transition_type_attr = NULL;
  const char *transition_duration_attr = NULL;
  const char *transition_delay_attr = NULL;
  const char *transition_easing_attr = NULL;
  const char *id_attr = NULL;
  const char *attach_to_attr = NULL;
  const char *attach_pos_attr = NULL;
  GskPath *path = NULL;
  unsigned int stroke_symbolic;
  GdkRGBA stroke_color;
  double stroke_opacity;
  double stroke_width;
  double stroke_min_width;
  double stroke_max_width;
  unsigned int stroke_linejoin;
  unsigned int stroke_linecap;
  unsigned int fill_symbolic;
  unsigned int fill_rule;
  GdkRGBA fill_color;
  double fill_opacity;
  uint64_t states;
  unsigned int transition_type;
  int64_t transition_duration;
  int64_t transition_delay;
  GtkEasingFunction transition_easing;
  GtkPathAnimationType animation_type;
  GtkPathAnimationDirection animation_direction;
  int64_t animation_duration;
  double animation_repeat;
  double animation_segment;
  GtkEasingFunction animation_easing;
  GtkCalcMode animation_mode;
  GArray *animation_keyframes = NULL;
  double origin;
  size_t idx;
  AttachData attach;
  PathElt elt;

  if (strcmp (element_name, "svg") == 0)
    {
      const char *width_attr = NULL;
      const char *height_attr = NULL;
      const char *viewbox_attr = NULL;
      const char *state_attr = NULL;
      const char *version_attr = NULL;
      double width, height;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                &handled,
                                "width", &width_attr,
                                "height", &height_attr,
                                "viewBox", &viewbox_attr,
                                "gpa:version", &version_attr,
                                "gpa:state", &state_attr,
                                NULL);

      if (width_attr == NULL)
        {
          set_missing_attribute_error (error, "width");
          return;
        }

      if (!parse_number ("width", width_attr, LENGTH | POSITIVE, &width, error))
        return;

      if (height_attr == NULL)
        {
          set_missing_attribute_error (error, "height");
          return;
        }

      if (!parse_number ("height", height_attr, LENGTH | POSITIVE, &height, error))
        return;

      data->paintable->width = width;
      data->paintable->height = height;

      if (viewbox_attr)
        {
          GStrv strv;
          unsigned int n;
          double x, y, w, h;

          strv = g_strsplit (viewbox_attr, " ", 0);
          n = g_strv_length (strv);
          if (n != 4)
            {
              set_attribute_error (error, "viewBox", viewbox_attr);
              g_strfreev (strv);
              return;
            }
          if (!parse_number ("viewBox", strv[0], LENGTH, &x, error) ||
              !parse_number ("viewBox", strv[1], LENGTH, &y, error) ||
              !parse_number ("viewBox", strv[2], LENGTH | POSITIVE, &w, error) ||
              !parse_number ("viewBox", strv[3], LENGTH | POSITIVE, &h, error))
            {
              g_strfreev (strv);
              return;
            }

          graphene_rect_init (&data->paintable->view_box, x, y, w, h);
        }

      if (version_attr)
        {
          unsigned int version;
          char *end;

          version = (unsigned int) g_ascii_strtoull (version_attr, &end, 10);
          if ((end && *end != '\0') || version != 1)
            {
              set_attribute_error (error, "gpa:version", version_attr);
              return;
            }

          data->version = version;
        }

      if (state_attr)
        {
          int state;
          char *end;

          state = (int) g_ascii_strtoll (state_attr, &end, 10);
          if ((end && *end != '\0') || (state < -1 || state > 63))
            {
              set_attribute_error (error, "gpa:state", state_attr);
              return;
            }

          data->state = (unsigned int) state;
        }

      return;
    }
  else if (strcmp (element_name, "g") == 0 ||
           strcmp (element_name, "defs") == 0 ||
           strcmp (element_name, "style") == 0 ||
           g_str_has_prefix (element_name, "sodipodi:") ||
           g_str_has_prefix (element_name, "inkscape:"))
    {
      /* Do nothing */
      return;
    }
  else if (strcmp (element_name, "circle") == 0)
    {
      const char *cx_attr = NULL;
      const char *cy_attr = NULL;
      const char *r_attr = NULL;
      double cx = 0;
      double cy = 0;
      double r = 0;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                &handled,
                                "cx", &cx_attr,
                                "cy", &cy_attr,
                                "r", &r_attr,
                                NULL);

      if (cx_attr)
        {
          if (!parse_number ("cx", cx_attr, 0, &cx, error))
            return;
        }

      if (cy_attr)
        {
          if (!parse_number ("cy", cy_attr, 0, &cy, error))
            return;
        }

      if (r_attr)
        {
          if (!parse_number ("r", r_attr, POSITIVE, &r, error))
            return;
        }

      if (r == 0)
        return;  /* nothing to do */

      path = circle_path_new (cx, cy, r);
    }
  else if (strcmp (element_name, "rect") == 0)
    {
      const char *x_attr = NULL;
      const char *y_attr = NULL;
      const char *width_attr = NULL;
      const char *height_attr = NULL;
      const char *rx_attr = NULL;
      const char *ry_attr = NULL;
      double x = 0;
      double y = 0;
      double width = 0;
      double height = 0;
      double rx = 0;
      double ry = 0;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                &handled,
                                "x", &x_attr,
                                "y", &y_attr,
                                "width", &width_attr,
                                "height", &height_attr,
                                "rx", &rx_attr,
                                "ry", &ry_attr,
                                NULL);

      if (x_attr)
        {
          if (!parse_number ("x", x_attr, 0, &x, error))
            return;
        }

      if (y_attr)
        {
          if (!parse_number ("y", y_attr, 0, &y, error))
            return;
        }

      if (width_attr)
        {
          if (!parse_number ("width", width_attr, POSITIVE, &width, error))
            return;
        }

      if (height_attr)
        {
          if (!parse_number ("height", height_attr, POSITIVE, &height, error))
            return;
        }

      if (width == 0 || height == 0)
        return;  /* nothing to do */

      if (rx_attr)
        {
          if (!parse_number ("rx", rx_attr, POSITIVE, &rx, error))
            return;
        }

     if (ry_attr)
        {
          if (!parse_number ("ry", ry_attr, POSITIVE, &ry, error))
            return;
        }

      if (!rx_attr && ry_attr)
        rx = ry;
      else if (rx_attr && !ry_attr)
        ry = rx;

      path = rect_path_new (x, y, width, height, rx, ry);
    }
  else if (strcmp (element_name, "path") == 0)
    {
      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                &handled,
                                "d", &path_attr,
                                NULL);

      if (!path_attr)
        {
          set_missing_attribute_error (error, "d");
          return;
        }

      path = gsk_path_parse (path_attr);
      if (!path)
        {
          set_attribute_error (error, "d", path_attr);
          return;
        }
    }
  else
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Unhandled element: %s", element_name);
      return;
    }

  g_assert (path != NULL);

  markup_filter_attributes (element_name,
                            attribute_names,
                            attribute_values,
                            &handled,
                            "stroke-width", &stroke_width_attr,
                            "stroke-opacity", &stroke_opacity_attr,
                            "stroke-linecap", &stroke_linecap_attr,
                            "stroke-linejoin", &stroke_linejoin_attr,
                            "fill-opacity", &fill_opacity_attr,
                            "fill-rule", &fill_rule_attr,
                            "id", &id_attr,
                            "gpa:fill", &fill_attr,
                            "gpa:stroke", &stroke_attr,
                            "gpa:stroke-width", &gtk_stroke_width_attr,
                            "gpa:states", &states_attr,
                            "gpa:origin", &origin_attr,
                            "gpa:animation-type", &animation_type_attr,
                            "gpa:animation-direction", &animation_direction_attr,
                            "gpa:animation-duration", &animation_duration_attr,
                            "gpa:animation-repeat", &animation_repeat_attr,
                            "gpa:animation-segment", &animation_segment_attr,
                            "gpa:animation-easing", &animation_easing_attr,
                            "gpa:transition-type", &transition_type_attr,
                            "gpa:transition-duration", &transition_duration_attr,
                            "gpa:transition-delay", &transition_delay_attr,
                            "gpa:transition-easing", &transition_easing_attr,
                            "gpa:attach-to", &attach_to_attr,
                            "gpa:attach-pos", &attach_pos_attr,
                            "class", &class_attr,
                            "stroke", &ignored,
                            "fill", &ignored,
                            NULL);

  first_unset = g_bit_nth_lsf (~handled, -1);
  if (first_unset != -1 && first_unset < g_strv_length ((char **) attribute_names))
    {
      g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                   "Unhandled attribute: %s", attribute_names[first_unset]);
      goto cleanup;
    }

  if (!fill_attr &&
      !stroke_attr &&
      !gtk_stroke_width_attr &&
      !states_attr &&
      !origin_attr &&
      !animation_type_attr &&
      !animation_direction_attr &&
      !animation_duration_attr &&
      !animation_repeat_attr &&
      !animation_segment_attr &&
      !animation_easing_attr &&
      !transition_type_attr &&
      !transition_duration_attr &&
      !transition_delay_attr &&
      !transition_easing_attr &&
      !attach_to_attr &&
      !attach_pos_attr)
    {
      /* backwards compat with traditional symbolic svg */
      if (class_attr)
        {
          GStrv classes = g_strsplit (class_attr, " ", 0);

          if (g_strv_has (classes, "transparent-fill"))
            fill_attr = NULL;
          else if (g_strv_has (classes, "foreground-fill"))
            fill_attr = "foreground";
          else if (g_strv_has (classes, "success") ||
                   g_strv_has (classes, "success-fill"))
            fill_attr = "success";
          else if (g_strv_has (classes, "warning") ||
                   g_strv_has (classes, "warning-fill"))
            fill_attr = "warning";
          else if (g_strv_has (classes, "error") ||
                   g_strv_has (classes, "error-fill"))
            fill_attr = "error";
          else
            fill_attr = "foreground";

          if (g_strv_has (classes, "success-stroke"))
            stroke_attr = "success";
          else if (g_strv_has (classes, "warning-stroke"))
            stroke_attr = "warning";
          else if (g_strv_has (classes, "error-stroke"))
            stroke_attr = "error";
          else if (g_strv_has (classes, "foreground-stroke"))
            stroke_attr = "foreground";

          if (stroke_attr)
            {
              if (!stroke_width_attr)
                stroke_width_attr = "2";

              if (!stroke_linecap_attr)
                stroke_linecap_attr = "round";

              if (!stroke_linejoin_attr)
                stroke_linejoin_attr = "round";
            }

          g_strfreev (classes);
        }
      else
        {
          fill_attr = "foreground";
        }
    }

  stroke_opacity = 1;
  if (stroke_opacity_attr)
    {
      if (!parse_number ("stroke-opacity", stroke_opacity_attr, UNIT, &stroke_opacity, error))
        goto cleanup;
    }

  stroke_symbolic = 0xffff;
  stroke_color = (GdkRGBA) { 0, 0, 0, 1 };
  if (stroke_attr)
    {
      if (!parse_paint ("gpa:stroke", stroke_attr, &stroke_symbolic, &stroke_color, error))
        goto cleanup;
    }

  stroke_color.alpha *= stroke_opacity;

  stroke_width = 2;
  if (stroke_width_attr)
    {
      if (!parse_number ("stroke-width", stroke_width_attr, POSITIVE, &stroke_width, error))
        goto cleanup;
    }

  stroke_min_width = stroke_width * 0.25;
  stroke_max_width = stroke_width * 1.5;

  if (gtk_stroke_width_attr)
    {
      double v[3];
      unsigned int len;

      if (!parse_numbers ("gpa:stroke-width", gtk_stroke_width_attr, v, 3, POSITIVE, &len, error))
        goto cleanup;

      if (len != 3 || v[1] < v[0] || v[2] < v[1])
        {
          set_attribute_error (error, "gpa:stroke-width", gtk_stroke_width_attr);
          goto cleanup;
        }

      stroke_min_width = v[0];
      stroke_width = v[1];
      stroke_max_width = v[2];
    }

  stroke_linecap = GSK_LINE_CAP_ROUND;
  if (stroke_linecap_attr)
    {
      if (!parse_enum ("stroke-linecap", stroke_linecap_attr,
                       (const char *[]) { "butt", "round", "square" }, 3,
                        &stroke_linecap, error))
        goto cleanup;
    }

  stroke_linejoin = GSK_LINE_JOIN_ROUND;
  if (stroke_linejoin_attr)
    {
      if (!parse_enum ("stroke-linejoin", stroke_linejoin_attr,
                       (const char *[]) { "miter", "round", "bevel" }, 3,
                        &stroke_linejoin, error))
        goto cleanup;
    }

  fill_rule = GSK_FILL_RULE_WINDING;
  if (fill_rule_attr)
    {
      if (strcmp (fill_rule_attr, "winding") == 0)
        fill_rule = GSK_FILL_RULE_WINDING;
      else if (!parse_enum ("fill-rule", fill_rule_attr,
                            (const char *[]) { "nonzero", "evenodd" }, 2,
                             &fill_rule, error))
        goto cleanup;
    }

  fill_opacity = 1;
  if (fill_opacity_attr)
    {
      if (!parse_number ("fill-opacity", fill_opacity_attr, UNIT, &fill_opacity, error))
        goto cleanup;
    }

  fill_symbolic = 0xffff;
  fill_color = (GdkRGBA) { 0, 0, 0, 1 };
  if (fill_attr)
    {
      if (!parse_paint ("gpa:fill", fill_attr, &fill_symbolic, &fill_color, error))
        goto cleanup;
    }

  fill_color.alpha *= fill_opacity;

  transition_type = GTK_PATH_TRANSITION_TYPE_NONE;
  if (transition_type_attr)
    {
      if (!parse_enum ("gpa:transition-type", transition_type_attr,
                       (const char *[]) { "none", "animate", "morph", "fade" }, 4,
                        &transition_type, error))
        goto cleanup;
    }

  transition_duration = 0.f;
  if (transition_duration_attr)
    {
      if (!parse_duration ("gpa:transition-duration", transition_duration_attr, POSITIVE, &transition_duration, error))
        goto cleanup;
    }

  transition_delay = 0.f;
  if (transition_delay_attr)
    {
      if (!parse_duration ("gpa:transition-delay", transition_delay_attr, 0, &transition_delay, error))
        goto cleanup;
    }

  transition_easing = GTK_EASING_FUNCTION_LINEAR;
  if (transition_easing_attr)
    {
      unsigned int easing;

      if (!parse_enum ("gpa:transition-easing", transition_easing_attr,
                       (const char *[]) { "linear", "ease-in-out", "ease-in",
                                          "ease-out", "ease" }, 5,
                        &easing, error))
        goto cleanup;

      transition_easing = (GtkEasingFunction) easing;
    }

  origin = 0;
  if (origin_attr)
    {
      if (!parse_number ("gpa:origin", origin_attr, UNIT, &origin, error))
        goto cleanup;
    }

  states = GTK_PATH_PAINTABLE_ALL_STATES;
  if (states_attr)
    {
      if (!parse_states (states_attr, &states))
        {
          set_attribute_error (error, "gpa:states", states_attr);
          goto cleanup;
        }
    }

  animation_type = GTK_PATH_ANIMATION_TYPE_NONE;
  if (animation_type_attr)
    {
      unsigned int type;

      if (!parse_enum ("gpa:animation-type", animation_type_attr,
                       (const char *[]) { "none", "automatic", }, 2,
                        &type, error))
        goto cleanup;

      animation_type = (GtkPathAnimationType) type;
    }

  animation_direction = GTK_PATH_ANIMATION_DIRECTION_NORMAL;
  if (animation_direction_attr)
    {
      unsigned int direction;

      if (!parse_enum ("gpa:animation-direction", animation_direction_attr,
                       (const char *[]) { "normal", "alternate", "reverse",
                                          "reverse-alternate", "in-out",
                                          "in-out-alternate", "in-out-reverse",
                                          "segment", "segment-alternate" }, 9,
                        &direction, error))
        goto cleanup;

      animation_direction = (GtkPathAnimationDirection) direction;
    }

  animation_duration = 0.f;
  if (animation_duration_attr)
    {
      if (!parse_duration ("gpa:animation-duration", animation_duration_attr, POSITIVE, &animation_duration, error))
        goto cleanup;
    }

  animation_repeat = INFINITY;
  if (animation_repeat_attr)
    {
      if (strcmp (animation_repeat_attr, "indefinite") == 0)
        animation_repeat = INFINITY;
      else if (!parse_number ("gpa:animation-repeat", animation_repeat_attr, POSITIVE, &animation_repeat, error))
        goto cleanup;
    }

  animation_segment = 0.2;
  if (animation_segment_attr)
    {
      if (!parse_number ("gpa:animation-segment", animation_segment_attr, POSITIVE, &animation_segment, error))
        goto cleanup;
    }

  animation_easing = GTK_EASING_FUNCTION_LINEAR;
  if (animation_easing_attr)
    {
      unsigned int easing;

      if (!parse_enum ("gpa:animation-easing", animation_easing_attr,
                       (const char *[]) { "linear", "ease-in-out", "ease-in",
                                          "ease-out", "ease" }, 5,
                        &easing, error))
        goto cleanup;

       animation_easing = (GtkEasingFunction) easing;
    }

  animation_keyframes = construct_animation_frames (animation_easing, error);
  if (!animation_keyframes)
    goto cleanup;

  if (animation_easing == GTK_EASING_FUNCTION_LINEAR)
    animation_mode = GTK_CALC_MODE_LINEAR;
  else
    animation_mode = GTK_CALC_MODE_SPLINE;

  attach.to = g_strdup (attach_to_attr);
  attach.position = 0;
  if (attach_pos_attr)
    {
      double v;

      if (!parse_number ("gpa:attach-pos", attach_pos_attr, UNIT, &v, error))
        goto cleanup;

      attach.position = v;
    }

  elt.path = gsk_path_ref (path);
  elt.measure = NULL;

  elt.states = states;

  elt.transition.type = (GtkPathTransitionType) transition_type;
  elt.transition.duration = transition_duration;
  elt.transition.delay = transition_delay;
  elt.transition.easing = transition_easing;
  elt.origin = origin;

  elt.animation.type = animation_type;
  elt.animation.direction = animation_direction;
  elt.animation.duration = animation_duration;
  elt.animation.repeat = animation_repeat;
  elt.animation.segment = animation_segment;
  elt.animation.mode = animation_mode;
  elt.animation.keyframes = g_array_ref (animation_keyframes);

  elt.fill.enabled = fill_attr != NULL;
  elt.fill.rule = (GskFillRule) fill_rule;
  elt.fill.symbolic = fill_symbolic;
  elt.fill.color = fill_color;

  elt.stroke.enabled = stroke_attr != NULL;
  elt.stroke.width = stroke_width;
  elt.stroke.min_width = stroke_min_width;
  elt.stroke.max_width = stroke_max_width;
  elt.stroke.linecap = (GskLineCap) stroke_linecap;
  elt.stroke.linejoin = (GskLineJoin) stroke_linejoin;
  elt.stroke.symbolic = stroke_symbolic;
  elt.stroke.color = stroke_color;

  elt.attach.to = (size_t) -1;
  elt.attach.position = 0;

  g_array_append_val (data->paintable->paths, elt);

  idx = data->paintable->paths->len - 1;

  g_array_append_val (data->attach, attach);

  if (id_attr)
    g_hash_table_insert (data->paths, g_strdup (id_attr), GUINT_TO_POINTER (idx));

cleanup:
  g_clear_pointer (&path, gsk_path_unref);
  g_clear_pointer (&animation_keyframes, g_array_unref);
}

static gboolean
gtk_path_paintable_init_from_bytes (GtkPathPaintable  *self,
                                    GBytes            *bytes,
                                    GError           **error)
{
  ParserData data;
  GMarkupParseContext *context;
  GMarkupParser parser = {
    start_element_cb,
    NULL,
    NULL,
    NULL,
    NULL,
  };
  gboolean ret;

  data.paintable = self;
  data.paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  data.attach = g_array_new (FALSE, TRUE, sizeof (AttachData));
  g_array_set_clear_func (data.attach, attach_data_clear);
  data.state = 0;
  data.version = 0;

  context = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &data, NULL);
  ret = g_markup_parse_context_parse (context,
                                      g_bytes_get_data (bytes, NULL),
                                      g_bytes_get_size (bytes),
                                      error);

  g_markup_parse_context_free (context);

  if (ret)
    {
      for (unsigned int i = 0; i < data.attach->len; i++)
        {
          PathElt *elt = &g_array_index (self->paths, PathElt, i);
          AttachData *attach = &g_array_index (data.attach, AttachData, i);
          gpointer value = NULL;

          if (!attach->to)
            continue;

          if (!g_hash_table_lookup_extended (data.paths, attach->to, NULL, &value))
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid %s attribute value: %s", "gpa:attach-to", attach->to);
              ret = FALSE;
              break;
            }

          elt->attach.to = (size_t) GPOINTER_TO_UINT (value);
          elt->attach.position = attach->position;
        }
    }

  g_hash_table_unref (data.paths);
  g_array_unref (data.attach);

  compute_bounds (self);
  set_state (self, data.state, FALSE);

  return ret;
}

/* }}} */
/* {{{ Painting */

static void
fill_path (GtkPathPaintable *self,
           PathElt          *elt,
           GskPath          *path,
           PaintData        *data)
{
  GdkRGBA c;

  if (!elt->fill.enabled)
    return;

  get_fill_color_for_path (self, elt, &c, data);

  gtk_snapshot_push_fill (data->snapshot, path, elt->fill.rule);
  gtk_snapshot_append_color (data->snapshot, &c, &self->bounds);
  gtk_snapshot_pop (data->snapshot);
}

static void
stroke_path (GtkPathPaintable *self,
             PathElt          *elt,
             GskPath          *path,
             PaintData        *data)
{
  GskStroke *stroke;
  GdkRGBA c;

  if (!elt->stroke.enabled)
    return;

  stroke = get_stroke_for_path (elt, data->weight);
  get_stroke_color_for_path (self, elt, &c, data);

  gtk_snapshot_push_stroke (data->snapshot, path, stroke);
  gtk_snapshot_append_color (data->snapshot, &c, &self->bounds);
  gtk_snapshot_pop (data->snapshot);

  gsk_stroke_free (stroke);
}

static void
paint_elt (GtkPathPaintable *self,
           PathElt          *elt,
           PaintData        *data)
{
  PathElt *base;
  double pos;
  double length;
  GskPathPoint point;
  graphene_point_t orig_pos, adjusted_pos;
  double orig_angle, adjusted_angle;
  GskTransform *transform = NULL;

  if (elt->attach.to == (size_t) -1)
    {
      fill_path (self, elt, elt->path, data);
      stroke_path (self, elt, elt->path, data);
      return;
    }

  base = &g_array_index (self->paths, PathElt, elt->attach.to);

  pos = lerp (elt->attach.position, base->current_start, base->current_end);

  if (elt->origin != 0)
    {
      if (!elt->measure)
        elt->measure = gsk_path_measure_new (elt->path);

      length = gsk_path_measure_get_length (elt->measure);
      gsk_path_measure_get_point (elt->measure, length * elt->origin, &point);
    }
  else
    {
      gsk_path_get_start_point (elt->path, &point);
    }

  gsk_path_point_get_position (&point, elt->path, &orig_pos);
  orig_angle = 0; /* FIXME */

  if (!base->measure)
    base->measure = gsk_path_measure_new (base->path);

  length = gsk_path_measure_get_length (base->measure);

  gsk_path_measure_get_point (base->measure, length * pos, &point);
  gsk_path_point_get_position (&point, base->path, &adjusted_pos);
  adjusted_angle = gsk_path_point_get_rotation (&point, base->path, GSK_PATH_TO_END);

  /* Now determine the transform that moves orig_pos to adjusted_pos
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

  gsk_transform_unref (transform);

  fill_path (self, elt, elt->path, data);
  stroke_path (self, elt, elt->path, data);

  gtk_snapshot_restore (data->snapshot);
}

static void
paint_elt_partial (GtkPathPaintable *self,
                   PathElt          *elt,
                   double            start,
                   double            end,
                   PaintData        *data)
{
  elt->current_start = start;
  elt->current_end = end;

  if (start > 0 || end < 1)
    {
      GskPath *path = get_path_segment (elt, start, end);

      /* TODO: fill */
      stroke_path (self, elt, path, data);
      gsk_path_unref (path);
    }
  else
    {
      paint_elt (self, elt, data);
    }
}

static void
paint_elt_animated (GtkPathPaintable *self,
                    PathElt          *elt,
                    double            start,
                    double            end,
                    PaintData        *data)
{
  double t;
  unsigned int rep;
  double origin;
  double segment;

  segment = elt->animation.segment;

  switch (elt->animation.type)
    {
    case GTK_PATH_ANIMATION_TYPE_NONE:
      paint_elt_partial (self, elt, start, end, data);
      return;
    case GTK_PATH_ANIMATION_TYPE_AUTOMATIC:
      if (elt->animation.repeat == INFINITY ||
          data->time < self->animation.start_time + elt->animation.duration * elt->animation.repeat)
        t = (data->time - self->animation.start_time) / (double) elt->animation.duration;
      else
        t = elt->animation.repeat;
      break;
    default:
      g_assert_not_reached ();
    }

  rep = (unsigned int) floor (t);
  t = compute_value (elt->animation.mode, elt->animation.keyframes, t - floor (t));

  origin = lerp (elt->origin, start, end);

  switch (elt->animation.direction)
    {
    case GTK_PATH_ANIMATION_DIRECTION_NORMAL:
      paint_elt_partial (self, elt, lerp (1 - t, start, origin), lerp (t, origin, end), data);
      break;
    case GTK_PATH_ANIMATION_DIRECTION_ALTERNATE:
      if (rep % 2 == 0)
        paint_elt_partial (self, elt, lerp (1 - t, start, origin), lerp (t, origin, end), data);
      else
        paint_elt_partial (self, elt, lerp (t, start, origin), lerp (1 - t, origin, end), data);
      break;
    case GTK_PATH_ANIMATION_DIRECTION_REVERSE:
      paint_elt_partial (self, elt, lerp (t, start, origin), lerp (1 - t, origin, end), data);
      break;
    case GTK_PATH_ANIMATION_DIRECTION_REVERSE_ALTERNATE:
      if (rep % 2 == 0)
        paint_elt_partial (self, elt, lerp (t, start, origin), lerp (1 - t, origin, end), data);
      else
        paint_elt_partial (self, elt, lerp (1 - t, start, origin), lerp (t, origin, end), data);
      break;
    case GTK_PATH_ANIMATION_DIRECTION_IN_OUT:
      if (rep % 2 == 0)
        paint_elt_partial (self, elt, lerp (1 - t, start, origin), lerp (t, origin, end), data);
      else
        {
          paint_elt_partial (self, elt, start,  lerp (1 - t, start, origin), data);
          paint_elt_partial (self, elt, lerp (t, origin, end), end, data);
        }
      break;
    case GTK_PATH_ANIMATION_DIRECTION_IN_OUT_REVERSE:
      if (rep % 2 == 0)
        {
          paint_elt_partial (self, elt, start,  lerp (t, start, origin), data);
          paint_elt_partial (self, elt, lerp (1 - t, origin, end), end, data);
        }
      else
        paint_elt_partial (self, elt, lerp (t, start, origin), lerp (1 - t, origin, end), data);
      break;
    case GTK_PATH_ANIMATION_DIRECTION_IN_OUT_ALTERNATE:
      switch (rep % 4)
        {
        case 0:
          paint_elt_partial (self, elt, lerp (1 - t, start, origin), lerp (t, origin, end), data);
          break;
        case 1:
          paint_elt_partial (self, elt, start,  lerp (1 - t, start, origin), data);
          paint_elt_partial (self, elt, lerp (t, origin, end), end, data);
          break;
        case 2:
          paint_elt_partial (self, elt, start,  lerp (t, start, origin), data);
          paint_elt_partial (self, elt, lerp (1 - t, origin, end), end, data);
          break;
        case 3:
          paint_elt_partial (self, elt, lerp (t, start, origin), lerp (1 - t, origin, end), data);
          break;
        default:
          g_assert_not_reached ();
        }
      break;
    case GTK_PATH_ANIMATION_DIRECTION_SEGMENT:
      if (segment >= 1)
        paint_elt_partial (self, elt, start, end, data);
      else
        paint_elt_partial (self, elt, lerp (t, start, end), lerp (fmodf (t + segment, 1), start, end), data);
      break;
    case GTK_PATH_ANIMATION_DIRECTION_SEGMENT_ALTERNATE:
      if (segment >= 1)
        paint_elt_partial (self, elt, start, end, data);
      else if (rep % 2 == 0)
        paint_elt_partial (self, elt, lerp (t * (1 - segment), start, end), lerp (t * (1 - segment) + segment, start, end), data);
      else
        paint_elt_partial (self, elt, lerp (1 - segment - t * (1 - segment), start, end), lerp (1 - t * (1 - segment), start, end), data);
      break;
    default:
      g_assert_not_reached ();
    }
}

/* This is doing an animated blur together with alpha
 * thresholding to achieve a 'blobbing' effect.
 */
static void
paint_elt_with_blobbing (GtkPathPaintable *self,
                         PathElt          *elt,
                         double            t,
                         PaintData        *data)
{
  GskComponentTransfer *identity;
  GskComponentTransfer *alpha;
  double blur;

  blur = t * CLAMP (MAX (data->width, data->height) / 2, 0, 64);

  identity = gsk_component_transfer_new_identity ();
  alpha = gsk_component_transfer_new_discrete (5, (float []) { 0, 1, 1, 1, 1 });

  gtk_snapshot_push_component_transfer (data->snapshot, identity, identity, identity, alpha);
  gtk_snapshot_push_blur (data->snapshot, blur);

  paint_elt_animated (self, elt, 0, 1, data);

  gtk_snapshot_pop (data->snapshot);
  gtk_snapshot_pop (data->snapshot);

  gsk_component_transfer_free (identity);
  gsk_component_transfer_free (alpha);
}

static void
paint_elt_with_fade (GtkPathPaintable *self,
                     PathElt          *elt,
                     double            t,
                     PaintData        *data)
{
  gtk_snapshot_push_opacity (data->snapshot, 1 - t);
  paint_elt_animated (self, elt, 0, 1, data);
  gtk_snapshot_pop (data->snapshot);
}

static void
invalidate_in_idle (GtkPathPaintable *self)
{
  self->pending_invalidate = 0;
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static void
paint (GtkPathPaintable *self,
       PaintData        *data)
{
  int64_t out_end;

  out_end = self->transition.start_time + self->transition.out_duration;

  if (self->transition.running &&
      self->state != self->transition.new_state &&
      data->time >= out_end)
    {
      set_state (self, self->transition.new_state, TRUE);
    }

  for (unsigned int i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);
      gboolean in_old_state, in_new_state;

      if (!self->transition.running)
        {
          if (path_is_in_state (elt, self->state))
            paint_elt_animated (self, elt, 0, 1, data);

          continue;
        }

      in_old_state = path_is_in_state (elt, self->transition.old_state);
      in_new_state = path_is_in_state (elt, self->transition.new_state);

      if (in_old_state && in_new_state)
        {
          /* not transitioning */
          paint_elt_animated (self, elt, 0, 1, data);
        }
      else if (in_old_state && !in_new_state)
        {
          int64_t start_time, end_time;

          start_time = out_end - (elt->transition.duration + elt->transition.delay);
          end_time = start_time + elt->transition.duration;

          if (data->time < start_time)
            {
              paint_elt_animated (self, elt, 0, 1, data);
            }
          else if (data->time >= start_time &&
                   data->time <= end_time)
            {
              /* disappearing */
              double t = (data->time - start_time) / (double) (end_time - start_time);

              t = apply_easing (elt->transition.easing, t);

              switch (elt->transition.type)
                {
                case GTK_PATH_TRANSITION_TYPE_NONE:
                  if (t < 1)
                    paint_elt_animated (self, elt, 0, 1, data);
                  break;
                case GTK_PATH_TRANSITION_TYPE_MORPH:
                  if (t < 1)
                    paint_elt_with_blobbing (self, elt, t, data);
                  break;
                case GTK_PATH_TRANSITION_TYPE_FADE:
                  if (t < 1)
                    paint_elt_with_fade (self, elt, t, data);
                  break;
                case GTK_PATH_TRANSITION_TYPE_ANIMATE:
                  paint_elt_animated (self, elt, elt->origin * t, 1 - (1 - elt->origin) * t, data);
                  break;
                default:
                  g_assert_not_reached ();
                }
            }
          else if (data->time > end_time)
            {
              /* gone */
            }
        }
      else if (!in_old_state && in_new_state)
        {
          int64_t start_time, end_time;

          start_time = out_end + elt->transition.delay;
          end_time = start_time + elt->transition.duration;

          if (data->time < start_time)
            {
              /* not started */
            }
          else if (data->time >= start_time &&
                   data->time <= end_time)
            {
              /* appearing */
              double t = (data->time - start_time) / (double) (end_time - start_time);

              t = apply_easing (elt->transition.easing, t);

              switch (elt->transition.type)
                {
                case GTK_PATH_TRANSITION_TYPE_NONE:
                  if (t > 0)
                    paint_elt_animated (self, elt, 0, 1, data);
                  break;
                case GTK_PATH_TRANSITION_TYPE_MORPH:
                  if (t > 0)
                    paint_elt_with_blobbing (self, elt, 1 - t, data);
                  break;
                case GTK_PATH_TRANSITION_TYPE_FADE:
                  if (t > 0)
                    paint_elt_with_fade (self, elt, 1 - t, data);
                  break;
                case GTK_PATH_TRANSITION_TYPE_ANIMATE:
                  paint_elt_animated (self, elt, (1 - t) * elt->origin, 1 - (1 - t) * (1 - elt->origin), data);
                  break;
                default:
                  g_assert_not_reached ();
                }
            }
          else if (data->time > end_time)
            {
              paint_elt_animated (self, elt, 0, 1, data);
            }
        }
    }

  if (self->transition.running || data->time < self->animation.end_time)
    {
      if (!self->pending_invalidate)
        self->pending_invalidate = g_idle_add_once ((GSourceOnceFunc) invalidate_in_idle, self);
    }

  if (data->time >= self->transition.start_time + self->transition.out_duration + self->transition.in_duration)
    self->transition.running = FALSE;
}

/* }}} */
/* {{{ GtkSymbolicPaintable implementation */

static void
gtk_path_paintable_snapshot_with_weight (GtkSymbolicPaintable *paintable,
                                         GtkSnapshot          *snapshot,
                                         double                width,
                                         double                height,
                                         const GdkRGBA        *colors,
                                         size_t                 n_colors,
                                         double                weight)
{
  GtkPathPaintable *self = GTK_PATH_PAINTABLE (paintable);
  PaintData data;
  graphene_rect_t view_box;

  data.snapshot = snapshot;
  data.width = width;
  data.height = height;
  data.colors = colors;
  data.n_colors = n_colors;
  data.weight = self->weight >= 1 ? self->weight : weight;
  data.time = g_get_monotonic_time ();

  if (self->view_box.size.width == 0 || self->view_box.size.height == 0)
    graphene_rect_init (&view_box, 0, 0, self->width, self->height);
  else
    view_box = self->view_box;

  gtk_snapshot_save (snapshot);
  gtk_snapshot_scale (snapshot, width / view_box.size.width,
                                height / view_box.size.height);
  gtk_snapshot_translate (snapshot,
                          &GRAPHENE_POINT_INIT (- view_box.origin.x,
                                                - view_box.origin.y));

  paint (self, &data);
  gtk_snapshot_restore (snapshot);
}

static void
gtk_path_paintable_snapshot_symbolic (GtkSymbolicPaintable *paintable,
                                      GtkSnapshot          *snapshot,
                                      double                width,
                                      double                height,
                                      const GdkRGBA        *colors,
                                      size_t                 n_colors)
{
  gtk_path_paintable_snapshot_with_weight (paintable,
                                           snapshot,
                                           width, height,
                                           colors, n_colors,
                                           400);
}

static void
gtk_path_paintable_init_symbolic_paintable_interface (GtkSymbolicPaintableInterface *iface)
{
  iface->snapshot_symbolic = gtk_path_paintable_snapshot_symbolic;
  iface->snapshot_with_weight = gtk_path_paintable_snapshot_with_weight;
}

/* }}} */
/* {{{ GdkPaintable implementation */

static void
gtk_path_paintable_snapshot (GdkPaintable *paintable,
                             GtkSnapshot  *snapshot,
                             double        width,
                             double        height)
{
  gtk_path_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (paintable),
                                        snapshot,
                                        width, height,
                                        NULL, 0);
}

static int
gtk_path_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  GtkPathPaintable *self = GTK_PATH_PAINTABLE (paintable);

  return ceil (self->width);
}

static int
gtk_path_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  GtkPathPaintable *self = GTK_PATH_PAINTABLE (paintable);

  return ceil (self->height);
}

static void
gtk_path_paintable_init_paintable_interface (GdkPaintableInterface *iface)
{
  iface->snapshot = gtk_path_paintable_snapshot;
  iface->get_intrinsic_width = gtk_path_paintable_get_intrinsic_width;
  iface->get_intrinsic_height = gtk_path_paintable_get_intrinsic_height;
}

/* }}} */
/* {{{ GObject boilerplate */

G_DEFINE_TYPE_WITH_CODE (GtkPathPaintable, gtk_path_paintable, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GDK_TYPE_PAINTABLE,
                                                gtk_path_paintable_init_paintable_interface)
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_SYMBOLIC_PAINTABLE,
                                                gtk_path_paintable_init_symbolic_paintable_interface))

static void
gtk_path_paintable_init (GtkPathPaintable *self)
{
  self->paths = g_array_new (FALSE, TRUE, sizeof (PathElt));
  g_array_set_clear_func (self->paths, clear_path_elt);

  self->state = GTK_PATH_PAINTABLE_STATE_EMPTY;
  self->max_state = GTK_PATH_PAINTABLE_STATE_EMPTY;
  self->weight = -1.f;

  self->transition.running = FALSE;
}

static void
gtk_path_paintable_dispose (GObject *object)
{
  GtkPathPaintable *self = GTK_PATH_PAINTABLE (object);

  g_clear_handle_id (&self->pending_notify, g_source_remove);
  g_clear_handle_id (&self->pending_invalidate, g_source_remove);

  g_array_unref (self->paths);

  G_OBJECT_CLASS (gtk_path_paintable_parent_class)->dispose (object);
}

static void
gtk_path_paintable_get_property (GObject      *object,
                                 unsigned int  property_id,
                                 GValue       *value,
                                 GParamSpec   *pspec)
{
  GtkPathPaintable *self = GTK_PATH_PAINTABLE (object);

  switch (property_id)
    {
    case PROP_STATE:
      g_value_set_uint (value, self->state);
      break;

    case PROP_WEIGHT:
      g_value_set_float (value, self->weight);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_path_paintable_set_property (GObject      *object,
                                 unsigned int  property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  GtkPathPaintable *self = GTK_PATH_PAINTABLE (object);

  switch (property_id)
    {
    case PROP_STATE:
      gtk_path_paintable_set_state (self, g_value_get_uint (value));
      break;

    case PROP_WEIGHT:
      gtk_path_paintable_set_weight (self, g_value_get_float (value));
      break;

    case PROP_RESOURCE:
      {
        const char *path = g_value_get_string (value);
        if (path)
          {
            GBytes *bytes = g_resources_lookup_data (path, 0, NULL);
            gtk_path_paintable_init_from_bytes (self, bytes, NULL);
            g_bytes_unref (bytes);
          }
      }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
gtk_path_paintable_class_init (GtkPathPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = gtk_path_paintable_dispose;
  object_class->get_property = gtk_path_paintable_get_property;
  object_class->set_property = gtk_path_paintable_set_property;

  /**
   * GtkPathPaintable:state:
   *
   * The current state of the paintable.
   *
   * This can be a number between 0 and 63, or the special value
   * `GTK_PATH_PAINTABLE_STATE_EMPTY` to indicate the 'none' state
   * in which nothing is drawn.
   *
   * Since: 4.22
   */
  properties[PROP_STATE] =
    g_param_spec_uint ("state", NULL, NULL,
                       0, G_MAXUINT, GTK_PATH_PAINTABLE_STATE_EMPTY,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPathPaintable:weight:
   *
   * If not set to -1, this value overrides the weight used
   * when rendering the paintable.
   *
   * Since: 4.22
   */
  properties[PROP_WEIGHT] =
    g_param_spec_float ("weight", NULL, NULL,
                        -1.f, 1000.f, -1.f,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  /**
   * GtkPathPaintable:resource:
   *
   * Construct-only property to create a path paintable from
   * a resource in ui files.
   *
   * Since: 4.22
   */
  properties[PROP_RESOURCE] =
    g_param_spec_string ("resource", NULL, NULL,
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);
}

/* }}} */
/* {{{ Public API */

/**
 * gtk_path_paintable_set_state:
 * @self: a paintable
 * @state: the state to set, as a value between 0 and 63,
 *   or `GTK_PATH_PAINTABLE_STATE_EMPTY`
 *
 * Sets the state of the paintable.
 *
 * Use [method@Gtk.PathPaintable.get_max_state] to
 * find out what states @self has.
 *
 * Since: 4.22
 */
void
gtk_path_paintable_set_state (GtkPathPaintable *self,
                              unsigned int      state)
{
  int64_t out, in;

  g_return_if_fail (GTK_IS_PATH_PAINTABLE (self));
  g_return_if_fail (state == GTK_PATH_PAINTABLE_STATE_EMPTY || state <= 63);

  if (self->state == state)
    return;

  if (compute_transition_duration (self, self->state, state, &out, &in))
    {
      self->transition.old_state = self->state;
      self->transition.new_state = state;
      self->transition.out_duration = out;
      self->transition.in_duration = in;
      self->transition.running = TRUE;
      self->transition.start_time = g_get_monotonic_time ();

      gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
    }
  else
    {
      self->transition.running = FALSE;
      set_state (self, state, FALSE);
    }
}

/**
 * gtk_path_paintable_get_state:
 * @self: a paintable
 *
 * Gets the current state of the paintable.
 *
 * Returns: the state
 *
 * Since: 4.22
 */
unsigned int
gtk_path_paintable_get_state (GtkPathPaintable *self)
{
  g_return_val_if_fail (GTK_IS_PATH_PAINTABLE (self), 0);

  return self->state;
}

/**
 * gtk_path_paintable_set_weight:
 * @self: a paintable
 * @weight: the font weight, as a value between -1 and 1000,
 *
 * Sets the weight that is used when stroking paths.
 *
 * This number is interpreted similar to a font weight,
 * with 400 being the nominal default weight that leaves
 * the stroke width unchanged. Smaller values produce
 * lighter strokes, bigger values heavier strokes.
 *
 * The default value of -1 means to use the weight
 * from CSS -gtk-icon-weight property.
 *
 * Since: 4.22
 */
void
gtk_path_paintable_set_weight (GtkPathPaintable *self,
                               float             weight)
{
  g_return_if_fail (GTK_IS_PATH_PAINTABLE (self));
  g_return_if_fail (-1 <= weight && weight <= 1000);

  if (self->weight == weight)
    return;

  self->weight = weight;

  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WEIGHT]);
}

/**
 * gtk_path_paintable_get_weight:
 * @self: a paintable
 *
 * Gets the value of the weight property.
 *
 * Returns: the weight
 *
 * Since: 4.22
 */
float
gtk_path_paintable_get_weight (GtkPathPaintable *self)
{
  g_return_val_if_fail (GTK_IS_PATH_PAINTABLE (self), -1);

  return self->weight;
}

/**
 * gtk_path_paintable_get_max_state:
 * @self: a paintable
 *
 * Returns the largest value that occurs among the
 * states of the paths in this paintable.
 *
 * Returns: the largest state of the paintable
 *
 * Since: 4.22
 */
unsigned int
gtk_path_paintable_get_max_state (GtkPathPaintable *self)
{
  g_return_val_if_fail (GTK_IS_PATH_PAINTABLE (self), 0);

  if (self->max_state == GTK_PATH_PAINTABLE_STATE_EMPTY)
    self->max_state = compute_max_state (self);

  return self->max_state;
}

/**
 * gtk_path_paintable_new_from_bytes:
 * @bytes: the data
 * @error: return location for an error
 *
 * Parses the data in @bytes and creates a
 * paintable.
 *
 * The supported format is a [subset](icon-format.html)
 * of SVG.
 *
 * Returns: the paintable
 *
 * Since: 4.22
 */
GtkPathPaintable *
gtk_path_paintable_new_from_bytes (GBytes  *bytes,
                                   GError **error)
{
  GtkPathPaintable *paintable;

  paintable = g_object_new (GTK_PATH_PAINTABLE_TYPE, NULL);

  if (!gtk_path_paintable_init_from_bytes (paintable, bytes, error))
    g_clear_object (&paintable);

  return paintable;
}

/**
 * gtk_path_paintable_new_from_resource:
 * @path: the resource path
 *
 * Parses the resource and creates a paintable.
 *
 * The supported format is a [subset](icon-format.html)
 * of SVG.
 *
 * Returns: the paintable
 *
 * Since: 4.22
 */
GtkPathPaintable *
gtk_path_paintable_new_from_resource (const char *path)
{
  return g_object_new (GTK_PATH_PAINTABLE_TYPE,
                       "resource", path,
                       NULL);
}

/* }}} */

/* vim:set foldmethod=marker: */
