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

#include "gtksvgelementinternal.h"

#include "gtksvgutilsprivate.h"
#include "gtksvgpathutilsprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgcolorstopprivate.h"
#include "gtksvgfilterprivate.h"
#include "gtksvghrefprivate.h"
#include "gtksvgtransformprivate.h"
#include "gtksvgenumprivate.h"
#include "gtksvgnumberprivate.h"
#include "gtksvgnumbersprivate.h"
#include "gtksvgpathprivate.h"
#include "gtksvgstringlistprivate.h"
#include "gtksvglanguageprivate.h"
#include "gtksvgkeywordprivate.h"
#include "gsk/gskpathprivate.h"
#include "gtksvganimationprivate.h"
#include "gtkmain.h"


static void
text_node_clear (TextNode *self)
{
  switch (self->type)
    {
    case TEXT_NODE_SHAPE:
      // shape node not owned
      break;
    case TEXT_NODE_CHARACTERS:
      g_assert (self->characters.layout == NULL);
      g_free (self->characters.text);
      break;
    default:
      g_assert_not_reached ();
    }
}

void
property_value_clear (PropertyValue *v)
{
  g_clear_pointer (&v->value, svg_value_unref);
}

static void
style_elt_free (gpointer p)
{
  StyleElt *e = (StyleElt *) p;

  g_bytes_unref (e->content);
  g_free (e);
}

void
svg_element_free (SvgElement *element)
{
  g_clear_pointer (&element->id, g_free);
  g_clear_pointer (&element->style, g_free);
  g_clear_pointer (&element->classes, g_strfreev);
  g_clear_object (&element->css_node);

  g_clear_pointer (&element->specified, g_array_unref);
  g_clear_pointer (&element->inline_styles, g_array_unref);

  for (SvgProperty attr = FIRST_SVG_PROPERTY; attr <= LAST_SVG_PROPERTY; attr++)
    {
      g_clear_pointer (&element->base[attr], svg_value_unref);
      g_clear_pointer (&element->current[attr], svg_value_unref);
    }

  g_clear_pointer (&element->styles, g_ptr_array_unref);
  g_clear_pointer (&element->shapes, g_ptr_array_unref);
  g_clear_pointer (&element->animations, g_ptr_array_unref);
  g_clear_pointer (&element->color_stops, g_ptr_array_unref);
  g_clear_pointer (&element->filters, g_ptr_array_unref);
  g_clear_pointer (&element->deps, g_ptr_array_unref);

  g_clear_pointer (&element->path, gsk_path_unref);
  g_clear_pointer (&element->measure, gsk_path_measure_unref);

  g_clear_pointer (&element->text, g_array_unref);

  if (element->type == SVG_ELEMENT_POLYLINE || element->type == SVG_ELEMENT_POLYGON)
    g_clear_pointer (&element->path_for.polyline.points, svg_value_unref);

  g_clear_pointer (&element->gpa.fill, svg_value_unref);
  g_clear_pointer (&element->gpa.stroke, svg_value_unref);
  g_clear_pointer (&element->gpa.width, svg_value_unref);
  g_free (element->gpa.attach.ref);

  _gtk_bitmask_free (element->attrs);

  g_free (element);
}

SvgElement *
svg_element_new (SvgElement     *parent,
                 SvgElementType  type)
{
  SvgElement *element = g_new0 (SvgElement, 1);

  element->parent = parent;
  element->type = type;

  element->attrs = _gtk_bitmask_new ();

  element->specified = array_new_with_clear_func (sizeof (PropertyValue), (GDestroyNotify) property_value_clear);

  element->inline_styles = array_new_with_clear_func (sizeof (PropertyValue), (GDestroyNotify) property_value_clear);

  for (SvgProperty attr = FIRST_SVG_PROPERTY; attr <= LAST_SVG_PROPERTY; attr++)
    {
      element->base[attr] = svg_property_ref_initial_value (attr, type, parent != NULL);
      element->current[attr] = svg_value_ref (element->base[attr]);
    }

  if (svg_element_type_is_container (type))
    element->shapes = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_element_free);

  if (svg_element_type_is_gradient (type))
    element->color_stops = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_color_stop_free);

  if (svg_element_type_is_filter (type))
    element->filters = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_filter_free);

  if (svg_element_type_is_text (type))
    element->text = array_new_with_clear_func (sizeof (TextNode), (GDestroyNotify) text_node_clear);

  if (svg_element_type_is_path (type))
    {
      element->gpa.states = ALL_STATES;
    }

  element->styles = g_ptr_array_new_with_free_func (style_elt_free);

  element->css_node = gtk_css_node_new ();
  gtk_css_node_set_name (element->css_node, g_quark_from_static_string (svg_element_type_get_name (type)));
  if (parent)
    gtk_css_node_set_parent (element->css_node, parent->css_node);

  element->focusable = svg_element_get_initial_focusable (element);

  if (type == SVG_ELEMENT_LINK)
    gtk_css_node_set_state (element->css_node, GTK_STATE_FLAG_LINK);

  return element;
}

/* {{{ Conditional exclusion */

gboolean
svg_element_conditionally_excluded (SvgElement *element,
                                    GtkSvg     *svg)
{
  SvgValue *required_extensions = element->current[SVG_PROPERTY_REQUIRED_EXTENSIONS];
  SvgValue *system_language = element->current[SVG_PROPERTY_SYSTEM_LANGUAGE];
  PangoLanguage *lang;

  if (svg_value_equal (required_extensions, svg_string_list_new (NULL)) &&
      svg_value_equal (system_language, svg_language_new_list (0, NULL)))
    return FALSE;

  for (unsigned int i = 0; i < svg_string_list_get_length (required_extensions); i++)
    {
      if ((svg->features & GTK_SVG_EXTENSIONS) == 0)
        return TRUE;

      if (strcmp (svg_string_list_get (required_extensions, i), "http://www.gtk.org/grappa") != 0)
        return TRUE;
    }

  lang = gtk_get_default_language ();

  for (unsigned int i = 0; i < svg_language_get_length (system_language); i++)
    {
      const char *l1;
      const char *l2;

      if (lang == svg_language_get (system_language, i))
        return FALSE;

      l1 = pango_language_to_string (lang);
      l2 = pango_language_to_string (svg_language_get (system_language, i));

      for (unsigned int j = 0; l1[j]; j++)
        {
          if (l1[j] == '-')
            return FALSE;

          if (l1[j] != l2[j])
            break;
        }
    }

  return TRUE;
}

/* }}} */

