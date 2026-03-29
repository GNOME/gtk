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

#include "gtksvgfiltertypeprivate.h"

/* We flatten the tree between feMerge/feMergeNode and
 * feComponentTransfer/feFuncR... to avoid problems
 * with attribute addressing. The only places where
 * this matters are apply_filter_tree and serialize_filter.
 */

static SvgProperty flood_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_COLOR, SVG_PROPERTY_FE_OPACITY,
};

static SvgProperty blur_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_IN, SVG_PROPERTY_FE_STD_DEV, SVG_PROPERTY_FE_BLUR_EDGE_MODE,
};

static SvgProperty blend_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_IN, SVG_PROPERTY_FE_IN2, SVG_PROPERTY_FE_BLEND_MODE,
  SVG_PROPERTY_FE_BLEND_COMPOSITE,
};

static SvgProperty color_matrix_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_IN, SVG_PROPERTY_FE_COLOR_MATRIX_TYPE, SVG_PROPERTY_FE_COLOR_MATRIX_VALUES,
};

static SvgProperty composite_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_IN, SVG_PROPERTY_FE_IN2, SVG_PROPERTY_FE_COMPOSITE_OPERATOR,
  SVG_PROPERTY_FE_COMPOSITE_K1, SVG_PROPERTY_FE_COMPOSITE_K2, SVG_PROPERTY_FE_COMPOSITE_K3, SVG_PROPERTY_FE_COMPOSITE_K4,
};

static SvgProperty offset_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_IN, SVG_PROPERTY_FE_DX, SVG_PROPERTY_FE_DY,
};

static SvgProperty displacement_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_IN, SVG_PROPERTY_FE_IN2, SVG_PROPERTY_FE_DISPLACEMENT_SCALE, SVG_PROPERTY_FE_DISPLACEMENT_X, SVG_PROPERTY_FE_DISPLACEMENT_Y,
};

static SvgProperty tile_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_IN,
};

static SvgProperty image_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_IMAGE_HREF, SVG_PROPERTY_FE_IMAGE_CONTENT_FIT,
};

static SvgProperty merge_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
};

static SvgProperty merge_node_attrs[] = {
  SVG_PROPERTY_FE_IN,
};

static SvgProperty component_transfer_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_IN,
};

static SvgProperty func_attrs[] = {
  SVG_PROPERTY_FE_FUNC_TYPE, SVG_PROPERTY_FE_FUNC_VALUES, SVG_PROPERTY_FE_FUNC_SLOPE,
  SVG_PROPERTY_FE_FUNC_INTERCEPT, SVG_PROPERTY_FE_FUNC_AMPLITUDE, SVG_PROPERTY_FE_FUNC_EXPONENT,
  SVG_PROPERTY_FE_FUNC_OFFSET,
};

static SvgProperty dropshadow_attrs[] = {
  SVG_PROPERTY_FE_X, SVG_PROPERTY_FE_Y, SVG_PROPERTY_FE_WIDTH, SVG_PROPERTY_FE_HEIGHT, SVG_PROPERTY_FE_RESULT,
  SVG_PROPERTY_FE_IN, SVG_PROPERTY_FE_STD_DEV, SVG_PROPERTY_FE_DX, SVG_PROPERTY_FE_DY,
  SVG_PROPERTY_FE_COLOR, SVG_PROPERTY_FE_OPACITY,
};

typedef struct
{
  const char *name;
  unsigned int n_attrs;
  SvgProperty *attrs;
} FilterTypeInfo;

