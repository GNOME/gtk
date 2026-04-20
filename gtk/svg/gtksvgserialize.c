/*
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksvgprivate.h"
#include "gtksvgserializeprivate.h"

#include "gtkenums.h"
#include "gtksymbolicpaintable.h"
#include "gsk/gskpathprivate.h"
#include <glib/gstdio.h>
#include "gtksvgstringutilsprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgtransformprivate.h"
#include "gtksvgpaintprivate.h"
#include "gtksvgviewboxprivate.h"
#include "gtksvglocationprivate.h"
#include "gtksvgelementtypeprivate.h"
#include "gtksvgpropertyprivate.h"
#include "gtksvgfiltertypeprivate.h"
#include "gtksvgfilterprivate.h"
#include "gtksvgcolorstopprivate.h"
#include "gtksvgelementinternal.h"
#include "gtksvganimationprivate.h"
#include "gtksvgkeywordprivate.h"
#include "gtksvggpaprivate.h"
#include "gtksvgtimespecprivate.h"
#include "gtksvgparserprivate.h"

#include <stdint.h>


/* We don't aim for 100% accurate reproduction here, so
 * we allow values to be normalized, and we don't necessarily
 * preserve explicitly set default values. SvgAnimations are
 * always emitted as children of the shape they belong to,
 * regardless of where they were placed in the original svg.
 *
 * In addition to the original DOM values, we allow serializing
 * 'snapshots' of a running animation at a given time, which
 * is very useful for tests. When doing so, we can also write
 * out some internal state in custom attributes, which is,
 * again, useful for tests.
 */

#define BASE_INDENT 2
#define ATTR_INDENT 8

/* copied from gtksymbolicpaintable.c */
static const GdkRGBA *
pad_colors (GdkRGBA        col[5],
            const GdkRGBA *colors,
            unsigned int   n_colors)
{
  GdkRGBA default_colors[5] = {
    [GTK_SYMBOLIC_COLOR_FOREGROUND] = { 0.745, 0.745, 0.745, 1.0 },
    [GTK_SYMBOLIC_COLOR_ERROR] = { 0.797, 0, 0, 1.0 },
    [GTK_SYMBOLIC_COLOR_WARNING] = { 0.957, 0.473, 0.242, 1.0 },
    [GTK_SYMBOLIC_COLOR_SUCCESS] = { 0.305, 0.602, 0.023, 1.0 },
    [GTK_SYMBOLIC_COLOR_ACCENT] = { 0.208, 0.518, 0.894, 1.0 },
  };

  memcpy (col, default_colors, sizeof (GdkRGBA) * 5);

  if (n_colors != 0)
    memcpy (col, colors, sizeof (GdkRGBA) * MIN (5, n_colors));

  return col;
}

static void
append_string_attr (GString    *s,
                    int         indent,
                    const char *name,
                    const char *value)
{
  string_indent (s, indent + ATTR_INDENT);
  g_string_append (s, name);
  g_string_append (s, "='");
  string_append_escaped (s, value);
  g_string_append_c (s, '\'');
}

static void
append_double_attr (GString    *s,
                    int         indent,
                    const char *name,
                    double      value,
                    const char *units)
{
  string_indent (s, indent + ATTR_INDENT);
  g_string_append (s, name);
  string_append_double (s, "='", value);
  if (units)
    g_string_append (s, units);
  g_string_append_c (s, '\'');
}

static void
append_time_attr (GString    *s,
                  int         indent,
                  const char *name,
                  int64_t     time)
{
  append_double_attr (s, indent, name, time / (double) G_TIME_SPAN_MILLISECOND, "ms");
}

static void
serialize_shape_attrs (GString              *s,
                       GtkSvg               *svg,
                       int                   indent,
                       SvgElement           *shape,
                       GtkSvgSerializeFlags  flags)
{
  GtkSvgLocation loc;

  if (svg_element_get_id (shape))
    append_string_attr (s, indent, "id", svg_element_get_id (shape));

  if (svg_element_get_classes (shape))
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "class='");
      string_append_strv (s, svg_element_get_classes (shape));
      g_string_append_c (s, '\'');
    }

  if (svg_element_get_style (shape, &loc))
    append_string_attr (s, indent, "style", svg_element_get_style (shape, &loc));

  if (svg_element_get_focusable (shape) != svg_element_get_initial_focusable (shape))
    append_string_attr (s, indent, "tabindex", svg_element_get_focusable (shape) ? "0" : "-1");

  if (svg_element_get_autofocus (shape))
    append_string_attr (s, indent, "autofocus", "autofocus");

  for (SvgProperty attr = FIRST_SHAPE_PROPERTY; attr <= LAST_SHAPE_PROPERTY; attr++)
    {
      if ((flags & GTK_SVG_SERIALIZE_NO_COMPAT) == 0 &&
          svg->gpa_version > 0 &&
          svg_element_type_is_path (svg_element_get_type (shape)) &&
          attr == SVG_PROPERTY_VISIBILITY)
        {
          unsigned int state;

          if (svg->gpa_version == 0 &&
              (attr == SVG_PROPERTY_STROKE_MINWIDTH ||
               attr == SVG_PROPERTY_STROKE_MAXWIDTH))
            continue;

          if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
            state = svg->state;
          else
            state = svg->initial_state;

          if ((svg_element_get_states (shape) & BIT (state)) == 0)
            {
              append_string_attr (s, indent, "visibility", "hidden");
              continue;
            }
        }

      if (svg_element_is_specified (shape, attr) ||
          (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME))
        {
          SvgValue *value, *initial;

          if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
            value = svg_element_get_current_value (shape, attr);
          else
            value = svg_element_get_specified_value (shape, attr);

          initial = svg_property_ref_initial_value (attr, svg_element_get_type (shape), svg_element_get_parent (shape) != NULL);

          if (value && (svg_element_is_specified (shape, attr) || !svg_value_equal (value, initial)))
            {
              if (svg_property_has_presentation (attr))
                {
                  string_indent (s, indent + ATTR_INDENT);
                  g_string_append_printf (s, "%s='", svg_property_get_presentation (attr, svg_element_get_type (shape)));
                  svg_value_print (value, s);
                  g_string_append_c (s, '\'');
                }
            }

          svg_value_unref (initial);
        }
    }
}

