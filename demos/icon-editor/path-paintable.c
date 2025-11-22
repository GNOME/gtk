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

#define BIT(n) (G_GUINT64_CONSTANT (1) << (n))

struct _PathPaintable
{
  GObject parent_instance;

  GtkSvg *svg;
  GdkPaintable *render_paintable;
};

enum
{
  PROP_STATE = 1,
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
static unsigned int signals[LAST_SIGNAL];

/* {{{ Helpers */

static void
notify_state (GObject *object)
{
  g_object_notify_by_pspec (object, properties[PROP_STATE]);
}

static GdkPaintable *
ensure_render_paintable (PathPaintable *self)
{
  if (!self->render_paintable)
    {
      g_autoptr (GBytes) bytes = NULL;

      bytes = path_paintable_serialize (self, gtk_svg_get_state (self->svg));

      self->render_paintable = GDK_PAINTABLE (gtk_svg_new_from_bytes (bytes));
      gtk_svg_set_weight (GTK_SVG (self->render_paintable), gtk_svg_get_weight (self->svg));
      gtk_svg_play (GTK_SVG (self->render_paintable));

      g_signal_connect_swapped (self->render_paintable, "notify::state",
                                G_CALLBACK (notify_state), self);

      g_signal_connect_swapped (self->render_paintable, "invalidate-contents",
                                G_CALLBACK (gdk_paintable_invalidate_contents), self);
      g_signal_connect_swapped (self->render_paintable, "invalidate-size",
                                G_CALLBACK (gdk_paintable_invalidate_size), self);

    }

  return self->render_paintable;
}

/* }}} */
/* {{{ Parser */

static gboolean
parse_symbolic_svg (PathPaintable  *paintable,
                    GBytes         *bytes,
                    GError        **error)
{
  g_autoptr (GtkSvg) svg = gtk_svg_new_from_bytes (bytes);

  g_set_object (&paintable->svg, svg);

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
                                     size_t                n_colors,
                                     double                weight)
{
  gtk_symbolic_paintable_snapshot_with_weight (GTK_SYMBOLIC_PAINTABLE (ensure_render_paintable (PATH_PAINTABLE (paintable))),
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
                                  size_t                 n_colors)
{
  path_paintable_snapshot_with_weight (paintable, snapshot,
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
  gdk_paintable_snapshot (GDK_PAINTABLE (ensure_render_paintable (PATH_PAINTABLE (paintable))),
                          snapshot,
                          width, height);
}

static int
path_paintable_get_intrinsic_width (GdkPaintable *paintable)
{
  return gdk_paintable_get_intrinsic_width (GDK_PAINTABLE (ensure_render_paintable (PATH_PAINTABLE (paintable))));
}

static int
path_paintable_get_intrinsic_height (GdkPaintable *paintable)
{
  return gdk_paintable_get_intrinsic_height (GDK_PAINTABLE (ensure_render_paintable (PATH_PAINTABLE (paintable))));
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
  self->svg = gtk_svg_new ();
}

static void
path_paintable_dispose (GObject *object)
{
  PathPaintable *self = PATH_PAINTABLE (object);

  if (self->render_paintable)
    {
      g_signal_handlers_disconnect_by_func (self->render_paintable, notify_state, self);
      g_signal_handlers_disconnect_by_func (self->render_paintable, gdk_paintable_invalidate_contents, self);
      g_signal_handlers_disconnect_by_func (self->render_paintable, gdk_paintable_invalidate_size, self);

    }

  g_clear_object (&self->svg);
  g_clear_object (&self->render_paintable);

  G_OBJECT_CLASS (path_paintable_parent_class)->dispose (object);
}

static void
path_paintable_get_property (GObject      *object,
                             unsigned int  property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
{
  PathPaintable *self = PATH_PAINTABLE (object);

  switch (property_id)
    {
    case PROP_STATE:
      g_value_set_uint (value, path_paintable_get_state (self));
      break;

    case PROP_WEIGHT:
      g_value_set_double (value, path_paintable_get_weight (self));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
}

static void
path_paintable_set_property (GObject      *object,
                             unsigned int  property_id,
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
      path_paintable_set_weight (self, g_value_get_double (value));
      break;

    case PROP_RESOURCE:
      {
        const char *path = g_value_get_string (value);
        if (path)
          {
            g_autoptr (GBytes) bytes = NULL;
            GError *error = NULL;

            bytes = g_resources_lookup_data (path, 0, NULL);
            if (!parse_symbolic_svg (self, bytes, &error))
              g_error ("%s", error->message);
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
   * of the paintable, or the special value `(unsigned int) -1`
   * to indicate the 'none' state in which nothing is drawn.
   */
  properties[PROP_STATE] =
    g_param_spec_uint ("state", NULL, NULL,
                       0, G_MAXUINT, 0,
                       G_PARAM_READWRITE | G_PARAM_EXPLICIT_NOTIFY);

  properties[PROP_WEIGHT] =
    g_param_spec_double ("weight", NULL, NULL,
                         -1, 1000.f, -1,
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
  self->svg->width = width;
  self->svg->height = height;

  g_signal_emit (self, signals[CHANGED], 0);
  gdk_paintable_invalidate_size (GDK_PAINTABLE (self));
}

double
path_paintable_get_width (PathPaintable *self)
{
  return self->svg->width;
}

double
path_paintable_get_height (PathPaintable *self)
{
  return self->svg->height;
}

static Shape *
path_paintable_get_shape (PathPaintable *self,
                          size_t         path)
{
  return (Shape *) g_ptr_array_index (self->svg->content->shapes, path);
}

static void
set_default_shape_attrs (Shape *shape)
{
  shape->gpa.states = ALL_STATES;

  shape->gpa.transition = GPA_TRANSITION_NONE;
  shape->gpa.transition_duration = 0;
  shape->gpa.transition_delay = 0;
  shape->gpa.transition_easing = GPA_EASING_LINEAR;
  shape->gpa.origin = 0;

  shape->gpa.animation = GPA_ANIMATION_NORMAL;
  shape->gpa.animation_duration = 0;
  shape->gpa.animation_repeat = REPEAT_FOREVER;
  shape->gpa.animation_segment = 0.2;
  shape->gpa.animation_easing = GPA_EASING_LINEAR;

  svg_shape_attr_set (shape, SHAPE_ATTR_FILL, svg_paint_new_none ());
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE, svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND));
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_WIDTH, svg_number_new (2));
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_MINWIDTH, svg_number_new (0.5));
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_MAXWIDTH, svg_number_new (3));
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_LINECAP, svg_linecap_new (GSK_LINE_CAP_ROUND));
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_LINEJOIN, svg_linejoin_new (GSK_LINE_JOIN_ROUND));
}

size_t
path_paintable_add_path (PathPaintable *self,
                         GskPath       *path)
{
  Shape *shape;

  shape = svg_shape_add (self->svg->content, SHAPE_PATH);
  set_default_shape_attrs (shape);
  svg_shape_attr_set (shape, SHAPE_ATTR_PATH, svg_path_new (path));

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);

