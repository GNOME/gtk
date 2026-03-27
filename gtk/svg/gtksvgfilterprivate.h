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

#include "gtk/css/gtkcssparserprivate.h"
#include <graphene.h>
#include "gtksvgprivate.h"
#include "gtksvgenumprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgenumsprivate.h"

SvgValue *   svg_filter_new_none         (void);
SvgValue *   svg_filter_parse_css        (GtkCssParser      *parser);
SvgValue *   svg_filter_parse            (const char        *value);

gboolean     svg_filter_is_none          (const SvgValue    *value);

unsigned int svg_filter_get_length       (const SvgValue    *value);
FilterKind   svg_filter_get_kind         (const SvgValue    *value,
                                          unsigned int       pos);
const char * svg_filter_get_ref          (const SvgValue    *value,
                                          unsigned int       pos);
Shape *      svg_filter_get_shape        (const SvgValue    *value,
                                          unsigned int       pos);
void         svg_filter_set_shape        (SvgValue          *value,
                                          unsigned int       pos,
                                          Shape             *shape);
double       svg_filter_get_simple       (const SvgValue    *value,
                                          unsigned int       pos);
void         svg_filter_get_color_matrix (const SvgValue    *value,
                                          unsigned int       pos,
                                          graphene_matrix_t *matrix,
                                          graphene_vec4_t   *offset);
void         svg_filter_get_dropshadow   (const SvgValue    *value,
                                          unsigned int       pos,
                                          GdkColor          *color,
                                          double            *dx,
                                          double            *dy,
                                          double            *std_dev);

gboolean     svg_filter_needs_backdrop   (const SvgValue    *value);

void color_matrix_type_get_color_matrix  (ColorMatrixType    type,
                                          SvgValue          *values,
                                          graphene_matrix_t *matrix,
                                          graphene_vec4_t   *offset);

gboolean filter_needs_backdrop           (Shape             *filter);


G_END_DECLS
