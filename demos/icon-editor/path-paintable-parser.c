/*
 * Copyright (c) 2025 RedHat, Inc
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

#include <gtk/gtk.h>


/* This is a parser for a tiny subset of SVG with some extensions
 * in the gtk namespace to support the features of PathPaintable.
 *
 * See icon-format.md for a description of the format.
 */

typedef struct
{
  PathPaintable *paintable;
  GHashTable *paths;
} ParserData;

/* {{{ Helpers */

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
      const char **ptr;

      ptr = va_arg (ap, const char **);

      *ptr = NULL;
      for (int i = 0; attribute_names[i]; i++)
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
};

static gboolean
parse_float (const char  *name,
             const char  *value,
             guint        flags,
             float       *f,
             GError     **error)
{
  char *end;

  *f = g_ascii_strtod (value, &end);
  if ((end && *end != '\0' && ((flags & LENGTH) == 0 || strcmp (end, "px") != 0)) ||
      ((flags & POSITIVE) != 0 && *f < 0))
    {
      set_attribute_error (error, name, value);
      return FALSE;
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

char *
states_to_string (guint64 states)
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

      for (guint u = 0; u < 64; u++)
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

gboolean
states_parse (const char *text,
              guint64     default_value,
              guint64    *states)
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
  for (int i = 0; str[i]; i++)
    {
      guint u;
      char *end;

      u = (guint) g_ascii_strtoull (str[i], &end, 10);
      if ((end && *end != '\0') || (u > 63))
        {
          *states = ALL_STATES;
          return FALSE;
        }

      *states |= (G_GUINT64_CONSTANT (1) << u);
    }

  return TRUE;
}

gboolean
origin_parse (const char *text,
              float      *origin)
{
  char *end;

  *origin = (float) g_ascii_strtod (text, &end);
  if ((end && *end != '\0') || *origin < 0 || *origin > 1)
    return FALSE;

  return TRUE;
}

static inline gboolean
g_strv_has (GStrv       strv,
            const char *s)
{
  return g_strv_contains ((const char * const *) strv, s);
}

/* }}} */
/* {{{ SVG subset parser */

static void
start_element_cb (GMarkupParseContext  *context,
                  const gchar          *element_name,
                  const gchar         **attribute_names,
                  const gchar         **attribute_values,
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
  const char *animation_easing_attr = NULL;
  const char *animation_segment_attr = NULL;
  const char *origin_attr = NULL;
  const char *transition_type_attr = NULL;
  const char *transition_duration_attr = NULL;
  const char *transition_easing_attr = NULL;
  const char *id_attr = NULL;
  const char *attach_to_attr = NULL;
  const char *attach_pos_attr = NULL;
  GskPath *path = NULL;
  GskStroke *stroke = NULL;
  guint stroke_symbolic;
  GdkRGBA stroke_color;
  float stroke_opacity;
  guint fill_symbolic;
  GskFillRule fill_rule;
  GdkRGBA fill_color;
  float fill_opacity;
  guint64 states;
  TransitionType transition_type;;
  float transition_duration;
  EasingFunction transition_easing;
  AnimationType animation_type;
  AnimationDirection animation_direction;
  float animation_duration;
  EasingFunction animation_easing;
  float animation_segment;
  float origin;
  gsize idx;
  gsize attach_to;
  float attach_pos;
  float min_stroke_width;
  float max_stroke_width;

  if (strcmp (element_name, "svg") == 0)
    {
      const char *width_attr = NULL;
      const char *height_attr = NULL;
      const char *keywords_attr = NULL;
      const char *version_attr = NULL;
      float width, height;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                "width", &width_attr,
                                "height", &height_attr,
                                "gpa:keywords", &keywords_attr,
                                "gpa:version", &version_attr,
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

      if (version_attr)
        {
          guint version;
          char *end;

          version = (guint) g_ascii_strtoull (version_attr, &end, 10);
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
                            "gpa:animation-easing", &animation_easing_attr,
                            "gpa:animation-segment", &animation_segment_attr,
                            "gpa:transition-type", &transition_type_attr,
                            "gpa:transition-duration", &transition_duration_attr,
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
      !transition_easing_attr &&
      !origin_attr &&
      !animation_type_attr &&
      !animation_direction_attr &&
      !animation_duration_attr &&
      !animation_easing_attr &&
      !animation_segment_attr &&
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
      if (!parse_float ("stroke-opacity", stroke_opacity_attr, 0, &stroke_opacity, error))
        goto cleanup;
      stroke_opacity = CLAMP (stroke_opacity, 0, 1);
    }

  stroke_symbolic = 0xffff;
  stroke_color = (GdkRGBA) { 0, 0, 0, 1 };
  if (stroke_attr)
    {
      if (strcmp (stroke_attr, "foreground") == 0)
        stroke_symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
      else if (strcmp (stroke_attr, "success") == 0)
        stroke_symbolic = GTK_SYMBOLIC_COLOR_SUCCESS;
      else if (strcmp (stroke_attr, "warning") == 0)
        stroke_symbolic = GTK_SYMBOLIC_COLOR_WARNING;
      else if (strcmp (stroke_attr, "error") == 0)
        stroke_symbolic = GTK_SYMBOLIC_COLOR_ERROR;
      else if (strcmp (stroke_attr, "accent") == 0)
        stroke_symbolic = GTK_SYMBOLIC_COLOR_ACCENT;
      else
        {
          if (!gdk_rgba_parse (&stroke_color, stroke_attr))
            {
              set_attribute_error (error, "gpa:stroke", stroke_attr);
              goto cleanup;
            }
       }
    }

  stroke_color.alpha *= stroke_opacity;

  stroke = gsk_stroke_new (2);
  gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);
  gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_ROUND);
  min_stroke_width = 0.5;
  max_stroke_width = 5;

  if (stroke_width_attr)
    {
      float w;

      if (!parse_float ("stroke-width", stroke_width_attr, POSITIVE, &w, error))
        goto cleanup;

      gsk_stroke_set_line_width (stroke, w);

      min_stroke_width = w * 100.0 / 400.0;
      max_stroke_width = w * 1000.0 / 400.0;
    }

  if (gtk_stroke_width_attr)
    {
      int res;
      float w;

      res = sscanf (gtk_stroke_width_attr, "%f %f %f", &min_stroke_width, &w, &max_stroke_width);
      if (res < 3 || max_stroke_width < w || w < min_stroke_width)
        {
          set_attribute_error (error, "gpa:stroke-width", gtk_stroke_width_attr);
          goto cleanup;
        }

      gsk_stroke_set_line_width (stroke, w);
    }

  if (stroke_linecap_attr)
    {
      if (strcmp (stroke_linecap_attr, "butt") == 0)
        gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_BUTT);
      else if (strcmp (stroke_linecap_attr, "round") == 0)
        gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);
      else if (strcmp (stroke_linecap_attr, "square") == 0)
        gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_SQUARE);
      else
        {
          set_attribute_error (error, "stroke-linecap", stroke_linecap_attr);
          goto cleanup;
        }
    }

  if (stroke_linejoin_attr)
    {
      if (strcmp (stroke_linejoin_attr, "miter") == 0)
        gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_MITER);
      else if (strcmp (stroke_linejoin_attr, "round") == 0)
        gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_ROUND);
      else if (strcmp (stroke_linejoin_attr, "bevel") == 0)
        gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_BEVEL);
      else
        {
          set_attribute_error (error, "stroke-linejoin", stroke_linejoin_attr);
          goto cleanup;
        }
    }

  fill_rule = GSK_FILL_RULE_WINDING;
  if (fill_rule_attr)
    {
      if (strcmp (fill_rule_attr, "winding") == 0)
        fill_rule = GSK_FILL_RULE_WINDING;
      else if (strcmp (fill_rule_attr, "evenodd") == 0)
        fill_rule = GSK_FILL_RULE_EVEN_ODD;
      else
        {
          set_attribute_error (error, "fill-rule", fill_rule_attr);
          goto cleanup;
        }
    }

  fill_opacity = 1;
  if (fill_opacity_attr)
    {
      if (!parse_float ("fill-opacity", fill_opacity_attr, 0, &fill_opacity, error))
        goto cleanup;
      fill_opacity = CLAMP (fill_opacity, 0, 1);
    }

  fill_symbolic = 0xffff;
  fill_color = (GdkRGBA) { 0, 0, 0, 1 };
  if (fill_attr)
    {
      if (strcmp (fill_attr, "foreground") == 0)
        fill_symbolic = GTK_SYMBOLIC_COLOR_FOREGROUND;
      else if (strcmp (fill_attr, "success") == 0)
        fill_symbolic = GTK_SYMBOLIC_COLOR_SUCCESS;
      else if (strcmp (fill_attr, "warning") == 0)
        fill_symbolic = GTK_SYMBOLIC_COLOR_WARNING;
      else if (strcmp (fill_attr, "error") == 0)
        fill_symbolic = GTK_SYMBOLIC_COLOR_ERROR;
      else if (strcmp (stroke_attr, "accent") == 0)
        stroke_symbolic = GTK_SYMBOLIC_COLOR_ACCENT;
      else
        {
          if (!gdk_rgba_parse (&fill_color, fill_attr))
            {
              set_attribute_error (error, "gpa:fill", fill_attr);
              goto cleanup;
            }
       }
    }

  fill_color.alpha *= fill_opacity;

  transition_type = TRANSITION_TYPE_NONE;
  if (transition_type_attr)
    {
      const char *types[] = { "none", "animate", "blur", "fade" };
      guint i;

      for (i = 0; i < G_N_ELEMENTS (types); i++)
        {
          if (strcmp (transition_type_attr, types[i]) == 0)
            {
              transition_type = (TransitionType) i;
              break;
            }
        }

      if (i == G_N_ELEMENTS (types))
        {
          set_attribute_error (error, "gpa:transition-type", transition_type_attr);
          goto cleanup;
        }
    }

  transition_duration = 0.f;
  if (transition_duration_attr)
    {
      if (!parse_float ("gpa:transition-duration", transition_duration_attr, POSITIVE, &transition_duration, error))
        goto cleanup;
    }

  transition_easing = EASING_FUNCTION_LINEAR;
  if (transition_easing_attr)
    {
      const char *easing[] =  { "linear", "ease-in-out", "ease-in",
        "ease-out", "ease"
      };
      guint i;

      for (i = 0; i < G_N_ELEMENTS (easing); i++)
        {
          if (strcmp (transition_easing_attr, easing[i]) == 0)
            {
              transition_easing = (EasingFunction) i;
              break;
            }
        }

      if (i == G_N_ELEMENTS (easing))
        {
          set_attribute_error (error, "gpa:transition-easing", transition_easing_attr);
          goto cleanup;
        }
    }

  attach_to = (gsize) -1;
  origin = 0;
  if (origin_attr)
    {
      if (!origin_parse (origin_attr, &origin))
        {
          set_attribute_error (error, "gpa:origin", origin_attr);
          goto cleanup;
        }
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
      const char *types[] = { "none", "automatic" };
      guint i;

      for (i = 0; i < G_N_ELEMENTS (types); i++)
        {
          if (strcmp (animation_type_attr, types[i]) == 0)
            {
              animation_type = (AnimationType) i;
              break;
            }
        }

      if (i == G_N_ELEMENTS (types))
        {
          set_attribute_error (error, "gpa:animation-type", animation_type_attr);
          goto cleanup;
        }
    }

  animation_direction = ANIMATION_DIRECTION_NORMAL;
  if (animation_direction_attr)
    {
      const char *directions[] = { "normal", "alternate", "reverse",
        "reverse-alternate", "in-out", "in-out-alternate", "in-out-reverse",
        "segment", "segment-alternate"
      };
      guint i;

      for (i = 0; i < G_N_ELEMENTS (directions); i++)
        {
          if (strcmp (animation_direction_attr, directions[i]) == 0)
            {
              animation_direction = (AnimationDirection) i;
              break;
            }
        }

      if (i == G_N_ELEMENTS (directions))
        {
          set_attribute_error (error, "gpa:animation-direction", animation_direction_attr);
          goto cleanup;
        }
    }

  animation_duration = 0.f;
  if (animation_duration_attr)
    {
      if (!parse_float ("gpa:animation-duration", animation_duration_attr, POSITIVE, &animation_duration, error))
        goto cleanup;
    }

  animation_easing = EASING_FUNCTION_LINEAR;
  if (animation_easing_attr)
    {
      const char *easing[] =  { "linear", "ease-in-out", "ease-in",
        "ease-out", "ease"
      };
      guint i;

      for (i = 0; i < G_N_ELEMENTS (easing); i++)
        {
          if (strcmp (animation_easing_attr, easing[i]) == 0)
            {
              animation_easing = (EasingFunction) i;
              break;
            }
        }

      if (i == G_N_ELEMENTS (easing))
        {
          set_attribute_error (error, "gpa:animation-easing", animation_easing_attr);
          goto cleanup;
        }
    }

  animation_segment = 0.2f;
  if (animation_segment_attr)
    {
      if (!parse_float ("gpa:animation-segment", animation_segment_attr, POSITIVE, &animation_segment, error))
        goto cleanup;
    }

  attach_to = (gsize) -1;
  if (attach_to_attr)
    {
      gpointer value;

      /* Note that we avoid cycles by only allowing to attach
       * to an earlier path.
       */
      if (!g_hash_table_lookup_extended (data->paths, attach_to_attr, NULL, &value))
        {
          set_attribute_error (error, "gpa:attach-to", attach_to_attr);
          goto cleanup;
        }
      attach_to = GPOINTER_TO_UINT (value);
    }

  attach_pos = 0;
  if (attach_pos_attr)
    {
      if (!origin_parse (attach_pos_attr, &attach_pos))
        {
          set_attribute_error (error, "gpa:attach-pos", attach_pos_attr);
          goto cleanup;
        }
    }

  idx = path_paintable_add_path (data->paintable, path);

  path_paintable_set_path_states (data->paintable, idx, states);
  path_paintable_set_path_animation (data->paintable, idx, animation_type, animation_direction, animation_duration, animation_easing, animation_segment);
  path_paintable_set_path_transition (data->paintable, idx, transition_type, transition_duration, transition_easing);
  path_paintable_set_path_origin (data->paintable, idx, origin);
  path_paintable_set_path_fill (data->paintable, idx, fill_attr != NULL, fill_rule, fill_symbolic, &fill_color);
  path_paintable_set_path_stroke (data->paintable, idx, stroke_attr != NULL, stroke, stroke_symbolic, &stroke_color);
  path_paintable_set_path_stroke_variation (data->paintable, idx, min_stroke_width, max_stroke_width);
  path_paintable_attach_path (data->paintable, idx, attach_to, attach_pos);

  if (id_attr)
    g_hash_table_insert (data->paths, g_strdup (id_attr), GUINT_TO_POINTER (idx));

