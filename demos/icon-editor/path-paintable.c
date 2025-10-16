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
      float cx, cy, r;
    } circle;
    struct {
      float x, y, width, height, rx, ry;
    } rect;
    float shape_params[6];
  };

  char *id;
  uint64_t states;

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
    float repeat;
    float segment;
    EasingFunction easing;
    CalcMode mode;
    KeyFrame *frames;
    unsigned int n_frames;
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
static unsigned int signals[LAST_SIGNAL];

/* {{{ Helpers */

static void
clear_path_elt (gpointer data)
{
  PathElt *elt = data;

  gsk_path_unref (elt->path);
  g_free (elt->animation.frames);
  g_free (elt->id);
}

static void
notify_state (GObject *object)
{
  g_object_notify_by_pspec (object, properties[PROP_STATE]);
}

static GtkPathPaintable *
ensure_render_paintable (PathPaintable *self)
{
  if (!self->render_paintable)
    {
      g_autoptr (GBytes) bytes = NULL;
      g_autoptr (GError) error = NULL;

      bytes = path_paintable_serialize (self, self->state);
      self->render_paintable = gtk_path_paintable_new_from_bytes (bytes, &error);

      if (!self->render_paintable)
        g_error ("%s", error->message);

      gtk_path_paintable_set_weight (self->render_paintable, self->weight);

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

  if (elt1->animation.type != elt2->animation.type ||
      elt1->animation.direction != elt2->animation.direction ||
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

typedef struct
{
  PathPaintable *paintable;
  GHashTable *paths;
  GArray *attach;
} ParserData;

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

static void
markup_filter_attributes (const char *element_name,
                          const char **attribute_names,
                          const char **attribute_values,
                          const char  *name,
                          ...)
{
  va_list ap;

  va_start (ap, name);
  while (name)
    {
      const char **ptr = va_arg (ap, const char **);

      *ptr = NULL;
      for (unsigned int i = 0; attribute_names[i]; i++)
        {
          if (strcmp (attribute_names[i], name) == 0)
            {
              *ptr = attribute_values[i];
              break;
            }
        }

      name = va_arg (ap, const char *);
    }

  va_end (ap);
}

enum
{
  POSITIVE = 1 << 0,
  LENGTH   = 1 << 1,
  UNIT     = 1 << 2,
};

static gboolean
parse_float (const char    *name,
             const char    *value,
             unsigned int   flags,
             float         *f,
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
                float         *f,
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
        *f = v;
      else if (strcmp (end, "s") == 0)
        *f = v * 1000;
      else
        {
          set_attribute_error (error, name, value);
          return FALSE;
        }
    }
  else
    *f = v * 1000;

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

static GskPath *
circle_path_new (float cx,
                 float cy,
                 float radius)
{
  GskPathBuilder *builder = gsk_path_builder_new ();
  gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (cx, cy), radius);
  return gsk_path_builder_free_to_path (builder);
}

static GskPath *
rect_path_new (float x,
               float y,
               float width,
               float height,
               float rx,
               float ry)
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

static gboolean
states_parse (const char *text,
              uint64_t    default_value,
              uint64_t   *states)
{
  g_auto (GStrv) str = NULL;

  if (strcmp (text, "") == 0)
    {
      *states = default_value;
      return TRUE;
    }

  if (strcmp (text, "all") == 0)
    {
      *states = ALL_STATES;
      return TRUE;
    }

  if (strcmp (text, "none") == 0)
    {
      *states = NO_STATES;
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
          *states = ALL_STATES;
          return FALSE;
        }

      *states |= (G_GUINT64_CONSTANT (1) << u);
    }

  return TRUE;
}

static gboolean
origin_parse (const char *text,
              float      *origin)
{
  char *end;

  *origin = (float) g_ascii_strtod (text, &end);
  if ((end && *end != '\0') || *origin < 0 || *origin > 1)
    return FALSE;

  return TRUE;
}

static struct {
  float params[4];
} easing_funcs[] = {
  { { 0, 0, 1, 1 } },
  { { 0.42, 0, 0.58, 1 } },
  { { 0.42, 0, 1, 1 } },
  { { 0, 0, 0.58, 1 } },
  { { 0.25, 0.1, 0.25, 1 } },
};


static GArray *
construct_animation_frames (unsigned int easing, GError **error)
{
  GArray *res = g_array_new (FALSE, TRUE, sizeof (KeyFrame));

  KeyFrame frame;

  frame.value = 0;
  frame.time = 0;
  memcpy (frame.params, easing_funcs[easing].params, 4 * sizeof (float));
  g_array_append_val (res, frame);

  frame.value = 1;
  frame.time = 1;
  memcpy (frame.params, easing_funcs[easing].params, 4 * sizeof (float));
  g_array_append_val (res, frame);

  return res;
}

static inline gboolean
g_strv_has (GStrv       strv,
            const char *s)
{
  return g_strv_contains ((const char * const *) strv, s);
}

typedef struct {
  char *to;
  float pos;
} AttachData;

static void
attach_data_clear (gpointer data)
{
  AttachData *attach = data;
  g_free (attach->to);
}

static void
start_element_cb (GMarkupParseContext  *context,
                  const char           *element_name,
                  const char          **attribute_names,
                  const char          **attribute_values,
                  gpointer              user_data,
                  GError              **error)
{
  ParserData *data = user_data;
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
  GskStroke *stroke = NULL;
  unsigned int stroke_symbolic;
  GdkRGBA stroke_color;
  float stroke_opacity;
  unsigned int fill_symbolic;
  GskFillRule fill_rule;
  GdkRGBA fill_color;
  float fill_opacity;
  uint64_t states;
  TransitionType transition_type;;
  float transition_duration;
  float transition_delay;
  EasingFunction transition_easing;
  AnimationType animation_type;
  AnimationDirection animation_direction;
  float animation_duration;
  float animation_repeat;
  float animation_segment;
  unsigned int animation_easing;
  GArray *animation_keyframes = NULL;
  float origin;
  size_t idx;
  AttachData attach;
  float stroke_width;
  float min_stroke_width;
  float max_stroke_width;
  ShapeType shape_type;
  float shape_params[6];

  if (strcmp (element_name, "svg") == 0)
    {
      const char *width_attr = NULL;
      const char *height_attr = NULL;
      const char *keywords_attr = NULL;
      const char *version_attr = NULL;
      const char *state_attr = NULL;
      float width, height;
      unsigned int initial_state;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                "width", &width_attr,
                                "height", &height_attr,
                                "gpa:keywords", &keywords_attr,
                                "gpa:version", &version_attr,
                                "gpa:state", &state_attr,
                                NULL);

      if (width_attr == NULL)
        {
          set_missing_attribute_error (error, "width");
          return;
        }

      if (!parse_float ("width", width_attr, LENGTH, &width, error))
        return;

      if (height_attr == NULL)
        {
          set_missing_attribute_error (error, "height");
          return;
        }

      if (!parse_float ("height", height_attr, LENGTH, &height, error))
        return;

      path_paintable_set_size (data->paintable, width, height);

      if (keywords_attr)
        {
          GStrv keywords = g_strsplit (keywords_attr, " ", 0);

          path_paintable_set_keywords (data->paintable, keywords);

          g_strfreev (keywords);
        }

      initial_state = 0;
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

          initial_state = (unsigned int) state;
        }

      path_paintable_set_state (data->paintable, initial_state);

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
      float cx = 0;
      float cy = 0;
      float r = 0;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                "cx", &cx_attr,
                                "cy", &cy_attr,
                                "r", &r_attr,
                                NULL);

      if (cx_attr)
        {
          if (!parse_float ("cx", cx_attr, 0, &cx, error))
            return;
        }

      if (cy_attr)
        {
          if (!parse_float ("cy", cy_attr, 0, &cy, error))
            return;
        }

      if (r_attr)
        {
          if (!parse_float ("r", r_attr, POSITIVE, &r, error))
            return;
        }

      if (r == 0)
        return;  /* nothing to do */

      path = circle_path_new (cx, cy, r);
      shape_type = SHAPE_CIRCLE;
      memset (shape_params, 0, sizeof (float) * 6);
      shape_params[0] = cx;
      shape_params[1] = cy;
      shape_params[2] = r;
    }
  else if (strcmp (element_name, "rect") == 0)
    {
      const char *x_attr = NULL;
      const char *y_attr = NULL;
      const char *width_attr = NULL;
      const char *height_attr = NULL;
      const char *rx_attr = NULL;
      const char *ry_attr = NULL;
      float x = 0;
      float y = 0;
      float width = 0;
      float height = 0;
      float rx = 0;
      float ry = 0;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                "x", &x_attr,
                                "y", &y_attr,
                                "width", &width_attr,
                                "height", &height_attr,
                                "rx", &rx_attr,
                                "ry", &ry_attr,
                                NULL);

      if (x_attr)
        {
          if (!parse_float ("x", x_attr, 0, &x, error))
            return;
        }

      if (y_attr)
        {
          if (!parse_float ("y", y_attr, 0, &y, error))
            return;
        }

      if (width_attr)
        {
          if (!parse_float ("width", width_attr, POSITIVE, &width, error))
            return;
        }

      if (height_attr)
        {
          if (!parse_float ("height", height_attr, POSITIVE, &height, error))
            return;
        }

      if (width == 0 || height == 0)
        return;  /* nothing to do */

      if (rx_attr)
        {
          if (!parse_float ("rx", rx_attr, POSITIVE, &rx, error))
            return;
        }

     if (ry_attr)
        {
          if (!parse_float ("ry", ry_attr, POSITIVE, &ry, error))
            return;
        }

      if (!rx_attr && ry_attr)
        rx = ry;
      else if (rx_attr && !ry_attr)
        ry = rx;

      path = rect_path_new (x, y, width, height, rx, ry);
      shape_type = SHAPE_RECT;
      memset (shape_params, 0, sizeof (float) * 6);
      shape_params[0] = x;
      shape_params[1] = y;
      shape_params[2] = width;
      shape_params[3] = height;
      shape_params[4] = rx;
      shape_params[5] = ry;
    }
  else if (strcmp (element_name, "path") == 0)
    {
      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
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

      shape_type = SHAPE_PATH;
      memset (shape_params, 0, sizeof (float) * 6);
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
                            "stroke-width", &stroke_width_attr,
                            "stroke-opacity", &stroke_opacity_attr,
                            "stroke-linecap", &stroke_linecap_attr,
                            "stroke-linejoin", &stroke_linejoin_attr,
                            "fill-opacity", &fill_opacity_attr,
                            "fill-rule", &fill_rule_attr,
                            "id", &id_attr,
                            "gpa:stroke-width", &gtk_stroke_width_attr,
                            "gpa:fill", &fill_attr,
                            "gpa:stroke", &stroke_attr,
                            "gpa:states", &states_attr,
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
                            "gpa:origin", &origin_attr,
                            "gpa:attach-to", &attach_to_attr,
                            "gpa:attach-pos", &attach_pos_attr,
                            "class", &class_attr,
                            NULL);

  if (!stroke_attr &&
      !fill_attr &&
      !states_attr &&
      !transition_type_attr &&
      !transition_duration_attr &&
      !transition_delay_attr &&
      !transition_easing_attr &&
      !origin_attr &&
      !animation_type_attr &&
      !animation_direction_attr &&
      !animation_duration_attr &&
      !animation_segment_attr &&
      !animation_easing_attr &&
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
      if (!parse_float ("stroke-opacity", stroke_opacity_attr, UNIT, &stroke_opacity, error))
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

  stroke = gsk_stroke_new (stroke_width);
  gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);
  gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_ROUND);

  if (stroke_width_attr)
    {
      if (!parse_float ("stroke-width", stroke_width_attr, POSITIVE, &stroke_width, error))
        goto cleanup;

      gsk_stroke_set_line_width (stroke, stroke_width);
    }

  min_stroke_width = stroke_width * 0.25;
  max_stroke_width = stroke_width * 1.5;

  if (gtk_stroke_width_attr)
    {
      GStrv str;
      float w;

      str = g_strsplit (gtk_stroke_width_attr, " ", 0);
      if (g_strv_length (str) != 3)
        {
          set_attribute_error (error, "gpa:stroke-width", gtk_stroke_width_attr);
          g_strfreev (str);
          goto cleanup;
        }

      if (!parse_float ("gpa:stroke-width", str[0], POSITIVE, &min_stroke_width, error) ||
          !parse_float ("gpa:stroke-width", str[1], POSITIVE, &w, error) ||
          !parse_float ("gpa:stroke-width", str[2], POSITIVE, &max_stroke_width, error) ||
          w < min_stroke_width || w > max_stroke_width)
        {
          g_strfreev (str);
          goto cleanup;
        }
      g_strfreev (str);

      gsk_stroke_set_line_width (stroke, w);
    }

  if (stroke_linecap_attr)
    {
      GskLineCap linecap;

      if (!parse_enum ("stroke-linecap", stroke_linecap_attr,
                       (const char *[]) { "butt", "round", "square" }, 3,
                        &linecap, error))
        goto cleanup;

      gsk_stroke_set_line_cap (stroke, linecap);
    }

  if (stroke_linejoin_attr)
    {
      GskLineJoin linejoin;

      if (!parse_enum ("stroke-linejoin", stroke_linejoin_attr,
                       (const char *[]) { "miter", "round", "bevel" }, 3,
                        &linejoin, error))
        goto cleanup;

      gsk_stroke_set_line_join (stroke, linejoin);
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
      if (!parse_float ("fill-opacity", fill_opacity_attr, UNIT, &fill_opacity, error))
        goto cleanup;
      fill_opacity = CLAMP (fill_opacity, 0, 1);
    }

  fill_symbolic = 0xffff;
  fill_color = (GdkRGBA) { 0, 0, 0, 1 };
  if (fill_attr)
    {
      if (!parse_paint ("gpa:fill", fill_attr, &fill_symbolic, &fill_color, error))         goto cleanup;
    }

  fill_color.alpha *= fill_opacity;

  transition_type = TRANSITION_TYPE_NONE;
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

  transition_easing = EASING_FUNCTION_LINEAR;
  if (transition_easing_attr)
    {
      if (!parse_enum ("gpa:transition-easing", transition_easing_attr,
                       (const char *[]) { "linear", "ease-in-out", "ease-in",
                                          "ease-out", "ease" }, 5,
                        &transition_easing, error))
        goto cleanup;
    }

  origin = 0;
  if (origin_attr)
    {
      if (!parse_float ("gpa:origin", origin_attr, UNIT, &origin, error))
        goto cleanup;
    }

  states = ALL_STATES;
  if (states_attr)
    {
      if (!states_parse (states_attr, ALL_STATES, &states))
        {
          set_attribute_error (error, "gpa:states", states_attr);
          goto cleanup;
        }
    }

  animation_type = ANIMATION_TYPE_NONE;
  if (animation_type_attr)
    {
      if (!parse_enum ("gpa:animation-type", animation_type_attr,
                       (const char *[]) { "none", "automatic", }, 2,
                        &animation_type, error))
        goto cleanup;
    }

  animation_direction = ANIMATION_DIRECTION_NORMAL;
  if (animation_direction_attr)
    {
      if (!parse_enum ("gpa:animation-direction", animation_direction_attr,
                       (const char *[]) { "normal", "alternate", "reverse",
                                          "reverse-alternate", "in-out",
                                          "in-out-alternate", "in-out-reverse",
                                          "segment", "segment-alternate" }, 9,
                        &animation_direction, error))
        goto cleanup;
    }

  animation_duration = 0.f;
  if (animation_duration_attr)
    {
      if (!parse_duration ("gpa:animation-duration", animation_duration_attr, POSITIVE, &animation_duration, error))
        goto cleanup;
    }

  animation_repeat = G_MAXFLOAT;
  if (animation_repeat_attr)
    {
      if (strcmp (animation_repeat_attr, "indefinite") == 0)
        animation_repeat = G_MAXFLOAT;
      else if (!parse_float ("gpa:animation-repeat", animation_repeat_attr, POSITIVE, &animation_repeat, error))
        goto cleanup;
    }

  animation_segment = 0.2f;
  if (animation_segment_attr)
    {
      if (!parse_float ("gpa:animation-segment", animation_segment_attr, POSITIVE, &animation_segment, error))
        goto cleanup;
    }

  animation_easing = EASING_FUNCTION_LINEAR;
  if (animation_easing_attr)
    {
      if (!parse_enum ("gpa:animation-easing", animation_easing_attr,
                       (const char *[]) { "linear", "ease-in-out", "ease-in",
                                          "ease-out", "ease" }, 5,
                        &animation_easing, error))
        goto cleanup;
    }

  animation_keyframes = construct_animation_frames (animation_easing, error);
  if (!animation_keyframes)
    goto cleanup;


  attach.to = g_strdup (attach_to_attr);
  attach.pos = 0;
  if (attach_pos_attr)
    {
      if (!origin_parse (attach_pos_attr, &attach.pos))
        {
          set_attribute_error (error, "gpa:attach-pos", attach_pos_attr);
          goto cleanup;
        }
    }

  idx = path_paintable_add_path (data->paintable, path, shape_type, shape_params);

  path_paintable_set_path_states (data->paintable, idx, states);
  path_paintable_set_path_animation (data->paintable, idx, animation_type, animation_direction, animation_duration, animation_repeat, animation_easing, animation_segment);
  path_paintable_set_path_animation_timing (data->paintable, idx, animation_easing, CALC_MODE_SPLINE, (KeyFrame *) animation_keyframes->data, animation_keyframes->len);
  path_paintable_set_path_transition (data->paintable, idx, transition_type, transition_duration, transition_delay, transition_easing);
  path_paintable_set_path_origin (data->paintable, idx, origin);
  path_paintable_set_path_fill (data->paintable, idx, fill_attr != NULL, fill_rule, fill_symbolic, &fill_color);
  path_paintable_set_path_stroke (data->paintable, idx, stroke_attr != NULL, stroke, stroke_symbolic, &stroke_color);
  path_paintable_set_path_stroke_variation (data->paintable, idx, min_stroke_width, max_stroke_width);

  g_array_append_val (data->attach, attach);

  if (id_attr)
    {
      path_paintable_set_path_id (data->paintable, idx, id_attr);
      g_hash_table_insert (data->paths, g_strdup (id_attr), GUINT_TO_POINTER (idx));
    }

