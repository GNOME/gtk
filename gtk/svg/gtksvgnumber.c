/*
 * Copyright © 2025 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksvgvalueprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgstringutilsprivate.h"
#include "gtksvgenumprivate.h"

static const char * unit_names[] = {
  [SVG_UNIT_NUMBER] = "",
  [SVG_UNIT_PERCENTAGE] = "%",
  [SVG_UNIT_PX] = "px",
  [SVG_UNIT_PT] = "pt",
  [SVG_UNIT_IN] = "in",
  [SVG_UNIT_CM] = "cm",
  [SVG_UNIT_MM] = "mm",
  [SVG_UNIT_VW] = "vw",
  [SVG_UNIT_VH] = "vh",
  [SVG_UNIT_VMIN] = "vmin",
  [SVG_UNIT_VMAX] = "vmax",
  [SVG_UNIT_EM] = "em",
  [SVG_UNIT_EX] = "ex",
  [SVG_UNIT_DEG] = "deg",
  [SVG_UNIT_RAD] = "rad",
  [SVG_UNIT_GRAD] = "grad",
  [SVG_UNIT_TURN] = "turn",
};

const char *
svg_unit_name (SvgUnit unit)
{
  return unit_names[unit];
}

typedef struct
{
  SvgValue base;
  SvgUnit unit;
  double value;
} SvgNumber;

static gboolean
svg_number_equal (const SvgValue *value0,
                  const SvgValue *value1)
{
  const SvgNumber *n0 = (const SvgNumber *) value0;
  const SvgNumber *n1 = (const SvgNumber *) value1;

  return n0->unit == n1->unit && n0->value == n1->value;
}

static SvgValue *
svg_number_interpolate (const SvgValue    *value0,
                        const SvgValue    *value1,
                        SvgComputeContext *context,
                        double             t)
{
  const SvgNumber *n0 = (const SvgNumber *) value0;
  const SvgNumber *n1 = (const SvgNumber *) value1;

  if (n0->unit != n1->unit)
    return NULL;

  return svg_number_new_full (n0->unit, lerp (t, n0->value, n1->value));
}

static SvgValue *
svg_number_accumulate (const SvgValue    *value0,
                       const SvgValue    *value1,
                       SvgComputeContext *context,
                       int                n)
{
  const SvgNumber *n0 = (const SvgNumber *) value0;
  const SvgNumber *n1 = (const SvgNumber *) value1;

  if (n0->unit != n1->unit)
    return NULL;

  return svg_number_new_full (n0->unit, accumulate (n0->value, n1->value, n));
}

static void
svg_number_print (const SvgValue *value,
                  GString        *string)
{
  const SvgNumber *n = (const SvgNumber *) value;
  string_append_double (string, "", n->value);
  g_string_append (string, unit_names[n->unit]);
}

static double
svg_number_distance (const SvgValue *value0,
                     const SvgValue *value1)
{
  const SvgNumber *n0 = (const SvgNumber *) value0;
  const SvgNumber *n1 = (const SvgNumber *) value1;

  if (n0->unit != n1->unit)
    return 1;

  return fabs (n0->value - n1->value);
}

gboolean
is_absolute_length (SvgUnit unit)
{
  switch ((unsigned int) unit)
    {
    case SVG_UNIT_PX:
    case SVG_UNIT_PT:
    case SVG_UNIT_IN:
    case SVG_UNIT_CM:
    case SVG_UNIT_MM:
      return TRUE;
    default:
      return FALSE;
    }
}

double
absolute_length_to_px (double  value,
                       SvgUnit unit)
{
  switch ((unsigned int) unit)
    {
    case SVG_UNIT_PX: return value;
    case SVG_UNIT_PT: return value * 96 / 72;
    case SVG_UNIT_IN: return value * 96;
    case SVG_UNIT_CM: return value * 96 / 2.54;
    case SVG_UNIT_MM: return value * 96 / 25.4;
    default: g_assert_not_reached ();
    }
}

double
angle_to_deg (double  value,
              SvgUnit unit)
{
  switch ((unsigned int) unit)
    {
    case SVG_UNIT_DEG: return value;
    case SVG_UNIT_RAD: return value * 180.0 / M_PI;
    case SVG_UNIT_GRAD: return value * 360.0 / 400.0;
    case SVG_UNIT_TURN: return value * 360.0;
    default: g_assert_not_reached ();
    }
}

double
viewport_relative_to_px (double                 value,
                         SvgUnit                unit,
                         const graphene_rect_t *viewport)
{
  switch ((unsigned int) unit)
    {
    case SVG_UNIT_VW: return value * viewport->size.width / 100;
    case SVG_UNIT_VH: return value * viewport->size.height / 100;
    case SVG_UNIT_VMIN: return value * MIN (viewport->size.width,
                                            viewport->size.height) / 100;
    case SVG_UNIT_VMAX: return value * MAX (viewport->size.width,
                                            viewport->size.height) / 100;
    default: g_assert_not_reached ();
    }
}

double
shape_get_current_font_size (Shape             *shape,
                             ShapeAttr          attr,
                             SvgComputeContext *context)
{
  /* FIXME: units */
  if (attr != SHAPE_ATTR_FONT_SIZE)
    return ((SvgNumber *) shape->current[SHAPE_ATTR_FONT_SIZE])->value;
  else if (context->parent)
    return ((SvgNumber *) context->parent->current[SHAPE_ATTR_FONT_SIZE])->value;
  else
    return DEFAULT_FONT_SIZE;
}

