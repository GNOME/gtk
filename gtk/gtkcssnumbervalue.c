/* GTK - The GIMP Toolkit
 * Copyright (C) 2016 Benjamin Otte <otte@gnome.org>
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

#include "gtkcssdimensionvalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
};

GtkCssDimension
gtk_css_number_value_get_dimension (const GtkCssValue *value)
{
  GtkCssNumberValueClass *number_value_class = (GtkCssNumberValueClass *) value->class;

  return number_value_class->get_dimension (value);
}

gboolean
gtk_css_number_value_has_percent (const GtkCssValue *value)
{
  GtkCssNumberValueClass *number_value_class = (GtkCssNumberValueClass *) value->class;

  return number_value_class->has_percent (value);
}

GtkCssValue *
_gtk_css_number_value_new (double     value,
                           GtkCssUnit unit)
{
  return gtk_css_dimension_value_new (value, unit);
}

GtkCssValue *
_gtk_css_number_value_parse (GtkCssParser           *parser,
                             GtkCssNumberParseFlags  flags)
{
  return gtk_css_dimension_value_parse (parser, flags);
}

double
_gtk_css_number_value_get (const GtkCssValue *number,
                           double             one_hundred_percent)
{
  GtkCssNumberValueClass *number_value_class;

  g_return_val_if_fail (number != NULL, 0.0);

  number_value_class = (GtkCssNumberValueClass *) number->class;

  return number_value_class->get (number, one_hundred_percent);
}

