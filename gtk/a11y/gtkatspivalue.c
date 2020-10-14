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

#include "gtkatcontextprivate.h"
#include "gtkdebug.h"
#include "gtklevelbar.h"
#include "gtkpaned.h"
#include "gtkprogressbar.h"
#include "gtkrange.h"
#include "gtkscalebutton.h"
#include "gtkspinbutton.h"

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

  /* fall back for a) MinimumIncrement b) widgets that should have the
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
  GtkWidget *widget = GTK_WIDGET (gtk_at_context_get_accessible (self));

  if (g_strcmp0 (property_name, "CurrentValue") == 0)
    {
      /* we only allow setting values if that is part of the user-exposed
       * functionality of the widget.
       */
      if (GTK_IS_RANGE (widget))
        gtk_range_set_value (GTK_RANGE (widget), g_variant_get_double (value));
      else if (GTK_IS_PANED (widget))
        gtk_paned_set_position (GTK_PANED (widget), (int)(g_variant_get_double (value) + 0.5));
      else if (GTK_IS_SPIN_BUTTON (widget))
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), g_variant_get_double (value));
      else if (GTK_IS_SCALE_BUTTON (widget))
        gtk_scale_button_set_value (GTK_SCALE_BUTTON (widget), g_variant_get_double (value));
      return TRUE;
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
  if (GTK_IS_LEVEL_BAR (accessible) ||
      GTK_IS_PANED (accessible) ||
      GTK_IS_PROGRESS_BAR (accessible) ||
      GTK_IS_RANGE (accessible) ||
      GTK_IS_SCALE_BUTTON (accessible) ||
      GTK_IS_SPIN_BUTTON (accessible))
    return &value_vtable;

  return NULL;
}

