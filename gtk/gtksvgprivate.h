/*
 * Copyright © 2025 Red Hat, Inc
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
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 * Authors: Matthias Clasen <mclasen@redhat.com>
 */

#pragma once

#include "gtksvg.h"

G_BEGIN_DECLS

void           gtk_svg_set_load_time   (GtkSvg                *self,
                                        int64_t                load_time);

void           gtk_svg_set_playing     (GtkSvg                *self,
                                        gboolean               playing);

void           gtk_svg_advance         (GtkSvg                *self,
                                        int64_t                current_time);

typedef enum
{
  GTK_SVG_RUN_MODE_STOPPED,
  GTK_SVG_RUN_MODE_DISCRETE,
  GTK_SVG_RUN_MODE_CONTINUOUS,
} GtkSvgRunMode;

GtkSvgRunMode  gtk_svg_get_run_mode    (GtkSvg *self);

int64_t        gtk_svg_get_next_update (GtkSvg *self);

typedef enum
{
  GTK_SVG_SERIALIZE_DEFAULT           = 0,
  GTK_SVG_SERIALIZE_AT_CURRENT_TIME   = 1 << 0,
  GTK_SVG_SERIALIZE_EXCLUDE_ANIMATION = 1 << 1,
  GTK_SVG_SERIALIZE_INCLUDE_STATE     = 1 << 2,
} GtkSvgSerializeFlags;

GBytes *       gtk_svg_serialize_full  (GtkSvg                *self,
                                        GtkSvgSerializeFlags   flags);

G_END_DECLS
