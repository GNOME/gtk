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
#include "gtk/svg/gtksvgvalueprivate.h"
#include "svg/gtksvgnumberprivate.h"
#include "svg/gtksvgnumbersprivate.h"
#include "svg/gtksvgenumprivate.h"
#include "svg/gtksvgpaintprivate.h"
#include "svg/gtksvgpathprivate.h"
#include "svg/gtksvgviewboxprivate.h"
#include "svg/gtksvgelementprivate.h"


#define BIT(n) (G_GUINT64_CONSTANT (1) << (n))

struct _PathPaintable
{
  GObject parent_instance;

  GtkSvg *svg;
  graphene_rect_t viewport;
  GdkPaintable *render_paintable;
  GdkFrameClock *clock;
};

enum
{
  PROP_STATE = 1,
  PROP_WEIGHT,
  PROP_PLAYING,
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

      bytes = gtk_svg_serialize (self->svg);

      self->render_paintable = GDK_PAINTABLE (gtk_svg_new_from_bytes (bytes));
      gtk_svg_set_weight (GTK_SVG (self->render_paintable), gtk_svg_get_weight (self->svg));
      gtk_svg_set_state (GTK_SVG (self->render_paintable), gtk_svg_get_state (self->svg));

      gtk_svg_set_frame_clock (GTK_SVG (self->render_paintable), self->clock);

      g_object_bind_property (self->svg, "playing",
                              self->render_paintable, "playing",
                              G_BINDING_SYNC_CREATE);

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

void
path_paintable_set_svg (PathPaintable *self,
                        GtkSvg        *svg)
{
  gboolean playing = FALSE;

  if (self->svg)
    g_object_get (self->svg, "playing", &playing, NULL);

  g_set_object (&self->svg, svg);

  g_object_set (self->svg, "playing", playing, NULL);

  graphene_rect_init (&self->viewport, 0, 0, svg->width, svg->height);
  g_clear_object (&self->render_paintable);
  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);
}

static gboolean
parse_symbolic_svg (PathPaintable  *paintable,
                    GBytes         *bytes,
                    GError        **error)
{
  g_autoptr (GtkSvg) svg = gtk_svg_new_from_bytes (bytes);

  path_paintable_set_svg (paintable, svg);

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
  self->svg->gpa_version = 1;
  gtk_svg_set_state (self->svg, 0);
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
  g_clear_object (&self->clock);

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

    case PROP_PLAYING:
      g_value_set_boolean (value, path_paintable_get_playing (self));
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

    case PROP_PLAYING:
      path_paintable_set_playing (self, g_value_get_boolean (value));
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

  properties[PROP_PLAYING] =
    g_param_spec_boolean ("playing", NULL, NULL,
                          FALSE,
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

  svg_shape_attr_set (self->svg->content, SVG_PROPERTY_WIDTH, svg_number_new (width));
  svg_shape_attr_set (self->svg->content, SVG_PROPERTY_HEIGHT, svg_number_new (height));
  svg_shape_attr_set (self->svg->content,
                      SVG_PROPERTY_VIEW_BOX,
                      svg_view_box_new (&GRAPHENE_RECT_INIT (0, 0, width, height)));
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

SvgElement *
path_paintable_get_shape (PathPaintable *self,
                          size_t         path)
{
  return (SvgElement *) svg_element_get_child (self->svg->content, path);
}

void
shape_set_default_attrs (SvgElement *shape)
{
  svg_element_set_states (shape, ALL_STATES);

  svg_element_set_gpa_transition (shape, GPA_TRANSITION_NONE, GPA_EASING_LINEAR, 0, 0);
  svg_element_set_gpa_animation (shape, GPA_ANIMATION_NONE, GPA_EASING_LINEAR, 0, REPEAT_FOREVER, 0.2);
  svg_element_set_gpa_origin (shape, 0);

  svg_element_take_base_value (shape, SVG_PROPERTY_FILL, svg_paint_new_none ());
  svg_element_take_base_value (shape, SVG_PROPERTY_STROKE, svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND));
  svg_element_take_base_value (shape, SVG_PROPERTY_STROKE_WIDTH, svg_number_new (2));
  svg_element_take_base_value (shape, SVG_PROPERTY_STROKE_MINWIDTH, svg_number_new (0.5));
  svg_element_take_base_value (shape, SVG_PROPERTY_STROKE_MAXWIDTH, svg_number_new (3));
  svg_element_take_base_value (shape, SVG_PROPERTY_STROKE_LINECAP, svg_linecap_new (GSK_LINE_CAP_ROUND));
  svg_element_take_base_value (shape, SVG_PROPERTY_STROKE_LINEJOIN, svg_linejoin_new (GSK_LINE_JOIN_ROUND));
}

size_t
path_paintable_add_path (PathPaintable *self,
                         GskPath       *path)
{
  SvgElement *shape;

  shape = svg_element_new (self->svg->content, SVG_ELEMENT_PATH);
  svg_element_add_child (self->svg->content, shape);
  shape_set_default_attrs (shape);
  svg_element_take_base_value (shape, SVG_PROPERTY_PATH, svg_path_new (path));

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);

  return svg_element_get_n_children (self->svg->content) - 1;
}

