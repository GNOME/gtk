/* GTK - The GIMP Toolkit
 * Copyright (C) 2010 Carlos Garnacho <carlosg@gnome.org>
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

#include "gtkcsscolorvalueprivate.h"

#include "gtkcssrgbavalueprivate.h"
#include "gtkcssstylepropertyprivate.h"
#include "gtkhslaprivate.h"
#include "gtkstylepropertyprivate.h"
#include "gtkstyleproperties.h"
#include "gtkwin32themeprivate.h"

typedef enum {
  COLOR_TYPE_LITERAL,
  COLOR_TYPE_NAME,
  COLOR_TYPE_SHADE,
  COLOR_TYPE_ALPHA,
  COLOR_TYPE_MIX,
  COLOR_TYPE_WIN32,
  COLOR_TYPE_CURRENT_COLOR
} ColorType;

struct _GtkCssValue
{
  GTK_CSS_VALUE_BASE
  ColorType type;
  GtkCssValue *last_value;

  union
  {
    gchar *name;

    struct
    {
      GtkCssValue *color;
      gdouble factor;
    } shade, alpha;

    struct
    {
      GtkCssValue *color1;
      GtkCssValue *color2;
      gdouble factor;
    } mix;

    struct
    {
      gchar *theme_class;
      gint id;
    } win32;
  } sym_col;
};

static void
gtk_css_value_color_free (GtkCssValue *color)
{
  if (color->last_value)
    _gtk_css_value_unref (color->last_value);

  switch (color->type)
    {
    case COLOR_TYPE_NAME:
      g_free (color->sym_col.name);
      break;
    case COLOR_TYPE_SHADE:
      _gtk_css_value_unref (color->sym_col.shade.color);
      break;
    case COLOR_TYPE_ALPHA:
      _gtk_css_value_unref (color->sym_col.alpha.color);
      break;
    case COLOR_TYPE_MIX:
      _gtk_css_value_unref (color->sym_col.mix.color1);
      _gtk_css_value_unref (color->sym_col.mix.color2);
      break;
    case COLOR_TYPE_WIN32:
      g_free (color->sym_col.win32.theme_class);
      break;
    default:
      break;
    }

  g_slice_free (GtkCssValue, color);
}

static GtkCssValue *
gtk_css_value_color_get_fallback (guint                    property_id,
                                  GtkStyleProviderPrivate *provider,
				  int                      scale,
                                  GtkCssComputedValues    *values,
                                  GtkCssComputedValues    *parent_values)
{
  static const GdkRGBA transparent = { 0, 0, 0, 0 };

  switch (property_id)
    {
      case GTK_CSS_PROPERTY_BACKGROUND_IMAGE:
      case GTK_CSS_PROPERTY_BORDER_IMAGE_SOURCE:
      case GTK_CSS_PROPERTY_TEXT_SHADOW:
      case GTK_CSS_PROPERTY_ICON_SHADOW:
      case GTK_CSS_PROPERTY_BOX_SHADOW:
        return _gtk_css_rgba_value_new_from_rgba (&transparent);
      case GTK_CSS_PROPERTY_COLOR:
      case GTK_CSS_PROPERTY_BACKGROUND_COLOR:
      case GTK_CSS_PROPERTY_BORDER_TOP_COLOR:
      case GTK_CSS_PROPERTY_BORDER_RIGHT_COLOR:
      case GTK_CSS_PROPERTY_BORDER_BOTTOM_COLOR:
      case GTK_CSS_PROPERTY_BORDER_LEFT_COLOR:
      case GTK_CSS_PROPERTY_OUTLINE_COLOR:
        return _gtk_css_value_compute (_gtk_css_style_property_get_initial_value (_gtk_css_style_property_lookup_by_id (property_id)),
                                       property_id,
                                       provider,
				       scale,
                                       values,
                                       parent_values,
                                       NULL);
      default:
        if (property_id < GTK_CSS_PROPERTY_N_PROPERTIES)
          g_warning ("No fallback color defined for property '%s'", 
                     _gtk_style_property_get_name (GTK_STYLE_PROPERTY (_gtk_css_style_property_lookup_by_id (property_id))));
        return _gtk_css_rgba_value_new_from_rgba (&transparent);
    }
}

GtkCssValue *
_gtk_css_color_value_resolve (GtkCssValue             *color,
                              GtkStyleProviderPrivate *provider,
                              GtkCssValue             *current,
                              GtkCssDependencies       current_deps,
                              GtkCssDependencies      *dependencies,
                              GSList                  *cycle_list)
{
  GtkCssDependencies unused;
  GtkCssValue *value;

  g_return_val_if_fail (color != NULL, NULL);
  g_return_val_if_fail (provider == NULL || GTK_IS_STYLE_PROVIDER_PRIVATE (provider), NULL);

  if (dependencies == NULL)
    dependencies = &unused;
  *dependencies = 0;

  switch (color->type)
    {
    case COLOR_TYPE_LITERAL:
      return _gtk_css_value_ref (color->last_value);
    case COLOR_TYPE_NAME:
      {
	GtkCssValue *named;
        GSList cycle = { color, cycle_list };

        /* If color exists in cycle_list, we're currently resolving it.
         * So we've detected a cycle. */
        if (g_slist_find (cycle_list, color))
          return NULL;

        named = _gtk_style_provider_private_get_color (provider, color->sym_col.name);
	if (named == NULL)
	  return NULL;

        value = _gtk_css_color_value_resolve (named, provider, current, current_deps, dependencies, &cycle);
	if (value == NULL)
	  return NULL;
      }

      break;
    case COLOR_TYPE_SHADE:
      {
	GtkCssValue *val;
        GtkHSLA hsla;
	GdkRGBA shade;

	val = _gtk_css_color_value_resolve (color->sym_col.shade.color, provider, current, current_deps, dependencies, cycle_list);
	if (val == NULL)
	  return NULL;

        *dependencies = _gtk_css_dependencies_union (*dependencies, 0);
        
        _gtk_hsla_init_from_rgba (&hsla, _gtk_css_rgba_value_get_rgba (val));
        _gtk_hsla_shade (&hsla, &hsla, color->sym_col.shade.factor);

        _gdk_rgba_init_from_hsla (&shade, &hsla);

	_gtk_css_value_unref (val);

	value = _gtk_css_rgba_value_new_from_rgba (&shade);
      }

      break;
    case COLOR_TYPE_ALPHA:
      {
	GtkCssValue *val;
	GdkRGBA alpha;

	val = _gtk_css_color_value_resolve (color->sym_col.alpha.color, provider, current, current_deps, dependencies, cycle_list);
	if (val == NULL)
	  return NULL;

        *dependencies = _gtk_css_dependencies_union (*dependencies, 0);
	alpha = *_gtk_css_rgba_value_get_rgba (val);
	alpha.alpha = CLAMP (alpha.alpha * color->sym_col.alpha.factor, 0, 1);

	_gtk_css_value_unref (val);

	value = _gtk_css_rgba_value_new_from_rgba (&alpha);
      }
      break;

    case COLOR_TYPE_MIX:
      {
	GtkCssValue *val;
	GdkRGBA color1, color2, res;
        GtkCssDependencies dep1, dep2;

	val = _gtk_css_color_value_resolve (color->sym_col.mix.color1, provider, current, current_deps, &dep1, cycle_list);
	if (val == NULL)
	  return NULL;
	color1 = *_gtk_css_rgba_value_get_rgba (val);
	_gtk_css_value_unref (val);

	val = _gtk_css_color_value_resolve (color->sym_col.mix.color2, provider, current, current_deps, &dep2, cycle_list);
	if (val == NULL)
	  return NULL;
	color2 = *_gtk_css_rgba_value_get_rgba (val);
	_gtk_css_value_unref (val);

        *dependencies = _gtk_css_dependencies_union (dep1, dep2);
	res.red = CLAMP (color1.red + ((color2.red - color1.red) * color->sym_col.mix.factor), 0, 1);
	res.green = CLAMP (color1.green + ((color2.green - color1.green) * color->sym_col.mix.factor), 0, 1);
	res.blue = CLAMP (color1.blue + ((color2.blue - color1.blue) * color->sym_col.mix.factor), 0, 1);
	res.alpha = CLAMP (color1.alpha + ((color2.alpha - color1.alpha) * color->sym_col.mix.factor), 0, 1);

	value =_gtk_css_rgba_value_new_from_rgba (&res);
      }

      break;
    case COLOR_TYPE_WIN32:
      {
	GdkRGBA res;

	if (!_gtk_win32_theme_color_resolve (color->sym_col.win32.theme_class,
					     color->sym_col.win32.id,
					     &res))
	  return NULL;

	value = _gtk_css_rgba_value_new_from_rgba (&res);
      }

      break;
    case COLOR_TYPE_CURRENT_COLOR:
      if (current)
        {
          *dependencies = current_deps;
          return _gtk_css_value_ref (current);
        }
      else
        {
          return _gtk_css_color_value_resolve (_gtk_css_style_property_get_initial_value (_gtk_css_style_property_lookup_by_id (GTK_CSS_PROPERTY_COLOR)),
                                               provider,
                                               NULL,
                                               0,
                                               dependencies,
                                               cycle_list);
        }
      break;
    default:
      value = NULL;
      g_assert_not_reached ();
    }

  if (color->last_value != NULL &&
      _gtk_css_value_equal (color->last_value, value))
    {
      _gtk_css_value_unref (value);
      value = _gtk_css_value_ref (color->last_value);
    }
  else
    {
      if (color->last_value != NULL)
        _gtk_css_value_unref (color->last_value);
      color->last_value = _gtk_css_value_ref (value);
    }

  return value;
}

