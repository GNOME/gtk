/*
 * Copyright © 2025 Red Hat, Inc
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#include "config.h"

#include "gtksvgnumbersprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgstringutilsprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgelementinternal.h"

typedef struct
{
  double value;
  SvgUnit unit;
} Number;

static inline gboolean
number_equal (const Number *n0,
              const Number *n1)
{
  return n0->unit == n1->unit && n0->value == n1->value;
}

typedef struct
{
  SvgValue base;
  unsigned int n_values;
  Number values[1];
} SvgNumbers;

static unsigned int
svg_numbers_size (unsigned int n)
{
  return sizeof (SvgNumbers) + (n > 0 ? n - 1 : 0) * sizeof (Number);
}

static gboolean
svg_numbers_equal (const SvgValue *value0,
                   const SvgValue *value1)
{
  const SvgNumbers *p0 = (const SvgNumbers *) value0;
  const SvgNumbers *p1 = (const SvgNumbers *) value1;

  if (p0->n_values != p1->n_values)
    return FALSE;

  for (unsigned int i = 0; i < p0->n_values; i++)
    {
      if (!number_equal (&p0->values[i], &p1->values[i]))
        return FALSE;
    }

  return TRUE;
}

static SvgValue *
svg_numbers_accumulate (const SvgValue    *value0,
                        const SvgValue    *value1,
                        SvgComputeContext *context,
                        int                n)
{
  return NULL;
}

static void
svg_numbers_print (const SvgValue *value,
                   GString        *string)
{
  const SvgNumbers *p = (const SvgNumbers *) value;

  for (unsigned int i = 0; i < p->n_values; i++)
    {
      if (i > 0)
        g_string_append_c (string, ' ');
      string_append_double (string, "", p->values[i].value);
      g_string_append (string, svg_unit_name (p->values[i].unit));
    }
}

static SvgValue * svg_numbers_interpolate (const SvgValue    *value0,
                                           const SvgValue    *value1,
                                           SvgComputeContext *context,
                                           double             t);

static SvgValue * svg_numbers_resolve (const SvgValue    *value,
                                       SvgProperty        attr,
                                       unsigned int       idx,
                                       SvgElement        *shape,
                                       SvgComputeContext *context);

static const SvgValueClass SVG_NUMBERS_CLASS = {
  "SvgNumbers",
  svg_value_default_free,
  svg_numbers_equal,
  svg_numbers_interpolate,
  svg_numbers_accumulate,
  svg_numbers_print,
  svg_value_default_distance,
  svg_numbers_resolve,
};

SvgValue *
svg_numbers_new (double       *values,
                 unsigned int  n_values)
{
  SvgNumbers *result;

  result = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (n_values));
  result->n_values = n_values;

  for (unsigned int i = 0; i < n_values; i++)
    {
      result->values[i].unit = SVG_UNIT_NUMBER;
      result->values[i].value = values[i];
    }

  return (SvgValue *) result;
}

SvgValue *
svg_numbers_new_full (double       *values,
                      SvgUnit      *units,
                      unsigned int  n_values)
{
  SvgNumbers *result;

  result = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (n_values));
  result->n_values = n_values;

  for (unsigned int i = 0; i < n_values; i++)
    {
      result->values[i].unit = units[i];
      result->values[i].value = values[i];
    }

  return (SvgValue *) result;
}

SvgValue *
svg_numbers_new_identity_matrix (void)
{
  static SvgValue *id;

  if (id == NULL)
    {
      id = svg_numbers_new ((double []) { 1, 0, 0, 0, 0,
                                          0, 1, 0, 0, 0,
                                          0, 0, 1, 0, 0,
                                          0, 0, 0, 1, 0 }, 20);
      id->ref_count = 0;
    }

  return id;
}

SvgValue *
svg_numbers_new_none (void)
{
  static SvgNumbers none = { { &SVG_NUMBERS_CLASS, 0 }, .n_values = 0, .values[0] = { .unit = SVG_UNIT_NUMBER, .value = 0 } };

  return (SvgValue *) &none;
}

SvgValue *
svg_numbers_new1 (double value)
{
  static SvgNumbers singletons[] = {
    { { &SVG_NUMBERS_CLASS, 0 }, .n_values = 1, .values[0] = { .unit = SVG_UNIT_NUMBER, .value = 0 } },
    { { &SVG_NUMBERS_CLASS, 0 }, .n_values = 1, .values[0] = { .unit = SVG_UNIT_NUMBER, .value = 1 } },
    { { &SVG_NUMBERS_CLASS, 0 }, .n_values = 1, .values[0] = { .unit = SVG_UNIT_NUMBER, .value = 2 } },
  };
  SvgNumbers *p;

  for (unsigned int i = 0; i < G_N_ELEMENTS (singletons); i++)
    {
      if (singletons[i].values[0].value == value)
        return (SvgValue *) &singletons[i];
    }

  p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (1));

  p->n_values = 1;
  p->values[0].unit = SVG_UNIT_NUMBER;
  p->values[0].value = value;

  return (SvgValue *) p;
}

SvgValue *
svg_numbers_new_00 (void)
{
  static SvgNumbers *value = NULL;

  if (value == NULL)
    {
      value = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (2));
      ((SvgValue *) value)->ref_count = 0;
      value->values[0].value = value->values[1].value = 0;
      value->values[0].unit = value->values[1].unit = SVG_UNIT_NUMBER;
    }

  return (SvgValue *) value;
}

SvgValue *
svg_numbers_new2 (double v1, SvgUnit u1,
                  double v2, SvgUnit u2)
{
  SvgNumbers *n;

  if (v1 == 0 && u1 == SVG_UNIT_NUMBER &&
      v2 == 0 && u2 == SVG_UNIT_NUMBER)
    return svg_numbers_new_00 ();

  n = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (2));
  n->n_values = 2;

  n->values[0].value = v1;
  n->values[0].unit = u1;
  n->values[1].value = v2;
  n->values[1].unit = u2;

  return (SvgValue *) n;
}

SvgValue *
svg_numbers_parse2 (GtkCssParser *parser,
                    unsigned int  flags)
{
  SvgNumbers *p;
  GArray *numbers;
  Number n;

  numbers = g_array_new (FALSE, FALSE, sizeof (Number));

  while (1)
    {
      gtk_css_parser_skip_whitespace (parser);
      if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        break;

      if (!svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, flags, &n.value, &n.unit))
        {
          g_array_unref (numbers);
          return NULL;
        }

      g_array_append_val (numbers, n);

      skip_whitespace_and_optional_comma (parser);
    }

  p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS,
                                      svg_numbers_size (numbers->len));
  p->n_values = numbers->len;

  for (unsigned int i = 0; i < numbers->len; i++)
    p->values[i] = g_array_index (numbers, Number, i);

  g_array_unref (numbers);

  return (SvgValue *) p;
}

SvgValue *
svg_numbers_parse (GtkCssParser *parser)
{
  return svg_numbers_parse2 (parser, SVG_PARSE_NUMBER);
}

static SvgValue *
svg_numbers_interpolate (const SvgValue    *value0,
                         const SvgValue    *value1,
                         SvgComputeContext *context,
                         double             t)
{
  const SvgNumbers *p0 = (const SvgNumbers *) value0;
  const SvgNumbers *p1 = (const SvgNumbers *) value1;
  SvgNumbers *p;

  if (p0->n_values != p1->n_values)
    {
      if (t < 0.5)
        return svg_value_ref ((SvgValue *) value0);
      else
        return svg_value_ref ((SvgValue *) value1);
    }

  p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (p0->n_values));
  p->n_values = p0->n_values;

  for (unsigned int i = 0; i < p0->n_values; i++)
    {
      g_assert (p0->values[i].unit != SVG_UNIT_PERCENTAGE);
      p->values[i].unit = SVG_UNIT_NUMBER;
      p->values[i].value = lerp (t, p0->values[i].value, p1->values[i].value);
    }

  return (SvgValue *) p;
}

static SvgValue *
svg_numbers_resolve (const SvgValue    *value,
                     SvgProperty        attr,
                     unsigned int       idx,
                     SvgElement        *shape,
                     SvgComputeContext *context)
{
  SvgNumbers *orig = (SvgNumbers *) value;
  SvgNumbers *p;
  double size;

  if (orig->n_values == 0)
    return svg_value_ref ((SvgValue *) orig);

  if (attr == SVG_PROPERTY_TRANSFORM_ORIGIN)
    {
      double font_size;

      if (value == svg_numbers_new_00 ())
        return svg_value_ref ((SvgValue *) value);

      p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (2));
      p->n_values = 2;
      memcpy (p->values, orig->values, sizeof (Number) * 2);

      font_size = shape_get_current_font_size (shape, attr, context);

      for (unsigned int i = 0; i < 2; i++)
        {
          switch ((unsigned int) p->values[i].unit)
            {
            case SVG_UNIT_NUMBER:
            case SVG_UNIT_PERCENTAGE:
              break;

            case SVG_UNIT_PX:
            case SVG_UNIT_PT:
            case SVG_UNIT_IN:
            case SVG_UNIT_CM:
            case SVG_UNIT_MM:
              p->values[i].value = absolute_length_to_px (p->values[i].value, p->values[i].unit);
              p->values[i].unit = SVG_UNIT_PX;
              break;
            case SVG_UNIT_VW:
            case SVG_UNIT_VH:
            case SVG_UNIT_VMIN:
            case SVG_UNIT_VMAX:
              p->values[i].value = viewport_relative_to_px (p->values[i].value, p->values[i].unit, context->viewport);
              p->values[i].unit = SVG_UNIT_PX;
              break;
            case SVG_UNIT_EM:
              p->values[i].value = p->values[i].value * font_size;
              p->values[i].unit = SVG_UNIT_PX;
              break;
            case SVG_UNIT_EX:
              p->values[i].value = p->values[i].value * 0.5 * font_size;
              p->values[i].unit = SVG_UNIT_PX;
              break;
            default:
              g_assert_not_reached ();
            }
        }

      return (SvgValue *) p;
    }

  p = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (orig->n_values));
  p->n_values = orig->n_values;

  size = normalized_diagonal (context->viewport);
  for (unsigned int i = 0; i < orig->n_values; i++)
    {
      if (orig->values[i].unit == SVG_UNIT_PERCENTAGE)
        {
          p->values[i].unit = SVG_UNIT_NUMBER;
          p->values[i].value = orig->values[i].value * size / 100;
        }
      else
        {
          p->values[i].value = orig->values[i].value;
        }
    }

  return (SvgValue *) p;
}

unsigned int
svg_numbers_get_length (const SvgValue *value)
{
  const SvgNumbers *n = (const SvgNumbers *) value;

  g_assert (value->class == &SVG_NUMBERS_CLASS);

  return n->n_values;
}

SvgUnit
svg_numbers_get_unit (const SvgValue *value,
                      unsigned int    pos)
{
  const SvgNumbers *n = (const SvgNumbers *) value;

  g_assert (value->class == &SVG_NUMBERS_CLASS);
  g_assert (pos < n->n_values);

  return n->values[pos].unit;
}

double
svg_numbers_get (const SvgValue *value,
                 unsigned int    pos,
                 double          one_hundred_percent)
{
  const SvgNumbers *n = (const SvgNumbers *) value;

  g_assert (value->class == &SVG_NUMBERS_CLASS);
  g_assert (pos < n->n_values);

  if (n->values[pos].unit == SVG_UNIT_PERCENTAGE)
    return n->values[pos].value / 100 * one_hundred_percent;
  else
    return n->values[pos].value;
}

SvgValue *
svg_numbers_drop_last (const SvgValue *v)
{
  const SvgNumbers *o = (const SvgNumbers *) v;
  SvgNumbers *n;

  if (v->ref_count == 1)
    {
      ((SvgNumbers *) v)->n_values--;
      return (SvgValue *) v;
    }

  n = (SvgNumbers *) svg_value_alloc (&SVG_NUMBERS_CLASS, svg_numbers_size (o->n_values - 1));
  n->n_values = o->n_values - 1;

  for (unsigned int i = 0; i < n->n_values; i++)
    n->values[i] = o->values[i];

  return (SvgValue *) n;
}
