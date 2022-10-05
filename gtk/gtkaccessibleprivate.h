/* gtkaccessibleprivate.h: Accessible interface
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

#include "gtkaccessible.h"
#include "gtkatcontextprivate.h"

G_BEGIN_DECLS

struct _GtkAccessibleInterface
{
  GTypeInterface g_iface;

  GtkATContext *        (* get_at_context)      (GtkAccessible *self);

  gboolean              (* get_platform_state)  (GtkAccessible              *self,
                                                 GtkAccessiblePlatformState  state);
};

GtkATContext *  gtk_accessible_get_at_context   (GtkAccessible *self);

const char *    gtk_accessible_role_to_name     (GtkAccessibleRole  role,
                                                 const char        *domain);

gboolean gtk_accessible_role_is_range_subclass (GtkAccessibleRole role);

gboolean        gtk_accessible_should_present   (GtkAccessible     *self);

void            gtk_accessible_platform_changed (GtkAccessible                *self,
                                                 GtkAccessiblePlatformChange   change);
gboolean        gtk_accessible_get_platform_state (GtkAccessible              *self,
                                                   GtkAccessiblePlatformState  state);

void            gtk_accessible_bounds_changed   (GtkAccessible *self);

void            gtk_accessible_update_children  (GtkAccessible           *self,
                                                 GtkAccessible           *child,
                                                 GtkAccessibleChildState  state);

G_END_DECLS
