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

#include "gtkcssenumvalueprivate.h"

#include "gtkstylepropertyprivate.h"

/* repeated API */

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  int value;
  const char *name;
};

static void
gtk_css_value_enum_free (GtkCssValue *value)
{
  g_slice_free (GtkCssValue, value);
}

static gboolean
gtk_css_value_enum_equal (const GtkCssValue *enum1,
                          const GtkCssValue *enum2)
{
  return enum1 == enum2;
}

static void
gtk_css_value_enum_print (const GtkCssValue *value,
                          GString           *string)
{
  g_string_append (string, value->name);
}

/* GtkBorderStyle */

static const GtkCssValueClass GTK_CSS_VALUE_BORDER_STYLE = {
  gtk_css_value_enum_free,
  gtk_css_value_enum_equal,
  gtk_css_value_enum_print
};

static GtkCssValue border_style_values[] = {
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, GTK_BORDER_STYLE_NONE, "none" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, GTK_BORDER_STYLE_SOLID, "solid" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, GTK_BORDER_STYLE_INSET, "inset" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, GTK_BORDER_STYLE_OUTSET, "outset" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, GTK_BORDER_STYLE_HIDDEN, "hidden" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, GTK_BORDER_STYLE_DOTTED, "dotted" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, GTK_BORDER_STYLE_DASHED, "dashed" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, GTK_BORDER_STYLE_DOUBLE, "double" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, GTK_BORDER_STYLE_GROOVE, "groove" },
  { &GTK_CSS_VALUE_BORDER_STYLE, 1, GTK_BORDER_STYLE_RIDGE, "ridge" }
};

GtkCssValue *
_gtk_css_border_style_value_new (GtkBorderStyle border_style)
{
  g_return_val_if_fail (border_style < G_N_ELEMENTS (border_style_values), NULL);

  return _gtk_css_value_ref (&border_style_values[border_style]);
}

GtkCssValue *
_gtk_css_border_style_value_try_parse (GtkCssParser *parser)
{
  guint i;

  g_return_val_if_fail (parser != NULL, NULL);

  for (i = 0; i < G_N_ELEMENTS (border_style_values); i++)
    {
      if (_gtk_css_parser_try (parser, border_style_values[i].name, TRUE))
        return _gtk_css_value_ref (&border_style_values[i]);
    }

  return NULL;
}

GtkBorderStyle
_gtk_css_border_style_value_get (const GtkCssValue *value)
{
  g_return_val_if_fail (value->class == &GTK_CSS_VALUE_BORDER_STYLE, GTK_BORDER_STYLE_NONE);

  return value->value;
}

