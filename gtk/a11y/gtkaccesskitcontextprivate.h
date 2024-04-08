/* gtkaccesskitcontextprivate.h: AccessKit GtkATContext implementation
 *
 * Copyright 2024  GNOME Foundation
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

#include "gtkatcontextprivate.h"

#include <accesskit.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACCESSKIT_CONTEXT (gtk_accesskit_context_get_type())

G_DECLARE_FINAL_TYPE (GtkAccessKitContext, gtk_accesskit_context, GTK, ACCESSKIT_CONTEXT, GtkATContext)

GtkATContext *
gtk_accesskit_create_context (GtkAccessibleRole  accessible_role,
                              GtkAccessible     *accessible,
                              GdkDisplay        *display);

accesskit_node_id
gtk_accesskit_context_get_id (GtkAccessKitContext *self);

accesskit_node *
gtk_accesskit_context_build_node (GtkAccessKitContext      *self,
                                  accesskit_node_class_set *node_classes);

G_END_DECLS
