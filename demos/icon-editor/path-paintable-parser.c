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

#include "path-paintable-private.h"

#include <gtk/gtk.h>


/* This is a parser for a tiny subset of SVG with some extensions
 * in the gtk namespace to support the features of PathPaintable.
 *
 * <svg>
 *   width, height: The intrinsic size of the paintable
 *   gtk:duration: Transition duration (in seconds)
 *   gtk:delay: Transition delay (in seconds)
 *   gtk:easing: Easing function for transitions (one of
 *     'linear', 'ease-in-out', 'ease-in', 'ease-out' or 'ease')
 *
 * <circle>
 *   cx, cy, r: Circle parameters
 *
 * <rect>
 *   x, y, width, height, rx, ry: Rect parameters (possibly rounded)
 *
 * <path>
 *   d: the path
 *
 * <circle>, <rect>, <path>
 *   id: ID for the path
 *   stroke-width, stroke-opacity,
 *   stroke-linecap, stroke-linejoin: Stroke parameters
 *   fill-opacity, fill-rule: Fill parameters
 *   stroke: Ignored in favor of gtk:stroke
 *   gtk:stroke: Stroke paint. Either a symbolic color name
 *     ('foreground', 'success', 'warning', 'error'),
 *     or a fixed color in the format parsed by gdk_rgba_parse
 *   fill: Ignored in favor of gtk:fill
 *   gtk:fill: Fill paint
 *   gtk:states: A comma-separated list of numbers
 *   gtk:transition: The transition to use. One of 'none',
 *     'animate' or 'blur'
 *   gtk:origin: Where to start the animation. One of 'start',
 *     'middle' or 'end', or a number between 0 and 1
 *   gtk:attach-to: the ID of another path to attach to
 *   gtk:attach-pos: Where to attach the path. One of 'start',
 *     'middle' or 'end' or a number between 0 and 1
 *
 * For backwards compatibility, in the absence of any of the
 * gtk-namespaced attributes, the parser interprets the class
 * attribute for compatibility with traditional symbolic SVG icons.
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
  GString *str = g_string_new ("");

  if (states != ALL_STATES)
    {
      for (guint u = 0; u < 64; u++)
        {
          if ((states & (1ull << u)) != 0)
            {
              if (str->len > 0)
                g_string_append_c (str, ',');
              g_string_append_printf (str, "%u", u);
            }
        }
    }

  return g_string_free (str, FALSE);
}

gboolean
states_parse (const char *text,
              guint64    *states)
{
  g_auto (GStrv) str = NULL;

  if (strcmp (text, "") == 0 ||
      strcmp (text, "all") == 0)
    {
      *states = ALL_STATES;
      return TRUE;
    }

  *states = 0;

  str = g_strsplit (text, ",", 0);
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

      *states |= (1ull << u);
    }

  return TRUE;
}

char *
origin_to_string (float origin)
{
  if (origin == 0)
    return g_strdup ("start");
  else if (origin == 0.5)
    return g_strdup ("middle");
  else if (origin == 1)
    return g_strdup ("end");
  else
    return g_strdup_printf ("%g", origin);
}

gboolean
origin_parse (const char *text,
              float      *origin)
{
  if (strcmp (text, "start") == 0)
    *origin = 0;
  else if (strcmp (text, "middle") == 0)
    *origin = 0.5;
  else if (strcmp (text, "end") == 0)
    *origin = 1;
  else
    {
      char *end;

      *origin = (float) g_ascii_strtod (text, &end);
      if ((end && *end != '\0') || *origin < 0 || *origin > 1)
        return FALSE;
    }

  return TRUE;
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
  const char *stroke_opacity_attr = NULL;
  const char *stroke_linecap_attr = NULL;
  const char *stroke_linejoin_attr = NULL;
  const char *fill_attr = NULL;
  const char *fill_rule_attr = NULL;
  const char *fill_opacity_attr = NULL;
  const char *states_attr = NULL;
  const char *origin_attr = NULL;
  const char *transition_attr = NULL;
  const char *id_attr = NULL;
  const char *attach_to_attr = NULL;
  const char *attach_pos_attr = NULL;
  g_autoptr (GskPath) path = NULL;
  g_autoptr (GskStroke) stroke = NULL;
  guint stroke_symbolic;
  GdkRGBA stroke_color;
  double stroke_opacity;
  guint fill_symbolic;
  GskFillRule fill_rule;
  GdkRGBA fill_color;
  double fill_opacity;
  char *end;
  guint64 states;
  StateTransition transition;
  float origin;
  gsize idx;
  gsize attach_to;
  float attach_pos;

  if (strcmp (element_name, "svg") == 0)
    {
      const char *width_attr = NULL;
      const char *height_attr = NULL;
      const char *duration_attr = NULL;
      const char *delay_attr = NULL;
      const char *easing_attr = NULL;
      double width, height;

      markup_filter_attributes (element_name,
                                attribute_names,
                                attribute_values,
                                "width", &width_attr,
                                "height", &height_attr,
                                "gtk:duration", &duration_attr,
                                "gtk:delay", &delay_attr,
                                "gtk:easing", &easing_attr,
                                NULL);

      if (width_attr == NULL)
        {
          set_missing_attribute_error (error, "width");
          return;
        }

      width = g_ascii_strtod (width_attr, &end);
      if (end && *end != '\0' && strcmp (end, "px") != 0)
        {
          set_attribute_error (error, "width", width_attr);
          return;
        }

      if (height_attr == NULL)
        {
          set_missing_attribute_error (error, "height");
          return;
        }

      height = g_ascii_strtod (height_attr, &end);
      if (end && *end != '\0' && strcmp (end, "px") != 0)
        {
          set_attribute_error (error, "height", height_attr);
          return;
        }

      path_paintable_set_size (data->paintable, width, height);

      if (duration_attr)
        {
          float duration;

          duration = g_ascii_strtod (duration_attr, &end);
          if ((end && *end != '\0') || duration < 0)
            {
              set_attribute_error (error, "gtk:duration", duration_attr);
              return;
            }

          path_paintable_set_duration (data->paintable, duration);
        }

      if (delay_attr)
        {
          float delay;

          delay = g_ascii_strtod (delay_attr, &end);
          if ((end && *end != '\0') || delay < 0)
            {
              set_attribute_error (error, "gtk:delay", delay_attr);
              return;
            }

          path_paintable_set_delay (data->paintable, delay);
        }

      if (easing_attr)
        {
          if (strcmp (easing_attr, "linear") == 0)
            path_paintable_set_easing (data->paintable, EASING_FUNCTION_LINEAR);
          else if (strcmp (easing_attr, "ease-in-out") == 0)
            path_paintable_set_easing (data->paintable, EASING_FUNCTION_EASE_IN_OUT);
          else if (strcmp (easing_attr, "ease-in") == 0)
            path_paintable_set_easing (data->paintable, EASING_FUNCTION_EASE_IN);
          else if (strcmp (easing_attr, "ease-out") == 0)
            path_paintable_set_easing (data->paintable, EASING_FUNCTION_EASE_OUT);
          else if (strcmp (easing_attr, "ease") == 0)
            path_paintable_set_easing (data->paintable, EASING_FUNCTION_EASE);
          else
            {
              set_attribute_error (error, "gtk:easing", easing_attr);
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
          cx = g_ascii_strtod (cx_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "cx", cx_attr);
              return;
            }
        }

      if (cy_attr)
        {
          cy = g_ascii_strtod (cy_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "cy", cy_attr);
              return;
            }
        }

      if (r_attr)
        {
          r = g_ascii_strtod (r_attr, &end);
          if ((end && *end != '\0') || r < 0)
            {
              set_attribute_error (error, "r", r_attr);
              return;
            }
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
          x = g_ascii_strtod (x_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "x", x_attr);
              return;
            }
        }

      if (y_attr)
        {
          y = g_ascii_strtod (y_attr, &end);
          if (end && *end != '\0')
            {
              set_attribute_error (error, "y", y_attr);
              return;
            }
        }

      if (width_attr)
        {
          width = g_ascii_strtod (width_attr, &end);
          if ((end && *end != '\0') || width < 0)
            {
              set_attribute_error (error, "width", width_attr);
              return;
            }
        }

      if (height_attr)
        {
          height = g_ascii_strtod (height_attr, &end);
          if ((end && *end != '\0') || height < 0)
            {
              set_attribute_error (error, "height", height_attr);
              return;
            }
        }

      if (width == 0 || height == 0)
        return;  /* nothing to do */

      if (rx_attr)
        {
          rx = g_ascii_strtod (rx_attr, &end);
          if ((end && *end != '\0') || rx < 0)
            {
              set_attribute_error (error, "rx", rx_attr);
              return;
            }
        }

     if (ry_attr)
        {
          ry = g_ascii_strtod (ry_attr, &end);
          if ((end && *end != '\0') || ry < 0)
            {
              set_attribute_error (error, "ry", ry_attr);
              return;
            }
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
                            "gtk:fill", &fill_attr,
                            "gtk:stroke", &stroke_attr,
                            "gtk:states", &states_attr,
                            "gtk:transition", &transition_attr,
                            "gtk:origin", &origin_attr,
                            "gtk:attach-to", &attach_to_attr,
                            "gtk:attach-pos", &attach_pos_attr,
                            "class", &class_attr,
                            NULL);

  if (!stroke_attr &&
      !fill_attr &&
      !states_attr &&
      !transition_attr &&
      !origin_attr &&
      !attach_to_attr &&
      !attach_pos_attr)
    {
      /* backwards compat with traditional symbolic svg */
      if (class_attr)
        {
          const char * const *classes;

          classes = (const char * const *) g_strsplit (class_attr, " ", 0);

          if (g_strv_contains (classes, "transparent-fill"))
            fill_attr = NULL;
          else if (g_strv_contains (classes, "foreground-fill"))
            fill_attr = "foreground";
          else if (g_strv_contains (classes, "success") ||
                   g_strv_contains (classes, "success-fill"))
            fill_attr = "success";
          else if (g_strv_contains (classes, "warning") ||
                   g_strv_contains (classes, "warning-fill"))
            fill_attr = "warning";
          else if (g_strv_contains (classes, "error") ||
                   g_strv_contains (classes, "error-fill"))
            fill_attr = "error";
          else
            fill_attr = "foreground";

          if (g_strv_contains (classes, "success-stroke"))
            stroke_attr = "success";
          else if (g_strv_contains (classes, "warning-stroke"))
            stroke_attr = "warning";
          else if (g_strv_contains (classes, "error-stroke"))
            stroke_attr = "error";
          else if (g_strv_contains (classes, "foreground-stroke"))
            stroke_attr = "foreground";

          g_strfreev ((char **) classes);

          if (stroke_attr)
            {
              if (!stroke_width_attr)
                stroke_width_attr = "2";

              if (!stroke_linecap_attr)
                stroke_linecap_attr = "round";

              if (!stroke_linejoin_attr)
                stroke_linejoin_attr = "round";
            }
        }
      else
        {
          fill_attr = "foreground";
        }
    }

  stroke_opacity = 1;
  if (stroke_opacity_attr)
    {
      stroke_opacity = g_ascii_strtod (stroke_opacity_attr, &end);
      if (end && *end != '\0')
        {
          set_attribute_error (error, "stroke-opacity", stroke_opacity_attr);
          return;
        }
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
      else
        {
          if (!gdk_rgba_parse (&stroke_color, stroke_attr))
            {
              set_attribute_error (error, "gtk:stroke", stroke_attr);
              return;
            }
       }
    }

  stroke_color.alpha *= stroke_opacity;

  stroke = gsk_stroke_new (2);
  gsk_stroke_set_line_cap (stroke, GSK_LINE_CAP_ROUND);
  gsk_stroke_set_line_join (stroke, GSK_LINE_JOIN_ROUND);

  if (stroke_width_attr)
    {
      double w = g_ascii_strtod (stroke_width_attr, &end);
      if ((end && *end != '\0') || w < 0)
        {
          set_attribute_error (error, "stroke-width", stroke_width_attr);
          return;
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
          return;
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
          return;
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
          return;
        }
    }

  fill_opacity = 1;
  if (fill_opacity_attr)
    {
      fill_opacity = g_ascii_strtod (fill_opacity_attr, &end);
      if (end && *end != '\0')
        {
          set_attribute_error (error, "fill-opacity", fill_opacity_attr);
          return;
        }
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
      else
        {
          if (!gdk_rgba_parse (&fill_color, fill_attr))
            {
              set_attribute_error (error, "gtk:fill", fill_attr);
              return;
            }
       }
    }

  fill_color.alpha *= fill_opacity;

  transition = STATE_TRANSITION_ANIMATE;
  if (transition_attr)
    {
      if (strcmp (transition_attr, "none") == 0)
        transition = STATE_TRANSITION_NONE;
      else if (strcmp (transition_attr, "animate") == 0)
        transition = STATE_TRANSITION_ANIMATE;
      else if (strcmp (transition_attr, "blur") == 0)
        transition = STATE_TRANSITION_BLUR;
      else
        {
          set_attribute_error (error, "gtk:transition", transition_attr);
          return;
        }
    }

  origin = 0;
  if (origin_attr)
    {
      if (!origin_parse (origin_attr, &origin))
        {
          set_attribute_error (error, "gtk:origin", origin_attr);
          return;
        }
    }

  states = ALL_STATES;
  if (states_attr)
    {
      if (!states_parse (states_attr, &states))
        {
          set_attribute_error (error, "gtk:states", states_attr);
          return;
        }
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
          set_attribute_error (error, "gtk:attach-to", attach_to_attr);
          return;
        }
      attach_to = GPOINTER_TO_UINT (value);
    }

  attach_pos = 0;
  if (attach_pos_attr)
    {
      if (!origin_parse (attach_pos_attr, &origin))
        {
          set_attribute_error (error, "gtk:attach-pos", attach_pos_attr);
          return;
        }
    }

  idx = path_paintable_add_path (data->paintable, path);

  path_paintable_set_path_states (data->paintable, idx, states);
  path_paintable_set_path_transition (data->paintable, idx, transition);
  path_paintable_set_path_origin (data->paintable, idx, origin);
  path_paintable_set_path_fill (data->paintable, idx, fill_attr != NULL, fill_rule, fill_symbolic, &fill_color);
  path_paintable_set_path_stroke (data->paintable, idx, stroke_attr != NULL, stroke, stroke_symbolic, &stroke_color);
  path_paintable_attach_path (data->paintable, idx, attach_to, attach_pos);

  if (id_attr)
    g_hash_table_insert (data->paths, g_strdup (id_attr), GUINT_TO_POINTER (idx));
}

static void
end_element_cb (GMarkupParseContext  *context,
                const gchar          *element_name,
                gpointer              user_data,
                GError              **error)
{
}

static PathPaintable *
parse_symbolic_svg (GBytes  *bytes,
                    GError **error)
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

  data.paintable = path_paintable_new ();
  data.paths = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

  text = g_bytes_get_data (bytes, &length);

  context = g_markup_parse_context_new (&parser, G_MARKUP_PREFIX_ERROR_POSITION, &data, NULL);
  if (!g_markup_parse_context_parse (context, text, length, error))
    g_clear_object (&data.paintable);

  g_markup_parse_context_free (context);
  g_hash_table_unref (data.paths);

  return data.paintable;
}

/* }}} */
/* {{{ Public API */

PathPaintable *
path_paintable_new_from_bytes (GBytes  *bytes,
                               GError **error)
{
  return parse_symbolic_svg (bytes, error);
}

/* }}} */

/* vim:set foldmethod=marker: */
