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

G_BEGIN_DECLS

typedef enum
{
  DEFAULT_SOURCE,
  SOURCE_GRAPHIC,
  SOURCE_ALPHA,
  BACKGROUND_IMAGE,
  BACKGROUND_ALPHA,
  FILL_PAINT,
  STROKE_PAINT,
  PRIMITIVE_REF,
} SvgFilterPrimitiveRefType;

SvgValue * svg_filter_primitive_ref_new     (SvgFilterPrimitiveRefType  type);
SvgValue * svg_filter_primitive_ref_new_ref (const char                *ref);
SvgValue * svg_filter_primitive_ref_parse   (GtkCssParser              *parser);

SvgFilterPrimitiveRefType  svg_filter_primitive_ref_get_type  (const SvgValue *value);
const char *               svg_filter_primitive_ref_get_ref   (const SvgValue *value);

G_END_DECLS
