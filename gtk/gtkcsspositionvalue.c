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

#include "gtkcsspositionvalueprivate.h"

#include "gtkcssnumbervalueprivate.h"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssValue *x;
  GtkCssValue *y;
};

static void
gtk_css_value_position_free (GtkCssValue *value)
{
  _gtk_css_value_unref (value->x);
  _gtk_css_value_unref (value->y);

  g_slice_free (GtkCssValue, value);
}

static GtkCssValue *
gtk_css_value_position_compute (GtkCssValue             *position,
                                guint                    property_id,
                                GtkStyleProviderPrivate *provider,
				int                      scale,
                                GtkCssComputedValues    *values,
                                GtkCssComputedValues    *parent_values,
                                GtkCssDependencies      *dependencies)
{
  GtkCssValue *x, *y;
  GtkCssDependencies x_deps, y_deps;

  x = _gtk_css_value_compute (position->x, property_id, provider, scale, values, parent_values, &x_deps);
  y = _gtk_css_value_compute (position->y, property_id, provider, scale, values, parent_values, &y_deps);
  *dependencies = _gtk_css_dependencies_union (x_deps, y_deps);
  if (x == position->x && y == position->y)
    {
      _gtk_css_value_unref (x);
      _gtk_css_value_unref (y);
      return _gtk_css_value_ref (position);
    }

  return _gtk_css_position_value_new (x, y);
}

static gboolean
gtk_css_value_position_equal (const GtkCssValue *position1,
                              const GtkCssValue *position2)
{
  return _gtk_css_value_equal (position1->x, position2->x)
      && _gtk_css_value_equal (position1->y, position2->y);
}

static GtkCssValue *
gtk_css_value_position_transition (GtkCssValue *start,
                                   GtkCssValue *end,
                                   guint        property_id,
                                   double       progress)
{
  GtkCssValue *x, *y;

  x = _gtk_css_value_transition (start->x, end->x, property_id, progress);
  if (x == NULL)
    return NULL;
  y = _gtk_css_value_transition (start->y, end->y, property_id, progress);
  if (y == NULL)
    {
      _gtk_css_value_unref (x);
      return NULL;
    }

  return _gtk_css_position_value_new (x, y);
}

static void
gtk_css_value_position_print (const GtkCssValue *position,
                              GString           *string)
{
  struct {
    const char *x_name;
    const char *y_name;
    GtkCssValue *number;
  } values[] = { 
    { "left",   "top",    _gtk_css_number_value_new (0, GTK_CSS_PERCENT) },
    { "right",  "bottom", _gtk_css_number_value_new (100, GTK_CSS_PERCENT) }
  };
  GtkCssValue *center = _gtk_css_number_value_new (50, GTK_CSS_PERCENT);
  guint i;

  if (_gtk_css_value_equal (position->x, center))
    {
      if (_gtk_css_value_equal (position->y, center))
        {
          g_string_append (string, "center");
          goto done;
        }
    }
  else
    {
      for (i = 0; i < G_N_ELEMENTS (values); i++)
        {
          if (_gtk_css_value_equal (position->x, values[i].number))
            {
              g_string_append (string, values[i].x_name);
              break;
            }
        }
      if (i == G_N_ELEMENTS (values))
        _gtk_css_value_print (position->x, string);

      if (_gtk_css_value_equal (position->y, center))
        goto done;

      g_string_append_c (string, ' ');
    }

  for (i = 0; i < G_N_ELEMENTS (values); i++)
    {
      if (_gtk_css_value_equal (position->y, values[i].number))
        {
          g_string_append (string, values[i].y_name);
          goto done;
        }
    }
  if (i == G_N_ELEMENTS (values))
    {
      if (_gtk_css_value_equal (position->x, center))
        g_string_append (string, "center ");
      _gtk_css_value_print (position->y, string);
    }

done:
  for (i = 0; i < G_N_ELEMENTS (values); i++)
    _gtk_css_value_unref (values[i].number);
  _gtk_css_value_unref (center);
}

static const GtkCssValueClass GTK_CSS_VALUE_POSITION = {
  gtk_css_value_position_free,
  gtk_css_value_position_compute,
  gtk_css_value_position_equal,
  gtk_css_value_position_transition,
  gtk_css_value_position_print
};

