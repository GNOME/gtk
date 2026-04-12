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
#include "gtksvg.h"
#include "gtksvgtypesprivate.h"
#include "gtk/gtkcssnodeprivate.h"

G_BEGIN_DECLS

SvgColorStop * svg_color_stop_new               (SvgElement     *parent);

SvgColorStop * svg_color_stop_clone             (SvgColorStop   *stop,
                                                 SvgElement     *parent);

void           svg_color_stop_free              (SvgColorStop   *stop);

unsigned int   svg_color_stop_get_index         (SvgProperty     attr);
SvgProperty    svg_color_stop_get_property      (unsigned int    idx);

void           svg_color_stop_set_specified_value (SvgColorStop   *stop,
                                                   SvgProperty     attr,
                                                   SvgValue       *value);
void           svg_color_stop_take_specified_value (SvgColorStop   *stop,
                                                    SvgProperty     attr,
                                                    SvgValue       *value);
SvgValue *     svg_color_stop_get_specified_value (SvgColorStop   *stop,
                                                   SvgProperty     attr);
gboolean       svg_color_stop_is_specified        (SvgColorStop   *stop,
                                                   SvgProperty     attr);

void           svg_color_stop_set_base_value    (SvgColorStop   *stop,
                                                 SvgProperty     attr,
                                                 SvgValue       *value);
SvgValue *     svg_color_stop_get_base_value    (SvgColorStop   *stop,
                                                 SvgProperty     attr);

void           svg_color_stop_set_current_value (SvgColorStop   *stop,
                                                 SvgProperty     attr,
                                                 SvgValue       *value);
SvgValue *     svg_color_stop_get_current_value (SvgColorStop   *stop,
                                                 SvgProperty     attr);

void           svg_color_stop_set_id            (SvgColorStop   *stop,
                                                 const char     *id);
const char *   svg_color_stop_get_id            (SvgColorStop   *stop);

void           svg_color_stop_set_style         (SvgColorStop   *stop,
                                                 const char     *style);
const char *   svg_color_stop_get_style         (SvgColorStop   *stop);

void           svg_color_stop_parse_classes     (SvgColorStop   *stop,
                                                 const char     *classes);
void           svg_color_stop_take_classes      (SvgColorStop   *stop,
                                                 GStrv           classes);
GStrv          svg_color_stop_get_classes       (SvgColorStop   *stop);

void           svg_color_stop_set_origin        (SvgColorStop   *stop,
                                                 GtkSvgLocation *location);
void           svg_color_stop_get_origin        (SvgColorStop   *stop,
                                                 GtkSvgLocation *location);

GtkCssNode *   svg_color_stop_get_css_node      (SvgColorStop   *stop);

gboolean       svg_color_stop_equal             (SvgColorStop   *stop1,
                                                 SvgColorStop   *stop2);

GArray *       svg_color_stop_get_inline_styles (SvgColorStop   *stop);

G_END_DECLS
