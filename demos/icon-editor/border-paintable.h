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

#include <gtk/gtk.h>

#include "path-paintable.h"


#define BORDER_PAINTABLE_TYPE (border_paintable_get_type ())
G_DECLARE_FINAL_TYPE (BorderPaintable, border_paintable, BORDER, PAINTABLE, GObject)


BorderPaintable * border_paintable_new             (void);

void              border_paintable_set_paintable   (BorderPaintable *self,
                                                    PathPaintable   *paintable);
PathPaintable *   border_paintable_get_paintable   (BorderPaintable *self);

void              border_paintable_set_show_bounds (BorderPaintable *self,
                                                    gboolean         show_bounds);
gboolean          border_paintable_get_show_bounds (BorderPaintable *self);

void              border_paintable_set_show_spines (BorderPaintable *self,
                                                    gboolean         show_spines);
gboolean          border_paintable_get_show_spines (BorderPaintable *self);
