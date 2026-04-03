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

#include <glib.h>
#include "gtk/svg/gtksvgunitprivate.h"

static struct {
  const char *name;
  SvgDimension dimension;
} units_data[] = {
  [SVG_UNIT_NUMBER] = { "", SVG_DIMENSION_NUMBER },
  [SVG_UNIT_PERCENTAGE] = { "%", SVG_DIMENSION_NUMBER },
  [SVG_UNIT_PX] = { "px", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_PT] = { "pt", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_IN] = { "in", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_CM] = { "cm", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_MM] = { "mm", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_VW] = { "vw", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_VH] = { "vh", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_VMIN] = { "vmin", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_VMAX] = { "vmax", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_EM] = { "em", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_EX] = { "ex", SVG_DIMENSION_LENGTH },
  [SVG_UNIT_S] = { "s", SVG_DIMENSION_TIME },
  [SVG_UNIT_MS] = { "ms", SVG_DIMENSION_TIME },
  [SVG_UNIT_DEG] = { "deg", SVG_DIMENSION_ANGLE },
  [SVG_UNIT_RAD] = { "rad", SVG_DIMENSION_ANGLE },
  [SVG_UNIT_GRAD] = { "grad", SVG_DIMENSION_ANGLE },
  [SVG_UNIT_TURN] = { "turn", SVG_DIMENSION_ANGLE },
};

unsigned int
svg_units_get_length (void)
{
  return G_N_ELEMENTS (units_data);
}

const char *
svg_unit_name (SvgUnit unit)
{
  return units_data[unit].name;
}

SvgDimension
svg_unit_dimension (SvgUnit unit)
{
  return units_data[unit].dimension;
}