static SvgValue *
svg_number_resolve (const SvgValue    *value,
                    ShapeAttr          attr,
                    unsigned int       idx,
                    Shape             *shape,
                    SvgComputeContext *context)
{
  const SvgNumber *n = (const SvgNumber *) value;

  switch (n->unit)
    {
    case SVG_UNIT_NUMBER:
    case SVG_UNIT_PX:
      switch ((unsigned int) attr)
        {
        case SHAPE_ATTR_OPACITY:
        case SHAPE_ATTR_FILL_OPACITY:
        case SHAPE_ATTR_STROKE_OPACITY:
        case SHAPE_ATTR_STOP_OFFSET:
          return svg_number_new (CLAMP (n->value, 0, 1));
        default:
          return svg_value_ref ((SvgValue *) value);
        }
      break;
    case SVG_UNIT_PERCENTAGE:
      switch ((unsigned int) attr)
        {
        case SHAPE_ATTR_FONT_SIZE:
          {
            double parent_size;

            if (context->parent)
              parent_size = ((SvgNumber *) context->parent->current[SHAPE_ATTR_FONT_SIZE])->value;
            else
              parent_size = DEFAULT_FONT_SIZE;

            return svg_number_new (n->value * parent_size / 100);
          }
          break;
        case SHAPE_ATTR_OPACITY:
        case SHAPE_ATTR_FILL_OPACITY:
        case SHAPE_ATTR_STROKE_OPACITY:
        case SHAPE_ATTR_STOP_OFFSET:
          return svg_number_new (CLAMP (n->value / 100, 0, 1));
        case SHAPE_ATTR_FILTER:
          return svg_number_new (n->value / 100);
        case SHAPE_ATTR_STROKE_WIDTH:
        case SHAPE_ATTR_R:
          if (shape->type != SHAPE_RADIAL_GRADIENT)
            return svg_number_new_full (SVG_UNIT_PX, n->value * normalized_diagonal (context->viewport) / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_CX:
        case SHAPE_ATTR_RX:
          if (shape->type != SHAPE_RADIAL_GRADIENT)
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.width / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_CY:
        case SHAPE_ATTR_RY:
          if (shape->type != SHAPE_RADIAL_GRADIENT)
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.height / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_Y:
        case SHAPE_ATTR_HEIGHT:
          if (shape->type != SHAPE_FILTER &&
              shape->type != SHAPE_PATTERN &&
              (shape->type != SHAPE_MASK ||
               svg_enum_get (shape->current[SHAPE_ATTR_BOUND_UNITS]) != COORD_UNITS_OBJECT_BOUNDING_BOX))
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.height / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_X:
        case SHAPE_ATTR_WIDTH:
          if (shape->type != SHAPE_FILTER &&
              shape->type != SHAPE_PATTERN &&
              (shape->type != SHAPE_MASK ||
               svg_enum_get (shape->current[SHAPE_ATTR_BOUND_UNITS]) != COORD_UNITS_OBJECT_BOUNDING_BOX))
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.width / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_X1:
        case SHAPE_ATTR_X2:
          if (shape->type == SHAPE_LINE)
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.width / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        case SHAPE_ATTR_Y1:
        case SHAPE_ATTR_Y2:
          if (shape->type == SHAPE_LINE)
            return svg_number_new_full (SVG_UNIT_PX, n->value * context->viewport->size.height / 100);
          else
            return svg_value_ref ((SvgValue *) value);
        default:
          return svg_value_ref ((SvgValue *) value);
        }
      break;
    case SVG_UNIT_PT:
    case SVG_UNIT_IN:
    case SVG_UNIT_CM:
    case SVG_UNIT_MM:
      return svg_number_new_full (SVG_UNIT_PX, absolute_length_to_px (n->value, n->unit));

    case SVG_UNIT_VW:
    case SVG_UNIT_VH:
    case SVG_UNIT_VMIN:
    case SVG_UNIT_VMAX:
      return svg_number_new_full (SVG_UNIT_PX, viewport_relative_to_px (n->value,
                                                                        n->unit,
                                                                        context->viewport));

    case SVG_UNIT_EM:
      return svg_number_new_full (SVG_UNIT_PX, n->value * shape_get_current_font_size (shape, attr, context));

    case SVG_UNIT_EX:
      return svg_number_new_full (SVG_UNIT_PX, n->value * 0.5 * shape_get_current_font_size (shape, attr, context));

    case SVG_UNIT_RAD:
    case SVG_UNIT_DEG:
    case SVG_UNIT_GRAD:
    case SVG_UNIT_TURN:
      return svg_number_new_full (SVG_UNIT_DEG, angle_to_deg (n->value, n->unit));

    default:
      g_assert_not_reached ();
    }
}

static const SvgValueClass SVG_NUMBER_CLASS = {
  "SvgNumber",
  svg_value_default_free,
  svg_number_equal,
  svg_number_interpolate,
  svg_number_accumulate,
  svg_number_print,
  svg_number_distance,
  svg_number_resolve,
};

SvgValue *
svg_number_new (double value)
{
  static SvgNumber singletons[] = {
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = 0 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = 1 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = 2 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = 4 }, /* default miter limit */
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = DEFAULT_FONT_SIZE },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = PANGO_WEIGHT_NORMAL },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_NUMBER, .value = -1 }, /* unset path length */
  };
  SvgValue *result;

  for (unsigned int i = 0; i < G_N_ELEMENTS (singletons); i++)
    {
      if (value == singletons[i].value)
        return (SvgValue *) &singletons[i];
    }

  result = svg_value_alloc (&SVG_NUMBER_CLASS, sizeof (SvgNumber));
  ((SvgNumber *) result)->unit = SVG_UNIT_NUMBER;
  ((SvgNumber *) result)->value = value;

  return result;
}