GtkCssValue *
_gtk_css_position_value_new (GtkCssValue *x,
                             GtkCssValue *y)
{
  GtkCssValue *result;

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_POSITION);
  result->x = x;
  result->y = y;

  return result;
}

static GtkCssValue *
position_value_parse (GtkCssParser *parser, gboolean try)
{
  static const struct {
    const char *name;
    guint       percentage;
    gboolean    horizontal;
    gboolean    vertical;
  } names[] = {
    { "left",     0, TRUE,  FALSE },
    { "right",  100, TRUE,  FALSE },
    { "center",  50, TRUE,  TRUE  },
    { "top",      0, FALSE, TRUE  },
    { "bottom", 100, FALSE, TRUE  },
    { NULL    ,   0, TRUE,  FALSE }, /* used for numbers */
    { NULL    ,  50, TRUE,  TRUE  }  /* used for no value */
  };
  GtkCssValue *x, *y;
  GtkCssValue **missing;
  guint first, second;

  for (first = 0; names[first].name != NULL; first++)
    {
      if (_gtk_css_parser_try (parser, names[first].name, TRUE))
        {
          if (names[first].horizontal)
            {
	      x = _gtk_css_number_value_new (names[first].percentage, GTK_CSS_PERCENT);
              missing = &y;
            }
          else
            {
	      y = _gtk_css_number_value_new (names[first].percentage, GTK_CSS_PERCENT);
              missing = &x;
            }
          break;
        }
    }
  if (names[first].name == NULL)
    {
      if (_gtk_css_parser_has_number (parser))
        {
          missing = &y;
          x = _gtk_css_number_value_parse (parser,
                                           GTK_CSS_PARSE_PERCENT
                                           | GTK_CSS_PARSE_LENGTH);

          if (x == NULL)
            return NULL;
        }
      else
        {
          if (!try)
            _gtk_css_parser_error (parser, "Unrecognized position value");
          return NULL;
        }
    }

  for (second = 0; names[second].name != NULL; second++)
    {
      if (_gtk_css_parser_try (parser, names[second].name, TRUE))
        {
	  *missing = _gtk_css_number_value_new (names[second].percentage, GTK_CSS_PERCENT);
          break;
        }
    }

  if (names[second].name == NULL)
    {
      if (_gtk_css_parser_has_number (parser))
        {
          if (missing != &y)
            {
              if (!try)
                _gtk_css_parser_error (parser, "Invalid combination of values");
              _gtk_css_value_unref (y);
              return NULL;
            }
          y = _gtk_css_number_value_parse (parser,
                                           GTK_CSS_PARSE_PERCENT
                                           | GTK_CSS_PARSE_LENGTH);
          if (y == NULL)
            {
              _gtk_css_value_unref (x);
	      return NULL;
            }
        }
      else
        {
          second++;
          *missing = _gtk_css_number_value_new (50, GTK_CSS_PERCENT);
        }
    }
  else
    {
      if ((names[first].horizontal && !names[second].vertical) ||
          (!names[first].horizontal && !names[second].horizontal))
        {
          if (!try)
            _gtk_css_parser_error (parser, "Invalid combination of values");
          _gtk_css_value_unref (x);
          _gtk_css_value_unref (y);
          return NULL;
        }
    }

  return _gtk_css_position_value_new (x, y);
}

GtkCssValue *
_gtk_css_position_value_parse (GtkCssParser *parser)
{
  return position_value_parse (parser, FALSE);
}

GtkCssValue *
_gtk_css_position_value_try_parse (GtkCssParser *parser)
{
  return position_value_parse (parser, TRUE);
}

double
_gtk_css_position_value_get_x (const GtkCssValue *position,
                               double             one_hundred_percent)
{
  g_return_val_if_fail (position != NULL, 0.0);
  g_return_val_if_fail (position->class == &GTK_CSS_VALUE_POSITION, 0.0);

  return _gtk_css_number_value_get (position->x, one_hundred_percent);
}

double
_gtk_css_position_value_get_y (const GtkCssValue *position,
                               double             one_hundred_percent)
{
  g_return_val_if_fail (position != NULL, 0.0);
  g_return_val_if_fail (position->class == &GTK_CSS_VALUE_POSITION, 0.0);

  return _gtk_css_number_value_get (position->y, one_hundred_percent);
}