cleanup:
  g_clear_pointer (&path, gsk_path_unref);
  g_clear_pointer (&stroke, gsk_stroke_free);
  g_clear_pointer (&animation_keyframes, g_array_unref);
}

static gboolean
parse_symbolic_svg (PathPaintable  *paintable,
                    GBytes         *bytes,
                    GError        **error)
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
  const char *text;
  size_t length;
  gboolean ret;

  data.paintable = paintable;
  data.paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
  data.attach = g_array_new (FALSE, TRUE, sizeof (AttachData));
  g_array_set_clear_func (data.attach, attach_data_clear);

  text = g_bytes_get_data (bytes, &length);

  context = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &data, NULL);
  ret = g_markup_parse_context_parse (context, text, length, error);

  g_markup_parse_context_free (context);

  if (ret)
    {
      for (unsigned int i = 0; i < data.attach->len; i++)
        {
          AttachData *attach = &g_array_index (data.attach, AttachData, i);
          gpointer value;

          if (!attach->to)
            continue;

          if (!g_hash_table_lookup_extended (data.paths, attach->to, NULL, &value))
            {
              g_set_error (error, G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT,
                           "Invalid %s attribute value: %s", "gpa:attach-to", attach->to);
              ret = FALSE;
              break;
            }

          path_paintable_attach_path (paintable, i, GPOINTER_TO_UINT (value), attach->pos);
        }
    }

  g_hash_table_unref (data.paths);
  g_array_unref (data.attach);

  return ret;
}