static void
resolve_rx (SvgElement            *element,
            const graphene_rect_t *viewport,
            gboolean               current,
            double                *rx,
            double                *ry)
{
  SvgValue **values;

  if (current)
    values = element->current;
  else
    values = element->base;

  if (element->type == SVG_ELEMENT_ELLIPSE)
    {
      if (svg_value_is_auto (values[SVG_PROPERTY_RX]))
        {
          if (svg_value_is_auto (values[SVG_PROPERTY_RY]))
            *ry = 0;
          else
            *ry = svg_number_get (values[SVG_PROPERTY_RY], viewport->size.height);
          *rx = *ry;
         }
       else
         {
           *rx = svg_number_get (values[SVG_PROPERTY_RX], viewport->size.width);
           if (svg_value_is_auto (values[SVG_PROPERTY_RY]))
             *ry = *rx;
           else
             *ry = svg_number_get (values[SVG_PROPERTY_RY], viewport->size.height);
         }
    }
  else if (element->type == SVG_ELEMENT_RECT)
    {
      if (svg_value_is_auto (values[SVG_PROPERTY_RX]) &&
         svg_value_is_auto (values[SVG_PROPERTY_RY]))
        {
          *rx = *ry = 0;
        }
      else if (svg_value_is_auto (values[SVG_PROPERTY_RY]))
        {
          *rx = svg_number_get (values[SVG_PROPERTY_RX], viewport->size.width);
          *ry = *rx;
        }
      else if (svg_value_is_auto (values[SVG_PROPERTY_RX]))
        {
          *ry = svg_number_get (values[SVG_PROPERTY_RY], viewport->size.height);
          *rx = *ry;
        }
      else
        {
          *rx = svg_number_get (values[SVG_PROPERTY_RX], viewport->size.width);
          *ry = svg_number_get (values[SVG_PROPERTY_RY], viewport->size.height);
        }

      double w = svg_number_get (values[SVG_PROPERTY_WIDTH], viewport->size.width);
      double h = svg_number_get (values[SVG_PROPERTY_HEIGHT], viewport->size.height);
      if (*rx > w / 2)
        *rx = w / 2;
      if (*ry > h / 2)
        *ry = h / 2;
    }
  else
    g_assert_not_reached ();
}

GskPath *
svg_element_get_path (SvgElement            *element,
                      const graphene_rect_t *viewport,
                      gboolean               current)
{
  GskPathBuilder *builder;
  SvgValue **values;

  if (current)
    values = element->current;
  else
    values = element->base;

  switch (element->type)
    {
    case SVG_ELEMENT_LINE:
      builder = gsk_path_builder_new ();
      if (values[SVG_PROPERTY_X1] && values[SVG_PROPERTY_Y1] &&
          values[SVG_PROPERTY_X2] && values[SVG_PROPERTY_Y2])
        {
          double x1 = svg_number_get (values[SVG_PROPERTY_X1], viewport->size.width);
          double y1 = svg_number_get (values[SVG_PROPERTY_Y1], viewport->size.height);
          double x2 = svg_number_get (values[SVG_PROPERTY_X2], viewport->size.width);
          double y2 = svg_number_get (values[SVG_PROPERTY_Y2], viewport->size.height);
          gsk_path_builder_move_to (builder, x1, y1);
          gsk_path_builder_line_to (builder, x2, y2);
        }
      return gsk_path_builder_free_to_path (builder);

    case SVG_ELEMENT_POLYLINE:
    case SVG_ELEMENT_POLYGON:
      builder = gsk_path_builder_new ();
      if (values[SVG_PROPERTY_POINTS])
        {
          SvgValue *v = values[SVG_PROPERTY_POINTS];
          if (svg_numbers_get_length (v) > 0)
            {
              g_assert (svg_numbers_get_unit (v, 0) != SVG_UNIT_PERCENTAGE);
              g_assert (svg_numbers_get_unit (v, 1) != SVG_UNIT_PERCENTAGE);

              gsk_path_builder_move_to (builder,
                                        svg_numbers_get (v, 0, 1),
                                        svg_numbers_get (v, 1, 1));

              for (unsigned int i = 2; i + 1 < svg_numbers_get_length (v); i += 2)
                {
                  g_assert (svg_numbers_get_unit (v, i) != SVG_UNIT_PERCENTAGE);
                  g_assert (svg_numbers_get_unit (v, i + 1) != SVG_UNIT_PERCENTAGE);

                  gsk_path_builder_line_to (builder,
                                            svg_numbers_get (v, i, 1),
                                            svg_numbers_get (v, i + 1, 1));
                }
              if (element->type == SVG_ELEMENT_POLYGON)
                gsk_path_builder_close (builder);
            }
        }
      return gsk_path_builder_free_to_path (builder);

    case SVG_ELEMENT_CIRCLE:
      builder = gsk_path_builder_new ();
      if (values[SVG_PROPERTY_CX] && values[SVG_PROPERTY_CY] && values[SVG_PROPERTY_R])
        {
          double cx = svg_number_get (values[SVG_PROPERTY_CX], viewport->size.width);
          double cy = svg_number_get (values[SVG_PROPERTY_CY], viewport->size.height);
          double r = svg_number_get (values[SVG_PROPERTY_R], normalized_diagonal (viewport));
          gsk_path_builder_add_circle (builder, &GRAPHENE_POINT_INIT (cx, cy), r);
        }
      return gsk_path_builder_free_to_path (builder);

    case SVG_ELEMENT_ELLIPSE:
      builder = gsk_path_builder_new ();
      if (values[SVG_PROPERTY_CX] && values[SVG_PROPERTY_CY] &&
          values[SVG_PROPERTY_RX] && values[SVG_PROPERTY_RY])
        {
          double cx = svg_number_get (values[SVG_PROPERTY_CX], viewport->size.width);
          double cy = svg_number_get (values[SVG_PROPERTY_CY], viewport->size.height);
          double rx, ry;
          resolve_rx (element, viewport, current, &rx, &ry);
          path_builder_add_ellipse (builder, cx, cy, rx, ry);
        }
      return gsk_path_builder_free_to_path (builder);

    case SVG_ELEMENT_RECT:
      builder = gsk_path_builder_new ();
      if (values[SVG_PROPERTY_X] && values[SVG_PROPERTY_Y] &&
          values[SVG_PROPERTY_WIDTH] && values[SVG_PROPERTY_HEIGHT] &&
          values[SVG_PROPERTY_RX] && values[SVG_PROPERTY_RY])
        {
          double x = svg_number_get (values[SVG_PROPERTY_X], viewport->size.width);
          double y = svg_number_get (values[SVG_PROPERTY_Y], viewport->size.height);
          double width = svg_number_get (values[SVG_PROPERTY_WIDTH], viewport->size.width);
          double height = svg_number_get (values[SVG_PROPERTY_HEIGHT],viewport->size.height);
          double rx, ry;
          resolve_rx (element, viewport, current, &rx, &ry);
          if (rx == 0 || ry == 0)
            gsk_path_builder_add_rect (builder, &GRAPHENE_RECT_INIT (x, y, width, height));
          else
            gsk_path_builder_add_rounded_rect (builder,
                                               &(GskRoundedRect) {
                                                 .bounds = GRAPHENE_RECT_INIT (x, y, width, height),
                                                 .corner = {
                                                   GRAPHENE_SIZE_INIT (rx, ry),
                                                   GRAPHENE_SIZE_INIT (rx, ry),
                                                   GRAPHENE_SIZE_INIT (rx, ry),
                                                   GRAPHENE_SIZE_INIT (rx, ry),
                                                 }
                                              });
        }
      return gsk_path_builder_free_to_path (builder);

    case SVG_ELEMENT_PATH:
      if (values[SVG_PROPERTY_PATH] &&
          svg_path_get_gsk (values[SVG_PROPERTY_PATH]))
        {
          return gsk_path_ref (svg_path_get_gsk (values[SVG_PROPERTY_PATH]));
        }
      else
        {
          builder = gsk_path_builder_new ();
          return gsk_path_builder_free_to_path (builder);
        }
      break;

    case SVG_ELEMENT_USE:
      {
        SvgElement *use_element = svg_href_get_shape (element->current[SVG_PROPERTY_HREF]);
        if (use_element)
          {
            return svg_element_get_path (use_element, viewport, current);
          }
        else
          {
            builder = gsk_path_builder_new ();
            return gsk_path_builder_free_to_path (builder);
          }
      }
    case SVG_ELEMENT_GROUP:
    case SVG_ELEMENT_CLIP_PATH:
    case SVG_ELEMENT_MASK:
    case SVG_ELEMENT_DEFS:
    case SVG_ELEMENT_LINEAR_GRADIENT:
    case SVG_ELEMENT_RADIAL_GRADIENT:
    case SVG_ELEMENT_PATTERN:
    case SVG_ELEMENT_MARKER:
    case SVG_ELEMENT_TEXT:
    case SVG_ELEMENT_TSPAN:
    case SVG_ELEMENT_SVG:
    case SVG_ELEMENT_IMAGE:
    case SVG_ELEMENT_FILTER:
    case SVG_ELEMENT_SYMBOL:
    case SVG_ELEMENT_SWITCH:
    case SVG_ELEMENT_LINK:
      g_error ("Attempt to get the path of a %s", svg_element_type_get_name (element->type));
      break;
    default:
      g_assert_not_reached ();
    }
}