SvgValue *
svg_percentage_new (double value)
{
  static SvgNumber singletons[] = {
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = -10 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 0 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 25 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 50 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 100 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 120 },
    { { &SVG_NUMBER_CLASS, 0 }, .unit = SVG_UNIT_PERCENTAGE, .value = 150 },
  };
  SvgValue *result;

  for (unsigned int i = 0; i < G_N_ELEMENTS (singletons); i++)
    {
      if (value == singletons[i].value)
        return (SvgValue *) &singletons[i];
    }

  result = svg_value_alloc (&SVG_NUMBER_CLASS, sizeof (SvgNumber));
  ((SvgNumber *) result)->unit = SVG_UNIT_PERCENTAGE;
  ((SvgNumber *) result)->value = value;

  return result;
}

SvgValue *
svg_number_new_full (SvgUnit unit,
                     double  value)
{
  if (unit == SVG_UNIT_NUMBER)
    {
      return svg_number_new (value);
    }
  else if (unit == SVG_UNIT_PERCENTAGE)
    {
      return svg_percentage_new (value);
    }
  else
    {
      SvgNumber *result;

      result = (SvgNumber *) svg_value_alloc (&SVG_NUMBER_CLASS, sizeof (SvgNumber));
      result->value = value;
      result->unit = unit;

      return (SvgValue *) result;
    }
}

