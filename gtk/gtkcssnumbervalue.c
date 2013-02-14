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

#include "gtkcssnumbervalueprivate.h"

#include "gtkcssenumvalueprivate.h"
#include "gtkcssinitialvalueprivate.h"
#include "gtkstylepropertyprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssUnit unit;
  double value;
};

static void
gtk_css_value_number_free (GtkCssValue *value)
{
  g_slice_free (GtkCssValue, value);
}

static double
get_base_font_size (guint                    property_id,
                    GtkStyleProviderPrivate *provider,
                    GtkCssComputedValues    *values,
                    GtkCssComputedValues    *parent_values,
                    GtkCssDependencies      *dependencies)
{
  if (property_id == GTK_CSS_PROPERTY_FONT_SIZE)
    {
      *dependencies = GTK_CSS_DEPENDS_ON_PARENT;
      if (parent_values)
        return _gtk_css_number_value_get (_gtk_css_computed_values_get_value (parent_values, GTK_CSS_PROPERTY_FONT_SIZE), 100);
      else
        return _gtk_css_font_size_get_default (provider);
    }

  *dependencies = GTK_CSS_DEPENDS_ON_FONT_SIZE;
  return _gtk_css_number_value_get (_gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_FONT_SIZE), 100);
}
                    
static GtkCssValue *
gtk_css_value_number_compute (GtkCssValue             *number,
                              guint                    property_id,
                              GtkStyleProviderPrivate *provider,
                              GtkCssComputedValues    *values,
                              GtkCssComputedValues    *parent_values,
                              GtkCssDependencies      *dependencies)
{
  switch (number->unit)
    {
    default:
      g_assert_not_reached();
      /* fall through */
    case GTK_CSS_PERCENT:
      /* percentages for font sizes are computed, other percentages aren't */
      if (property_id == GTK_CSS_PROPERTY_FONT_SIZE)
        return _gtk_css_number_value_new (number->value / 100.0 * 
                                          get_base_font_size (property_id, provider, values, parent_values, dependencies),
                                          GTK_CSS_PX);
    case GTK_CSS_NUMBER:
    case GTK_CSS_PX:
    case GTK_CSS_DEG:
    case GTK_CSS_S:
      return _gtk_css_value_ref (number);
    case GTK_CSS_PT:
      return _gtk_css_number_value_new (number->value * 96.0 / 72.0,
                                        GTK_CSS_PX);
    case GTK_CSS_PC:
      return _gtk_css_number_value_new (number->value * 96.0 / 72.0 * 12.0,
                                        GTK_CSS_PX);
      break;
    case GTK_CSS_IN:
      return _gtk_css_number_value_new (number->value * 96.0,
                                        GTK_CSS_PX);
      break;
    case GTK_CSS_CM:
      return _gtk_css_number_value_new (number->value * 96.0 * 0.39370078740157477,
                                        GTK_CSS_PX);
      break;
    case GTK_CSS_MM:
      return _gtk_css_number_value_new (number->value * 96.0 * 0.039370078740157477,
                                        GTK_CSS_PX);
      break;
    case GTK_CSS_EM:
      return _gtk_css_number_value_new (number->value *
                                        get_base_font_size (property_id, provider, values, parent_values, dependencies),
                                        GTK_CSS_PX);
      break;
    case GTK_CSS_EX:
      /* for now we pretend ex is half of em */
      return _gtk_css_number_value_new (number->value * 0.5 * 
                                        get_base_font_size (property_id, provider, values, parent_values, dependencies),
                                        GTK_CSS_PX);
    case GTK_CSS_RAD:
      return _gtk_css_number_value_new (number->value * 360.0 / (2 * G_PI),
                                        GTK_CSS_DEG);
    case GTK_CSS_GRAD:
      return _gtk_css_number_value_new (number->value * 360.0 / 400.0,
                                        GTK_CSS_DEG);
    case GTK_CSS_TURN:
      return _gtk_css_number_value_new (number->value * 360.0,
                                        GTK_CSS_DEG);
    case GTK_CSS_MS:
      return _gtk_css_number_value_new (number->value / 1000.0,
                                        GTK_CSS_S);
    }
}