GskPath *
svg_element_get_current_path (SvgElement            *element,
                              const graphene_rect_t *viewport)
{
  if (element->path)
    {
      double rx, ry;

      switch (element->type)
        {
        case SVG_ELEMENT_LINE:
          if (element->path_for.line.x1 != svg_number_get (element->current[SVG_PROPERTY_X1], viewport->size.width) ||
              element->path_for.line.y1 != svg_number_get (element->current[SVG_PROPERTY_Y1], viewport->size.height) ||
              element->path_for.line.x2 != svg_number_get (element->current[SVG_PROPERTY_X2], viewport->size.width) ||
              element->path_for.line.y2 != svg_number_get (element->current[SVG_PROPERTY_Y2], viewport->size.height))
            {
              g_clear_pointer (&element->path, gsk_path_unref);
              g_clear_pointer (&element->measure, gsk_path_measure_unref);
            }
          break;

        case SVG_ELEMENT_POLYLINE:
        case SVG_ELEMENT_POLYGON:
          if (!svg_value_equal (element->path_for.polyline.points, element->current[SVG_PROPERTY_POINTS]))
            {
              g_clear_pointer (&element->path, gsk_path_unref);
              g_clear_pointer (&element->measure, gsk_path_measure_unref);
            }
          break;

        case SVG_ELEMENT_CIRCLE:
          if (element->path_for.circle.cx != svg_number_get (element->current[SVG_PROPERTY_CX], viewport->size.width) ||
              element->path_for.circle.cy != svg_number_get (element->current[SVG_PROPERTY_CY], viewport->size.height) ||
              element->path_for.circle.r != svg_number_get (element->current[SVG_PROPERTY_R], normalized_diagonal (viewport)))
            {
              g_clear_pointer (&element->path, gsk_path_unref);
              g_clear_pointer (&element->measure, gsk_path_measure_unref);
            }
          break;

        case SVG_ELEMENT_ELLIPSE:
          resolve_rx (element, viewport, TRUE, &rx, &ry);
          if (element->path_for.ellipse.cx != svg_number_get (element->current[SVG_PROPERTY_CX], viewport->size.width) ||
              element->path_for.ellipse.cy != svg_number_get (element->current[SVG_PROPERTY_CY], viewport->size.height) ||
              element->path_for.ellipse.rx != rx ||
              element->path_for.ellipse.ry != ry)
            {
              g_clear_pointer (&element->path, gsk_path_unref);
              g_clear_pointer (&element->measure, gsk_path_measure_unref);
            }
          break;

        case SVG_ELEMENT_RECT:
          resolve_rx (element, viewport, TRUE, &rx, &ry);
          if (element->path_for.rect.x != svg_number_get (element->current[SVG_PROPERTY_X], viewport->size.width) ||
              element->path_for.rect.y != svg_number_get (element->current[SVG_PROPERTY_Y], viewport->size.height) ||
              element->path_for.rect.w != svg_number_get (element->current[SVG_PROPERTY_WIDTH], viewport->size.width) ||
              element->path_for.rect.h != svg_number_get (element->current[SVG_PROPERTY_HEIGHT], viewport->size.height) ||
              element->path_for.rect.rx != rx ||
              element->path_for.rect.ry != ry)
            {
              g_clear_pointer (&element->path, gsk_path_unref);
              g_clear_pointer (&element->measure, gsk_path_measure_unref);
            }
          break;

        case SVG_ELEMENT_PATH:
          if (element->path != svg_path_get_gsk (element->current[SVG_PROPERTY_PATH]))
            {
              g_clear_pointer (&element->path, gsk_path_unref);
              g_clear_pointer (&element->measure, gsk_path_measure_unref);
            }
          break;

        case SVG_ELEMENT_USE:
          g_clear_pointer (&element->path, gsk_path_unref);
          break;

        case SVG_ELEMENT_GROUP:
        case SVG_ELEMENT_CLIP_PATH:
        case SVG_ELEMENT_MASK:
        case SVG_ELEMENT_DEFS:
        case SVG_ELEMENT_LINEAR_GRADIENT:
        case SVG_ELEMENT_RADIAL_GRADIENT:
        case SVG_ELEMENT_PATTERN:
        case SVG_ELEMENT_MARKER:
        case SVG_ELEMENT_TEXT:
        case SVG_ELEMENT_TSPAN:
        case SVG_ELEMENT_SVG:
        case SVG_ELEMENT_IMAGE:
        case SVG_ELEMENT_FILTER:
        case SVG_ELEMENT_SYMBOL:
        case SVG_ELEMENT_SWITCH:
        case SVG_ELEMENT_LINK:
          g_error ("Attempt to get the path of a %s", svg_element_type_get_name (element->type));
          break;
        default:
          g_assert_not_reached ();
        }
    }

  if (!element->path)
    {
      double rx, ry;
      element->path = svg_element_get_path (element, viewport, TRUE);
      element->valid_bounds = FALSE;

      switch (element->type)
        {
        case SVG_ELEMENT_LINE:
          element->path_for.line.x1 = svg_number_get (element->current[SVG_PROPERTY_X1], viewport->size.width);
          element->path_for.line.y1 = svg_number_get (element->current[SVG_PROPERTY_Y1], viewport->size.height);
          element->path_for.line.x2 = svg_number_get (element->current[SVG_PROPERTY_X2], viewport->size.width);
          element->path_for.line.y2 = svg_number_get (element->current[SVG_PROPERTY_Y2], viewport->size.height);
          break;

        case SVG_ELEMENT_POLYLINE:
        case SVG_ELEMENT_POLYGON:
          g_clear_pointer (&element->path_for.polyline.points, svg_value_unref);
          element->path_for.polyline.points = svg_value_ref (element->current[SVG_PROPERTY_POINTS]);
          break;

        case SVG_ELEMENT_CIRCLE:
          element->path_for.circle.cx = svg_number_get (element->current[SVG_PROPERTY_CX], viewport->size.width);
          element->path_for.circle.cy = svg_number_get (element->current[SVG_PROPERTY_CY], viewport->size.height);
          element->path_for.circle.r = svg_number_get (element->current[SVG_PROPERTY_R], normalized_diagonal (viewport));
          break;

        case SVG_ELEMENT_ELLIPSE:
          resolve_rx (element, viewport, TRUE, &rx, &ry);
          element->path_for.ellipse.cx = svg_number_get (element->current[SVG_PROPERTY_CX], viewport->size.width);
          element->path_for.ellipse.cy = svg_number_get (element->current[SVG_PROPERTY_CY], viewport->size.height);
          element->path_for.ellipse.rx = rx;
          element->path_for.ellipse.ry = ry;
          break;

        case SVG_ELEMENT_RECT:
          resolve_rx (element, viewport, TRUE, &rx, &ry);
          element->path_for.rect.x = svg_number_get (element->current[SVG_PROPERTY_X], viewport->size.width);
          element->path_for.rect.y = svg_number_get (element->current[SVG_PROPERTY_Y], viewport->size.height);
          element->path_for.rect.w = svg_number_get (element->current[SVG_PROPERTY_WIDTH], viewport->size.width);
          element->path_for.rect.h = svg_number_get (element->current[SVG_PROPERTY_HEIGHT], viewport->size.height);
          element->path_for.rect.rx = rx;
          element->path_for.rect.ry = ry;
          break;

        case SVG_ELEMENT_PATH:
        case SVG_ELEMENT_USE:
          break;

        case SVG_ELEMENT_GROUP:
        case SVG_ELEMENT_CLIP_PATH:
        case SVG_ELEMENT_MASK:
        case SVG_ELEMENT_DEFS:
        case SVG_ELEMENT_LINEAR_GRADIENT:
        case SVG_ELEMENT_RADIAL_GRADIENT:
        case SVG_ELEMENT_PATTERN:
        case SVG_ELEMENT_MARKER:
        case SVG_ELEMENT_TEXT:
        case SVG_ELEMENT_TSPAN:
        case SVG_ELEMENT_SVG:
        case SVG_ELEMENT_IMAGE:
        case SVG_ELEMENT_FILTER:
        case SVG_ELEMENT_SYMBOL:
        case SVG_ELEMENT_SWITCH:
        case SVG_ELEMENT_LINK:
        default:
          g_assert_not_reached ();
        }
    }

  return gsk_path_ref (element->path);
}

