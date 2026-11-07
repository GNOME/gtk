/* gtkaccessiblechildindexprivate.h
 *
 * Copyright 2026 Christian Hergert <christian@sourceandstack.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "gtkaccessible.h"
#include "gtkwidget.h"

G_BEGIN_DECLS

void           gtk_widget_accessible_child_index_remove          (GtkWidget     *parent,
                                                                  GtkWidget     *child);
void           gtk_widget_accessible_child_index_reposition      (GtkWidget     *parent,
                                                                  GtkWidget     *child,
                                                                  GtkWidget     *previous_sibling);
void           gtk_widget_clear_accessible_child_index           (GtkWidget     *widget);
int            gtk_widget_get_accessible_child_index             (GtkWidget     *parent,
                                                                  GtkWidget     *child);
void           gtk_widget_invalidate_accessible_child_index      (GtkWidget     *parent);
void           gtk_widget_update_accessible_child_index          (GtkWidget     *child);
GtkAccessible *gtk_widget_accessible_get_accessible_parent       (GtkAccessible *self);
GtkAccessible *gtk_widget_accessible_get_first_accessible_child  (GtkAccessible *self);
GtkAccessible *gtk_widget_accessible_get_next_accessible_sibling (GtkAccessible *self);

G_END_DECLS