  return self->svg->content->shapes->len - 1;
}

size_t
path_paintable_add_shape (PathPaintable *self,
                          ShapeType      shape_type,
                          double        *params,
                          unsigned int   n_params)
{
  Shape *shape;

  shape = svg_shape_add (self->svg->content, shape_type);
  set_default_shape_attrs (shape);

  switch ((unsigned int) shape_type)
    {
    case SHAPE_LINE:
      svg_shape_attr_set (shape, SHAPE_ATTR_X1, svg_number_new (params[0]));
      svg_shape_attr_set (shape, SHAPE_ATTR_Y1, svg_number_new (params[1]));
      svg_shape_attr_set (shape, SHAPE_ATTR_X2, svg_number_new (params[2]));
      svg_shape_attr_set (shape, SHAPE_ATTR_Y2, svg_number_new (params[3]));
      break;
    case SHAPE_CIRCLE:
      svg_shape_attr_set (shape, SHAPE_ATTR_CX, svg_number_new (params[0]));
      svg_shape_attr_set (shape, SHAPE_ATTR_CY, svg_number_new (params[1]));
      svg_shape_attr_set (shape, SHAPE_ATTR_R, svg_number_new (params[2]));
      break;
    case SHAPE_ELLIPSE:
      svg_shape_attr_set (shape, SHAPE_ATTR_CX, svg_number_new (params[0]));
      svg_shape_attr_set (shape, SHAPE_ATTR_CY, svg_number_new (params[1]));
      svg_shape_attr_set (shape, SHAPE_ATTR_RX, svg_number_new (params[2]));
      svg_shape_attr_set (shape, SHAPE_ATTR_RY, svg_number_new (params[3]));
      break;
    case SHAPE_RECT:
      svg_shape_attr_set (shape, SHAPE_ATTR_X, svg_number_new (params[0]));
      svg_shape_attr_set (shape, SHAPE_ATTR_Y, svg_number_new (params[1]));
      svg_shape_attr_set (shape, SHAPE_ATTR_WIDTH, svg_number_new (params[2]));
      svg_shape_attr_set (shape, SHAPE_ATTR_HEIGHT, svg_number_new (params[3]));
      svg_shape_attr_set (shape, SHAPE_ATTR_RX, svg_number_new (params[4]));
      svg_shape_attr_set (shape, SHAPE_ATTR_RY, svg_number_new (params[5]));
      break;
    case SHAPE_POLY_LINE:
    case SHAPE_POLYGON:
      svg_shape_attr_set (shape, SHAPE_ATTR_POINTS, svg_points_new (params, n_params));
      break;
    default:
      g_assert_not_reached ();
    }

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);

  return self->svg->content->shapes->len - 1;
}