GskPathMeasure *
svg_element_get_current_measure (SvgElement            *element,
                                 const graphene_rect_t *viewport)
{
  if (!element->measure)
    {
      GskPath *path = svg_element_get_current_path (element, viewport);
      element->measure = gsk_path_measure_new (path);
      gsk_path_unref (path);
    }

  return gsk_path_measure_ref (element->measure);
}

gboolean
svg_element_get_current_bounds (SvgElement            *element,
                                const graphene_rect_t *viewport,
                                GtkSvg                *svg,
                                graphene_rect_t       *bounds)
{
  graphene_rect_t b;
  gboolean has_any;
  gboolean ret = FALSE;

  graphene_rect_init_from_rect (&b, graphene_rect_zero ());

  switch (element->type)
    {
    case SVG_ELEMENT_LINE:
    case SVG_ELEMENT_POLYLINE:
    case SVG_ELEMENT_POLYGON:
    case SVG_ELEMENT_RECT:
    case SVG_ELEMENT_CIRCLE:
    case SVG_ELEMENT_ELLIPSE:
    case SVG_ELEMENT_PATH:
      if (!element->valid_bounds)
        {
          GskPath *path = svg_element_get_current_path (element, viewport);
          if (!gsk_path_get_tight_bounds (element->path, &element->bounds))
            graphene_rect_init (&element->bounds, 0, 0, 0, 0);
          element->valid_bounds = TRUE;
          gsk_path_unref (path);
        }
      graphene_rect_init_from_rect (&b, &element->bounds);
      ret = TRUE;
      break;
    case SVG_ELEMENT_USE:
      {
        SvgElement *use_element = svg_href_get_shape (element->current[SVG_PROPERTY_HREF]);
        if (use_element)
          ret = svg_element_get_current_bounds (use_element, viewport, svg, &b);
        ret = TRUE;
      }
      break;
    case SVG_ELEMENT_GROUP:
    case SVG_ELEMENT_CLIP_PATH:
    case SVG_ELEMENT_MASK:
    case SVG_ELEMENT_PATTERN:
    case SVG_ELEMENT_MARKER:
    case SVG_ELEMENT_SVG:
    case SVG_ELEMENT_SYMBOL:
    case SVG_ELEMENT_SWITCH:
    case SVG_ELEMENT_LINK:
      has_any = FALSE;
      for (unsigned int i = 0; i < element->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (element->shapes, i);
          graphene_rect_t b2;

          if (svg_enum_get (sh->current[SVG_PROPERTY_DISPLAY]) == DISPLAY_NONE)
            continue;

          if (svg_element_conditionally_excluded (sh, svg))
            continue;

          if (svg_element_get_current_bounds (sh, viewport, svg, &b2))
            {
              if (_gtk_bitmask_get (sh->attrs, SVG_PROPERTY_TRANSFORM))
                {
                   GskTransform *transform = svg_transform_get_gsk (sh->current[SVG_PROPERTY_TRANSFORM]);
                   gsk_transform_transform_bounds (transform, &b2, &b2);
                   gsk_transform_unref (transform);
                }

              if (!has_any)
                graphene_rect_init_from_rect (&b, &b2);
              else
                graphene_rect_union (&b, &b2, &b);
              has_any = TRUE;
            }

          if (element->type == SVG_ELEMENT_SWITCH)
            break;
        }
      if (!has_any)
        graphene_rect_init (&b, 0, 0, 0, 0);
      ret = TRUE;
      break;
    case SVG_ELEMENT_TEXT:
    case SVG_ELEMENT_TSPAN:
      if (!element->valid_bounds)
        g_critical ("No valid bounds for text");
      graphene_rect_init_from_rect (&b, &element->bounds);
      break;
    case SVG_ELEMENT_IMAGE:
      {
        double x, y, width, height;

        x = viewport->origin.x + svg_number_get (element->current[SVG_PROPERTY_X], viewport->size.width);
        y = viewport->origin.y + svg_number_get (element->current[SVG_PROPERTY_Y], viewport->size.height);
        width = svg_number_get (element->current[SVG_PROPERTY_WIDTH], viewport->size.width);
        height = svg_number_get (element->current[SVG_PROPERTY_HEIGHT], viewport->size.height);

        graphene_rect_init (&b, x, y, width, height);
        ret = TRUE;
      }
      break;
    case SVG_ELEMENT_DEFS:
    case SVG_ELEMENT_LINEAR_GRADIENT:
    case SVG_ELEMENT_RADIAL_GRADIENT:
    case SVG_ELEMENT_FILTER:
      break;
    default:
      g_assert_not_reached ();
    }

  graphene_rect_init_from_rect (bounds, &b);

  return ret;
}