gboolean
svg_number_parse2 (GtkCssParser *parser,
                   double        min,
                   double        max,
                   unsigned int  flags,
                   double       *d,
                   SvgUnit      *u)
{
  const GtkCssToken *token;
  double number;
  SvgUnit unit;

  token = gtk_css_parser_get_token (parser);

  if (gtk_css_token_is (token, GTK_CSS_TOKEN_PERCENTAGE))
    {
      if ((flags & PERCENTAGE) == 0)
        {
          gtk_css_parser_error_value (parser, "Percentages are not allowed here");
          return FALSE;
        }

      number = token->number.number;
      unit = SVG_UNIT_PERCENTAGE;
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_NUMBER) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_NUMBER))
    {
      number = token->number.number;

      if (number == 0)
        {
          if (flags & NUMBER)
            unit = SVG_UNIT_NUMBER;
          else if (flags & LENGTH)
            unit = SVG_UNIT_PX;
          else if (flags & ANGLE)
            unit = SVG_UNIT_DEG;
          else
            unit = SVG_UNIT_PERCENTAGE;
        }
      else if (flags & NUMBER)
        {
          unit = SVG_UNIT_NUMBER;
        }
      else
        {
          gtk_css_parser_error_syntax (parser, "Unit is missing");
          return FALSE;
        }
    }
  else if (gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_INTEGER_DIMENSION) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_INTEGER_DIMENSION) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNED_DIMENSION) ||
           gtk_css_token_is (token, GTK_CSS_TOKEN_SIGNLESS_DIMENSION))
    {
      guint i;

      number = token->dimension.value;

      for (i = 0; i < G_N_ELEMENTS (unit_names); i++)
        {
          if (g_ascii_strcasecmp (token->dimension.dimension, unit_names[i]) == 0)
            break;
        }

      if (FIRST_LENGTH_UNIT <= i && i <= LAST_LENGTH_UNIT)
        {
          if (flags & LENGTH)
            unit = i;
          else
            {
              gtk_css_parser_error_value (parser, "Lengths are not allowed here");
              return FALSE;
            }
        }
      else if (FIRST_ANGLE_UNIT <= i && i <= LAST_ANGLE_UNIT)
        {
          if (flags & ANGLE)
            unit = i;
          else
            {
              gtk_css_parser_error_value (parser, "Angles are not allowed here");
              return FALSE;
            }
        }
      else
        {
          gtk_css_parser_error_syntax (parser, "'%s' is not a valid unit", token->dimension.dimension);
          return FALSE;
        }
    }
  else
    {
      gtk_css_parser_error_syntax (parser, "Expected a number");
      return FALSE;
    }

  if (number < min || number > max)
    {
      gtk_css_parser_error_value (parser, "Out of range");
      return FALSE;
    }

  *d = number;
  *u = unit;

  gtk_css_parser_consume_token (parser);
  return TRUE;
}

SvgValue *
svg_number_parse (GtkCssParser *parser,
                  double        min,
                  double        max,
                  unsigned int  flags)
{
  double d;
  SvgUnit unit;

  if (svg_number_parse2 (parser, min, max, flags, &d, &unit))
    return svg_number_new_full (unit, d);
  else
    return NULL;
}

double
svg_number_get (const SvgValue *value,
                double          one_hundred_percent)
{
  const SvgNumber *n = (const SvgNumber *) value;

  g_assert (value->class == &SVG_NUMBER_CLASS);

  if (n->unit == SVG_UNIT_PERCENTAGE)
    return n->value / 100 * one_hundred_percent;
  else
    return n->value;
}

SvgUnit
svg_number_get_unit (const SvgValue *value)
{
  const SvgNumber *n = (const SvgNumber *) value;

  g_assert (value->class == &SVG_NUMBER_CLASS);

  return n->unit;
}

gboolean
svg_value_is_number (const SvgValue *value)
{
  return value->class == &SVG_NUMBER_CLASS;
}

gboolean
svg_value_is_positive_number (const SvgValue *value)
{
  const SvgNumber *n = (const SvgNumber *) value;

  return svg_value_is_number (value) && n->value >= 0;
}