static void
serialize_gpa_attrs (GString              *s,
                     GtkSvg               *svg,
                     int                   indent,
                     SvgElement           *shape,
                     GtkSvgSerializeFlags  flags)
{
  if (svg->gpa_version == 0 || !svg_element_type_is_path (svg_element_get_type (shape)))
    return;

  if (svg_element_get_gpa_stroke (shape))
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "gpa:stroke='");
      svg_paint_print_gpa (svg_element_get_gpa_stroke (shape), s);
      g_string_append_c (s, '\'');
    }

  if (svg_element_get_gpa_fill (shape))
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "gpa:fill='");
      svg_paint_print_gpa (svg_element_get_gpa_fill (shape), s);
      g_string_append_c (s, '\'');
    }

  if (svg_element_get_states (shape) != ALL_STATES)
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "gpa:states='");
      print_states (s, svg, svg_element_get_states (shape));
      g_string_append_c (s, '\'');
    }

  if ((flags & GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS) == 0)
    {
      GpaTransition transition;
      GpaAnimation animation;
      GpaEasing easing;
      int64_t duration;
      int64_t delay;
      double repeat;
      double segment;
      const char *id;
      double pos;

      svg_element_get_gpa_transition (shape, &transition, &easing, &duration, &delay);
      if (transition != GPA_TRANSITION_NONE)
        {
          const char *names[] = { "none", "animate", "morph", "fade" };
          append_string_attr (s, indent, "gpa:transition-type", names[transition]);
        }

      if (easing != GPA_EASING_LINEAR)
        {
          const char *names[] = { "linear", "ease-in-out", "ease-in", "ease-out", "ease" };
          append_string_attr (s, indent, "gpa:transition-easing", names[easing]);
        }

      if (duration != 0)
        append_time_attr (s, indent, "gpa:transition-duration", duration);

      if (delay != 0)
        append_time_attr (s, indent, "gpa:transition-delay", delay);

      svg_element_get_gpa_animation (shape, &animation, &easing, &duration, &repeat, &segment);
      if (animation != GPA_ANIMATION_NONE)
        {
          const char *names[] = { "none", "normal", "alternate", "reverse",
            "reverse-alternate", "in-out", "in-out-alternate", "in-out-reverse",
            "segment", "segment-alternate" };
          append_string_attr (s, indent, "gpa:animation-type", "automatic");
          append_string_attr (s, indent, "gpa:animation-direction", names[animation]);
        }

      if (easing != GPA_EASING_LINEAR)
        {
          const char *names[] = { "linear", "ease-in-out", "ease-in", "ease-out", "ease" };
          append_string_attr (s, indent, "gpa:animation-easing", names[easing]);
        }

      if (duration != 0)
        append_time_attr (s, indent, "gpa:animation-duration", duration);

      if (repeat != REPEAT_FOREVER)
        append_double_attr (s, indent, "gpa:animation-repeat", repeat, NULL);

      if (segment != 0.2)
        append_double_attr (s, indent, "gpa:animation-segment", segment, NULL);

      if (svg_element_get_gpa_origin (shape) != 0)
        append_double_attr (s, indent, "gpa:origin", svg_element_get_gpa_origin (shape), NULL);

      svg_element_get_gpa_attachment (shape, &id, &pos, NULL);
      if (id)
        append_string_attr (s, indent, "gpa:attach-to", id);

      if (pos != 0)
        append_double_attr (s, indent, "gpa:attach-pos", pos, NULL);
    }
}

