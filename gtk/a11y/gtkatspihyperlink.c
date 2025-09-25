/* gtkatspihyperlink.c: AT-SPI Hyperlink implementation
 *
 * Copyright 2025 Red Hat, Inc.
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

#include "gtkatspihyperlinkprivate.h"

#include "a11y/atspi/atspi-value.h"

#include "gtkaccessiblerangeprivate.h"
#include "gtkaccessiblehypertextprivate.h"
#include "gtkatcontextprivate.h"
#include "gtkatspicontextprivate.h"
#include "gtkdebug.h"

#include <gio/gio.h>

static void
hyperlink_handle_method (GDBusConnection       *connection,
                         const gchar           *sender,
                         const gchar           *object_path,
                         const gchar           *interface_name,
                         const gchar           *method_name,
                         GVariant              *parameters,
                         GDBusMethodInvocation *invocation,
                         gpointer               user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkAccessibleHyperlink *hyperlink = GTK_ACCESSIBLE_HYPERLINK (accessible);

  if (g_strcmp0 (method_name, "GetObject") == 0)
    {
      int index;

      g_variant_get (parameters, "(i)", &index);
      if (index != 0)
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_ARGUMENT,
                                                 "Index out of range %d", index);
        }
      else
        {
          GtkAccessible *parent = gtk_accessible_get_accessible_parent (accessible);
          GtkATContext *parent_context = gtk_accessible_get_at_context (parent);
          GVariant *ref = gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (parent_context));
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@(so))", ref));
          g_object_unref (parent_context);
          g_object_unref (parent);
        }
    }
  else if (g_strcmp0 (method_name, "GetURI") == 0)
    {
      int index;

      g_variant_get (parameters, "(i)", &index);
      if (index != 0)
        {
          g_dbus_method_invocation_return_error (invocation,
                                                 G_IO_ERROR,
                                                 G_IO_ERROR_INVALID_ARGUMENT,
                                                 "Index out of range %d", index);
        }
      else
        {
          const char *uri;

          uri = gtk_accessible_hyperlink_get_uri (hyperlink);
          g_dbus_method_invocation_return_value (invocation, g_variant_new ("(s)", uri));
        }
    }
  else if (g_strcmp0 (method_name, "IsValid") == 0)
    {
      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(b)", TRUE));
    }
}

static GVariant *
hyperlink_handle_get_property (GDBusConnection  *connection,
                               const gchar      *sender,
                               const gchar      *object_path,
                               const gchar      *interface_name,
                               const gchar      *property_name,
                               GError          **error,
                               gpointer          user_data)
{
  GtkATContext *self = user_data;
  GtkAccessible *accessible = gtk_at_context_get_accessible (self);
  GtkAccessibleHyperlink *hyperlink = GTK_ACCESSIBLE_HYPERLINK (accessible);
  GtkAccessibleTextRange bounds;

  gtk_accessible_hyperlink_get_extents (hyperlink, &bounds);

  if (g_strcmp0 (property_name, "NAnchors") == 0)
    {
      return g_variant_new_int16 (1);
    }
  else if (g_strcmp0 (property_name, "StartIndex") == 0)
    {
      return g_variant_new_int32 ((gint32) bounds.start);
    }
  else if (g_strcmp0 (property_name, "EndIndex") == 0)
    {
      return g_variant_new_int32 ((gint32) bounds.start + bounds.length);
    }

  g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
               "Unknown property '%s'", property_name);

  return NULL;
}

static const GDBusInterfaceVTable hyperlink_vtable = {
  hyperlink_handle_method,
  hyperlink_handle_get_property,
  NULL,
};

const GDBusInterfaceVTable *
gtk_atspi_get_hyperlink_vtable (GtkAccessible *accessible)
{
  if (GTK_IS_ACCESSIBLE_HYPERLINK (accessible))
    return &hyperlink_vtable;

  return NULL;
}