void
path_paintable_delete_path (PathPaintable *self,
                            size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  svg_shape_delete (shape);

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);
}

void
path_paintable_move_path (PathPaintable *self,
                          size_t         idx,
                          size_t         new_pos)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  g_ptr_array_remove (self->svg->content->shapes, shape);
  g_ptr_array_insert (self->svg->content->shapes, new_pos, shape);

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);
}

void
path_paintable_duplicate_path (PathPaintable *self,
                               gsize          idx)
{
  graphene_size_t *viewport = &self->svg->view_box.size;
  gsize idx2;
  gboolean do_fill, do_stroke;
  GtkSymbolicColor symbolic;
  GskFillRule fill_rule;
  GdkRGBA color;
  GskStroke *stroke;
  double *params;
  unsigned int n_params;
  double p[6];
  GskPath *path;
  double min = 0;
  double max = 100;
  size_t to = (size_t) -1;
  double pos = 0;

  Shape *shape = path_paintable_get_shape (self, idx);

  switch ((unsigned int) shape->type)
    {
    case SHAPE_POLY_LINE:
    case SHAPE_POLYGON:
      params = svg_shape_attr_get_points (shape, SHAPE_ATTR_POINTS, &n_params);
      idx2 = path_paintable_add_shape (self, shape->type, params, n_params);
      break;
    case SHAPE_LINE:
      p[0] = svg_shape_attr_get_number (shape, SHAPE_ATTR_X1, viewport);
      p[1] = svg_shape_attr_get_number (shape, SHAPE_ATTR_Y1, viewport);
      p[2] = svg_shape_attr_get_number (shape, SHAPE_ATTR_X2, viewport);
      p[3] = svg_shape_attr_get_number (shape, SHAPE_ATTR_Y2, viewport);
      idx2 = path_paintable_add_shape (self, shape->type, p, 4);
      break;
    case SHAPE_CIRCLE:
      p[0] = svg_shape_attr_get_number (shape, SHAPE_ATTR_CX, viewport);
      p[1] = svg_shape_attr_get_number (shape, SHAPE_ATTR_CY, viewport);
      p[2] = svg_shape_attr_get_number (shape, SHAPE_ATTR_R, viewport);
      idx2 = path_paintable_add_shape (self, shape->type, p, 3);
      break;
    case SHAPE_ELLIPSE:
      p[0] = svg_shape_attr_get_number (shape, SHAPE_ATTR_CX, viewport);
      p[1] = svg_shape_attr_get_number (shape, SHAPE_ATTR_CY, viewport);
      p[2] = svg_shape_attr_get_number (shape, SHAPE_ATTR_RX, viewport);
      p[3] = svg_shape_attr_get_number (shape, SHAPE_ATTR_RY, viewport);
      idx2 = path_paintable_add_shape (self, shape->type, p, 4);
      break;
    case SHAPE_RECT:
      p[0] = svg_shape_attr_get_number (shape, SHAPE_ATTR_X, viewport);
      p[1] = svg_shape_attr_get_number (shape, SHAPE_ATTR_Y, viewport);
      p[2] = svg_shape_attr_get_number (shape, SHAPE_ATTR_WIDTH, viewport);
      p[3] = svg_shape_attr_get_number (shape, SHAPE_ATTR_HEIGHT, viewport);
      p[4] = svg_shape_attr_get_number (shape, SHAPE_ATTR_RX, viewport);
      p[5] = svg_shape_attr_get_number (shape, SHAPE_ATTR_RY, viewport);
      idx2 = path_paintable_add_shape (self, shape->type, p, 6);
      break;
    case SHAPE_PATH:
      path = svg_shape_attr_get_path (shape, SHAPE_ATTR_PATH);
      idx2 = path_paintable_add_path (self, path);
      break;
    default:
      g_assert_not_reached ();
    }

  path_paintable_set_path_id (self, idx2,
                              path_paintable_get_path_id (self, idx));
  path_paintable_set_path_states (self, idx2,
                                  path_paintable_get_path_states (self, idx));
  do_fill = path_paintable_get_path_fill (self, idx, &fill_rule, &symbolic, &color);
  path_paintable_set_path_fill (self, idx2, do_fill, fill_rule, symbolic, &color);
  stroke = gsk_stroke_new (1);
  do_stroke = path_paintable_get_path_stroke (self, idx, stroke, &symbolic, &color);
  path_paintable_set_path_stroke (self, idx2, do_stroke, stroke, symbolic, &color);
  gsk_stroke_free (stroke);

  path_paintable_get_path_stroke_variation (self, idx, &min, &max);
  path_paintable_set_path_stroke_variation (self, idx2, min, max);

  path_paintable_set_path_animation (self, idx2,
                                     path_paintable_get_path_animation_direction (self, idx),
                                     path_paintable_get_path_animation_duration (self, idx),
                                     path_paintable_get_path_animation_repeat (self, idx),
                                     path_paintable_get_path_animation_easing (self, idx),
                                     path_paintable_get_path_animation_segment (self, idx));

  path_paintable_set_path_transition (self, idx2,
                                      path_paintable_get_path_transition_type (self, idx),
                                      path_paintable_get_path_transition_duration (self, idx),
                                      path_paintable_get_path_transition_delay (self, idx),
                                      path_paintable_get_path_transition_easing (self, idx));

  path_paintable_set_path_origin (self, idx2,
                                  path_paintable_get_path_origin (self, idx));

  path_paintable_get_attach_path (self, idx, &to, &pos);
  if (to != (gsize) -1)
    path_paintable_attach_path (self, idx2, to, pos);

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);
}

