/* GTK+ - accessibility implementations
 * Copyright 2012 Red Hat, Inc
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include "gtkcolorswatchaccessibleprivate.h"

static void atk_action_interface_init (AtkActionIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkColorSwatchAccessible, _gtk_color_swatch_accessible, GTK_TYPE_WIDGET_ACCESSIBLE,
                         G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, atk_action_interface_init))

static void
_gtk_color_swatch_accessible_class_init (GtkColorSwatchAccessibleClass *klass)
{
}

static void
_gtk_color_swatch_accessible_init (GtkColorSwatchAccessible *scale)
{
}

static gint
gtk_color_swatch_accessible_get_n_actions (AtkAction *action)
{
  return 3;
}

static const gchar *
gtk_color_swatch_accessible_get_keybinding (AtkAction *action,
                                            gint       i)
{
  return NULL;
}

static const gchar *
gtk_color_swatch_accessible_get_name (AtkAction *action,
                                      gint       i)
{
  switch (i)
    {
    case 0: return "select";
    case 1: return "activate";
    case 2: return "customize";
    default: return NULL;
    }
}

static const gchar *
gtk_color_swatch_accessible_get_localized_name (AtkAction *action,
                                                gint       i)
{
  switch (i)
    {
    case 0: return C_("Action name", "Select");
    case 1: return C_("Action name", "Activate");
    case 2: return C_("Action name", "Customize");
    default: return NULL;
    }
}

static const gchar *
gtk_color_swatch_accessible_get_description (AtkAction *action,
                                             gint       i)
{
  switch (i)
    {
    case 0: return C_("Action description", "Selects the color");
    case 1: return C_("Action description", "Activates the color");
    case 2: return C_("Action description", "Customizes the color");
    default: return NULL;
    }
}

static gboolean
gtk_color_swatch_accessible_do_action (AtkAction *action,
                                       gint       i)
{
  GtkWidget *widget;

  widget = gtk_accessible_get_widget (GTK_ACCESSIBLE (action));
  if (widget == NULL)
    return FALSE;

  switch (i)
    {
    case 0:
      gtk_widget_set_state_flags (widget, GTK_STATE_FLAG_SELECTED, FALSE);
      break;

    case 1:
      g_signal_emit_by_name (widget, "activate");
      break;

    case 2:
      g_signal_emit_by_name (widget, "customize");
      break;

    default:
      return FALSE;
    }

  return TRUE;
}

static void
atk_action_interface_init (AtkActionIface *iface)
{
  iface->do_action = gtk_color_swatch_accessible_do_action;
  iface->get_n_actions = gtk_color_swatch_accessible_get_n_actions;
  iface->get_keybinding = gtk_color_swatch_accessible_get_keybinding;
  iface->get_name = gtk_color_swatch_accessible_get_name;
  iface->get_localized_name = gtk_color_swatch_accessible_get_localized_name;
  iface->get_description = gtk_color_swatch_accessible_get_description;
}