static gboolean
gtk_css_value_number_equal (const GtkCssValue *number1,
                            const GtkCssValue *number2)
{
  return number1->unit == number2->unit &&
         number1->value == number2->value;
}

static GtkCssValue *
gtk_css_value_number_transition (GtkCssValue *start,
                                 GtkCssValue *end,
                                 guint        property_id,
                                 double       progress)
{
  /* FIXME: This needs to be supported at least for percentages,
   * but for that we kinda need to support calc(5px + 50%) */
  if (start->unit != end->unit)
    return NULL;

  return _gtk_css_number_value_new (start->value + (end->value - start->value) * progress,
                                    start->unit);
}

static void
gtk_css_value_number_print (const GtkCssValue *number,
                            GString           *string)
{
  char buf[G_ASCII_DTOSTR_BUF_SIZE];

  const char *names[] = {
    /* [GTK_CSS_NUMBER] = */ "",
    /* [GTK_CSS_PERCENT] = */ "%",
    /* [GTK_CSS_PX] = */ "px",
    /* [GTK_CSS_PT] = */ "pt",
    /* [GTK_CSS_EM] = */ "em",
    /* [GTK_CSS_EX] = */ "ex",
    /* [GTK_CSS_PC] = */ "pc",
    /* [GTK_CSS_IN] = */ "in",
    /* [GTK_CSS_CM] = */ "cm",
    /* [GTK_CSS_MM] = */ "mm",
    /* [GTK_CSS_RAD] = */ "rad",
    /* [GTK_CSS_DEG] = */ "deg",
    /* [GTK_CSS_GRAD] = */ "grad",
    /* [GTK_CSS_TURN] = */ "turn",
    /* [GTK_CSS_S] = */ "s",
    /* [GTK_CSS_MS] = */ "ms",
  };

  g_ascii_dtostr (buf, sizeof (buf), number->value);
  g_string_append (string, buf);
  if (number->value != 0.0)
    g_string_append (string, names[number->unit]);
}

static const GtkCssValueClass GTK_CSS_VALUE_NUMBER = {
  gtk_css_value_number_free,
  gtk_css_value_number_compute,
  gtk_css_value_number_equal,
  gtk_css_value_number_transition,
  gtk_css_value_number_print
};

GtkCssValue *
_gtk_css_number_value_new (double     value,
                           GtkCssUnit unit)
{
  static GtkCssValue number_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, GTK_CSS_NUMBER, 0 },
    { &GTK_CSS_VALUE_NUMBER, 1, GTK_CSS_NUMBER, 1 },
  };
  static GtkCssValue px_singletons[] = {
    { &GTK_CSS_VALUE_NUMBER, 1, GTK_CSS_PX, 0 },
    { &GTK_CSS_VALUE_NUMBER, 1, GTK_CSS_PX, 1 },
    { &GTK_CSS_VALUE_NUMBER, 1, GTK_CSS_PX, 2 },
    { &GTK_CSS_VALUE_NUMBER, 1, GTK_CSS_PX, 3 },
    { &GTK_CSS_VALUE_NUMBER, 1, GTK_CSS_PX, 4 },
  };
  GtkCssValue *result;

  if (unit == GTK_CSS_NUMBER && (value == 0 || value == 1))
    return _gtk_css_value_ref (&number_singletons[(int) value]);

  if (unit == GTK_CSS_PX &&
      (value == 0 ||
       value == 1 ||
       value == 2 ||
       value == 3 ||
       value == 4))
    {
      return _gtk_css_value_ref (&px_singletons[(int) value]);
    }

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_NUMBER);
  result->unit = unit;
  result->value = value;

  return result;
}

GtkCssUnit
_gtk_css_number_value_get_unit (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_NUMBER, GTK_CSS_NUMBER);

  return value->unit;
}

double
_gtk_css_number_value_get (const GtkCssValue *number,
                           double             one_hundred_percent)
{
  g_return_val_if_fail (number != NULL, 0.0);
  g_return_val_if_fail (number->class == &GTK_CSS_VALUE_NUMBER, 0.0);

  if (number->unit == GTK_CSS_PERCENT)
    return number->value * one_hundred_percent / 100;
  else
    return number->value;
}

