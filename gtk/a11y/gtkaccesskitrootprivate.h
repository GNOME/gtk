/* gtkaccesskitrootprivate.h: AccessKit root object
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

#include "gtkaccesskitcontextprivate.h"
#include "gtkroot.h"

#include <accesskit.h>

G_BEGIN_DECLS

#define GTK_TYPE_ACCESSKIT_ROOT (gtk_accesskit_root_get_type())

G_DECLARE_FINAL_TYPE (GtkAccessKitRoot, gtk_accesskit_root, GTK, ACCESSKIT_ROOT, GObject)

GtkAccessKitRoot *
gtk_accesskit_root_new (GtkRoot *root_widget);

guint32
gtk_accesskit_root_new_id (GtkAccessKitRoot *self);

guint32
gtk_accesskit_root_add_context (GtkAccessKitRoot    *self,
                                GtkAccessKitContext *context);

void
gtk_accesskit_root_remove_context (GtkAccessKitRoot *self, guint32 id);

void
gtk_accesskit_root_queue_update (GtkAccessKitRoot *self,
                                 guint32           id,
                                 gboolean          force_to_end);

void
gtk_accesskit_root_update_tree (GtkAccessKitRoot *self);

void
gtk_accesskit_root_update_window_focus_state (GtkAccessKitRoot *self,
                                              gboolean          focused);

G_END_DECLS