static GtkCssValue *
gtk_css_value_color_compute (GtkCssValue             *value,
                             guint                    property_id,
                             GtkStyleProviderPrivate *provider,
			     int                      scale,
                             GtkCssComputedValues    *values,
                             GtkCssComputedValues    *parent_values,
                             GtkCssDependencies      *dependencies)
{
  GtkCssValue *resolved, *current;
  GtkCssDependencies current_deps;

  /* The computed value of the ‘currentColor’ keyword is the computed
   * value of the ‘color’ property. If the ‘currentColor’ keyword is
   * set on the ‘color’ property itself, it is treated as ‘color: inherit’. 
   */
  if (property_id == GTK_CSS_PROPERTY_COLOR)
    {
      if (parent_values)
        {
          current = _gtk_css_computed_values_get_value (parent_values, GTK_CSS_PROPERTY_COLOR);
          current_deps = GTK_CSS_EQUALS_PARENT;
        }
      else
        {
          current = NULL;
          current_deps = 0;
        }
    }
  else
    {
      current = _gtk_css_computed_values_get_value (values, GTK_CSS_PROPERTY_COLOR);
      current_deps = GTK_CSS_DEPENDS_ON_COLOR;
    }
  
  resolved = _gtk_css_color_value_resolve (value,
                                           provider,
                                           current,
                                           current_deps,
                                           dependencies,
                                           NULL);

  if (resolved == NULL)
    return gtk_css_value_color_get_fallback (property_id, provider, scale, values, parent_values);

  return resolved;
}

