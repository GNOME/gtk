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
#include "gtksvgparserprivate.h"

#include "gtkenums.h"
#include "gtktypebuiltins.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtk/css/gtkcssdataurlprivate.h"
#include "gtk/gtkcssselectorprivate.h"
#include <glib/gstdio.h>
#include "gtksvgenumtypes.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgstringutilsprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgkeywordprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgnumbersprivate.h"
#include "gtksvgstringprivate.h"
#include "gtksvgstringlistprivate.h"
#include "gtksvgenumprivate.h"
#include "gtksvgfilterrefprivate.h"
#include "gtksvgtransformprivate.h"
#include "gtksvgcolorprivate.h"
#include "gtksvgpaintprivate.h"
#include "gtksvgfilterfunctionsprivate.h"
#include "gtksvgdasharrayprivate.h"
#include "gtksvgpathprivate.h"
#include "gtksvgpathdataprivate.h"
#include "gtksvgclipprivate.h"
#include "gtksvgmaskprivate.h"
#include "gtksvgviewboxprivate.h"
#include "gtksvgcontentfitprivate.h"
#include "gtksvgorientprivate.h"
#include "gtksvglanguageprivate.h"
#include "gtksvghrefprivate.h"
#include "gtksvgtextdecorationprivate.h"
#include "gtksvglocationprivate.h"
#include "gtksvgerrorprivate.h"
#include "gtksvgelementtypeprivate.h"
#include "gtksvgpropertyprivate.h"
#include "gtksvgfiltertypeprivate.h"
#include "gtksvgfilterprivate.h"
#include "gtksvgcolorstopprivate.h"
#include "gtksvgelementinternal.h"
#include "gtksvganimationprivate.h"

#include <stdint.h>

#ifdef HAVE_PANGOFT
#include <pango/pangofc-fontmap.h>
#endif

/* {{{ Parser */

/* The parser creates the shape tree. We maintain a current shape,
 * and a current animation. Some things are done in a post-processing
 * step: finding the shape that an animation belongs to, resolving
 * other kinds of shape references, determining the proper order
 * for computing updated values.
 *
 * Note that we treat images, text, markers, gradients, filters as
 * shapes, but not color stops and filter primitives. SvgAnimations
 * are their own thing too.
 *
 * So each shapes can have multiple
 * - child shapes
 * - animations
 * - color stops
 * - filter primitives
 */
typedef struct
{
  GtkSvg *svg;
  SvgElement *current_shape;
  GSList *shape_stack;
  GHashTable *shapes;
  GHashTable *animations;
  SvgAnimation *current_animation;
  GPtrArray *pending_animations;
  GHashTable *pending_refs;
  struct {
    const GSList *to;
    GtkSvgLocation start;
    GtkSvgError code;
    char *reason;
    gboolean skip_over_target;
  } skip;
  struct {
    GtkSvgLocation start;
    gboolean collect;
    GString *text;
  } text;
  uint64_t num_loaded_elements;
  gboolean load_user_style;
} ParserData;

/* {{{ SvgAnimation attributes */

typedef struct
{
  GtkSvg *svg;
  GArray *array;
  GError *error;
  SvgElement *default_event_target;
} Specs;

static gboolean
time_spec_parse_one (GtkCssParser *parser,
                     gpointer      user_data)
{
  Specs *specs = user_data;
  TimeSpec *spec;

  g_array_set_size (specs->array, specs->array->len + 1);
  spec = &g_array_index (specs->array, TimeSpec, specs->array->len - 1);

  return time_spec_parse (parser, specs->svg, specs->default_event_target, spec, &specs->error);
}

static gboolean
parse_base_animation_attrs (SvgAnimation         *a,
                            const char           *element_name,
                            const char          **attr_names,
                            const char          **attr_values,
                            uint64_t             *handled,
                            ParserData           *data,
                            GMarkupParseContext  *context)
{
  const char *id_attr = NULL;
  const char *href_attr = NULL;
  const char *xlink_href_attr = NULL;
  const char *begin_attr = NULL;
  const char *end_attr = NULL;
  const char *dur_attr = NULL;
  const char *repeat_count_attr = NULL;
  const char *repeat_dur_attr = NULL;
  const char *fill_attr = NULL;
  const char *restart_attr = NULL;
  const char *attr_name_attr = NULL;
  const char *ignored = NULL;
  SvgProperty attr;
  SvgElement *current_shape = NULL;
  SvgElementType current_type;
  SvgFilterType filter_type = 0;
  const char *href_attr_name = "href";

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "id", &id_attr,
                            "href", &href_attr,
                            "xlink:href", &xlink_href_attr,
                            "begin", &begin_attr,
                            "end", &end_attr,
                            "dur", &dur_attr,
                            "repeatCount", &repeat_count_attr,
                            "repeatDur", &repeat_dur_attr,
                            "fill", &fill_attr,
                            "restart", &restart_attr,
                            "attributeName", &attr_name_attr,
                            "gpa:status", &ignored,
                            NULL);

  a->id = g_strdup (id_attr);

  if (xlink_href_attr && !href_attr)
    {
      href_attr = xlink_href_attr;
      href_attr_name = "xlink:href";
    }

  if (href_attr)
    {
      if (href_attr[0] != '#')
        gtk_svg_invalid_attribute (data->svg, context, attr_names, href_attr_name,
                                   "Missing '#' in 'href'");
      else
        a->href = g_strdup (href_attr + 1);
    }

  if (a->href)
    current_shape = g_hash_table_lookup (data->shapes, a->href);

  if (!current_shape)
    current_shape = data->current_shape;

  current_type = svg_element_get_type (current_shape);

  if (begin_attr)
    {
      GtkCssParser *parser = parser_new_for_string (begin_attr);
      Specs specs = { 0, };

      specs.svg = data->svg;
      specs.default_event_target = current_shape;
      specs.array = g_array_new (FALSE, TRUE, sizeof (TimeSpec));
      g_array_set_clear_func (specs.array, (GDestroyNotify) time_spec_clear);

      if (parser_parse_list (parser, time_spec_parse_one, &specs))
        {
          for (unsigned int i = 0; i < specs.array->len; i++)
            {
              TimeSpec *spec = &g_array_index (specs.array, TimeSpec, i);
              TimeSpec *begin;

              a->has_begin = 1;
              begin = svg_animation_add_begin (a, timeline_get_time_spec (data->svg->timeline, spec));
              time_spec_add_animation (begin, a);
            }
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "begin", NULL);
        }

      gtk_css_parser_unref (parser);
      g_array_unref (specs.array);

      if (specs.error)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "begin", "%s", specs.error->message);
          g_error_free (specs.error);
        }
    }
  else
    {
      TimeSpec *begin;
      begin = svg_animation_add_begin (a, timeline_get_start_of_time (data->svg->timeline));
      time_spec_add_animation (begin, a);
    }

  if (end_attr)
    {
      GtkCssParser *parser = parser_new_for_string (end_attr);
      Specs specs = { 0, };

      specs.svg = data->svg;
      specs.default_event_target = current_shape;
      specs.array = g_array_new (FALSE, TRUE, sizeof (TimeSpec));
      g_array_set_clear_func (specs.array, (GDestroyNotify) time_spec_clear);

      if (parser_parse_list (parser, time_spec_parse_one, &specs))
        {
          for (unsigned int i = 0; i < specs.array->len; i++)
            {
              TimeSpec *spec = &g_array_index (specs.array, TimeSpec, i);
              TimeSpec *end;

              a->has_end = 1;
              end = svg_animation_add_end (a, timeline_get_time_spec (data->svg->timeline, spec));
              time_spec_add_animation (end, a);
            }
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "end", NULL);
        }

      gtk_css_parser_unref (parser);
      g_array_unref (specs.array);

      if (specs.error)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "end", "%s", specs.error->message);
          g_error_free (specs.error);
        }
    }
  else
    {
      TimeSpec *end;
      end = svg_animation_add_end (a, timeline_get_end_of_time (data->svg->timeline));
      time_spec_add_animation (end, a);
    }

  a->simple_duration = INDEFINITE;
  if (dur_attr)
    {
      a->has_simple_duration = 1;
      if (!parse_duration (dur_attr, TRUE, &a->simple_duration))
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "dur", NULL);
          a->has_simple_duration = 0;
        }
    }

  a->repeat_count = REPEAT_FOREVER;
  if (repeat_count_attr)
    {
      a->has_repeat_count = 1;
      if (!parse_number_or_named (repeat_count_attr, 0, DBL_MAX, "indefinite", REPEAT_FOREVER, &a->repeat_count))
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "repeatCount", NULL);
          a->has_repeat_count = 0;
        }
    }

  a->repeat_duration = INDEFINITE;
  if (repeat_dur_attr)
    {
      a->has_repeat_duration = 1;
      if (!parse_duration (repeat_dur_attr, TRUE, &a->repeat_duration))
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "repeatDur", NULL);
          a->has_repeat_duration = 0;
        }
    }

  if (!a->has_repeat_duration && !a->has_repeat_count)
    {
      a->repeat_count = 1;
      a->repeat_duration = a->simple_duration;
    }
  else if (!a->has_repeat_count && !a->has_simple_duration)
    {
      a->repeat_count = 1;
      a->simple_duration = a->repeat_duration;
    }
  else if (a->has_repeat_count && a->has_simple_duration && !a->has_repeat_duration)
    {
      if (a->repeat_count == REPEAT_FOREVER)
        a->repeat_duration = INDEFINITE;
      else
        a->repeat_duration = a->simple_duration * a->repeat_count;
    }
  else if (a->has_repeat_duration && a->has_simple_duration && !a->has_repeat_count)
    {
      if (a->repeat_duration == INDEFINITE)
        a->repeat_count = REPEAT_FOREVER;
      else
        a->repeat_count = a->repeat_duration / a->simple_duration;
    }
  else if (a->has_repeat_duration && a->has_repeat_count && !a->has_simple_duration)
    {
      if (a->repeat_duration == INDEFINITE || a->repeat_count == REPEAT_FOREVER)
        a->simple_duration = INDEFINITE;
      else
        a->simple_duration = a->repeat_duration / a->repeat_count;
    }

  a->fill = ANIMATION_FILL_REMOVE;
  if (fill_attr)
    {
      unsigned int value;

      if (!parse_enum (fill_attr,
                       (const char *[]) { "freeze", "remove" }, 2,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "fill", NULL);
      else
        a->fill = (AnimationFill) value;
    }

  a->restart = ANIMATION_RESTART_ALWAYS;
  if (restart_attr)
    {
      unsigned int value;

      if (!parse_enum (restart_attr,
                       (const char *[]) { "always", "whenNotActive", "never" }, 3,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "restart", NULL);
      else
        a->restart = (AnimationRestart) value;
    }

  if (attr_name_attr && strcmp (attr_name_attr, "xlink:href") == 0)
    attr_name_attr = "href";

  if (current_shape != NULL &&
      current_type == SVG_ELEMENT_FILTER &&
      current_shape->filters->len > 0)
    {
      SvgFilter *f;

      f = g_ptr_array_index (current_shape->filters,
                              current_shape->filters->len - 1);
      filter_type = svg_filter_get_type (f);
    }

  attr = a->attr;
  if (a->type == ANIMATION_TYPE_MOTION)
    {
      if (attr_name_attr)
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "attributeName",
                                   "Not allowed on <animateMotion>");
    }
  else if (a->type == ANIMATION_TYPE_TRANSFORM)
    {
      if (current_shape != NULL)
        {
          const char *expected;

          /* FIXME: if href is set, current_shape might be the wrong shape */
          expected = svg_property_get_presentation (SVG_PROPERTY_TRANSFORM, current_type);
          if (expected == NULL)
            gtk_svg_invalid_attribute (data->svg, context, attr_names, "attributeName",
                                       "No transform attribute");
          else if (attr_name_attr && strcmp (attr_name_attr, expected) != 0)
            gtk_svg_invalid_attribute (data->svg, context, attr_names, "attributeName",
                                       "Value must be '%s'", expected);
        }

      a->attr = SVG_PROPERTY_TRANSFORM;
    }
  else if (!attr_name_attr)
    {
      gtk_svg_missing_attribute (data->svg, context, "attributeName", NULL);
      return FALSE;
    }
  /* FIXME: if href is set, current_shape might be the wrong shape */
  else if (current_shape != NULL &&
           ((current_type == SVG_ELEMENT_FILTER &&
             svg_property_lookup_for_filter (attr_name_attr, current_type, filter_type, &attr)) ||
            (current_type != SVG_ELEMENT_FILTER &&
             svg_property_lookup (attr_name_attr, current_type, &attr))))
    {
      a->attr = attr;
      /* FIXME: if href is set, current_shape might be the wrong shape */
      if (has_ancestor (context, "stop"))
        a->idx = current_shape->color_stops->len;
      else if (has_ancestor (context, "filter"))
        a->idx = current_shape->filters->len;
    }
  else
    {
      gtk_svg_invalid_attribute (data->svg, context, attr_names, "attributeName",
                                 "Can't animate '%s'", attr_name_attr);
      return FALSE;
    }

  return TRUE;
}

static GArray *
create_default_times (CalcMode      calc_mode,
                      unsigned int  n_values)
{
  GArray *times;
  double n;

  if (calc_mode == CALC_MODE_DISCRETE)
    n = n_values;
  else
    n = n_values - 1;

  times = g_array_new (FALSE, FALSE, sizeof (double));

  for (unsigned int i = 0; i < n_values; i++)
    {
      double d = i / (double) n;
      g_array_append_val (times, d);
    }

  return times;
}

static gboolean
parse_spline (GtkCssParser *parser,
              gpointer      user_data)
{
  GArray *array = user_data;

  for (unsigned int i = 0; i < 4; i++)
    {
      double v;

      gtk_css_parser_skip_whitespace (parser);

      if (!gtk_css_parser_consume_number (parser, &v))
        return FALSE;

      if (v < 0 || v > 1)
        return FALSE;

      g_array_append_val (array, v);

      skip_whitespace_and_optional_comma (parser);
    }

  return TRUE;
}

