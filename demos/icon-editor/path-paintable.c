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

typedef struct
{
  GskPath *path;

  ShapeType shape_type;
  union {
    struct {
      float x1, y1, x2, y2;
    } line;
    struct {
      float cx, cy, r;
    } circle;
    struct {
      float cx, cy, rx, ry;
    } ellipse;
    struct {
      float x, y, width, height, rx, ry;
    } rect;
    float shape_params[6];
    struct {
      float *params;
      unsigned int n_params;
    } polyline;
  };

  char *id;
  uint64_t states;

  struct {
    GpaTransition type;
    float duration;
    float delay;
    GpaEasing easing;
    float origin;
  } transition;

  struct {
    GpaAnimation direction;
    float duration;
    float repeat;
    float segment;
    GpaEasing easing;
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

} PathElt;

struct _PathPaintable
{
  GObject parent_instance;
  GArray *paths;

  double width, height;

  unsigned int state;
  float weight;

  char *keywords;
  GdkPaintable *render_paintable;
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
static unsigned int signals[LAST_SIGNAL];

/* {{{ Helpers */

static void
clear_path_elt (gpointer data)
{
  PathElt *elt = data;

  gsk_path_unref (elt->path);
  g_free (elt->id);

  if (elt->shape_type == SHAPE_POLY_LINE ||
      elt->shape_type == SHAPE_POLYGON)
    g_free (elt->polyline.params);
}

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

      bytes = path_paintable_serialize (self, self->state);

      self->render_paintable = GDK_PAINTABLE (gtk_svg_new_from_bytes (bytes));
      gtk_svg_set_weight (GTK_SVG (self->render_paintable), self->weight);
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

  if (elt1->animation.direction != elt2->animation.direction ||
      elt1->animation.duration != elt2->animation.duration ||
      elt1->animation.repeat != elt2->animation.repeat ||
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
/* {{{ Parser */

static GskPath *
line_path_new (float x1, float y1,
               float x2, float y2)
{
  GskPathBuilder *builder = gsk_path_builder_new ();
  gsk_path_builder_move_to (builder, x1, y1);
  gsk_path_builder_line_to (builder, x2, y2);
  return gsk_path_builder_free_to_path (builder);
}

static GskPath *
circle_path_new (float cx, float cy,
                 float radius)
{
  GskPathBuilder *builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (cx, cy), radius);
  return gsk_path_builder_free_to_path (builder);
}

static GskPath *
ellipse_path_new (float cx, float cy,
                  float rx, float ry)
{
  GskPathBuilder *builder = gsk_path_builder_new ();
  gsk_path_builder_move_to  (builder, cx + rx, cy);
  gsk_path_builder_conic_to (builder, cx + rx, cy + ry,
                                      cx,      cy + ry, M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx - rx, cy + ry,
                                      cy - rx, cy,      M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx - rx, cy - ry,
                                      cx,      cy - ry, M_SQRT1_2);
  gsk_path_builder_conic_to (builder, cx + rx, cy - ry,
                                      cx + rx, cy,      M_SQRT1_2);
  gsk_path_builder_close    (builder);
  return gsk_path_builder_free_to_path (builder);
}

static GskPath *
rect_path_new (float x, float y,
               float w, float h,
               float rx, float ry)
{
  GskPathBuilder *builder = gsk_path_builder_new ();
  if (rx == 0 && ry == 0)
    gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (x, y, w, h));
  else
    gsk_path_builder_add_rounded_rect (builder,
                                       &(GskRoundedRect) { .bounds = GRAPHENE_RECT_INIT (x, y, w, h),
                                                           .corner = {
                                                             GRAPHENE_SIZE_INIT (rx, ry),
                                                             GRAPHENE_SIZE_INIT (rx, ry),
                                                             GRAPHENE_SIZE_INIT (rx, ry),
                                                             GRAPHENE_SIZE_INIT (rx, ry)
                                                           }
                                                         });
  return gsk_path_builder_free_to_path (builder);
}

static GskPath *
polyline_path_new (float        *params,
                   unsigned int  n_params,
                   gboolean      close)
{
  GskPathBuilder *builder = gsk_path_builder_new ();

  if (n_params > 1)
    {
      gsk_path_builder_move_to (builder, params[0], params[1]);
      for (unsigned int i = 2; i + 1 < n_params; i += 2)
        gsk_path_builder_line_to (builder, params[i], params[i + 1]);
      if (close)
        gsk_path_builder_close (builder);
    }

  return gsk_path_builder_free_to_path (builder);
}

static gboolean
extract_shapes (GtkSvg         *svg,
                PathPaintable  *paintable,
                GError        **error)
{
  const graphene_size_t *viewport = &svg->view_box.size;
  const char *shape_name[] = {
    "line", "polyline", "polygon", "rect", "circle", "ellipse",
    "path", "group", "clipPath", "mask", "defs", "use",
    "linearGradient", "radialGradient"
  };

  for (unsigned int i = 0; i < svg->content->shapes->len; i++)
    {
      Shape *shape = g_ptr_array_index (svg->content->shapes, i);
      size_t idx;
      PaintKind paint;
      GskFillRule fill_rule;
      GtkSymbolicColor symbolic;
      GdkRGBA color;
      GskStroke *stroke;

      switch (shape->type)
        {
        case SHAPE_DEFS:
          continue;

        case SHAPE_GROUP:
        case SHAPE_CLIP_PATH:
        case SHAPE_MASK:
        case SHAPE_USE:
        case SHAPE_LINEAR_GRADIENT:
        case SHAPE_RADIAL_GRADIENT:
          g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                       "Unsupported shape: %s", shape_name[shape->type]);
          return FALSE;

        case SHAPE_POLY_LINE:
        case SHAPE_POLYGON:
          {
            g_autoptr (GskPath) path = NULL;
            unsigned int n_params;
            double *parms = gtk_svg_attr_get_points (shape, SHAPE_ATTR_POINTS, &n_params);
            float *params;

            params = g_newa (float, n_params);
            for (unsigned int j = 0; j < n_params; j++)
              params[j] = parms[j];

            path = polyline_path_new (params, n_params, shape->type == SHAPE_POLYGON);
            idx = path_paintable_add_path (paintable, NULL, shape->type, params, n_params);
          }
          break;

        case SHAPE_LINE:
          {
            g_autoptr (GskPath) path = NULL;
            float x1, y1, x2, y2;
            x1 = gtk_svg_attr_get_number (shape, SHAPE_ATTR_X1, viewport);
            y1 = gtk_svg_attr_get_number (shape, SHAPE_ATTR_Y1, viewport);
            x2 = gtk_svg_attr_get_number (shape, SHAPE_ATTR_X2, viewport);
            y2 = gtk_svg_attr_get_number (shape, SHAPE_ATTR_Y2, viewport);
            path = line_path_new (x1, y1, x2, y2);
            idx = path_paintable_add_path (paintable, path, SHAPE_LINE,
                                           (float *) &(float[]) { x1, y1, x2, y2 }, 4);
          }
          break;
        case SHAPE_CIRCLE:
          {
            g_autoptr (GskPath) path = NULL;
            float cx, cy, r;
            cx = gtk_svg_attr_get_number (shape, SHAPE_ATTR_CX, viewport);
            cy = gtk_svg_attr_get_number (shape, SHAPE_ATTR_CY, viewport);
            r = gtk_svg_attr_get_number (shape, SHAPE_ATTR_R, viewport);
            path = circle_path_new (cx, cy, r);
            idx = path_paintable_add_path (paintable, path, SHAPE_CIRCLE,
                                           (float *) &(float[]) { cx, cy, r }, 3);
          }
          break;
        case SHAPE_ELLIPSE:
          {
            g_autoptr (GskPath) path = NULL;
            float cx, cy, rx, ry;
            cx = gtk_svg_attr_get_number (shape, SHAPE_ATTR_CX, viewport);
            cy = gtk_svg_attr_get_number (shape, SHAPE_ATTR_CY, viewport);
            rx = gtk_svg_attr_get_number (shape, SHAPE_ATTR_RX, viewport);
            ry = gtk_svg_attr_get_number (shape, SHAPE_ATTR_RY, viewport);
            path = ellipse_path_new (cx, cy, rx, ry);
            idx = path_paintable_add_path (paintable, path, SHAPE_ELLIPSE,
                                           (float *) &(float[]) { cx, cy, rx, ry }, 4);
          }
          break;
        case SHAPE_RECT:
          {
            g_autoptr (GskPath) path = NULL;
            float x, y, width, height, rx, ry;
            x = gtk_svg_attr_get_number (shape, SHAPE_ATTR_X, viewport);
            y = gtk_svg_attr_get_number (shape, SHAPE_ATTR_Y, viewport);
            width = gtk_svg_attr_get_number (shape, SHAPE_ATTR_WIDTH, viewport);
            height = gtk_svg_attr_get_number (shape, SHAPE_ATTR_HEIGHT, viewport);
            rx = gtk_svg_attr_get_number (shape, SHAPE_ATTR_RX, viewport);
            ry = gtk_svg_attr_get_number (shape, SHAPE_ATTR_RY, viewport);
            path = rect_path_new (x, y, width, height, rx, ry);
            idx = path_paintable_add_path (paintable, path, SHAPE_RECT,
                                           (float *) &(float[]) { x, y, width, height, rx, ry }, 6);
          }
          break;
        case SHAPE_PATH:
          {
            g_autoptr (GskPath) path = NULL;
            path = gtk_svg_attr_get_path (shape, SHAPE_ATTR_PATH);
            idx = path_paintable_add_path (paintable, path, SHAPE_PATH,
                                           (float *) &(float[]) { 0, 0, 0, 0, 0, 0 }, 0);
          }
          break;
        default:
          g_assert_not_reached ();
        }

      path_paintable_set_path_id (paintable, idx, shape->id);
      path_paintable_set_path_states (paintable, idx, shape->gpa.states);

      color = (GdkRGBA) { 0, 0, 0, 1 };
      fill_rule = gtk_svg_attr_get_enum (shape, SHAPE_ATTR_FILL_RULE);
      paint = gtk_svg_attr_get_paint (shape, SHAPE_ATTR_FILL, &symbolic, &color);
      color.alpha *= gtk_svg_attr_get_number (shape, SHAPE_ATTR_FILL_OPACITY, NULL);

      path_paintable_set_path_fill (paintable, idx, paint != PAINT_NONE, fill_rule, symbolic, &color);

      stroke = gsk_stroke_new (1);

      gsk_stroke_set_line_width (stroke,
                                 gtk_svg_attr_get_number (shape, SHAPE_ATTR_STROKE_WIDTH, NULL));
      gsk_stroke_set_line_join (stroke,
                                gtk_svg_attr_get_enum (shape, SHAPE_ATTR_STROKE_LINEJOIN));
      gsk_stroke_set_line_cap (stroke,
                               gtk_svg_attr_get_enum (shape, SHAPE_ATTR_STROKE_LINECAP));
      gsk_stroke_set_miter_limit (stroke,
                                  gtk_svg_attr_get_number (shape, SHAPE_ATTR_STROKE_MITERLIMIT, NULL));

      color = (GdkRGBA) { 0, 0, 0, 1 };
      paint = gtk_svg_attr_get_paint (shape, SHAPE_ATTR_STROKE, &symbolic, &color);
      color.alpha *= gtk_svg_attr_get_number (shape, SHAPE_ATTR_STROKE_OPACITY, NULL);
      path_paintable_set_path_stroke (paintable, idx, paint != PAINT_NONE, stroke, symbolic, &color);
      gsk_stroke_free (stroke);

      path_paintable_set_path_stroke_variation (paintable, idx,
                                                gtk_svg_attr_get_number (shape, SHAPE_ATTR_STROKE_MINWIDTH, NULL),
                                                gtk_svg_attr_get_number (shape, SHAPE_ATTR_STROKE_MAXWIDTH, NULL));

      path_paintable_set_path_animation (paintable, idx,
                                         shape->gpa.animation,
                                         shape->gpa.animation_duration / (double) G_TIME_SPAN_MILLISECOND,
                                         shape->gpa.animation_repeat,
                                         shape->gpa.animation_easing,
                                         shape->gpa.animation_segment);

      path_paintable_set_path_transition (paintable, idx,
                                          shape->gpa.transition,
                                          shape->gpa.transition_duration / (double) G_TIME_SPAN_MILLISECOND,
                                          shape->gpa.transition_delay / (double) G_TIME_SPAN_MILLISECOND,
                                          shape->gpa.transition_easing);

      path_paintable_set_path_origin (paintable, idx, shape->gpa.origin);

      if (shape->gpa.attach.shape)
        {
          unsigned int to;
          g_assert (shape->gpa.attach.shape->parent == svg->content);
          g_ptr_array_find (svg->content->shapes, shape->gpa.attach.shape, &to);
          path_paintable_attach_path (paintable, idx, to, shape->gpa.attach.pos);
        }
    }

  return TRUE;
}

