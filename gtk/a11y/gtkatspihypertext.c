/* gtkatspihypertext.c: AT-SPI Hypertext implementation
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

#include "gtkatspihypertextprivate.h"

#include "a11y/atspi/atspi-value.h"

#include "gtkaccessiblerangeprivate.h"
#include "gtkatcontextprivate.h"
#include "gtkatspicontextprivate.h"
#include "gtkaccessiblehypertextprivate.h"
#include "gtkdebug.h"

#include <gio/gio.h>

static void
hypertext_handle_method (GDBusConnection       *connection,
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
  GtkAccessibleHypertext *hypertext = GTK_ACCESSIBLE_HYPERTEXT (accessible);

  if (g_strcmp0 (method_name, "GetNLinks") == 0)
    {
      unsigned int n = gtk_accessible_hypertext_get_n_links (hypertext);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", n));
    }
  else if (g_strcmp0 (method_name, "GetLink") == 0)
    {
      int index;
      GtkAccessibleHyperlink *link;
      GtkATContext *ctx;
      GVariant *ref;

      g_variant_get (parameters, "(i)", &index);

      link = gtk_accessible_hypertext_get_link (hypertext, index);
      ctx = gtk_accessible_get_at_context (GTK_ACCESSIBLE (link));
      ref = gtk_at_spi_context_to_ref (GTK_AT_SPI_CONTEXT (ctx));

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(@(so))", ref));

      g_object_unref (ctx);
    }
  else if (g_strcmp0 (method_name, "GetLinkIndex") == 0)
    {
      int offset, index;

      g_variant_get (parameters, "(i)", &offset);
      index = gtk_accessible_hypertext_get_link_at (hypertext, offset);

      g_dbus_method_invocation_return_value (invocation, g_variant_new ("(i)", index));
    }
}

static const GDBusInterfaceVTable hypertext_vtable = {
  hypertext_handle_method,
  NULL,
  NULL,
};

const GDBusInterfaceVTable *
gtk_atspi_get_hypertext_vtable (GtkAccessible *accessible)
{
  if (GTK_IS_ACCESSIBLE_HYPERTEXT (accessible))
    return &hypertext_vtable;

  return NULL;
}