static gboolean
parse_value_animation_attrs (SvgAnimation         *a,
                             const char           *element_name,
                             const char          **attr_names,
                             const char          **attr_values,
                             uint64_t             *handled,
                             ParserData           *data,
                             GMarkupParseContext  *context)
{
  const char *type_attr = NULL;
  const char *calc_mode_attr = NULL;
  const char *values_attr = NULL;
  const char *from_attr = NULL;
  const char *to_attr = NULL;
  const char *by_attr = NULL;
  const char *key_times_attr = NULL;
  const char *splines_attr = NULL;
  const char *additive_attr = NULL;
  const char *accumulate_attr = NULL;
  const char *color_interpolation_attr = NULL;
  TransformType transform_type = TRANSFORM_NONE;
  GArray *times = NULL;
  GArray *points = NULL;
  GArray *params = NULL;
  GPtrArray *values = NULL;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "type", &type_attr,
                            "calcMode", &calc_mode_attr,
                            "values", &values_attr,
                            "from", &from_attr,
                            "to", &to_attr,
                            "by", &by_attr,
                            "keyTimes", &key_times_attr,
                            "keySplines", &splines_attr,
                            "additive", &additive_attr,
                            "accumulate", &accumulate_attr,
                            "color-interpolation", &color_interpolation_attr,
                            NULL);

  if (a->type == ANIMATION_TYPE_MOTION)
    transform_type = TRANSFORM_TRANSLATE;

  if (a->type == ANIMATION_TYPE_TRANSFORM)
    {
      if (type_attr)
        {
          unsigned int value;

          if (!parse_enum (type_attr,
                           (const char *[]) { "translate", "scale", "rotate",
                                              "skewX", "skewY" }, 5,
                            &value))
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "type", NULL);
              return FALSE;
            }
          else
            transform_type = (TransformType) (value + 1);
        }
      else
        {
          gtk_svg_missing_attribute (data->svg, context, "type", NULL);
          return FALSE;
        }
    }
  else if (type_attr)
    {
      gtk_svg_invalid_attribute (data->svg, context, attr_names, "type", NULL);
    }

  if (calc_mode_attr)
    {
      unsigned int value;

      if (!parse_enum (calc_mode_attr,
                       (const char *[]) { "discrete", "linear", "paced", "spline" }, 4,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "calcMode", NULL);
      else
        a->calc_mode = (CalcMode) value;
   }

  if (additive_attr)
    {
      unsigned int value;

      if (!parse_enum (additive_attr,
                       (const char *[]) { "replace", "sum", }, 2,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "additive", NULL);
      else
        a->additive = (AnimationAdditive) value;
   }

  if (accumulate_attr)
    {
      unsigned int value;

      if (!parse_enum (accumulate_attr,
                       (const char *[]) { "none", "sum", }, 2,
                       &value))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "accumulate", NULL);
      else
        a->accumulate = (AnimationAccumulate) value;
   }

  if (color_interpolation_attr)
    {
      SvgValue *v;
      GError *error = NULL;

      v = svg_property_parse (SVG_PROPERTY_COLOR_INTERPOLATION, color_interpolation_attr, &error);
      if (!v)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "color-interpolation", "%s", error->message);
          g_error_free (error);
        }
      else
        {
          a->color_interpolation = svg_enum_get (v);
          svg_value_unref (v);
        }
    }

  if (values_attr)
    {
      values = svg_property_parse_values (a->attr, transform_type, values_attr);
      if (!values || values->len < 2)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "values", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }
    }
  else if (from_attr && to_attr)
    {
      GPtrArray *tovals;

      values = svg_property_parse_values (a->attr, transform_type, from_attr);
      if (!values || values->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "from", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      tovals = svg_property_parse_values (a->attr, transform_type, to_attr);
      if (!tovals || tovals->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "to", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&tovals, g_ptr_array_unref);
          return FALSE;
        }

      g_ptr_array_extend_and_steal (values, tovals);
    }
  else if (from_attr && by_attr)
    {
      GPtrArray *byvals;
      SvgValue *from;
      SvgValue *by;
      SvgValue *to;
      SvgComputeContext ctx = { 0, };

      values = svg_property_parse_values (a->attr, transform_type, from_attr);

      if (!values || values->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "from", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      byvals = svg_property_parse_values (a->attr, transform_type, by_attr);
      if (!byvals || byvals->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "by", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&byvals, g_ptr_array_unref);
          return FALSE;
        }

      from = g_ptr_array_index (values, 0);
      by = g_ptr_array_index (byvals, 0);
      ctx.interpolation = GDK_COLOR_STATE_SRGB; /* Nothing else is used */
      to = svg_value_accumulate (by, from, &ctx, 1);
      g_ptr_array_add (values, to);
      g_ptr_array_unref (byvals);
    }
  else if (to_attr)
    {
      values = svg_property_parse_values (a->attr, transform_type, to_attr);

      if (!values || values->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "to", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      /* We use a special keyword for this */
      g_ptr_array_insert (values, 0, svg_current_new ());
    }
  else if (by_attr)
    {
      SvgValue *from;
      SvgValue *by;

      values = svg_property_parse_values (a->attr, transform_type, by_attr);

      if (!values || values->len != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "by", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          return FALSE;
        }

      by = g_ptr_array_index (values, 0);
      if (svg_value_is_number (by))
        from = svg_number_new_full (svg_number_get_unit (by), 0);
      else if (svg_value_is_transform (by))
        from = svg_transform_new_none ();
      else if (svg_value_is_paint (by) &&
               svg_paint_get_kind (by) == PAINT_COLOR)
        from = svg_paint_new_transparent ();
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "by",
                                     "Don't know how to handle this value");
          g_ptr_array_unref (values);
          return FALSE;
        }

      g_ptr_array_insert (values, 0, from);

      a->additive = ANIMATION_ADDITIVE_SUM;
    }

  if (a->type == ANIMATION_TYPE_MOTION)
    {
      if (values != NULL)
        {
          GskPathBuilder *builder = gsk_path_builder_new ();
          double length;
          double vals[6];

          points = g_array_new (FALSE, FALSE, sizeof (double));

          for (unsigned int i = 0; i < values->len; i++)
            {
              SvgValue *tf = g_ptr_array_index (values, i);
              TransformType type;

              type = svg_transform_get_primitive (tf, 0, vals);

              if (svg_transform_get_length (tf) != 1 || type != TRANSFORM_TRANSLATE)
                {
                  gtk_svg_invalid_attribute (data->svg, context, attr_names, NULL,
                                             "Transform is not a translation");
                  g_ptr_array_unref (values);
                  g_array_unref (points);
                  return FALSE;
                }

              if (i == 0)
                {
                  length = 0;
                  gsk_path_builder_move_to (builder, vals[0], vals[1]);
                }
              else
                {
                  length += graphene_point_distance (gsk_path_builder_get_current_point (builder),
                                                     &GRAPHENE_POINT_INIT (vals[0], vals[1]), NULL, NULL);
                  gsk_path_builder_line_to (builder, vals[0], vals[1]);
                }
              g_array_append_val (points, length);
            }

          for (unsigned int i = 1; i < points->len; i++)
            g_array_index (points, double, i) /= length;

          a->motion.path = gsk_path_builder_free_to_path (builder);

          g_clear_pointer (&values, g_ptr_array_unref);
        }
    }
  else
    {
      if (values == NULL)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, NULL,
                                     "Either 'values' or 'from', 'to' or "
                                     "'by' must be given");
          return FALSE;
        }
    }

  if (a->calc_mode == CALC_MODE_PACED)
    {
      if (values && values->len > 2)
        {
          double length, distance;
          const SvgValue *v0, *v1;

          /* use value distances to compute evenly paced times */
          times = g_array_new (FALSE, FALSE, sizeof (double));
          length = distance = 0;
          g_array_append_val (times, distance);
          for (unsigned int i = 1; i < values->len; i++)
            {
              v0 = g_ptr_array_index (values, i - 1);
              v1 = g_ptr_array_index (values, i);
              distance = svg_value_distance (v0, v1);
              g_array_append_val (times, distance);
              length += distance;
            }
           for (unsigned int i = 1; i < times->len; i++)
             {
               g_array_index (times, double, i) /= length;
             }
           for (unsigned int i = 1; i < times->len; i++)
             {
               g_array_index (times, double, i) += g_array_index (times, double, i - 1);
             }
        }
    }
  else if (key_times_attr)
    {
      times = parse_number_list (key_times_attr, 0, 1);
      if (!times)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyTimes", NULL);
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&points, g_array_unref);
          return FALSE;
        }
    }

  if (times != NULL)
    {
      if (values && times->len != values->len)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, NULL,
                                     "'values' and 'keyTimes' must have "
                                     "the same number of items");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          g_clear_pointer (&points, g_array_unref);
          return FALSE;
        }

      if (times->len == 0)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyTimes",
                                     "No values");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          g_clear_pointer (&points, g_array_unref);
          return FALSE;
        }

      if (g_array_index (times, double, 0) != 0)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyTimes",
                                     "First value must be 0");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          g_clear_pointer (&points, g_array_unref);
          return FALSE;
        }

      if (a->calc_mode != CALC_MODE_DISCRETE && g_array_index (times, double, times->len - 1) != 1)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyTimes",
                                     "Last value must be 1");
          g_clear_pointer (&values, g_ptr_array_unref);
          g_clear_pointer (&times, g_array_unref);
          g_clear_pointer (&points, g_array_unref);
          return FALSE;
        }

      for (unsigned int i = 1; i < times->len; i++)
        {
          if (g_array_index (times, double, i) < g_array_index (times, double, i - 1))
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyTimes",
                                         "Values must be increasing");
              g_clear_pointer (&values, g_ptr_array_unref);
              g_clear_pointer (&times, g_array_unref);
              g_clear_pointer (&points, g_array_unref);
              return FALSE;
            }
        }
    }

  if (a->calc_mode != CALC_MODE_PACED)
    {
      params = NULL;
      if (splines_attr)
        {
          GtkCssParser *parser = parser_new_for_string (splines_attr);
          unsigned int n;

          params = g_array_new (FALSE, TRUE, sizeof (double));
          if (!parser_parse_list (parser, parse_spline, params))
            {
              gtk_css_parser_unref (parser);
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "keySplines", NULL);
              g_clear_pointer (&values, g_ptr_array_unref);
              g_clear_pointer (&times, g_array_unref);
              g_clear_pointer (&params, g_array_unref);
              g_clear_pointer (&points, g_array_unref);
              return FALSE;
            }
          gtk_css_parser_unref (parser);

          n = params->len / 4;

          if (times == NULL)
            times = create_default_times (a->calc_mode, n + 1);

          if (n != times->len - 1)
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "keySplines",
                                         "Wrong number of values");
              g_clear_pointer (&values, g_ptr_array_unref);
              g_clear_pointer (&times, g_array_unref);
              g_clear_pointer (&params, g_array_unref);
              g_clear_pointer (&points, g_array_unref);
              return FALSE;
            }
        }
    }

  if (times == NULL)
    {
      unsigned int n;

      if (values)
        n = values->len;
      else if (points)
        n = points->len;
      else
        n = 2;

      times = create_default_times (a->calc_mode, n);
    }

  g_assert (times != NULL);

  if (times->len < 2 ||
      (values && times->len != values->len) ||
      (params && 4 * (times->len - 1) != params->len) ||
      (points && times->len != points->len))
    {
      gtk_svg_invalid_attribute (data->svg, context, attr_names, NULL,
                                 "Invalid value attributes");
      g_clear_pointer (&values, g_ptr_array_unref);
      g_clear_pointer (&times, g_array_unref);
      g_clear_pointer (&params, g_array_unref);
      g_clear_pointer (&points, g_array_unref);
      return FALSE;
    }

  svg_animation_fill_from_values (a,
                                  (double *) times->data,
                                  times->len,
                                  values ? (SvgValue **) values->pdata : NULL,
                                  params ? (double *) params->data : NULL,
                                  points ? (double *) points->data : NULL);

  g_clear_pointer (&values, g_ptr_array_unref);
  g_clear_pointer (&times, g_array_unref);
  g_clear_pointer (&points, g_array_unref);
  g_clear_pointer (&params, g_array_unref);

  return TRUE;
}

static gboolean
parse_motion_animation_attrs (SvgAnimation         *a,
                              const char           *element_name,
                              const char          **attr_names,
                              const char          **attr_values,
                              uint64_t             *handled,
                              ParserData           *data,
                              GMarkupParseContext  *context)
{
  const char *path_attr = NULL;
  const char *rotate_attr = NULL;
  const char *key_points_attr = NULL;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "path", &path_attr,
                            "rotate", &rotate_attr,
                            "keyPoints", &key_points_attr,
                            NULL);

  if (path_attr)
    {
      SvgValue *value = NULL;

      if (strcmp (path_attr, "none") == 0)
        value = svg_path_new_none ();
      else
        {
          SvgPathData *path_data = NULL;

          if (svg_path_data_parse_full (path_attr, &path_data))
            value = svg_path_new_from_data (path_data);
          else
            gtk_svg_invalid_attribute (data->svg, context, attr_names, "path", "Path data is invalid");
        }

      if (value)
        {
          g_clear_pointer (&a->motion.path, gsk_path_unref);
          a->motion.path = gsk_path_ref (svg_path_get_gsk (value));
          svg_value_unref (value);
        }
      else
        return FALSE;
    }

  a->motion.rotate = ROTATE_FIXED;
  a->motion.angle = 0;
  if (rotate_attr)
    {
      GtkCssParser *parser = parser_new_for_string (rotate_attr);
      SvgValue *value;

      gtk_css_parser_skip_whitespace (parser);
      value = svg_number_parse (parser, -DBL_MAX, DBL_MAX, 0);
      if (value)
        {
          a->motion.angle = svg_number_get (value, 100);
          a->motion.rotate = ROTATE_FIXED;
          svg_value_unref (value);
        }
      else if (!parser_try_enum (parser,
                                 (const char *[]) { "auto", "auto-reverse" }, 2,
                                 (unsigned int *) &a->motion.rotate))
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "rotate", NULL);
        }

      gtk_css_parser_skip_whitespace (parser);
      if (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "rotate",
                                   "Junk at end of value");

      gtk_css_parser_unref (parser);
    }

  if (a->calc_mode != CALC_MODE_PACED)
    {
      if (key_points_attr)
        {
          GArray *points = parse_number_list (key_points_attr, 0, 1);
          if (!points)
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyPoints", NULL);
              return FALSE;
            }

          if (points->len != a->n_frames)
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, "keyPoints",
                                         "Wrong number of values");
              g_array_unref (points);
              return FALSE;
            }

          for (unsigned int i = 0; i < a->n_frames; i++)
            a->frames[i].point = g_array_index (points, double, i);

          g_array_unref (points);
        }
    }

  if (a->calc_mode == CALC_MODE_PACED)
    {
      /* When paced, keyTimes, keyPoints and keySplines
       * are all ignored, and we calculate times and
       * points from the path to ensure an even pace
       */

      g_clear_pointer (&a->frames, g_free);
      a->n_frames = 0;

      if (a->motion.path && !gsk_path_is_empty (a->motion.path))
        {
          svg_animation_motion_fill_from_path (a, a->motion.path);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "calcMode",
                                     "Paced animation without explicit path is not implemented");
          return FALSE;
        }
    }

  return TRUE;
}

/* }}} */
/* {{{ Attributes */