size_t
path_paintable_add_shape (PathPaintable  *self,
                          SvgElementType  type,
                          double         *params,
                          unsigned int    n_params)
{
  SvgElement *shape;

  shape = svg_element_new (self->svg->content, type);
  svg_element_add_child (self->svg->content, shape);

  shape_set_default_attrs (shape);

  switch ((unsigned int) type)
    {
    case SVG_ELEMENT_LINE:
      svg_element_take_base_value (shape, SVG_PROPERTY_X1, svg_number_new (params[0]));
      svg_element_take_base_value (shape, SVG_PROPERTY_Y1, svg_number_new (params[1]));
      svg_element_take_base_value (shape, SVG_PROPERTY_X2, svg_number_new (params[2]));
      svg_element_take_base_value (shape, SVG_PROPERTY_Y2, svg_number_new (params[3]));
      break;
    case SVG_ELEMENT_CIRCLE:
      svg_element_take_base_value (shape, SVG_PROPERTY_CX, svg_number_new (params[0]));
      svg_element_take_base_value (shape, SVG_PROPERTY_CY, svg_number_new (params[1]));
      svg_element_take_base_value (shape, SVG_PROPERTY_R, svg_number_new (params[2]));
      break;
    case SVG_ELEMENT_ELLIPSE:
      svg_element_take_base_value (shape, SVG_PROPERTY_CX, svg_number_new (params[0]));
      svg_element_take_base_value (shape, SVG_PROPERTY_CY, svg_number_new (params[1]));
      svg_element_take_base_value (shape, SVG_PROPERTY_RX, svg_number_new (params[2]));
      svg_element_take_base_value (shape, SVG_PROPERTY_RY, svg_number_new (params[3]));
      break;
    case SVG_ELEMENT_RECT:
      svg_element_take_base_value (shape, SVG_PROPERTY_X, svg_number_new (params[0]));
      svg_element_take_base_value (shape, SVG_PROPERTY_Y, svg_number_new (params[1]));
      svg_element_take_base_value (shape, SVG_PROPERTY_WIDTH, svg_number_new (params[2]));
      svg_element_take_base_value (shape, SVG_PROPERTY_HEIGHT, svg_number_new (params[3]));
      svg_element_take_base_value (shape, SVG_PROPERTY_RX, svg_number_new (params[4]));
      svg_element_take_base_value (shape, SVG_PROPERTY_RY, svg_number_new (params[5]));
      break;
    case SVG_ELEMENT_POLYLINE:
    case SVG_ELEMENT_POLYGON:
      svg_element_take_base_value (shape, SVG_PROPERTY_POINTS, svg_numbers_new (params, n_params));
      break;
    default:
      g_assert_not_reached ();
    }

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);

  return svg_element_get_n_children (self->svg->content) - 1;
}

