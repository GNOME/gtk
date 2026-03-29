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
#include "gdk/gdkcolorprivate.h"
#include "gtksvgprivate.h"
#include "gtkenums.h"
#include "gdkrgba.h"
#include "gtksvgvalueprivate.h"
#include "gtksvgenumsprivate.h"

SvgValue *   svg_paint_new_none            (void);
SvgValue *   svg_paint_new_symbolic        (GtkSymbolicColor  symbolic);
SvgValue *   svg_paint_new_rgba            (const GdkRGBA    *rgba);
SvgValue *   svg_paint_new_color           (const GdkColor   *color);
SvgValue *   svg_paint_new_black           (void);
SvgValue *   svg_paint_new_transparent     (void);
SvgValue *   svg_paint_new_simple          (PaintKind         kind);
SvgValue *   svg_paint_new_server          (const char       *ref);
SvgValue *   svg_paint_new_server_with_fallback
                                           (const char       *ref,
                                            const GdkColor   *fallback);
SvgValue *   svg_paint_new_server_with_current_color
                                           (const char       *ref);

SvgValue *   svg_paint_parse               (GtkCssParser     *parser);
SvgValue *   svg_paint_parse_gpa           (const char       *value);
void         svg_paint_print_gpa           (const SvgValue   *value,
                                            GString          *string);

gboolean     svg_value_is_paint            (const SvgValue   *value);
PaintKind    svg_paint_get_kind            (const SvgValue   *value);
GtkSymbolicColor
             svg_paint_get_symbolic        (const SvgValue   *value);
gboolean     svg_paint_is_symbolic         (const SvgValue   *value,
                                            GtkSymbolicColor *symbolic);
const GdkColor *
             svg_paint_get_color           (const SvgValue   *value);
const char * svg_paint_get_server_ref      (const SvgValue   *value);
SvgElement * svg_paint_get_server_shape    (const SvgValue *value);
void         svg_paint_set_server_shape    (SvgValue         *value,
                                            SvgElement       *shape);
const GdkColor *
             svg_paint_get_server_fallback (const SvgValue   *value);

gboolean     paint_is_server               (PaintKind         kind);
gboolean     parse_symbolic_color          (const char       *string,
                                            GtkSymbolicColor *sym);

G_END_DECLS
