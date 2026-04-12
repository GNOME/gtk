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

#include "gdk/gdktexture.h"
#include "gtksvgtypesprivate.h"
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_href_new_none     (void);
SvgValue *   svg_href_new_plain    (const char     *string);
SvgValue *   svg_href_new_url_take (const char     *string);

const char * svg_href_get_id       (const SvgValue *value);
const char * svg_href_get_ref      (const SvgValue *value);
HrefKind     svg_href_get_kind     (const SvgValue *value);
SvgElement * svg_href_get_shape    (const SvgValue *value);
void         svg_href_set_shape    (SvgValue       *value,
                                    SvgElement     *shape);
SvgAnimation * svg_href_get_animation (const SvgValue *value);
void         svg_href_set_animation   (SvgValue       *value,
                                       SvgAnimation    *animation);
GdkTexture * svg_href_get_texture  (const SvgValue *value);
void         svg_href_set_texture  (SvgValue       *value,
                                    GdkTexture     *texture);


G_END_DECLS
