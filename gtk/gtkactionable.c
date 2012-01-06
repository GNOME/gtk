/*
 * Copyright © 2012 Canonical Limited
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * licence or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,
 * USA.
 *
 * Authors: Ryan Lortie <desrt@desrt.ca>
 */

#include "config.h"

#include "gtkactionable.h"

#include "gtkwidget.h"
#include "gtkintl.h"

G_DEFINE_INTERFACE (GtkActionable, gtk_actionable, GTK_TYPE_WIDGET)

static void
gtk_actionable_default_init (GtkActionableInterface *iface)
{
  g_object_interface_install_property (iface,
    g_param_spec_string ("action-name", P_("action name"),
                         P_("The name of the associated action, like 'app.quit'"),
                         NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_interface_install_property (iface,
    g_param_spec_variant ("action-target", P_("action target value"),
                          P_("The parameter for action invocations"),
                          G_VARIANT_TYPE_ANY, NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

const gchar *
gtk_actionable_get_action_name (GtkActionable *actionable)
{
  g_return_val_if_fail (GTK_IS_ACTIONABLE (actionable), NULL);

  return GTK_ACTIONABLE_GET_IFACE (actionable)
    ->get_action_name (actionable);
}

void
gtk_actionable_set_action_name (GtkActionable *actionable,
                                const gchar   *action_name)
{
  g_return_if_fail (GTK_IS_ACTIONABLE (actionable));

  GTK_ACTIONABLE_GET_IFACE (actionable)
    ->set_action_name (actionable, action_name);
}

GVariant *
gtk_actionable_get_action_target_value (GtkActionable *actionable)
{
  g_return_val_if_fail (GTK_IS_ACTIONABLE (actionable), NULL);

  return GTK_ACTIONABLE_GET_IFACE (actionable)
    ->get_action_target_value (actionable);
}

void
gtk_actionable_set_action_target_value (GtkActionable *actionable,
                                        GVariant      *target_value)
{
  g_return_if_fail (GTK_IS_ACTIONABLE (actionable));

  GTK_ACTIONABLE_GET_IFACE (actionable)
    ->set_action_target_value (actionable, target_value);
}

void
gtk_actionable_set_action_target (GtkActionable *actionable,
                                  const gchar   *format_string,
                                  ...)
{
  va_list ap;

  va_start (ap, format_string);
  gtk_actionable_set_action_target_value (actionable, g_variant_new_va (format_string, NULL, &ap));
  va_end (ap);
}

void
gtk_actionable_set_detailed_action_name (GtkActionable *actionable,
                                         const gchar   *detailed_action_name)
{
  gchar **parts;

  g_return_if_fail (GTK_IS_ACTIONABLE (actionable));

  if (detailed_action_name == NULL)
    {
      gtk_actionable_set_action_name (actionable, NULL);
      gtk_actionable_set_action_target_value (actionable, NULL);
      return;
    }

  parts = g_strsplit (detailed_action_name, "::", 2);
  gtk_actionable_set_action_name (actionable, parts[0]);
  if (parts[0] && parts[1])
    gtk_actionable_set_action_target (actionable, "s", parts[1]);
  else
    gtk_actionable_set_action_target_value (actionable, NULL);
  g_strfreev (parts);
}