static void
parse_shape_attrs (SvgElement           *shape,
                   const char           *element_name,
                   const char          **attr_names,
                   const char          **attr_values,
                   uint64_t             *handled,
                   ParserData           *data,
                   GMarkupParseContext  *context)
{
  for (unsigned int i = 0; attr_names[i]; i++)
    {
      SvgProperty attr;

      if (*handled & BIT (i))
        continue;

      if (svg_element_get_type (shape) == SVG_ELEMENT_SVG &&
          (g_str_has_prefix (attr_names[i], "xmlns") ||
           strcmp (attr_names[i], "version") == 0 ||
           strcmp (attr_names[i], "baseProfile") == 0))
        {
          *handled |= BIT (i);
          continue;
        }

      if (strcmp (attr_names[i], "class") == 0)
        {
          *handled |= BIT (i);
          svg_element_parse_classes (shape, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "style") == 0)
        {
          GtkSvgLocation start, end;

          gtk_svg_location_init_attr_range (&start, &end, context, i);
          *handled |= BIT (i);
          svg_element_set_style (shape, attr_values[i], &start);
        }
      else if (strcmp (attr_names[i], "id") == 0)
        {
          *handled |= BIT (i);
          svg_element_set_id (shape, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "tabindex") == 0)
        {
          int tabindex;
          char *end;

          *handled |= BIT (i);
          tabindex = g_ascii_strtoll (attr_values[i], &end, 10);
          if (end && *end != '\0')
            gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "Not an integer");
          else
            svg_element_set_focusable (shape, tabindex >= 0);
        }
      else if (strcmp (attr_names[i], "autofocus") == 0)
        {
          *handled |= BIT (i);
          if (strcmp (attr_values[i], "") == 0 || strcmp (attr_values[i], "autofocus") == 0)
            svg_element_set_autofocus (shape, TRUE);
          else
            gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], NULL);
        }
      else if (svg_property_lookup (attr_names[i], svg_element_get_type (shape), &attr) &&
               svg_property_has_presentation (attr))
        {
          if (svg_property_applies_to (attr, svg_element_get_type (shape)))
            {
              if (svg_element_is_specified (shape, attr) &&
                  svg_attr_is_deprecated (attr_names[i]))
                {
                  /* ignore */
                }
              else
                {
                  SvgValue *value;
                  GError *error = NULL;

                  value = svg_property_parse_and_validate (attr, attr_values[i], &error);
                  /* It is possible that a value is returned and error
                   * is still set, e.g. for 'd' or 'points'
                   */
                  if (error)
                    {
                      gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "%s", error->message);
                      g_error_free (error);
                    }
                  else if (!value)
                    {
                      gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], NULL);
                    }

                  if (value)
                    svg_element_take_specified_value (shape, attr, value);
                }
            }
          else
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "Unknown attribute: %s", attr_names[i]);
            }

          *handled |= BIT (i);
        }
    }

  if (svg_property_applies_to (SVG_PROPERTY_FX, svg_element_get_type (shape)) &&
      svg_property_applies_to (SVG_PROPERTY_FY, svg_element_get_type (shape)))
    {
      if (svg_element_is_specified (shape, SVG_PROPERTY_CX) &&
          !svg_element_is_specified (shape, SVG_PROPERTY_FX))
        svg_element_set_specified_value (shape, SVG_PROPERTY_FX, svg_element_get_specified_value (shape, SVG_PROPERTY_CX));
      if (svg_element_is_specified (shape, SVG_PROPERTY_CY) &&
          !svg_element_is_specified (shape, SVG_PROPERTY_FY))
        svg_element_set_specified_value (shape, SVG_PROPERTY_FY, svg_element_get_specified_value (shape, SVG_PROPERTY_CY));
    }

  if (svg_element_get_autofocus (shape) && svg_element_get_focusable (shape))
    {
      if (data->svg->initial_focus == NULL)
        data->svg->initial_focus = shape;
      else
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "autofocus", "Duplicate autofocus");
    }
}

static void
parse_svg_gpa_attrs (GtkSvg               *svg,
                     const char           *element_name,
                     const char          **attr_names,
                     const char          **attr_values,
                     uint64_t             *handled,
                     ParserData           *data,
                     GMarkupParseContext  *context)
{
  const char *state_names_attr = NULL;
  const char *state_attr = NULL;
  const char *version_attr = NULL;
  const char *keywords_attr = NULL;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "gpa:state-names", &state_names_attr,
                            "gpa:state", &state_attr,
                            "gpa:version", &version_attr,
                            "gpa:keywords", &keywords_attr,
                            NULL);

  if (state_names_attr)
    {
      GStrv strv = parse_strv (state_names_attr);

      if (strv == NULL)
        {
          gtk_svg_invalid_attribute (svg, context, attr_names, "gpa:state-names", NULL);
        }
      else
        {
          if (!gtk_svg_set_state_names (svg, (const char **) strv))
            gtk_svg_invalid_attribute (svg, context, attr_names, "gpa:state-names", NULL);
          g_strfreev (strv);
        }
    }

  if (state_attr)
    {
      double v;
      unsigned int state;

      if (parse_number (state_attr, 0, 63, &v))
        {
          svg->initial_state = (unsigned int) v;
          gtk_svg_set_state (svg, (unsigned int) v);
        }
      else if (find_named_state (svg, state_attr, &state))
        {
          svg->initial_state = state;
          gtk_svg_set_state (svg, state);
        }
      else
        gtk_svg_invalid_attribute (svg, context, attr_names, "gpa:state", NULL);
    }

  if (version_attr)
    {
      unsigned int version;
      char *end;

      version = (unsigned int) g_ascii_strtoull (version_attr, &end, 10);
      if ((end && *end != '\0') || version != 1)
        gtk_svg_invalid_attribute (svg, context, attr_names, "gpa:version",
                                   "Must be 1");
      else
        svg->gpa_version = version;
    }

  if (keywords_attr)
    g_set_str (&svg->keywords, keywords_attr);
}

static void
parse_shape_gpa_attrs (SvgElement           *shape,
                       const char           *element_name,
                       const char          **attr_names,
                       const char          **attr_values,
                       uint64_t             *handled,
                       ParserData           *data,
                       GMarkupParseContext  *context)
{
  const char *stroke_attr = NULL;
  const char *fill_attr = NULL;
  const char *strokewidth_attr = NULL;
  const char *states_attr = NULL;
  const char *transition_type_attr = NULL;
  const char *transition_duration_attr = NULL;
  const char *transition_delay_attr = NULL;
  const char *transition_easing_attr = NULL;
  const char *animation_type_attr = NULL;
  const char *animation_direction_attr = NULL;
  const char *animation_duration_attr = NULL;
  const char *animation_repeat_attr = NULL;
  const char *animation_segment_attr = NULL;
  const char *animation_easing_attr = NULL;
  const char *origin_attr = NULL;
  const char *attach_to_attr = NULL;
  const char *attach_pos_attr = NULL;
  SvgValue *value;
  uint64_t states;
  double origin;
  unsigned int transition_type;
  int64_t transition_duration;
  int64_t transition_delay;
  unsigned int transition_easing;
  unsigned int has_animation;
  unsigned int animation_direction;
  int64_t animation_duration;
  double animation_repeat;
  unsigned int animation_easing;
  double animation_segment;
  double attach_pos;

  if (!svg_element_type_is_path (svg_element_get_type (shape)))
    return;

  markup_filter_attributes (element_name,
                            attr_names, attr_values,
                            handled,
                            "gpa:stroke", &stroke_attr,
                            "gpa:fill", &fill_attr,
                            "gpa:stroke-width", &strokewidth_attr,
                            "gpa:states", &states_attr,
                            "gpa:origin", &origin_attr,
                            "gpa:transition-type", &transition_type_attr,
                            "gpa:transition-duration", &transition_duration_attr,
                            "gpa:transition-delay", &transition_delay_attr,
                            "gpa:transition-easing", &transition_easing_attr,
                            "gpa:animation-type", &animation_type_attr,
                            "gpa:animation-direction", &animation_direction_attr,
                            "gpa:animation-duration", &animation_duration_attr,
                            "gpa:animation-repeat", &animation_repeat_attr,
                            "gpa:animation-segment", &animation_segment_attr,
                            "gpa:animation-easing", &animation_easing_attr,
                            "gpa:attach-to", &attach_to_attr,
                            "gpa:attach-pos", &attach_pos_attr,
                            NULL);

  if (stroke_attr)
    {
      value = svg_paint_parse_gpa (stroke_attr);
      if (value)
        {
          svg_element_set_gpa_stroke (shape, value);
          svg_element_take_base_value (shape, SVG_PROPERTY_STROKE, value);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:stroke", NULL);
        }
    }

  if (fill_attr)
    {
      value = svg_paint_parse_gpa (fill_attr);
      if (value)
        {
          svg_element_set_gpa_fill (shape, value);
          svg_element_take_base_value (shape, SVG_PROPERTY_FILL, value);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:fill", NULL);
        }
    }

  if (strokewidth_attr)
    {
      SvgValue *values[3] = { NULL, };

      if (strokewidth_parse (strokewidth_attr, values) &&
          values[0] && values[1] && values[2])
        {
          svg_element_set_specified_value (shape, SVG_PROPERTY_STROKE_MINWIDTH, values[0]);
          svg_element_set_specified_value (shape, SVG_PROPERTY_STROKE_WIDTH, values[1]);
          svg_element_set_specified_value (shape, SVG_PROPERTY_STROKE_MAXWIDTH, values[2]);
          svg_element_set_gpa_width (shape, values[1]);
        }
      else
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:stroke-width", NULL);
        }

      g_clear_pointer (&values[0], svg_value_unref);
      g_clear_pointer (&values[1], svg_value_unref);
      g_clear_pointer (&values[2], svg_value_unref);
    }

  states = ALL_STATES;
  if (states_attr)
    {
      if (!parse_states (data->svg, states_attr, &states))
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:states", NULL);
          states = ALL_STATES;
        }
      else
        {
          if (states != NO_STATES)
            data->svg->max_state = MAX (data->svg->max_state, g_bit_nth_msf (states, -1));
        }
    }

  origin = 0;
  if (origin_attr)
    {
      if (!parse_number (origin_attr, 0, 1, &origin))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:origin", NULL);
    }

  transition_type = GPA_TRANSITION_NONE;
  if (transition_type_attr)
    {
      if (!parse_enum (transition_type_attr,
                       (const char *[]) { "none", "animate", "morph", "fade" }, 4,
                        &transition_type))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:transition-type", NULL);
    }

  transition_duration = 0;
  if (transition_duration_attr)
    {
      if (!parse_duration (transition_duration_attr, FALSE, &transition_duration))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:transition-duration", NULL);
    }

  transition_delay = 0;
  if (transition_delay_attr)
    {
      if (!parse_duration (transition_delay_attr, FALSE, &transition_delay))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:transition-delay", NULL);
    }

  transition_easing = GPA_EASING_LINEAR;
  if (transition_easing_attr)
    {

      if (!parse_enum (transition_easing_attr,
                       (const char *[]) { "linear", "ease-in-out", "ease-in",
                                          "ease-out", "ease" }, 5,
                        &transition_easing))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:transition-easing", NULL);
    }

  has_animation = 1;
  if (animation_type_attr)
    {
      if (!parse_enum (animation_type_attr,
                       (const char *[]) { "none", "automatic", }, 2,
                        &has_animation))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-type", NULL);
    }

  animation_direction = GPA_ANIMATION_NONE;
  if (has_animation && animation_direction_attr)
    {
      if (!parse_enum (animation_direction_attr,
                       (const char *[]) { "none", "normal", "alternate", "reverse",
                                          "reverse-alternate", "in-out",
                                          "in-out-alternate", "in-out-reverse",
                                          "segment", "segment-alternate" }, 10,
                        &animation_direction))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-direction", NULL);
    }

  animation_duration = 0;
  if (animation_duration_attr)
    {
      if (!parse_duration (animation_duration_attr, FALSE, &animation_duration))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-duration", NULL);
    }

  animation_repeat = REPEAT_FOREVER;
  if (animation_repeat_attr)
    {
      if (!parse_number_or_named (animation_repeat_attr, 0, DBL_MAX, "indefinite", REPEAT_FOREVER, &animation_repeat))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-repeat", NULL);
    }

  animation_segment = 0.2;
  if (animation_segment_attr)
    {
      if (!parse_number (animation_segment_attr, 0, 1, &animation_segment))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-segment", NULL);
    }

  animation_easing = GPA_EASING_LINEAR;
  if (animation_easing_attr)
    {
      if (!parse_enum (animation_easing_attr,
                       (const char *[]) { "linear", "ease-in-out", "ease-in",
                                          "ease-out", "ease" }, 5,
                        &animation_easing))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:animation-easing", NULL);
    }

  attach_pos = 0;
  if (attach_pos_attr)
    {
      if (!parse_number (attach_pos_attr, 0, 1, &attach_pos))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "gpa:attach-pos", NULL);
    }

  svg_element_set_states (shape, states);
  svg_element_set_gpa_transition (shape,
                                  transition_type,
                                  transition_easing,
                                  transition_duration,
                                  transition_delay);
  svg_element_set_gpa_animation (shape,
                                 animation_direction,
                                 animation_easing,
                                 animation_duration,
                                 animation_repeat,
                                 animation_segment);
  svg_element_set_gpa_origin (shape, origin);
  svg_element_set_gpa_attachment (shape, attach_to_attr, attach_pos, NULL);

  if ((data->svg->features & GTK_SVG_ANIMATIONS) == 0)
    return;

  if (transition_type != GPA_TRANSITION_NONE ||
      animation_direction != GPA_ANIMATION_NONE)
    {
      /* our dasharray-based animations require unit path length */
      if (svg_element_is_specified (shape, SVG_PROPERTY_PATH_LENGTH))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "pathLength",
                                   "Can't set '%s' and use gpa features", "pathLength");

      if (svg_element_is_specified (shape, SVG_PROPERTY_STROKE_DASHARRAY))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "stroke-dasharray",
                                   "Can't set '%s' and use gpa features", "stroke-dasharray");

      if (svg_element_is_specified (shape, SVG_PROPERTY_STROKE_DASHOFFSET))
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "stroke-dashoffset",
                                   "Can't set '%s' and use gpa features", "stroke-dashoffset");

      if (svg_element_is_specified (shape, SVG_PROPERTY_FILTER) &&
          transition_type == GPA_TRANSITION_MORPH)
        gtk_svg_invalid_attribute (data->svg, context, attr_names, "filter",
                                   "Can't set '%s' and use gpa features", "filter");
    }

  create_states (shape,
                 data->svg->timeline,
                 states,
                 transition_delay,
                 data->svg->state);

  if (attach_to_attr ||
      transition_type == GPA_TRANSITION_ANIMATE ||
      animation_direction != GPA_ANIMATION_NONE)
    create_path_length (shape, data->svg->timeline);

  if (attach_to_attr)
    create_attachment (shape,
                       data->svg->timeline,
                       states,
                       attach_to_attr,
                       attach_pos,
                       origin);

  create_transitions (shape,
                      data->svg->timeline,
                      data->shapes,
                      states,
                      transition_type,
                      transition_duration,
                      transition_delay,
                      transition_easing,
                      origin);

  create_animations (shape,
                     data->svg->timeline,
                     states,
                     data->svg->state,
                     animation_repeat,
                     animation_duration,
                     animation_direction,
                     animation_easing,
                     animation_segment,
                     origin);
}

/* }}} */
/* {{{ Color Stop attributes */

static void
parse_color_stop_attrs (SvgElement           *shape,
                        SvgColorStop         *stop,
                        const char           *element_name,
                        const char          **attr_names,
                        const char          **attr_values,
                        uint64_t             *handled,
                        ParserData           *data,
                        GMarkupParseContext  *context)
{
  for (unsigned int i = 0; attr_names[i]; i++)
    {
      SvgProperty attr;

      if (strcmp (attr_names[i], "id") == 0)
        {
          *handled |= BIT (i);
          svg_color_stop_set_id (stop, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "style") == 0)
        {
          *handled |= BIT (i);
          svg_color_stop_set_style (stop, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "class") == 0)
        {
          *handled |= BIT (i);
          svg_color_stop_parse_classes (stop, attr_values[i]);
        }

      else if (svg_property_lookup_for_stop (attr_names[i], svg_element_get_type (shape), &attr))
        {
          SvgValue *value;
          GError *error = NULL;

          *handled |= BIT (i);
          value = svg_property_parse_and_validate (attr, attr_values[i], &error);
          if (error)
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "%s", error->message);
              g_error_free (error);
            }
          else if (!value)
            {
              gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], NULL);
            }

          if (value)
            svg_color_stop_take_specified_value (stop, attr, value);
        }
      else
        gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "Unknown attribute: %s", attr_names[i]);
    }
}

