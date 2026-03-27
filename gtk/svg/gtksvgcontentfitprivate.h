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
#include "gtksvgenumsprivate.h"

G_BEGIN_DECLS

SvgValue *   svg_content_fit_new_none    (void);
SvgValue *   svg_content_fit_new         (Align           align_x,
                                          Align           align_y,
                                          MeetOrSlice     meet);
SvgValue *   svg_content_fit_parse       (GtkCssParser   *parser);

gboolean     svg_content_fit_is_none     (const SvgValue *value);
Align        svg_content_fit_get_align_x (const SvgValue *value);
Align        svg_content_fit_get_align_y (const SvgValue *value);
MeetOrSlice  svg_content_fit_get_meet    (const SvgValue *value);

G_END_DECLS
