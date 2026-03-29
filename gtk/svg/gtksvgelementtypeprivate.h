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

#include <glib.h>
#include "gtksvgtypesprivate.h"

G_BEGIN_DECLS

void         svg_element_type_init                 (void);
gboolean     svg_element_type_lookup               (const char     *name,
                                                    SvgElementType *result);

const char * svg_element_type_get_name             (SvgElementType  type);
gboolean     svg_element_type_is_container         (SvgElementType  type);
gboolean     svg_element_type_is_path              (SvgElementType  type);
gboolean     svg_element_type_is_text              (SvgElementType  type);
gboolean     svg_element_type_is_gradient          (SvgElementType  type);
gboolean     svg_element_type_is_filter            (SvgElementType  type);
gboolean     svg_element_type_never_rendered       (SvgElementType  type);
gboolean     svg_element_type_is_clip_path_content (SvgElementType  type);

G_END_DECLS
