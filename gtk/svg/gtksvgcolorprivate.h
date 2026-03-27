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
#include "gdk/gdkcolorprivate.h"
#include "gtk/gtkenums.h"

G_BEGIN_DECLS

typedef enum
{
  COLOR_CURRENT,
  COLOR_SYMBOLIC,
  COLOR_PLAIN,
} ColorKind;

SvgValue * svg_color_new_symbolic    (GtkSymbolicColor  symbolic);
SvgValue * svg_color_new_current     (void);
SvgValue * svg_color_new_black       (void);
SvgValue * svg_color_new_transparent (void);
SvgValue * svg_color_new_color       (const GdkColor   *color);
SvgValue * svg_color_new_rgba        (const GdkRGBA    *rgba);
SvgValue * svg_color_parse           (GtkCssParser     *parser);

ColorKind  svg_color_get_kind        (const SvgValue   *value);
GtkSymbolicColor
           svg_color_get_symbolic    (const SvgValue   *value);
const GdkColor *
           svg_color_get_color       (const SvgValue   *value);

const char *symbolic_system_color    (GtkSymbolicColor c);

G_END_DECLS