static void
serialize_base_animation_attrs (GString      *s,
                                GtkSvg       *svg,
                                int           indent,
                                SvgAnimation *a)
{
  if (a->id)
    append_string_attr (s, indent, "id", a->id);

  if (a->type != ANIMATION_TYPE_MOTION)
    append_string_attr (s, indent, "attributeName", svg_property_get_presentation (a->attr, svg_element_get_type (a->shape)));

  if (a->has_begin)
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "begin='");
      time_specs_print (a->begin, svg, s);
      g_string_append (s, "'");
    }

  if (a->has_end)
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "end='");
      time_specs_print (a->end, svg, s);
      g_string_append (s, "'");
    }

  if (a->has_simple_duration)
    {
      if (a->simple_duration == INDEFINITE)
        append_string_attr (s, indent, "dur", "indefinite");
      else
        append_time_attr (s, indent, "dur", a->simple_duration);
    }

  if (a->has_repeat_count)
    {
      if (a->repeat_count == REPEAT_FOREVER)
        append_string_attr (s, indent, "repeatCount", "indefinite");
      else
        append_double_attr (s, indent, "repeatCount", a->repeat_count, NULL);
    }

  if (a->has_repeat_duration)
    {
      if (a->repeat_duration == INDEFINITE)
        append_string_attr (s, indent, "repeatDur", "indefinite");
      else
        append_time_attr (s, indent, "repeatDur", a->repeat_duration);
    }

  if (a->fill != ANIMATION_FILL_REMOVE)
    {
      const char *fill[] = { "freeze", "remove" };
      append_string_attr (s, indent, "fill", fill[a->fill]);
    }

  if (a->restart != ANIMATION_RESTART_ALWAYS)
    {
      const char *restart[] = { "always", "whenNotActive", "never" };
      append_string_attr (s, indent, "restart", restart[a->restart]);
    }
}

static void
serialize_value_animation_attrs (GString      *s,
                                 GtkSvg       *svg,
                                 int           indent,
                                 SvgAnimation *a)
{
  if (a->type != ANIMATION_TYPE_MOTION)
    {
      if (a->n_frames == 2)
        {
          if (a->type != ANIMATION_TYPE_SET &&
              !svg_value_is_current (a->frames[0].value))
            {
              string_indent (s, indent + ATTR_INDENT);
              g_string_append (s, "from='");
              if (a->type == ANIMATION_TYPE_TRANSFORM && a->attr == SVG_PROPERTY_TRANSFORM)
                svg_primitive_transform_print (a->frames[0].value, s);
              else
                svg_value_print (a->frames[0].value, s);
              g_string_append (s, "'");
            }

          string_indent (s, indent + ATTR_INDENT);
          g_string_append (s, "to='");
          if (a->type == ANIMATION_TYPE_TRANSFORM && a->attr == SVG_PROPERTY_TRANSFORM)
            svg_primitive_transform_print (a->frames[1].value, s);
          else
            svg_value_print (a->frames[1].value, s);
          g_string_append_c (s, '\'');
        }
      else
        {
          string_indent (s, indent + ATTR_INDENT);
          g_string_append (s, "values='");
          if (a->type == ANIMATION_TYPE_TRANSFORM && a->attr == SVG_PROPERTY_TRANSFORM)
            {
              for (unsigned int i = 0; i < a->n_frames; i++)
                {
                  if (i > 0)
                    g_string_append (s, "; ");

                  svg_primitive_transform_print (a->frames[i].value, s);
                }
            }
          else
            {
              for (unsigned int i = 0; i < a->n_frames; i++)
                {
                  if (i > 0)
                    g_string_append (s, "; ");
                  svg_value_print (a->frames[i].value, s);
                }
            }
          g_string_append_c (s, '\'');
        }
    }

  if (a->calc_mode == CALC_MODE_SPLINE)
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "keySplines='");
      for (unsigned int i = 0; i + 1 < a->n_frames; i++)
        {
          string_append_double (s, i > 0 ? "; " : "", a->frames[i].params[0]);
          string_append_double (s, " ", a->frames[i].params[1]);
          string_append_double (s, " ", a->frames[i].params[2]);
          string_append_double (s, " ", a->frames[i].params[3]);
        }
      g_string_append_c (s, '\'');
    }

  if (a->calc_mode != CALC_MODE_PACED)
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "keyTimes='");
      for (unsigned int i = 0; i < a->n_frames; i++)
        string_append_double (s, i > 0 ? "; " : "", a->frames[i].time);
      g_string_append_c (s, '\'');
    }

  if (a->calc_mode != svg_animation_type_default_calc_mode (a->type))
    {
      const char *modes[] = { "discrete", "linear", "paced", "spline" };
      append_string_attr (s, indent, "calcMode",  modes[a->calc_mode]);
    }

  if (a->additive != ANIMATION_ADDITIVE_REPLACE)
    {
      const char *additive[] = { "replace", "sum" };
      append_string_attr (s, indent, "additive", additive[a->additive]);
    }

  if (a->accumulate != ANIMATION_ACCUMULATE_NONE)
    {
      const char *accumulate[] = { "none", "sum" };
      append_string_attr (s, indent, "accumulate", accumulate[a->accumulate]);
    }

  if (a->color_interpolation != COLOR_INTERPOLATION_SRGB)
    {
      const char *vals[] = { "auto", "sRGB", "linearRGB" };
      append_string_attr (s, indent, "color-interpolation", vals[a->color_interpolation]);
    }
}

