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

#include "gtkcssdimensionvalueprivate.h"

#include "gtkcssenumvalueprivate.h"
#include "gtkstylepropertyprivate.h"

#include "fallback-c89.c"

struct _GtkCssValue {
  GTK_CSS_VALUE_BASE
  GtkCssUnit unit;
  double value;
};

static void
gtk_css_value_dimension_free (GtkCssValue *value)
{
  g_slice_free (GtkCssValue, value);
}

static double
get_base_font_size_px (guint             property_id,
                       GtkStyleProvider *provider,
                       GtkCssStyle      *style,
                       GtkCssStyle      *parent_style)
{
  if (property_id == GTK_CSS_PROPERTY_FONT_SIZE)
    {
      if (parent_style)
        return _gtk_css_number_value_get (gtk_css_style_get_value (parent_style, GTK_CSS_PROPERTY_FONT_SIZE), 100);
      else
        return gtk_css_font_size_get_default_px (provider, style);
    }

  return _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_FONT_SIZE), 100);
}

static double
get_dpi (GtkCssStyle *style)
{
  return _gtk_css_number_value_get (gtk_css_style_get_value (style, GTK_CSS_PROPERTY_DPI), 96);
}

static GtkCssValue *
gtk_css_value_dimension_compute (GtkCssValue      *number,
                                 guint             property_id,
                                 GtkStyleProvider *provider,
                                 GtkCssStyle      *style,
                                 GtkCssStyle      *parent_style)
{
  GtkBorderStyle border_style;

  /* special case according to http://dev.w3.org/csswg/css-backgrounds/#the-border-width */
  switch (property_id)
    {
      case GTK_CSS_PROPERTY_BORDER_TOP_WIDTH:
        border_style = _gtk_css_border_style_value_get(gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_TOP_STYLE));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          return gtk_css_dimension_value_new (0, GTK_CSS_NUMBER);
        break;
      case GTK_CSS_PROPERTY_BORDER_RIGHT_WIDTH:
        border_style = _gtk_css_border_style_value_get(gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_RIGHT_STYLE));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          return gtk_css_dimension_value_new (0, GTK_CSS_NUMBER);
        break;
      case GTK_CSS_PROPERTY_BORDER_BOTTOM_WIDTH:
        border_style = _gtk_css_border_style_value_get(gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_BOTTOM_STYLE));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          return gtk_css_dimension_value_new (0, GTK_CSS_NUMBER);
        break;
      case GTK_CSS_PROPERTY_BORDER_LEFT_WIDTH:
        border_style = _gtk_css_border_style_value_get(gtk_css_style_get_value (style, GTK_CSS_PROPERTY_BORDER_LEFT_STYLE));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          return gtk_css_dimension_value_new (0, GTK_CSS_NUMBER);
        break;
      case GTK_CSS_PROPERTY_OUTLINE_WIDTH:
        border_style = _gtk_css_border_style_value_get(gtk_css_style_get_value (style, GTK_CSS_PROPERTY_OUTLINE_STYLE));
        if (border_style == GTK_BORDER_STYLE_NONE || border_style == GTK_BORDER_STYLE_HIDDEN)
          return gtk_css_dimension_value_new (0, GTK_CSS_NUMBER);
        break;
      default:
        break;
    }

  switch (number->unit)
    {
    default:
      g_assert_not_reached();
      G_GNUC_FALLTHROUGH;
    case GTK_CSS_PERCENT:
      /* percentages for font sizes are computed, other percentages aren't */
      if (property_id == GTK_CSS_PROPERTY_FONT_SIZE)
        return gtk_css_dimension_value_new (number->value / 100.0 * 
                                            get_base_font_size_px (property_id, provider, style, parent_style),
                                            GTK_CSS_PX);
      G_GNUC_FALLTHROUGH;
    case GTK_CSS_NUMBER:
    case GTK_CSS_PX:
    case GTK_CSS_DEG:
    case GTK_CSS_S:
      return _gtk_css_value_ref (number);
    case GTK_CSS_PT:
      return gtk_css_dimension_value_new (number->value * get_dpi (style) / 72.0,
                                          GTK_CSS_PX);
    case GTK_CSS_PC:
      return gtk_css_dimension_value_new (number->value * get_dpi (style) / 72.0 * 12.0,
                                          GTK_CSS_PX);
    case GTK_CSS_IN:
      return gtk_css_dimension_value_new (number->value * get_dpi (style),
                                          GTK_CSS_PX);
    case GTK_CSS_CM:
      return gtk_css_dimension_value_new (number->value * get_dpi (style) * 0.39370078740157477,
                                          GTK_CSS_PX);
    case GTK_CSS_MM:
      return gtk_css_dimension_value_new (number->value * get_dpi (style) * 0.039370078740157477,
                                          GTK_CSS_PX);
    case GTK_CSS_EM:
      return gtk_css_dimension_value_new (number->value *
                                          get_base_font_size_px (property_id, provider, style, parent_style),
                                          GTK_CSS_PX);
    case GTK_CSS_EX:
      /* for now we pretend ex is half of em */
      return gtk_css_dimension_value_new (number->value * 0.5 *
                                          get_base_font_size_px (property_id, provider, style, parent_style),
                                          GTK_CSS_PX);
    case GTK_CSS_REM:
      return gtk_css_dimension_value_new (number->value *
                                          gtk_css_font_size_get_default_px (provider, style),
                                          GTK_CSS_PX);
    case GTK_CSS_RAD:
      return gtk_css_dimension_value_new (number->value * 360.0 / (2 * G_PI),
                                          GTK_CSS_DEG);
    case GTK_CSS_GRAD:
      return gtk_css_dimension_value_new (number->value * 360.0 / 400.0,
                                          GTK_CSS_DEG);
    case GTK_CSS_TURN:
      return gtk_css_dimension_value_new (number->value * 360.0,
                                          GTK_CSS_DEG);
    case GTK_CSS_MS:
      return gtk_css_dimension_value_new (number->value / 1000.0,
                                          GTK_CSS_S);
    }
}

