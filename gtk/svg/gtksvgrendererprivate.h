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

#include "gtksvg.h"
#include "gtksvgtypesprivate.h"
#include "gtksnapshot.h"
#include <graphene.h>
#include "gsk/gsktypes.h"

G_BEGIN_DECLS

void            gtk_svg_snapshot_full (GtkSvg                 *self,
                                       GtkSnapshot            *snapshot,
                                       double                  width,
                                       double                  height,
                                       const GdkRGBA          *colors,
                                       size_t                  n_colors,
                                       double                  weight);

SvgElement *    gtk_svg_pick_element  (GtkSvg                 *self,
                                       const graphene_point_t *point);

GskRenderNode * gtk_svg_apply_filter  (GtkSvg                *self,
                                       const char            *filter,
                                       const graphene_rect_t *bounds,
                                       GskRenderNode         *node);

G_END_DECLS
