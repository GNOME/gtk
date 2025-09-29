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


#define PAINTABLE_EDITOR_TYPE (paintable_editor_get_type ())
G_DECLARE_FINAL_TYPE (PaintableEditor, paintable_editor, PAINTABLE, EDITOR, GtkWidget)


PaintableEditor * paintable_editor_new           (void);

void              paintable_editor_set_paintable (PaintableEditor *self,
                                                  PathPaintable   *paintable);
PathPaintable *   paintable_editor_get_paintable (PaintableEditor *self);
void              paintable_editor_add_path      (PaintableEditor *self);
