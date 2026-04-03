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
#include "gtksvgvalueprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

gboolean      svg_path_data_parse_full  (const char   *string,
                                         SvgPathData **path_data);
SvgPathData * svg_path_data_parse       (const char   *string);
void          svg_path_data_free        (SvgPathData  *path_data);

SvgPathData * svg_path_data_from_gsk    (GskPath      *path);
GskPath *     svg_path_data_to_gsk      (SvgPathData  *path_data);

SvgPathData * svg_path_data_interpolate (SvgPathData  *path_data0,
                                         SvgPathData  *path_data1,
                                         double        t);

void svg_path_data_print                (SvgPathData  *path_data,
                                         GString      *string);
