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
#include "gtksvgunitprivate.h"
#include "gtksvgprivate.h"

G_BEGIN_DECLS

SvgValue *  svg_orient_new_angle         (double          angle,
                                          SvgUnit         unit);
SvgValue *  svg_orient_new_auto          (gboolean        start_reverse);
SvgValue *  svg_orient_parse             (GtkCssParser   *parser);

OrientKind  svg_orient_get_kind          (const SvgValue *value);
gboolean    svg_orient_get_start_reverse (const SvgValue *value);
double      svg_orient_get_angle         (const SvgValue *value);

G_END_DECLS