static gboolean
parse_symbolic_svg (PathPaintable  *paintable,
                    GBytes         *bytes,
                    GError        **error)
{
  g_autoptr (GtkSvg) svg = gtk_svg_new_from_bytes (bytes);

  path_paintable_set_size (paintable, svg->width, svg->height);
  path_paintable_set_state (paintable, svg->state);
  path_paintable_set_keywords (paintable, svg->gpa_keywords);

  return extract_shapes (svg, paintable, error);
}

/* }}} */
/* {{{ Serialization */

static char *
states_to_string (uint64_t states)
{
  if (states == ALL_STATES)
    {
      return g_strdup ("all");
    }
  else if (states == NO_STATES)
    {
      return g_strdup ("none");
    }
  else
    {
      GString *str = g_string_new ("");

      for (unsigned int u = 0; u < 64; u++)
        {
          if ((states & (G_GUINT64_CONSTANT (1) << u)) != 0)
            {
              if (str->len > 0)
                g_string_append_c (str, ' ');
              g_string_append_printf (str, "%u", u);
            }
        }
      return g_string_free (str, FALSE);
    }
}

static void
path_paintable_save_path (PathPaintable *self,
                          size_t         idx,
                          unsigned int   initial_state,
                          GString       *str)
{
  const char *sym[] = { "foreground", "error", "warning", "success", "accent" };
  const char *easing[] = { "linear", "ease-in-out", "ease-in", "ease-out", "ease", "custom" };
  const char *fallback_color[] = { "rgb(0,0,0)", "rgb(255,0,0)", "rgb(255,255,0)", "rgb(0,255,0)", "rgb(0,0,255)", };
  GskStroke *stroke;
  unsigned int stroke_symbolic;
  unsigned int fill_symbolic;
  gboolean stroke_enabled;
  gboolean fill_enabled;
  GdkRGBA color;
  GskFillRule fill_rule;
  uint64_t states;
  size_t to;
  float pos;
  GStrvBuilder *class_builder;
  GStrv class_strv;
  char *class_str;
  gboolean has_gtk_attr = FALSE;
  char buffer[G_ASCII_DTOSTR_BUF_SIZE];
  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  stroke = gsk_stroke_new (1);

  if (elt->shape_type == SHAPE_LINE)
    {
      g_string_append (str,  "  <line");
      g_string_append_printf (str, " x1='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->line.x1));
      g_string_append_printf (str, " y1='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->line.y1));
      g_string_append_printf (str, " x2='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->line.x2));
      g_string_append_printf (str, " y2='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->line.y2));
    }
  else if (elt->shape_type == SHAPE_CIRCLE)
    {
      g_string_append (str,  "  <circle");
      g_string_append_printf (str, " cx='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->circle.cx));
      g_string_append_printf (str, " cy='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->circle.cy));
      g_string_append_printf (str, " r='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->circle.r));
    }
  else if (elt->shape_type == SHAPE_ELLIPSE)
    {
      g_string_append (str,  "  <ellipse");
      g_string_append_printf (str, " cx='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->ellipse.cx));
      g_string_append_printf (str, " cy='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->ellipse.cy));
      g_string_append_printf (str, " rx='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->ellipse.rx));
      g_string_append_printf (str, " ry='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->ellipse.ry));
    }
  else if (elt->shape_type == SHAPE_RECT)
    {
      g_string_append (str,  "  <rect");
      g_string_append_printf (str, " x='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->rect.x));
      g_string_append_printf (str, " y='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->rect.y));
      g_string_append_printf (str, " width='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->rect.width));
      g_string_append_printf (str, " height='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->rect.height));
      if (elt->rect.rx != 0 || elt->rect.ry != 0)
        {
          g_string_append_printf (str, " rx='%s'",
                                  g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->rect.rx));
          g_string_append_printf (str, " ry='%s'",
                                  g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->rect.ry));
        }
    }
  else if (elt->shape_type == SHAPE_POLY_LINE ||
           elt->shape_type == SHAPE_POLYGON)
    {
      if (elt->shape_type == SHAPE_POLY_LINE)
        g_string_append (str, "  <polyline points='");
      else
        g_string_append (str, "  <polygon points='");

      if (elt->polyline.n_params == 0)
        g_string_append (str, "none");
      else
        for (unsigned int i = 0; i < elt->polyline.n_params; i++)
          {
            if (i > 0)
              g_string_append_c (str, ' ');
            g_string_append (str, g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->polyline.params[i]));
          }

      g_string_append_c (str, '\'');
    }
  else if (elt->shape_type == SHAPE_PATH)
    {
      g_string_append (str, "  <path d='");
      gsk_path_print (path_paintable_get_path (self, idx), str);
      g_string_append (str, "'");
    }
  else
    g_assert_not_reached ();

  if (elt->id)
    g_string_append_printf (str, "\n        id='%s'", elt->id);
  class_builder = g_strv_builder_new ();

  states = path_paintable_get_path_states (self, idx);
  if (states != ALL_STATES)
    {
      g_autofree char *s = states_to_string (states);
      g_string_append_printf (str, "\n        gpa:states='%s'", s);
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_animation_direction (self, idx) != GPA_ANIMATION_NONE)
    {
      const char *direction[] = { "none", "normal", "alternate", "reverse", "reverse-alternate", "in-out", "in-out-alternate", "in-out-reverse", "segment", "segment-alternate" };

      g_string_append (str, "\n        gpa:animation-type='automatic'");
      has_gtk_attr = TRUE;

      g_string_append_printf (str, "\n        gpa:animation-direction='%s'", direction[path_paintable_get_path_animation_direction (self, idx)]);
    }

  if (path_paintable_get_path_animation_duration (self, idx) != 0)
    {
      g_string_append_printf (str, "\n        gpa:animation-duration='%sms'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g",
                                               path_paintable_get_path_animation_duration (self, idx)));
      has_gtk_attr = TRUE;
    }

  if (isfinite (path_paintable_get_path_animation_repeat (self, idx)) == 1)
    {
      g_string_append_printf (str, "\n        gpa:animation-repeat='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g",
                                               path_paintable_get_path_animation_repeat (self, idx)));
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_animation_easing (self, idx) != GPA_EASING_LINEAR)
    {
      g_string_append_printf (str, "\n        gpa:animation-easing='%s'",
                              easing[path_paintable_get_path_animation_easing (self, idx)]);
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_animation_segment (self, idx) != 0.2f)
    {
      g_string_append_printf (str, "\n        gpa:animation-segment='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g",
                                               path_paintable_get_path_animation_segment (self, idx)));
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_transition_type (self, idx) != GPA_TRANSITION_NONE)
    {
      const char *transition[] = { "none", "animate", "morph", "fade" };

      g_string_append_printf (str, "\n        gpa:transition-type='%s'", transition[path_paintable_get_path_transition_type (self, idx)]);
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_transition_duration (self, idx) != 0)
    {
      g_string_append_printf (str, "\n        gpa:transition-duration='%sms'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g",
                                               path_paintable_get_path_transition_duration (self, idx)));
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_transition_delay (self, idx) != 0)
    {
      g_string_append_printf (str, "\n        gpa:transition-delay='%sms'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g",
                                               path_paintable_get_path_transition_delay (self, idx)));
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_transition_easing (self, idx) != GPA_EASING_LINEAR)
    {
      g_string_append_printf (str, "\n        gpa:transition-easing='%s'",
                              easing[path_paintable_get_path_transition_easing (self, idx)]);
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_origin (self, idx) != 0)
    {
      g_string_append_printf (str, "\n        gpa:origin='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g",
                                               path_paintable_get_path_origin (self, idx)));
      has_gtk_attr = TRUE;
    }

  to = (size_t) -1;
  pos = 0;

  path_paintable_get_attach_path (self, idx, &to, &pos);
  if (to != (size_t) -1)
    {
      const char *id;

      id = path_paintable_get_path_id (self, to);

      if (id)
        g_string_append_printf (str, "\n        gpa:attach-to='%s'", id);

      g_string_append_printf (str, "\n        gpa:attach-pos='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", pos));
      has_gtk_attr = TRUE;
    }

  if ((stroke_enabled = path_paintable_get_path_stroke (self, idx, stroke, &stroke_symbolic, &color)))
    {
      const char *linecap[] = { "butt", "round", "square" };
      const char *linejoin[] = { "miter", "round", "bevel" };
      float width, min_width, max_width;

      width = gsk_stroke_get_line_width (stroke);

      g_string_append_printf (str, "\n        stroke-width='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", width));
      g_string_append_printf (str, "\n        stroke-linecap='%s'", linecap[gsk_stroke_get_line_cap (stroke)]);
      g_string_append_printf (str, "\n        stroke-linejoin='%s'", linejoin[gsk_stroke_get_line_join (stroke)]);

      if (stroke_symbolic == 0xffff)
        {
          g_autofree char *s = gdk_rgba_to_string (&color);
          g_string_append_printf (str, "\n        stroke='%s'", s);
          g_string_append_printf (str, "\n        gpa:stroke='%s'", s);
          has_gtk_attr = TRUE;
        }
      else if (stroke_symbolic <= GTK_SYMBOLIC_COLOR_ACCENT)
        {
          if (color.alpha < 1)
            g_string_append_printf (str, "\n        stroke-opacity='%s'",
                                    g_ascii_formatd (buffer, sizeof (buffer), "%g", color.alpha));
          g_string_append_printf (str, "\n        stroke='%s'", fallback_color[stroke_symbolic]);
          if (stroke_symbolic < GTK_SYMBOLIC_COLOR_ACCENT)
            g_strv_builder_take (class_builder, g_strdup_printf ("%s-stroke", sym[stroke_symbolic]));
          else
            has_gtk_attr = TRUE;
        }

      min_width = width * 0.25;
      max_width = width * 1.5;

      path_paintable_get_path_stroke_variation (self, idx, &min_width, &max_width);
      if (min_width != width * 0.25 || max_width != width * 1.5)
        {
          char buffer2[G_ASCII_DTOSTR_BUF_SIZE];
          char buffer3[G_ASCII_DTOSTR_BUF_SIZE];

          g_string_append_printf (str, "\n        gpa:stroke-width='%s %s %s'",
                                  g_ascii_formatd (buffer, sizeof (buffer), "%g", min_width),
                                  g_ascii_formatd (buffer2, sizeof (buffer2), "%g", width),
                                  g_ascii_formatd (buffer3, sizeof (buffer3), "%g", max_width));
          has_gtk_attr = TRUE;
        }
    }
  else
    {
      g_string_append (str, "\n        stroke='none'");
    }

  if ((fill_enabled = path_paintable_get_path_fill (self, idx, &fill_rule, &fill_symbolic, &color)))
    {
      const char *rule[] = { "nonzero", "evenodd" };

      g_string_append_printf (str, "\n        fill-rule='%s'", rule[fill_rule]);

      if (fill_symbolic == 0xffff)
        {
          g_autofree char *s = gdk_rgba_to_string (&color);
          g_string_append_printf (str, "\n        fill='%s'", s);
          g_string_append_printf (str, "\n        gpa:fill='%s'", s);
          has_gtk_attr = TRUE;
        }
      else if (fill_symbolic <= GTK_SYMBOLIC_COLOR_ACCENT)
        {
          if (color.alpha < 1)
            g_string_append_printf (str, "\n        fill-opacity='%s'",
                                    g_ascii_formatd (buffer, sizeof (buffer), "%g", color.alpha));
          g_string_append_printf (str, "\n        fill='%s'", fallback_color[fill_symbolic]);

          if (fill_symbolic < GTK_SYMBOLIC_COLOR_ACCENT)
            g_strv_builder_take (class_builder, g_strdup_printf ("%s-fill", sym[fill_symbolic]));
          else
            has_gtk_attr = TRUE;
        }
    }
  else
    {
      g_string_append (str, "\n        fill='none'");
      g_strv_builder_add (class_builder, "transparent-fill");
    }

  if ((path_paintable_get_path_states (self, idx) & (G_GUINT64_CONSTANT(1) << initial_state)) == 0)
    g_strv_builder_add (class_builder, "not-initial-state");

  class_strv = g_strv_builder_unref_to_strv (class_builder);
  class_str = g_strjoinv (" ", class_strv);
  g_string_append_printf (str, "\n        class='%s'", class_str);
  g_free (class_str);
  g_strfreev (class_strv);

  if (has_gtk_attr)
    {
      if (stroke_enabled && stroke_symbolic <= GTK_SYMBOLIC_COLOR_ACCENT)
        g_string_append_printf (str, "\n        gpa:stroke='%s'", sym[stroke_symbolic]);
      if (fill_enabled && fill_symbolic <= GTK_SYMBOLIC_COLOR_ACCENT)
        g_string_append_printf (str, "\n        gpa:fill='%s'", sym[fill_symbolic]);
    }

  g_string_append (str, "/>\n");

  gsk_stroke_free (stroke);
}

static void
path_paintable_save (PathPaintable *self,
                     GString       *str,
                     unsigned int   initial_state)
{
  char buffer[G_ASCII_DTOSTR_BUF_SIZE];
  const char *keywords;

  g_string_append (str, "<svg xmlns='http://www.w3.org/2000/svg'");
  g_string_append_printf (str, "\n     width='%s' height='%s'",
                          g_ascii_formatd (buffer, sizeof (buffer), "%g", path_paintable_get_width (self)),
                          g_ascii_formatd (buffer, sizeof (buffer), "%g", path_paintable_get_height (self)));
  g_string_append (str, "\n     xmlns:gpa='https://www.gtk.org/grappa'");

  g_string_append (str, "\n     gpa:version='1'");

  keywords = path_paintable_get_keywords (self);
  if (keywords)
    g_string_append_printf (str,      "\n     gpa:keywords='%s'", keywords);

  if (initial_state != (unsigned int) -1)
    g_string_append_printf (str,      "\n     gpa:state='%u'", initial_state);

  g_string_append (str, ">\n");

  /* Compatibility with other renderers */
  g_string_append (str, "  <style type='text/css'>\n");
  g_string_append (str, "    .not-initial-state {\n      display: none;\n    }\n");
  g_string_append (str, "  </style>\n");

  for (size_t idx = 0; idx < path_paintable_get_n_paths (self); idx++)
    path_paintable_save_path (self, idx, initial_state, str);

  g_string_append (str, "</svg>");
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
  self->paths = g_array_new (FALSE, TRUE, sizeof (PathElt));
  g_array_set_clear_func (self->paths, clear_path_elt);

  self->state = 0;
  self->width = 100;
  self->height = 100;
}

static void
path_paintable_dispose (GObject *object)
{
  PathPaintable *self = PATH_PAINTABLE (object);

  g_array_unref (self->paths);
  g_free (self->keywords);

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
path_paintable_get_property (GObject      *object,
                             unsigned int  property_id,
                             GValue       *value,
                             GParamSpec   *pspec)
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
      path_paintable_set_weight (self, g_value_get_float (value));
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

static void
set_shape_and_params (PathElt      *elt,
                      ShapeType     shape_type,
                      float        *params,
                      unsigned int  n_params)
{
  if (elt->shape_type == SHAPE_POLY_LINE ||
      elt->shape_type == SHAPE_POLYGON)
    g_free (elt->polyline.params);

  elt->shape_type = shape_type;

  if (shape_type == SHAPE_LINE)
    {
      elt->line.x1 = params[0];
      elt->line.y1 = params[1];
      elt->line.x2 = params[2];
      elt->line.y2 = params[3];
    }
  else if (shape_type == SHAPE_CIRCLE)
    {
      elt->circle.cx = params[0];
      elt->circle.cy = params[1];
      elt->circle.r = params[2];
    }
  else if (shape_type == SHAPE_ELLIPSE)
    {
      elt->ellipse.cx = params[0];
      elt->ellipse.cy = params[1];
      elt->ellipse.rx = params[2];
      elt->ellipse.ry = params[3];
    }
  else if (shape_type == SHAPE_RECT)
    {
      elt->rect.x = params[0];
      elt->rect.y = params[1];
      elt->rect.width = params[2];
      elt->rect.height = params[3];
      elt->rect.rx = params[4];
      elt->rect.ry = params[5];
    }
  else if (shape_type == SHAPE_POLY_LINE ||
           shape_type == SHAPE_POLYGON)
    {
      elt->polyline.params = g_new (float, n_params);
      elt->polyline.n_params = n_params;
      memcpy (elt->polyline.params, params, sizeof (float) * n_params);
    }
}

size_t
path_paintable_add_path (PathPaintable *self,
                         GskPath       *path,
                         ShapeType      shape_type,
                         float         *params,
                         unsigned int   n_params)
{
  PathElt elt;

  elt.path = gsk_path_ref (path);
  elt.id = NULL;

  set_shape_and_params (&elt, shape_type, params, n_params);

  elt.states = ALL_STATES;

  elt.transition.type = GPA_TRANSITION_NONE;
  elt.transition.duration = 0;
  elt.transition.delay = 0;
  elt.transition.easing = GPA_EASING_LINEAR;
  elt.transition.origin = 0;

  elt.animation.direction = GPA_ANIMATION_NORMAL;
  elt.animation.duration = 0;
  elt.animation.repeat = INFINITY;
  elt.animation.segment = 0.2;
  elt.animation.easing = GPA_EASING_LINEAR;

  elt.fill.enabled = FALSE;
  elt.fill.rule = GSK_FILL_RULE_WINDING;
  elt.fill.symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
  elt.fill.color = (GdkRGBA) { 0, 0, 0, 1 };

  elt.stroke.enabled = TRUE;
  elt.stroke.width = 2;
  elt.stroke.min_width = 0.5;
  elt.stroke.max_width = 3;
  elt.stroke.linecap = GSK_LINE_CAP_ROUND;
  elt.stroke.linejoin = GSK_LINE_JOIN_ROUND;
  elt.stroke.symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
  elt.stroke.color = (GdkRGBA) { 0, 0, 0, 1 };

  elt.attach.to = (size_t) -1;
  elt.attach.position = 0;

  g_array_append_val (self->paths, elt);

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);

  return self->paths->len - 1;
}

void
path_paintable_delete_path (PathPaintable *self,
                            size_t         idx)
{
  for (size_t i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (elt->attach.to == (size_t) -1)
        continue;

      if (elt->attach.to == idx)
        elt->attach.to = (size_t) -1;
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
                          size_t         idx,
                          size_t         new_pos)
{
  PathElt tmp;

  g_return_if_fail (idx < self->paths->len);
  g_return_if_fail (new_pos < self->paths->len);

  if (new_pos == idx)
    return;

  for (size_t i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (elt->attach.to == (size_t) -1)
        continue;

      if (elt->attach.to == idx)
        elt->attach.to = new_pos;
      else if (idx < elt->attach.to && elt->attach.to <= new_pos)
        elt->attach.to -= 1;
      else if (elt->attach.to >= new_pos && elt->attach.to < idx)
        elt->attach.to += 1;
    }

  tmp = g_array_index (self->paths, PathElt, idx);

  /* Do not clear path struct while removing item */
  g_array_set_clear_func (self->paths, NULL);
  g_array_remove_index (self->paths, idx);
  g_array_set_clear_func (self->paths, clear_path_elt);

  g_array_insert_val (self->paths, new_pos, tmp);

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);
}

void
path_paintable_duplicate_path (PathPaintable *self,
                               gsize          idx)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt elt = g_array_index (self->paths, PathElt, idx);

  g_array_append_val (self->paths, elt);

  PathElt *pelt = &g_array_index (self->paths, PathElt, self->paths->len - 1);

  gsk_path_ref (pelt->path);
  pelt->id = g_strdup (pelt->id);

  g_signal_emit (self, signals[CHANGED], 0);
  g_signal_emit (self, signals[PATHS_CHANGED], 0);
}

void
path_paintable_set_path (PathPaintable *self,
                         size_t         idx,
                         GskPath       *path)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  g_clear_pointer (&elt->path, gsk_path_unref);
  elt->path = gsk_path_ref (path);

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_shape (PathPaintable *self,
                               size_t         idx,
                               GskPath       *path,
                               ShapeType      shape_type,
                               float         *params,
                               unsigned int   n_params)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  set_shape_and_params (elt, shape_type, params, n_params);

  g_clear_pointer (&elt->path, gsk_path_unref);
  if (path)
    elt->path = gsk_path_ref (path);
  else
    switch ((unsigned int) shape_type)
      {
      case SHAPE_LINE:
        elt->path = line_path_new (params[0], params[1], params[2], params[3]);
        break;
      case SHAPE_CIRCLE:
        elt->path = circle_path_new (params[0], params[1], params[2]);
        break;
      case SHAPE_ELLIPSE:
        elt->path = ellipse_path_new (params[0], params[1], params[2], params[3]);
        break;
      case SHAPE_RECT:
        elt->path = rect_path_new (params[0], params[1], params[2], params[3], params[4], params[5]);
        break;
      case SHAPE_POLY_LINE:
      case SHAPE_POLYGON:
        elt->path = polyline_path_new (params, n_params, shape_type == SHAPE_POLYGON);
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
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->states == states)
    return;

  elt->states = states;

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MAX_STATE]);

  g_signal_emit (self, signals[CHANGED], 0);
}