void
path_paintable_set_path (PathPaintable *self,
                         size_t         idx,
                         GskPath       *path)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  shape->type = SHAPE_PATH;

  svg_shape_attr_set (shape, SHAPE_ATTR_PATH, svg_path_new (path));

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_shape (PathPaintable *self,
                          size_t         idx,
                          ShapeType      shape_type,
                          double        *params,
                          unsigned int   n_params)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  shape->type = shape_type;

  switch ((unsigned int) shape_type)
    {
    case SHAPE_LINE:
      svg_shape_attr_set (shape, SHAPE_ATTR_X1, svg_number_new (params[0]));
      svg_shape_attr_set (shape, SHAPE_ATTR_Y1, svg_number_new (params[1]));
      svg_shape_attr_set (shape, SHAPE_ATTR_X2, svg_number_new (params[2]));
      svg_shape_attr_set (shape, SHAPE_ATTR_Y2, svg_number_new (params[3]));
      break;
    case SHAPE_CIRCLE:
      svg_shape_attr_set (shape, SHAPE_ATTR_CX, svg_number_new (params[0]));
      svg_shape_attr_set (shape, SHAPE_ATTR_CY, svg_number_new (params[1]));
      svg_shape_attr_set (shape, SHAPE_ATTR_R, svg_number_new (params[2]));
      break;
    case SHAPE_ELLIPSE:
      svg_shape_attr_set (shape, SHAPE_ATTR_CX, svg_number_new (params[0]));
      svg_shape_attr_set (shape, SHAPE_ATTR_CY, svg_number_new (params[1]));
      svg_shape_attr_set (shape, SHAPE_ATTR_RX, svg_number_new (params[2]));
      svg_shape_attr_set (shape, SHAPE_ATTR_RY, svg_number_new (params[3]));
      break;
    case SHAPE_RECT:
      svg_shape_attr_set (shape, SHAPE_ATTR_X, svg_number_new (params[0]));
      svg_shape_attr_set (shape, SHAPE_ATTR_Y, svg_number_new (params[1]));
      svg_shape_attr_set (shape, SHAPE_ATTR_WIDTH, svg_number_new (params[2]));
      svg_shape_attr_set (shape, SHAPE_ATTR_HEIGHT, svg_number_new (params[3]));
      svg_shape_attr_set (shape, SHAPE_ATTR_RX, svg_number_new (params[4]));
      svg_shape_attr_set (shape, SHAPE_ATTR_RY, svg_number_new (params[5]));
      break;
    case SHAPE_POLY_LINE:
    case SHAPE_POLYGON:
      svg_shape_attr_set (shape, SHAPE_ATTR_POINTS, svg_points_new (params, n_params));
      break;
    default:
      g_assert_not_reached ();
    }

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_states (PathPaintable *self,
                                size_t         idx,
                                uint64_t       states)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  if (shape->gpa.states == states)
    return;

  shape->gpa.states = states;

  g_signal_emit (self, signals[CHANGED], 0);
}

gboolean
path_paintable_set_path_id (PathPaintable *self,
                            size_t         idx,
                            const char    *id)
{
#if 0
  for (size_t i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (i != idx &&
          elt->id != NULL && id != NULL &&
          strcmp (elt->id, id) == 0)
        return FALSE;
    }
#endif

  Shape *shape = path_paintable_get_shape (self, idx);

  if (g_set_str (&shape->id, id))
    g_signal_emit (self, signals[CHANGED], 0);

  return TRUE;
}

const char *
path_paintable_get_path_id (PathPaintable *self,
                            size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->id;
}