static double
width_apply_weight (double width,
                    double minwidth,
                    double maxwidth,
                    double weight)
{
  if (weight < 1)
    {
      g_assert_not_reached ();
    }
  else if (weight < 400)
    {
      double f = (400 - weight) / (400 - 1);
      return lerp (f, width, minwidth);
    }
  else if (weight == 400)
    {
      return width;
    }
  else if (weight <= 1000)
    {
      double f = (weight - 400) / (1000 - 400);
      return lerp (f, width, maxwidth);
    }
  else
    {
      g_assert_not_reached ();
    }
}

GskStroke *
svg_element_create_basic_stroke (SvgElement            *element,
                                 const graphene_rect_t *viewport,
                                 gboolean               allow_gpa,
                                 double                 weight)
{
  GskStroke *stroke;
  double width;

  width = svg_number_get (element->current[SVG_PROPERTY_STROKE_WIDTH], normalized_diagonal (viewport));

  if (allow_gpa)
    {
      double min, max;

      min = svg_number_get (element->current[SVG_PROPERTY_STROKE_MINWIDTH], width);
      max = svg_number_get (element->current[SVG_PROPERTY_STROKE_MAXWIDTH], width);

      width = width_apply_weight (width, min, max, weight);
    }

  stroke = gsk_stroke_new (width);

  gsk_stroke_set_line_cap (stroke, svg_enum_get (element->current[SVG_PROPERTY_STROKE_LINECAP]));
  gsk_stroke_set_line_join (stroke, svg_enum_get (element->current[SVG_PROPERTY_STROKE_LINEJOIN]));
  gsk_stroke_set_miter_limit (stroke, svg_number_get (element->current[SVG_PROPERTY_STROKE_MITERLIMIT], 1));

  return stroke;
}

gboolean
svg_element_get_current_stroke_bounds (SvgElement            *element,
                                       const graphene_rect_t *viewport,
                                       GtkSvg                *svg,
                                       graphene_rect_t       *bounds)
{
  graphene_rect_t b;
  gboolean has_any;
  gboolean ret = FALSE;

  graphene_rect_init_from_rect (&b, graphene_rect_zero ());

  switch (element->type)
    {
    case SVG_ELEMENT_LINE:
    case SVG_ELEMENT_POLYLINE:
    case SVG_ELEMENT_POLYGON:
    case SVG_ELEMENT_RECT:
    case SVG_ELEMENT_CIRCLE:
    case SVG_ELEMENT_ELLIPSE:
    case SVG_ELEMENT_PATH:
      {
        GskPath *path = svg_element_get_current_path (element, viewport);
        GskStroke *stroke = svg_element_create_basic_stroke (element, viewport, TRUE, 400);
        if (!gsk_path_get_stroke_bounds (element->path, stroke, &b))
          graphene_rect_init (&b, 0, 0, 0, 0);
        gsk_path_unref (path);
        gsk_stroke_free (stroke);
        ret = TRUE;
      }
      break;
    case SVG_ELEMENT_USE:
      {
        SvgElement *use_element = svg_href_get_shape (element->current[SVG_PROPERTY_HREF]);
        if (use_element)
          ret = svg_element_get_current_stroke_bounds (use_element, viewport, svg, &b);
        ret = TRUE;
      }
      break;
    case SVG_ELEMENT_GROUP:
    case SVG_ELEMENT_CLIP_PATH:
    case SVG_ELEMENT_MASK:
    case SVG_ELEMENT_PATTERN:
    case SVG_ELEMENT_MARKER:
    case SVG_ELEMENT_SVG:
    case SVG_ELEMENT_SYMBOL:
    case SVG_ELEMENT_SWITCH:
    case SVG_ELEMENT_LINK:
      has_any = FALSE;
      for (unsigned int i = 0; i < element->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (element->shapes, i);
          graphene_rect_t b2;

          if (svg_enum_get (sh->current[SVG_PROPERTY_DISPLAY]) == DISPLAY_NONE)
            continue;

          if (svg_element_conditionally_excluded (sh, svg))
            continue;

          if (svg_element_get_current_stroke_bounds (sh, viewport, svg, &b2))
            {
              if (!has_any)
                graphene_rect_init_from_rect (&b, &b2);
              else
                graphene_rect_union (&b, &b2, &b);
              has_any = TRUE;
            }

          if (element->type == SVG_ELEMENT_SWITCH)
            break;
        }
      if (!has_any)
        graphene_rect_init (&b, 0, 0, 0, 0);
      ret = TRUE;
      break;
    case SVG_ELEMENT_TEXT:
    case SVG_ELEMENT_TSPAN:
      if (!element->valid_bounds)
        g_critical ("No valid bounds for text");
      graphene_rect_init_from_rect (&b, &element->bounds);
      break;
    case SVG_ELEMENT_IMAGE:
      {
        double x, y, width, height;

        x = viewport->origin.x + svg_number_get (element->current[SVG_PROPERTY_X], viewport->size.width);
        y = viewport->origin.y + svg_number_get (element->current[SVG_PROPERTY_Y], viewport->size.height);
        width = svg_number_get (element->current[SVG_PROPERTY_WIDTH], viewport->size.width);
        height = svg_number_get (element->current[SVG_PROPERTY_HEIGHT], viewport->size.height);

        graphene_rect_init (&b, x, y, width, height);
        ret = TRUE;
      }
      break;
    case SVG_ELEMENT_DEFS:
    case SVG_ELEMENT_LINEAR_GRADIENT:
    case SVG_ELEMENT_RADIAL_GRADIENT:
    case SVG_ELEMENT_FILTER:
      break;
    default:
      g_assert_not_reached ();
    }

  graphene_rect_init_from_rect (bounds, &b);

  return ret;
}

void
svg_element_add_color_stop (SvgElement   *element,
                            SvgColorStop *stop)
{
  g_assert (gtk_css_node_get_parent (svg_color_stop_get_css_node (stop)) == element->css_node);
  g_assert (svg_element_type_is_gradient (element->type));
  g_ptr_array_add (element->color_stops, stop);
}

void
svg_element_add_filter (SvgElement *element,
                        SvgFilter  *filter)
{
  g_assert (gtk_css_node_get_parent (svg_filter_get_css_node (filter)) == element->css_node);
  g_assert (svg_element_type_is_filter (element->type));
  g_ptr_array_add (element->filters, filter);
}

void
svg_element_add_child (SvgElement *element,
                       SvgElement *child)
{
  g_assert (gtk_css_node_get_parent (svg_element_get_css_node (child)) == element->css_node);
  g_assert (svg_element_type_is_container (element->type));
  g_ptr_array_add (element->shapes, child);

  if (child->type == SVG_ELEMENT_TSPAN)
    {
      SvgElement *text_parent = NULL;

      if (svg_element_type_is_text (element->type))
        text_parent = element;
      else if (element->parent &&
               element->type == SVG_ELEMENT_LINK &&
               svg_element_type_is_text (element->parent->type))
        text_parent = element->parent;

      if (text_parent)
        {
          TextNode node = {
            .type = TEXT_NODE_SHAPE,
            .shape = { .shape = child }
          };
          g_array_append_val (text_parent->text, node);
        }
    }
}