gboolean
path_paintable_set_path_id (PathPaintable *self,
                            size_t         idx,
                            const char    *id)
{
  g_return_val_if_fail (idx < self->paths->len, FALSE);

  for (size_t i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (i != idx &&
          elt->id != NULL && id != NULL &&
          strcmp (elt->id, id) == 0)
        return FALSE;
    }

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (g_set_str (&elt->id, id))
    g_signal_emit (self, signals[CHANGED], 0);

  return TRUE;
}

const char *
path_paintable_get_path_id (PathPaintable *self,
                            size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, NULL);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->id;
}

void
path_paintable_set_path_transition (PathPaintable   *self,
                                    size_t           idx,
                                    GpaTransition    type,
                                    float            duration,
                                    float            delay,
                                    GpaEasing        easing)
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

  if (elt->fill.enabled && elt->transition.type == GPA_TRANSITION_ANIMATE)
    g_warning ("Can't currently transition fills");

  g_signal_emit (self, signals[CHANGED], 0);
}

void
path_paintable_set_path_animation (PathPaintable      *self,
                                   size_t              idx,
                                   GpaAnimation        direction,
                                   float               duration,
                                   float               repeat,
                                   GpaEasing           easing,
                                   float               segment)
{
  g_return_if_fail (idx < self->paths->len);
  g_return_if_fail (duration >= 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->animation.direction == direction &&
      elt->animation.duration == duration &&
      elt->animation.repeat == repeat &&
      elt->animation.easing == easing &&
      elt->animation.segment == segment)
    return;

  elt->animation.direction = direction;
  elt->animation.duration = duration;
  elt->animation.repeat = repeat;
  elt->animation.easing = easing;
  elt->animation.segment = segment;

  g_signal_emit (self, signals[CHANGED], 0);
}

