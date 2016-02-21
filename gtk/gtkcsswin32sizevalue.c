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

#include "gtkwin32drawprivate.h"
#include "gtkwin32themeprivate.h"

typedef enum {
  GTK_WIN32_SIZE,
  GTK_WIN32_PART_WIDTH,
  GTK_WIN32_PART_HEIGHT,
  GTK_WIN32_PART_BORDER_TOP,
  GTK_WIN32_PART_BORDER_RIGHT,
  GTK_WIN32_PART_BORDER_BOTTOM,
  GTK_WIN32_PART_BORDER_LEFT
} GtkWin32SizeType;

static const char *css_value_names[] = {
  "-gtk-win32-size(",
  "-gtk-win32-part-width(",
  "-gtk-win32-part-height(",
  "-gtk-win32-part-border-top(",
  "-gtk-win32-part-border-right(",
  "-gtk-win32-part-border-bottom(",
  "-gtk-win32-part-border-left("
};

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  double                 scale;         /* needed for calc() math */
  GtkWin32Theme         *theme;
  GtkWin32SizeType       type;
  union {
    struct {
      gint               id;
    } size;
    struct {
      gint               part;
      gint               state;
    } part;
  }                      val;
};

static GtkCssValue *    gtk_css_win32_size_value_new (double            scale,
                                                      GtkWin32Theme    *theme,
                                                      GtkWin32SizeType  type);

static void
gtk_css_value_win32_size_free (GtkCssValue *value)
{
  gtk_win32_theme_unref (value->theme);
  g_slice_free (GtkCssValue, value);
}

static int
gtk_css_value_win32_compute_size (const GtkCssValue *value)
{
  GtkBorder border;
  int size;

  switch (value->type)
    {
    case GTK_WIN32_SIZE:
      size = gtk_win32_theme_get_size (value->theme, value->val.size.id);
      break;

    case GTK_WIN32_PART_WIDTH:
      gtk_win32_theme_get_part_size (value->theme, value->val.part.part, value->val.part.state, &size, NULL);
      break;

    case GTK_WIN32_PART_HEIGHT:
      gtk_win32_theme_get_part_size (value->theme, value->val.part.part, value->val.part.state, NULL, &size);
      break;

    case GTK_WIN32_PART_BORDER_TOP:
      gtk_win32_theme_get_part_border (value->theme, value->val.part.part, value->val.part.state, &border);
      size = border.top;
      break;

    case GTK_WIN32_PART_BORDER_RIGHT:
      gtk_win32_theme_get_part_border (value->theme, value->val.part.part, value->val.part.state, &border);
      size = border.right;
      break;

    case GTK_WIN32_PART_BORDER_BOTTOM:
      gtk_win32_theme_get_part_border (value->theme, value->val.part.part, value->val.part.state, &border);
      size = border.bottom;
      break;

    case GTK_WIN32_PART_BORDER_LEFT:
      gtk_win32_theme_get_part_border (value->theme, value->val.part.part, value->val.part.state, &border);
      size = border.left;
      break;

    default:
      g_assert_not_reached ();
      return 0;
    }

  return size;
}

static GtkCssValue *
gtk_css_value_win32_size_compute (GtkCssValue             *value,
                                  guint                    property_id,
                                  GtkStyleProviderPrivate *provider,
                                  GtkCssStyle             *style,
                                  GtkCssStyle             *parent_style)
{
  return _gtk_css_number_value_new (value->scale * gtk_css_value_win32_compute_size (value), GTK_CSS_PX);
}

