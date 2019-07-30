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
#include "gtktimingfunctionprivate.h"

#include <math.h>

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkTimingFunction *tm;
};

static void
gtk_css_value_ease_free (GtkCssValue *value)
{
  gtk_timing_function_unref (value->tm);
  g_slice_free (GtkCssValue, value);
}

static GtkCssValue *
gtk_css_value_ease_compute (GtkCssValue      *value,
                            guint             property_id,
                            GtkStyleProvider *provider,
                            GtkCssStyle      *style,
                            GtkCssStyle      *parent_style)
{
  return _gtk_css_value_ref (value);
}

static gboolean
gtk_css_value_ease_equal (const GtkCssValue *ease1,
                          const GtkCssValue *ease2)
{
  return gtk_timing_function_equal (ease1->tm, ease2->tm);
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
  gtk_timing_function_print (ease->tm, string);
}

static const GtkCssValueClass GTK_CSS_VALUE_EASE = {
  gtk_css_value_ease_free,
  gtk_css_value_ease_compute,
  gtk_css_value_ease_equal,
  gtk_css_value_ease_transition,
  NULL,
  NULL,
  gtk_css_value_ease_print
};

GtkCssValue *
_gtk_css_ease_value_new_ease (void)
{
  GtkCssValue *value;

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_EASE);
  value->tm = gtk_ease ();

  return value;
}

static const struct {
  const char *name;
  guint is_bezier :1;
  guint is_function :1;
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
      if (parser_values[i].is_function)
        {
          if (gtk_css_parser_has_function (parser, parser_values[i].name))
            return TRUE;
        }
      else
        {
          if (gtk_css_parser_has_ident (parser, parser_values[i].name))
            return TRUE;
        }
    }

  return FALSE;
}

GtkCssValue *
_gtk_css_ease_value_parse (GtkCssParser *parser)
{
  GtkCssValue *value;
  GtkTimingFunction *tm;

  if (!gtk_timing_function_parser_parse (parser, &tm))
    return NULL;

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_EASE);
  value->tm = tm;

  return value;
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

  return gtk_timing_function_transform_time (ease->tm, progress, 1.0);
}
