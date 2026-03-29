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

#include "gtksvgmaskprivate.h"
#include "gtksvgelementinternal.h"

typedef struct
{
  SvgValue base;
  MaskKind kind;

  char *ref;
  SvgElement *shape;
} SvgMask;

static void
svg_mask_free (SvgValue *value)
{
  const SvgMask *m = (const SvgMask *) value;

  if (m->kind == MASK_URL)
    g_free (m->ref);

  g_free (value);
}

static gboolean
svg_mask_equal (const SvgValue *value0,
                const SvgValue *value1)
{
  const SvgMask *m0 = (const SvgMask *) value0;
  const SvgMask *m1 = (const SvgMask *) value1;

  if (m0->kind != m1->kind)
    return FALSE;

  switch (m0->kind)
    {
    case MASK_NONE:
      return TRUE;
    case MASK_URL:
      return m0->shape == m1->shape;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
svg_mask_interpolate (const SvgValue    *value0,
                      const SvgValue    *value1,
                      SvgComputeContext *context,
                      double             t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value0);
  else
    return svg_value_ref ((SvgValue *) value1);
}

static SvgValue *
svg_mask_accumulate (const SvgValue    *value0,
                     const SvgValue    *value1,
                     SvgComputeContext *context,
                     int                n)
{
  return NULL;
}

static void
svg_mask_print (const SvgValue *value,
                GString        *string)
{
  const SvgMask *m = (const SvgMask *) value;

  switch (m->kind)
    {
    case MASK_NONE:
      g_string_append (string, "none");
      break;
    case MASK_URL:
      g_string_append_printf (string, "url(%s)", m->ref);
      break;
    default:
      g_assert_not_reached ();
    }
}

static const SvgValueClass SVG_MASK_CLASS = {
  "SvgMask",
  svg_mask_free,
  svg_mask_equal,
  svg_mask_interpolate,
  svg_mask_accumulate,
  svg_mask_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

SvgValue *
svg_mask_new_none (void)
{
  static SvgMask none = { { &SVG_MASK_CLASS, 0 }, MASK_NONE };
  return (SvgValue *) &none;
}

SvgValue *
svg_mask_new_url_take (const char *string)
{
  SvgMask *result;

  result = (SvgMask *) svg_value_alloc (&SVG_MASK_CLASS, sizeof (SvgMask));
  result->kind = MASK_URL;
  result->ref = (char *) string;
  result->shape = NULL;

  return (SvgValue *) result;
}

SvgValue *
svg_mask_parse (GtkCssParser *parser)
{
  if (gtk_css_parser_try_ident (parser, "none"))
    return svg_mask_new_none ();
  else
    {
      char *url;
      SvgValue *res = NULL;

      url = gtk_css_parser_consume_url (parser);
      if (url)
        res = svg_mask_new_url_take (url);

      return res;
    }
}

MaskKind
svg_mask_get_kind (const SvgValue *value)
{
  const SvgMask *mask = (const SvgMask *) value;

  g_assert (value->class == &SVG_MASK_CLASS);

  return mask->kind;
}

const char *
svg_mask_get_id (const SvgValue *value)
{
  const SvgMask *mask = (const SvgMask *) value;

  g_assert (value->class == &SVG_MASK_CLASS);
  g_assert (mask->kind == MASK_URL);

  if (mask->ref[0] == '#')
    return mask->ref + 1;
  else
    return mask->ref;
}

SvgElement *
svg_mask_get_shape (const SvgValue *value)
{
  const SvgMask *mask = (const SvgMask *) value;

  g_assert (value->class == &SVG_MASK_CLASS);
  g_assert (mask->kind == MASK_URL);

  return mask->shape;
}

void svg_mask_set_shape (SvgValue   *value,
                         SvgElement *shape)
{
  SvgMask *mask = (SvgMask *) value;

  g_assert (value->class == &SVG_MASK_CLASS);
  g_assert (mask->kind == MASK_URL);

  mask->shape = shape;
}