/* }}} */
/* {{{ Serialization */

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

  if (elt->shape_type == SHAPE_CIRCLE)
    {
      g_string_append (str,  "  <circle");
      g_string_append_printf (str, " cx='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->circle.cx));
      g_string_append_printf (str, " cy='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->circle.cy));
      g_string_append_printf (str, " r='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g", elt->circle.r));
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

  if (path_paintable_get_path_animation_type (self, idx) != ANIMATION_TYPE_NONE)
    {
      const char *type[] = { "none", "automatic", "external" };

      g_string_append_printf (str, "\n        gpa:animation-type='%s'", type[path_paintable_get_path_animation_type (self, idx)]);
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_animation_direction (self, idx) != ANIMATION_DIRECTION_NORMAL)
    {
      const char *direction[] = { "normal", "alternate", "reverse", "reverse-alternate", "in-out", "in-out-alternate", "in-out-reverse", "segment", "segment-alternate" };

      g_string_append_printf (str, "\n        gpa:animation-direction='%s'", direction[path_paintable_get_path_animation_direction (self, idx)]);
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_animation_duration (self, idx) != 0)
    {
      g_string_append_printf (str, "\n        gpa:animation-duration='%sms'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g",
                                               path_paintable_get_path_animation_duration (self, idx)));
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_animation_repeat (self, idx) != G_MAXFLOAT)
    {
      g_string_append_printf (str, "\n        gpa:animation-repeat='%s'",
                              g_ascii_formatd (buffer, sizeof (buffer), "%g",
                                               path_paintable_get_path_animation_repeat (self, idx)));
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_animation_easing (self, idx) != EASING_FUNCTION_LINEAR)
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

  if (path_paintable_get_path_transition_type (self, idx) != TRANSITION_TYPE_NONE)
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

  if (path_paintable_get_path_transition_easing (self, idx) != EASING_FUNCTION_LINEAR)
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
}