static gboolean
gtk_css_value_color_equal (const GtkCssValue *value1,
                           const GtkCssValue *value2)
{
  if (value1->type != value2->type)
    return FALSE;

  switch (value1->type)
    {
    case COLOR_TYPE_LITERAL:
      return _gtk_css_value_equal (value1->last_value, value2->last_value);
    case COLOR_TYPE_NAME:
      return g_str_equal (value1->sym_col.name, value2->sym_col.name);
    case COLOR_TYPE_SHADE:
      return value1->sym_col.shade.factor == value2->sym_col.shade.factor &&
             _gtk_css_value_equal (value1->sym_col.shade.color,
                                   value2->sym_col.shade.color);
    case COLOR_TYPE_ALPHA:
      return value1->sym_col.alpha.factor == value2->sym_col.alpha.factor &&
             _gtk_css_value_equal (value1->sym_col.alpha.color,
                                   value2->sym_col.alpha.color);
    case COLOR_TYPE_MIX:
      return value1->sym_col.mix.factor == value2->sym_col.mix.factor &&
             _gtk_css_value_equal (value1->sym_col.mix.color1,
                                   value2->sym_col.mix.color1) &&
             _gtk_css_value_equal (value1->sym_col.mix.color2,
                                   value2->sym_col.mix.color2);
    case COLOR_TYPE_WIN32:
      return g_str_equal (value1->sym_col.win32.theme_class, value2->sym_col.win32.theme_class) &&
             value1->sym_col.win32.id == value2->sym_col.win32.id;
    case COLOR_TYPE_CURRENT_COLOR:
      return TRUE;
    default:
      g_assert_not_reached ();
      return FALSE;
    }
}