static void
serialize_animation_status (GString              *s,
                            GtkSvg               *svg,
                            int                   indent,
                            SvgAnimation         *a,
                            GtkSvgSerializeFlags  flags)
{
  if (flags & GTK_SVG_SERIALIZE_INCLUDE_STATE)
    {
      const char *status[] = { "inactive", "running", "done" };
      append_string_attr (s, indent, "gpa:status", status[a->status]);

      /* Not writing out start/end time, since that will be hard to compare */

      if (!a->has_simple_duration)
        {
          int64_t d = svg_animation_get_simple_duration (a);
          if (d == INDEFINITE)
            append_string_attr (s, indent, "gpa:computed-simple-duration", "indefinite");
          else
            append_time_attr (s, indent, "gpa:computed-simple-duration", d);
        }

      if (a->current.begin != INDEFINITE)
        append_time_attr (s, indent, "gpa:current-start-time", a->current.begin - svg->load_time);

      if (a->current.end != INDEFINITE)
        append_time_attr (s, indent, "gpa:current-end-time", a->current.end - svg->load_time);
    }
}

static void
serialize_animation_set (GString              *s,
                         GtkSvg               *svg,
                         int                   indent,
                         SvgAnimation         *a,
                         GtkSvgSerializeFlags  flags)
{
  string_indent (s, indent);
  g_string_append (s, "<set");
  serialize_base_animation_attrs (s, svg, indent, a);
  string_indent (s, indent + ATTR_INDENT);
  g_string_append (s, "to='");
  svg_value_print (a->frames[0].value, s);
  g_string_append_c (s, '\'');
  serialize_animation_status (s, svg, indent, a, flags);
  g_string_append (s, "/>");
}

static void
serialize_animation_animate (GString              *s,
                             GtkSvg               *svg,
                             int                   indent,
                             SvgAnimation         *a,
                             GtkSvgSerializeFlags  flags)
{
  string_indent (s, indent);
  g_string_append (s, "<animate");
  serialize_base_animation_attrs (s, svg, indent, a);
  serialize_value_animation_attrs (s, svg, indent, a);
  serialize_animation_status (s, svg, indent, a, flags);
  g_string_append (s, "/>");
}

static void
serialize_animation_transform (GString              *s,
                               GtkSvg               *svg,
                               int                   indent,
                               SvgAnimation         *a,
                               GtkSvgSerializeFlags  flags)
{
  const char *types[] = { "none", "translate", "scale", "rotate", "any" };

  string_indent (s, indent);
  g_string_append (s, "<animateTransform");
  serialize_base_animation_attrs (s, svg, indent, a);
  serialize_value_animation_attrs (s, svg, indent, a);
  append_string_attr (s, indent, "type", types[svg_transform_get_type (a->frames[0].value, 0)]);
  serialize_animation_status (s, svg, indent, a, flags);
  g_string_append (s, "/>");
}

static void
serialize_animation_motion (GString              *s,
                            GtkSvg               *svg,
                            int                   indent,
                            SvgAnimation         *a,
                            GtkSvgSerializeFlags  flags)
{
  string_indent (s, indent);
  g_string_append (s, "<animateMotion");
  serialize_base_animation_attrs (s, svg, indent, a);
  serialize_value_animation_attrs (s, svg, indent, a);

  if (a->calc_mode != CALC_MODE_PACED)
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "keyPoints='");
      for (unsigned int i = 0; i < a->n_frames; i++)
        string_append_double (s, i > 0 ? "; " : "", a->frames[i].point);
      g_string_append (s, "'");
    }

  if (a->motion.rotate != ROTATE_FIXED)
    {
      const char *values[] = { "auto", "auto-reverse" };
      append_string_attr (s, indent, "rotate", values[a->motion.rotate]);
    }
  else if (a->motion.angle != 0)
    append_double_attr (s, indent, "rotate", a->motion.angle, NULL);

  serialize_animation_status (s, svg, indent, a, flags);

  if (a->motion.path_shape)
    {
      g_string_append (s, ">");
      string_indent (s, indent + BASE_INDENT);
      g_string_append_printf (s, "<mpath href='#%s'/>", svg_element_get_id (a->motion.path_shape));
      string_indent (s, indent);
      g_string_append (s, "</animateMotion>");
    }
  else
    {
      GskPath *path;

      if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
        path = svg_animation_motion_get_current_path (a, NULL);
      else
        path = svg_animation_motion_get_base_path (a, NULL);
      if (path)
        {
          string_indent (s, indent + ATTR_INDENT);
          g_string_append (s, "path='");
          gsk_path_print (path, s);
          g_string_append_c (s, '\'');
          gsk_path_unref (path);
        }
      g_string_append (s, "/>");
    }
}