void
path_paintable_set_path_transition (PathPaintable   *self,
                                    size_t           idx,
                                    GpaTransition    type,
                                    double           duration,
                                    double           delay,
                                    GpaEasing        easing)
{
  g_return_if_fail (duration >= 0);

  Shape *shape = path_paintable_get_shape (self, idx);

  if (shape->gpa.transition == type &&
      shape->gpa.transition_duration == duration * G_TIME_SPAN_MILLISECOND &&
      shape->gpa.transition_delay == delay * G_TIME_SPAN_MILLISECOND &&
      shape->gpa.transition_easing == easing)
    return;

  shape->gpa.transition = type;
  shape->gpa.transition_duration = duration * G_TIME_SPAN_MILLISECOND;
  shape->gpa.transition_delay = delay * G_TIME_SPAN_MILLISECOND;
  shape->gpa.transition_easing = easing;

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_animation (PathPaintable      *self,
                                   size_t              idx,
                                   GpaAnimation        direction,
                                   double              duration,
                                   double              repeat,
                                   GpaEasing           easing,
                                   double              segment)
{
  g_return_if_fail (duration >= 0);

  Shape *shape = path_paintable_get_shape (self, idx);

  if (shape->gpa.animation == direction &&
      shape->gpa.animation_duration == duration * G_TIME_SPAN_MILLISECOND &&
      shape->gpa.animation_repeat == repeat &&
      shape->gpa.animation_easing == easing &&
      shape->gpa.animation_segment == segment)
    return;

  shape->gpa.animation = direction;
  shape->gpa.animation_duration = duration * G_TIME_SPAN_MILLISECOND;
  shape->gpa.animation_repeat = repeat;
  shape->gpa.animation_easing = easing;
  shape->gpa.animation_segment = segment;

  g_signal_emit (self, signals[CHANGED], 0);
}

GpaAnimation
path_paintable_get_path_animation_direction (PathPaintable *self,
                                             size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.animation;
}

double
path_paintable_get_path_animation_duration (PathPaintable *self,
                                            size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.animation_duration / (double) G_TIME_SPAN_MILLISECOND;
}

double
path_paintable_get_path_animation_repeat (PathPaintable *self,
                                          size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.animation_repeat;
}

GpaEasing
path_paintable_get_path_animation_easing (PathPaintable *self,
                                          size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.animation_easing;
}

double
path_paintable_get_path_animation_segment (PathPaintable *self,
                                           size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.animation_segment;
}

void
path_paintable_set_path_origin (PathPaintable *self,
                                size_t         idx,
                                double         origin)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  if (shape->gpa.origin == origin)
    return;

  shape->gpa.origin = origin;

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_fill (PathPaintable   *self,
                              size_t           idx,
                              gboolean         enabled,
                              GskFillRule      rule,
                              unsigned int     symbolic,
                              const GdkRGBA   *color)
{
  Shape *shape = path_paintable_get_shape (self, idx);
  PaintKind kind;
  GtkSymbolicColor fill_symbolic;
  GdkRGBA fill_color;
  GskFillRule fill_rule;

  kind = svg_shape_attr_get_paint (shape, SHAPE_ATTR_FILL, &fill_symbolic, &fill_color);
  fill_rule = svg_shape_attr_get_enum (shape, SHAPE_ATTR_FILL_RULE);

  if (enabled == (kind != PAINT_NONE) &&
      fill_rule == rule &&
      fill_symbolic == symbolic &&
      ((symbolic != 0xffff && fill_color.alpha == color->alpha) ||
       gdk_rgba_equal (&fill_color, color)))
    return;

  svg_shape_attr_set (shape, SHAPE_ATTR_FILL_RULE, svg_fill_rule_new (rule));
  if (!enabled)
    svg_shape_attr_set (shape, SHAPE_ATTR_FILL, svg_paint_new_none ());
  else if (symbolic != 0xffff)
    svg_shape_attr_set (shape, SHAPE_ATTR_FILL, svg_paint_new_symbolic (symbolic));
  else
    svg_shape_attr_set (shape, SHAPE_ATTR_FILL, svg_paint_new_rgba (color));
  /* FIXME opacity */

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_stroke (PathPaintable *self,
                                size_t         idx,
                                gboolean       enabled,
                                GskStroke     *stroke,
                                unsigned int   symbolic,
                                const GdkRGBA *color)
{
  Shape *shape = path_paintable_get_shape (self, idx);
  graphene_size_t *viewport = &self->svg->view_box.size;
  PaintKind kind;
  GtkSymbolicColor stroke_symbolic;
  GdkRGBA stroke_color;
  double width;
  GskLineCap linecap;
  GskLineJoin linejoin;

  kind = svg_shape_attr_get_paint (shape, SHAPE_ATTR_STROKE, &stroke_symbolic, &stroke_color);
  width = svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_WIDTH, viewport);
  linecap = svg_shape_attr_get_enum (shape, SHAPE_ATTR_STROKE_LINEJOIN);
  linejoin = svg_shape_attr_get_enum (shape, SHAPE_ATTR_STROKE_LINEJOIN);

  if (enabled == (kind != PAINT_NONE) &&
      width == gsk_stroke_get_line_width (stroke) &&
      linecap == gsk_stroke_get_line_cap (stroke) &&
      linejoin == gsk_stroke_get_line_join (stroke) &&
      stroke_symbolic == symbolic &&
      ((symbolic != 0xffff && stroke_color.alpha == color->alpha) ||
       gdk_rgba_equal (&stroke_color, color)))
    return;

  if (!enabled)
    svg_shape_attr_set (shape, SHAPE_ATTR_STROKE, svg_paint_new_none ());
  else if (symbolic != 0xffff)
    svg_shape_attr_set (shape, SHAPE_ATTR_STROKE, svg_paint_new_symbolic (symbolic));
  else
    svg_shape_attr_set (shape, SHAPE_ATTR_STROKE, svg_paint_new_rgba (color));
  /* FIXME opacity */
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_WIDTH, svg_number_new (gsk_stroke_get_line_width (stroke)));
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_MINWIDTH, svg_number_new (gsk_stroke_get_line_width (stroke) * 100. / 400.));
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_MAXWIDTH, svg_number_new (gsk_stroke_get_line_width (stroke) * 1000. / 400.));
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_LINECAP, svg_linecap_new (gsk_stroke_get_line_cap (stroke)));
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_LINEJOIN, svg_linejoin_new (gsk_stroke_get_line_join (stroke)));

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_stroke_variation (PathPaintable *self,
                                          size_t         idx,
                                          double         min_width,
                                          double         max_width)
{
  Shape *shape = path_paintable_get_shape (self, idx);
  graphene_size_t *viewport = &self->svg->view_box.size;
  double min, max;

  min = svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_MINWIDTH, viewport);
  max = svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_MAXWIDTH, viewport);

  if (min == min_width && max == max_width)
    return;

  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_MINWIDTH, svg_number_new (min_width));
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_MAXWIDTH, svg_number_new (max_width));

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_get_path_stroke_variation (PathPaintable *self,
                                          size_t         idx,
                                          double        *min_width,
                                          double        *max_width)
{
  Shape *shape = path_paintable_get_shape (self, idx);
  graphene_size_t *viewport = &self->svg->view_box.size;

  *min_width = svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_MINWIDTH, viewport);
  *min_width = svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_MAXWIDTH, viewport);
}

