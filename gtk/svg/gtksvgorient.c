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

#include "gtksvgorientprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgstringutilsprivate.h"
#include "gtksvgnumberprivate.h"

typedef struct
{
  SvgValue base;
  OrientKind kind;
  gboolean start_reverse;
  double angle;
  SvgUnit unit;
} SvgOrient;

static gboolean
svg_orient_equal (const SvgValue *value0,
                  const SvgValue *value1)
{
  const SvgOrient *v0 = (const SvgOrient *) value0;
  const SvgOrient *v1 = (const SvgOrient *) value1;

  if (v0->kind != v1->kind)
    return FALSE;

  if (v0->kind == ORIENT_AUTO)
    return v0->start_reverse == v1->start_reverse;
  else
    return v0->angle == v1->angle &&
           v0->unit == v1->unit;
}

static SvgValue *
svg_orient_interpolate (const SvgValue    *value0,
                        const SvgValue    *value1,
                        SvgComputeContext *context,
                        double             t)
{
  const SvgOrient *v0 = (const SvgOrient *) value0;
  const SvgOrient *v1 = (const SvgOrient *) value1;

  if (v0->kind == v1->kind &&
      v0->kind == ORIENT_ANGLE &&
      v0->unit == v1->unit)
    return svg_orient_new_angle (lerp (v0->angle, v1->angle, t), v0->unit);

  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_orient_accumulate (const SvgValue    *value0,
                       const SvgValue    *value1,
                       SvgComputeContext *context,
                       int                n)
{
  return NULL;
}

static void
svg_orient_print (const SvgValue *value,
                  GString        *string)
{
  const SvgOrient *v = (const SvgOrient *) value;

  if (v->kind == ORIENT_ANGLE)
    {
      string_append_double (string, "", v->angle);
      g_string_append (string, svg_unit_name (v->unit));
    }
  else if (v->start_reverse)
    {
      g_string_append (string, "auto-start-reverse");
    }
  else
    {
      g_string_append (string, "auto");
    }
}

static SvgValue * svg_orient_resolve (const SvgValue    *value,
                                      ShapeAttr          attr,
                                      unsigned int       idx,
                                      Shape             *shape,
                                      SvgComputeContext *context);

static const SvgValueClass SVG_ORIENT_CLASS = {
  "SvgOrient",
  svg_value_default_free,
  svg_orient_equal,
  svg_orient_interpolate,
  svg_orient_accumulate,
  svg_orient_print,
  svg_value_default_distance,
  svg_orient_resolve,
};

SvgValue *
svg_orient_new_angle (double  angle,
                      SvgUnit unit)
{
  static SvgOrient def = { { &SVG_ORIENT_CLASS, 0 }, .kind = ORIENT_ANGLE, .angle = 0, .unit = SVG_UNIT_NUMBER };
  SvgOrient *v;

  if (angle == 0 && unit == SVG_UNIT_NUMBER)
    return (SvgValue *) &def;

  v = (SvgOrient *) svg_value_alloc (&SVG_ORIENT_CLASS, sizeof (SvgOrient));

  v->kind = ORIENT_ANGLE;
  v->angle = angle;
  v->unit = unit;

  return (SvgValue *) v;
}

SvgValue *
svg_orient_new_auto (gboolean start_reverse)
{
  SvgOrient *v;

  v = (SvgOrient *) svg_value_alloc (&SVG_ORIENT_CLASS, sizeof (SvgOrient));

  v->kind = ORIENT_AUTO;
  v->start_reverse = start_reverse;

  return (SvgValue *) v;
}

SvgValue *
svg_orient_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "auto"))
    return svg_orient_new_auto (FALSE);
  else if (gtk_css_parser_try_ident (parser, "auto-start-reverse"))
    return svg_orient_new_auto (TRUE);
  else
    {
      double f;
      SvgUnit unit;

      if (!svg_number_parse2 (parser, -DBL_MAX, DBL_MAX, SVG_PARSE_NUMBER|SVG_PARSE_ANGLE, &f, &unit))
        return NULL;

      return svg_orient_new_angle (f, unit);
    }
}

static SvgValue *
svg_orient_resolve (const SvgValue     *value,
                    ShapeAttr           attr,
                    unsigned int        idx,
                    Shape              *shape,
                    SvgComputeContext  *context)
{
  const SvgOrient *v = (const SvgOrient *) value;

  if (v->kind == ORIENT_ANGLE)
    {
      switch ((unsigned int) v->unit)
        {
        case SVG_UNIT_NUMBER:
          return svg_value_ref ((SvgValue *) value);
        case SVG_UNIT_RAD:
          return svg_orient_new_angle (v->angle * 180.0 / M_PI, SVG_UNIT_NUMBER);
        case SVG_UNIT_DEG:
          return svg_orient_new_angle (v->angle, SVG_UNIT_NUMBER);
        case SVG_UNIT_GRAD:
          return svg_orient_new_angle (v->angle * 360.0 / 400.0, SVG_UNIT_NUMBER);
        case SVG_UNIT_TURN:
          return svg_orient_new_angle (v->angle * 360.0, SVG_UNIT_NUMBER);
        default:
          g_assert_not_reached ();
        }
    }
  else
    return svg_value_ref ((SvgValue *) value);
}

OrientKind
svg_orient_get_kind (const SvgValue *value)
{
  const SvgOrient *v = (const SvgOrient *) value;

  g_assert (value->class == &SVG_ORIENT_CLASS);

  return v->kind;
}

gboolean
svg_orient_get_start_reverse (const SvgValue *value)
{
  const SvgOrient *v = (const SvgOrient *) value;

  g_assert (value->class == &SVG_ORIENT_CLASS);
  g_assert (v->kind == ORIENT_AUTO);

  return v->start_reverse;
}

double
svg_orient_get_angle (const SvgValue *value)
{
  const SvgOrient *v = (const SvgOrient *) value;

  g_assert (value->class == &SVG_ORIENT_CLASS);
  g_assert (v->kind == ORIENT_ANGLE);

  return v->angle;
}