static void
serialize_animation (GString              *s,
                     GtkSvg               *svg,
                     int                   indent,
                     SvgAnimation         *a,
                     GtkSvgSerializeFlags  flags)
{
  if (flags & GTK_SVG_SERIALIZE_EXCLUDE_ANIMATION)
    return;

  if ((flags & GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS) == 0)
    {
      if (a->id && g_str_has_prefix (a->id, "gpa:"))
        return;
    }

  switch (a->type)
    {
    case ANIMATION_TYPE_SET:
      serialize_animation_set (s, svg, indent, a, flags);
      break;
    case ANIMATION_TYPE_ANIMATE:
      serialize_animation_animate (s, svg, indent, a, flags);
      break;
    case ANIMATION_TYPE_TRANSFORM:
      serialize_animation_transform (s, svg, indent, a, flags);
      break;
    case ANIMATION_TYPE_MOTION:
      serialize_animation_motion (s, svg, indent, a, flags);
      break;
    default:
      g_assert_not_reached ();
    }
}

static void
serialize_color_stop (GString              *s,
                      GtkSvg               *svg,
                      int                   indent,
                      SvgElement           *shape,
                      unsigned int          idx,
                      GtkSvgSerializeFlags  flags)
{
  SvgColorStop *stop;
  const char *names[] = { "offset", "stop-color", "stop-opacity" };

  stop = g_ptr_array_index (shape->color_stops, idx);

  string_indent (s, indent);
  g_string_append (s, "<stop");

  if (svg_color_stop_get_id (stop))
    append_string_attr (s, indent, "id", svg_color_stop_get_id (stop));

  if (svg_color_stop_get_classes (stop))
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "class='");
      string_append_strv (s, svg_color_stop_get_classes (stop));
      g_string_append_c (s, '\'');
    }

  if (svg_color_stop_get_style (stop))
    append_string_attr (s, indent, "style", svg_color_stop_get_style (stop));

  for (unsigned int i = 0; i < N_STOP_PROPERTIES; i++)
    {
      SvgProperty attr = svg_color_stop_get_property (i);
      SvgValue *value;

      if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
        value = svg_color_stop_get_current_value (stop, attr);
      else
        value = svg_color_stop_get_specified_value (stop, attr);

      if (value)
        {
          string_indent (s, indent + ATTR_INDENT);
          g_string_append_printf (s, "%s='", names[i]);
          svg_value_print (value, s);
          g_string_append (s, "'");
        }
    }
  g_string_append (s, ">");

  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          if (a->idx == idx + 1)
            serialize_animation (s, svg, indent + BASE_INDENT, a, flags);
        }
    }

  string_indent (s, indent);
  g_string_append (s, "</stop>");
}

static void
serialize_filter_begin (GString              *s,
                        GtkSvg               *svg,
                        int                   indent,
                        SvgElement           *shape,
                        SvgFilter            *f,
                        unsigned int          idx,
                        GtkSvgSerializeFlags  flags)
{
  SvgFilterType type = svg_filter_get_type (f);

  string_indent (s, indent);
  g_string_append_printf (s, "<%s", svg_filter_type_get_name (type));

  if (svg_filter_get_id (f))
    append_string_attr (s, indent, "id", svg_filter_get_id (f));

  if (svg_filter_get_classes (f))
    {
      string_indent (s, indent + ATTR_INDENT);
      g_string_append (s, "class='");
      string_append_strv (s, svg_filter_get_classes (f));
      g_string_append_c (s, '\'');
    }

  if (svg_filter_get_style (f))
    append_string_attr (s, indent, "style", svg_filter_get_style (f));

  for (unsigned int i = 0; i < svg_filter_type_get_n_attrs (type); i++)
    {
      SvgProperty attr = svg_filter_type_get_property (type, i);
      SvgValue *initial;
      SvgValue *value;

      initial = svg_filter_ref_initial_value (f, attr);
      if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
        value = svg_filter_get_current_value (f, attr);
      else
        value = svg_filter_get_specified_value (f, attr);

      if (value && !svg_value_equal (value, initial))
        {
          string_indent (s, indent + ATTR_INDENT);
          g_string_append_printf (s, "%s='", svg_property_get_presentation (attr, svg_element_get_type (shape)));
          svg_value_print (value, s);
          g_string_append (s, "'");
        }

      svg_value_unref (initial);
    }

  g_string_append (s, ">");

  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          if (a->idx == idx + 1)
            serialize_animation (s, svg, indent + BASE_INDENT, a, flags);
        }
    }
}

static void
serialize_filter_end (GString              *s,
                      GtkSvg               *svg,
                      int                   indent,
                      SvgElement           *shape,
                      SvgFilter            *f,
                      GtkSvgSerializeFlags  flags)
{
  SvgFilterType type = svg_filter_get_type (f);

  string_indent (s, indent);
  g_string_append_printf (s, "</%s>", svg_filter_type_get_name (type));
}