static gboolean
gtk_css_value_dimension_equal (const GtkCssValue *number1,
                            const GtkCssValue *number2)
{
  return number1->unit == number2->unit &&
         number1->value == number2->value;
}

static void
gtk_css_value_dimension_print (const GtkCssValue *number,
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
    /* [GTK_CSS_REM] = */ "rem",
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

  if (isinf (number->value))
    g_string_append (string, "infinite");
  else
    {
      g_ascii_dtostr (buf, sizeof (buf), number->value);
      g_string_append (string, buf);
      if (number->value != 0.0)
        g_string_append (string, names[number->unit]);
    }
}

static double
gtk_css_value_dimension_get (const GtkCssValue *value,
                             double             one_hundred_percent)
{
  if (value->unit == GTK_CSS_PERCENT)
    return value->value * one_hundred_percent / 100;
  else
    return value->value;
}

static GtkCssDimension
gtk_css_value_dimension_get_dimension (const GtkCssValue *value)
{
  return gtk_css_unit_get_dimension (value->unit);
}

static gboolean
gtk_css_value_dimension_has_percent (const GtkCssValue *value)
{
  return gtk_css_unit_get_dimension (value->unit) == GTK_CSS_DIMENSION_PERCENTAGE;
}

static GtkCssValue *
gtk_css_value_dimension_multiply (const GtkCssValue *value,
                                  double             factor)
{
  return gtk_css_dimension_value_new (value->value * factor, value->unit);
}

static GtkCssValue *
gtk_css_value_dimension_try_add (const GtkCssValue *value1,
                                 const GtkCssValue *value2)
{
  if (value1->unit != value2->unit)
    return NULL;

  return gtk_css_dimension_value_new (value1->value + value2->value, value1->unit);
}