static gboolean
gtk_css_value_win32_size_equal (const GtkCssValue *value1,
                                const GtkCssValue *value2)
{
  if (value1->type != value2->type ||
      !gtk_win32_theme_equal (value1->theme, value2->theme) )
    return FALSE;

  switch (value1->type)
    {
    case GTK_WIN32_SIZE:
      return value1->val.size.id == value2->val.size.id;

    case GTK_WIN32_PART_WIDTH:
    case GTK_WIN32_PART_HEIGHT:
      return value1->val.part.part == value2->val.part.part
          && value1->val.part.state == value2->val.part.state;

    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static void
gtk_css_value_win32_size_print (const GtkCssValue *value,
                                GString           *string)
{
  if (value->scale != 1.0)
    {
      g_string_append_printf (string, "%g * ", value->scale);
    }
  g_string_append (string, css_value_names[value->type]);
  gtk_win32_theme_print (value->theme, string);

  switch (value->type)
    {
    case GTK_WIN32_SIZE:
      {
        const char *name = gtk_win32_get_sys_metric_name_for_id (value->val.size.id);
        if (name)
          g_string_append (string, name);
        else
          g_string_append_printf (string, ", %d", value->val.size.id);
      }
      break;

    case GTK_WIN32_PART_WIDTH:
    case GTK_WIN32_PART_HEIGHT:
    case GTK_WIN32_PART_BORDER_TOP:
    case GTK_WIN32_PART_BORDER_RIGHT:
    case GTK_WIN32_PART_BORDER_BOTTOM:
    case GTK_WIN32_PART_BORDER_LEFT:
      g_string_append_printf (string, ", %d, %d", value->val.part.part, value->val.part.state);
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  g_string_append (string, ")");
}

static double
gtk_css_value_win32_size_get (const GtkCssValue *value,
                              double             one_hundred_percent)
{
  return value->scale * gtk_css_value_win32_compute_size (value);
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
  GtkCssValue *result;

  result = gtk_css_win32_size_value_new (value->scale * factor, value->theme, value->type);
  result->val = value->val;

  return result;
}

static GtkCssValue *
gtk_css_value_win32_size_try_add (const GtkCssValue *value1,
                                  const GtkCssValue *value2)
{
  GtkCssValue *result;

  if (!gtk_css_value_win32_size_equal (value1, value2))
    return NULL;

  result = gtk_css_win32_size_value_new (value1->scale + value2->scale, value1->theme, value1->type);
  result->val = value1->val;

  return result;
}

static gint
gtk_css_value_win32_size_get_calc_term_order (const GtkCssValue *value)
{
  return 2000 + 100 * value->type;
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
gtk_css_win32_size_value_new (double            scale,
                              GtkWin32Theme    *theme,
                              GtkWin32SizeType  type)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_WIN32_SIZE.value_class);
  result->scale = scale;
  result->theme = gtk_win32_theme_ref (theme);
  result->type = type;

  return result;
}

static GtkCssValue *
gtk_css_win32_size_value_parse_size (GtkCssValue *value,
                                     GtkCssParser *parser)
{
  char *name;

  name = _gtk_css_parser_try_ident (parser, TRUE);
  if (name)
    {
      value->val.size.id = gtk_win32_get_sys_metric_id_for_name (name);
      if (value->val.size.id == -1)
        {
          _gtk_css_parser_error (parser, "'%s' is not a name for a win32 metric.", name);
          _gtk_css_value_unref (value);
          g_free (name);
          return NULL;
        }
      g_free (name);
    }
  else if (!_gtk_css_parser_try_int (parser, &value->val.size.id))
    {
      _gtk_css_value_unref (value);
      _gtk_css_parser_error (parser, "Expected an integer ID");
      return NULL;
    }

  return value;
}

static GtkCssValue *
gtk_css_win32_size_value_parse_part_size (GtkCssValue *value,
                                          GtkCssParser *parser)
{
  if (!_gtk_css_parser_try_int (parser, &value->val.part.part))
    {
      _gtk_css_value_unref (value);
      _gtk_css_parser_error (parser, "Expected an integer part ID");
      return NULL;
    }

  if (! _gtk_css_parser_try (parser, ",", TRUE))
    {
      _gtk_css_value_unref (value);
      _gtk_css_parser_error (parser, "Expected ','");
      return NULL;
    }

  if (!_gtk_css_parser_try_int (parser, &value->val.part.state))
    {
      _gtk_css_value_unref (value);
      _gtk_css_parser_error (parser, "Expected an integer state ID");
      return NULL;
    }

  return value;
}

GtkCssValue *
gtk_css_win32_size_value_parse (GtkCssParser           *parser,
                                GtkCssNumberParseFlags  flags)
{
  GtkWin32Theme *theme;
  GtkCssValue *result;
  guint type;

  for (type = 0; type < G_N_ELEMENTS(css_value_names); type++)
    {
      if (_gtk_css_parser_try (parser, css_value_names[type], TRUE))
        break;
    }

  if (type >= G_N_ELEMENTS(css_value_names))
    {
      _gtk_css_parser_error (parser, "Not a win32 size value");
      return NULL;
    }

  theme = gtk_win32_theme_parse (parser);
  if (theme == NULL)
    return NULL;

  result = gtk_css_win32_size_value_new (1.0, theme, type);
  gtk_win32_theme_unref (theme);

  if (! _gtk_css_parser_try (parser, ",", TRUE))
    {
      _gtk_css_value_unref (result);
      _gtk_css_parser_error (parser, "Expected ','");
      return NULL;
    }

  switch (result->type)
    {
    case GTK_WIN32_SIZE:
      result = gtk_css_win32_size_value_parse_size (result, parser);
      break;

    case GTK_WIN32_PART_WIDTH:
    case GTK_WIN32_PART_HEIGHT:
    case GTK_WIN32_PART_BORDER_TOP:
    case GTK_WIN32_PART_BORDER_RIGHT:
    case GTK_WIN32_PART_BORDER_BOTTOM:
    case GTK_WIN32_PART_BORDER_LEFT:
      result = gtk_css_win32_size_value_parse_part_size (result, parser);
      break;

    default:
      g_assert_not_reached ();
      _gtk_css_value_unref (result);
      result = NULL;
      break;
    }

  if (result == NULL)
    return NULL;

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_value_unref (result);
      _gtk_css_parser_error (parser, "Expected ')'");
      return NULL;
    }

  return result;
}
