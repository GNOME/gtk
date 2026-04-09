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
#include "gsk/gskcomponenttransfer.h"
#include "gtk/gtkcssnodeprivate.h"

G_BEGIN_DECLS

SvgFilter *    svg_filter_new               (SvgElement     *parent,
                                             SvgFilterType   type);

void           svg_filter_free              (SvgFilter      *filter);

SvgFilterType  svg_filter_get_type          (SvgFilter      *filter);

SvgValue *     svg_filter_ref_initial_value (SvgFilter      *filter,
                                             SvgProperty     attr);

void           svg_filter_set_specified_value (SvgFilter      *filter,
                                               SvgProperty     attr,
                                               SvgValue       *value);
void           svg_filter_take_specified_value (SvgFilter      *filter,
                                                SvgProperty     attr,
                                                SvgValue       *value);
SvgValue *     svg_filter_get_specified_value (SvgFilter      *filter,
                                               SvgProperty     attr);
gboolean       svg_filter_is_specified        (SvgFilter      *filter,
                                               SvgProperty     attr);

void           svg_filter_set_base_value    (SvgFilter      *filter,
                                             SvgProperty     attr,
                                             SvgValue       *value);
SvgValue *     svg_filter_get_base_value    (SvgFilter      *filter,
                                             SvgProperty     attr);

void           svg_filter_set_current_value (SvgFilter      *filter,
                                             SvgProperty     attr,
                                             SvgValue       *value);
SvgValue *     svg_filter_get_current_value (SvgFilter      *filter,
                                             SvgProperty     attr);

void           svg_filter_set_id            (SvgFilter      *filter,
                                             const char     *id);
const char *   svg_filter_get_id            (SvgFilter      *filter);

void           svg_filter_set_style         (SvgFilter      *filter,
                                             const char     *style);
const char *   svg_filter_get_style         (SvgFilter      *filter);

void           svg_filter_parse_classes     (SvgFilter      *filter,
                                             const char     *classes);
void           svg_filter_take_classes      (SvgFilter      *filter,
                                             GStrv           classes);
GStrv          svg_filter_get_classes       (SvgFilter      *filter);

void           svg_filter_set_origin        (SvgFilter      *filter,
                                             GtkSvgLocation *location);
void           svg_filter_get_origin        (SvgFilter      *filter,
                                             GtkSvgLocation *location);

GtkCssNode *   svg_filter_get_css_node      (SvgFilter      *filter);

GskComponentTransfer *
               svg_filter_get_component_transfer (SvgFilter *filter);

gboolean       svg_filter_needs_backdrop    (SvgFilter      *filter);

gboolean       filter_needs_backdrop        (SvgElement     *shape);

gboolean       svg_filter_equal             (SvgFilter      *filter1,
                                             SvgFilter      *filter2);

GArray *       svg_filter_get_inline_styles (SvgFilter      *filter);

G_END_DECLS
