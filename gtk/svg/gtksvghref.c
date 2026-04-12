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

#include "gtksvghrefprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgelementinternal.h"

/* The difference between HREF_PLAIN and HREF_URL is about
 * the accepted syntax: plain fragment vs url(). When
 * serializing, HREF_URL adds back the url() wrapper
 */

typedef struct
{
  SvgValue base;
  HrefKind kind;

  char *ref;
  SvgElement *shape;
  GdkTexture *texture;
} SvgHref;

static void
svg_href_free (SvgValue *value)
{
  const SvgHref *r = (const SvgHref *) value;

  if (r->kind != HREF_NONE)
    g_free (r->ref);

  if (r->texture)
    g_object_unref (r->texture);

  g_free (value);
}

static gboolean
svg_href_equal (const SvgValue *value1,
                const SvgValue *value2)
{
  const SvgHref *r1 = (const SvgHref *) value1;
  const SvgHref *r2 = (const SvgHref *) value2;

  if (r1->kind != r2->kind)
    return FALSE;

  switch (r1->kind)
    {
    case HREF_NONE:
      return TRUE;
    case HREF_PLAIN:
    case HREF_URL:
      return r1->shape == r2->shape &&
             r1->texture == r2->texture &&
             g_strcmp0 (r1->ref, r2->ref) == 0;
    default:
      g_assert_not_reached ();
    }
}

static SvgValue *
svg_href_interpolate (const SvgValue    *value1,
                      const SvgValue    *value2,
                      SvgComputeContext *context,
                      double             t)
{
  if (t < 0.5)
    return svg_value_ref ((SvgValue *) value1);
  else
    return svg_value_ref ((SvgValue *) value2);
}

static SvgValue *
svg_href_accumulate (const SvgValue    *value1,
                     const SvgValue    *value2,
                     SvgComputeContext *context,
                     int                n)
{
  return NULL;
}

static void
svg_href_print (const SvgValue *value,
                GString        *string)
{
  const SvgHref *r = (const SvgHref *) value;

  switch (r->kind)
    {
    case HREF_NONE:
      g_string_append (string, "none");
      break;
    case HREF_PLAIN:
      g_string_append_printf (string, "%s", r->ref);
      break;
    case HREF_URL:
      g_string_append_printf (string, "url(%s)", r->ref);
      break;
    default:
      g_assert_not_reached ();
    }
}

static const SvgValueClass SVG_HREF_CLASS = {
  "SvgHref",
  svg_href_free,
  svg_href_equal,
  svg_href_interpolate,
  svg_href_accumulate,
  svg_href_print,
  svg_value_default_distance,
  svg_value_default_resolve,
};

SvgValue *
svg_href_new_none (void)
{
  static SvgHref none = { { &SVG_HREF_CLASS, 0 }, HREF_NONE };
  return (SvgValue *) &none;
}

SvgValue *
svg_href_new_plain (const char *string)
{
  SvgHref *result;

  result = (SvgHref *) svg_value_alloc (&SVG_HREF_CLASS, sizeof (SvgHref));
  result->kind = HREF_PLAIN;
  result->ref = g_strdup (string);
  result->shape = NULL;

  return (SvgValue *) result;
}

SvgValue *
svg_href_new_url_take (const char *string)
{
  SvgHref *result;

  result = (SvgHref *) svg_value_alloc (&SVG_HREF_CLASS, sizeof (SvgHref));
  result->kind = HREF_URL;
  result->ref = (char *) string;
  result->shape = NULL;

  return (SvgValue *) result;
}

const char *
svg_href_get_id (const SvgValue *value)
{
  const SvgHref *href = (const SvgHref *) value;

  g_assert (value->class == &SVG_HREF_CLASS);

  if (href->kind == HREF_NONE)
    return NULL;

  if (href->ref[0] == '#')
    return href->ref + 1;
  else
    return href->ref;
}

HrefKind
svg_href_get_kind (const SvgValue *value)
{
  const SvgHref *href = (const SvgHref *) value;

  g_assert (value->class == &SVG_HREF_CLASS);

  return href->kind;
}

SvgElement *
svg_href_get_shape (const SvgValue *value)
{
  const SvgHref *href = (const SvgHref *) value;

  g_assert (value->class == &SVG_HREF_CLASS);

  return href->shape;
}

void
svg_href_set_shape (SvgValue   *value,
                    SvgElement *shape)
{
  SvgHref *href = (SvgHref *) value;

  g_assert (value->class == &SVG_HREF_CLASS);

  href->shape = shape;
}

GdkTexture *
svg_href_get_texture (const SvgValue *value)
{
  const SvgHref *href = (const SvgHref *) value;

  g_assert (value->class == &SVG_HREF_CLASS);

  return href->texture;
}

void
svg_href_set_texture (SvgValue   *value,
                      GdkTexture *texture)
{
  SvgHref *href = (SvgHref *) value;

  g_assert (value->class == &SVG_HREF_CLASS);

  href->texture = texture;
}
