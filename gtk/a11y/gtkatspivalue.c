/* gtkatspivalue.c: AT-SPI Value implementation
 *
 * Copyright 2020 Red Hat, Inc.
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

#include "gtkatspivalueprivate.h"

#include "a11y/atspi/atspi-value.h"

#include "gtkaccessiblerange.h"
#include "gtkatcontextprivate.h"
#include "gtkdebug.h"

#include <gio/gio.h>

static GVariant *
handle_value_get_property (GDBusConnection  *connection,
                           const gchar      *sender,
                           const gchar      *object_path,
                           const gchar      *interface_name,
                           const gchar      *property_name,
                           GError          **error,
                           gpointer          user_data)
{
  GtkATContext *ctx = GTK_AT_CONTEXT (user_data);
  struct {
    const char *name;
    GtkAccessibleProperty property;
  } properties[] = {
    { "MinimumValue", GTK_ACCESSIBLE_PROPERTY_VALUE_MIN },
    { "MaximumValue", GTK_ACCESSIBLE_PROPERTY_VALUE_MAX },
    { "CurrentValue", GTK_ACCESSIBLE_PROPERTY_VALUE_NOW },
  };
  int i;

  for (i = 0; i < G_N_ELEMENTS (properties); i++)
    {
      if (g_strcmp0 (property_name,  properties[i].name) == 0)
        {
          if (gtk_at_context_has_accessible_property (ctx, properties[i].property))
            {
              GtkAccessibleValue *value;

              value = gtk_at_context_get_accessible_property (ctx, properties[i].property);
              return g_variant_new_double (gtk_number_accessible_value_get (value));
            }
        }
    }

  if (g_strcmp0 (property_name, "MinimumIncrement") == 0)
    {
      GtkAccessibleRange *range = GTK_ACCESSIBLE_RANGE (gtk_at_context_get_accessible (ctx));
      return g_variant_new_double(gtk_accessible_range_get_minimum_increment (range));
  }
  /* fall back for widgets that should have the
   * properties but don't
   */
  return g_variant_new_double (0.0);
}

static gboolean
handle_value_set_property (GDBusConnection  *connection,
                           const gchar      *sender,
                           const gchar      *object_path,
                           const gchar      *interface_name,
                           const gchar      *property_name,
                           GVariant         *value,
                           GError          **error,
                           gpointer          user_data)
{
  GtkATContext *self = user_data;
  GtkAccessibleRange *range = GTK_ACCESSIBLE_RANGE (gtk_at_context_get_accessible (self));

  if (g_strcmp0 (property_name, "CurrentValue") == 0)
    {
      return gtk_accessible_range_set_current_value (range, g_variant_get_double (value));
    }

  return FALSE;
}

static const GDBusInterfaceVTable value_vtable = {
  NULL,
  handle_value_get_property,
  handle_value_set_property,
};

const GDBusInterfaceVTable *
gtk_atspi_get_value_vtable (GtkAccessible *accessible)
{
  if (GTK_IS_ACCESSIBLE_RANGE (accessible))
    return &value_vtable;

  return NULL;
}

