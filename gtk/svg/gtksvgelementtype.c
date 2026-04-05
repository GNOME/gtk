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

#include "gtksvgelementtypeprivate.h"

typedef enum
{
  ELEMENT_IS_CONTAINER      = 1 << 0,
  ELEMENT_IS_PATH           = 1 << 1,
  ELEMENT_IS_TEXT           = 1 << 2,
  ELEMENT_IS_GRADIENT       = 1 << 3,
  ELEMENT_IS_FILTER         = 1 << 4,
  ELEMENT_IS_NEVER_RENDERED = 1 << 5,
} ElementTypeFlags;

typedef struct
{
  const char *name;
  ElementTypeFlags flags;
} ElementTypeInfo;

static ElementTypeInfo element_types[] = {
  [SVG_ELEMENT_LINE] = {
    .name = "line",
    .flags = ELEMENT_IS_PATH,
  },
  [SVG_ELEMENT_POLYLINE] = {
    .name = "polyline",
    .flags = ELEMENT_IS_PATH,
  },
  [SVG_ELEMENT_POLYGON] = {
    .name = "polygon",
    .flags = ELEMENT_IS_PATH,
  },
  [SVG_ELEMENT_RECT] = {
    .name = "rect",
    .flags = ELEMENT_IS_PATH,
  },
  [SVG_ELEMENT_CIRCLE] = {
    .name = "circle",
    .flags = ELEMENT_IS_PATH,
  },
  [SVG_ELEMENT_ELLIPSE] = {
    .name = "ellipse",
    .flags = ELEMENT_IS_PATH,
  },
  [SVG_ELEMENT_PATH] = {
    .name = "path",
    .flags = ELEMENT_IS_PATH,
  },
  [SVG_ELEMENT_GROUP] = {
    .name = "g",
    .flags = ELEMENT_IS_CONTAINER,
  },
  [SVG_ELEMENT_CLIP_PATH] = {
    .name = "clipPath",
    .flags = ELEMENT_IS_CONTAINER | ELEMENT_IS_NEVER_RENDERED,
  },
  [SVG_ELEMENT_MASK] = {
    .name = "mask",
    .flags = ELEMENT_IS_CONTAINER | ELEMENT_IS_NEVER_RENDERED,
  },
  [SVG_ELEMENT_DEFS] = {
    .name = "defs",
    .flags = ELEMENT_IS_CONTAINER | ELEMENT_IS_NEVER_RENDERED,
  },
  [SVG_ELEMENT_USE] = {
    .name = "use",
  },
  [SVG_ELEMENT_LINEAR_GRADIENT] = {
    .name = "linearGradient",
    .flags = ELEMENT_IS_GRADIENT | ELEMENT_IS_NEVER_RENDERED,
  },
  [SVG_ELEMENT_RADIAL_GRADIENT] = {
    .name = "radialGradient",
   .flags = ELEMENT_IS_GRADIENT | ELEMENT_IS_NEVER_RENDERED,
  },
  [SVG_ELEMENT_PATTERN] = {
    .name = "pattern",
    .flags = ELEMENT_IS_CONTAINER | ELEMENT_IS_NEVER_RENDERED,
  },
  [SVG_ELEMENT_MARKER] = {
    .name = "marker",
    .flags = ELEMENT_IS_CONTAINER | ELEMENT_IS_NEVER_RENDERED,
  },
  [SVG_ELEMENT_TEXT] = {
    .name = "text",
    .flags = ELEMENT_IS_CONTAINER | ELEMENT_IS_TEXT,
  },
  [SVG_ELEMENT_TSPAN] = {
    .name = "tspan",
    .flags = ELEMENT_IS_CONTAINER | ELEMENT_IS_TEXT | ELEMENT_IS_NEVER_RENDERED,
  },
  [SVG_ELEMENT_SVG] = {
    .name = "svg",
    .flags = ELEMENT_IS_CONTAINER,
  },
  [SVG_ELEMENT_IMAGE] = {
    .name = "image",
  },
  [SVG_ELEMENT_FILTER] = {
    .name = "filter",
    .flags = ELEMENT_IS_FILTER | ELEMENT_IS_NEVER_RENDERED,
  },
  [SVG_ELEMENT_SYMBOL] = {
    .name = "symbol",
    .flags = ELEMENT_IS_CONTAINER | ELEMENT_IS_NEVER_RENDERED,
   },
  [SVG_ELEMENT_SWITCH] = {
    .name = "switch",
    .flags = ELEMENT_IS_CONTAINER,
  },
  [SVG_ELEMENT_LINK] = {
    .name = "a",
    .flags = ELEMENT_IS_CONTAINER,
  },
};

gboolean
svg_element_type_is_container (SvgElementType type)
{
  return (element_types[type].flags & ELEMENT_IS_CONTAINER) != 0;
}

gboolean
svg_element_type_is_path (SvgElementType type)
{
  return (element_types[type].flags & ELEMENT_IS_PATH) != 0;
}

gboolean
svg_element_type_is_text (SvgElementType type)
{
  return (element_types[type].flags & ELEMENT_IS_TEXT) != 0;
}

gboolean
svg_element_type_is_gradient (SvgElementType type)
{
  return (element_types[type].flags & ELEMENT_IS_GRADIENT) != 0;
}

gboolean
svg_element_type_is_filter (SvgElementType type)
{
  return (element_types[type].flags & ELEMENT_IS_FILTER) != 0;
}

gboolean
svg_element_type_never_rendered (SvgElementType type)
{
  return (element_types[type].flags & ELEMENT_IS_NEVER_RENDERED) != 0;
}

gboolean
svg_element_type_is_clip_path_content (SvgElementType type)
{
  return (element_types[type].flags & (ELEMENT_IS_PATH | ELEMENT_IS_TEXT)) != 0;
}

gboolean
svg_element_type_is_graphical (SvgElementType type)
{
  return svg_element_type_is_path (type) || svg_element_type_is_text (type);
}

static unsigned int
element_type_hash (gconstpointer v)
{
  const ElementTypeInfo *t = (const ElementTypeInfo *) v;

  return g_str_hash (t->name);
}

static gboolean
element_type_equal (gconstpointer v0,
                    gconstpointer v1)
{
  const ElementTypeInfo *t0 = (const ElementTypeInfo *) v0;
  const ElementTypeInfo *t1 = (const ElementTypeInfo *) v1;

  return strcmp (t0->name, t1->name) == 0;
}

static GHashTable *element_type_lookup_table;

void
svg_element_type_init (void)
{
  element_type_lookup_table = g_hash_table_new (element_type_hash,
                                                element_type_equal);

  for (unsigned int i = 0; i < G_N_ELEMENTS (element_types); i++)
    g_hash_table_add (element_type_lookup_table, &element_types[i]);
}

gboolean
svg_element_type_lookup (const char     *name,
                         SvgElementType *type)
{
  ElementTypeInfo key;
  ElementTypeInfo *value;

  key.name = name;

  value = g_hash_table_lookup (element_type_lookup_table, &key);

  if (!value)
    return FALSE;

  *type = value - element_types;
  return TRUE;
}

const char *
svg_element_type_get_name (SvgElementType type)
{
  return element_types[type].name;
}
