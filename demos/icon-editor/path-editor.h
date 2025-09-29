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


#define PATH_EDITOR_TYPE (path_editor_get_type ())
G_DECLARE_FINAL_TYPE (PathEditor, path_editor, PATH, EDITOR, GtkWidget)


PathEditor *    path_editor_new           (PathPaintable *paintable,
                                           size_t         idx);

void            path_editor_set_paintable (PathEditor    *self,
                                           PathPaintable *paintable);
PathPaintable * path_editor_get_paintable (PathEditor    *self);

void            path_editor_set_path      (PathEditor    *self,
                                           size_t         idx);
size_t          path_editor_get_path      (PathEditor    *self);
void            path_editor_edit_path     (PathEditor    *self);
