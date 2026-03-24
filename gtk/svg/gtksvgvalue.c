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

SvgValue *
svg_value_alloc (const SvgValueClass *class,
                 size_t                size)
{
  SvgValue *value;

  value = g_malloc0 (size);

  value->class = class;
  value->ref_count = 1;

  return value;
}

gboolean
svg_value_is_immortal (const SvgValue *value)
{
  return value->ref_count == 0;
}

SvgValue *
svg_value_ref (SvgValue *value)
{
  if (svg_value_is_immortal (value))
    return value;

  value->ref_count += 1;
  return value;
}

void
svg_value_unref (SvgValue *value)
{
  if (svg_value_is_immortal (value))
    return;

  if (value->ref_count > 1)
    {
      value->ref_count -= 1;
      return;
    }

  value->class->free (value);
}

gboolean
svg_value_equal (const SvgValue *value0,
                 const SvgValue *value1)
{
  if (value0 == value1)
    return TRUE;

  if (value0->class != value1->class)
    return FALSE;

  return value0->class->equal (value0, value1);
}

void
svg_value_print (const SvgValue *value,
                 GString        *string)
{
  value->class->print (value, string);
}

char *
svg_value_to_string (const SvgValue *value)
{
  GString *s = g_string_new ("");
  svg_value_print (value, s);
  return g_string_free (s, FALSE);
}

G_DEFINE_BOXED_TYPE (SvgValue, svg_value, svg_value_ref, svg_value_unref)

/* Compute v = t * a + (1 - t) * b */
SvgValue *
svg_value_interpolate (const SvgValue    *value0,
                       const SvgValue    *value1,
                       SvgComputeContext *context,
                       double             t)
{
  g_return_val_if_fail (value0->class == value1->class, svg_value_ref ((SvgValue *) value0));

  if (t == 0)
    return svg_value_ref ((SvgValue *) value0);

  if (t == 1)
    return svg_value_ref ((SvgValue *) value1);

  if (value0 == value1)
    return svg_value_ref ((SvgValue *) value0);

  return value1->class->interpolate (value0, value1, context, t);
}

/* Compute v = n * b + a */
SvgValue *
svg_value_accumulate (const SvgValue    *value0,
                      const SvgValue    *value1,
                      SvgComputeContext *context,
                      int                n)
{
  g_return_val_if_fail (value0->class == value1->class, svg_value_ref ((SvgValue *) value0));

  if (n == 0)
    return svg_value_ref ((SvgValue *) value0);

  return value0->class->accumulate (value0, value1, context, n);
}

void
svg_value_default_free (SvgValue *value)
{
  g_free (value);
}

double
svg_value_default_distance (const SvgValue *value0,
                            const SvgValue *value1)
{
  g_warning ("Can't determine distance between %s values",
             value0->class->name);
  return 1;
}

double
svg_value_distance (const SvgValue *value0,
                    const SvgValue *value1)
{
  g_return_val_if_fail (value0->class == value1->class, 1);

  return value0->class->distance (value0, value1);
}

SvgValue *
svg_value_default_resolve (const SvgValue    *value,
                           ShapeAttr          attr,
                           unsigned int       idx,
                           Shape             *shape,
                           SvgComputeContext *context)
{
  return svg_value_ref ((SvgValue *) value);
}

SvgValue *
svg_value_resolve (const SvgValue    *value,
                   ShapeAttr          attr,
                   unsigned int       idx,
                   Shape             *shape,
                   SvgComputeContext *context)
{
  return value->class->resolve (value, attr, idx, shape, context);
}