/* }}} */
/* {{{ Filter attributes */

static void
parse_filter_attrs (SvgElement           *shape,
                    SvgFilter            *f,
                    const char           *element_name,
                    const char          **attr_names,
                    const char          **attr_values,
                    uint64_t             *handled,
                    ParserData           *data,
                    GMarkupParseContext  *context)
{
  SvgFilterType type = svg_filter_get_type (f);

  for (unsigned int i = 0; attr_names[i]; i++)
    {
      SvgProperty attr;

      if (strcmp (attr_names[i], "id") == 0)
        {
          *handled |= BIT (i);
          svg_filter_set_id (f, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "style") == 0)
        {
          *handled |= BIT (i);
          svg_filter_set_style (f, attr_values[i]);
        }
      else if (strcmp (attr_names[i], "class") == 0)
        {
          *handled |= BIT (i);
          svg_filter_parse_classes (f, attr_values[i]);
        }
      else if (svg_property_lookup_for_filter (attr_names[i], svg_element_get_type (shape), type, &attr))
        {
          if (svg_filter_is_specified (f, attr) && svg_attr_is_deprecated (attr_names[i]))
            {
              /* ignore */
            }
          else
            {
              SvgValue *value;
              GError *error = NULL;

              *handled |= BIT (i);
              value = svg_property_parse_and_validate (attr, attr_values[i], &error);
              if (error)
                {
                  gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "%s", error->message);
                  g_error_free (error);
                }
              else if (!value)
                {
                  gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], NULL);
                }

              if (value)
                svg_filter_take_specified_value (f, attr, value);
            }
        }
      else
        gtk_svg_invalid_attribute (data->svg, context, attr_names, attr_names[i], "Unknown attribute: %s", attr_names[i]);
    }
}

/* }}} */

G_GNUC_PRINTF (4, 5)
static void
skip_element (ParserData          *data,
              GMarkupParseContext *context,
              GtkSvgError          code,
              const char          *format,
              ...)
{
  va_list args;

  gtk_svg_location_init_tag_start (&data->skip.start, context);
  data->skip.to = g_markup_parse_context_get_element_stack (context);
  data->skip.skip_over_target = TRUE;
  data->skip.code = code;

  va_start (args, format);
  g_vasprintf (&data->skip.reason, format, args);
  va_end (args);
}

static void
start_collect_text (ParserData          *data,
                    GMarkupParseContext *context)
{
  gtk_svg_location_init (&data->text.start, context);
  data->text.collect = TRUE;
  g_string_set_size (data->text.text, 0);
}

static void
start_element_cb (GMarkupParseContext  *context,
                  const char           *element_name,
                  const char          **attr_names,
                  const char          **attr_values,
                  gpointer              user_data,
                  GError              **gmarkup_error)
{
  ParserData *data = user_data;
  SvgElementType shape_type;
  uint64_t handled = 0;
  SvgFilterType filter_type;
  GtkSvgLocation location;

  if (data->skip.to)
    return;

  gtk_svg_location_init_tag_start (&location, context);

  if (data->num_loaded_elements++ > LOADING_LIMIT)
    {
      data->skip.start = location;
      data->skip.to = g_markup_parse_context_get_element_stack (context)->next;
      data->skip.code = GTK_SVG_ERROR_LIMITS_EXCEEDED;
      data->skip.reason = g_strdup ("Loading limit exceeded");
      data->skip.skip_over_target = FALSE;
      return;
    }

  if (svg_element_type_lookup (element_name, &shape_type))
    {
      SvgElement *shape = NULL;
      const char *id;

      if (data->current_shape &&
          !svg_element_type_is_container (svg_element_get_type (data->current_shape)))
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Parent element can't contain shapes");
          return;
        }

      if (shape_type != SVG_ELEMENT_USE &&
          !svg_element_type_is_clip_path_content (shape_type) &&
          has_ancestor (context, "clipPath") &&
          shape_type != SVG_ELEMENT_CLIP_PATH)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<clipPath> can only contain shapes, not %s", element_name);
          return;
        }

      shape = svg_element_new (data->current_shape, shape_type);
      svg_element_set_origin (shape, &location);

      if (data->current_shape == NULL && svg_element_get_type (shape) == SVG_ELEMENT_SVG)
        {
          data->svg->content = shape;

          if (data->svg->features & GTK_SVG_EXTENSIONS)
            parse_svg_gpa_attrs (data->svg,
                                 element_name, attr_names, attr_values,
                                 &handled, data, context);
        }

      parse_shape_attrs (shape,
                         element_name, attr_names, attr_values,
                         &handled, data, context);

      if (data->svg->gpa_version > 0)
        parse_shape_gpa_attrs (shape,
                               element_name, attr_names, attr_values,
                               &handled, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (data->current_shape)
        svg_element_add_child (data->current_shape, shape);

      data->shape_stack = g_slist_prepend (data->shape_stack, data->current_shape);

      data->current_shape = shape;

      id = svg_element_get_id (shape);
      if (id && !g_hash_table_contains (data->shapes, id))
        g_hash_table_insert (data->shapes, (gpointer) id, shape);

      return;
    }

  if (strcmp (element_name, "stop") == 0)
    {
      SvgColorStop *stop;

      if (data->current_shape == NULL ||
          (!check_ancestors (context, "linearGradient", NULL) &&
           !check_ancestors (context, "radialGradient", NULL)))
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<stop> only allowed in <linearGradient> or <radialGradient>");
          return;
        }

      stop = svg_color_stop_new (data->current_shape);
      svg_color_stop_set_origin (stop, &location);

      parse_color_stop_attrs (data->current_shape, stop,
                              element_name, attr_names, attr_values,
                              &handled, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      svg_element_add_color_stop (data->current_shape, stop);

      return;
    }

  if (svg_filter_type_lookup (element_name, &filter_type))
    {
      SvgFilter *f;

      if (filter_type == SVG_FILTER_MERGE_NODE)
        {
          if (!check_ancestors (context, "feMerge", "filter", NULL))
            {
              skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<%s> only allowed in <feMerge>", element_name);
              return;
            }

        }
      else if (filter_type == SVG_FILTER_FUNC_R ||
               filter_type == SVG_FILTER_FUNC_G ||
               filter_type == SVG_FILTER_FUNC_B ||
               filter_type == SVG_FILTER_FUNC_A)
        {
          if (!check_ancestors (context, "feComponentTransfer", "filter", NULL))
            {
              skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<%s> only allowed in <feComponentTransfer>", element_name);
              return;
            }
        }
      else
        {
          if (!check_ancestors (context, "filter", NULL))
            {
              skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<%s> only allowed in <filter>", element_name);
              return;
            }
        }

      f = svg_filter_new (data->current_shape, filter_type);
      svg_filter_set_origin (f, &location);

      parse_filter_attrs (data->current_shape, f,
                          element_name, attr_names, attr_values,
                          &handled, data, context);

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (filter_type == SVG_FILTER_COLOR_MATRIX)
        {
          SvgValue *values = svg_filter_get_base_value (f, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES);
          SvgValue *initial = svg_filter_ref_initial_value (f, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES);

          if (svg_numbers_get_length (values) != svg_numbers_get_length (initial))
            {
              svg_filter_set_base_value (f, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES, initial);
              if (!svg_filter_is_specified (f, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES))
                {
                  /* If this wasn't user-provided, we quietly correct the initial
                   * value to match the type
                   */
                  gtk_svg_invalid_attribute (data->svg, context, attr_names, "values", NULL);
                }
            }
          svg_value_unref (initial);
        }

      svg_element_add_filter (data->current_shape, f);

      return;
    }

  if (strcmp (element_name, "metadata") == 0)
    {
      return;
    }

  if (strcmp (element_name, "rdf:RDF") == 0 ||
      strcmp (element_name, "cc:Work") == 0 ||
      strcmp (element_name, "dc:subject") == 0 ||
      strcmp (element_name, "dc:description") == 0 ||
      strcmp (element_name, "dc:creator") == 0 ||
      strcmp (element_name, "cc:Agent") == 0 ||
      strcmp (element_name, "dc:title") == 0 ||
      strcmp (element_name, "cc:license") == 0 ||
      strcmp (element_name, "rdf:Bag") == 0 ||
      strcmp (element_name, "rdf:li") == 0)
    {
      if (!has_ancestor (context, "metadata"))
        skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring RDF elements outside <metadata>: <%s>", element_name);

      if (strcmp (element_name, "rdf:li") == 0)
        {
          /* Verify we're in the right place */
          if (check_ancestors (context, "rdf:Bag", "dc:subject", "cc:Work", "rdf:RDF", "metadata", NULL))
            start_collect_text (data, context);
          else
            skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring RDF element in wrong context: <%s>", element_name);
        }
      else if (strcmp (element_name, "dc:description") == 0)
        {
          if (check_ancestors (context, "cc:Work", "rdf:RDF", "metadata", NULL))
            start_collect_text (data, context);
          else
            skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring RDF element in wrong context: <%s>", element_name);
        }
      else if (strcmp (element_name, "dc:title") == 0)
        {
          if (check_ancestors (context, "cc:Agent", "dc:creator", "cc:Work", "rdf:RDF", "metadata", NULL))
            start_collect_text (data, context);
          else
            skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring RDF element in wrong context: <%s>", element_name);
        }
      else if (strcmp (element_name, "cc:license") == 0)
        {
          const char *license = NULL;

          markup_filter_attributes (element_name,
                                    attr_names, attr_values,
                                    &handled,
                                    "rdf:resource", &license,
                                    NULL);

          g_set_str (&data->svg->license, license);
        }

      return;
    }

  if (strcmp (element_name, "font-face") == 0 ||
      strcmp (element_name, "font-face-src") == 0)
    {
      return;
    }

  if (strcmp (element_name, "font-face-uri") == 0)
    {
      if (check_ancestors (context, "font-face-src", "font-face", NULL))
        {
          for (unsigned int i = 0; attr_names[i]; i++)
            {
              if (strcmp (attr_names[i], "href") == 0)
                {
                  if (!add_font_from_url (data->svg, context, attr_names, attr_names[i], attr_values[i]))
                    {
                      // too bad
                    }
                }
            }
        }
      else
        skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring font element in the wrong context: <%s>", element_name);

      return;
    }

  if (strcmp (element_name, "style") == 0)
    {
      gboolean is_css = TRUE;

      for (unsigned int i = 0; attr_names[i]; i++)
        {
          if (strcmp (attr_names[i], "type") == 0)
            {
              if (strcmp (attr_values[i], "text/css") != 0)
                is_css = FALSE;
              break;
            }
        }

      g_string_set_size (data->text.text, 0);
      if (is_css)
        start_collect_text (data, context);

      return;
    }

  if (strcmp (element_name, "textPath") == 0 ||
      strcmp (element_name, "feConvolveMatrix") == 0 ||
      strcmp (element_name, "feDiffuseLighting") == 0 ||
      strcmp (element_name, "feMorphology") == 0 ||
      strcmp (element_name, "feSpecularLighting") == 0 ||
      strcmp (element_name, "feTurbulence") == 0)
    {
      skip_element (data, context, GTK_SVG_ERROR_NOT_IMPLEMENTED, "<%s> is not supported", element_name);
      return;
    }

  if (strcmp (element_name, "title") == 0 ||
      strcmp (element_name, "desc") == 0 ||
      g_str_has_prefix (element_name, "sodipodi:") ||
      g_str_has_prefix (element_name, "inkscape:"))
    {
      skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Ignoring metadata and non-standard elements: <%s>", element_name);
      return;
    }

  if (strcmp (element_name, "set") == 0)
    {
      SvgAnimation *a;
      const char *to_attr = NULL;
      SvgValue *value;
      GError *error = NULL;

      if ((data->svg->features & GTK_SVG_ANIMATIONS) == 0)
        {
          skip_element (data, context, GTK_SVG_ERROR_FEATURE_DISABLED, "SvgAnimations are disabled");
          return;
        }

      if (data->current_animation)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Nested animation elements are not allowed: <set>");
          return;
        }

      a = svg_animation_new (ANIMATION_TYPE_SET);

      markup_filter_attributes (element_name,
                                attr_names, attr_values,
                                &handled,
                                "to", &to_attr,
                                NULL);

      if (!parse_base_animation_attrs (a,
                                       element_name,
                                       attr_names, attr_values,
                                       &handled,
                                       data,
                                       context))
        {
          svg_animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Skipping <%s> - bad attributes", element_name);
          return;
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (!to_attr)
        {
          gtk_svg_missing_attribute (data->svg, context, "to", NULL);
          svg_animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Dropping <set> without 'to'");
          return;
        }

      a->calc_mode = CALC_MODE_DISCRETE;
      a->frames = g_new0 (Frame, 2);
      a->frames[0].time = 0;
      a->frames[1].time = 1;

      value = svg_property_parse_and_validate (a->attr, to_attr, &error);
      if (error)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "to", "%s", error->message);
          g_error_free (error);
        }
      else if (!value)
        {
          gtk_svg_invalid_attribute (data->svg, context, attr_names, "to", NULL);
        }

      if (!value)
        {
          svg_animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Dropping <set> without 'to'");
          return;
        }

      a->frames[0].value = svg_value_ref (value);
      a->frames[1].value = svg_value_ref (value);
      a->n_frames = 2;

      svg_value_unref (value);

      if (!a->href ||
          (data->current_shape != NULL &&
           g_strcmp0 (a->href, svg_element_get_id (data->current_shape)) == 0))
        {
          a->shape = data->current_shape;
          svg_element_add_animation (data->current_shape, a);
        }
      else
        g_ptr_array_add (data->pending_animations, a);

      if (a->id)
        g_hash_table_insert (data->animations, a->id, a);

      data->current_animation = a;

      return;
    }

  if (strcmp (element_name, "animate") == 0 ||
      strcmp (element_name, "animateTransform") == 0 ||
      strcmp (element_name, "animateMotion") == 0)
    {
      SvgAnimation *a;

      if ((data->svg->features & GTK_SVG_ANIMATIONS) == 0)
        {
          skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "SvgAnimations are disabled");
          return;
        }

      if (data->current_animation)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Nested animation elements are not allowed: <%s>", element_name);
          return;
        }

      if (strcmp (element_name, "animate") == 0)
        a = svg_animation_new (ANIMATION_TYPE_ANIMATE);
      else if (strcmp (element_name, "animateTransform") == 0)
        a = svg_animation_new (ANIMATION_TYPE_TRANSFORM);
      else
        a = svg_animation_new (ANIMATION_TYPE_MOTION);

      a->line = location.lines;

      if (!parse_base_animation_attrs (a,
                                       element_name,
                                       attr_names, attr_values,
                                       &handled,
                                       data,
                                       context))
        {
          svg_animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Skipping <%s> - bad attributes", element_name);
          return;
        }

      if (!parse_value_animation_attrs (a,
                                        element_name,
                                        attr_names, attr_values,
                                        &handled,
                                        data,
                                        context))
        {
          svg_animation_drop_and_free (a);
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Skipping <%s> - bad attributes", element_name);
          return;
        }

      if (a->type == ANIMATION_TYPE_MOTION)
        {
          if (!parse_motion_animation_attrs (a,
                                             element_name,
                                             attr_names, attr_values,
                                             &handled,
                                             data,
                                             context))
            {
              svg_animation_drop_and_free (a);
              skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "Skipping <%s>: bad attributes", element_name);
              return;
            }
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (data->current_shape != NULL &&
          (a->href == NULL ||
           g_strcmp0 (a->href, svg_element_get_id (data->current_shape)) == 0))
        {
          a->shape = data->current_shape;
          svg_element_add_animation (data->current_shape, a);
        }
      else
        g_ptr_array_add (data->pending_animations, a);

      if (a->id)
        g_hash_table_insert (data->animations, a->id, a);

      data->current_animation = a;

      return;
    }

  if (strcmp (element_name, "mpath") == 0)
    {
      const char *xlink_href_attr = NULL;
      const char *href_attr = NULL;
      const char *href_attr_name = "href";

      if (data->current_animation == NULL ||
          data->current_animation->type != ANIMATION_TYPE_MOTION ||
          data->current_animation->motion.path_ref != NULL)
        {
          skip_element (data, context, GTK_SVG_ERROR_INVALID_ELEMENT, "<mpath> only allowed in <animateMotion>");
          return;
        }

      for (unsigned int i = 0; attr_names[i]; i++)
        {
          if (strcmp (attr_names[i], "xlink:href") == 0)
            {
              xlink_href_attr = attr_values[i];
              handled |= BIT (i);
            }
          else if (strcmp (attr_names[i], "href") == 0)
            {
              href_attr = attr_values[i];
              handled |= BIT (i);
            }
        }

      if (xlink_href_attr && !href_attr)
        {
          href_attr = xlink_href_attr;
          href_attr_name = "xlink:href";
        }

      if (href_attr != NULL)
        {
          if (href_attr[0] != '#')
            gtk_svg_invalid_attribute (data->svg, context, attr_names, href_attr_name,
                                       "Missing '#' in href");
          else
            data->current_animation->motion.path_ref = g_strdup (href_attr + 1);
        }

      gtk_svg_check_unhandled_attributes (data->svg, context, attr_names, handled);

      if (!data->current_animation->motion.path_ref)
        gtk_svg_missing_attribute (data->svg, context, "href", NULL);

      return;
    }

  /* If we get here, its all over */
  skip_element (data, context, GTK_SVG_ERROR_IGNORED_ELEMENT, "Unknown element: <%s>", element_name);
}

