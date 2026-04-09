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

#include "gtksvgfilterprivate.h"

#include "gtksvgvalueprivate.h"
#include "gtksvgenumprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgnumbersprivate.h"
#include "gtksvgpropertyprivate.h"
#include "gtksvgutilsprivate.h"
#include "gtksvgprivate.h"
#include "gtksvgfilterrefprivate.h"
#include "gtksvgfiltertypeprivate.h"
#include "gtksvgelementinternal.h"


struct _SvgFilter
{
  SvgFilterType type;
  unsigned int attrs;
  char *id;
  char *style;
  char **classes;
  size_t lines;
  GtkCssNode *css_node;
  GArray *inline_styles;
  GArray *specified;
  SvgValue **current;
  SvgValue *base[1];
};

void
svg_filter_free (SvgFilter *filter)
{
  g_free (filter->id);
  g_free (filter->style);
  g_strfreev (filter->classes);
  g_object_unref (filter->css_node);
  g_array_unref (filter->specified);
  g_array_unref (filter->inline_styles);

  for (unsigned int i = 0; i < svg_filter_type_get_n_attrs (filter->type); i++)
    {
      g_clear_pointer (&filter->base[i], svg_value_unref);
      g_clear_pointer (&filter->current[i], svg_value_unref);
    }
  g_free (filter);
}

SvgValue *
svg_filter_ref_initial_value (SvgFilter   *filter,
                              SvgProperty  attr)
{
  if (filter->type == SVG_FILTER_COLOR_MATRIX && attr == SVG_PROPERTY_FE_COLOR_MATRIX_VALUES)
    {
      switch (svg_enum_get (filter->base[svg_filter_type_get_index (filter->type, SVG_PROPERTY_FE_COLOR_MATRIX_TYPE)]))
        {
        case COLOR_MATRIX_TYPE_MATRIX: return svg_numbers_new_identity_matrix ();
        case COLOR_MATRIX_TYPE_SATURATE: return svg_numbers_new1 (1);
        case COLOR_MATRIX_TYPE_HUE_ROTATE: return svg_numbers_new1 (0);
        case COLOR_MATRIX_TYPE_LUMINANCE_TO_ALPHA: return svg_numbers_new_none ();
        default: g_assert_not_reached ();
        }
    }

  if (filter->type == SVG_FILTER_DROPSHADOW && attr == SVG_PROPERTY_FE_STD_DEV)
    return svg_number_new (2);

  if (filter->type == SVG_FILTER_DROPSHADOW &&
      (attr == SVG_PROPERTY_FE_DX || attr == SVG_PROPERTY_FE_DY))
    return svg_number_new (2);

  return svg_property_ref_initial_value (attr, SVG_ELEMENT_FILTER, TRUE);
}

void
svg_filter_set_specified_value (SvgFilter   *filter,
                                SvgProperty  attr,
                                SvgValue    *value)
{
  unsigned int i;
  PropertyValue *v;
  unsigned int pos = svg_filter_type_get_index (filter->type, attr);

  if ((filter->attrs & BIT (pos)) != 0)
    {
      for (i = 0; i < filter->specified->len; i++)
        {
          v = &g_array_index (filter->specified, PropertyValue, i);

          if (v->attr == attr)
            {
              g_clear_pointer (&v->value, svg_value_unref);
              break;
            }
        }
    }
  else
    i = filter->specified->len;

  if (i == filter->specified->len)
    {
      PropertyValue pv = { attr, NULL };
      g_array_append_val (filter->specified, pv);
    }

  v = &g_array_index (filter->specified, PropertyValue, i);
  g_assert (v->value == NULL);
  if (value)
    {
      filter->attrs |= BIT (pos);
      v->value = svg_value_ref (value);
    }
  else
    {
      filter->attrs &= ~BIT (pos);
    }
}

void
svg_filter_take_specified_value (SvgFilter   *filter,
                                 SvgProperty  attr,
                                 SvgValue    *value)
{
  svg_filter_set_specified_value (filter, attr, value);
  if (value)
    svg_value_unref (value);
}

SvgValue *
svg_filter_get_specified_value (SvgFilter   *filter,
                                SvgProperty  attr)
{
  if (svg_filter_is_specified (filter, attr))
    {
      for (unsigned int i = 0; i < filter->specified->len; i++)
        {
          PropertyValue *v = &g_array_index (filter->specified, PropertyValue, i);
          if (v->attr == attr)
            return v->value;
        }
    }

  return NULL;
}