GpaAnimation
path_paintable_get_path_animation_direction (PathPaintable *self,
                                             size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, GPA_ANIMATION_NONE);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.direction;
}

float
path_paintable_get_path_animation_duration (PathPaintable *self,
                                            size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.duration;
}

float
path_paintable_get_path_animation_repeat (PathPaintable *self,
                                          size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.repeat;
}

GpaEasing
path_paintable_get_path_animation_easing (PathPaintable *self,
                                          size_t         idx)
{
  g_return_val_if_fail (idx< self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.easing;
}

float
path_paintable_get_path_animation_segment (PathPaintable *self,
                                           size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0.2f);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.segment;
}

void
path_paintable_set_path_origin (PathPaintable *self,
                                size_t         idx,
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
                              size_t           idx,
                              gboolean         enabled,
                              GskFillRule      rule,
                              unsigned int     symbolic,
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

  if (elt->fill.enabled && elt->transition.type == GPA_TRANSITION_ANIMATE)
    g_warning ("Can't currently transition fills");

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
                                          size_t         idx,
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
                                          size_t         idx,
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
                            size_t         idx,
                            size_t         to,
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
                                size_t         idx,
                                size_t        *to,
                                float         *pos)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  *to = elt->attach.to;
  *pos = elt->attach.position;
}

