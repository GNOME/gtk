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
#include "gtksvgprivate.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgunitprivate.h"

G_BEGIN_DECLS

SvgValue *    svg_dash_array_new_none   (void);
SvgValue *    svg_dash_array_new        (double         *values,
                                         unsigned int    n_values);
SvgValue *    svg_dash_array_parse      (GtkCssParser   *parser);

DashArrayKind svg_dash_array_get_kind   (const SvgValue *value);
unsigned int  svg_dash_array_get_length (const SvgValue *value);
SvgUnit       svg_dash_array_get_unit   (const SvgValue *value,
                                         unsigned int    pos);
double        svg_dash_array_get        (const SvgValue *value,
                                         unsigned int    pos,
                                         double          one_hundred_percent);

G_END_DECLS
