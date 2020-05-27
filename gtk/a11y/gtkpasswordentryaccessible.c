/* gtkpasswordentryaccessible.c: A GtkWidgetAccessible for GtkPasswordEntry
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

#include "config.h"

#include "gtkpasswordentryaccessible.h"

#include "gtkintl.h"

struct _GtkPasswordEntryAccessible
{
  GtkEntryAccessible parent_instance;
};

G_DEFINE_TYPE (GtkPasswordEntryAccessible, gtk_password_entry_accessible, GTK_TYPE_ENTRY_ACCESSIBLE)

static void
gtk_password_entry_accessible_class_init (GtkPasswordEntryAccessibleClass *klass)
{
}

static void
gtk_password_entry_accessible_init (GtkPasswordEntryAccessible *self)
{
  AtkObject *atk_obj = ATK_OBJECT (self);

  atk_obj->role = ATK_ROLE_PASSWORD_TEXT;
  atk_object_set_name (atk_obj, _("Password"));
}
