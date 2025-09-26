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

#include "path-paintable.h"

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
 * - transitioning out the paths that are no longer present
 * - transitioning in the paths that are newly present
 *
 * The duration of the phases, the delay between them and the
 * easing function are settable. There is a number of choices
 * for the effect used to transition each path:
 * - no effect
 * - animated stroke
 * - morphing
 * - fade
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

  struct {
    TransitionType type;
    float duration;
    float delay;
    EasingFunction easing;
    float origin;
  } transition;

  struct {
    AnimationType type;
    AnimationDirection direction;
    float duration;
    EasingFunction easing;
    float segment;
  } animation;

  struct {
    gboolean enabled;
    float width;
    float min_width;
    float max_width;
    guint symbolic;
    GdkRGBA color;
    GskLineCap linecap;
    GskLineJoin linejoin;
  } stroke;

  struct {
    gboolean enabled;
    GskFillRule rule;
    guint symbolic;
    GdkRGBA color;
  } fill;

  struct {
    gsize to;
    float position;
  } attach;

} PathElt;

struct _PathPaintable
{
  GObject parent_instance;
  GArray *paths;

  double width, height;

  guint state;
  float weight;

  GStrv keywords;
  GtkPathPaintable *render_paintable;
};

enum
{
  PROP_STATE = 1,
  PROP_MAX_STATE,
  PROP_WEIGHT,
  PROP_RESOURCE,
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

static void
notify_state (GObject *object)
{
  g_object_notify_by_pspec (object, properties[PROP_STATE]);
}

static void
ensure_render_paintable (PathPaintable *self)
{
  if (!self->render_paintable)
    {
      GBytes *bytes = path_paintable_serialize (self, self->state);
      GError *error = NULL;

      self->render_paintable = gtk_path_paintable_new_from_bytes (bytes, &error);

      if (!self->render_paintable)
        {
          g_print ("Failed to parse\n%s\n", (const char *) g_bytes_get_data (bytes, NULL));
          g_error ("%s", error->message);
        }

      gtk_path_paintable_set_weight (self->render_paintable, self->weight);

      g_signal_connect_swapped (self->render_paintable, "notify::state",
                                G_CALLBACK (notify_state), self);

      g_signal_connect_swapped (self->render_paintable, "invalidate-contents",
                                G_CALLBACK (gdk_paintable_invalidate_contents), self);
      g_signal_connect_swapped (self->render_paintable, "invalidate-size",
                                G_CALLBACK (gdk_paintable_invalidate_size), self);

    }
}

static gboolean
path_equal (GskPath *p1,
            GskPath *p2)
{
  char *s1 = gsk_path_to_string (p1);
  char *s2 = gsk_path_to_string (p2);
  gboolean res;

  res = strcmp (s1, s2) == 0;

  g_free (s1);
  g_free (s2);

  return res;
}

static gboolean
path_elt_equal (PathElt *elt1,
                PathElt *elt2)
{
  if (elt1->states != elt2->states)
    return FALSE;

  if (elt1->animation.type != elt2->animation.type ||
      elt1->animation.direction != elt2->animation.direction ||
      elt1->animation.duration != elt2->animation.duration ||
      elt1->animation.easing != elt2->animation.easing)
    return FALSE;

  if (elt1->transition.type != elt2->transition.type ||
      elt1->transition.duration != elt2->transition.duration ||
      elt1->transition.delay != elt2->transition.delay ||
      elt1->transition.easing != elt2->transition.easing ||
      elt1->transition.origin != elt2->transition.origin)
    return FALSE;

  if (elt1->stroke.enabled != elt2->stroke.enabled ||
      elt1->stroke.width != elt2->stroke.width ||
      elt1->stroke.min_width != elt2->stroke.min_width ||
      elt1->stroke.max_width != elt2->stroke.max_width ||
      elt1->stroke.symbolic != elt2->stroke.symbolic ||
      elt1->stroke.linecap != elt2->stroke.linecap ||
      elt1->stroke.linejoin != elt2->stroke.linejoin ||
      elt1->stroke.color.alpha != elt2->stroke.color.alpha)
    return FALSE;

  if (elt1->stroke.symbolic == 0xffff)
    {
      if (!gdk_rgba_equal (&elt1->stroke.color, &elt2->stroke.color))
        return FALSE;
    }

  if (elt1->fill.enabled != elt2->fill.enabled ||
      elt1->fill.rule != elt2->fill.rule ||
      elt1->fill.symbolic != elt2->fill.symbolic ||
      elt1->fill.color.alpha != elt2->fill.color.alpha)
    return FALSE;

  if (elt1->fill.symbolic == 0xffff)
    {
      if (!gdk_rgba_equal (&elt1->fill.color, &elt2->fill.color))
        return FALSE;
    }

  if (elt1->attach.to != elt2->attach.to ||
      elt1->attach.position != elt2->attach.position)
    return FALSE;

  if (!path_equal (elt1->path, elt2->path))
    return FALSE;

  return TRUE;
}

/* }}} */
/* {{{ GtkSymbolicPaintable implementation */

static void
path_paintable_snapshot_with_weight (GtkSymbolicPaintable *paintable,
                                     GtkSnapshot          *snapshot,
                                     double                width,
                                     double                height,
                                     const GdkRGBA        *colors,
                                     gsize                 n_colors,
                                     double                weight)
{
  PathPaintable *self = PATH_PAINTABLE (paintable);

  ensure_render_paintable (self);

  gtk_symbolic_paintable_snapshot_with_weight (GTK_SYMBOLIC_PAINTABLE (self->render_paintable),
                                               snapshot,
                                               width, height,
                                               colors, n_colors,
                                               weight);
}

static void
path_paintable_snapshot_symbolic (GtkSymbolicPaintable  *paintable,
                                  GtkSnapshot           *snapshot,
                                  double                 width,
                                  double                 height,
                                  const GdkRGBA         *colors,
                                  gsize                  n_colors)
{
  PathPaintable *self = PATH_PAINTABLE (paintable);

  ensure_render_paintable (self);

  gtk_symbolic_paintable_snapshot_symbolic (GTK_SYMBOLIC_PAINTABLE (self->render_paintable),
                                            snapshot,
                                            width, height,
                                            colors, n_colors);
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
  PathPaintable *self = PATH_PAINTABLE (paintable);

  ensure_render_paintable (self);

  gdk_paintable_snapshot (GDK_PAINTABLE (self->render_paintable),
                          snapshot,
                          width, height);
}

static int
path_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  PathPaintable *self = PATH_PAINTABLE (paintable);