void
path_paintable_attach_path (PathPaintable *self,
                            size_t         idx,
                            size_t         to,
                            double         pos)
{
  Shape *shape = path_paintable_get_shape (self, idx);
  size_t cto = (size_t) -1;
  double cpos = 0;

  path_paintable_get_attach_path (self, idx, &cto, &cpos);
  if (cto == to && cpos == pos)
    return;

  if (to != (size_t) -1)
    {
      Shape *s = path_paintable_get_shape (self, to);
      g_set_str (&shape->gpa.attach.ref, shape->id);
      shape->gpa.attach.shape = s;
      shape->gpa.attach.pos = pos;
    }
  else
    {
      g_clear_pointer (&shape->gpa.attach.ref, g_free);
      shape->gpa.attach.shape = NULL;
      shape->gpa.attach.pos = pos;
    }

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_get_attach_path (PathPaintable *self,
                                size_t         idx,
                                size_t        *to,
                                double        *pos)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  *pos = shape->gpa.attach.pos;

  for (unsigned int i = 0; i < self->svg->content->shapes->len; i++)
    {
      Shape *s = g_ptr_array_index (self->svg->content->shapes, i);
      if (s == shape->gpa.attach.shape)
        {
          *to = i;
          break;
        }
    }
}

void
path_paintable_set_keywords (PathPaintable *self,
                             const char    *keywords)
{
  if (g_set_str (&self->svg->gpa_keywords, keywords))
    g_signal_emit (self, signals[CHANGED], 0);
}

const char *
path_paintable_get_keywords (PathPaintable *self)
{
  return self->svg->gpa_keywords;
}

size_t
path_paintable_get_n_paths (PathPaintable *self)
{
  return self->svg->content->shapes->len;
}

GskPath *
path_paintable_get_path (PathPaintable *self,
                         size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);
  graphene_size_t *viewport = &self->svg->view_box.size;

  if (shape->type == SHAPE_PATH)
    return svg_shape_attr_get_path (shape, SHAPE_ATTR_PATH);
  else
    return svg_shape_get_path (shape, viewport);
}

ShapeType
path_paintable_get_path_shape_type (PathPaintable *self,
                                    size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->type;
}

