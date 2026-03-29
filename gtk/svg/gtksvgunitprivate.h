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

G_BEGIN_DECLS

typedef enum
{
  SVG_UNIT_NUMBER,
  SVG_UNIT_PERCENTAGE,
  SVG_UNIT_PX,
  SVG_UNIT_PT,
  SVG_UNIT_IN,
  SVG_UNIT_CM,
  SVG_UNIT_MM,
  SVG_UNIT_VW,
  SVG_UNIT_VH,
  SVG_UNIT_VMIN,
  SVG_UNIT_VMAX,
  SVG_UNIT_EM,
  SVG_UNIT_EX,
  SVG_UNIT_S,
  SVG_UNIT_MS,
  SVG_UNIT_DEG,
  SVG_UNIT_RAD,
  SVG_UNIT_GRAD,
  SVG_UNIT_TURN,
} SvgUnit;

typedef enum
{
  SVG_DIMENSION_NUMBER,
  SVG_DIMENSION_LENGTH,
  SVG_DIMENSION_TIME,
  SVG_DIMENSION_ANGLE,
} SvgDimension;


unsigned int  svg_units_get_length (void);
const char   *svg_unit_name        (SvgUnit unit);
SvgDimension  svg_unit_dimension   (SvgUnit unit);

G_END_DECLS