gboolean
svg_filter_is_specified (SvgFilter   *filter,
                         SvgProperty  attr)
{
  unsigned int pos = svg_filter_type_get_index (filter->type, attr);
  return (filter->attrs & BIT (pos)) != 0;
}

void
svg_filter_set_base_value (SvgFilter   *filter,
                           SvgProperty  attr,
                           SvgValue    *value)
{
  unsigned int pos;

  if (!svg_filter_type_has_property (filter->type, attr))
    return;

  pos = svg_filter_type_get_index (filter->type, attr);
  g_clear_pointer (&filter->base[pos], svg_value_unref);
  if (value)
    filter->base[pos] = svg_value_ref (value);
  else
    filter->base[pos] = svg_filter_ref_initial_value (filter, attr);
}

SvgValue *
svg_filter_get_base_value (SvgFilter   *filter,
                           SvgProperty  attr)
{
  unsigned int pos = svg_filter_type_get_index (filter->type, attr);
  return filter->base[pos];
}

void
svg_filter_set_current_value (SvgFilter   *filter,
                              SvgProperty  attr,
                              SvgValue    *value)
{
  unsigned int pos = svg_filter_type_get_index (filter->type, attr);

  if (value)
    svg_value_ref (value);
  g_clear_pointer (&filter->current[pos], svg_value_unref);
  filter->current[pos] = value;
}

SvgValue *
svg_filter_get_current_value (SvgFilter   *filter,
                              SvgProperty  attr)
{
  unsigned int pos = svg_filter_type_get_index (filter->type, attr);
  return filter->current[pos];
}

GskComponentTransfer *
svg_filter_get_component_transfer (SvgFilter *filter)
{
  ComponentTransferType type;

  g_assert (filter->type == SVG_FILTER_FUNC_R ||
            filter->type == SVG_FILTER_FUNC_G ||
            filter->type == SVG_FILTER_FUNC_B ||
            filter->type == SVG_FILTER_FUNC_A);

  type = svg_enum_get (filter->current[svg_filter_type_get_index (filter->type, SVG_PROPERTY_FE_FUNC_TYPE)]);
  switch (type)
    {
    case COMPONENT_TRANSFER_IDENTITY:
      return gsk_component_transfer_new_identity ();
    case COMPONENT_TRANSFER_TABLE:
    case COMPONENT_TRANSFER_DISCRETE:
      {
        SvgValue *v = filter->current [svg_filter_type_get_index (filter->type, SVG_PROPERTY_FE_FUNC_VALUES)];
        unsigned int n_values = svg_numbers_get_length (v);
        float *values = g_newa (float, n_values);

        if (n_values == 0)
          return gsk_component_transfer_new_identity ();

        for (unsigned int i = 0; i < n_values; i++)
          values[i] = svg_numbers_get (v, i, 100);

        if (type == COMPONENT_TRANSFER_TABLE)
          return gsk_component_transfer_new_table (n_values, values);
        else
          return gsk_component_transfer_new_discrete (n_values, values);
      }
    case COMPONENT_TRANSFER_LINEAR:
      {
        double slope = svg_number_get (filter->current[svg_filter_type_get_index (filter->type, SVG_PROPERTY_FE_FUNC_SLOPE)], 1);
        double intercept = svg_number_get (filter->current[svg_filter_type_get_index (filter->type, SVG_PROPERTY_FE_FUNC_INTERCEPT)], 1);
        return gsk_component_transfer_new_linear (slope, intercept);
      }
    case COMPONENT_TRANSFER_GAMMA:
      {
        double amplitude = svg_number_get (filter->current[svg_filter_type_get_index (filter->type, SVG_PROPERTY_FE_FUNC_AMPLITUDE)], 1);
        double exponent = svg_number_get (filter->current[svg_filter_type_get_index (filter->type, SVG_PROPERTY_FE_FUNC_EXPONENT)], 1);
        double offset = svg_number_get (filter->current[svg_filter_type_get_index (filter->type, SVG_PROPERTY_FE_FUNC_OFFSET)], 1);
        return gsk_component_transfer_new_gamma (amplitude, exponent, offset);
      }

    default:
      g_assert_not_reached ();
    }
}

gboolean
svg_filter_needs_backdrop (SvgFilter *filter)
{
  SvgProperty attr[] = { SVG_PROPERTY_FE_IN, SVG_PROPERTY_FE_IN2 };

  for (unsigned int i = 0; i < G_N_ELEMENTS (attr); i++)
    {
      if (svg_filter_type_has_property (filter->type, attr[i]))
        {
          int idx = svg_filter_type_get_index (filter->type, attr[i]);
          SvgFilterRefType type = svg_filter_ref_get_type (filter->current[idx]);
          if (type == FILTER_REF_BACKGROUND_IMAGE || type == FILTER_REF_BACKGROUND_ALPHA)
            return TRUE;
        }
    }

  return FALSE;
}

