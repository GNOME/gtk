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

void         svg_filter_type_init         (void);

gboolean     svg_filter_type_lookup       (const char    *name,
                                           SvgFilterType *result);

const char * svg_filter_type_get_name     (SvgFilterType type);
unsigned int svg_filter_type_get_n_attrs  (SvgFilterType type);
SvgProperty  svg_filter_type_get_property (SvgFilterType  type,
                                           unsigned int   idx);
gboolean     svg_filter_type_has_property (SvgFilterType  type,
                                           SvgProperty    attr);
unsigned int svg_filter_type_get_index    (SvgFilterType  type,
                                           SvgProperty    attr);

G_END_DECLS