static GtkCssValue *
gtk_css_value_color_transition (GtkCssValue *start,
                                GtkCssValue *end,
                                guint        property_id,
                                double       progress)
{
  return _gtk_css_color_value_new_mix (start, end, progress);
}

static void
gtk_css_value_color_print (const GtkCssValue *value,
                           GString           *string)
{
  switch (value->type)
    {
    case COLOR_TYPE_LITERAL:
      _gtk_css_value_print (value->last_value, string);
      break;
    case COLOR_TYPE_NAME:
      g_string_append (string, "@");
      g_string_append (string, value->sym_col.name);
      break;
    case COLOR_TYPE_SHADE:
      {
        char factor[G_ASCII_DTOSTR_BUF_SIZE];

        g_string_append (string, "shade(");
        _gtk_css_value_print (value->sym_col.shade.color, string);
        g_string_append (string, ", ");
        g_ascii_dtostr (factor, sizeof (factor), value->sym_col.shade.factor);
        g_string_append (string, factor);
        g_string_append (string, ")");
      }
      break;
    case COLOR_TYPE_ALPHA:
      {
        char factor[G_ASCII_DTOSTR_BUF_SIZE];

        g_string_append (string, "alpha(");
        _gtk_css_value_print (value->sym_col.alpha.color, string);
        g_string_append (string, ", ");
        g_ascii_dtostr (factor, sizeof (factor), value->sym_col.alpha.factor);
        g_string_append (string, factor);
        g_string_append (string, ")");
      }
      break;
    case COLOR_TYPE_MIX:
      {
        char factor[G_ASCII_DTOSTR_BUF_SIZE];

        g_string_append (string, "mix(");
        _gtk_css_value_print (value->sym_col.mix.color1, string);
        g_string_append (string, ", ");
        _gtk_css_value_print (value->sym_col.mix.color2, string);
        g_string_append (string, ", ");
        g_ascii_dtostr (factor, sizeof (factor), value->sym_col.mix.factor);
        g_string_append (string, factor);
        g_string_append (string, ")");
      }
      break;
    case COLOR_TYPE_WIN32:
      {
        g_string_append_printf (string, GTK_WIN32_THEME_SYMBOLIC_COLOR_NAME"(%s, %d)", 
			        value->sym_col.win32.theme_class, value->sym_col.win32.id);
      }
      break;
    case COLOR_TYPE_CURRENT_COLOR:
      g_string_append (string, "currentColor");
      break;
    default:
      g_assert_not_reached ();
    }
}

static const GtkCssValueClass GTK_CSS_VALUE_COLOR = {
  gtk_css_value_color_free,
  gtk_css_value_color_compute,
  gtk_css_value_color_equal,
  gtk_css_value_color_transition,
  gtk_css_value_color_print
};