static gint
gtk_css_value_dimension_get_calc_term_order (const GtkCssValue *value)
{
  /* note: the order is alphabetic */
  guint order_per_unit[] = {
    /* [GTK_CSS_NUMBER] = */ 0,
    /* [GTK_CSS_PERCENT] = */ 16,
    /* [GTK_CSS_PX] = */ 11,
    /* [GTK_CSS_PT] = */ 10,
    /* [GTK_CSS_EM] = */ 3,
    /* [GTK_CSS_EX] = */ 4,
    /* [GTK_CSS_REM] = */ 13,
    /* [GTK_CSS_PC] = */ 9,
    /* [GTK_CSS_IN] = */ 6,
    /* [GTK_CSS_CM] = */ 1,
    /* [GTK_CSS_MM] = */ 7,
    /* [GTK_CSS_RAD] = */ 12,
    /* [GTK_CSS_DEG] = */ 2,
    /* [GTK_CSS_GRAD] = */ 5,
    /* [GTK_CSS_TURN] = */ 15,
    /* [GTK_CSS_S] = */ 14,
    /* [GTK_CSS_MS] = */ 8
  };

  return 1000 + order_per_unit[value->unit];
}

static const GtkCssNumberValueClass GTK_CSS_VALUE_DIMENSION = {
  {
    gtk_css_value_dimension_free,
    gtk_css_value_dimension_compute,
    gtk_css_value_dimension_equal,
    gtk_css_number_value_transition,
    NULL,
    NULL,
    gtk_css_value_dimension_print
  },
  gtk_css_value_dimension_get,
  gtk_css_value_dimension_get_dimension,
  gtk_css_value_dimension_has_percent,
  gtk_css_value_dimension_multiply,
  gtk_css_value_dimension_try_add,
  gtk_css_value_dimension_get_calc_term_order
};

GtkCssValue *
gtk_css_dimension_value_new (double     value,
                             GtkCssUnit unit)
{
  static GtkCssValue number_singletons[] = {
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_NUMBER, 0 },
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_NUMBER, 1 },
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_NUMBER, 96 }, /* DPI default */
  };
  static GtkCssValue px_singletons[] = {
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_PX, 0 },
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_PX, 1 },
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_PX, 2 },
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_PX, 3 },
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_PX, 4 },
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_PX, 16 }, /* Icon size default */
  };
  static GtkCssValue percent_singletons[] = {
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_PERCENT, 0 },
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_PERCENT, 100 },
  };
  static GtkCssValue second_singletons[] = {
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_S, 0 },
    { &GTK_CSS_VALUE_DIMENSION.value_class, 1, GTK_CSS_S, 1 },
  };
  GtkCssValue *result;

  switch ((guint)unit)
    {
    case GTK_CSS_NUMBER:
      if (value == 0 || value == 1)
        return _gtk_css_value_ref (&number_singletons[(int) value]);

      if (value == 96)
        return _gtk_css_value_ref (&number_singletons[2]);

      break;

    case GTK_CSS_PX:
      if (value == 0 ||
          value == 1 ||
          value == 2 ||
          value == 3 ||
          value == 4)
        return _gtk_css_value_ref (&px_singletons[(int) value]);
      else if (value == 16)
        return _gtk_css_value_ref (&px_singletons[5]);

      break;

    case GTK_CSS_PERCENT:
      if (value == 0)
        return _gtk_css_value_ref (&percent_singletons[0]);

      if (value == 100)
        return _gtk_css_value_ref (&percent_singletons[1]);

      break;

    case GTK_CSS_S:
      if (value == 0 || value == 1)
        return _gtk_css_value_ref (&second_singletons[(int)value]);

      break;

    default:
      ;
    }

  result = _gtk_css_value_new (GtkCssValue, &GTK_CSS_VALUE_DIMENSION.value_class);
  result->unit = unit;
  result->value = value;

  return result;
}