static void
serialize_shape (GString              *s,
                 GtkSvg               *svg,
                 int                   indent,
                 SvgElement           *shape,
                 GtkSvgSerializeFlags  flags)
{
  if (svg_element_get_type (shape) == SVG_ELEMENT_DEFS &&
      shape->shapes->len == 0)
    return;

  if (indent > 0) /* Hack: this is for <svg> */
    {
      string_indent (s, indent);
      g_string_append_printf (s, "<%s", svg_element_type_get_name (svg_element_get_type (shape)));
      serialize_shape_attrs (s, svg, indent, shape, flags);
      serialize_gpa_attrs (s, svg, indent, shape, flags);

      if (flags & GTK_SVG_SERIALIZE_INCLUDE_STATE)
        {
          GtkStateFlags state = gtk_css_node_get_state (svg_element_get_css_node (shape));
          struct {
            GtkStateFlags flag;
            const char *name;
          } checked_flags[] = {
            { GTK_STATE_FLAG_ACTIVE, "active" },
            { GTK_STATE_FLAG_PRELIGHT, "hover" },
            { GTK_STATE_FLAG_FOCUSED, "focused" },
            { GTK_STATE_FLAG_LINK, "link" },
            { GTK_STATE_FLAG_VISITED, "visited" },
          };
          gboolean started = FALSE;

          for (unsigned int i = 0; i < G_N_ELEMENTS (checked_flags); i++)
            {
              if (state & checked_flags[i].flag)
                {
                  if (!started)
                    {
                      string_indent (s, indent + ATTR_INDENT);
                      g_string_append_printf (s, "gpa:css-state='");
                    }
                  else
                    g_string_append_c (s, ',');
                  g_string_append (s, checked_flags[i].name);
                  started = TRUE;
                }
            }
          if (started)
            g_string_append_c (s, '\'');
        }

      g_string_append_c (s, '>');
    }

  if (svg_element_get_title (shape))
    {
      string_indent (s, indent + BASE_INDENT);
      g_string_append (s, "<title>");
      g_string_append (s, svg_element_get_title (shape));
      g_string_append (s, "</title>");
    }

  if (svg_element_get_description (shape))
    {
      string_indent (s, indent + BASE_INDENT);
      g_string_append (s, "<desc>");
      g_string_append (s, svg_element_get_description (shape));
      g_string_append (s, "</desc>");
    }

  for (unsigned int i = 0; i < shape->styles->len; i++)
    {
      StyleElt *elt = g_ptr_array_index (shape->styles, i);
      string_indent (s, indent + BASE_INDENT);
      g_string_append (s, "<style type='text/css'>");
      g_string_append (s, g_bytes_get_data (elt->content, NULL));
      g_string_append (s, "</style>");
    }

  if (svg_element_type_is_gradient (svg_element_get_type (shape)))
    {
      for (unsigned int idx = 0; idx < shape->color_stops->len; idx++)
        serialize_color_stop (s, svg, indent + BASE_INDENT, shape, idx, flags);
    }

  if (svg_element_type_is_filter (svg_element_get_type (shape)))
    {
      for (unsigned int idx = 0; idx < shape->filters->len; idx++)
        {
          SvgFilter *f = g_ptr_array_index (shape->filters, idx);

          serialize_filter_begin (s, svg, indent + BASE_INDENT, shape, f, idx, flags);

          if (svg_filter_get_type (f) == SVG_FILTER_MERGE)
            {
              for (idx++; idx < shape->filters->len; idx++)
                {
                  SvgFilter *f2 = g_ptr_array_index (shape->filters, idx);
                  if (svg_filter_get_type (f2) != SVG_FILTER_MERGE_NODE)
                    {
                      idx--;
                      break;
                    }

                  serialize_filter_begin (s, svg, indent + 2 * BASE_INDENT, shape, f2, idx, flags);
                  serialize_filter_end (s, svg, indent + 2 * BASE_INDENT, shape, f2, flags);
                }
            }

          if (svg_filter_get_type (f) == SVG_FILTER_COMPONENT_TRANSFER)
            {
              for (idx++; idx < shape->filters->len; idx++)
                {
                  SvgFilter *f2 = g_ptr_array_index (shape->filters, idx);
                  SvgFilterType t = svg_filter_get_type (f2);
                  if (t != SVG_FILTER_FUNC_R &&
                      t != SVG_FILTER_FUNC_G &&
                      t != SVG_FILTER_FUNC_B &&
                      t != SVG_FILTER_FUNC_A)
                    {
                      idx--;
                      break;
                    }

                  serialize_filter_begin (s, svg, indent + 2 * BASE_INDENT, shape, f2, idx, flags);
                  serialize_filter_end (s, svg, indent + 2 * BASE_INDENT, shape, f2, flags);
                }
            }

          serialize_filter_end (s, svg, indent + BASE_INDENT, shape, f, flags);
        }
    }

  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          if (a->idx == 0)
            serialize_animation (s, svg, indent + BASE_INDENT, a, flags);
        }
    }

  if (svg_element_type_is_text (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->text->len; i++)
        {
          TextNode *node = &g_array_index (shape->text, TextNode, i);
          switch (node->type)
            {
            case TEXT_NODE_SHAPE:
              serialize_shape (s, svg, indent + BASE_INDENT, node->shape.shape, flags);
              break;
            case TEXT_NODE_CHARACTERS:
              {
                char *escaped = g_markup_escape_text (node->characters.text, -1);
                g_string_append (s, escaped);
                g_free (escaped);
              }
              break;
            default:
              g_assert_not_reached ();
            }
        }
      g_string_append_printf (s, "</%s>", svg_element_type_get_name (svg_element_get_type (shape)));
      return;
    }
  else if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          serialize_shape (s, svg, indent + BASE_INDENT, sh, flags);
        }
    }

  if (indent > 0)
    {
      string_indent (s, indent);
      g_string_append_printf (s, "</%s>", svg_element_type_get_name (svg_element_get_type (shape)));
    }
}

