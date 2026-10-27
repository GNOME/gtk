/* gtkactionkeyprivate.h
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

#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _GtkActionKey           GtkActionKey;
typedef struct _GtkActionInvocationKey GtkActionInvocationKey;

GtkActionKey           *gtk_action_key_new                   (const char             *full_name);
GtkActionKey           *gtk_action_key_ref                   (GtkActionKey           *self);
void                    gtk_action_key_unref                 (GtkActionKey           *self);
const char             *gtk_action_key_get_full_name         (GtkActionKey           *self);
const char             *gtk_action_key_get_local_name        (GtkActionKey           *self);
gsize                   gtk_action_key_get_prefix_length     (GtkActionKey           *self);
guint                   gtk_action_key_hash                  (gconstpointer           data);
gboolean                gtk_action_key_equal                 (gconstpointer           a,
                                                              gconstpointer           b);
guint                   gtk_action_key_pool_get_size         (void);
GtkActionInvocationKey *gtk_action_invocation_key_new        (GtkActionKey           *action_key,
                                                              GVariant               *target);
GtkActionInvocationKey *gtk_action_invocation_key_ref        (GtkActionInvocationKey *self);
void                    gtk_action_invocation_key_unref      (GtkActionInvocationKey *self);
GtkActionKey           *gtk_action_invocation_key_get_action (GtkActionInvocationKey *self);
GVariant               *gtk_action_invocation_key_get_target (GtkActionInvocationKey *self);
guint                   gtk_action_invocation_key_hash       (gconstpointer           data);
gboolean                gtk_action_invocation_key_equal      (gconstpointer           a,
                                                              gconstpointer           b);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkActionKey, gtk_action_key_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkActionInvocationKey, gtk_action_invocation_key_unref)

G_END_DECLS