static void
path_paintable_save (PathPaintable *self,
                     GString       *str,
                     unsigned int   initial_state)
{
  GStrv keywords;
  char buffer[G_ASCII_DTOSTR_BUF_SIZE];

  g_string_append_printf (str, "<svg width='%s' height='%s'",
                          g_ascii_formatd (buffer, sizeof (buffer), "%g", path_paintable_get_width (self)),
                          g_ascii_formatd (buffer, sizeof (buffer), "%g", path_paintable_get_height (self)));
  g_string_append (str, "\n     xmlns:gpa='https://www.gtk.org/grappa'");

  g_string_append (str, "\n     gpa:version='1'");

  keywords = path_paintable_get_keywords (self);
  if (keywords)
    {
      g_string_append (str,      "\n     gpa:keywords='");
      for (unsigned int i = 0; keywords[i]; i++)
        {
          if (i > 0)
            g_string_append_c (str, ' ');
          g_string_append (str, keywords[i]);
        }
      g_string_append_c (str, '\'');
    }

  if (initial_state != 0)
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

size_t
path_paintable_add_path (PathPaintable *self,
                         GskPath       *path,
                         ShapeType      shape_type,
                         float          params[6])
{
  PathElt elt;

  elt.path = gsk_path_ref (path);
  elt.id = NULL;

  elt.shape_type = shape_type;
  if (shape_type == SHAPE_CIRCLE)
    {
      elt.circle.cx = params[0];
      elt.circle.cy = params[1];
      elt.circle.r = params[2];
    }
  else if (shape_type == SHAPE_RECT)
    {
      elt.rect.x = params[0];
      elt.rect.y = params[1];
      elt.rect.width = params[2];
      elt.rect.height = params[3];
      elt.rect.rx = params[4];
      elt.rect.ry = params[5];
    }

  elt.states = ALL_STATES;

  elt.transition.type = TRANSITION_TYPE_NONE;
  elt.transition.duration = 0;
  elt.transition.delay = 0;
  elt.transition.easing = EASING_FUNCTION_LINEAR;
  elt.transition.origin = 0;

  elt.animation.type = ANIMATION_TYPE_NONE;
  elt.animation.direction = ANIMATION_DIRECTION_NORMAL;
  elt.animation.duration = 0;
  elt.animation.repeat = G_MAXFLOAT;
  elt.animation.segment = 0.2;
  elt.animation.easing = EASING_FUNCTION_LINEAR;
  elt.animation.mode = CALC_MODE_LINEAR;
  elt.animation.frames = NULL;
  elt.animation.n_frames = 0;

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
  gsk_path_ref (tmp.path);

  g_array_remove_index (self->paths, idx);
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

  gsk_path_ref (elt.path);

  g_array_append_val (self->paths, elt);

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
                                    TransitionType   type,
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
                                   size_t              idx,
                                   AnimationType       type,
                                   AnimationDirection  direction,
                                   float               duration,
                                   float               repeat,
                                   EasingFunction      easing,
                                   float               segment)
{
  g_return_if_fail (idx < self->paths->len);
  g_return_if_fail (duration >= 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->animation.type == type &&
      elt->animation.direction == direction &&
      elt->animation.duration == duration &&
      elt->animation.repeat == repeat &&
      elt->animation.easing == easing &&
      elt->animation.segment == segment)
    return;

  elt->animation.type = type;
  elt->animation.direction = direction;
  elt->animation.duration = duration;
  elt->animation.repeat = repeat;
  elt->animation.easing = easing;
  elt->animation.segment = segment;

  g_signal_emit (self, signals[CHANGED], 0);
}

AnimationType
path_paintable_get_path_animation_type (PathPaintable *self,
                                        size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, ANIMATION_TYPE_NONE);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.type;
}