  ensure_render_paintable (self);

  return gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (self->render_paintable));
}

static int
path_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  PathPaintable *self = PATH_PAINTABLE (paintable);

  return gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (self->render_paintable));
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

  self->state = STATE_UNSET;
}

static void
path_paintable_dispose (GObject *object)
{
  PathPaintable *self = PATH_PAINTABLE (object);

  g_array_unref (self->paths);

  g_clear_pointer (&self->keywords, g_strfreev);

  if (self->render_paintable)
    {
      g_signal_handlers_disconnect_by_func (self->render_paintable, notify_state, self);
      g_signal_handlers_disconnect_by_func (self->render_paintable, gdk_paintable_invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->render_paintable, gdk_paintable_invalidate_size, self);

    }

  g_clear_object (&self->render_paintable);

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
    case PROP_STATE:
      g_value_set_uint (value, self->state);
      break;

    case PROP_MAX_STATE:
      g_value_set_uint (value, path_paintable_get_max_state (self));
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
path_paintable_set_property (GObject      *object,
                             guint         property_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  PathPaintable *self = PATH_PAINTABLE (object);

  switch (property_id)
    {
    case PROP_STATE:
      path_paintable_set_state (self, g_value_get_uint (value));
      break;

    case PROP_WEIGHT:
      path_paintable_set_weight (self, g_value_get_float (value));
      break;

    case PROP_RESOURCE:
      {
        const char *path = g_value_get_string (value);
        if (path)
          {
            GBytes *bytes;
            GError *error = NULL;

            bytes = g_resources_lookup_data (path, 0, NULL);
            if (!parse_symbolic_svg (self, bytes, &error))
              g_error ("%s", error->message);

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
path_paintable_changed (PathPaintable *self)
{
  g_clear_object (&self->render_paintable);
  gdk_paintable_invalidate_contents (GDK_PAINTABLE (self));
}

static void
path_paintable_class_init (PathPaintableClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = path_paintable_dispose;
  object_class->get_property = path_paintable_get_property;
  object_class->set_property = path_paintable_set_property;

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

  properties[PROP_WEIGHT] =
    g_param_spec_float ("weight", NULL, NULL,
                        -1.f, 1000.f, -1.f,
                        G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_RESOURCE] =
    g_param_spec_string ("resource", NULL, NULL,
                         NULL,
                         G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (object_class, NUM_PROPERTIES, properties);

  /**
   * PathPaintable::changed:
   *
   * Emitted when the paintable changes in any way that
   * would change the serialization.
   */
  signals[CHANGED] =
    g_signal_new_class_handler ("changed",
                                G_TYPE_FROM_CLASS (object_class),
                                G_SIGNAL_RUN_LAST,
                                G_CALLBACK (path_paintable_changed),
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

  g_signal_emit (self, signals[CHANGED], 0);
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
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

  elt.path = gsk_path_ref (path);

  elt.states = ALL_STATES;

  elt.transition.type = TRANSITION_TYPE_NONE;
  elt.transition.duration = 0;
  elt.transition.delay = 0;
  elt.transition.easing = EASING_FUNCTION_LINEAR;
  elt.transition.origin = 0;

  elt.animation.type = ANIMATION_TYPE_NONE;
  elt.animation.direction = ANIMATION_DIRECTION_NORMAL;
  elt.animation.duration = 0;
  elt.animation.easing = EASING_FUNCTION_LINEAR;
  elt.animation.segment = 0.2;

  elt.fill.enabled = FALSE;
  elt.fill.rule = GSK_FILL_RULE_WINDING;
  elt.fill.symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
  elt.fill.color = (GdkRGBA) { 0, 0, 0, 1 };

  elt.stroke.enabled = TRUE;
  elt.stroke.width = 2;
  elt.stroke.min_width = 0.5;
  elt.stroke.max_width = 5;
  elt.stroke.linecap = GSK_LINE_CAP_ROUND;
  elt.stroke.linejoin = GSK_LINE_JOIN_ROUND;
  elt.stroke.symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
  elt.stroke.color = (GdkRGBA) { 0, 0, 0, 1 };

  elt.attach.to = (gsize) -1;
  elt.attach.position = 0;

  g_array_append_val (self->paths, elt);

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

      if (elt->attach.to == (gsize) -1)
        continue;

      if (elt->attach.to == idx)
        elt->attach.to = (gsize) -1;
      else if (elt->attach.to > idx)
        elt->attach.to -= 1;
    }

  g_array_remove_index (self->paths, idx);

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

      if (elt->attach.to == (gsize) -1)
        continue;

      if (elt->attach.to == idx)
        elt->attach.to = new_pos;
      else if (idx < elt->attach.to && elt->attach.to <= new_pos)
        elt->attach.to -= 1;
      else if (elt->attach.to >= new_pos && elt->attach.to < idx)
        elt->attach.to += 1;
    }

  tmp = g_array_index (self->paths, PathElt, idx);
  gsk_path_ref (tmp.path);

  g_array_remove_index (self->paths, idx);
  g_array_insert_val (self->paths, new_pos, tmp);

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

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_STATE]);

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_transition (PathPaintable   *self,
                                    gsize            idx,
                                    TransitionType  type,
                                    float            duration,
                                    float            delay,
                                    EasingFunction   easing)
{
  g_return_if_fail (idx < self->paths->len);
  g_return_if_fail (duration >= 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->transition.type == type &&
      elt->transition.duration == duration &&
      elt->transition.delay == delay &&
      elt->transition.easing == easing)
    return;

  elt->transition.type = type;
  elt->transition.duration = duration;
  elt->transition.delay = delay;
  elt->transition.easing = easing;

  if (elt->fill.enabled && elt->transition.type == TRANSITION_TYPE_ANIMATE)
    g_warning ("Can't currently transition fills");

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_animation (PathPaintable      *self,
                                   gsize               idx,
                                   AnimationType       type,
                                   AnimationDirection  direction,
                                   float               duration,
                                   EasingFunction      easing,
                                   float               segment)
{
  g_return_if_fail (idx < self->paths->len);
  g_return_if_fail (duration >= 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->animation.type == type &&
      elt->animation.direction == direction &&
      elt->animation.duration == duration &&
      elt->animation.easing == easing &&
      elt->animation.segment == segment)
    return;

  elt->animation.type = type;
  elt->animation.direction = direction;
  elt->animation.duration = duration;
  elt->animation.easing = easing;
  elt->animation.segment = segment;

  g_signal_emit (self, signals[CHANGED], 0);
}

AnimationType
path_paintable_get_path_animation_type (PathPaintable *self,
                                        gsize          idx)
{
  g_return_val_if_fail (idx < self->paths->len, ANIMATION_TYPE_NONE);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.type;
}

AnimationDirection
path_paintable_get_path_animation_direction (PathPaintable *self,
                                             gsize          idx)
{
  g_return_val_if_fail (idx < self->paths->len, ANIMATION_DIRECTION_NORMAL);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.direction;
}

float
path_paintable_get_path_animation_duration (PathPaintable *self,
                                            gsize          idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.duration;
}

EasingFunction
path_paintable_get_path_animation_easing (PathPaintable *self,
                                          gsize          idx)
{
  g_return_val_if_fail (idx< self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.easing;
}

float
path_paintable_get_path_animation_segment (PathPaintable *self,
                                           gsize          idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0.2f);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.segment;
}
void
path_paintable_set_path_origin (PathPaintable *self,
                                gsize          idx,
                                float          origin)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->transition.origin == origin)
    return;

  elt->transition.origin = origin;

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_fill (PathPaintable   *self,
                              gsize            idx,
                              gboolean         enabled,
                              GskFillRule      rule,
                              guint            symbolic,
                              const GdkRGBA   *color)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->fill.enabled == enabled &&
      elt->fill.rule == rule &&
      elt->fill.symbolic == symbolic &&
      ((symbolic != 0xffff && elt->fill.color.alpha == color->alpha) ||
       gdk_rgba_equal (&elt->fill.color, color)))
    return;

  elt->fill.enabled = enabled;
  elt->fill.rule = rule;
  elt->fill.symbolic = symbolic;
  elt->fill.color = *color;

  if (elt->fill.enabled && elt->transition.type == TRANSITION_TYPE_ANIMATE)
    g_warning ("Can't currently transition fills");

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_stroke (PathPaintable *self,
                                gsize          idx,
                                gboolean       enabled,
                                GskStroke     *stroke,
                                guint          symbolic,
                                const GdkRGBA *color)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->stroke.enabled == enabled &&
      elt->stroke.width == gsk_stroke_get_line_width (stroke) &&
      elt->stroke.linecap == gsk_stroke_get_line_cap (stroke) &&
      elt->stroke.linejoin == gsk_stroke_get_line_join (stroke) &&
      elt->stroke.symbolic == symbolic &&
      ((symbolic != 0xffff && elt->stroke.color.alpha == color->alpha) ||
       gdk_rgba_equal (&elt->stroke.color, color)))
    return;

  elt->stroke.enabled = enabled;
  elt->stroke.width = gsk_stroke_get_line_width (stroke);
  elt->stroke.min_width = elt->stroke.width * 100. / 400.;
  elt->stroke.max_width = elt->stroke.width * 1000. / 400.;
  elt->stroke.linecap = gsk_stroke_get_line_cap (stroke);
  elt->stroke.linejoin = gsk_stroke_get_line_join (stroke);
  elt->stroke.symbolic = symbolic;
  elt->stroke.color = *color;

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_stroke_variation (PathPaintable *self,
                                          gsize          idx,
                                          float          min_width,
                                          float          max_width)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->stroke.min_width == min_width &&
      elt->stroke.max_width == max_width)
    return;

  elt->stroke.min_width = min_width;
  elt->stroke.max_width = max_width;

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_get_path_stroke_variation (PathPaintable *self,
                                          gsize          idx,
                                          float         *min_width,
                                          float         *max_width)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  *min_width = elt->stroke.min_width;
  *max_width = elt->stroke.max_width;
}