unsigned int
svg_element_get_n_children (SvgElement *element)
{
  return element->shapes->len;
}

SvgElement *
svg_element_get_child (SvgElement   *element,
                       unsigned int  idx)
{
  return g_ptr_array_index (element->shapes, idx);
}

void
svg_element_add_animation (SvgElement   *shape,
                           SvgAnimation *animation)
{
  if (!shape->animations)
    shape->animations = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_animation_free);
  g_ptr_array_add (shape->animations, animation);
}

void
svg_element_set_base_value (SvgElement  *element,
                            SvgProperty  attr,
                            SvgValue    *value)
{
  g_clear_pointer (&element->base[attr], svg_value_unref);
  if (value)
    element->base[attr] = svg_value_ref (value);
  else
    element->base[attr] = svg_property_ref_initial_value (attr,
                                                          svg_element_get_type (element),
                                                          svg_element_get_parent (element) != NULL);
}

void
svg_element_take_base_value (SvgElement  *element,
                             SvgProperty  attr,
                             SvgValue    *value)
{
  svg_element_set_base_value (element, attr, value);
  if (value)
    svg_value_unref (value);
}

SvgValue *
svg_element_get_base_value (SvgElement  *element,
                            SvgProperty  attr)
{
  return element->base[attr];
}

void
svg_element_set_specified_value (SvgElement  *element,
                                 SvgProperty  attr,
                                 SvgValue    *value)
{
  unsigned int i;
  PropertyValue *v;

  if (_gtk_bitmask_get (element->attrs, attr))
    {
      for (i = 0; i < element->specified->len; i++)
        {
          v = &g_array_index (element->specified, PropertyValue, i);

          if (v->attr == attr)
            {
              g_clear_pointer (&v->value, svg_value_unref);
              break;
            }
        }
    }
  else
    i = element->specified->len;

  if (i == element->specified->len)
    {
      PropertyValue pv = { attr, NULL };
      g_array_append_val (element->specified, pv);
    }

  v = &g_array_index (element->specified, PropertyValue, i);
  g_assert (v->attr == attr);
  g_assert (v->value == NULL);
  if (value)
    v->value = svg_value_ref (value);

  element->attrs = _gtk_bitmask_set (element->attrs, attr, value != NULL);
}

void
svg_element_take_specified_value (SvgElement  *element,
                                  SvgProperty  attr,
                                  SvgValue    *value)
{
  svg_element_set_specified_value (element, attr, value);
  if (value)
    svg_value_unref (value);
}

SvgValue *
svg_element_get_specified_value (SvgElement  *element,
                                 SvgProperty  attr)
{
  for (unsigned int i = 0; i < element->specified->len; i++)
    {
      PropertyValue *v = &g_array_index (element->specified, PropertyValue, i);

      if (v->attr == attr)
        return v->value;
    }

  return NULL;
}

gboolean
svg_element_is_specified (SvgElement  *element,
                          SvgProperty  attr)
{
  return _gtk_bitmask_get (element->attrs, attr);
}

void
svg_element_set_current_value (SvgElement  *element,
                               SvgProperty  attr,
                               SvgValue    *value)
{
  if (value)
    svg_value_ref (value);
  g_clear_pointer (&element->current[attr], svg_value_unref);
  element->current[attr] = value;
}

SvgValue *
svg_element_get_current_value (SvgElement  *element,
                               SvgProperty  attr)
{
  return element->current[attr];
}

SvgElementType
svg_element_get_type (SvgElement *element)
{
  return element->type;
}

void
svg_element_set_id (SvgElement *element,
                    const char *id)
{
  g_set_str (&element->id, id);
  gtk_css_node_set_id (element->css_node, g_quark_from_string (id));
}

const char *
svg_element_get_id (SvgElement *element)
{
  return element->id;
}

void
svg_element_set_style (SvgElement           *element,
                       const char           *style,
                       const GtkSvgLocation *location)
{
  g_set_str (&element->style, style);
  element->style_loc = *location;
}

const char *
svg_element_get_style (SvgElement     *element,
                       GtkSvgLocation *location)
{
  *location = element->style_loc;
  return element->style;
}

void
svg_element_parse_classes (SvgElement *element,
                           const char *classes)
{
  svg_element_take_classes (element, parse_strv (classes));
}

void
svg_element_take_classes (SvgElement *element,
                          GStrv       classes)
{
  g_strfreev (element->classes);
  element->classes = classes;
  gtk_css_node_set_classes (element->css_node, (const char **) element->classes);
}

GStrv
svg_element_get_classes (SvgElement *element)
{
  return element->classes;
}

GtkCssNode *
svg_element_get_css_node (SvgElement *element)
{
  return element->css_node;
}

void
svg_element_set_origin (SvgElement     *element,
                        GtkSvgLocation *location)
{
  element->line = location->lines;
}

void
svg_element_get_origin (SvgElement     *element,
                        GtkSvgLocation *location)
{
  location->lines = element->line;
}

void
svg_element_delete (SvgElement *element)
{
  if (element->text)
    {
      for (unsigned int i = 0; i < element->text->len; i++)
        {
          TextNode *node = &g_array_index (element->text, TextNode, i);
          if (node->type == TEXT_NODE_SHAPE && node->shape.shape == element)
            {
              g_array_remove_index (element->text, i);
              break;
            }
        }
    }

  g_ptr_array_remove (element->parent->shapes, element);
}

SvgElement *
svg_element_get_parent (SvgElement *element)
{
  return element->parent;
}

/* Given two elements, find ancestors
 * such that ancestor0 != ancestor1 and
 * ancestor0->parent == ancestor1->parent
 */
gboolean
svg_element_common_ancestor (SvgElement  *element0,
                             SvgElement  *element1,
                             SvgElement **ancestor0,
                             SvgElement **ancestor1)
{
  SvgElement *parent0, *parent1;
  unsigned int depth0, depth1;

  depth0 = 0;
  parent0 = element0;
  while (parent0->parent)
    {
      parent0 = parent0->parent;
      depth0++;
    }

  depth1 = 0;
  parent1 = element1;
  while (parent1->parent)
    {
      parent1 = parent1->parent;
      depth1++;
    }

  if (parent0 != parent1)
    return FALSE;

  while (depth0 > depth1)
    {
      element0 = element0->parent;
      depth0--;
    }

  while (depth1 > depth0)
    {
      element1 = element1->parent;
      depth1--;
    }

  while (element0->parent != element1->parent)
    {
      element0 = element0->parent;
      element1 = element1->parent;
    }

  g_assert (element0->parent == element1->parent);

  *ancestor0 = element0;
  *ancestor1 = element1;

  return TRUE;
}

uint64_t
svg_element_get_states (SvgElement *element)
{
  return element->gpa.states;
}

void
svg_element_set_states (SvgElement *element,
                        uint64_t    states)
{
  element->gpa.states = states;
}