unsigned int
path_paintable_get_n_shape_params (PathPaintable *self,
                                   size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  switch ((unsigned int) shape->type)
    {
    case SHAPE_RECT: return 6;
    case SHAPE_CIRCLE: return 3;
    case SHAPE_ELLIPSE: return 4;
    case SHAPE_LINE: return 4;
    case SHAPE_PATH: return 0;
    case SHAPE_POLY_LINE:
    case SHAPE_POLYGON:
      {
        unsigned int n;
        svg_shape_attr_get_points (shape, SHAPE_ATTR_POINTS, &n);
        return n;
      }
    default:
      g_assert_not_reached ();
    }
}

void
path_paintable_get_shape_params (PathPaintable *self,
                                 size_t         idx,
                                 double        *params)
{
  Shape *shape = path_paintable_get_shape (self, idx);
  graphene_size_t *viewport = &self->svg->view_box.size;

  switch ((unsigned int) shape->type)
    {
    case SHAPE_RECT:
      params[0] = svg_shape_attr_get_number (shape, SHAPE_ATTR_X, viewport);
      params[1] = svg_shape_attr_get_number (shape, SHAPE_ATTR_Y, viewport);
      params[2] = svg_shape_attr_get_number (shape, SHAPE_ATTR_WIDTH, viewport);
      params[3] = svg_shape_attr_get_number (shape, SHAPE_ATTR_HEIGHT, viewport);
      params[4] = svg_shape_attr_get_number (shape, SHAPE_ATTR_RX, viewport);
      params[5] = svg_shape_attr_get_number (shape, SHAPE_ATTR_RY, viewport);
      break;
    case SHAPE_CIRCLE:
      params[0] = svg_shape_attr_get_number (shape, SHAPE_ATTR_CX, viewport);
      params[1] = svg_shape_attr_get_number (shape, SHAPE_ATTR_CY, viewport);
      params[2] = svg_shape_attr_get_number (shape, SHAPE_ATTR_R, viewport);
      break;
    case SHAPE_ELLIPSE:
      params[0] = svg_shape_attr_get_number (shape, SHAPE_ATTR_CX, viewport);
      params[1] = svg_shape_attr_get_number (shape, SHAPE_ATTR_CY, viewport);
      params[2] = svg_shape_attr_get_number (shape, SHAPE_ATTR_RX, viewport);
      params[3] = svg_shape_attr_get_number (shape, SHAPE_ATTR_RY, viewport);
      break;
    case SHAPE_LINE:
      params[0] = svg_shape_attr_get_number (shape, SHAPE_ATTR_X1, viewport);
      params[1] = svg_shape_attr_get_number (shape, SHAPE_ATTR_Y1, viewport);
      params[2] = svg_shape_attr_get_number (shape, SHAPE_ATTR_X2, viewport);
      params[3] = svg_shape_attr_get_number (shape, SHAPE_ATTR_Y2, viewport);
      break;
    case SHAPE_PATH:
      break;
    case SHAPE_POLY_LINE:
    case SHAPE_POLYGON:
      {
        unsigned int n;
        double *p;
        p = svg_shape_attr_get_points (shape, SHAPE_ATTR_POINTS, &n);
        memcpy (params, p, sizeof (double) * n);
      }
      break;
    default:
      g_assert_not_reached ();
    }

}

uint64_t
path_paintable_get_path_states (PathPaintable *self,
                                size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.states;
}

GpaTransition
path_paintable_get_path_transition_type (PathPaintable *self,
                                         size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.transition;
}

double
path_paintable_get_path_transition_duration (PathPaintable *self,
                                             size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.transition_duration / (double) G_TIME_SPAN_MILLISECOND;
}

double
path_paintable_get_path_transition_delay (PathPaintable *self,
                                          size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.transition_delay / (double) G_TIME_SPAN_MILLISECOND;
}

GpaEasing
path_paintable_get_path_transition_easing (PathPaintable *self,
                                           size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.transition_easing;
}

double
path_paintable_get_path_origin (PathPaintable *self,
                                size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.origin;
}

gboolean
path_paintable_get_path_fill (PathPaintable *self,
                              size_t         idx,
                              GskFillRule   *rule,
                              unsigned int  *symbolic,
                              GdkRGBA       *color)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  *rule = svg_shape_attr_get_enum (shape, SHAPE_ATTR_FILL_RULE);
  return svg_shape_attr_get_paint (shape, SHAPE_ATTR_FILL, symbolic, color);
}

gboolean
path_paintable_get_path_stroke (PathPaintable *self,
                                size_t         idx,
                                GskStroke     *stroke,
                                unsigned int  *symbolic,
                                GdkRGBA       *color)
{
  Shape *shape = path_paintable_get_shape (self, idx);
  graphene_size_t *viewport = &self->svg->view_box.size;

  gsk_stroke_set_line_width (stroke, svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_WIDTH, viewport));
  gsk_stroke_set_line_cap (stroke, svg_shape_attr_get_enum (shape, SHAPE_ATTR_STROKE_LINECAP));
  gsk_stroke_set_line_join (stroke, svg_shape_attr_get_enum (shape, SHAPE_ATTR_STROKE_LINEJOIN));

  return svg_shape_attr_get_paint (shape, SHAPE_ATTR_STROKE, symbolic, color);
}

