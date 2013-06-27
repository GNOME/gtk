/* GTK - The GIMP Toolkit
 * Copyright (C) 2011 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkcsseasevalueprivate.h"

#include <math.h>

typedef enum {
  GTK_CSS_EASE_CUBIC_BEZIER,
  GTK_CSS_EASE_STEPS
} GtkCssEaseType;

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssEaseType type;
  union {
    struct {
      double x1;
      double y1;
      double x2;
      double y2;
    } cubic;
    struct {
      guint steps;
      gboolean start;
    } steps;
  } u;
};

static void
gtk_css_value_ease_free (GtkCssValue *value)
{
  g_slice_free (GtkCssValue, value);
}

static GtkCssValue *
gtk_css_value_ease_compute (GtkCssValue             *value,
                            guint                    property_id,
                            GtkStyleProviderPrivate *provider,
			    int                      scale,
                            GtkCssComputedValues    *values,
                            GtkCssComputedValues    *parent_values,
                            GtkCssDependencies      *dependencies)
{
  return _gtk_css_value_ref (value);
}

static gboolean
gtk_css_value_ease_equal (const GtkCssValue *ease1,
                          const GtkCssValue *ease2)
{
  if (ease1->type != ease2->type)
    return FALSE;
  
  switch (ease1->type)
    {
    case GTK_CSS_EASE_CUBIC_BEZIER:
      return ease1->u.cubic.x1 == ease2->u.cubic.x1 &&
             ease1->u.cubic.y1 == ease2->u.cubic.y1 &&
             ease1->u.cubic.x2 == ease2->u.cubic.x2 &&
             ease1->u.cubic.y2 == ease2->u.cubic.y2;
    case GTK_CSS_EASE_STEPS:
      return ease1->u.steps.steps == ease2->u.steps.steps &&
             ease1->u.steps.start == ease2->u.steps.start;
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static GtkCssValue *
gtk_css_value_ease_transition (GtkCssValue *start,
                               GtkCssValue *end,
                               guint        property_id,
                               double       progress)
{
  return NULL;
}

static void
gtk_css_value_ease_print (const GtkCssValue *ease,
                          GString           *string)
{
  switch (ease->type)
    {
    case GTK_CSS_EASE_CUBIC_BEZIER:
      if (ease->u.cubic.x1 == 0.25 && ease->u.cubic.y1 == 0.1 &&
          ease->u.cubic.x2 == 0.25 && ease->u.cubic.y2 == 1.0)
        g_string_append (string, "ease");
      else if (ease->u.cubic.x1 == 0.0 && ease->u.cubic.y1 == 0.0 &&
               ease->u.cubic.x2 == 1.0 && ease->u.cubic.y2 == 1.0)
        g_string_append (string, "linear");
      else if (ease->u.cubic.x1 == 0.42 && ease->u.cubic.y1 == 0.0 &&
               ease->u.cubic.x2 == 1.0  && ease->u.cubic.y2 == 1.0)
        g_string_append (string, "ease-in");
      else if (ease->u.cubic.x1 == 0.0  && ease->u.cubic.y1 == 0.0 &&
               ease->u.cubic.x2 == 0.58 && ease->u.cubic.y2 == 1.0)
        g_string_append (string, "ease-out");
      else if (ease->u.cubic.x1 == 0.42 && ease->u.cubic.y1 == 0.0 &&
               ease->u.cubic.x2 == 0.58 && ease->u.cubic.y2 == 1.0)
        g_string_append (string, "ease-in-out");
      else
        g_string_append_printf (string, "cubic-bezier(%g,%g,%g,%g)", 
                                ease->u.cubic.x1, ease->u.cubic.y1,
                                ease->u.cubic.x2, ease->u.cubic.y2);
      break;
    case GTK_CSS_EASE_STEPS:
      if (ease->u.steps.steps == 1)
        {
          g_string_append (string, ease->u.steps.start ? "step-start" : "step-end");
        }
      else
        {
          g_string_append_printf (string, "steps(%u%s)", ease->u.steps.steps, ease->u.steps.start ? ",start" : "");
        }
      break;
    default:
      g_assert_not_reached ();
      break;
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_EASE = {
  gtk_css_value_ease_free,
  gtk_css_value_ease_compute,
  gtk_css_value_ease_equal,
  gtk_css_value_ease_transition,
  gtk_css_value_ease_print
};

GtkCssValue *
_gtk_css_ease_value_new_cubic_bezier (double x1,
                                      double y1,
                                      double x2,
                                      double y2)
{
  GtkCssValue *value;

  g_return_val_if_fail (x1 >= 0.0, NULL);
  g_return_val_if_fail (x1 <= 1.0, NULL);
  g_return_val_if_fail (x2 >= 0.0, NULL);
  g_return_val_if_fail (x2 <= 1.0, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_EASE);
  
  value->type = GTK_CSS_EASE_CUBIC_BEZIER;
  value->u.cubic.x1 = x1;
  value->u.cubic.y1 = y1;
  value->u.cubic.x2 = x2;
  value->u.cubic.y2 = y2;

  return value;
}

static GtkCssValue *
_gtk_css_ease_value_new_steps (guint n_steps,
                               gboolean start)
{
  GtkCssValue *value;

  g_return_val_if_fail (n_steps > 0, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_EASE);
  
  value->type = GTK_CSS_EASE_STEPS;
  value->u.steps.steps = n_steps;
  value->u.steps.start = start;

  return value;
}

static const struct {
  const char *name;
  guint is_bezier :1;
  guint needs_custom :1;
  double values[4];
} parser_values[] = {
  { "linear",       TRUE,  FALSE, { 0.0,  0.0, 1.0,  1.0 } },
  { "ease-in-out",  TRUE,  FALSE, { 0.42, 0.0, 0.58, 1.0 } },
  { "ease-in",      TRUE,  FALSE, { 0.42, 0.0, 1.0,  1.0 } },
  { "ease-out",     TRUE,  FALSE, { 0.0,  0.0, 0.58, 1.0 } },
  { "ease",         TRUE,  FALSE, { 0.25, 0.1, 0.25, 1.0 } },
  { "step-start",   FALSE, FALSE, { 1.0,  1.0, 0.0,  0.0 } },
  { "step-end",     FALSE, FALSE, { 1.0,  0.0, 0.0,  0.0 } },
  { "steps",        FALSE, TRUE,  { 0.0,  0.0, 0.0,  0.0 } },
  { "cubic-bezier", TRUE,  TRUE,  { 0.0,  0.0, 0.0,  0.0 } }
};

gboolean
_gtk_css_ease_value_can_parse (GtkCssParser *parser)
{
  guint i;

  for (i = 0; i < G_N_ELEMENTS (parser_values); i++)
    {
      if (_gtk_css_parser_has_prefix (parser, parser_values[i].name))
        return TRUE;
    }

  return FALSE;
}

static GtkCssValue *
gtk_css_ease_value_parse_cubic_bezier (GtkCssParser *parser)
{
  double values[4];
  guint i;

  for (i = 0; i < 4; i++)
    {
      if (!_gtk_css_parser_try (parser, i ? "," : "(", TRUE))
        {
          _gtk_css_parser_error (parser, "Expected '%s'", i ? "," : "(");
          return NULL;
        }
      if (!_gtk_css_parser_try_double (parser, &values[i]))
        {
          _gtk_css_parser_error (parser, "Expected a number");
          return NULL;
        }
      if ((i == 0 || i == 2) &&
          (values[i] < 0 || values[i] > 1.0))
        {
          _gtk_css_parser_error (parser, "value %g out of range. Must be from 0.0 to 1.0", values[i]);
          return NULL;
        }
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_parser_error (parser, "Missing closing ')' for cubic-bezier");
      return NULL;
    }

  return _gtk_css_ease_value_new_cubic_bezier (values[0], values[1], values[2], values[3]);
}

static GtkCssValue *
gtk_css_ease_value_parse_steps (GtkCssParser *parser)
{
  guint n_steps;
  gboolean start;

  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected '('");
      return NULL;
    }

  if (!_gtk_css_parser_try_uint (parser, &n_steps))
    {
      _gtk_css_parser_error (parser, "Expected number of steps");
      return NULL;
    }

  if (_gtk_css_parser_try (parser, ",", TRUE))
    {
      if (_gtk_css_parser_try (parser, "start", TRUE))
        start = TRUE;
      else if (_gtk_css_parser_try (parser, "end", TRUE))
        start = FALSE;
      else
        {
          _gtk_css_parser_error (parser, "Only allowed values are 'start' and 'end'");
          return NULL;
        }
    }
  else
    start = FALSE;

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_parser_error (parser, "Missing closing ')' for steps");
      return NULL;
    }

  return _gtk_css_ease_value_new_steps (n_steps, start);
}

GtkCssValue *
_gtk_css_ease_value_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (parser_values); i++)
    {
      if (_gtk_css_parser_try (parser, parser_values[i].name, FALSE))
        {
          if (parser_values[i].needs_custom)
            {
              if (parser_values[i].is_bezier)
                return gtk_css_ease_value_parse_cubic_bezier (parser);
              else
                return gtk_css_ease_value_parse_steps (parser);
            }

          _gtk_css_parser_skip_whitespace (parser);

          if (parser_values[i].is_bezier)
            return _gtk_css_ease_value_new_cubic_bezier (parser_values[i].values[0],
                                                         parser_values[i].values[1],
                                                         parser_values[i].values[2],
                                                         parser_values[i].values[3]);
          else
            return _gtk_css_ease_value_new_steps (parser_values[i].values[0],
                                                  parser_values[i].values[1] != 0.0);
        }
    }

  _gtk_css_parser_error (parser, "Unknown value");
  return NULL;
}

double
_gtk_css_ease_value_transform (const GtkCssValue *ease,
                               double             progress)
{
  g_return_val_if_fail (ease->class == &GTK_CSS_VALUE_EASE, 1.0);

  if (progress <= 0)
    return 0;
  if (progress >= 1)
    return 1;

  switch (ease->type)
    {
    case GTK_CSS_EASE_CUBIC_BEZIER:
      {
        static const double epsilon = 0.00001;
        double tmin, t, tmax;

        tmin = 0.0;
        tmax = 1.0;
        t = progress;

        while (tmin < tmax)
          {
             double sample;
             sample = (((1.0 + 3 * ease->u.cubic.x1 - 3 * ease->u.cubic.x2) * t
                       +      -6 * ease->u.cubic.x1 + 3 * ease->u.cubic.x2) * t
                       +       3 * ease->u.cubic.x1                       ) * t;
             if (fabs(sample - progress) < epsilon)
               break;

             if (progress > sample)
               tmin = t;
             else
               tmax = t;
             t = (tmax + tmin) * .5;
          }

        return (((1.0 + 3 * ease->u.cubic.y1 - 3 * ease->u.cubic.y2) * t
                +      -6 * ease->u.cubic.y1 + 3 * ease->u.cubic.y2) * t
                +       3 * ease->u.cubic.y1                       ) * t;
      }
    case GTK_CSS_EASE_STEPS:
      progress *= ease->u.steps.steps;
      progress = floor (progress) + (ease->u.steps.start ? 0 : 1);
      return progress / ease->u.steps.steps;
    default:
      g_assert_not_reached ();
      return 1.0;
    }
}