gboolean
svg_element_equal (SvgElement *element1,
                   SvgElement *element2)
{
  if (element1->type != element2->type ||
      g_strcmp0 (svg_element_get_id (element1), svg_element_get_id (element2)) != 0)
    return FALSE;

  if (g_strcmp0 (svg_element_get_id (element1), svg_element_get_id (element2)) != 0)
    return FALSE;

  if (g_strcmp0 (element1->style, element2->style) != 0)
    return FALSE;

  if (element1->classes && element2->classes)
    {
      if (!g_strv_equal ((const char * const *) element1->classes,
                         (const char * const *) element2->classes))
        return FALSE;
    }
  else
    {
      if (element1->classes != element2->classes)
        return FALSE;
    }

  for (unsigned int i = 0; i < N_SVG_PROPERTIES; i++)
    {
      if (!svg_value_equal (element1->base[i], element2->base[i]))
        return FALSE;
    }

  if (svg_element_type_is_container (element1->type))
    {
      if (element1->shapes->len != element2->shapes->len)
        return FALSE;

      for (unsigned int i = 0; i < element1->shapes->len; i++)
        {
          SvgElement *s1 = g_ptr_array_index (element1->shapes, i);
          SvgElement *s2 = g_ptr_array_index (element2->shapes, i);

          if (!svg_element_equal (s1, s2))
            return FALSE;
        }
    }

  if (svg_element_type_is_gradient (element1->type))
    {
      if (element1->color_stops->len != element2->color_stops->len)
        return FALSE;

      for (unsigned int i = 0; i < element1->color_stops->len; i++)
        {
          SvgColorStop *s1 = g_ptr_array_index (element1->color_stops, i);
          SvgColorStop *s2 = g_ptr_array_index (element2->color_stops, i);

          if (!svg_color_stop_equal (s1, s2))
            return FALSE;
        }
    }

  if (svg_element_type_is_filter (element1->type))
    {
      if (element1->filters->len != element2->filters->len)
        return FALSE;

      for (unsigned int i = 0; i < element1->filters->len; i++)
        {
          SvgFilter *f1 = g_ptr_array_index (element1->filters, i);
          SvgFilter *f2 = g_ptr_array_index (element2->filters, i);

          if (!svg_filter_equal (f1, f2))
            return FALSE;
        }
    }

  if (element1->animations || element2->animations)
    {
      if (!element1->animations || !element2->animations)
        return FALSE;

      if (element1->animations->len != element2->animations->len)
        return FALSE;

      for (unsigned int i = 0; i < element1->animations->len; i++)
        {
          SvgAnimation *a1 = g_ptr_array_index (element1->animations, i);
          SvgAnimation *a2 = g_ptr_array_index (element2->animations, i);

          if (!svg_animation_equal (a1, a2))
            return FALSE;
        }
    }

  return TRUE;
}

void
svg_element_foreach (SvgElement       *element,
                     SvgShapeCallback  callback,
                     gpointer          user_data)
{
  callback (element, user_data);

  if (element->shapes)
    {
      for (unsigned int i = 0; i < element->shapes->len; i++)
        {
          SvgElement *sh = g_ptr_array_index (element->shapes, i);
          svg_element_foreach (sh, callback, user_data);
        }
    }
}

void
svg_element_set_gpa_width (SvgElement *element,
                           SvgValue   *value)
{
  if (element->gpa.width)
    svg_value_unref (element->gpa.width);
  element->gpa.width = svg_value_ref (value);
}

SvgValue *
svg_element_get_gpa_width (SvgElement *element)
{
  return element->gpa.width;
}

void
svg_element_set_gpa_fill (SvgElement *element,
                          SvgValue   *value)
{
  if (element->gpa.fill)
    svg_value_unref (element->gpa.fill);
  element->gpa.fill = svg_value_ref (value);
}

SvgValue *
svg_element_get_gpa_fill (SvgElement *element)
{
  return element->gpa.fill;
}

void
svg_element_set_gpa_stroke (SvgElement *element,
                            SvgValue   *value)
{
  if (element->gpa.stroke)
    svg_value_unref (element->gpa.stroke);
  element->gpa.stroke = svg_value_ref (value);
}

SvgValue *
svg_element_get_gpa_stroke (SvgElement *element)
{
  return element->gpa.stroke;
}

void
svg_element_set_gpa_transition (SvgElement    *element,
                                GpaTransition  transition,
                                GpaEasing      easing,
                                int64_t        duration,
                                int64_t        delay)
{
  element->gpa.transition = transition;
  element->gpa.transition_easing = easing;
  element->gpa.transition_duration = duration;
  element->gpa.transition_delay = delay;
}

void
svg_element_get_gpa_transition (SvgElement    *element,
                                GpaTransition *transition,
                                GpaEasing     *easing,
                                int64_t       *duration,
                                int64_t       *delay)
{
  if (transition)
    *transition = element->gpa.transition;
  if (easing)
    *easing = element->gpa.transition_easing;
  if (duration)
    *duration = element->gpa.transition_duration;
  if (delay)
    *delay = element->gpa.transition_delay;
}

void
svg_element_set_gpa_animation (SvgElement   *element,
                               GpaAnimation  animation,
                               GpaEasing     easing,
                               int64_t       duration,
                               double        repeat,
                               double        segment)
{
  element->gpa.animation = animation;
  element->gpa.animation_easing = easing;
  element->gpa.animation_duration = duration;
  element->gpa.animation_repeat = repeat;
  element->gpa.animation_segment = segment;
}

void
svg_element_get_gpa_animation (SvgElement   *element,
                               GpaAnimation *animation,
                               GpaEasing    *easing,
                               int64_t      *duration,
                               double       *repeat,
                               double       *segment)
{
  if (animation)
    *animation = element->gpa.animation;
  if (easing)
    *easing = element->gpa.animation_easing;
  if (duration)
    *duration = element->gpa.animation_duration;
  if (repeat)
    *repeat = element->gpa.animation_repeat;
  if (segment)
    *segment = element->gpa.animation_segment;
}

void
svg_element_set_gpa_origin (SvgElement *element,
                            double      origin)
{
  element->gpa.origin = origin;
}

double
svg_element_get_gpa_origin (SvgElement *element)
{
  return element->gpa.origin;
}

void
svg_element_set_gpa_attachment (SvgElement *element,
                                const char *id,
                                double      position,
                                SvgElement *shape)
{
  g_set_str (&element->gpa.attach.ref, id);
  element->gpa.attach.pos = position;
  element->gpa.attach.shape = shape;
}

void
svg_element_get_gpa_attachment (SvgElement  *element,
                                const char **id,
                                double      *position,
                                SvgElement **shape)
{
  if (id)
    *id = element->gpa.attach.ref;
  if (position)
    *position = element->gpa.attach.pos;
  if (shape)
    *shape = element->gpa.attach.shape;
}

