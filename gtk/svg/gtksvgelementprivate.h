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

#pragma once

#pragma once

#include <glib.h>
#include <graphene.h>
#include "gtksvg.h"
#include "gtkenums.h"
#include "gtkbitmaskprivate.h"
#include "gtkcssnodeprivate.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"
#include "gtk/svg/gtksvgelementtypeprivate.h"
#include "gtk/svg/gtksvgfiltertypeprivate.h"
#include "gtk/svg/gtksvgpropertyprivate.h"
#include "gsk/gskpath.h"
#include "gsk/gskpathmeasure.h"
#include "gtk/gtkcssnodeprivate.h"

G_BEGIN_DECLS

void         svg_element_free                (SvgElement            *element);
SvgElement * svg_element_new                 (SvgElement            *parent,
                                              SvgElementType         type);

void         svg_element_set_type            (SvgElement            *element,
                                              SvgElementType         type);

SvgElementType
             svg_element_get_type            (SvgElement            *element);
void         svg_element_add_color_stop      (SvgElement            *element,
                                              SvgColorStop          *stop);
void         svg_element_add_filter          (SvgElement            *element,
                                              SvgFilter             *filter);
void         svg_element_add_child           (SvgElement            *element,
                                              SvgElement            *child);
void         svg_element_add_animation       (SvgElement            *element,
                                              SvgAnimation          *animation);

unsigned int svg_element_get_n_children      (SvgElement            *element);
SvgElement * svg_element_get_child           (SvgElement            *element,
                                              unsigned int           idx);
void         svg_element_move_child_down     (SvgElement            *element,
                                              SvgElement            *child);

void         svg_element_set_specified_value (SvgElement            *element,
                                              SvgProperty            attr,
                                              SvgValue              *value);
void         svg_element_take_specified_value (SvgElement            *element,
                                               SvgProperty            attr,
                                               SvgValue              *value);
SvgValue *   svg_element_get_specified_value (SvgElement            *element,
                                              SvgProperty            attr);
gboolean     svg_element_is_specified        (SvgElement            *element,
                                              SvgProperty            attr);
void         svg_element_set_base_value      (SvgElement            *element,
                                              SvgProperty            attr,
                                              SvgValue              *value);
void         svg_element_take_base_value     (SvgElement            *element,
                                              SvgProperty            attr,
                                              SvgValue              *value);
SvgValue *   svg_element_get_base_value      (SvgElement            *element,
                                              SvgProperty            attr);
void         svg_element_set_current_value   (SvgElement            *element,
                                              SvgProperty            attr,
                                              SvgValue              *value);
SvgValue *   svg_element_get_current_value   (SvgElement            *element,
                                              SvgProperty            attr);

void         svg_element_set_id              (SvgElement            *element,
                                              const char            *id);
const char * svg_element_get_id              (SvgElement            *element);
void         svg_element_set_style           (SvgElement            *element,
                                              const char            *style,
                                              const GtkSvgLocation  *location);
const char * svg_element_get_style           (SvgElement            *element,
                                              GtkSvgLocation        *location);
void         svg_element_parse_classes       (SvgElement            *element,
                                              const char            *classes);
void         svg_element_take_classes        (SvgElement            *element,
                                              GStrv                  classes);
GStrv        svg_element_get_classes         (SvgElement            *element);
GtkCssNode * svg_element_get_css_node        (SvgElement            *element);

void         svg_element_set_origin          (SvgElement            *element,
                                              GtkSvgLocation        *location);
void         svg_element_get_origin          (SvgElement            *element,
                                              GtkSvgLocation        *location);

GskPath *    svg_element_get_path            (SvgElement            *element,
                                              const graphene_rect_t *viewport,
                                              gboolean               current);
GskPath *    svg_element_get_current_path    (SvgElement            *element,
                                              const graphene_rect_t *viewport);
GskPathMeasure *
             svg_element_get_current_measure (SvgElement            *element,
                                              const graphene_rect_t *viewport);

gboolean     svg_element_get_current_bounds  (SvgElement            *element,
                                              const graphene_rect_t *viewport,
                                              GtkSvg                *svg,
                                              graphene_rect_t       *bounds);
gboolean     svg_element_get_current_stroke_bounds
                                             (SvgElement            *element,
                                              const graphene_rect_t *viewport,
                                              GtkSvg                *svg,
                                              graphene_rect_t       *bounds);
