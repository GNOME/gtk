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


#define PATH_PAINTABLE_TYPE (path_paintable_get_type ())
G_DECLARE_FINAL_TYPE (PathPaintable, path_paintable, PATH, PAINTABLE, GObject)


PathPaintable * path_paintable_new_from_bytes (GBytes         *bytes,
                                               GError        **error);

GBytes *        path_paintable_serialize      (PathPaintable  *self);

gboolean        path_paintable_equal          (PathPaintable  *self,
                                               PathPaintable  *other);

#define STATE_UNSET ((guint) -1)

void            path_paintable_set_state      (PathPaintable  *self,
                                               guint           state);
guint           path_paintable_get_max_state  (PathPaintable  *self);