static void
end_element_cb (GMarkupParseContext *context,
                const char          *element_name,
                gpointer             user_data,
                GError             **gmarkup_error)
{
  ParserData *data = user_data;
  SvgElementType shape_type;

  data->text.collect = FALSE;

  if (data->skip.to != NULL)
    {
      if (data->skip.to == g_markup_parse_context_get_element_stack (context))
        {
          GtkSvgLocation end;
          const char *parent;

          if (data->skip.to->next)
            parent = data->skip.to->next->data;
          else
            parent = NULL;

          gtk_svg_location_init_tag_end (&end, context);

          gtk_svg_skipped_element (data->svg,
                                   parent,
                                   &data->skip.start,
                                   &end,
                                   data->skip.code,
                                   "%s", data->skip.reason);
          g_clear_pointer (&data->skip.reason, g_free);
          data->skip.to = NULL;
          if (!data->skip.skip_over_target)
            goto do_target;
        }
      return;
    }

do_target:
  if (strcmp (element_name, "style") == 0)
    {
      if (data->text.text->len > 0)
        {
          char *string;
          StyleElt *elt;

          string = g_strdup (data->text.text->str);
          elt = g_new0 (StyleElt, 1);
          elt->content = g_bytes_new_take (string, strlen (string));
          elt->location = data->text.start;
          g_ptr_array_add (data->current_shape->styles, elt);
        }
    }
  else if (strcmp (element_name, "rdf:li") == 0)
    {
      g_set_str (&data->svg->keywords, data->text.text->str);
    }
  else if (strcmp (element_name, "dc:description") == 0)
    {
      g_set_str (&data->svg->description, data->text.text->str);
    }
  else if (strcmp (element_name, "dc:title") == 0)
    {
      g_set_str (&data->svg->author, data->text.text->str);
    }
  else if (svg_element_type_lookup (element_name, &shape_type))
    {
      GSList *tos = data->shape_stack;

      g_assert (shape_type == svg_element_get_type (data->current_shape));

      data->current_shape = tos->data;
      data->shape_stack = tos->next;
      g_slist_free_1 (tos);
    }
  else if (strcmp (element_name, "set") == 0 ||
           strcmp (element_name, "animate") == 0 ||
           strcmp (element_name, "animateTransform") == 0 ||
           strcmp (element_name, "animateMotion") == 0)
    {
      data->current_animation = NULL;
    }
}

static void
text_cb (GMarkupParseContext  *context,
         const char           *text,
         size_t                len,
         gpointer              user_data,
         GError              **error)
{
  ParserData *data = user_data;
  SvgElement *text_parent = NULL;

  if (!data->current_shape)
    return;

  if (svg_element_type_is_text (svg_element_get_type (data->current_shape)))
    text_parent = data->current_shape;
  else
    {
      SvgElement *parent = svg_element_get_parent (data->current_shape);
      if (parent &&
          svg_element_get_type (data->current_shape) == SVG_ELEMENT_LINK &&
           svg_element_type_is_text (svg_element_get_type (parent)))
        text_parent = parent;
    }

  if (text_parent)
    {
      TextNode node = {
        .type = TEXT_NODE_CHARACTERS,
        .characters = { .text = g_strndup (text, len) }
      };
      g_array_append_val (text_parent->text, node);
      return;
    }

  if (!data->text.collect)
    return;

  g_string_append_len (data->text.text, text, len);
}

static void
error_cb (GMarkupParseContext *context,
          GError              *error,
          gpointer             user_data)
{
  ParserData *data = user_data;

  gtk_svg_markup_error (data->svg, context, error);
}

/* {{{ Href handling, dependency tracking */

static void
add_dependency (SvgElement *shape0,
                SvgElement *shape1)
{
  if (!shape0->deps)
    shape0->deps = g_ptr_array_new ();
  g_ptr_array_add (shape0->deps, shape1);
}

/* Record the fact that when computing updated
 * values, shape2 must be handled before shape1
 */
static void
add_dependency_to_common_ancestor (SvgElement *shape0,
                                   SvgElement *shape1)
{
  SvgElement *anc0, *anc1;

  if (svg_element_common_ancestor (shape0, shape1, &anc0, &anc1))
    add_dependency (anc0, anc1);
}

