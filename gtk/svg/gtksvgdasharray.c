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

#include "gtksvgdasharrayprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgstringutilsprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgelementinternal.h"

typedef struct
{
  double value;
  SvgUnit unit;
} Dash;

typedef struct
{
  SvgValue base;
  DashArrayKind kind;
  unsigned int n_dashes;
  Dash dashes[2];
} SvgDashArray;

static size_t
svg_dash_array_size (size_t n)
{
  return sizeof (SvgDashArray) + (n > 1 ? n - 2 : 0) * sizeof (Dash);
}

static gboolean
svg_dash_array_equal (const SvgValue *value0,
                      const SvgValue *value1)
{
  const SvgDashArray *da0 = (const SvgDashArray *) value0;
  const SvgDashArray *da1 = (const SvgDashArray *) value1;

  if (da0->kind != da1->kind)
    return FALSE;

  if (da0->kind == DASH_ARRAY_NONE)
    return TRUE;

  if (da0->n_dashes != da1->n_dashes)
    return FALSE;

  for (unsigned int i = 0; i < da0->n_dashes; i++)
    {
      if (da0->dashes[i].unit != da1->dashes[i].unit)
        return FALSE;
      if (da0->dashes[i].value != da1->dashes[i].value)
        return FALSE;
    }

  return TRUE;
}

static SvgValue * svg_dash_array_interpolate (const SvgValue    *value0,
                                              const SvgValue    *value1,
                                              SvgComputeContext *context,
                                              double             t);

static void svg_dash_array_print (const SvgValue *value,
                                  GString        *s);

static SvgValue * svg_dash_array_accumulate (const SvgValue    *value0,
                                             const SvgValue    *value1,
                                             SvgComputeContext *context,
                                             int                n);
static SvgValue * svg_dash_array_resolve    (const SvgValue    *value,
                                             SvgProperty        attr,
                                             unsigned int       idx,
                                             SvgElement        *shape,
                                             SvgComputeContext *context);

static const SvgValueClass SVG_DASH_ARRAY_CLASS = {
  "SvgFilter",
  svg_value_default_free,
  svg_dash_array_equal,
  svg_dash_array_interpolate,
  svg_dash_array_accumulate,
  svg_dash_array_print,
  svg_value_default_distance,
  svg_dash_array_resolve,
};

static SvgDashArray *
svg_dash_array_alloc (unsigned int n)
{
  SvgDashArray *a;

  a = (SvgDashArray *) svg_value_alloc (&SVG_DASH_ARRAY_CLASS, svg_dash_array_size (n));
  a->n_dashes = n;

  return a;
}

SvgValue *
svg_dash_array_new_none (void)
{
  static SvgDashArray none = { { &SVG_DASH_ARRAY_CLASS, 0 }, DASH_ARRAY_NONE, 0 };
  return (SvgValue *) &none;
}

SvgValue *
svg_dash_array_new (double       *values,
                    unsigned int  n)
{
  static SvgDashArray da01 = { { &SVG_DASH_ARRAY_CLASS, 0 }, DASH_ARRAY_DASHES, 2, { { .unit = SVG_UNIT_NUMBER, .value = 0 }, { .unit = SVG_UNIT_NUMBER, .value = 1 } } };
  static SvgDashArray da10 = { { &SVG_DASH_ARRAY_CLASS, 0 }, DASH_ARRAY_DASHES, 2, { { .unit = SVG_UNIT_NUMBER, .value = 1 }, { .unit = SVG_UNIT_NUMBER, .value = 0 } } };

  if (n == 2 && values[0] == 0 && values[1] == 1)
    {
      return (SvgValue *) &da01;
    }
  else if (n == 2 && values[0] == 1 && values[1] == 0)
    {
      return (SvgValue *) &da10;
    }
  else
    {
      SvgDashArray *a = svg_dash_array_alloc (n);

      a->kind = DASH_ARRAY_DASHES;
      for (unsigned int i = 0; i < n; i++)
        {
          a->dashes[i].unit = SVG_UNIT_NUMBER;
          a->dashes[i].value = values[i];
        }

      return (SvgValue *) a;
    }
}

SvgValue *
svg_dash_array_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    {
      return svg_dash_array_new_none ();
    }
  else
    {
      GArray *array;
      SvgDashArray *dashes;

      array = g_array_new (FALSE, FALSE, sizeof (Dash));

      while (!gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_EOF))
        {
          Dash n;

          gtk_css_parser_skip_whitespace (parser);

          if (!svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_PERCENTAGE|SVG_PARSE_LENGTH,
                                  &n.value, &n.unit))
            {
              g_array_unref (array);
              return NULL;
            }

          gtk_css_parser_skip_whitespace (parser);

          if (gtk_css_parser_has_token (parser, GTK_CSS_TOKEN_COMMA))
            gtk_css_parser_skip (parser);

          g_array_append_val (array, n);
        }

      dashes = svg_dash_array_alloc (array->len);
      dashes->kind = DASH_ARRAY_DASHES;

      for (unsigned int i = 0; i < array->len; i++)
        {
          Dash *n = &g_array_index (array, Dash, i);
          dashes->dashes[i].unit = n->unit;
          dashes->dashes[i].value = n->value;
        }

      g_array_unref (array);

      return (SvgValue *) dashes;
    }
}

