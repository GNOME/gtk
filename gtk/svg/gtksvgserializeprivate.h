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
#include "gtksvgprivate.h"
#include "gtksvganimationprivate.h"
#include "gtksvgutilsprivate.h"

G_BEGIN_DECLS

/*< private >
 * GtkSvgSerializeFlags:
 * @GTK_SVG_SERIALIZE_DEFAULT: Default behavior. Serialize
 *   the DOM, with gpa attributes, and with compatibility
 *   tweaks
 * @GTK_SVG_SERIALIZE_AT_CURRENT_TIME: Serialize the current
 *   values of a running animation, as opposed to the DOM
 *   values that the parser produced
 * @GTK_SVG_SERIALIZE_INCLUDE_STATE: Include custom attributes
 *   with various information about the state of the renderer,
 *   such as the current time, or the status of running animations
 * @GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS: Instead of gpa attributes,
 *   include the animations that were generated from them
 * @GTK_SVG_SERIALIZE_NO_COMPAT: Don't include things that
 *   improve the rendering of the serialized result in renderers
 *   which don't support extensions, but stick to the pristine
 *   DOM
 */
typedef enum
{
  GTK_SVG_SERIALIZE_DEFAULT            = 0,
  GTK_SVG_SERIALIZE_AT_CURRENT_TIME    = 1 << 0,
  GTK_SVG_SERIALIZE_EXCLUDE_ANIMATION  = 1 << 1,
  GTK_SVG_SERIALIZE_INCLUDE_STATE      = 1 << 2,
  GTK_SVG_SERIALIZE_EXPAND_GPA_ATTRS   = 1 << 3,
  GTK_SVG_SERIALIZE_NO_COMPAT          = 1 << 4,
} GtkSvgSerializeFlags;

GBytes *       gtk_svg_serialize_full  (GtkSvg                *self,
                                        const GdkRGBA         *colors,
                                        size_t                 n_colors,
                                        GtkSvgSerializeFlags   flags);

G_END_DECLS