/*< private >
 * gtk_svg_serialize_full:
 * @self: an SVG paintable
 * @colors (array length=n_colors): a pointer to an array of colors
 * @n_colors: the number of colors
 * @flags: flags that influence what content is
 *   included in the serialization
 *
 * Serializes the content of the renderer as SVG.
 *
 * The SVG will be similar to the orignally loaded one,
 * but is not guaranteed to be 100% identical.
 *
 * The @flags argument can be used together with
 * [method@Gtk.Svg.advance] to reproduce content
 * for specific times. For producing still images that
 * represent individual frames of the animation, you
 * have to exclude the animation elements, since their
 * values would otherwise interfere.
 *
 * Note that the 'shadow DOM' optionally includes some
 * custom attributes that represent internal state, e.g.
 * a 'gpa:status' attribute on animation elements that
 * reflects whether they are currently running.
 *
 * When serializing the 'shadow DOM', the viewport from the
 * most recently drawn frame is used to resolve percentages.
 *
 * Returns: the serialized contents
 *
 * Since: 4.22
 */
GBytes *
gtk_svg_serialize_full (GtkSvg               *self,
                        const GdkRGBA        *colors,
                        size_t                n_colors,
                        GtkSvgSerializeFlags  flags)
{
  GString *s = g_string_new ("");

  if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
    {
      graphene_rect_t viewport;
      GdkRGBA real_colors[5];
      const GdkRGBA *col = colors;
      size_t n_col = n_colors;
      SvgComputeContext context;

      if (n_colors >= 5)
        {
          col = colors;
          n_col = n_colors;
        }
      else
        {
          col = pad_colors (real_colors, colors, n_colors);
          n_col = 5;
        }

      viewport = GRAPHENE_RECT_INIT (0, 0, self->current_width, self->current_height);

      if (self->style_changed)
        {
          apply_styles_to_shape (self->content, self);
          self->style_changed = FALSE;
        }

      if (self->view_changed)
        {
          apply_view (self->content, self->view);
          self->view_changed = FALSE;
        }

      context.svg = self;
      context.viewport = &viewport;
      context.current_time = self->current_time;
      context.parent = NULL;
      context.colors = col;
      context.n_colors = n_col;
      context.interpolation = GDK_COLOR_STATE_SRGB;

      compute_current_values_for_shape (self->content, &context);
    }

  g_string_append (s, "<svg");

  string_indent (s, ATTR_INDENT);
  g_string_append (s, "xmlns='http://www.w3.org/2000/svg'");
  string_indent (s, ATTR_INDENT);
  g_string_append (s, "xmlns:svg='http://www.w3.org/2000/svg'");
  if (self->keywords || self->description || self->author || self->license)
    {
      /* we only need these to write out keywords or description
       * in a way that inkscape understands
       */
      string_indent (s, ATTR_INDENT);
      g_string_append (s, "xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'");
      string_indent (s, ATTR_INDENT);
      g_string_append (s, "xmlns:cc='http://creativecommons.org/ns#'");
      string_indent (s, ATTR_INDENT);
      g_string_append (s, "xmlns:dc='http://purl.org/dc/elements/1.1/'");
    }

  if (self->gpa_version > 0 || (flags & GTK_SVG_SERIALIZE_INCLUDE_STATE))
    {
      string_indent (s, ATTR_INDENT);
      g_string_append (s, "xmlns:gpa='https://www.gtk.org/grappa'");
      string_indent (s, ATTR_INDENT);
      g_string_append_printf (s, "gpa:version='%u'", MAX (self->gpa_version, 1));
      if (self->n_state_names > 0)
        {
          string_indent (s, ATTR_INDENT);
          g_string_append (s, "gpa:state-names='");
          for (unsigned int i = 0; i < self->n_state_names; i++)
            {
              if (i > 0)
                g_string_append_c (s, ' ');
              g_string_append (s, self->state_names[i]);
            }
          g_string_append (s, "'");
        }

      string_indent (s, ATTR_INDENT);
      if (flags & GTK_SVG_SERIALIZE_AT_CURRENT_TIME)
        g_string_append_printf (s, "gpa:state='%u'", self->state);
      else
        g_string_append_printf (s, "gpa:state='%u'", self->initial_state);
    }

  if (flags & GTK_SVG_SERIALIZE_INCLUDE_STATE)
    {
      string_indent (s, ATTR_INDENT);
      string_append_double (s,
                            "gpa:state-change-delay='",
                            (self->state_change_delay) / (double) G_TIME_SPAN_MILLISECOND);
      g_string_append (s, "ms'");
      if (self->load_time != INDEFINITE)
        {
          string_indent (s, ATTR_INDENT);
          string_append_double (s,
                                "gpa:time-since-load='",
                                (self->current_time - self->load_time) / (double) G_TIME_SPAN_MILLISECOND);
          g_string_append (s, "ms'");
        }
    }

  serialize_shape_attrs (s, self, 0, self->content, flags);
  g_string_append (s, ">");

  if (self->keywords || self->description || self->author || self->license)
    {
      string_indent (s, BASE_INDENT);
      g_string_append (s, "<metadata>");
      string_indent (s, 2 * BASE_INDENT);
      g_string_append (s, "<rdf:RDF>");
      string_indent (s, 3 * BASE_INDENT);
      g_string_append (s, "<cc:Work>");
      if (self->license)
        {
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "<cc:license");
          string_indent (s, 4 * BASE_INDENT + ATTR_INDENT);
          g_string_append (s, "rdf:resource='");
          string_append_escaped (s, self->license);
          g_string_append (s, "'/>");
        }
      if (self->author)
        {
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "<dc:creator>");
          string_indent (s, 5 * BASE_INDENT);
          g_string_append (s, "<cc:Agent>");
          string_indent (s, 6 * BASE_INDENT);
          g_string_append (s, "<dc:title>");
          string_append_escaped (s, self->author);
          g_string_append (s, "</dc:title>");
          string_indent (s, 5 * BASE_INDENT);
          g_string_append (s, "</cc:Agent>");
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "</dc:creator>");
        }
      if (self->description)
        {
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "<dc:description>");
          string_append_escaped (s, self->description);
          g_string_append (s, "</dc:description>");
        }
      if (self->keywords)
        {
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "<dc:subject>");
          string_indent (s, 5 * BASE_INDENT);
          g_string_append (s, "<rdf:Bag>");
          string_indent (s, 6 * BASE_INDENT);
          g_string_append (s, "<rdf:li>");
          string_append_escaped (s, self->keywords);
          g_string_append (s, "</rdf:li>");
          string_indent (s, 5 * BASE_INDENT);
          g_string_append (s, "</rdf:Bag>");
          string_indent (s, 4 * BASE_INDENT);
          g_string_append (s, "</dc:subject>");
        }
      string_indent (s, 3 * BASE_INDENT);
      g_string_append (s, "</cc:Work>");
      string_indent (s, 2 * BASE_INDENT);
      g_string_append (s, "</rdf:RDF>");
      string_indent (s, BASE_INDENT);
      g_string_append (s, "</metadata>");
    }

  if (self->font_files)
    {
      for (unsigned int i = 0; i < self->font_files->len; i++)
        {
          const char *file;
          char *data;
          size_t len;
          GBytes *bytes;

          file = g_ptr_array_index (self->font_files, i);

          if (!g_file_get_contents (file, &data, &len, NULL))
            continue;

          bytes = g_bytes_new_take (data, len);

          string_indent (s, BASE_INDENT);
          g_string_append (s, "<font-face>");
          string_indent (s, 2 * BASE_INDENT);
          g_string_append (s, "<font-face-src>");
          string_indent (s, 3 * BASE_INDENT);
          g_string_append (s, "<font-face-uri");
          string_indent (s, 3 * BASE_INDENT + ATTR_INDENT);
          g_string_append (s, "href='data:font/ttf;base64,\\\n");
          string_append_base64 (s, bytes);
          g_string_append (s, "'/>");
          string_indent (s, 2 * BASE_INDENT);
          g_string_append (s, "</font-face-src>");
          string_indent (s, BASE_INDENT);
          g_string_append (s, "</font-face>");

          g_bytes_unref (bytes);
        }
    }

  serialize_shape (s, self, 0, self->content, flags);
  g_string_append (s, "\n</svg>\n");

  return g_string_free_to_bytes (s);
}

