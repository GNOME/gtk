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

#include "gsk/gskpath.h"
#include "gsk/gsktransform.h"
#include "gsk/gskroundedrect.h"
#include "gtk/gtksnapshot.h"
#include "gtk/gtkenums.h"


GskPath * svg_transform_path (GskTransform *transform,
                              GskPath      *path);

typedef enum
{
  PATH_EMPTY,
  PATH_RECT,
  PATH_ROUNDED_RECT,
  PATH_CIRCLE,
  PATH_GENERAL,
} SvgPathClassification;

SvgPathClassification svg_path_classify (GskPath        *path,
                                         GskRoundedRect *rect);

void svg_snapshot_push_fill (GtkSnapshot *snapshot,
                             GskPath     *path,
                             GskFillRule  rule);