GtkCssValue *
_gtk_css_color_value_new_literal (const GdkRGBA *color)
{
  GtkCssValue *value;

  g_return_val_if_fail (color != NULL, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_LITERAL;
  value->last_value = _gtk_css_rgba_value_new_from_rgba (color);

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_rgba (double red,
                               double green,
                               double blue,
                               double alpha)
{
  GdkRGBA rgba = { red, green, blue, alpha };

  return _gtk_css_color_value_new_literal (&rgba);
}

GtkCssValue *
_gtk_css_color_value_new_name (const gchar *name)
{
  GtkCssValue *value;

  g_return_val_if_fail (name != NULL, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_NAME;
  value->sym_col.name = g_strdup (name);

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_shade (GtkCssValue *color,
                                gdouble      factor)
{
  GtkCssValue *value;

  g_return_val_if_fail (color->class == &GTK_CSS_VALUE_COLOR, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_SHADE;
  value->sym_col.shade.color = _gtk_css_value_ref (color);
  value->sym_col.shade.factor = factor;

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_alpha (GtkCssValue *color,
                                gdouble      factor)
{
  GtkCssValue *value;

  g_return_val_if_fail (color->class == &GTK_CSS_VALUE_COLOR, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_ALPHA;
  value->sym_col.alpha.color = _gtk_css_value_ref (color);
  value->sym_col.alpha.factor = factor;

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_mix (GtkCssValue *color1,
                              GtkCssValue *color2,
                              gdouble      factor)
{
  GtkCssValue *value;

  g_return_val_if_fail (color1->class == &GTK_CSS_VALUE_COLOR, NULL);
  g_return_val_if_fail (color2->class == &GTK_CSS_VALUE_COLOR, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_MIX;
  value->sym_col.mix.color1 = _gtk_css_value_ref (color1);
  value->sym_col.mix.color2 = _gtk_css_value_ref (color2);
  value->sym_col.mix.factor = factor;

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_win32 (const gchar *theme_class,
                                gint         id)
{
  GtkCssValue *value;

  g_return_val_if_fail (theme_class != NULL, NULL);

  value = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_COLOR);
  value->type = COLOR_TYPE_WIN32;
  value->sym_col.win32.theme_class = g_strdup (theme_class);
  value->sym_col.win32.id = id;

  return value;
}

GtkCssValue *
_gtk_css_color_value_new_current_color (void)
{
  static GtkCssValue current_color = { &GTK_CSS_VALUE_COLOR, 1, COLOR_TYPE_CURRENT_COLOR, NULL, };

  return _gtk_css_value_ref (&current_color);
}

typedef enum {
  COLOR_RGBA,
  COLOR_RGB,
  COLOR_LIGHTER,
  COLOR_DARKER,
  COLOR_SHADE,
  COLOR_ALPHA,
  COLOR_MIX,
  COLOR_WIN32
} ColorParseType;

static GtkCssValue *
gtk_css_color_parse_win32 (GtkCssParser *parser)
{
  GtkCssValue *color;
  char *class;
  int id;

  class = _gtk_css_parser_try_name (parser, TRUE);
  if (class == NULL)
    {
      _gtk_css_parser_error (parser,
			     "Expected name as first argument to  '-gtk-win32-color'");
      return NULL;
    }

  if (! _gtk_css_parser_try (parser, ",", TRUE))
    {
      g_free (class);
      _gtk_css_parser_error (parser,
			     "Expected ','");
      return NULL;
    }

  if (!_gtk_css_parser_try_int (parser, &id))
    {
      g_free (class);
      _gtk_css_parser_error (parser, "Expected a valid integer value");
      return NULL;
    }

  color = _gtk_css_color_value_new_win32 (class, id);
  g_free (class);
  return color;
}

static GtkCssValue *
_gtk_css_color_value_parse_function (GtkCssParser   *parser,
                                     ColorParseType  color)
{
  GtkCssValue *value;
  GtkCssValue *child1, *child2;
  double d;

  if (!_gtk_css_parser_try (parser, "(", TRUE))
    {
      _gtk_css_parser_error (parser, "Missing opening bracket in color definition");
      return NULL;
    }

  if (color == COLOR_RGB || color == COLOR_RGBA)
    {
      GdkRGBA rgba;
      double tmp;
      guint i;

      for (i = 0; i < 3; i++)
        {
          if (i > 0 && !_gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser, "Expected ',' in color definition");
              return NULL;
            }

          if (!_gtk_css_parser_try_double (parser, &tmp))
            {
              _gtk_css_parser_error (parser, "Invalid number for color value");
              return NULL;
            }
          if (_gtk_css_parser_try (parser, "%", TRUE))
            tmp /= 100.0;
          else
            tmp /= 255.0;
          if (i == 0)
            rgba.red = tmp;
          else if (i == 1)
            rgba.green = tmp;
          else if (i == 2)
            rgba.blue = tmp;
          else
            g_assert_not_reached ();
        }

      if (color == COLOR_RGBA)
        {
          if (i > 0 && !_gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser, "Expected ',' in color definition");
              return NULL;
            }

          if (!_gtk_css_parser_try_double (parser, &rgba.alpha))
            {
              _gtk_css_parser_error (parser, "Invalid number for alpha value");
              return NULL;
            }
        }
      else
        rgba.alpha = 1.0;
      
      value = _gtk_css_color_value_new_literal (&rgba);
    }
  else if (color == COLOR_WIN32)
    {
      value = gtk_css_color_parse_win32 (parser);
      if (value == NULL)
	return NULL;
    }
  else
    {
      child1 = _gtk_css_color_value_parse (parser);
      if (child1 == NULL)
        return NULL;

      if (color == COLOR_MIX)
        {
          if (!_gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser, "Expected ',' in color definition");
              _gtk_css_value_unref (child1);
              return NULL;
            }

          child2 = _gtk_css_color_value_parse (parser);
          if (child2 == NULL)
            {
              _gtk_css_value_unref (child1);
              return NULL;
            }
        }
      else
        child2 = NULL;

      if (color == COLOR_LIGHTER)
        d = 1.3;
      else if (color == COLOR_DARKER)
        d = 0.7;
      else
        {
          if (!_gtk_css_parser_try (parser, ",", TRUE))
            {
              _gtk_css_parser_error (parser, "Expected ',' in color definition");
              _gtk_css_value_unref (child1);
              if (child2)
                _gtk_css_value_unref (child2);
              return NULL;
            }

          if (!_gtk_css_parser_try_double (parser, &d))
            {
              _gtk_css_parser_error (parser, "Expected number in color definition");
              _gtk_css_value_unref (child1);
              if (child2)
                _gtk_css_value_unref (child2);
              return NULL;
            }
        }
      
      switch (color)
        {
        case COLOR_LIGHTER:
        case COLOR_DARKER:
        case COLOR_SHADE:
          value = _gtk_css_color_value_new_shade (child1, d);
          break;
        case COLOR_ALPHA:
          value = _gtk_css_color_value_new_alpha (child1, d);
          break;
        case COLOR_MIX:
          value = _gtk_css_color_value_new_mix (child1, child2, d);
          break;
        default:
          g_assert_not_reached ();
          value = NULL;
        }

      _gtk_css_value_unref (child1);
      if (child2)
        _gtk_css_value_unref (child2);
    }

  if (!_gtk_css_parser_try (parser, ")", TRUE))
    {
      _gtk_css_parser_error (parser, "Expected ')' in color definition");
      _gtk_css_value_unref (value);
      return NULL;
    }

  return value;
}

GtkCssValue *
_gtk_css_color_value_parse (GtkCssParser *parser)
{
  GtkCssValue *value;
  GdkRGBA rgba;
  guint color;
  const char *names[] = {"rgba", "rgb",  "lighter", "darker", "shade", "alpha", "mix",
			 GTK_WIN32_THEME_SYMBOLIC_COLOR_NAME};
  char *name;

  if (_gtk_css_parser_try (parser, "currentColor", TRUE))
    return _gtk_css_color_value_new_current_color ();

  if (_gtk_css_parser_try (parser, "transparent", TRUE))
    {
      GdkRGBA transparent = { 0, 0, 0, 0 };
      
      return _gtk_css_color_value_new_literal (&transparent);
    }

  if (_gtk_css_parser_try (parser, "@", FALSE))
    {
      name = _gtk_css_parser_try_name (parser, TRUE);

      if (name)
        {
          value = _gtk_css_color_value_new_name (name);
        }
      else
        {
          _gtk_css_parser_error (parser, "'%s' is not a valid color color name", name);
          value = NULL;
        }

      g_free (name);
      return value;
    }

  for (color = 0; color < G_N_ELEMENTS (names); color++)
    {
      if (_gtk_css_parser_try (parser, names[color], TRUE))
        break;
    }

  if (color < G_N_ELEMENTS (names))
    return _gtk_css_color_value_parse_function (parser, color);

  if (_gtk_css_parser_try_hash_color (parser, &rgba))
    return _gtk_css_color_value_new_literal (&rgba);

  name = _gtk_css_parser_try_name (parser, TRUE);
  if (name)
    {
      if (gdk_rgba_parse (&rgba, name))
        {
          value = _gtk_css_color_value_new_literal (&rgba);
        }
      else
        {
          _gtk_css_parser_error (parser, "'%s' is not a valid color name", name);
          value = NULL;
        }
      g_free (name);
      return value;
    }

  _gtk_css_parser_error (parser, "Not a color definition");
  return NULL;
}