PathPaintable *
path_paintable_copy (PathPaintable *self)
{
  g_autoptr (GBytes) bytes = NULL;
  PathPaintable *other;

  bytes = path_paintable_serialize (self, self->svg->state);
  other = path_paintable_new_from_bytes (bytes, NULL);

  return other;
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
  PaintKind kind;
  GtkSymbolicColor symbolic;
  GdkRGBA color;

  for (size_t i = 0; i < self->svg->content->shapes->len; i++)
    {
      Shape *shape = g_ptr_array_index (self->svg->content->shapes, i);

      switch (shape->type)
        {
        case SHAPE_PATH:
          compat = MAX (compat, GTK_4_0);
          break;
        case SHAPE_LINE:
        case SHAPE_POLY_LINE:
        case SHAPE_POLYGON:
        case SHAPE_RECT:
        case SHAPE_CIRCLE:
        case SHAPE_ELLIPSE:
          compat = MAX (compat, GTK_4_22);
          break;
        case SHAPE_GROUP:
        case SHAPE_CLIP_PATH:
        case SHAPE_MASK:
        case SHAPE_DEFS:
        case SHAPE_USE:
        case SHAPE_LINEAR_GRADIENT:
        case SHAPE_RADIAL_GRADIENT:
          compat = MAX (compat, GTK_4_22);
          continue;
        default:
          g_assert_not_reached ();
        }

      kind = svg_shape_attr_get_paint (shape, SHAPE_ATTR_STROKE, &symbolic, &color);
      if (kind != PAINT_NONE)
        compat = MAX (compat, GTK_4_20);

      if (shape->gpa.transition != GPA_TRANSITION_NONE ||
          shape->gpa.animation != GPA_ANIMATION_NONE ||
          shape->gpa.attach.ref != NULL)
        compat = MAX (compat, GTK_4_22);
    }

  return compat;
}

/* }}} */
/* {{{ Public API */

void
path_paintable_set_state (PathPaintable *self,
                          unsigned int   state)
{
  if (gtk_svg_get_state (self->svg) == state)
    return;

  gtk_svg_set_state (self->svg, state);

  if (self->render_paintable)
    gtk_svg_set_state (GTK_SVG (self->render_paintable), state);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_STATE]);
}

unsigned int
path_paintable_get_state (PathPaintable *self)
{
  return gtk_svg_get_state (self->svg);
}

void
path_paintable_set_weight (PathPaintable *self,
                           double         weight)
{
  if (gtk_svg_get_weight (self->svg) == weight)
    return;

  gtk_svg_set_weight (self->svg, weight);

  if (self->render_paintable)
    gtk_svg_set_weight (GTK_SVG (self->render_paintable), weight);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WEIGHT]);
}

double
path_paintable_get_weight (PathPaintable *self)
{
  return gtk_svg_get_weight (self->svg);
}

unsigned int
path_paintable_get_n_states (PathPaintable *self)
{
  return gtk_svg_get_n_states (self->svg);
}

gboolean
path_paintable_equal (PathPaintable *self,
                      PathPaintable *other)
{
  return gtk_svg_equal (self->svg, other->svg);
}

PathPaintable *
path_paintable_new_from_bytes (GBytes  *bytes,
                               GError **error)
{
  PathPaintable *paintable = path_paintable_new ();

  if (!parse_symbolic_svg (paintable, bytes, error))
    g_clear_object (&paintable);

  return paintable;
}

PathPaintable *
path_paintable_new_from_resource (const char *resource)
{
  g_autoptr (GBytes) bytes = g_resources_lookup_data (resource, 0, NULL);
  g_autoptr (GError) error = NULL;
  PathPaintable *res;

  if (!bytes)
    g_error ("Resource %s not found", resource);

  res = path_paintable_new_from_bytes (bytes, &error);
  if (!res)
    g_error ("Failed to parse %s: %s", resource, error->message);

  return res;
}

GBytes *
path_paintable_serialize (PathPaintable *self,
                          unsigned int   initial_state)
{
  GBytes *bytes;
  unsigned int state = self->svg->state;

  self->svg->state = initial_state;
  bytes = gtk_svg_serialize_full (self->svg,
                                  NULL, 0,
                                  GTK_SVG_SERIALIZE_INCLUDE_GPA_ATTRS);
  self->svg->state = state;

  return bytes;
}

GBytes *
path_paintable_serialize_as_svg (PathPaintable *self)
{
  return gtk_svg_serialize (self->svg);
}

/* }}} */

/* vim:set foldmethod=marker: */
