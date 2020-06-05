/* gtkpasswordentryaccessible.h: A GtkWidgetAccessible for GtkPasswordEntry
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

#if !defined (__GTK_A11Y_H_INSIDE__) && !defined (GTK_COMPILATION)
#error "Only <gtk/gtk-a11y.h> can be included directly."
#endif

#include <gtk/a11y/gtkentryaccessible.h>

G_BEGIN_DECLS

#define GTK_TYPE_PASSWORD_ENTRY_ACCESSIBLE      (gtk_password_entry_accessible_get_type())
#define GTK_PASSWORD_ENTRY_ACCESSIBLE(obj)      (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_PASSWORD_ENTRY_ACCESSIBLE, GtkPasswordEntryAccessible))
#define GTK_IS_PASSWORD_ENTRY_ACCESSIBLE(obj)   (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_PASSWORD_ENTRY_ACCESSIBLE))

typedef struct _GtkPasswordEntryAccessible      GtkPasswordEntryAccessible;
typedef struct _GtkPasswordEntryAccessibleClass GtkPasswordEntryAccessibleClass;

struct _GtkPasswordEntryAccessibleClass
{
  GtkWidgetAccessibleClass parent_class;
};

GDK_AVAILABLE_IN_ALL
GType gtk_password_entry_accessible_get_type (void) G_GNUC_CONST;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtkPasswordEntryAccessible, g_object_unref)

G_END_DECLS
