/* GTK - The GIMP Toolkit
 * Copyright © 2016 Benjamin Otte <otte@gnome.org>
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

#include "gtkcsscalcvalueprivate.h"

#include "gtkcssenumvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkstylepropertyprivate.h"

#include "fallback-c89.c"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssValue *term;
};

static void
gtk_css_value_calc_free (GtkCssValue *value)
{
  _gtk_css_value_unref (value->term);
  g_slice_free (GtkCssValue, value);
}

static GtkCssValue *gtk_css_calc_value_new (GtkCssValue *term);

static GtkCssValue *
gtk_css_value_calc_compute (GtkCssValue             *calc,
                            guint                    property_id,
                            GtkStyleProviderPrivate *provider,
                            GtkCssStyle             *style,
                            GtkCssStyle             *parent_style)
{
  GtkCssValue *computed_term;

  computed_term = _gtk_css_value_compute (calc->term, property_id, provider, style, parent_style);
  if (computed_term == calc->term)
    return _gtk_css_value_ref (calc);

  return gtk_css_calc_value_new (computed_term);
}


static gboolean
gtk_css_value_calc_equal (const GtkCssValue *calc1,
                          const GtkCssValue *calc2)
{
  return _gtk_css_value_equal (calc1->term, calc2->term);
}

static GtkCssValue *
gtk_css_value_calc_transition (GtkCssValue *start,
                               GtkCssValue *end,
                               guint        property_id,
                               double       progress)
{
  return NULL;
}

static void
gtk_css_value_calc_print (const GtkCssValue *calc,
                          GString           *string)
{
  g_string_append (string, "calc(");
  _gtk_css_value_print (calc->term, string);
  g_string_append (string, ")");
}

double
gtk_css_value_calc_get (const GtkCssValue *value,
                        double             one_hundred_percent)
{
  return _gtk_css_number_value_get (value->term, one_hundred_percent);
}

GtkCssDimension
gtk_css_value_calc_get_dimension (const GtkCssValue *value)
{
  return gtk_css_number_value_get_dimension (value->term);
}

gboolean
gtk_css_value_calc_has_percent (const GtkCssValue *value)
{
  return gtk_css_number_value_has_percent (value->term);
}

static const GtkCssNumberValueClass GTK_CSS_VALUE_CALC = {
  {
    gtk_css_value_calc_free,
    gtk_css_value_calc_compute,
    gtk_css_value_calc_equal,
    gtk_css_value_calc_transition,
    gtk_css_value_calc_print
  },
  gtk_css_value_calc_get,
  gtk_css_value_calc_get_dimension,
  gtk_css_value_calc_has_percent,
};

static GtkCssValue *
gtk_css_calc_value_new (GtkCssValue *term)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_CALC.value_class);
  result->term = term;

  return result;
}

GtkCssValue *
gtk_css_calc_value_parse (GtkCssParser           *parser,
                          GtkCssNumberParseFlags  flags)
{
  GtkCssValue *term;

  if (!_gtk_css_parser_try (parser, "calc(", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected 'calc('");
      return NULL;
    }

  term = _gtk_css_number_value_parse (parser, flags);
  if (term == NULL)
    return NULL;

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_value_unref (term);
      _gtk_css_parser_error (parser, "Expected ')' for calc() statement");
      return NULL;
    }

  return gtk_css_calc_value_new (term);
}

