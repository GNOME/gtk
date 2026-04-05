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

#include "gtksvgvalueprivate.h"
#include "gtksvgkeywordprivate.h"

enum
{
  SVG_INHERIT,
  SVG_INITIAL,
  SVG_CURRENT,
  SVG_AUTO,
  SVG_UNSET,
};

typedef struct
{
  SvgValue base;
  unsigned int keyword;
} SvgKeyword;

static gboolean
svg_keyword_equal (const SvgValue *value0,
                   const SvgValue *value1)
{
  const SvgKeyword *k0 = (const SvgKeyword *) value0;
  const SvgKeyword *k1 = (const SvgKeyword *) value1;
  return k0->keyword == k1->keyword;
}

static SvgValue *
svg_keyword_interpolate (const SvgValue    *value0,
                         const SvgValue    *value1,
                         SvgComputeContext *context,
                         double             t)
{
  return NULL;
}

static SvgValue *
svg_keyword_accumulate (const SvgValue    *value0,
                        const SvgValue    *value1,
                        SvgComputeContext *context,
                        int                n)
{
  return NULL;
}

static void
svg_keyword_print (const SvgValue *value,
                   GString        *string)
{
  const SvgKeyword *k = (const SvgKeyword *) value;

  switch (k->keyword)
    {
    case SVG_INHERIT:
      g_string_append (string, "inherit");
      break;
    case SVG_INITIAL:
      g_string_append (string, "initial");
      break;
    case SVG_CURRENT:
      g_string_append (string, "current");
      break;
    case SVG_AUTO:
      g_string_append (string, "auto");
      break;
    case SVG_UNSET:
      g_string_append (string, "unset");
      break;
    default:
      g_assert_not_reached ();
    }
}

static const SvgValueClass SVG_KEYWORD_CLASS = {
  "SvgKeyword",
  svg_value_default_free,
  svg_keyword_equal,
  svg_keyword_interpolate,
  svg_keyword_accumulate,
  svg_keyword_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

SvgValue *
svg_inherit_new (void)
{
  static SvgKeyword inherit = { { &SVG_KEYWORD_CLASS, 0 }, SVG_INHERIT };

  return (SvgValue *) &inherit;
}

SvgValue *
svg_initial_new (void)
{
  static SvgKeyword initial = { { &SVG_KEYWORD_CLASS, 0 }, SVG_INITIAL };

  return (SvgValue *) &initial;
}

SvgValue *
svg_current_new (void)
{
  static SvgKeyword current = { { &SVG_KEYWORD_CLASS, 0 }, SVG_CURRENT };

  return (SvgValue *) &current;
}

SvgValue *
svg_auto_new (void)
{
  static SvgKeyword auto_value = { { &SVG_KEYWORD_CLASS, 0 }, SVG_AUTO };

  return (SvgValue *) &auto_value;
}

SvgValue *
svg_unset_new (void)
{
  static SvgKeyword unset_value = { { &SVG_KEYWORD_CLASS, 0 }, SVG_UNSET };

  return (SvgValue *) &unset_value;
}

gboolean
svg_value_is_inherit (const SvgValue *value)
{
  return value->class == &SVG_KEYWORD_CLASS &&
         ((const SvgKeyword *) value)->keyword == SVG_INHERIT;
}

gboolean
svg_value_is_initial (const SvgValue *value)
{
  return value->class == &SVG_KEYWORD_CLASS &&
         ((const SvgKeyword *) value)->keyword == SVG_INITIAL;
}

gboolean
svg_value_is_current (const SvgValue *value)
{
  return value->class == &SVG_KEYWORD_CLASS &&
         ((const SvgKeyword *) value)->keyword == SVG_CURRENT;
}

gboolean
svg_value_is_auto (const SvgValue *value)
{
  return value->class == &SVG_KEYWORD_CLASS &&
         ((const SvgKeyword *) value)->keyword == SVG_AUTO;
}

gboolean
svg_value_is_unset (const SvgValue *value)
{
  return value->class == &SVG_KEYWORD_CLASS &&
         ((const SvgKeyword *) value)->keyword == SVG_UNSET;
}
