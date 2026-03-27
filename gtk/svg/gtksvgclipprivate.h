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
#include "gsk/gskenums.h"
#include "gsk/gskpath.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_clip_new_none      (void);
SvgValue *   svg_clip_new_path      (const char     *string,
                                     unsigned int    fill_rule);
SvgValue *   svg_clip_new_url_take  (const char     *string);

SvgValue *   svg_clip_parse         (GtkCssParser   *parser);

ClipKind     svg_clip_get_kind      (const SvgValue *value);
const char * svg_clip_get_id        (const SvgValue *value);
Shape *      svg_clip_get_shape     (const SvgValue *value);
GskFillRule  svg_clip_get_fill_rule (const SvgValue *value);
GskPath *    svg_clip_get_path      (const SvgValue *value);

void         svg_clip_set_shape     (SvgValue       *value,
                                     Shape          *shape);

G_END_DECLS