void
path_paintable_set_path_states (PathPaintable *self,
                                size_t         idx,
                                uint64_t       states)
{
  SvgElement *shape = path_paintable_get_shape (self, idx);

  if (svg_element_get_states (shape) == states)
    return;

  svg_element_set_states (shape, states);

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_states_by_id (PathPaintable *self,
                                      const char    *id,
                                      uint64_t       states)
{
  SvgElement *shape = path_paintable_get_shape_by_id (self, id);

  if (svg_element_get_states (shape) == states)
    return;

  svg_element_set_states (shape, states);

  g_signal_emit (self, signals[CHANGED], 0);
}

const char *
path_paintable_get_path_id (PathPaintable *self,
                            size_t         idx)
{
  SvgElement *shape = path_paintable_get_shape (self, idx);

  return svg_element_get_id (shape);
}

void
path_paintable_set_path_fill (PathPaintable   *self,
                              size_t           idx,
                              gboolean         enabled,
                              GskFillRule      rule,
                              unsigned int     symbolic,
                              const GdkRGBA   *color)
{
  SvgElement *shape = path_paintable_get_shape (self, idx);
  PaintKind kind;
  GtkSymbolicColor fill_symbolic;
  GdkRGBA fill_color;
  GskFillRule fill_rule;

  kind = svg_shape_attr_get_paint (shape, SVG_PROPERTY_FILL, &fill_symbolic, &fill_color);
  fill_rule = svg_shape_attr_get_enum (shape, SVG_PROPERTY_FILL_RULE);

  if (enabled == (kind != PAINT_NONE) &&
      fill_rule == rule &&
      fill_symbolic == symbolic &&
      ((symbolic != 0xffff && fill_color.alpha == color->alpha) ||
       gdk_rgba_equal (&fill_color, color)))
    return;

  svg_shape_attr_set (shape, SVG_PROPERTY_FILL_RULE, svg_fill_rule_new (rule));
  if (!enabled)
    svg_shape_attr_set (shape, SVG_PROPERTY_FILL, svg_paint_new_none ());
  else if (symbolic != 0xffff)
    svg_shape_attr_set (shape, SVG_PROPERTY_FILL, svg_paint_new_symbolic (symbolic));
  else
    svg_shape_attr_set (shape, SVG_PROPERTY_FILL, svg_paint_new_rgba (color));
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
  SvgElement *shape = path_paintable_get_shape (self, idx);
  PaintKind kind;
  GtkSymbolicColor stroke_symbolic;
  GdkRGBA stroke_color;
  double width;
  GskLineCap linecap;
  GskLineJoin linejoin;
  double miterlimit;

  kind = svg_shape_attr_get_paint (shape, SVG_PROPERTY_STROKE, &stroke_symbolic, &stroke_color);
  width = svg_shape_attr_get_number (shape, SVG_PROPERTY_STROKE_WIDTH, &self->viewport);
  linecap = svg_shape_attr_get_enum (shape, SVG_PROPERTY_STROKE_LINEJOIN);
  linejoin = svg_shape_attr_get_enum (shape, SVG_PROPERTY_STROKE_LINEJOIN);
  miterlimit = svg_shape_attr_get_number (shape, SVG_PROPERTY_STROKE_MITERLIMIT, &self->viewport);

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
    svg_shape_attr_set (shape, SVG_PROPERTY_STROKE, svg_paint_new_none ());
  else if (symbolic != 0xffff)
    svg_shape_attr_set (shape, SVG_PROPERTY_STROKE, svg_paint_new_symbolic (symbolic));
  else
    svg_shape_attr_set (shape, SVG_PROPERTY_STROKE, svg_paint_new_rgba (color));
  /* FIXME opacity */
  svg_shape_attr_set (shape, SVG_PROPERTY_STROKE_WIDTH, svg_number_new (gsk_stroke_get_line_width (stroke)));
  svg_shape_attr_set (shape, SVG_PROPERTY_STROKE_MINWIDTH, svg_number_new (gsk_stroke_get_line_width (stroke) * 100. / 400.));
  svg_shape_attr_set (shape, SVG_PROPERTY_STROKE_MAXWIDTH, svg_number_new (gsk_stroke_get_line_width (stroke) * 1000. / 400.));
  svg_shape_attr_set (shape, SVG_PROPERTY_STROKE_LINECAP, svg_linecap_new (gsk_stroke_get_line_cap (stroke)));
  svg_shape_attr_set (shape, SVG_PROPERTY_STROKE_LINEJOIN, svg_linejoin_new (gsk_stroke_get_line_join (stroke)));
  svg_shape_attr_set (shape, SVG_PROPERTY_STROKE_MITERLIMIT, svg_number_new (gsk_stroke_get_miter_limit (stroke)));

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_get_attach_path (PathPaintable *self,
                                size_t         idx,
                                size_t        *to,
                                double        *pos)
{
  SvgElement *shape = path_paintable_get_shape (self, idx);
  SvgElement *sh;

  svg_element_get_gpa_attachment (shape, NULL, pos, &sh);

  for (unsigned int i = 0; i < svg_element_get_n_children (self->svg->content); i++)
    {
      SvgElement *s = svg_element_get_child (self->svg->content, i);
      if (s == sh)
        {
          *to = i;
          break;
        }
    }
}

static SvgElement *
find_attach_shape (SvgElement *s,
                   SvgElement *from)
{
  SvgElement *sh;

  svg_element_get_gpa_attachment (s, NULL, NULL, &sh);
  if (svg_element_get_type (s) == SVG_ELEMENT_SVG ||
      svg_element_get_type (s) == SVG_ELEMENT_GROUP ||
      svg_element_get_type (s) == SVG_ELEMENT_LINK)
    {
      for (unsigned int i = 0; i < svg_element_get_n_children (s); i++)
        {
          SvgElement *s2 = svg_element_get_child (s,i);
          SvgElement *to = find_attach_shape (s2, from);
          if (to != NULL)
            return to;
        }
    }
  else if (sh == from)
    {
      return s;
    }

  return NULL;
}

void
path_paintable_get_attach_path_for_shape (PathPaintable  *self,
                                          SvgElement     *shape,
                                          SvgElement    **to,
                                          double         *pos)
{
  svg_element_get_gpa_attachment (shape, NULL, pos, NULL);
  *to = find_attach_shape (self->svg->content, shape);
}

void
path_paintable_set_keywords (PathPaintable *self,
                             const char    *keywords)
{
  if (g_set_str (&self->svg->keywords, keywords))
    g_signal_emit (self, signals[CHANGED], 0);
}

const char *
path_paintable_get_keywords (PathPaintable *self)
{
  return self->svg->keywords;
}

void
path_paintable_set_description (PathPaintable *self,
                                const char    *description)
{
  if (g_set_str (&self->svg->description, description))
    g_signal_emit (self, signals[CHANGED], 0);
}

const char *
path_paintable_get_description (PathPaintable *self)
{
  return self->svg->description;
}

void
path_paintable_set_author (PathPaintable *self,
                           const char    *author)
{
  if (g_set_str (&self->svg->author, author))
    g_signal_emit (self, signals[CHANGED], 0);
}

const char *
path_paintable_get_author (PathPaintable *self)
{
  return self->svg->author;
}

void
path_paintable_set_license (PathPaintable *self,
                            const char    *license)
{
  if (g_set_str (&self->svg->license, license))
    g_signal_emit (self, signals[CHANGED], 0);
}

const char *
path_paintable_get_license (PathPaintable *self)
{
  return self->svg->license;
}

size_t
path_paintable_get_n_paths (PathPaintable *self)
{
  return svg_element_get_n_children (self->svg->content);
}

GskPath *
path_paintable_get_path (PathPaintable *self,
                         size_t         idx)
{
  SvgElement *shape = path_paintable_get_shape (self, idx);

  return svg_shape_get_path (shape, &self->viewport);
}

uint64_t
path_paintable_get_path_states (PathPaintable *self,
                                size_t         idx)
{
  SvgElement *shape = path_paintable_get_shape (self, idx);

  return svg_element_get_states (shape);
}

double
path_paintable_get_path_origin (PathPaintable *self,
                                size_t         idx)
{
  SvgElement *shape = path_paintable_get_shape (self, idx);

  return svg_element_get_gpa_origin (shape);
}

gboolean
path_paintable_get_path_fill (PathPaintable *self,
                              size_t         idx,
                              GskFillRule   *rule,
                              unsigned int  *symbolic,
                              GdkRGBA       *color)
{
  SvgElement *shape = path_paintable_get_shape (self, idx);

  *rule = svg_shape_attr_get_enum (shape, SVG_PROPERTY_FILL_RULE);
  return svg_shape_attr_get_paint (shape, SVG_PROPERTY_FILL, symbolic, color);
}

gboolean
path_paintable_get_path_stroke (PathPaintable *self,
                                size_t         idx,
                                GskStroke     *stroke,
                                unsigned int  *symbolic,
                                GdkRGBA       *color)
{
  SvgElement *shape = path_paintable_get_shape (self, idx);

  gsk_stroke_set_line_width (stroke, svg_shape_attr_get_number (shape, SVG_PROPERTY_STROKE_WIDTH, &self->viewport));
  gsk_stroke_set_line_cap (stroke, svg_shape_attr_get_enum (shape, SVG_PROPERTY_STROKE_LINECAP));
  gsk_stroke_set_line_join (stroke, svg_shape_attr_get_enum (shape, SVG_PROPERTY_STROKE_LINEJOIN));
  gsk_stroke_set_miter_limit (stroke, svg_shape_attr_get_number (shape, SVG_PROPERTY_STROKE_MITERLIMIT, &self->viewport));

  return svg_shape_attr_get_paint (shape, SVG_PROPERTY_STROKE, symbolic, color);
}

PathPaintable *
path_paintable_copy (PathPaintable *self)
{
  g_autoptr (GBytes) bytes = NULL;
  PathPaintable *other;

  bytes = gtk_svg_serialize (self->svg);
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
  const char *ref;
  char *str;
  GpaTransition transition;
  GpaAnimation animation;

  for (size_t i = 0; i < svg_element_get_n_children (self->svg->content); i++)
    {
      SvgElement *shape = svg_element_get_child (self->svg->content, i);

      switch (svg_element_get_type (shape))
        {
        case SVG_ELEMENT_PATH:
          compat = MAX (compat, GTK_4_0);
          break;
        case SVG_ELEMENT_LINE:
        case SVG_ELEMENT_POLYLINE:
        case SVG_ELEMENT_POLYGON:
        case SVG_ELEMENT_RECT:
        case SVG_ELEMENT_CIRCLE:
        case SVG_ELEMENT_ELLIPSE:
          compat = MAX (compat, GTK_4_22);
          break;
        case SVG_ELEMENT_GROUP:
        case SVG_ELEMENT_CLIP_PATH:
        case SVG_ELEMENT_MASK:
        case SVG_ELEMENT_DEFS:
        case SVG_ELEMENT_USE:
        case SVG_ELEMENT_LINEAR_GRADIENT:
        case SVG_ELEMENT_RADIAL_GRADIENT:
        case SVG_ELEMENT_PATTERN:
        case SVG_ELEMENT_MARKER:
        case SVG_ELEMENT_TEXT:
        case SVG_ELEMENT_TSPAN:
        case SVG_ELEMENT_SVG:
        case SVG_ELEMENT_IMAGE:
        case SVG_ELEMENT_FILTER:
        case SVG_ELEMENT_SYMBOL:
        case SVG_ELEMENT_SWITCH:
        case SVG_ELEMENT_LINK:
          compat = MAX (compat, GTK_4_22);
          continue;
        default:
          g_assert_not_reached ();
        }

      paint_kind = svg_shape_attr_get_paint (shape, SVG_PROPERTY_STROKE, &symbolic, &color);
      if (paint_kind != PAINT_NONE)
        compat = MAX (compat, GTK_4_20);

      svg_element_get_gpa_transition (shape, &transition, NULL, NULL, NULL);
      svg_element_get_gpa_animation (shape, &animation, NULL, NULL, NULL, NULL);
      svg_element_get_gpa_attachment (shape, &ref, NULL, NULL);
      if (transition != GPA_TRANSITION_NONE ||
          animation != GPA_ANIMATION_NONE ||
          ref != NULL)
        compat = MAX (compat, GTK_4_22);

      paint_order = svg_shape_attr_get_enum (shape, SVG_PROPERTY_PAINT_ORDER);
      if (paint_order != PAINT_ORDER_FILL_STROKE_MARKERS)
        compat = MAX (compat, GTK_4_22);

      opacity = svg_shape_attr_get_number (shape, SVG_PROPERTY_OPACITY, NULL);
      if (opacity != 1)
        compat = MAX (compat, GTK_4_22);

      miterlimit = svg_shape_attr_get_number (shape, SVG_PROPERTY_STROKE_MITERLIMIT, NULL);
      if (miterlimit != 4)
        compat = MAX (compat, GTK_4_22);

      clip_kind = svg_shape_attr_get_clip (shape, SVG_PROPERTY_CLIP_PATH, &clip_path, &ref);
      if (clip_kind != CLIP_NONE)
        compat = MAX (compat, GTK_4_22);

      str = svg_shape_attr_get_transform (shape, SVG_PROPERTY_TRANSFORM);
      if (g_strcmp0 (str, "none") != 0)
        compat = MAX (compat, GTK_4_22);
      g_free (str);

      str = svg_shape_attr_get_filter (shape, SVG_PROPERTY_FILTER);
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
  for (size_t i = 0; i < svg_element_get_n_children (self->svg->content); i++)
    {
      SvgElement *shape = svg_element_get_child (self->svg->content, i);

      switch (svg_element_get_type (shape))
        {
        case SVG_ELEMENT_PATH:
        case SVG_ELEMENT_LINE:
        case SVG_ELEMENT_POLYLINE:
        case SVG_ELEMENT_POLYGON:
        case SVG_ELEMENT_RECT:
        case SVG_ELEMENT_CIRCLE:
        case SVG_ELEMENT_ELLIPSE:
          if (g_strcmp0 (svg_element_get_id (shape), id) == 0)
            return svg_shape_get_path (shape, &self->viewport);
          break;
        case SVG_ELEMENT_GROUP:
        case SVG_ELEMENT_CLIP_PATH:
        case SVG_ELEMENT_MASK:
        case SVG_ELEMENT_DEFS:
        case SVG_ELEMENT_USE:
        case SVG_ELEMENT_LINEAR_GRADIENT:
        case SVG_ELEMENT_RADIAL_GRADIENT:
        case SVG_ELEMENT_PATTERN:
        case SVG_ELEMENT_MARKER:
        case SVG_ELEMENT_TEXT:
        case SVG_ELEMENT_TSPAN:
        case SVG_ELEMENT_SVG:
        case SVG_ELEMENT_IMAGE:
        case SVG_ELEMENT_FILTER:
        case SVG_ELEMENT_SYMBOL:
        case SVG_ELEMENT_SWITCH:
        case SVG_ELEMENT_LINK:
          break;
        default:
          g_assert_not_reached ();
        }
    }

  return NULL;
}

SvgElement *
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

static unsigned int
shape_get_max_state (SvgElement *shape)
{
  if (svg_element_get_type (shape) == SVG_ELEMENT_GROUP ||
      svg_element_get_type (shape) == SVG_ELEMENT_LINK ||
      svg_element_get_type (shape) == SVG_ELEMENT_SVG)
    {
      unsigned int state = 0;

      for (unsigned int i = 0; i < svg_element_get_n_children (shape); i++)
        {
          SvgElement *sh = svg_element_get_child (shape, i);
          state = MAX (state, shape_get_max_state (sh));
        }

      return state;
    }
  else if (svg_element_get_states (shape) == 0)
    {
      return 0;
    }
  else if (svg_element_get_states (shape) == ALL_STATES)
    {
      return 63;
    }
  else
    {
      return g_bit_nth_msf (svg_element_get_states (shape), -1);
    }
}

unsigned int
path_paintable_get_max_state (PathPaintable *self)
{
  unsigned int state;
  unsigned int n_names;

  state = shape_get_max_state (self->svg->content);

  gtk_svg_get_state_names (self->svg, &n_names);
  if (n_names > 0)
    return MAX (state, n_names - 1);

  return state;
}

const char **
path_paintable_get_state_names (PathPaintable *self,
                                unsigned int  *length)
{
  return gtk_svg_get_state_names (self->svg, length);
}

gboolean
path_paintable_set_state_names (PathPaintable  *self,
                                const char    **names)
{
  g_signal_emit (self, signals[CHANGED], 0);

  return gtk_svg_set_state_names (self->svg, names);
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
    g_error ("Failed to parse %s: %s", resource, error ? error->message : "");

  return res;
}

const graphene_rect_t *
path_paintable_get_viewport (PathPaintable *self)
{
  return &self->viewport;
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

gboolean
shape_is_graphical (SvgElement *shape)
{
  return svg_element_type_is_path (svg_element_get_type (shape)) ||
         svg_element_type_is_text (svg_element_get_type (shape));
}

gboolean
shape_has_children (SvgElement *shape)
{
  return svg_element_type_is_container (svg_element_get_type (shape));
}

gboolean
shape_has_gpa (SvgElement *shape)
{
  return svg_element_type_is_path (svg_element_get_type (shape));
}

gboolean
shape_has_ancestor (SvgElement *shape,
                    SvgElement *ancestor)
{
  for (SvgElement *p = svg_element_get_parent (shape); p; p = svg_element_get_parent (p))
    if (p == ancestor)
      return TRUE;

  return FALSE;
}

GdkPaintable *
shape_get_path_image (SvgElement *shape,
                      GtkSvg     *orig)
{
  GtkSvg *svg = gtk_svg_new ();
  g_autoptr (GBytes) bytes = NULL;
  SvgValue *value;

  svg->width = orig->width;
  svg->height = orig->width;

  value = svg_element_get_base_value (orig->content, SVG_PROPERTY_WIDTH);
  svg_element_set_base_value (svg->content, SVG_PROPERTY_WIDTH, value);

  value = svg_element_get_base_value (orig->content, SVG_PROPERTY_HEIGHT);
  svg_element_set_base_value (svg->content, SVG_PROPERTY_HEIGHT, value);

  value = svg_element_get_base_value (orig->content, SVG_PROPERTY_VIEW_BOX);
  svg_element_set_base_value (svg->content, SVG_PROPERTY_VIEW_BOX, value);

  if (shape_is_graphical (shape))
    {
      SvgElement *clone = svg_element_duplicate (shape, svg->content);
      svg_element_set_base_value (clone, SVG_PROPERTY_VISIBILITY, NULL);
      svg_element_set_base_value (clone, SVG_PROPERTY_DISPLAY, NULL);
      svg_element_add_child (svg->content, clone);
    }
  bytes = gtk_svg_serialize (svg);
  g_object_unref (svg);
  svg = gtk_svg_new_from_bytes (bytes);
  gtk_svg_play (svg);

  return GDK_PAINTABLE (svg);
}

static SvgElement *
get_shape_by_id (SvgElement      *shape,
                 const char *id)
{
  for (unsigned int i = 0; i < svg_element_get_n_children (shape); i++)
    {
      SvgElement *sh = svg_element_get_child (shape, i);

      if (g_strcmp0 (svg_element_get_id (sh), id) == 0)
        return sh;
      else if (shape_has_children (sh))
        {
          SvgElement *sh2 = get_shape_by_id (sh, id);
          if (sh2)
            return sh2;
        }
    }

  return NULL;
}

SvgElement *
path_paintable_get_shape_by_id (PathPaintable *self,
                                const char    *id)
{
  return get_shape_by_id (self->svg->content, id);
}

static unsigned int
shape_count (SvgElement *shape)
{
  SvgElementType type = svg_element_get_type (shape);

  if (type == SVG_ELEMENT_SVG ||
      type == SVG_ELEMENT_GROUP ||
      type == SVG_ELEMENT_LINK)
    {
      unsigned int count = 0;

      for (unsigned int i = 0; i < svg_element_get_n_children (shape); i++)
        {
          SvgElement *sh = svg_element_get_child (shape, i);

          count += shape_count (sh);
        }

      return count;
    }

  return 1;
}

unsigned int
path_paintable_get_shape_count (PathPaintable *self)
{
  return shape_count (self->svg->content);
}

static void
clear_tempfile (gpointer data)
{
  GFile *file = data;

  g_file_delete (file, NULL, NULL);
  g_object_unref (file);
}

GtkIconPaintable *
path_paintable_get_icon_paintable (PathPaintable *self)
{
  GtkIconPaintable *paintable;
  GFile *file;
  GIOStream *iostream;
  GOutputStream *ostream;
  GInputStream *istream;
  GBytes *bytes;
  GError *error = NULL;

  file = g_file_new_tmp ("gtkXXXXXX-symbolic.svg", (GFileIOStream **) &iostream, &error);
  if (error)
    {
      g_warning ("%s", error->message);
      g_error_free (error);
      return NULL;
    }

  ostream = g_io_stream_get_output_stream (iostream);
  bytes = gtk_svg_serialize (self->svg);
  istream = g_memory_input_stream_new_from_bytes (bytes);

  g_output_stream_splice (ostream, istream, 0, NULL, &error);
  if (error)
    {
      g_object_unref (file);
      g_object_unref (iostream);
      g_bytes_unref (bytes);
      g_warning ("%s", error->message);
      g_error_free (error);
      return NULL;
    }

  paintable = gtk_icon_paintable_new_for_file (file, 64, 1);

  g_object_set_data_full (G_OBJECT (paintable), "file", file, clear_tempfile);

  g_object_unref (iostream);
  g_bytes_unref (bytes);

  return paintable;
}

void
path_paintable_set_playing (PathPaintable *self,
                            gboolean       playing)
{
  if (self->svg->playing == playing)
    return;

  g_object_set (self->svg, "playing", playing, NULL);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_PLAYING]);
}

gboolean
path_paintable_get_playing (PathPaintable *self)
{
  return self->svg->playing;
}

void
path_paintable_set_frame_clock (PathPaintable *self,
                                GdkFrameClock *clock)
{
  g_set_object (&self->clock, clock);

  if (self->render_paintable)
    gtk_svg_set_frame_clock (GTK_SVG (self->render_paintable), clock);
}

GtkSvg *
path_paintable_get_svg (PathPaintable *self)
{
  return self->svg;
}

char *
path_paintable_find_unused_id (PathPaintable *self,
                               const char    *prefix)
{
  for (unsigned int i = 1; i < 256; i++)
    {
      char id[64];
      g_snprintf (id, sizeof (id), "%s%u", prefix, i);
      if (path_paintable_get_shape_by_id (self, id) == NULL)
        return g_strdup (id);
    }

  return NULL;
}

/* }}} */
/* vim:set foldmethod=marker: */