static void
resolve_clip_ref (SvgValue   *value,
                  SvgElement *shape,
                  ParserData *data)
{
  if (svg_value_is_unset (value))
    return;

  if (svg_clip_get_kind (value) == CLIP_URL &&
      svg_clip_get_shape (value) == NULL)
    {
      const char *ref = svg_clip_get_id (value);
      SvgElement *target = g_hash_table_lookup (data->shapes, ref);
      if (!target)
        gtk_svg_invalid_reference (data->svg,
                                   "No path with ID %s (resolving clip-path)",
                                   ref);
      else if (target->type != SVG_ELEMENT_CLIP_PATH)
        gtk_svg_invalid_reference (data->svg,
                                   "SvgElement with ID %s not a <clipPath> (resolving clip-path)",
                                   ref);
      else
        {
          svg_clip_set_shape (value, target);
          if (shape)
            add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_mask_ref (SvgValue   *value,
                  SvgElement *shape,
                  ParserData *data)
{
  if (svg_value_is_unset (value))
    return;

  if (svg_mask_get_kind (value) == MASK_URL && svg_mask_get_shape (value) == NULL)
    {
      const char *id = svg_mask_get_id (value);
      SvgElement *target = g_hash_table_lookup (data->shapes, id);
      if (!target)
        gtk_svg_invalid_reference (data->svg, "No shape with ID %s (resolving mask)", id);
      else if (target->type != SVG_ELEMENT_MASK)
        gtk_svg_invalid_reference (data->svg, "SvgElement with ID %s not a <mask> (resolving mask)", id);
      else
        {
          svg_mask_set_shape (value, target);
          if (shape)
            add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_href_ref (SvgValue   *value,
                  SvgElement *shape,
                  ParserData *data)
{
  const char *ref;

  if (svg_value_is_unset (value))
    return;

  if (svg_href_get_kind (value) == HREF_NONE)
    return;

  /* Since hrefs can't be set in css, we can only ever
   * get here from a presentation attr, so we always
   * have a shape.
   */
  g_assert (shape != NULL);

  ref = svg_href_get_ref (value);
  if (svg_element_get_type (shape) == SVG_ELEMENT_IMAGE || svg_element_get_type (shape) == SVG_ELEMENT_FILTER)
    {
      GError *error = NULL;
      GdkTexture *texture;

      texture = get_texture (data->svg, ref, &error);
      svg_href_set_texture (value, texture);
      if (texture != NULL)
        return;

      if (svg_element_get_type (shape) == SVG_ELEMENT_IMAGE)
        {
          if (g_error_matches (error, GTK_SVG_ERROR, GTK_SVG_ERROR_FEATURE_DISABLED))
            gtk_svg_emit_error (data->svg, error);
          else
            gtk_svg_invalid_reference (data->svg, "Failed to load %s (resolving <image>): %s", ref, error->message);
          g_error_free (error);
          return; /* Image href is always external */
        }
      g_error_free (error);
    }

  if (svg_href_get_shape (value) == NULL)
    {
      const char *id = NULL;
      SvgElement *target = NULL;

      if (ref && ref[0] == '#')
        id = ref + 1;

      if (id)
        target = g_hash_table_lookup (data->shapes, id);

       if (!target)
         {
          if (id && svg_element_get_type (shape) == SVG_ELEMENT_LINK)
            {
              SvgAnimation *animation = svg_element_find_animation (data->svg->content, id);
              if (animation)
                {
                  svg_href_set_animation (value, animation);
                  return;
                }
            }

          gtk_svg_invalid_reference (data->svg,
                                     "No element with ID %s (resolving href in <%s>)",
                                     ref,
                                     svg_element_type_get_name (svg_element_get_type (shape)));
        }
      else if (svg_element_get_type (shape) == SVG_ELEMENT_USE &&
               svg_element_or_ancestor_has_type (shape, SVG_ELEMENT_CLIP_PATH) &&
               !svg_element_type_is_clip_path_content (target->type))
        {
          gtk_svg_invalid_reference (data->svg,
                                     "Can't include a <%s> in a <clipPath> via <use>",
                                     svg_element_type_get_name (target->type));
        }
      else
        {
          svg_href_set_shape (value, target);
          add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_marker_ref (SvgValue   *value,
                    SvgElement *shape,
                    ParserData *data)
{
  if (svg_value_is_unset (value))
    return;

  if (svg_href_get_kind (value) != HREF_NONE && svg_href_get_shape (value) == NULL)
    {
      const char *ref = svg_href_get_id (value);
      SvgElement *target = g_hash_table_lookup (data->shapes, ref);
      if (!target)
        {
          gtk_svg_invalid_reference (data->svg, "No shape with ID %s", ref);
        }
      else if (target->type != SVG_ELEMENT_MARKER)
        {
          gtk_svg_invalid_reference (data->svg,
                                     "SvgElement with ID %s not a <marker>",
                                     ref);
        }
      else
        {
          svg_href_set_shape (value, target);
          if (shape)
            add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_paint_ref (SvgValue   *value,
                   SvgElement *shape,
                   ParserData *data)
{
  SvgValue *paint = value;

  if (svg_value_is_unset (value))
    return;

  if (paint_is_server (svg_paint_get_kind (paint)) &&
      svg_paint_get_server_shape (paint) == NULL)
    {
      const char *ref = svg_paint_get_server_ref (paint);
      SvgElement *target = g_hash_table_lookup (data->shapes, ref);

      if (!target)
        {
          GtkSymbolicColor symbolic;

          if ((data->svg->features & GTK_SVG_EXTENSIONS) != 0 &&
              g_str_has_prefix (ref, "gpa:") &&
              parse_symbolic_color (ref + strlen ("gpa:"), &symbolic))
            return; /* Handled later */

          gtk_svg_invalid_reference (data->svg, "No shape with ID %s (resolving fill or stroke)", ref);
        }
      else if (target->type != SVG_ELEMENT_LINEAR_GRADIENT &&
               target->type != SVG_ELEMENT_RADIAL_GRADIENT &&
               target->type != SVG_ELEMENT_PATTERN)
        {
          gtk_svg_invalid_reference (data->svg, "SvgElement with ID %s not a paint server (resolving fill or stroke)", ref);
        }
      else
        {
          svg_paint_set_server_shape (paint, target);
          if (shape)
            add_dependency_to_common_ancestor (shape, target);
        }
    }
}

static void
resolve_attach_ref (SvgElement *shape,
                    ParserData *data)
{
  const char *id;
  double position;
  SvgElement *sh;

  svg_element_get_gpa_attachment (shape, &id, &position, &sh);
  if (id != NULL && sh == NULL)
    {
      sh = g_hash_table_lookup (data->shapes, id);
      svg_element_set_gpa_attachment (shape, id, position, sh);
    }
}

static void
resolve_filter_ref (SvgValue   *value,
                    SvgElement *shape,
                    ParserData *data)
{
  if (svg_value_is_unset (value))
    return;

  for (unsigned int i = 0; i < svg_filter_functions_get_length (value); i++)
    {
      if (svg_filter_functions_get_kind (value, i) == FILTER_REF &&
          svg_filter_functions_get_shape (value, i) == NULL)
        {
          const char *ref = svg_filter_functions_get_ref (value, i);
          SvgElement *target = g_hash_table_lookup (data->shapes, ref);
          if (!target)
            {
              gtk_svg_invalid_reference (data->svg,
                                         "No shape with ID %s (resolving filter)",
                                         ref);
            }
          else if (target->type != SVG_ELEMENT_FILTER)
            {
              gtk_svg_invalid_reference (data->svg,
                                         "SvgElement with ID %s not a filter (resolving filter)",
                                         ref);
            }
          else
            {
              svg_filter_functions_set_shape (value, i, target);
              if (shape)
                add_dependency_to_common_ancestor (shape, target);
            }
        }
    }
}

static void
resolve_refs_for_animation (SvgAnimation  *a,
                            ParserData *data)
{
  unsigned int first;

  for (int k = 0; k < 2; k++)
    {
      GPtrArray *specs = k == 0 ? a->begin : a->end;

      for (unsigned int j = 0; j < specs->len; j++)
        {
          TimeSpec *spec = g_ptr_array_index (specs, j);
          if (spec->type == TIME_SPEC_TYPE_SYNC && spec->sync.base == NULL)
            {
              g_assert (spec->sync.ref);
              spec->sync.base = g_hash_table_lookup (data->animations, spec->sync.ref);
              if (!spec->sync.base)
                gtk_svg_invalid_reference (data->svg, "No animation with ID %s", spec->sync.ref);
              else
                svg_animation_add_dep (spec->sync.base, a);
            }
        }
    }

  /* The resolve functions don't know how to handle
   * our special current keyword, and it gets resolved
   * later anyway, so skip it.
   */
  if (a->frames[0].value && svg_value_is_current (a->frames[0].value))
    first = 1;
  else
    first = 0;

  if (a->attr == SVG_PROPERTY_CLIP_PATH)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_clip_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_MASK)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_mask_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_HREF)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_href_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_MARKER_START ||
           a->attr == SVG_PROPERTY_MARKER_MID ||
           a->attr == SVG_PROPERTY_MARKER_END)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_marker_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_FILL ||
           a->attr == SVG_PROPERTY_STROKE)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_paint_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_FILTER)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_filter_ref (a->frames[j].value, a->shape, data);
    }
  else if (a->attr == SVG_PROPERTY_FE_IMAGE_HREF)
    {
      for (unsigned int j = first; j < a->n_frames; j++)
        resolve_href_ref (a->frames[j].value, a->shape, data);
    }

  if (a->motion.path_ref)
    {
      SvgElement *shape = g_hash_table_lookup (data->shapes, a->motion.path_ref);
      if (shape == NULL)
        gtk_svg_invalid_reference (data->svg,
                                   "No path with ID %s (resolving <mpath>",
                                   a->motion.path_ref);
      else if (!svg_element_type_is_path (svg_element_get_type (shape)))
        gtk_svg_invalid_reference (data->svg,
                                   "Element with ID %s is not a shape (resolving <mpath>",
                                   a->motion.path_ref);
      else
        {
          a->motion.path_shape = shape;
          add_dependency_to_common_ancestor (a->shape, a->motion.path_shape);
          if (a->id && g_str_has_prefix (a->id, "gpa:attachment:"))
            {
              /* a's path is attached to a->motion.path_shape
               * Make sure it moves along with transitions and animations
               */
              create_attachment_connection (a, a->motion.path_shape, data->svg->timeline);
            }
        }
    }
}

static void
resolve_animation_refs (SvgElement *shape,
                        ParserData *data)
{
  if (shape->animations)
    {
      for (unsigned int i = 0; i < shape->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (shape->animations, i);
          resolve_refs_for_animation (a, data);
        }
    }

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          resolve_animation_refs (sh, data);
        }
    }
}

static void
resolve_filter_image_refs (SvgElement *shape,
                           ParserData *data)
{
  if (svg_element_get_type (shape) != SVG_ELEMENT_FILTER)
    return;

  for (unsigned int i = 0; i < shape->filters->len; i++)
    {
      SvgFilter *f = g_ptr_array_index (shape->filters, i);
      SvgFilterType type = svg_filter_get_type (f);

      if (type == SVG_FILTER_IMAGE)
        resolve_href_ref (svg_filter_get_specified_value (f, SVG_PROPERTY_FE_IMAGE_HREF), shape, data);
    }
}

static void
resolve_ref_in_property (SvgProperty  attr,
                         SvgValue    *value,
                         SvgElement  *shape,
                         ParserData  *data)
{
  switch ((unsigned int) attr)
    {
    case SVG_PROPERTY_FILL:
    case SVG_PROPERTY_STROKE:
      resolve_paint_ref (value, shape, data);
      break;
    case SVG_PROPERTY_MASK:
      resolve_mask_ref (value, shape, data);
      break;
    case SVG_PROPERTY_MARKER_START:
    case SVG_PROPERTY_MARKER_MID:
    case SVG_PROPERTY_MARKER_END:
      resolve_marker_ref (value, shape, data);
      break;
    case SVG_PROPERTY_CLIP_PATH:
      resolve_clip_ref (value, shape, data);
      break;
    case SVG_PROPERTY_FILTER:
      resolve_filter_ref (value, shape, data);
      break;
    case SVG_PROPERTY_HREF:
      resolve_href_ref (value, shape, data);
      break;
    default:
      break;
    }
}

static void
resolve_refs_in_properties (PropertyValue *styles,
                            unsigned int   n_styles,
                            SvgElement    *shape,
                            ParserData    *data)
{
  for (unsigned int i = 0; i < n_styles; i++)
    {
      PropertyValue *p = &styles[i];
      resolve_ref_in_property (p->attr, p->value, shape, data);
    }
}

static void
resolve_refs_in_ruleset (SvgCssRuleset *r,
                         SvgElement    *shape,
                         ParserData    *data)
{
  resolve_refs_in_properties (r->styles, r->n_styles, shape, data);
}

static void
resolve_refs_in_array (GArray     *a,
                       SvgElement *shape,
                       ParserData *data)
{
  resolve_refs_in_properties ((PropertyValue *)a->data, a->len, shape, data);
}

static void
resolve_refs_in_rulesets (GArray    *rulesets,
                         SvgElement *shape,
                         ParserData *data)
{
  for (unsigned int i = 0; i < rulesets->len; i++)
    {
      SvgCssRuleset *r = &g_array_index (rulesets, SvgCssRuleset, i);
      resolve_refs_in_ruleset (r, shape, data);
    }
}

static gboolean
can_add (SvgElement *shape,
         GHashTable *waiting)
{
  if (shape->deps)
    {
      for (unsigned int i = 0; i < shape->deps->len; i++)
        {
          SvgElement *dep = g_ptr_array_index (shape->deps, i);
          if (g_hash_table_contains (waiting, dep))
            return FALSE;
        }
    }

  return TRUE;
}

static void
do_compute_update_order (SvgElement *shape,
                         GtkSvg     *svg,
                         GHashTable *waiting)
{
  unsigned int n_waiting;
  gboolean has_cycle = FALSE;
  SvgElement *last = NULL;

  if (!svg_element_type_is_container (svg_element_get_type (shape)))
    return;

  g_assert (g_hash_table_size (waiting) == 0);

  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      SvgElement *sh = g_ptr_array_index (shape->shapes, i);
      do_compute_update_order (sh, svg, waiting);
    }

  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      SvgElement *sh = g_ptr_array_index (shape->shapes, i);
      g_hash_table_add (waiting, sh);
    }

  n_waiting = g_hash_table_size (waiting);
  while (n_waiting > 0)
    {
      GHashTableIter iter;
      SvgElement *key;

      g_hash_table_iter_init (&iter, waiting);
      while (g_hash_table_iter_next (&iter, (gpointer *) &key, NULL))
        {
          if (can_add (key, waiting) || has_cycle)
            {
              if (last)
                last->next = key;
              else
                shape->first = key;
              last = key;
              last->next = NULL;
              g_hash_table_iter_remove (&iter);
            }
        }

      if (n_waiting == g_hash_table_size (waiting))
        {
          gtk_svg_update_error (svg, "Cyclic dependency detected");
          has_cycle = TRUE;
        }

      n_waiting = g_hash_table_size (waiting);
    }

  for (unsigned int i = 0; i < shape->shapes->len; i++)
    {
      SvgElement *sh = g_ptr_array_index (shape->shapes, i);
      g_clear_pointer (&sh->deps, g_ptr_array_unref);
    }
}

static void
compute_update_order (SvgElement *shape,
                      GtkSvg     *svg)
{
  GHashTable *waiting = g_hash_table_new (g_direct_hash, g_direct_equal);
  do_compute_update_order (shape, svg, waiting);
  g_hash_table_unref (waiting);
}

/* }}} */
/* {{{ CSS handling */

/* Most of this is adapted from gtkcssprovider.c */

#define GDK_ARRAY_NAME svg_css_selectors
#define GDK_ARRAY_TYPE_NAME SvgCssSelectors
#define GDK_ARRAY_ELEMENT_TYPE GtkCssSelector *
#define GDK_ARRAY_PREALLOC 64
#include "gdk/gdkarrayimpl.c"

static void
svg_css_ruleset_init_copy (SvgCssRuleset  *new,
                           SvgCssRuleset  *ruleset,
                           GtkCssSelector *selector)
{
  memcpy (new, ruleset, sizeof (SvgCssRuleset));
  new->selector = selector;
  if (ruleset->owns_styles)
    ruleset->owns_styles = FALSE;
}

void
svg_css_ruleset_clear (SvgCssRuleset *ruleset)
{
  if (ruleset->owns_styles)
    {
      for (unsigned int i = 0; i < ruleset->n_styles; i++)
        {
          svg_value_unref (ruleset->styles[i].value);
          ruleset->styles[i].value = NULL;
        }
      g_free (ruleset->styles);
    }

  if (ruleset->selector)
    _gtk_css_selector_free (ruleset->selector);

  memset (ruleset, 0, sizeof (SvgCssRuleset));
}

static void
svg_css_ruleset_add (SvgCssRuleset *ruleset,
                     SvgProperty    attr,
                     SvgValue      *value,
                     gboolean       important)
{
  unsigned int i;

  g_return_if_fail (ruleset->owns_styles || ruleset->n_styles == 0);

  ruleset->owns_styles = TRUE;

  for (i = 0; i < ruleset->n_styles; i++)
    {
      if (ruleset->styles[i].attr == attr &&
          ruleset->styles[i].important == important)
        {
          svg_value_unref (ruleset->styles[i].value);
          ruleset->styles[i].value = NULL;
          break;
        }
    }

  if (i == ruleset->n_styles)
    {
      ruleset->n_styles++;
      ruleset->styles = g_realloc (ruleset->styles, ruleset->n_styles * sizeof (PropertyValue));
      ruleset->styles[i].attr = attr;
    }

  ruleset->styles[i].value = value;
  ruleset->styles[i].important = important;
}

typedef struct _SvgCssScanner SvgCssScanner;
struct _SvgCssScanner
{
  ParserData *data;
  GtkCssParser *parser;
  SvgCssScanner *parent;
};

static void
svg_css_scanner_parser_error (GtkCssParser         *parser,
                              const GtkCssLocation *css_start,
                              const GtkCssLocation *css_end,
                              const GError         *css_error,
                              gpointer              user_data)
{
  ParserData *data = user_data;

  if (css_error->domain == GTK_CSS_PARSER_ERROR)
    {
      GError *error;
      GFile *file;
      GtkSvgLocation start = { 0, };
      GtkSvgLocation end = { 0, };

      error = g_error_new_literal (GTK_SVG_ERROR,
                                   GTK_SVG_ERROR_INVALID_SYNTAX,
                                   css_error->message);

      if ((file = gtk_css_parser_get_file (parser)) != NULL)
        gtk_svg_error_set_input (error, g_file_peek_path (file));
      else if (data->load_user_style)
        gtk_svg_error_set_input (error, "stylesheet");
      else
        {
          gtk_svg_error_set_input (error, "svg");
          start = data->text.start;
          end = data->text.start;
        }

      if (data->current_shape)
        {
          gtk_svg_error_set_element (error, svg_element_type_get_name (svg_element_get_type (data->current_shape)));
          gtk_svg_error_set_attribute (error, "style");
        }

      start.bytes += css_start->bytes;
      start.lines += css_start->lines;
      if (css_start->lines == 0)
        start.line_chars += css_start->line_chars;
      else
        start.line_chars = css_start->line_chars;

      end.bytes += css_end->bytes;
      end.lines += css_end->lines;
      if (css_end->lines == 0)
        end.line_chars += css_end->line_chars;
      else
        end.line_chars = css_end->line_chars;

      gtk_svg_error_set_location (error, &start, &end);

      gtk_svg_emit_error (data->svg, error);
      g_clear_error (&error);
    }
}


static SvgCssScanner *
svg_css_scanner_new (ParserData    *data,
                     SvgCssScanner *parent,
                     GFile         *file,
                     GBytes        *bytes)
{
  SvgCssScanner *scanner = g_new0 (SvgCssScanner, 1);

  scanner->data = data;
  scanner->parent = parent;
  scanner->parser = gtk_css_parser_new_for_bytes (bytes, file, svg_css_scanner_parser_error, data, NULL);

  return scanner;
}

static void
svg_css_scanner_destroy (SvgCssScanner *scanner)
{
  gtk_css_parser_unref (scanner->parser);
  g_free (scanner);
}

static int
svg_css_compare_rule (gconstpointer a_,
                      gconstpointer b_)
{
  const SvgCssRuleset *a = (const SvgCssRuleset *) a_;
  const SvgCssRuleset *b = (const SvgCssRuleset *) b_;

  /* Sort from highest to lowest specificity */
  return _gtk_css_selector_compare (b->selector, a->selector);
}

static void load_internal (ParserData    *data,
                           SvgCssScanner *scanner,
                           GFile         *file,
                           GBytes        *bytes);

static gboolean
svg_css_scanner_would_recurse (SvgCssScanner *scanner,
                               GFile         *file)
{
  while (scanner)
    {
      GFile *parser_file = gtk_css_parser_get_file (scanner->parser);
      if (parser_file && g_file_equal (parser_file, file))
        return TRUE;

      scanner = scanner->parent;
    }

  return FALSE;
}

static gboolean
parse_import (SvgCssScanner *scanner)
{
  GFile *file;

  if (!gtk_css_parser_try_at_keyword (scanner->parser, "import"))
    return FALSE;

  if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_STRING))
    {
      char *url = gtk_css_parser_consume_string (scanner->parser);

      if (url)
        {
          if (gtk_css_parser_get_file (scanner->parser))
            file = gtk_css_parser_resolve_url (scanner->parser, url);
          else
            file = g_file_new_for_path (url);

          if (file == NULL)
            {
              gtk_css_parser_error_import (scanner->parser,
                                           "Could not resolve \"%s\" to a valid URL",
                                           url);
            }
          g_free (url);
        }
      else
        file = NULL;
    }
  else
    {
      char *url = gtk_css_parser_consume_url (scanner->parser);

      if (url)
        {
          if (gtk_css_parser_get_file (scanner->parser))
            file = gtk_css_parser_resolve_url (scanner->parser, url);
          else
            file = g_file_new_for_uri (url);
          g_free (url);
        }
      else
        file = NULL;
    }

  if (file == NULL)
    {
      /* nothing to do; */
    }
  else if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_error_syntax (scanner->parser, "Expected ';'");
    }
  else if ((scanner->data->svg->features & GTK_SVG_EXTERNAL_RESOURCES) == 0)
    {
      /* Not allowed; TODO: emit an error */
    }
  else if (svg_css_scanner_would_recurse (scanner, file))
    {
       char *path = g_file_get_path (file);
       gtk_css_parser_error (scanner->parser,
                             GTK_CSS_PARSER_ERROR_IMPORT,
                             gtk_css_parser_get_block_location (scanner->parser),
                             gtk_css_parser_get_end_location (scanner->parser),
                             "Loading '%s' would recurse",
                             path);
       g_free (path);
    }
  else
    {
      GError *load_error = NULL;

      GBytes *bytes = g_file_load_bytes (file, NULL, NULL, &load_error);

      if (bytes == NULL)
        {
          gtk_css_parser_error (scanner->parser,
                                GTK_CSS_PARSER_ERROR_IMPORT,
                                gtk_css_parser_get_block_location (scanner->parser),
                                gtk_css_parser_get_end_location (scanner->parser),
                                "Failed to import: %s",
                                load_error->message);
          g_error_free (load_error);
        }
      else
        {
          load_internal (scanner->data, scanner, file, bytes);
          g_bytes_unref (bytes);
        }
    }

  g_clear_object (&file);

  return TRUE;
}


