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

#include "gtkcsswin32sizevalueprivate.h"

#include "gtkwin32themeprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  double         scale;         /* needed for calc() math */
  GtkWin32Theme *theme;
  gint           id;
};

static GtkCssValue *    gtk_css_win32_size_value_new (double         scale,
                                                      GtkWin32Theme *theme,
                                                      int            id);

static void
gtk_css_value_win32_size_free (GtkCssValue *value)
{
  gtk_win32_theme_unref (value->theme);
  g_slice_free (GtkCssValue, value);
}

static GtkCssValue *
gtk_css_value_win32_size_compute (GtkCssValue             *value,
                                  guint                    property_id,
                                  GtkStyleProviderPrivate *provider,
                                  GtkCssStyle             *style,
                                  GtkCssStyle             *parent_style)
{
  return _gtk_css_number_value_new (value->scale * gtk_win32_theme_get_size (value->theme, value->id), GTK_CSS_PX);
}

static gboolean
gtk_css_value_win32_size_equal (const GtkCssValue *value1,
                                const GtkCssValue *value2)
{
  return gtk_win32_theme_equal (value1->theme, value2->theme) &&
         value1->id == value2->id;
}

static void
gtk_css_value_win32_size_print (const GtkCssValue *value,
                                GString           *string)
{
  if (value->scale != 1.0)
    {
      g_string_append_printf (string, "%g * ", value->scale);
    }
  g_string_append (string, "-gtk-win32-size(");
  gtk_win32_theme_print (value->theme, string);
  g_string_append_printf (string, ", %d)", value->id);
}

static double
gtk_css_value_win32_size_get (const GtkCssValue *value,
                              double             one_hundred_percent)
{
  return value->scale * gtk_win32_theme_get_size (value->theme, value->id);
}

static GtkCssDimension
gtk_css_value_win32_size_get_dimension (const GtkCssValue *value)
{
  return GTK_CSS_DIMENSION_LENGTH;
}

static gboolean
gtk_css_value_win32_size_has_percent (const GtkCssValue *value)
{
  return FALSE;
}

static GtkCssValue *
gtk_css_value_win32_size_multiply (const GtkCssValue *value,
                                   double             factor)
{
  return gtk_css_win32_size_value_new (value->scale * factor, value->theme, value->id);
}

static GtkCssValue *
gtk_css_value_win32_size_try_add (const GtkCssValue *value1,
                                  const GtkCssValue *value2)
{
  if (!gtk_css_value_win32_size_equal (value1, value2))
    return NULL;

  return gtk_css_win32_size_value_new (value1->scale + value2->scale, value1->theme, value1->id);
}

static gint
gtk_css_value_win32_size_get_calc_term_order (const GtkCssValue *value)
{
  return 2000;
}

static const GtkCssNumberValueClass GTK_CSS_VALUE_WIN32_SIZE = {
  {
    gtk_css_value_win32_size_free,
    gtk_css_value_win32_size_compute,
    gtk_css_value_win32_size_equal,
    gtk_css_number_value_transition,
    gtk_css_value_win32_size_print
  },
  gtk_css_value_win32_size_get,
  gtk_css_value_win32_size_get_dimension,
  gtk_css_value_win32_size_has_percent,
  gtk_css_value_win32_size_multiply,
  gtk_css_value_win32_size_try_add,
  gtk_css_value_win32_size_get_calc_term_order
};

static GtkCssValue *
gtk_css_win32_size_value_new (double         scale,
                              GtkWin32Theme *theme,
                              int            id)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_WIN32_SIZE.value_class);
  result->scale = scale;
  result->theme = gtk_win32_theme_ref (theme);
  result->id = id;

  return result;
}

GtkCssValue *
gtk_css_win32_size_value_parse (GtkCssParser           *parser,
                                GtkCssNumberParseFlags  flags)
{
  GtkWin32Theme *theme;
  char *theme_class;
  GtkCssValue *result;
  int id;

  if (!_gtk_css_parser_try (parser, "-gtk-win32-size(", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected '-gtk-win32-size('");
      return NULL;
    }

  theme_class = _gtk_css_parser_try_name (parser, TRUE);
  if (theme_class == NULL)
    {
      _gtk_css_parser_error (parser, "Expected name as first argument to  '-gtk-win32-size'");
      return NULL;
    }

  if (! _gtk_css_parser_try (parser, ",", TRUE))
    {
      g_free (theme_class);
      _gtk_css_parser_error (parser, "Expected ','");
      return NULL;
    }

  if (!_gtk_css_parser_try_int (parser, &id))
    {
      g_free (theme_class);
      _gtk_css_parser_error (parser, "Expected an integer ID");
      return 0;
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      g_free (theme_class);
      _gtk_css_parser_error (parser, "Expected ')'");
      return NULL;
    }

  theme = gtk_win32_theme_lookup (theme_class);
  g_free (theme_class);

  result = gtk_css_win32_size_value_new (1.0, theme, id);
  gtk_win32_theme_unref (theme);

  return result;
}