GtkCssValue *
gtk_css_dimension_value_parse (GtkCssParser           *parser,
                               GtkCssNumberParseFlags  flags)
{
  static const struct {
    const char *name;
    GtkCssUnit unit;
    GtkCssNumberParseFlags required_flags;
  } units[] = {
    { "px",   GTK_CSS_PX,      GTK_CSS_PARSE_LENGTH },
    { "pt",   GTK_CSS_PT,      GTK_CSS_PARSE_LENGTH },
    { "em",   GTK_CSS_EM,      GTK_CSS_PARSE_LENGTH },
    { "ex",   GTK_CSS_EX,      GTK_CSS_PARSE_LENGTH },
    { "rem",  GTK_CSS_REM,     GTK_CSS_PARSE_LENGTH },
    { "pc",   GTK_CSS_PC,      GTK_CSS_PARSE_LENGTH },
    { "in",   GTK_CSS_IN,      GTK_CSS_PARSE_LENGTH },
    { "cm",   GTK_CSS_CM,      GTK_CSS_PARSE_LENGTH },
    { "mm",   GTK_CSS_MM,      GTK_CSS_PARSE_LENGTH },
    { "rad",  GTK_CSS_RAD,     GTK_CSS_PARSE_ANGLE  },
    { "deg",  GTK_CSS_DEG,     GTK_CSS_PARSE_ANGLE  },
    { "grad", GTK_CSS_GRAD,    GTK_CSS_PARSE_ANGLE  },
    { "turn", GTK_CSS_TURN,    GTK_CSS_PARSE_ANGLE  },
    { "s",    GTK_CSS_S,       GTK_CSS_PARSE_TIME   },
    { "ms",   GTK_CSS_MS,      GTK_CSS_PARSE_TIME   }
  };
  const GtkCssToken *token;
  GtkCssValue *result;
  GtkCssUnit unit;
  double number;

  token = gtk_css_parser_get_token (parser);

  /* Handle percentages first */
  if (gtk_css_token_is (token, GTK_CSS_TOKEN_PERCENTAGE))
    {
      if (!(flags & GTK_CSS_PARSE_PERCENT))
        {
          gtk_css_parser_error_value (parser, "Percentages are not allowed here");
          return NULL;
        }
      number = token->number.number;
      unit = GTK_CSS_PERCENT;
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      number = token->number.number;
      if (number == 0.0)
        {
          if (flags & GTK_CSS_PARSE_NUMBER)
            unit = GTK_CSS_NUMBER;
          else if (flags & GTK_CSS_PARSE_LENGTH)
            unit = GTK_CSS_PX;
          else if (flags & GTK_CSS_PARSE_ANGLE)
            unit = GTK_CSS_DEG;
          else if (flags & GTK_CSS_PARSE_TIME)
            unit = GTK_CSS_S;
          else
            unit = GTK_CSS_PERCENT;
        }
      else if (flags & GTK_CSS_PARSE_NUMBER)
        {
          unit = GTK_CSS_NUMBER;
        }
      else
        {
          gtk_css_parser_error_syntax (parser, "Unit is missing.");
          return NULL;
        }
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_DIMENSION))
    {
      guint i;

      for (i = 0; i < G_N_ELEMENTS (units); i++)
        {
          if (flags & units[i].required_flags &&
              g_ascii_strcasecmp (token->dimension.dimension, units[i].name) == 0)
            break;
        }

      if (i >= G_N_ELEMENTS (units))
        {
          gtk_css_parser_error_syntax (parser, "'%s' is not a valid unit.", token->dimension.dimension);
          return NULL;
        }

      unit = units[i].unit;
      number = token->dimension.value;
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "Expected a number");
      return NULL;
    }

  if (flags & GTK_CSS_POSITIVE_ONLY &&
      number < 0)
    {
      gtk_css_parser_error_value (parser, "negative values are not allowed.");
      return NULL;
    }

  result = gtk_css_dimension_value_new (number, unit);
  gtk_css_parser_consume_token (parser);

  return result;
}