/* {{{ Public API */

/**
 * gtk_svg_serialize:
 * @self: an SVG paintable
 *
 * Serializes the content of the renderer as SVG.
 *
 * The SVG will be similar to the orignally loaded one,
 * but is not guaranteed to be 100% identical.
 *
 * This function serializes the DOM, i.e. the results
 * of parsing the SVG. It does not reflect the effect
 * of applying animations.
 *
 * Returns: the serialized contents
 *
 * Since: 4.22
 */
GBytes *
gtk_svg_serialize (GtkSvg *self)
{
  return gtk_svg_serialize_full (self, NULL, 0, GTK_SVG_SERIALIZE_DEFAULT);
}

/**
 * gtk_svg_write_to_file:
 * @self: an SVG paintable
 * @filename: the file to save to
 * @error: return location for an error
 *
 * Serializes the paintable, and saves the result to a file.
 *
 * Returns: true, unless an error occurred
 *
 * Since: 4.22
 */
gboolean
gtk_svg_write_to_file (GtkSvg      *self,
                       const char  *filename,
                       GError     **error)
{
  GBytes *bytes;
  gboolean ret;

  bytes = gtk_svg_serialize (self);
  ret = g_file_set_contents (filename,
                             g_bytes_get_data (bytes, NULL),
                             g_bytes_get_size (bytes),
                             error);
  g_bytes_unref (bytes);

  return ret;
}

/* }}} */

/* vim:set foldmethod=marker: */
