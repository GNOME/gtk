/*
 * Copyright © Red Hat
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the licence, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "gtkrestorable.h"

G_BEGIN_DECLS

GtkSaveContext *gtk_save_context_new_root (GFile *bulk_dir);

GVariant *gtk_save_context_serialize (GtkSaveContext *self);

GtkRestoreContext *gtk_restore_context_new (GVariant         *state,
                                            GtkRestoreReason  reason);

gboolean gtk_restorable_check_state_dirty (GtkRestorable *restorable);

G_END_DECLS