GskStroke *  svg_element_create_basic_stroke (SvgElement            *element,
                                              const graphene_rect_t *viewport,
                                              gboolean               allow_gpa,
                                              double                 weight);

SvgElement * svg_element_get_parent          (SvgElement            *element);
gboolean     svg_element_common_ancestor     (SvgElement            *element0,
                                              SvgElement            *element1,
                                              SvgElement           **ancestor0,
                                              SvgElement           **ancestor1);

gboolean     svg_element_conditionally_excluded
                                             (SvgElement            *element,
                                              GtkSvg                *svg);

void         svg_element_delete              (SvgElement            *element);

void         svg_element_set_states          (SvgElement            *element,
                                              uint64_t               states);
uint64_t     svg_element_get_states          (SvgElement            *element);

void         svg_element_set_autofocus       (SvgElement            *element,
                                              gboolean               autofocus);
gboolean     svg_element_get_autofocus       (SvgElement            *element);

void         svg_element_set_focusable       (SvgElement            *element,
                                              gboolean               focusable);
gboolean     svg_element_get_focusable       (SvgElement            *element);

gboolean     svg_element_get_initial_focusable
                                             (SvgElement            *element);

void         svg_element_set_focus           (SvgElement            *element,
                                              gboolean               focus);
gboolean     svg_element_get_focus           (SvgElement            *element);
void         svg_element_set_active          (SvgElement            *element,
                                              gboolean               active);
gboolean     svg_element_get_active          (SvgElement            *element);
void         svg_element_set_hover           (SvgElement            *element,
                                              gboolean               hover);
gboolean     svg_element_get_hover           (SvgElement            *element);
void         svg_element_set_visited         (SvgElement            *element,
                                              gboolean               hover);
gboolean     svg_element_get_visited         (SvgElement            *element);

void         svg_element_set_gpa_width       (SvgElement            *element,
                                              SvgValue              *value);
SvgValue *   svg_element_get_gpa_width       (SvgElement            *element);
void         svg_element_set_gpa_fill        (SvgElement            *element,
                                              SvgValue              *value);
SvgValue *   svg_element_get_gpa_fill        (SvgElement            *element);
void         svg_element_set_gpa_stroke      (SvgElement            *element,
                                              SvgValue              *value);
SvgValue *   svg_element_get_gpa_stroke      (SvgElement            *element);
void         svg_element_set_gpa_transition  (SvgElement            *element,
                                              GpaTransition          transition,
                                              GpaEasing              easing,
                                              int64_t                duration,
                                              int64_t                delay);
void         svg_element_get_gpa_transition  (SvgElement            *element,
                                              GpaTransition         *transition,
                                              GpaEasing             *easing,
                                              int64_t               *duration,
                                              int64_t               *delay);
void         svg_element_set_gpa_animation   (SvgElement            *element,
                                              GpaAnimation           animation,
                                              GpaEasing              easing,
                                              int64_t                duration,
                                              double                 repeat,
                                              double                 segment);
void         svg_element_get_gpa_animation   (SvgElement            *element,
                                              GpaAnimation          *animation,
                                              GpaEasing             *easing,
                                              int64_t               *duration,
                                              double                *repeat,
                                              double                *segment);
void         svg_element_set_gpa_origin      (SvgElement            *element,
                                              double                 origin);
double       svg_element_get_gpa_origin      (SvgElement            *element);
void         svg_element_set_gpa_attachment  (SvgElement            *element,
                                              const char            *id,
                                              double                 position,
                                              SvgElement            *sh);
void         svg_element_get_gpa_attachment  (SvgElement            *element,
                                              const char           **id,
                                              double                *position,
                                              SvgElement           **sh);

gboolean     svg_element_equal               (SvgElement            *element1,
                                              SvgElement            *element2);

SvgElement * svg_element_duplicate           (SvgElement            *element,
                                              SvgElement            *parent);

typedef void (* SvgShapeCallback) (SvgElement    *shape,
                                   gpointer  user_data);

void
svg_element_foreach (SvgElement       *element,
                     SvgShapeCallback  callback,
                     gpointer          user_data);

SvgAnimation *svg_element_find_animation (SvgElement *element,
                                          const char *id);

G_END_DECLS
