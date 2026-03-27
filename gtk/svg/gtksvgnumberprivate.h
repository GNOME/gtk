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

#include "gtksvgvalueprivate.h"
#include "gtk/css/gtkcssparserprivate.h"
#include "gtksvgprivate.h"
#include <graphene.h>

G_BEGIN_DECLS

typedef enum
{
  SVG_PARSE_NUMBER     = 1 << 0,
  SVG_PARSE_PERCENTAGE = 1 << 1,
  SVG_PARSE_LENGTH     = 1 << 2,
  SVG_PARSE_TIME       = 1 << 3,
  SVG_PARSE_ANGLE      = 1 << 4,
} SvgNumberParseFlags;

SvgValue * svg_number_new               (double               value);
SvgValue * svg_number_new_full          (SvgUnit              unit,
                                         double               value);

SvgValue * svg_percentage_new           (double               value);
gboolean   svg_number_parse2            (GtkCssParser        *parser,
                                         double               min,
                                         double               max,
                                         SvgNumberParseFlags  flags,
                                         double              *d,
                                         SvgUnit             *u);

SvgValue * svg_number_parse             (GtkCssParser        *parser,
                                         double               min,
                                         double               max,
                                         SvgNumberParseFlags  flags);

SvgUnit    svg_number_get_unit          (const SvgValue      *value);
double     svg_number_get               (const SvgValue      *value,
                                         double               one_hundred_percent);
gboolean   svg_value_is_number          (const SvgValue      *value);
gboolean   svg_value_is_positive_number (const SvgValue      *value);


const char *svg_unit_name (SvgUnit unit);
SvgDimension svg_unit_dimension (SvgUnit unit);

gboolean is_absolute_length          (SvgUnit                unit);

double   absolute_length_to_px       (double                 value,
                                      SvgUnit                unit);

double   shape_get_current_font_size (Shape                 *shape,
                                      ShapeAttr              attr,
                                      SvgComputeContext     *context);
double   angle_to_deg                (double                 value,
                                      SvgUnit                unit);
double   viewport_relative_to_px     (double                 value,
                                      SvgUnit                unit,
                                      const graphene_rect_t *viewport);

G_END_DECLS