static void
svg_dash_array_print (const SvgValue *value,
                      GString        *s)
{
  const SvgDashArray *dashes = (const SvgDashArray *) value;

  if (dashes->kind == DASH_ARRAY_NONE)
    {
      g_string_append (s, "none");
    }
  else
    {
      for (unsigned int i = 0; i < dashes->n_dashes; i++)
        {
          if (i > 0)
            g_string_append_c (s, ' ');
          string_append_double (s, "", dashes->dashes[i].value);
          g_string_append (s, svg_unit_name (dashes->dashes[i].unit));
        }
    }
}

static SvgValue *
svg_dash_array_interpolate (const SvgValue    *value0,
                            const SvgValue    *value1,
                            SvgComputeContext *context,
                            double             t)
{
  const SvgDashArray *a0 = (const SvgDashArray *) value0;
  const SvgDashArray *a1 = (const SvgDashArray *) value1;
  SvgDashArray *a = NULL;
  unsigned int n_dashes;

  if (a0->kind != a1->kind)
    goto out;

  if (a0->kind == DASH_ARRAY_NONE)
    return (SvgValue *) svg_dash_array_new_none ();

  n_dashes = lcm (a0->n_dashes, a1->n_dashes);

  a = svg_dash_array_alloc (n_dashes);
  a->kind = a0->kind;

  for (unsigned int i = 0; i < n_dashes; i++)
    {
      if (a0->dashes[i % a0->n_dashes].unit != a1->dashes[i %a1->n_dashes].unit)
        {
          g_clear_pointer ((SvgValue **)&a, svg_value_unref);
          break;
        }

      a->dashes[i].unit = a0->dashes[i % a0->n_dashes].unit;
      a->dashes[i].value = lerp (t, a0->dashes[i % a0->n_dashes].value,
                                    a1->dashes[i % a1->n_dashes].value);
    }

out:
  if (a)
    return (SvgValue *) a;

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_dash_array_accumulate (const SvgValue    *value0,
                           const SvgValue    *value1,
                           SvgComputeContext *context,
                           int                n)
{
  return NULL;
}

static SvgValue *
svg_dash_array_resolve (const SvgValue    *value,
                        SvgProperty        attr,
                        unsigned int       idx,
                        SvgElement        *shape,
                        SvgComputeContext *context)
{
  SvgDashArray *orig = (SvgDashArray *) value;
  SvgDashArray *a;
  double size;

  g_assert (value->class == &SVG_DASH_ARRAY_CLASS);

  if (orig->kind == DASH_ARRAY_NONE)
    return svg_value_ref ((SvgValue *) orig);

  a = svg_dash_array_alloc (orig->n_dashes);
  a->kind = orig->kind;

  size = normalized_diagonal (context->viewport);
  for (unsigned int i = 0; i < orig->n_dashes; i++)
    {
      a->dashes[i].unit = SVG_UNIT_NUMBER;
      switch ((unsigned int) orig->dashes[i].unit)
        {
        case SVG_UNIT_NUMBER:
        case SVG_UNIT_PX:
          a->dashes[i].value = orig->dashes[i].value;
          break;
        case SVG_UNIT_PERCENTAGE:
          a->dashes[i].value = orig->dashes[i].value / 100 * size;
          break;
        case SVG_UNIT_PT:
        case SVG_UNIT_IN:
        case SVG_UNIT_CM:
        case SVG_UNIT_MM:
          a->dashes[i].value = absolute_length_to_px (orig->dashes[i].value, orig->dashes[i].unit);
          break;
        case SVG_UNIT_VW:
        case SVG_UNIT_VH:
        case SVG_UNIT_VMIN:
        case SVG_UNIT_VMAX:
          a->dashes[i].value = viewport_relative_to_px (orig->dashes[i].value, orig->dashes[i].unit, context->viewport);
          break;
        case SVG_UNIT_EM:
          a->dashes[i].value = orig->dashes[i].value * shape_get_current_font_size (shape, attr, context);
          break;
        case SVG_UNIT_EX:
          a->dashes[i].value = orig->dashes[i].value * 0.5 * shape_get_current_font_size (shape, attr, context);
          break;
        default:
          g_assert_not_reached ();
        }
    }

  return (SvgValue *) a;
}

DashArrayKind
svg_dash_array_get_kind (const SvgValue *value)
{
  const SvgDashArray *da = (const SvgDashArray *) value;

  g_assert (value->class == &SVG_DASH_ARRAY_CLASS);

  return da->kind;
}

SvgUnit
svg_dash_array_get_unit (const SvgValue *value,
                         unsigned int    pos)
{
  const SvgDashArray *da = (const SvgDashArray *) value;

  g_assert (value->class == &SVG_DASH_ARRAY_CLASS);
  g_assert (pos < da->n_dashes);

  return da->dashes[pos].unit;
}

double
svg_dash_array_get (const SvgValue *value,
                    unsigned int    pos,
                    double          one_hundred_percent)
{
  const SvgDashArray *da = (const SvgDashArray *) value;

  g_assert (value->class == &SVG_DASH_ARRAY_CLASS);
  g_assert (pos < da->n_dashes);

  if (da->dashes[pos].unit == SVG_UNIT_PERCENTAGE)
    return da->dashes[pos].value * one_hundred_percent / 100;
  else
    return da->dashes[pos].value;
}

unsigned int
svg_dash_array_get_length (const SvgValue *value)
{
  const SvgDashArray *da = (const SvgDashArray *) value;

  g_assert (value->class == &SVG_DASH_ARRAY_CLASS);

  return da->n_dashes;
}