cleanup:
  g_clear_pointer (&path, gsk_path_unref);
  g_clear_pointer (&stroke, gsk_stroke_free);
}

static void
end_element_cb (GMarkupParseContext  *context,
                const gchar          *element_name,
                gpointer              user_data,
                GError              **error)
{
}

gboolean
parse_symbolic_svg (PathPaintable  *paintable,
                    GBytes         *bytes,
                    GError        **error)
{
  ParserData data;
  GMarkupParseContext *context;
  GMarkupParser parser = {
    start_element_cb,
    end_element_cb,
    NULL,
    NULL,
    NULL,
  };
  const char *text;
  gsize length;
  gboolean ret;

  data.paintable = paintable;
  data.paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  text = g_bytes_get_data (bytes, &length);

  context = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &data, NULL);
  ret = g_markup_parse_context_parse (context, text, length, error);

  g_markup_parse_context_free (context);

  g_hash_table_unref (data.paths);

  return ret;
}

/* }}} */
/* {{{ Serialization */

static void
path_paintable_save_path (PathPaintable *self,
                          gsize          idx,
                          GString       *str)
{
  const char *sym[] = { "foreground", "error", "warning", "success", "accent" };
  const char *easing[] = { "linear", "ease-in-out", "ease-in", "ease-out", "ease" };
  const char *fallback_color[] = { "rgb(0,0,0)", "rgb(255,0,0)", "rgb(255,255,0)", "rgb(0,255,0)", "rgb(0,0,255)", };
  GskStroke *stroke;
  guint stroke_symbolic;
  guint fill_symbolic;
  gboolean stroke_enabled;
  gboolean fill_enabled;
  GdkRGBA color;
  GskFillRule fill_rule;
  guint64 states;
  gsize to;
  float pos;
  GStrvBuilder *class_builder;
  GStrv class_strv;
  char *class_str;
  gboolean has_gtk_attr = FALSE;

  stroke = gsk_stroke_new (1);

  g_string_append (str, "  <path d='");
  gsk_path_print (path_paintable_get_path (self, idx), str);
  g_string_append (str, "'");

  g_string_append_printf (str, "\n        id='path%lu'", idx);

  class_builder = g_strv_builder_new ();

  states = path_paintable_get_path_states (self, idx);
  if (states != ALL_STATES)
    {
      char *s;

      s = states_to_string (states);
      g_string_append_printf (str, "\n        gpa:states='%s'", s);
      g_free (s);
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
      g_string_append_printf (str, "\n        gpa:animation-duration='%g'",
                              path_paintable_get_path_animation_duration (self, idx));
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_animation_easing (self, idx) != EASING_FUNCTION_LINEAR)
    {
      g_string_append_printf (str, "\n        gpa:animation-easing='%s'",
                              easing[path_paintable_get_path_animation_easing (self, idx)]);
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_animation_segment (self, idx) != 0.2)
    {
      g_string_append_printf (str, "\n        gpa:animation-segment='%g'",
                              path_paintable_get_path_animation_segment (self, idx));
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_transition_type (self, idx) != TRANSITION_TYPE_NONE)
    {
      const char *transition[] = { "none", "animate", "blur", "fade" };

      g_string_append_printf (str, "\n        gpa:transition-type='%s'", transition[path_paintable_get_path_transition_type (self, idx)]);
      has_gtk_attr = TRUE;
    }

  if (path_paintable_get_path_transition_duration (self, idx) != 0)
    {
      g_string_append_printf (str, "\n        gpa:transition-duration='%g'", path_paintable_get_path_transition_duration (self, idx));
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
      g_string_append_printf (str, "\n        gpa:origin='%g'",
                              path_paintable_get_path_origin (self, idx));
      has_gtk_attr = TRUE;
    }

  path_paintable_get_attach_path (self, idx, &to, &pos);
  if (to != (gsize) -1)
    {
      g_string_append_printf (str, "\n        gpa:attach-to='path%lu'", to);
      g_string_append_printf (str, "\n        gpa:attach-pos='%g'", pos);
      has_gtk_attr = TRUE;
    }

  if ((stroke_enabled = path_paintable_get_path_stroke (self, idx, stroke, &stroke_symbolic, &color)))
    {
      const char *linecap[] = { "butt", "round", "square" };
      const char *linejoin[] = { "miter", "round", "bevel" };

      g_string_append_printf (str, "\n        stroke-width='%g'", gsk_stroke_get_line_width (stroke));
      g_string_append_printf (str, "\n        stroke-linecap='%s'", linecap[gsk_stroke_get_line_cap (stroke)]);
      g_string_append_printf (str, "\n        stroke-linejoin='%s'", linejoin[gsk_stroke_get_line_join (stroke)]);

      if (stroke_symbolic == 0xffff)
        {
          char *s = gdk_rgba_to_string (&color);
          g_string_append_printf (str, "\n        stroke='%s'", s);
          g_string_append_printf (str, "\n        gpa:stroke='%s'", s);
          g_free (s);
          has_gtk_attr = TRUE;
        }
      else if (stroke_symbolic <= GTK_SYMBOLIC_COLOR_ACCENT)
        {
          if (color.alpha < 1)
            g_string_append_printf (str, "\n        stroke-opacity='%g'", color.alpha);
          g_string_append_printf (str, "\n        stroke='%s'", fallback_color[stroke_symbolic]);
          if (stroke_symbolic < GTK_SYMBOLIC_COLOR_ACCENT)
            g_strv_builder_take (class_builder, g_strdup_printf ("%s-stroke", sym[stroke_symbolic]));
          else
            has_gtk_attr = TRUE;
        }
    }
  else
    {
      g_string_append (str, "\n        stroke='none'");
    }

  if ((fill_enabled = path_paintable_get_path_fill (self, idx, &fill_rule, &fill_symbolic, &color)))
    {
      const char *rule[] = { "winding", "evenodd" };

      g_string_append_printf (str, "\n        fill-rule='%s'", rule[fill_rule]);

      if (fill_symbolic == 0xffff)
        {
          char *s = gdk_rgba_to_string (&color);
          g_string_append_printf (str, "\n        fill='%s'", s);
          g_string_append_printf (str, "\n        gpa:fill='%s'", s);
          g_free (s);
          has_gtk_attr = TRUE;
        }
      else if (fill_symbolic <= GTK_SYMBOLIC_COLOR_ACCENT)
        {
          if (color.alpha < 1)
            g_string_append_printf (str, "\n        fill-opacity='%g'", color.alpha);
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
                     guint          state)
{
  GStrv keywords;

  g_string_append_printf (str, "<svg width='%g' height='%g'",
                          path_paintable_get_width (self),
                          path_paintable_get_height (self));
  g_string_append (str, "\n     xmlns:gpa='https://www.gtk.org/grappa'");

  g_string_append (str, "\n     gpa:version='1'");

  keywords = path_paintable_get_keywords (self);
  if (keywords)
    {
      g_string_append (str,      "\n     gpa:keywords='");
      for (int i = 0; keywords[i]; i++)
        {
          if (i > 0)
            g_string_append_c (str, ' ');
          g_string_append (str, keywords[i]);
        }
      g_string_append_c (str, '\'');
    }

  if (path_paintable_get_state (self) != STATE_UNSET)
    {
      g_string_append_printf (str,      "\n     gpa:state='%u'",
                              path_paintable_get_state (self));
    }

  g_string_append (str, ">\n");

  for (gsize idx = 0; idx < path_paintable_get_n_paths (self); idx++)
    {
      guint64 states = path_paintable_get_path_states (self, idx);
      if (state == STATE_UNSET || (states & (G_GUINT64_CONSTANT (1) << state)) != 0)
        path_paintable_save_path (self, idx, str);
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
path_paintable_new_from_resource (const char *path)
{
  g_autoptr (GBytes) bytes = g_resources_lookup_data (path, 0, NULL);
  g_autoptr (GError) error = NULL;
  PathPaintable *res;

  if (!bytes)
    g_error ("Resource %s not found", path);

  res = path_paintable_new_from_bytes (bytes, &error);
  if (!res)
    g_error ("Failed to parse %s: %s", path, error->message);

  return res;
}

GBytes *
path_paintable_serialize (PathPaintable *self)
{
  GString *str = g_string_new ("");

  path_paintable_save (self, str, STATE_UNSET);

  return g_string_free_to_bytes (str);
}

/* }}} */

/* vim:set foldmethod=marker: */