AnimationDirection
path_paintable_get_path_animation_direction (PathPaintable *self,
                                             size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, ANIMATION_DIRECTION_NORMAL);

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

EasingFunction
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
path_paintable_set_path_animation_timing (PathPaintable  *self,
                                          size_t          idx,
                                          EasingFunction  easing,
                                          CalcMode        mode,
                                          const KeyFrame *frames,
                                          unsigned int    n_frames)
{
  g_return_if_fail (idx < self->paths->len);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  if (elt->animation.easing == easing &&
      elt->animation.mode == mode &&
      elt->animation.n_frames == n_frames &&
      memcmp (elt->animation.frames, frames, sizeof (KeyFrame) * n_frames) == 0)
    return;

  elt->animation.easing = easing;
  elt->animation.mode = mode;
  if (frames != elt->animation.frames)
    {
      g_free (elt->animation.frames);
      elt->animation.frames = g_memdup2 (frames, sizeof (KeyFrame) * n_frames);
      elt->animation.n_frames = n_frames;
    }

  g_signal_emit (self, signals[CHANGED], 0);
}

CalcMode
path_paintable_get_path_animation_mode (PathPaintable *self,
                                        size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, CALC_MODE_LINEAR);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.mode;
}

unsigned int
path_paintable_get_path_animation_n_frames (PathPaintable *self,
                                            size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.n_frames;
}