static void
parse_at_keyword (SvgCssScanner *scanner)
{
  gtk_css_parser_start_semicolon_block (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);
  if (!parse_import (scanner))
    gtk_css_parser_error_syntax (scanner->parser, "Unknown @ rule");
  gtk_css_parser_end_block (scanner->parser);
}

static void
clear_selectors (SvgCssSelectors *selectors)
{
  for (int i = 0; i < svg_css_selectors_get_size (selectors); i++)
    _gtk_css_selector_free (svg_css_selectors_get (selectors, i));
}

static void
parse_selector_list (SvgCssScanner   *scanner,
                     SvgCssSelectors *selectors)
{
  do
    {
      GtkCssSelector *selector = _gtk_css_selector_parse (scanner->parser);

      if (selector == NULL)
        {
          clear_selectors (selectors);
          svg_css_selectors_clear (selectors);
          return;
        }

      svg_css_selectors_append (selectors, selector);
    }
  while (gtk_css_parser_try_token (scanner->parser, GTK_CSS_TOKEN_COMMA));
}

static gboolean
lookup_property (const char  *name,
                 SvgProperty *result,
                 gboolean    *marker)
{
  if (strcmp (name, "marker") == 0)
    {
      *marker = TRUE;
      *result = SVG_PROPERTY_MARKER_START;
      return TRUE;
    }

  *marker = FALSE;
  return svg_property_lookup_for_css (name, result);
}

static void
parse_declaration (SvgCssScanner *scanner,
                   PropertyValue *p,
                   gboolean      *marker)
{
  char *name;

  p->value = NULL;
  p->important = FALSE;
  *marker = FALSE;

  /* advance the location over whitespace */
  gtk_css_parser_get_token (scanner->parser);
  gtk_css_parser_start_semicolon_block (scanner->parser, GTK_CSS_TOKEN_EOF);

  if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      gtk_css_parser_warn_syntax (scanner->parser, "Empty declaration");
      gtk_css_parser_end_block (scanner->parser);
      return;
    }

  name = gtk_css_parser_consume_ident (scanner->parser);
  if (name == NULL)
    goto out;

  if (lookup_property (name, &p->attr, marker))
    {
      if (!gtk_css_parser_try_token (scanner->parser, GTK_CSS_TOKEN_COLON))
        {
          gtk_css_parser_error_syntax (scanner->parser, "Expected ':'");
          goto out;
        }

      p->value = svg_property_parse_css (p->attr, scanner->parser);

      if (p->value == NULL)
        goto out;

      if (gtk_css_parser_try_delim (scanner->parser, '!') &&
          gtk_css_parser_try_ident (scanner->parser, "important"))
        {
          p->important = TRUE;
        }

      if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
        {
          gtk_css_parser_error_syntax (scanner->parser, "Junk at end of value for %s", name);
          g_clear_pointer (&p->value, svg_value_unref);
          goto out;
        }
    }
  else
    {
      gtk_css_parser_error_value (scanner->parser, "No property named \"%s\"", name);
    }

out:
  g_free (name);
  gtk_css_parser_end_block (scanner->parser);
}

static void
parse_declarations_into_ruleset (SvgCssScanner *scanner,
                                 SvgCssRuleset *r)
{
  while (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      PropertyValue p = { 0, };
      gboolean marker;

      parse_declaration (scanner, &p, &marker);
      if (p.value)
        {
          svg_css_ruleset_add (r, p.attr, svg_value_ref (p.value), p.important);
          if (marker)
            {
              svg_css_ruleset_add (r, SVG_PROPERTY_MARKER_MID, svg_value_ref (p.value), p.important);
              svg_css_ruleset_add (r, SVG_PROPERTY_MARKER_END, svg_value_ref (p.value), p.important);
            }
          svg_value_unref (p.value);
        }
    }
}

static void
parse_declarations_into_array (SvgCssScanner *scanner,
                               GArray        *styles)
{
  while (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      PropertyValue p = { 0, };
      gboolean marker;

      parse_declaration (scanner, &p, &marker);
      if (p.value)
        {
          g_array_append_val (styles, p);
          if (marker)
            {
              p.attr = SVG_PROPERTY_MARKER_MID;
              svg_value_ref (p.value);
              g_array_append_val (styles, p);
              p.attr = SVG_PROPERTY_MARKER_END;
              svg_value_ref (p.value);
              g_array_append_val (styles, p);
            }
        }
    }
}

static void
parse_style_into_properties (GArray     *styles,
                             const char *style,
                             ParserData *data)
{
  GBytes *bytes;
  SvgCssScanner *scanner;

  if (style == NULL)
    return;

  bytes = g_bytes_new_static (style, strlen (style));
  scanner = svg_css_scanner_new (data, NULL, NULL, bytes);
  parse_declarations_into_array (scanner, styles);
  svg_css_scanner_destroy (scanner);
  g_bytes_unref (bytes);
}

static void
commit_ruleset (GArray          *rulesets,
                SvgCssSelectors *selectors,
                SvgCssRuleset   *ruleset)
{
  if (ruleset->styles == NULL)
    return;

  for (unsigned int i = 0; i < svg_css_selectors_get_size (selectors); i++)
    {
      SvgCssRuleset *new;

      g_array_set_size (rulesets, rulesets->len + 1);
      new = &g_array_index (rulesets, SvgCssRuleset, rulesets->len - 1);
      svg_css_ruleset_init_copy (new, ruleset, gtk_css_selector_copy (svg_css_selectors_get (selectors, i)));
    }
}

static void
parse_ruleset (SvgCssScanner *scanner)
{
  SvgCssSelectors selectors;
  SvgCssRuleset ruleset = { 0, };

  svg_css_selectors_init (&selectors);

  parse_selector_list (scanner, &selectors);
  if (svg_css_selectors_get_size (&selectors) == 0)
    {
      gtk_css_parser_skip_until (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);
      gtk_css_parser_skip (scanner->parser);
      goto out;
    }

  if (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY))
    {
      gtk_css_parser_error_syntax (scanner->parser, "Expected '{' after selectors");
      clear_selectors (&selectors);
      gtk_css_parser_skip_until (scanner->parser, GTK_CSS_TOKEN_OPEN_CURLY);
      gtk_css_parser_skip (scanner->parser);
      goto out;
    }

  gtk_css_parser_start_block (scanner->parser);
  parse_declarations_into_ruleset (scanner, &ruleset);
  gtk_css_parser_end_block (scanner->parser);
  if (scanner->data->load_user_style)
    commit_ruleset (scanner->data->svg->user_styles, &selectors, &ruleset);
  else
    commit_ruleset (scanner->data->svg->author_styles, &selectors, &ruleset);
  svg_css_ruleset_clear (&ruleset);
  clear_selectors (&selectors);

out:
  svg_css_selectors_clear (&selectors);
}

static void
parse_statement (SvgCssScanner *scanner)
{
  if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_AT_KEYWORD))
    parse_at_keyword (scanner);
  else
    parse_ruleset (scanner);
}

static void
parse_stylesheet (SvgCssScanner *scanner)
{
  while (!gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_EOF))
    {
      if (gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_CDO) ||
          gtk_css_parser_has_token (scanner->parser, GTK_CSS_TOKEN_CDC))
        {
          gtk_css_parser_consume_token (scanner->parser);
          continue;
        }

      parse_statement (scanner);
    }
}

static void
load_internal (ParserData    *data,
               SvgCssScanner *parent,
               GFile         *file,
               GBytes        *bytes)
{
  SvgCssScanner *scanner;

  scanner = svg_css_scanner_new (data, parent, file, bytes);
  parse_stylesheet (scanner);
  svg_css_scanner_destroy (scanner);
}

static void
load_styles_for_shape (SvgElement *shape,
                       ParserData *data)
{
  for (unsigned int i = 0; i < shape->styles->len; i++)
    {
      StyleElt *elt = g_ptr_array_index (shape->styles, i);
      data->text.start = elt->location;
      load_internal (data, NULL, NULL, elt->content);
    }

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          load_styles_for_shape (sh, data);
        }
    }
}

static void
load_author_styles (ParserData *data)
{
  data->current_shape = NULL;
  data->load_user_style = FALSE;
  load_styles_for_shape (data->svg->content, data);

  g_array_sort (data->svg->author_styles, svg_css_compare_rule);
  resolve_refs_in_rulesets (data->svg->author_styles, NULL, data);
}

static void
load_user_styles (ParserData *data)
{
  if (data->svg->stylesheet == NULL)
    return;

  data->current_shape = NULL;
  data->load_user_style = TRUE;
  load_internal (data, NULL, NULL, data->svg->stylesheet);

  g_array_sort (data->svg->user_styles, svg_css_compare_rule);
  resolve_refs_in_rulesets (data->svg->user_styles, NULL, data);
}

static void
load_inline_style_here (SvgElement *element,
                        gpointer    user_data)
{
  ParserData *data = user_data;
  const char *style;

  data->current_shape = element;

  style = svg_element_get_style (element, &data->text.start);
  data->text.start.line_chars += strlen ("style='");
  data->text.start.bytes += strlen ("style='");
  parse_style_into_properties (element->inline_styles, style, data);

  if (svg_element_type_is_gradient (element->type))
    {
      for (unsigned int idx = 0; idx < element->color_stops->len; idx++)
        {
          SvgColorStop *stop = g_ptr_array_index (element->color_stops, idx);

          style = svg_color_stop_get_style (stop);
          svg_color_stop_get_origin (stop, &data->text.start);
          data->text.start.line_chars += strlen ("style='");
          data->text.start.bytes += strlen ("style='");
          parse_style_into_properties (svg_color_stop_get_inline_styles (stop),
                                       style,
                                       data);
        }
    }

  if (svg_element_type_is_filter (element->type))
    {
      for (unsigned int idx = 0; idx < element->filters->len; idx++)
        {
          SvgFilter *f = g_ptr_array_index (element->filters, idx);

          style = svg_filter_get_style (f);
          svg_filter_get_origin (f, &data->text.start);
          data->text.start.line_chars += strlen ("style='");
          data->text.start.bytes += strlen ("style='");
          parse_style_into_properties (svg_filter_get_inline_styles (f),
                                       style,
                                       data);
        }
    }
}

static void
load_inline_styles (ParserData *data)
{
  svg_element_foreach (data->svg->content, load_inline_style_here, data);
}

static void
resolve_refs_here (SvgElement *element,
                   gpointer    user_data)
{
  ParserData *data = user_data;

  resolve_refs_in_array (element->specified, element, data);
  resolve_refs_in_array (element->inline_styles, element, data);

  /* I don't think refs are possible in styles of color stops or filters */

  resolve_attach_ref (element, data);
  resolve_filter_image_refs (element, data);
}

static void
resolve_refs_in_shapes (ParserData *data)
{
  svg_element_foreach (data->svg->content, resolve_refs_here, data);
}

#if 0
static gboolean
ruleset_has_important (SvgCssRuleset *r,
                        gboolean      important)
{
  for (unsigned int j = 0; j < r->n_styles; j++)
    {
      PropertyValue *p = &r->styles[j];
      if (p->important == important)
        return TRUE;
    }
  return FALSE;
}

static void
dump_rulesets (GString  *s,
               GArray   *rulesets,
               gboolean  important)
{
  for (unsigned int i = 0; i < rulesets->len; i++)
    {
      SvgCssRuleset *r = &g_array_index (rulesets, SvgCssRuleset, i);

      if (!ruleset_has_important (r, important))
        continue;

      _gtk_css_selector_print (r->selector, s);
      g_string_append (s, " {\n");
      for (unsigned int j = 0; j < r->n_styles; j++)
        {
          PropertyValue *p = &r->styles[j];
          if (p->important != important)
            continue;
          g_string_append (s, "  ");
          g_string_append (s, svg_property_get_name (p->attr));
          g_string_append (s, ": ");
          svg_value_print (p->value, s);
          g_string_append (s, ";");
          g_string_append (s, "\n");
        }
      g_string_append (s, "}\n");
    }
}

static void
dump_styles (ParserData *data)
{
  GString *s = g_string_new ("");

  if (data->svg->user_styles->len > 0)
    {
      g_string_append (s, "/* user styles */\n");
      dump_rulesets (s, data->svg->user_styles, FALSE);
    }
  if (data->svg->author_styles->len > 0)
    {
      g_string_append (s, "/* author styles */\n");
      dump_rulesets (s, data->svg->author_styles, FALSE);
      g_string_append (s, "/* important author styles */\n");
      dump_rulesets (s, data->svg->author_styles, TRUE);
    }
  if (data->svg->user_styles->len > 0)
    {
      g_string_append (s, "/* important user styles */\n");
      dump_rulesets (s, data->svg->user_styles, TRUE);
    }

  g_print ("%s", s->str);
  g_string_free (s, TRUE);
}
#endif

static void
apply_ruleset_to_shape (SvgCssRuleset  *r,
                        gboolean        important,
                        SvgElement     *shape,
                        unsigned int    idx,
                        GtkBitmask    **set)
{
  for (unsigned int j = 0; j < r->n_styles; j++)
    {
      PropertyValue *p = &r->styles[j];

      if (_gtk_bitmask_get (*set, p->attr))
        continue;

      if (important != p->important)
        continue;

      if (svg_property_applies_to (p->attr, svg_element_get_type (shape)))
        shape_set_base_value (shape, p->attr, idx, p->value);

      *set = _gtk_bitmask_set (*set, p->attr, TRUE);
    }
}

