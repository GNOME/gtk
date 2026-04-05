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

#include "gtksvgcolorstopprivate.h"

#include "gtksvgutilsprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgelementinternal.h"


struct _SvgColorStop
{
  unsigned int attrs;
  SvgValue *specified[N_STOP_PROPERTIES];
  SvgValue *base[N_STOP_PROPERTIES];
  SvgValue *current[N_STOP_PROPERTIES];
  size_t lines;
  char *id;
  char *style;
  char **classes;
  GtkCssNode *css_node;
};

void
svg_color_stop_free (SvgColorStop *stop)
{
  g_free (stop->id);
  g_free (stop->style);
  g_strfreev (stop->classes);
  g_object_unref (stop->css_node);

  for (unsigned int i = 0; i < N_STOP_PROPERTIES; i++)
    {
      g_clear_pointer (&stop->specified[i], svg_value_unref);
      g_clear_pointer (&stop->base[i], svg_value_unref);
      g_clear_pointer (&stop->current[i], svg_value_unref);
    }
  g_free (stop);
}

unsigned int
svg_color_stop_get_index (SvgProperty attr)
{
  g_assert (attr >= FIRST_STOP_PROPERTY && attr <= LAST_STOP_PROPERTY);
  return attr - FIRST_STOP_PROPERTY;
}

SvgProperty
svg_color_stop_get_property (unsigned int idx)
{
  g_assert (idx < N_STOP_PROPERTIES);
  return idx + FIRST_STOP_PROPERTY;
}

SvgColorStop *
svg_color_stop_new (SvgElement *parent)
{
  SvgColorStop *stop = g_new0 (SvgColorStop, 1);

  for (SvgProperty attr = FIRST_STOP_PROPERTY; attr <= LAST_STOP_PROPERTY; attr++)
    stop->base[attr - FIRST_STOP_PROPERTY] = svg_property_ref_initial_value (attr, SVG_ELEMENT_LINEAR_GRADIENT, TRUE);

  stop->css_node = gtk_css_node_new ();
  gtk_css_node_set_name (stop->css_node, g_quark_from_static_string ("stop"));
  gtk_css_node_set_parent (stop->css_node, parent->css_node);

  return stop;
}

void
svg_color_stop_set_specified_value (SvgColorStop *stop,
                                    SvgProperty   attr,
                                    SvgValue     *value)
{
  unsigned int pos = svg_color_stop_get_index (attr);
  g_clear_pointer (&stop->specified[pos], svg_value_unref);
  if (value)
    stop->specified[pos] = svg_value_ref (value);
}

void
svg_color_stop_take_specified_value (SvgColorStop *stop,
                                     SvgProperty   attr,
                                     SvgValue     *value)
{
  svg_color_stop_set_specified_value (stop, attr, value);
  if (value)
    svg_value_unref (value);
}

SvgValue *
svg_color_stop_get_specified_value (SvgColorStop *stop,
                                    SvgProperty   attr)
{
  unsigned int pos = svg_color_stop_get_index (attr);
  return stop->specified[pos];
}

gboolean
svg_color_stop_is_specified (SvgColorStop *stop,
                             SvgProperty   attr)
{
  unsigned int pos = svg_color_stop_get_index (attr);
  return stop->specified[pos] != NULL;
}

void
svg_color_stop_set_base_value (SvgColorStop *stop,
                               SvgProperty   attr,
                               SvgValue     *value)
{
  unsigned int pos = svg_color_stop_get_index (attr);
  g_clear_pointer (&stop->base[pos], svg_value_unref);
  if (value)
    {
      stop->base[pos] = svg_value_ref (value);
      stop->attrs |= BIT (pos);
    }
  else
    {
      stop->base[pos] = svg_property_ref_initial_value (attr, SVG_ELEMENT_LINEAR_GRADIENT, TRUE);
      stop->attrs &= ~BIT (pos);
    }
}

SvgValue *
svg_color_stop_get_base_value (SvgColorStop *stop,
                               SvgProperty   attr)
{
  unsigned int pos = svg_color_stop_get_index (attr);
  return stop->base[pos];
}

void
svg_color_stop_set_current_value (SvgColorStop *stop,
                                  SvgProperty   attr,
                                  SvgValue     *value)
{
  unsigned int pos = svg_color_stop_get_index (attr);

  if (value)
    svg_value_ref (value);
  g_clear_pointer (&stop->current[pos], svg_value_unref);
  stop->current[pos] = value;
}

SvgValue *
svg_color_stop_get_current_value (SvgColorStop *stop,
                                  SvgProperty   attr)
{
  unsigned int pos = svg_color_stop_get_index (attr);
  return stop->current[pos];
}

void
svg_color_stop_set_id (SvgColorStop *stop,
                       const char   *id)
{
  g_free (stop->id);
  stop->id = g_strdup (id);
  gtk_css_node_set_id (stop->css_node, g_quark_from_string (id));
}

const char *
svg_color_stop_get_id (SvgColorStop *stop)
{
  return stop->id;
}

void
svg_color_stop_set_style (SvgColorStop *stop,
                          const char   *style)
{
  g_free (stop->style);
  stop->style = g_strdup (style);
}

const char *
svg_color_stop_get_style (SvgColorStop *stop)
{
  return stop->style;
}

void
svg_color_stop_parse_classes (SvgColorStop *stop,
                              const char   *classes)
{
  svg_color_stop_take_classes (stop, parse_strv (classes));
}

void
svg_color_stop_take_classes (SvgColorStop *stop,
                             GStrv         classes)
{
  g_strfreev (stop->classes);
  stop->classes = classes;
  gtk_css_node_set_classes (stop->css_node, (const char **) stop->classes);
}

GStrv
svg_color_stop_get_classes (SvgColorStop *stop)
{
  return stop->classes;
}

void
svg_color_stop_set_origin (SvgColorStop   *stop,
                           GtkSvgLocation *location)
{
  stop->lines = location->lines;
}

void
svg_color_stop_get_origin (SvgColorStop   *stop,
                           GtkSvgLocation *location)
{
  location->lines = stop->lines;
}

GtkCssNode *
svg_color_stop_get_css_node (SvgColorStop *stop)
{
  return stop->css_node;
}

gboolean
svg_color_stop_equal (SvgColorStop *stop1,
                      SvgColorStop *stop2)
{
  for (unsigned int i = 0; i < N_STOP_PROPERTIES; i++)
    {
      if (!svg_value_equal (stop1->base[i], stop2->base[i]))
        return FALSE;
    }

  if (g_strcmp0 (stop1->id, stop2->id) != 0)
    return FALSE;

  if (g_strcmp0 (stop1->style, stop2->style) != 0)
    return FALSE;

  if (stop1->classes && stop2->classes)
    {
      if (!g_strv_equal ((const char * const *) stop1->classes,
                         (const char * const *) stop2->classes))
        return FALSE;
    }
  else
    {
      if (stop1->classes != stop2->classes)
        return FALSE;
    }

  return TRUE;
}