void
path_paintable_attach_path (PathPaintable *self,
                            gsize          idx,
                            gsize          to,
                            float          pos)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->attach.to == to && elt->attach.position == pos)
    return;

  elt->attach.to = to;
  elt->attach.position = pos;

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

  *to = elt->attach.to;
  *pos = elt->attach.position;
}

void
path_paintable_set_keywords (PathPaintable *self,
                             GStrv          keywords)
{
  g_clear_pointer (&self->keywords, g_strfreev);
  self->keywords = g_strdupv (keywords);

  g_signal_emit (self, signals[CHANGED], 0);
}

GStrv
path_paintable_get_keywords (PathPaintable *self)
{
  return self->keywords;
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

TransitionType
path_paintable_get_path_transition_type (PathPaintable *self,
                                         gsize          idx)
{
  g_return_val_if_fail (idx< self->paths->len, TRANSITION_TYPE_NONE);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition.type;
}

float
path_paintable_get_path_transition_duration (PathPaintable *self,
                                             gsize          idx)
{
  g_return_val_if_fail (idx< self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition.duration;
}

float
path_paintable_get_path_transition_delay (PathPaintable *self,
                                          gsize          idx)
{
  g_return_val_if_fail (idx< self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition.delay;
}

EasingFunction
path_paintable_get_path_transition_easing (PathPaintable *self,
                                           gsize          idx)
{
  g_return_val_if_fail (idx< self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition.easing;
}

float
path_paintable_get_path_origin (PathPaintable *self,
                                gsize          idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0.0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition.origin;
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

  *rule = elt->fill.rule;
  *symbolic = elt->fill.symbolic;
  *color = elt->fill.color;

  return elt->fill.enabled;
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

  gsk_stroke_set_line_width (stroke, elt->stroke.width);
  gsk_stroke_set_line_cap (stroke, elt->stroke.linecap);
  gsk_stroke_set_line_join (stroke, elt->stroke.linejoin);
  *symbolic = elt->stroke.symbolic;
  *color = elt->stroke.color;

  return elt->stroke.enabled;
}

PathPaintable *
path_paintable_copy (PathPaintable *self)
{
  PathPaintable *other;
  GskStroke *stroke;
  gboolean enabled;
  GskFillRule rule = GSK_FILL_RULE_WINDING;
  guint symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
  GdkRGBA color = (GdkRGBA) { 0, 0, 0, 1 };
  gsize to = (gsize) -1;
  float pos = 0;

  other = path_paintable_new ();

  path_paintable_set_size (other, self->width, self->height);
  path_paintable_set_keywords (other, self->keywords);

  stroke = gsk_stroke_new (1);

  for (gsize i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      path_paintable_add_path (other, elt->path);
      path_paintable_set_path_states (other, i, path_paintable_get_path_states (self, i));
      path_paintable_set_path_transition (other, i,
                                          path_paintable_get_path_transition_type (self, i),
                                          path_paintable_get_path_transition_duration (self, i),
                                          path_paintable_get_path_transition_delay (self, i),
                                          path_paintable_get_path_transition_easing (self, i));
      path_paintable_set_path_origin (other, i, path_paintable_get_path_origin (self, i));
      path_paintable_set_path_animation (other, i,
                                         path_paintable_get_path_animation_type (self, i),
                                         path_paintable_get_path_animation_direction (self, i),
                                         path_paintable_get_path_animation_duration (self, i),
                                         path_paintable_get_path_animation_easing (self, i),
                                         path_paintable_get_path_animation_segment (self, i));

      enabled = path_paintable_get_path_fill (self, i, &rule, &symbolic, &color);
      path_paintable_set_path_fill (other, i, enabled, rule, symbolic, &color);

      enabled = path_paintable_get_path_stroke (self, i, stroke, &symbolic, &color);
      path_paintable_set_path_stroke (other, i, enabled, stroke, symbolic, &color);

      path_paintable_get_attach_path (self, i, &to, &pos);
      path_paintable_attach_path (other, i, to, pos);
    }

  gsk_stroke_free (stroke);

  return other;
}

PathPaintable *
path_paintable_combine (PathPaintable *one,
                        PathPaintable *two)
{
  PathPaintable *res;
  guint max_state;
  gsize n_paths;
  GskStroke *stroke;

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
            states = states | (G_GUINT64_CONSTANT (1) << j);

          path_paintable_set_path_states (res, i, states);
        }
    }

  stroke = gsk_stroke_new (1);

  for (gsize i = 0; i < path_paintable_get_n_paths (two); i++)
    {
      gsize idx;
      guint64 states;
      gboolean enabled;
      GskFillRule rule = GSK_FILL_RULE_WINDING;
      guint symbolic = 0;
      GdkRGBA color;
      gsize attach_to = (gsize) -1;
      float attach_pos = 0;;

      idx = path_paintable_add_path (res, path_paintable_get_path (two, i));

      path_paintable_set_path_transition (res, idx,
                                          path_paintable_get_path_transition_type (two, i),
                                          path_paintable_get_path_transition_duration (two, i),
                                          path_paintable_get_path_transition_delay (two, i),
                                          path_paintable_get_path_transition_easing (two, i));
      path_paintable_set_path_origin (res, idx,
                                      path_paintable_get_path_origin (two, i));

      path_paintable_set_path_animation (res, idx,
                                         path_paintable_get_path_animation_type (two, i),
                                         path_paintable_get_path_animation_direction (two, i),
                                         path_paintable_get_path_animation_duration (two, i),
                                         path_paintable_get_path_animation_easing (two, i),
                                         path_paintable_get_path_animation_segment (two, i));

      states = path_paintable_get_path_states (two, i);
      if (states == ALL_STATES)
        {
          guint max2 = path_paintable_get_max_state (two);

          states = 0;
          for (gsize j = 0; j <= max2; j++)
            states |= G_GUINT64_CONSTANT (1) << j;
        }
      path_paintable_set_path_states (res, idx, states << (max_state + 1));

      enabled = path_paintable_get_path_fill (two, i, &rule, &symbolic, &color);
      path_paintable_set_path_fill (res, idx, enabled, rule, symbolic, &color);

      enabled = path_paintable_get_path_stroke (two, i, stroke, &symbolic, &color);
      path_paintable_set_path_stroke (res, idx, enabled, stroke, symbolic, &color);

      path_paintable_get_attach_path (two, i, &attach_to, &attach_pos);
      path_paintable_attach_path (res, idx, attach_to + n_paths, attach_pos);
    }

  gsk_stroke_free (stroke);

  return res;
}

GtkCompatibility
path_paintable_get_compatibility (PathPaintable *self)
{
  /* Compatible with 4.0:
   * - Fills
   *
   * Compatible with 4.20:
   * - Fills
   * - Strokes
   *
   * Compatible with 4.22:
   * - Fills
   * - Strokes
   * - Transitions
   * - Animations
   * - Attachments
   *
   * This is informational.
   * Icons may still render (in a degraded fashion) with older GTK.
   */

  GtkCompatibility compat = GTK_4_0;

  for (gsize i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (elt->stroke.enabled)
        compat = MAX (compat, GTK_4_20);

      if (elt->transition.type != TRANSITION_TYPE_NONE ||
          elt->animation.type != ANIMATION_TYPE_NONE ||
          elt->attach.to != (gsize) - 1)
        compat = MAX (compat, GTK_4_22);
    }

  return compat;
}

/* }}} */
/* {{{ Public API */

void
path_paintable_set_state (PathPaintable *self,
                          guint          state)
{
  if (self->state == state)
    return;

  self->state = state;

  if (self->render_paintable)
    gtk_path_paintable_set_state (self->render_paintable, state);
}

guint
path_paintable_get_state (PathPaintable *self)
{
  return self->state;
}

void
path_paintable_set_weight (PathPaintable *self,
                           float          weight)
{
  if (self->weight == weight)
    return;

  self->weight = weight;

  if (self->render_paintable)
    gtk_path_paintable_set_weight (self->render_paintable, weight);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WEIGHT]);
}

float
path_paintable_get_weight (PathPaintable *self)
{
  return self->weight;
}

guint
path_paintable_get_max_state (PathPaintable *self)
{
  ensure_render_paintable (self);

  return gtk_path_paintable_get_max_state (self->render_paintable);
}

static inline gboolean
g_strv_same (GStrv self,
             GStrv other)
{
  if (self == other)
    return TRUE;

  if ((!self || self[0] == NULL) &&
      (!other || other[0] == NULL))
    return TRUE;

  return self && other &&
         g_strv_equal ((const char * const *) self,
                       (const char * const *) other);
}

gboolean
path_paintable_equal (PathPaintable *self,
                      PathPaintable *other)
{
  if (self->width != other->width ||
      self->height != other->height)
    return FALSE;

  if (self->paths->len != other->paths->len)
    return FALSE;

  if (!g_strv_same (self->keywords, other->keywords))
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