const KeyFrame *
path_paintable_get_path_animation_frames (PathPaintable *self,
                                          size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, NULL);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->animation.frames;
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

  if (elt->fill.enabled && elt->transition.type == TRANSITION_TYPE_ANIMATE)
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

uint64_t
path_paintable_get_path_states (PathPaintable *self,
                                size_t         idx)
{
  g_return_val_if_fail (idx < self->paths->len, 0);

  PathElt *elt = &g_array_index (self->paths, PathElt, idx);

  return elt->states;
}

TransitionType
path_paintable_get_path_transition_type (PathPaintable *self,
                                         size_t         idx)
{
  g_return_val_if_fail (idx< self->paths->len, TRANSITION_TYPE_NONE);

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

EasingFunction
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

      path_paintable_add_path (other, elt->path, elt->shape_type, elt->shape_params);
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

      idx = path_paintable_add_path (res, elt->path, elt->shape_type, elt->shape_params);

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

      if (elt->transition.type != TRANSITION_TYPE_NONE ||
          elt->animation.type != ANIMATION_TYPE_NONE ||
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
    gtk_path_paintable_set_state (self->render_paintable, state);
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
    gtk_path_paintable_set_weight (self->render_paintable, weight);

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
  return gtk_path_paintable_get_max_state (ensure_render_paintable (self));
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

/* }}} */

/* vim:set foldmethod=marker: */