void
path_paintable_set_keywords (PathPaintable *self,
                             const char    *keywords)
{
  if (g_set_str (&self->keywords, keywords))
    g_signal_emit (self, signals[CHANGED], 0);
}

const char *
path_paintable_get_keywords (PathPaintable *self)
{
  return self->keywords;
}

size_t
path_paintable_get_n_paths (PathPaintable *self)
{
  return self->paths->len;
}

GskPath *
path_paintable_get_path (PathPaintable *self,
                         size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, NULL);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->path;
}

ShapeType
path_paintable_get_path_shape_type (PathPaintable *self,
                                    size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, SHAPE_PATH);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->shape_type;
}

float *
path_paintable_get_path_shape_params (PathPaintable *self,
                                      size_t         idx,
                                      size_t        *n_params)
{
  g_return_val_if_fail (idx < self->paths->len, NULL);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  switch ((unsigned int) elt->shape_type)
    {
    case SHAPE_RECT:
      *n_params = 6;
      break;
    case SHAPE_CIRCLE:
      *n_params = 3;
      break;
    case SHAPE_ELLIPSE:
      *n_params = 4;
      break;
    case SHAPE_LINE:
      *n_params = 4;
      break;
    case SHAPE_PATH:
      *n_params = 0;
      break;
    default:
      g_assert_not_reached ();
    }
  return elt->shape_params;
}

