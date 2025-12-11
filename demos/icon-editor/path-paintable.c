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
path_paintable_real_changed (PathPaintable *self)
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
                                G_CALLBACK (path_paintable_real_changed),
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
/* {{{ API */

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

Shape *
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
    case SHAPE_POLYLINE:
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

const char *
path_paintable_get_path_id (PathPaintable *self,
                            size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->id;
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
  graphene_rect_t *viewport = &self->svg->viewport;
  PaintKind kind;
  GtkSymbolicColor stroke_symbolic;
  GdkRGBA stroke_color;
  double width;
  GskLineCap linecap;
  GskLineJoin linejoin;
  double miterlimit;

  kind = svg_shape_attr_get_paint (shape, SHAPE_ATTR_STROKE, &stroke_symbolic, &stroke_color);
  width = svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_WIDTH, viewport);
  linecap = svg_shape_attr_get_enum (shape, SHAPE_ATTR_STROKE_LINEJOIN);
  linejoin = svg_shape_attr_get_enum (shape, SHAPE_ATTR_STROKE_LINEJOIN);
  miterlimit = svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_MITERLIMIT, viewport);

  if (enabled == (kind != PAINT_NONE) &&
      width == gsk_stroke_get_line_width (stroke) &&
      linecap == gsk_stroke_get_line_cap (stroke) &&
      linejoin == gsk_stroke_get_line_join (stroke) &&
      miterlimit == gsk_stroke_get_miter_limit (stroke) &&
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
  svg_shape_attr_set (shape, SHAPE_ATTR_STROKE_MITERLIMIT, svg_number_new (gsk_stroke_get_miter_limit (stroke)));

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
  const graphene_rect_t *viewport = &self->svg->viewport;

  return svg_shape_get_path (shape, viewport);
}

uint64_t
path_paintable_get_path_states (PathPaintable *self,
                                size_t         idx)
{
  Shape *shape = path_paintable_get_shape (self, idx);

  return shape->gpa.states;
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
  const graphene_rect_t *viewport = &self->svg->viewport;

  gsk_stroke_set_line_width (stroke, svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_WIDTH, viewport));
  gsk_stroke_set_line_cap (stroke, svg_shape_attr_get_enum (shape, SHAPE_ATTR_STROKE_LINECAP));
  gsk_stroke_set_line_join (stroke, svg_shape_attr_get_enum (shape, SHAPE_ATTR_STROKE_LINEJOIN));
  gsk_stroke_set_miter_limit (stroke, svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_MITERLIMIT, viewport));

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
   * - Anything else
   *
   * This is informational.
   * Icons may still render (in a degraded fashion) with older GTK.
   */
  GtkCompatibility compat = GTK_4_0;
  PaintKind paint_kind;
  GtkSymbolicColor symbolic;
  GdkRGBA color;
  PaintOrder paint_order;
  double opacity;
  double miterlimit;
  ClipKind clip_kind;
  GskPath *clip_path;
  char *str;

  for (size_t i = 0; i < self->svg->content->shapes->len; i++)
    {
      Shape *shape = g_ptr_array_index (self->svg->content->shapes, i);

      switch (shape->type)
        {
        case SHAPE_PATH:
          compat = MAX (compat, GTK_4_0);
          break;
        case SHAPE_LINE:
        case SHAPE_POLYLINE:
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
        case SHAPE_PATTERN:
          compat = MAX (compat, GTK_4_22);
          continue;
        default:
          g_assert_not_reached ();
        }

      paint_kind = svg_shape_attr_get_paint (shape, SHAPE_ATTR_STROKE, &symbolic, &color);
      if (paint_kind != PAINT_NONE)
        compat = MAX (compat, GTK_4_20);

      if (shape->gpa.transition != GPA_TRANSITION_NONE ||
          shape->gpa.animation != GPA_ANIMATION_NONE ||
          shape->gpa.attach.ref != NULL)
        compat = MAX (compat, GTK_4_22);

      paint_order = svg_shape_attr_get_enum (shape, SHAPE_ATTR_PAINT_ORDER);
      if (paint_order != PAINT_ORDER_FILL_STROKE_MARKERS)
        compat = MAX (compat, GTK_4_22);

      opacity = svg_shape_attr_get_number (shape, SHAPE_ATTR_OPACITY, NULL);
      if (opacity != 1)
        compat = MAX (compat, GTK_4_22);

      miterlimit = svg_shape_attr_get_number (shape, SHAPE_ATTR_STROKE_MITERLIMIT, NULL);
      if (miterlimit != 4)
        compat = MAX (compat, GTK_4_22);

      clip_kind = svg_shape_attr_get_clip (shape, SHAPE_ATTR_CLIP_PATH, &clip_path);
      if (clip_kind != CLIP_NONE)
        compat = MAX (compat, GTK_4_22);

      str = svg_shape_attr_get_transform (shape, SHAPE_ATTR_TRANSFORM);
      if (g_strcmp0 (str, "none") != 0)
        compat = MAX (compat, GTK_4_22);
      g_free (str);

      str = svg_shape_attr_get_filter (shape, SHAPE_ATTR_FILTER);
      if (g_strcmp0 (str, "none") != 0)
        compat = MAX (compat, GTK_4_22);
      g_free (str);

      if (compat == GTK_4_22)
        break;
    }

  return compat;
}

