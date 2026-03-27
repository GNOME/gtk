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
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"
#include "gsk/gsktransform.h"

SvgValue * svg_transform_new_none      (void);
SvgValue * svg_transform_new_translate (double x, double y);
SvgValue * svg_transform_new_scale     (double x, double y);
SvgValue * svg_transform_new_rotate    (double angle, double x, double y);
SvgValue * svg_transform_new_skew_x    (double angle);
SvgValue * svg_transform_new_skew_y    (double angle);
SvgValue * svg_transform_new_matrix    (double params[6]);
SvgValue * svg_transform_new_rotate_and_shift
                                       (double            angle,
                                        graphene_point_t *orig,
                                        graphene_point_t *final);

SvgValue * svg_transform_parse         (const char    *value);
SvgValue * svg_transform_parse_css     (GtkCssParser  *parser);

SvgValue * primitive_transform_parse       (TransformType   type,
                                            const char     *value);
void       svg_primitive_transform_print   (const SvgValue *value,
                                            GString        *string);

GskTransform * svg_transform_get_gsk       (const SvgValue *value);

unsigned int   svg_transform_get_length    (const SvgValue *value);
SvgValue *     svg_transform_get_transform (const SvgValue *value,
                                            unsigned int    pos);
TransformType  svg_transform_get_type      (const SvgValue *value,
                                            unsigned int    pos);
gboolean       svg_transform_is_none       (const SvgValue *value);
TransformType  svg_transform_get_primitive (const SvgValue *value,
                                            unsigned int    pos,
                                            double          params[6]);
gboolean       svg_value_is_transform      (const SvgValue *value);


typedef struct
{
  double value;
  SvgUnit unit;
} ParserNumber;

unsigned int css_parser_parse_number (GtkCssParser *parser,
                                      unsigned int  n,
                                      gpointer      data);

unsigned int css_parser_parse_number_length (GtkCssParser *parser,
                                             unsigned int  n,
                                             gpointer      data);

unsigned int css_parser_parse_number_angle (GtkCssParser *parser,
                                            unsigned int  n,
                                            gpointer      data);

unsigned int css_parser_parse_number_percentage (GtkCssParser *parser,
                                                 unsigned int  n,
                                                 gpointer      data);

G_END_DECLS