uint64_t
path_paintable_get_path_states (PathPaintable *self,
                                size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->states;
}

GpaTransition
path_paintable_get_path_transition_type (PathPaintable *self,
                                         size_t         idx)
{
  g_return_val_if_fail (idx< self->paths->len, GPA_TRANSITION_NONE);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition.type;
}

float
path_paintable_get_path_transition_duration (PathPaintable *self,
                                             size_t         idx)
{
  g_return_val_if_fail (idx< self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition.duration;
}

float
path_paintable_get_path_transition_delay (PathPaintable *self,
                                          size_t         idx)
{
  g_return_val_if_fail (idx< self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition.delay;
}

GpaEasing
path_paintable_get_path_transition_easing (PathPaintable *self,
                                           size_t         idx)
{
  g_return_val_if_fail (idx< self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition.easing;
}

float
path_paintable_get_path_origin (PathPaintable *self,
                                size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0.0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->transition.origin;
}

gboolean
path_paintable_get_path_fill (PathPaintable *self,
                              size_t         idx,
                              GskFillRule   *rule,
                              unsigned int  *symbolic,
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
                                size_t         idx,
                                GskStroke     *stroke,
                                unsigned int  *symbolic,
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
  unsigned int symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
  GdkRGBA color = (GdkRGBA) { 0, 0, 0, 1 };
  size_t to = (size_t) -1;
  float pos = 0;

  other = path_paintable_new ();

  path_paintable_set_size (other, self->width, self->height);
  path_paintable_set_keywords (other, self->keywords);

  stroke = gsk_stroke_new (1);

  for (size_t i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);
      float *params;
      unsigned int n_params;

      if (elt->shape_type == SHAPE_POLY_LINE || elt->shape_type == SHAPE_POLYGON)
        {
          params = elt->polyline.params;
          n_params = elt->polyline.n_params;
        }
      else
        {
          params = elt->shape_params;
          n_params = 6;
        }

      path_paintable_add_path (other, elt->path, elt->shape_type, params, n_params);
      path_paintable_set_path_states (other, i, path_paintable_get_path_states (self, i));
      path_paintable_set_path_transition (other, i,
                                          path_paintable_get_path_transition_type (self, i),
                                          path_paintable_get_path_transition_duration (self, i),
                                          path_paintable_get_path_transition_delay (self, i),
                                          path_paintable_get_path_transition_easing (self, i));
      path_paintable_set_path_origin (other, i, path_paintable_get_path_origin (self, i));
      path_paintable_set_path_animation (other, i,
                                         path_paintable_get_path_animation_direction (self, i),
                                         path_paintable_get_path_animation_duration (self, i),
                                         path_paintable_get_path_animation_repeat (self, i),
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
  unsigned int max_state;
  size_t n_paths;
  GskStroke *stroke;

  res = path_paintable_copy (one);

  max_state = path_paintable_get_max_state (res);
  n_paths = path_paintable_get_n_paths (res);

  for (size_t i = 0; i < path_paintable_get_n_paths (res); i++)
    {
      uint64_t states;

      states = path_paintable_get_path_states (res, i);
      if (states == ALL_STATES)
        {
          states = 0;

          for (size_t j = 0; j <= max_state; j++)
            states = states | (G_GUINT64_CONSTANT (1) << j);

          path_paintable_set_path_states (res, i, states);
        }
    }

  stroke = gsk_stroke_new (1);

  for (size_t i = 0; i < path_paintable_get_n_paths (two); i++)
    {
      size_t idx;
      uint64_t states;
      gboolean enabled;
      GskFillRule rule = GSK_FILL_RULE_WINDING;
      unsigned int symbolic = 0;
      GdkRGBA color;
      size_t attach_to = (size_t) -1;
      float attach_pos = 0;
      PathElt *elt = &g_array_index (two->paths, PathElt, i);
      float *params;
      unsigned int n_params;

      if (elt->shape_type == SHAPE_POLY_LINE || elt->shape_type == SHAPE_POLYGON)
        {
          params = elt->polyline.params;
          n_params = elt->polyline.n_params;
        }
      else
        {
          params = elt->shape_params;
          n_params = 6;
        }

      idx = path_paintable_add_path (res, elt->path, elt->shape_type, params, n_params);

      path_paintable_set_path_transition (res, idx,
                                          path_paintable_get_path_transition_type (two, i),
                                          path_paintable_get_path_transition_duration (two, i),
                                          path_paintable_get_path_transition_delay (two, i),
                                          path_paintable_get_path_transition_easing (two, i));
      path_paintable_set_path_origin (res, idx,
                                      path_paintable_get_path_origin (two, i));

      path_paintable_set_path_animation (res, idx,
                                         path_paintable_get_path_animation_direction (two, i),
                                         path_paintable_get_path_animation_duration (two, i),
                                         path_paintable_get_path_animation_repeat (two, i),
                                         path_paintable_get_path_animation_easing (two, i),
                                         path_paintable_get_path_animation_segment (two, i));

      states = path_paintable_get_path_states (two, i);
      if (states == ALL_STATES)
        {
          unsigned int max2 = path_paintable_get_max_state (two);

          states = 0;
          for (size_t j = 0; j <= max2; j++)
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

  for (size_t i = 0; i < self->paths->len; i++)
    {
      PathElt *elt = &g_array_index (self->paths, PathElt, i);

      if (elt->stroke.enabled)
        compat = MAX (compat, GTK_4_20);

      if (elt->transition.type != GPA_TRANSITION_NONE ||
          elt->animation.direction != GPA_ANIMATION_NONE ||
          elt->attach.to != (size_t) - 1)
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
  if (self->state == state)
    return;

  self->state = state;

  if (self->render_paintable)
    gtk_svg_set_state (GTK_SVG (self->render_paintable), state);
}

unsigned int
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
    gtk_svg_set_weight (GTK_SVG (self->render_paintable), weight);

  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_WEIGHT]);
}

float
path_paintable_get_weight (PathPaintable *self)
{
  return self->weight;
}

unsigned int
path_paintable_get_max_state (PathPaintable *self)
{
  return gtk_svg_get_n_states (GTK_SVG (ensure_render_paintable (self)));
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

  if (g_strcmp0 (self->keywords, other->keywords) != 0)
    return FALSE;

  for (size_t i = 0; i < self->paths->len; i++)
    {
      PathElt *elt1 = &g_array_index (self->paths, PathElt, i);
      PathElt *elt2 = &g_array_index (other->paths, PathElt, i);

      if (!path_elt_equal (elt1, elt2))
        return FALSE;
    }

  return TRUE;
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
  GString *str = g_string_new ("");

  path_paintable_save (self, str, initial_state);

  return g_string_free_to_bytes (str);
}

GBytes *
path_paintable_serialize_as_svg (PathPaintable *self)
{
  return gtk_svg_serialize (GTK_SVG (ensure_render_paintable (self)));
}

/* }}} */

/* vim:set foldmethod=marker: */
