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

#if !defined (__GTK_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include <gsk/gsk.h>
#include <gtk/gtksnapshot.h>


#define GTK_PATH_PAINTABLE_TYPE (gtk_path_paintable_get_type ())

GDK_AVAILABLE_IN_4_22
GDK_DECLARE_INTERNAL_TYPE (GtkPathPaintable, gtk_path_paintable, GTK, PATH_PAINTABLE, GObject)

GDK_AVAILABLE_IN_4_22
GtkPathPaintable * gtk_path_paintable_new_from_bytes    (GBytes            *bytes,
                                                         GError           **error);

GDK_AVAILABLE_IN_4_22
GtkPathPaintable * gtk_path_paintable_new_from_resource (const char        *path);

/**
 * GTK_PATH_PAINTABLE_STATE_EMPTY:
 *
 * Represents the empty state for [class@Gtk.PathPaintable].
 *
 * Since: 4.22
 */
#define GTK_PATH_PAINTABLE_STATE_EMPTY ((unsigned int) -1)

GDK_AVAILABLE_IN_4_22
void                gtk_path_paintable_set_state        (GtkPathPaintable  *self,
                                                         unsigned int       state);
GDK_AVAILABLE_IN_4_22
unsigned int        gtk_path_paintable_get_state        (GtkPathPaintable  *self);
GDK_AVAILABLE_IN_4_22
unsigned int        gtk_path_paintable_get_max_state    (GtkPathPaintable  *self);
GDK_AVAILABLE_IN_4_22
void                gtk_path_paintable_set_weight       (GtkPathPaintable  *self,
                                                         float              weight);
GDK_AVAILABLE_IN_4_22
float               gtk_path_paintable_get_weight       (GtkPathPaintable  *self);