static FilterTypeInfo filter_types[] = {
  [SVG_FILTER_FLOOD] = {
    .name = "feFlood",
    .n_attrs = G_N_ELEMENTS (flood_attrs),
    .attrs = flood_attrs,
  },
  [SVG_FILTER_BLUR] = {
    .name = "feGaussianBlur",
    .n_attrs = G_N_ELEMENTS (blur_attrs),
    .attrs = blur_attrs,
  },
  [SVG_FILTER_BLEND] = {
    .name = "feBlend",
    .n_attrs = G_N_ELEMENTS (blend_attrs),
    .attrs = blend_attrs,
  },
  [SVG_FILTER_COLOR_MATRIX] = {
    .name = "feColorMatrix",
    .n_attrs = G_N_ELEMENTS (color_matrix_attrs),
    .attrs = color_matrix_attrs,
  },
  [SVG_FILTER_COMPOSITE] = {
    .name = "feComposite",
    .n_attrs = G_N_ELEMENTS (composite_attrs),
    .attrs = composite_attrs,
  },
  [SVG_FILTER_OFFSET] = {
    .name = "feOffset",
    .n_attrs = G_N_ELEMENTS (offset_attrs),
    .attrs = offset_attrs,
  },
  [SVG_FILTER_DISPLACEMENT] = {
    .name = "feDisplacementMap",
    .n_attrs = G_N_ELEMENTS (displacement_attrs),
    .attrs = displacement_attrs,
  },
  [SVG_FILTER_TILE] = {
    .name = "feTile",
    .n_attrs = G_N_ELEMENTS (tile_attrs),
    .attrs = tile_attrs,
  },
  [SVG_FILTER_IMAGE] = {
    .name = "feImage",
    .n_attrs = G_N_ELEMENTS (image_attrs),
    .attrs = image_attrs,
  },
  [SVG_FILTER_MERGE] = {
    .name = "feMerge",
    .n_attrs = G_N_ELEMENTS (merge_attrs),
    .attrs = merge_attrs,
  },
  [SVG_FILTER_MERGE_NODE] = {
    .name = "feMergeNode",
    .n_attrs = G_N_ELEMENTS (merge_node_attrs),
    .attrs = merge_node_attrs,
  },
  [SVG_FILTER_COMPONENT_TRANSFER] = {
    .name = "feComponentTransfer",
    .n_attrs = G_N_ELEMENTS (component_transfer_attrs),
    .attrs = component_transfer_attrs,
  },
  [SVG_FILTER_FUNC_R] = {
    .name = "feFuncR",
    .n_attrs = G_N_ELEMENTS (func_attrs),
    .attrs = func_attrs,
  },
  [SVG_FILTER_FUNC_G] = {
    .name = "feFuncG",
    .n_attrs = G_N_ELEMENTS (func_attrs),
    .attrs = func_attrs,
  },
  [SVG_FILTER_FUNC_B] = {
    .name = "feFuncB",
    .n_attrs = G_N_ELEMENTS (func_attrs),
    .attrs = func_attrs,
  },
  [SVG_FILTER_FUNC_A] = {
    .name = "feFuncA",
    .n_attrs = G_N_ELEMENTS (func_attrs),
    .attrs = func_attrs,
  },
  [SVG_FILTER_DROPSHADOW] = {
    .name = "feDropShadow",
    .n_attrs = G_N_ELEMENTS (dropshadow_attrs),
    .attrs = dropshadow_attrs,
  },
};

static unsigned int
filter_type_hash (gconstpointer v)
{
  const FilterTypeInfo *t = (const FilterTypeInfo *) v;

  return g_str_hash (t->name);
}

static gboolean
filter_type_equal (gconstpointer v0,
                   gconstpointer v1)
{
  const FilterTypeInfo *t0 = (const FilterTypeInfo *) v0;
  const FilterTypeInfo *t1 = (const FilterTypeInfo *) v1;

  return strcmp (t0->name, t1->name) == 0;
}

static GHashTable *filter_type_lookup_table;

void
svg_filter_type_init (void)
{
  filter_type_lookup_table = g_hash_table_new (filter_type_hash,
                                               filter_type_equal);

  for (unsigned int i = 0; i < G_N_ELEMENTS (filter_types); i++)
    g_hash_table_add (filter_type_lookup_table, &filter_types[i]);
}

gboolean
svg_filter_type_lookup (const char    *name,
                        SvgFilterType *result)
{
  FilterTypeInfo key;
  FilterTypeInfo *value;

  key.name = name;
  value = g_hash_table_lookup (filter_type_lookup_table, &key);
  if (!value)
    return FALSE;

  *result = value - filter_types;
  return TRUE;
}

const char *
svg_filter_type_get_name (SvgFilterType type)
{
  return filter_types[type].name;
}

unsigned int
svg_filter_type_get_n_attrs (SvgFilterType type)
{
  return filter_types[type].n_attrs;
}

SvgProperty
svg_filter_type_get_property (SvgFilterType type,
                              unsigned int  idx)
{
  return filter_types[type].attrs[idx];
}

gboolean
svg_filter_type_has_property (SvgFilterType type,
                              SvgProperty   attr)
{
  for (int i = 0; i < filter_types[type].n_attrs; i++)
    {
      if (attr == filter_types[type].attrs[i])
        return TRUE;
    }

  return FALSE;
}

unsigned int
svg_filter_type_get_index (SvgFilterType type,
                           SvgProperty   attr)
{
  for (int i = 0; i < filter_types[type].n_attrs; i++)
    {
      if (attr == filter_types[type].attrs[i])
        return i;
    }

  g_assert_not_reached ();
}
