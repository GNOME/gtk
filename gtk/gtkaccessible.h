/* gtkaccessible.h: Accessible interface
 *
 * Copyright 2020  GNOME Foundation
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <glib-object.h>
#include <gtk/gtktypes.h>
#include <gtk/gtkenums.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACCESSIBLE (gtk_accessible_get_type())

GDK_AVAILABLE_IN_ALL
G_DECLARE_INTERFACE (GtkAccessible, gtk_accessible, GTK, ACCESSIBLE, GObject)

GDK_AVAILABLE_IN_ALL
GtkAccessibleRole       gtk_accessible_get_accessible_role      (GtkAccessible *self);

GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_update_state             (GtkAccessible      *self,
                                                                 GtkAccessibleState  first_state,
                                                                 ...);
GDK_AVAILABLE_IN_ALL
void                    gtk_accessible_update_state_value       (GtkAccessible      *self,
                                                                 GtkAccessibleState  state,
                                                                 const GValue       *value);

G_END_DECLS