SvgElement *
svg_element_duplicate (SvgElement *element,
                       SvgElement *parent)
{
  SvgElement *copy = g_new0 (SvgElement, 1);

  copy->type = element->type;
  copy->parent = parent;
  copy->attrs = _gtk_bitmask_copy (element->attrs);
  copy->id = NULL;
  copy->style = g_strdup (element->style);
  copy->classes = g_strdupv (element->classes);
  copy->css_node = gtk_css_node_new ();
  gtk_css_node_set_parent (copy->css_node, parent->css_node);
  gtk_css_node_set_name (copy->css_node, g_quark_from_static_string (svg_element_type_get_name (copy->type)));
  gtk_css_node_set_classes (copy->css_node, (const char **) copy->classes);

  for (unsigned int i = FIRST_SVG_PROPERTY; i <= LAST_SVG_PROPERTY; i++)
    copy->base[i] = svg_value_ref (element->base[i]);

  copy->shapes = g_ptr_array_new ();
  copy->animations = g_ptr_array_new ();
  copy->styles = g_ptr_array_new ();

  copy->gpa.states = element->gpa.states;
  copy->gpa.transition = element->gpa.transition;
  copy->gpa.transition_easing = element->gpa.transition_easing;
  copy->gpa.transition_duration = element->gpa.transition_duration;
  copy->gpa.transition_delay = element->gpa.transition_delay;
  copy->gpa.animation = element->gpa.animation;
  copy->gpa.animation_easing = element->gpa.animation_easing;
  copy->gpa.animation_duration = element->gpa.animation_duration;
  copy->gpa.animation_repeat = element->gpa.animation_repeat;
  copy->gpa.animation_segment = element->gpa.animation_segment;
  copy->gpa.origin = element->gpa.origin;
  copy->gpa.attach.ref = NULL;
  copy->gpa.attach.shape = NULL;
  copy->gpa.attach.pos = 0;

  return copy;
}

void
svg_element_set_type (SvgElement     *element,
                      SvgElementType  type)
{
  SvgElementType old_type;

  if (element->type == type)
    return;

  old_type = element->type;
  element->type = type;

  if (svg_element_type_is_filter (old_type) &&
      !svg_element_type_is_filter (type))
    g_clear_pointer (&element->filters, g_ptr_array_unref);
  else if (!svg_element_type_is_filter (old_type) &&
           svg_element_type_is_filter (type))
    element->filters = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_filter_free);

  if (svg_element_type_is_gradient (old_type) &&
      !svg_element_type_is_gradient (type))
    g_clear_pointer (&element->color_stops, g_ptr_array_unref);
  else if (!svg_element_type_is_gradient (old_type) &&
           svg_element_type_is_gradient (type))
    element->color_stops = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_color_stop_free);

  if (svg_element_type_is_container (old_type) &&
      !svg_element_type_is_container (type))
    g_clear_pointer (&element->shapes, g_ptr_array_unref);
  if (!svg_element_type_is_container (old_type) &&
      svg_element_type_is_container (type))
    element->shapes = g_ptr_array_new_with_free_func ((GDestroyNotify) svg_element_free);

  for (unsigned int attr = FIRST_SHAPE_PROPERTY; attr <= LAST_SHAPE_PROPERTY; attr++)
    {
      if (!svg_property_applies_to (attr, type))
        svg_element_set_base_value (element, attr, NULL);
    }
}

void
svg_element_move_child_down (SvgElement *element,
                             SvgElement *child)
{
  for (unsigned int i = 0; i < element->shapes->len; i++)
    {
      if (g_ptr_array_index (element->shapes, i) == child)
        {
          g_ptr_array_steal_index (element->shapes, i);
          g_ptr_array_insert (element->shapes, i + 1, child);
          return;
        }
    }
}

SvgAnimation *
svg_element_find_animation (SvgElement *element,
                            const char *id)
{
  if (element->animations)
    {
      for (unsigned int i = 0; i < element->animations->len; i++)
        {
          SvgAnimation *a = g_ptr_array_index (element->animations, i);
          if (g_strcmp0 (a->id, id) == 0)
            return a;
        }
    }

  if (svg_element_type_is_container (element->type))
    {
      for (unsigned int i = 0; i < element->shapes->len; i++)
        {
          SvgElement *child = g_ptr_array_index (element->shapes, i);
          SvgAnimation *a = svg_element_find_animation (child, id);
          if (a)
            return a;
        }
    }

  return NULL;
}

void
svg_element_set_autofocus (SvgElement *element,
                           gboolean    autofocus)
{
  element->autofocus = autofocus;
}

gboolean
svg_element_get_autofocus (SvgElement *element)
{
  return element->autofocus;
}

void
svg_element_set_focusable (SvgElement *element,
                           gboolean    focusable)
{
  element->focusable = focusable;
}

gboolean
svg_element_get_focusable (SvgElement *element)
{
  return element->focusable;
}

gboolean
svg_element_get_initial_focusable (SvgElement *element)
{
  if (element->type == SVG_ELEMENT_SVG && element->parent == NULL)
    return TRUE;
  else if (element->type == SVG_ELEMENT_LINK)
    return TRUE;
  else
    return FALSE;
}

static void
svg_element_change_css_state (SvgElement *element,
                              GtkStateFlags add,
                              GtkStateFlags remove)
{
  GtkStateFlags state = gtk_css_node_get_state (element->css_node);
  gtk_css_node_set_state (element->css_node, (state | add) & ~remove);
}

void
svg_element_set_focus (SvgElement *element,
                       gboolean    focus)
{
  if (focus)
    svg_element_change_css_state (element, GTK_STATE_FLAG_FOCUSED, 0);
  else
    svg_element_change_css_state (element, 0, GTK_STATE_FLAG_FOCUSED);
}

gboolean
svg_element_get_focus (SvgElement *element)
{
  return (gtk_css_node_get_state (element->css_node) & GTK_STATE_FLAG_FOCUSED) != 0;
}

void
svg_element_set_active (SvgElement *element,
                        gboolean    active)
{
  if (active)
    svg_element_change_css_state (element, GTK_STATE_FLAG_ACTIVE, 0);
  else
    svg_element_change_css_state (element, 0, GTK_STATE_FLAG_ACTIVE);
}

gboolean
svg_element_get_active (SvgElement *element)
{
  return (gtk_css_node_get_state (element->css_node) & GTK_STATE_FLAG_ACTIVE) != 0;
}

void
svg_element_set_hover (SvgElement *element,
                       gboolean    hover)
{
  if (hover)
    svg_element_change_css_state (element, GTK_STATE_FLAG_PRELIGHT, 0);
  else
    svg_element_change_css_state (element, 0, GTK_STATE_FLAG_PRELIGHT);

  for (element = element->parent; element; element = element->parent)
    {
      if (hover)
        svg_element_change_css_state (element, GTK_STATE_FLAG_PRELIGHT, 0);
      else
        svg_element_change_css_state (element, 0, GTK_STATE_FLAG_PRELIGHT);
    }
}

gboolean
svg_element_get_hover (SvgElement *element)
{
  return (gtk_css_node_get_state (element->css_node) & GTK_STATE_FLAG_PRELIGHT) != 0;
}

void
svg_element_set_visited (SvgElement *element,
                         gboolean    visited)
{
  if (visited)
    svg_element_change_css_state (element, GTK_STATE_FLAG_VISITED, 0);
  else
    svg_element_change_css_state (element, 0, GTK_STATE_FLAG_VISITED);
}

gboolean
svg_element_get_visited (SvgElement *element)
{
  return (gtk_css_node_get_state (element->css_node) & GTK_STATE_FLAG_VISITED) != 0;
}