gboolean
filter_needs_backdrop (SvgElement *filter)
{
  g_assert (filter->type == SVG_ELEMENT_FILTER);

  for (unsigned int i = 0; i < filter->filters->len; i++)
    {
      SvgFilter *f = g_ptr_array_index (filter->filters, i);

      if (svg_filter_needs_backdrop (f))
        return TRUE;
    }

  return FALSE;
}

SvgFilter *
svg_filter_new (SvgElement    *parent,
                SvgFilterType  type)
{
  unsigned int n_attrs;
  SvgFilter *filter;

  g_assert (parent->type == SVG_ELEMENT_FILTER);

  n_attrs = svg_filter_type_get_n_attrs (type);
  filter = g_malloc0 (sizeof (SvgFilter) + sizeof (SvgValue *) * (2 * n_attrs - 1));

  filter->type = type;
  filter->current = filter->base + n_attrs;

  filter->specified = array_new_with_clear_func (sizeof (PropertyValue), (GDestroyNotify) property_value_clear);
  filter->inline_styles = array_new_with_clear_func (sizeof (PropertyValue), (GDestroyNotify) property_value_clear);

  for (unsigned int i = 0; i < n_attrs; i++)
    {
      SvgProperty attr = svg_filter_type_get_property (type, i);
      filter->base[i] = svg_filter_ref_initial_value (filter, attr);
    }

  filter->css_node = gtk_css_node_new ();
  gtk_css_node_set_name (filter->css_node, g_quark_from_static_string (svg_filter_type_get_name (type)));
  gtk_css_node_set_parent (filter->css_node, parent->css_node);

  return filter;
}

SvgFilterType
svg_filter_get_type (SvgFilter *filter)
{
  return filter->type;
}

void
svg_filter_set_id (SvgFilter  *filter,
                   const char *id)
{
  g_free (filter->id);
  filter->id = g_strdup (id);
  gtk_css_node_set_id (filter->css_node, g_quark_from_string (id));
}

const char *
svg_filter_get_id (SvgFilter *filter)
{
  return filter->id;
}

void
svg_filter_set_style (SvgFilter  *filter,
                      const char *style)
{
  g_free (filter->style);
  filter->style = g_strdup (style);
}

const char *
svg_filter_get_style (SvgFilter *filter)
{
  return filter->style;
}

void
svg_filter_parse_classes (SvgFilter  *filter,
                        const char *classes)
{
  svg_filter_take_classes (filter, parse_strv (classes));
}

void
svg_filter_take_classes (SvgFilter *filter,
                         GStrv      classes)
{
  g_strfreev (filter->classes);
  filter->classes = classes;
  gtk_css_node_set_classes (filter->css_node, (const char **) filter->classes);
}

GStrv
svg_filter_get_classes (SvgFilter *filter)
{
  return filter->classes;
}

void
svg_filter_set_origin (SvgFilter      *filter,
                       GtkSvgLocation *location)
{
  filter->lines = location->lines;
}

void
svg_filter_get_origin (SvgFilter      *filter,
                       GtkSvgLocation *location)
{
  location->lines = filter->lines;
  location->line_chars = 0;
  location->bytes = 0;
}

GtkCssNode *
svg_filter_get_css_node (SvgFilter *filter)
{
  return filter->css_node;
}

gboolean
svg_filter_equal (SvgFilter *filter1,
                  SvgFilter *filter2)
{
  if (filter1->type != filter2->type)
    return FALSE;

  for (unsigned int i = 0; i < svg_filter_type_get_n_attrs (filter1->type); i++)
    {
      if (!svg_value_equal (filter1->base[i], filter2->base[i]))
        return FALSE;
    }

  if (g_strcmp0 (filter1->id, filter2->id) != 0)
    return FALSE;

  if (g_strcmp0 (filter1->style, filter2->style) != 0)
    return FALSE;

  if (filter1->classes && filter2->classes)
    {
      if (!g_strv_equal ((const char * const *) filter1->classes,
                         (const char * const *) filter2->classes))
        return FALSE;
    }
  else
    {
      if (filter1->classes != filter2->classes)
        return FALSE;
    }

  return TRUE;
}

GArray *
svg_filter_get_inline_styles (SvgFilter *filter)
{
  return filter->inline_styles;
}