static void
apply_styles_here (SvgElement   *shape,
                   unsigned int  idx,
                   GtkSvg       *svg)
{
  GtkCssNode *node;
  GtkBitmask *set;
  GArray *inline_styles = NULL;

  if (idx > 0)
    {
      if (svg_element_type_is_gradient (svg_element_get_type (shape)))
        {
          SvgColorStop *stop = g_ptr_array_index (shape->color_stops, idx - 1);
          node = svg_color_stop_get_css_node (stop);
          inline_styles = svg_color_stop_get_inline_styles (stop);
        }
      else
        {
          SvgFilter *ff = g_ptr_array_index (shape->filters, idx - 1);
          node = svg_filter_get_css_node (ff);
          inline_styles = svg_filter_get_inline_styles (ff);
        }
    }
  else
    {
      node = svg_element_get_css_node (shape);
      inline_styles = shape->inline_styles;
    }

  if (node == NULL)
    return;

  /* Our strategy to compute base values is the following:
   * - unset everything
   * - apply presentation attributes and style values, in priority order
   * - what is left unset at compute time will be inherited or set to initial values
   */

  /* unset */
  if (idx == 0)
    {
      for (unsigned int i = FIRST_SVG_PROPERTY; i <= LAST_SVG_PROPERTY; i++)
        shape_set_base_value (shape, i, idx, svg_unset_new ());
    }
  else if (svg_element_type_is_gradient (shape->type))
    {
      for (unsigned int i = FIRST_STOP_PROPERTY; i <= LAST_STOP_PROPERTY; i++)
        shape_set_base_value (shape, i, idx, svg_unset_new ());
    }
  else if (svg_element_type_is_filter (shape->type))
    {
      for (unsigned int i = FIRST_FILTER_PROPERTY; i <= LAST_FILTER_PROPERTY; i++)
        shape_set_base_value (shape, i, idx, svg_unset_new ());
    }
  else
    g_assert_not_reached ();

  /* Work from the highest priority downwards,
   * and keep a bitmask of attrs we've already set.
   */
  set = _gtk_bitmask_new ();

  /* important user styles */
  for (unsigned int i = 0; i < svg->user_styles->len; i++)
    {
      SvgCssRuleset *r = &g_array_index (svg->user_styles, SvgCssRuleset, i);
      if (gtk_css_selector_matches (r->selector, node))
        apply_ruleset_to_shape (r, TRUE, shape, idx, &set);
    }

  /* important inline styles */
  if (inline_styles)
    {
      for (unsigned int i = 0; i < inline_styles->len; i++)
        {
          PropertyValue *p = &g_array_index (inline_styles, PropertyValue, i);
          if (p->important && !_gtk_bitmask_get (set, p->attr))
            {
              shape_set_base_value (shape, p->attr, idx, p->value);
              set = _gtk_bitmask_set (set, p->attr, TRUE);
            }
        }
    }

  /* important author styles */
  for (unsigned int i = 0; i < svg->author_styles->len; i++)
    {
      SvgCssRuleset *r = &g_array_index (svg->author_styles, SvgCssRuleset, i);
      if (gtk_css_selector_matches (r->selector, node))
        apply_ruleset_to_shape (r, TRUE, shape, idx, &set);
    }

  /* inline styles */
  if (inline_styles)
    {
      for (unsigned int i = 0; i < inline_styles->len; i++)
        {
          PropertyValue *p = &g_array_index (inline_styles, PropertyValue, i);
          if (!p->important && !_gtk_bitmask_get (set, p->attr))
            {
              shape_set_base_value (shape, p->attr, idx, p->value);
              set = _gtk_bitmask_set (set, p->attr, TRUE);
            }
        }
    }

  /* author styles */
  for (unsigned int i = 0; i < svg->author_styles->len; i++)
    {
      SvgCssRuleset *r = &g_array_index (svg->author_styles, SvgCssRuleset, i);
      if (gtk_css_selector_matches (r->selector, node))
        apply_ruleset_to_shape (r, FALSE, shape, idx, &set);
    }

  /* user styles */
  for (unsigned int i = 0; i < svg->user_styles->len; i++)
    {
      SvgCssRuleset *r = &g_array_index (svg->user_styles, SvgCssRuleset, i);
      if (gtk_css_selector_matches (r->selector, node))
        apply_ruleset_to_shape (r, FALSE, shape, idx, &set);
    }

  /* presentation attributes */
  if (idx == 0)
    {
      for (unsigned int i = 0; i < shape->specified->len; i++)
        {
          PropertyValue *p = &g_array_index (shape->specified, PropertyValue, i);
          if (!_gtk_bitmask_get (set, p->attr))
            svg_element_set_base_value (shape, p->attr, p->value);
        }
    }
  else if (svg_element_type_is_gradient (shape->type))
    {
      SvgColorStop *stop = g_ptr_array_index (shape->color_stops, idx - 1);
      for (unsigned int attr = FIRST_STOP_PROPERTY; attr <= LAST_STOP_PROPERTY; attr++)
        {
          if (!_gtk_bitmask_get (set, attr))
            {
              SvgValue *value = svg_color_stop_get_specified_value (stop, attr);
              if (value)
                svg_color_stop_set_base_value (stop, attr, value);
            }
        }
    }
  else if (svg_element_type_is_filter (shape->type))
    {
      SvgFilter *filter = g_ptr_array_index (shape->filters, idx - 1);
      SvgFilterType filter_type = svg_filter_get_type (filter);
      unsigned int n_attrs = svg_filter_type_get_n_attrs (filter_type);
      for (unsigned int i = 0; i < n_attrs; i++)
        {
          SvgProperty attr = svg_filter_type_get_property (filter_type, i);
          if (!_gtk_bitmask_get (set, attr))
            {
              SvgValue *value = svg_filter_get_specified_value (filter, attr);
              if (value)
                svg_filter_set_base_value (filter, attr, value);
            }
        }
    }
  else
    g_assert_not_reached ();

   _gtk_bitmask_free (set);
}

void
apply_styles_to_shape (SvgElement *shape,
                       GtkSvg     *svg)
{
  apply_styles_here (shape, 0, svg);

  if (svg_element_type_is_container (svg_element_get_type (shape)))
    {
      for (unsigned int i = 0; i < shape->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (shape->shapes, i);
          apply_styles_to_shape (sh, svg);
        }
    }

  if (svg_element_type_is_gradient (svg_element_get_type (shape)))
    {
      for (unsigned int idx = 0; idx < shape->color_stops->len; idx++)
        apply_styles_here (shape, idx + 1, svg);
    }

  if (svg_element_type_is_filter (svg_element_get_type (shape)))
    {
      for (unsigned int idx = 0; idx < shape->filters->len; idx++)
        apply_styles_here (shape, idx + 1, svg);
    }

  /* Apply traditional symbolic heuristics *after*
   * CSS and styles, so that these take precedence.
   */
  if (svg->gpa_version == 0 &&
      ((svg->features & GTK_SVG_TRADITIONAL_SYMBOLIC) != 0))
    {
      GStrv classes = svg_element_get_classes (shape);
      SvgValue *value;
      gboolean has_stroke;

      if (!classes)
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);
      else if (g_strv_has (classes, "transparent-fill"))
        value = svg_paint_new_none ();
      else if (g_strv_has (classes, "foreground-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);
      else if (g_strv_has (classes, "success") ||
               g_strv_has (classes, "success-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_SUCCESS);
      else if (g_strv_has (classes, "warning") ||
               g_strv_has (classes, "warning-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_WARNING);
      else if (g_strv_has (classes, "error") ||
               g_strv_has (classes, "error-fill"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_ERROR);
      else
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);

      svg_element_set_base_value (shape, SVG_PROPERTY_FILL, value);
      svg_value_unref (value);

      if (!classes)
        value = svg_paint_new_none ();
      else if (g_strv_has (classes, "success-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_SUCCESS);
      else if (g_strv_has (classes, "warning-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_WARNING);
      else if (g_strv_has (classes, "error-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_ERROR);
      else if (g_strv_has (classes, "foreground-stroke"))
        value = svg_paint_new_symbolic (GTK_SYMBOLIC_COLOR_FOREGROUND);
      else
        value = svg_paint_new_none ();

      has_stroke = !svg_value_equal (value, svg_paint_new_none ());
      svg_element_set_base_value (shape, SVG_PROPERTY_STROKE, value);
      svg_value_unref (value);

      if (has_stroke)
        {
          value = svg_number_new (2);
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_WIDTH, value);
          svg_value_unref (value);

          value = svg_linejoin_new (GSK_LINE_JOIN_ROUND);
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_LINEJOIN, value);
          svg_value_unref (value);

          value = svg_linecap_new (GSK_LINE_CAP_ROUND);
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_LINECAP, value);
          svg_value_unref (value);
        }
    }

  /* gpa attrs are supported to have higher priority than
   * style and CSS, so re-set them here
   */
  if (svg->gpa_version > 0)
    {
      if (svg_element_get_gpa_fill (shape))
        svg_element_set_base_value (shape, SVG_PROPERTY_FILL, svg_element_get_gpa_fill (shape));
      if (svg_element_get_gpa_stroke (shape))
        svg_element_set_base_value (shape, SVG_PROPERTY_STROKE, svg_element_get_gpa_stroke (shape));
      if (svg_element_get_gpa_width (shape))
        svg_element_set_base_value (shape, SVG_PROPERTY_STROKE_WIDTH, svg_element_get_gpa_width (shape));
    }

  if (svg_element_is_specified (shape, SVG_PROPERTY_COLOR))
    {
      SvgValue *color = svg_element_get_base_value (shape, SVG_PROPERTY_COLOR);

      if ((svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          svg_color_get_kind (color) == COLOR_SYMBOLIC)
        {
          SvgValue *value = svg_color_new_black ();
          svg_element_set_base_value (shape, SVG_PROPERTY_COLOR, value);
          svg_value_unref (value);
          color = svg_element_get_base_value (shape, SVG_PROPERTY_COLOR);
        }

      if (svg_color_get_kind (color) == COLOR_SYMBOLIC)
        svg->used |= BIT (svg_color_get_symbolic (color) + 1);
    }

  if (svg_element_is_specified (shape, SVG_PROPERTY_FILL))
    {
      SvgValue *paint = svg_element_get_base_value (shape, SVG_PROPERTY_FILL);
      GtkSymbolicColor symbolic;

      if ((svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          svg_paint_get_kind (paint) == PAINT_SYMBOLIC)
        {
          SvgValue *value = svg_paint_new_black ();
          svg_element_set_base_value (shape, SVG_PROPERTY_FILL, value);
          svg_value_unref (value);
          paint = svg_element_get_base_value (shape, SVG_PROPERTY_FILL);
        }

      if (svg_paint_is_symbolic (paint, &symbolic))
        svg->used |= BIT (symbolic + 1);
    }

  if (svg_element_is_specified (shape, SVG_PROPERTY_STROKE))
    {
      SvgValue *paint = svg_element_get_base_value (shape, SVG_PROPERTY_STROKE);
      GtkSymbolicColor symbolic;

      if ((svg->features & GTK_SVG_EXTENSIONS) == 0 &&
          svg_paint_get_kind (paint) == PAINT_SYMBOLIC)
        {
          SvgValue *value = svg_paint_new_black ();
          svg_element_set_base_value (shape, SVG_PROPERTY_STROKE, value);
          svg_value_unref (value);
          paint = svg_element_get_base_value (shape, SVG_PROPERTY_STROKE);
        }

      if (svg_paint_is_symbolic (paint, &symbolic))
        svg->used |= BIT (symbolic + 1);

      if (svg_paint_get_kind (paint) != PAINT_NONE)
        svg->used |= GTK_SVG_USES_STROKES;
    }
}

/* }}} */

void
gtk_svg_init_from_bytes (GtkSvg *self,
                         GBytes *bytes)
{
  ParserData data;
  GMarkupParseContext *context;
  GMarkupParser parser = {
    start_element_cb,
    end_element_cb,
    text_cb,
    NULL,
    error_cb,
  };

  g_clear_pointer (&self->content, svg_element_free);

  if ((self->features & GTK_SVG_SYSTEM_RESOURCES) == 0)
    {
      g_assert (self->fontmap == NULL);
      ensure_fontmap (self);
    }

  data.svg = self;
  data.current_shape = NULL;
  data.shape_stack = NULL;
  data.shapes = g_hash_table_new (g_str_hash, g_str_equal);
  data.animations = g_hash_table_new (g_str_hash, g_str_equal);
  data.current_animation = NULL;
  data.pending_animations = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_animation_free);
  data.skip.to = NULL;
  data.skip.reason = NULL;
  data.text.text = g_string_new ("");
  data.text.collect = FALSE;
  data.num_loaded_elements = 0;

  context = g_markup_parse_context_new (&parser,
                                        G_MARKUP_PREFIX_ERROR_POSITION |
                                        G_MARKUP_TREAT_CDATA_AS_TEXT,
                                        &data, NULL);
  if (!g_markup_parse_context_parse (context,
                                     g_bytes_get_data (bytes, NULL),
                                     g_bytes_get_size (bytes),
                                     NULL) ||
      !g_markup_parse_context_end_parse (context, NULL))
    {
      gtk_svg_clear_content (self);
      g_slist_free (data.shape_stack);
      g_clear_pointer (&data.skip.reason, g_free);

      g_ptr_array_set_size (data.pending_animations, 0);
    }
  else
    {
      g_assert (data.current_shape == NULL);
      g_assert (data.shape_stack == NULL);
      g_assert (data.current_animation == NULL);
      g_assert (data.skip.to == NULL);
      g_assert (data.skip.reason == NULL);
    }

  g_markup_parse_context_free (context);

  if (self->content == NULL)
    self->content = svg_element_new (NULL, SVG_ELEMENT_SVG);

  load_user_styles (&data);
  load_author_styles (&data);
  load_inline_styles (&data);

  apply_styles_to_shape (self->content, self);
  resolve_refs_in_shapes (&data);

  if (svg_element_is_specified (self->content, SVG_PROPERTY_VIEW_BOX))
    {
      graphene_rect_t vb;

      svg_view_box_get (self->content->base[SVG_PROPERTY_VIEW_BOX], &vb);
      self->width = vb.size.width;
      self->height = vb.size.height;
    }

  if (svg_element_is_specified (self->content, SVG_PROPERTY_WIDTH))
    {
      SvgUnit unit = svg_number_get_unit (self->content->base[SVG_PROPERTY_WIDTH]);
      double value = svg_number_get (self->content->base[SVG_PROPERTY_WIDTH], 100);

      if (is_absolute_length (unit))
        self->width = absolute_length_to_px (value, unit);
      else if (unit != SVG_UNIT_PERCENTAGE)
        self->width = value;
    }

  if (svg_element_is_specified (self->content, SVG_PROPERTY_HEIGHT))
    {
      SvgUnit unit = svg_number_get_unit (self->content->base[SVG_PROPERTY_HEIGHT]);
      double value = svg_number_get (self->content->base[SVG_PROPERTY_HEIGHT], 100);

      if (is_absolute_length (unit))
        self->height = absolute_length_to_px (value, unit);
      else if (unit != SVG_UNIT_PERCENTAGE)
        self->height = value;
    }

  if (!svg_element_is_specified (self->content, SVG_PROPERTY_VIEW_BOX) &&
      !svg_element_is_specified (self->content, SVG_PROPERTY_WIDTH) &&
      !svg_element_is_specified (self->content, SVG_PROPERTY_HEIGHT))
    {
      /* arbitrary */
      self->width = 200;
      self->height = 200;
    }

  for (unsigned int i = 0; i < data.pending_animations->len; i++)
    {
      SvgAnimation *a = g_ptr_array_index (data.pending_animations, i);
      SvgElement *shape;

      g_assert (a->href != NULL);
      g_assert (a->shape == NULL);

      shape = g_hash_table_lookup (data.shapes, a->href);
      if (!shape)
        {
          gtk_svg_invalid_reference (self,
                                     "No shape with ID %s (resolving begin or end attribute)",
                                     a->href);
          svg_animation_free (a);
        }
      else
        {
          a->shape = shape;
          svg_element_add_animation (shape, a);
        }
    }

  /* Faster than stealing the items out of the array one-by-one */
  g_ptr_array_set_free_func (data.pending_animations, NULL);
  g_ptr_array_set_size (data.pending_animations, 0);

  resolve_animation_refs (self->content, &data);

  compute_update_order (self->content, self);

  self->state_change_delay = timeline_get_state_change_delay (self->timeline);

  g_hash_table_unref (data.shapes);
  g_hash_table_unref (data.animations);
  g_ptr_array_unref (data.pending_animations);
  g_string_free (data.text.text, TRUE);

  if (self->gpa_version > 0 &&
      (self->features & GTK_SVG_ANIMATIONS) == 0)
    apply_state (self, self->state);
}

void
gtk_svg_init_from_resource (GtkSvg     *self,
                            const char *path)
{
  GBytes *bytes;

  bytes = g_resources_lookup_data (path, 0, NULL);
  if (bytes)
    {
      gtk_svg_init_from_bytes (self, bytes);
      g_bytes_unref (bytes);
    }
}

/* }}} */