GskPath *
path_paintable_get_path_by_id (PathPaintable *self,
                               const char    *id)
{
  const graphene_rect_t *viewport = &self->svg->viewport;

  for (size_t i = 0; i < self->svg->content->shapes->len; i++)
    {
      Shape *shape = g_ptr_array_index (self->svg->content->shapes, i);

      switch (shape->type)
        {
        case SHAPE_PATH:
        case SHAPE_LINE:
        case SHAPE_POLYLINE:
        case SHAPE_POLYGON:
        case SHAPE_RECT:
        case SHAPE_CIRCLE:
        case SHAPE_ELLIPSE:
          if (g_strcmp0 (shape->id, id) == 0)
            return svg_shape_get_path (shape, viewport);
          break;
        case SHAPE_GROUP:
        case SHAPE_CLIP_PATH:
        case SHAPE_MASK:
        case SHAPE_DEFS:
        case SHAPE_USE:
        case SHAPE_LINEAR_GRADIENT:
        case SHAPE_RADIAL_GRADIENT:
        case SHAPE_PATTERN:
          break;
        default:
          g_assert_not_reached ();
        }
    }

  return NULL;
}

Shape *
path_paintable_get_content (PathPaintable *self)
{
  return self->svg->content;
}

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
  bytes = gtk_svg_serialize (self->svg);
  self->svg->state = state;

  return bytes;
}

GBytes *
path_paintable_serialize_as_svg (PathPaintable *self)
{
  return gtk_svg_serialize (self->svg);
}

const graphene_rect_t *
path_paintable_get_viewport (PathPaintable *self)
{
  return &self->svg->viewport;
}

void
path_paintable_changed (PathPaintable *self)
{
  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_paths_changed (PathPaintable *self)
{
  g_signal_emit (self, signals[PATHS_CHANGED], 0);
}

Shape *
shape_duplicate (Shape *shape)
{
  Shape *copy = g_new0 (Shape, 1);

  copy->type = shape->type;
  copy->parent = shape->parent;
  copy->attrs = shape->attrs;
  copy->id = NULL;
  copy->display = shape->display;
  for (unsigned int i = 0; i < N_SHAPE_ATTRS; i++)
    {
      if (shape->base[i])
        copy->base[i] = svg_value_ref (shape->base[i]);
    }

  copy->animations = g_ptr_array_new ();

  copy->gpa.states = shape->gpa.states;
  copy->gpa.transition = shape->gpa.transition;
  copy->gpa.transition_easing = shape->gpa.transition_easing;
  copy->gpa.transition_duration = shape->gpa.transition_duration;
  copy->gpa.transition_delay = shape->gpa.transition_delay;
  copy->gpa.animation = shape->gpa.animation;
  copy->gpa.animation_easing = shape->gpa.animation_easing;
  copy->gpa.animation_duration = shape->gpa.animation_duration;
  copy->gpa.animation_repeat = shape->gpa.animation_repeat;
  copy->gpa.animation_segment = shape->gpa.animation_segment;
  copy->gpa.origin = shape->gpa.origin;
  copy->gpa.attach.ref = NULL;
  copy->gpa.attach.shape = NULL;
  copy->gpa.attach.pos = 0;

  return copy;
}

gboolean
shape_is_graphical (Shape *shape)
{
  switch (shape->type)
    {
    case SHAPE_LINE:
    case SHAPE_POLYLINE:
    case SHAPE_POLYGON:
    case SHAPE_RECT:
    case SHAPE_CIRCLE:
    case SHAPE_ELLIPSE:
    case SHAPE_PATH:
      return TRUE;
    case SHAPE_GROUP:
    case SHAPE_CLIP_PATH:
    case SHAPE_MASK:
    case SHAPE_DEFS:
    case SHAPE_USE:
    case SHAPE_LINEAR_GRADIENT:
    case SHAPE_RADIAL_GRADIENT:
    case SHAPE_PATTERN:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}

static gboolean
shape_is_group (Shape *shape)
{
  switch (shape->type)
    {
    case SHAPE_LINE:
    case SHAPE_POLYLINE:
    case SHAPE_POLYGON:
    case SHAPE_RECT:
    case SHAPE_CIRCLE:
    case SHAPE_ELLIPSE:
    case SHAPE_PATH:
      return FALSE;
    case SHAPE_GROUP:
    case SHAPE_CLIP_PATH:
    case SHAPE_MASK:
    case SHAPE_DEFS:
      return TRUE;
    case SHAPE_USE:
    case SHAPE_LINEAR_GRADIENT:
    case SHAPE_RADIAL_GRADIENT:
    case SHAPE_PATTERN:
      return FALSE;
    default:
      g_assert_not_reached ();
    }
}
static Shape *
get_shape_by_id (Shape      *shape,
                 const char *id)
{
  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      Shape *sh = g_ptr_array_index (shape->shapes, i);

      if (g_strcmp0 (sh->id, id) == 0)
        return sh;
      else if (shape_is_group (sh))
        {
          Shape *sh2 = get_shape_by_id (sh, id);
          if (sh2)
            return sh2;
        }
    }

  return NULL;
}

Shape *
path_paintable_get_shape_by_id (PathPaintable *self,
                                const char    *id)
{
  return get_shape_by_id (self->svg->content, id);
}

/* }}} */

/* vim:set foldmethod=marker: */
