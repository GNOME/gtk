/* GAIL - The GNOME Accessibility Enabling Library
 * Copyright 2001 Sun Microsystems Inc.
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

#include <gtk/gtk.h>
#include "gtkbooleancellaccessible.h"

static AtkActionIface *parent_action_iface;

static gint
gtk_boolean_cell_accessible_get_n_actions (AtkAction *action)
{
  return parent_action_iface->get_n_actions (action) + 1;
}

static const gchar *
gtk_boolean_cell_accessible_get_description (AtkAction *action,
                                             gint       i)
{
  if (i == 0)
    return "toggles the cell";

  return parent_action_iface->get_description (action, i - 1);
}

static const gchar *
gtk_boolean_cell_accessible_action_get_name (AtkAction *action,
                                             gint       i)
{
  if (i == 0)
    return "toggle";

  return parent_action_iface->get_description (action, i - 1);
}

static gboolean
gtk_boolean_cell_accessible_do_action (AtkAction *action,
                                       gint       i)
{
  if (i == 0)
    return parent_action_iface->do_action (action, 2);
  else
    return parent_action_iface->do_action (action, i - 1);
}

static void
gtk_boolean_cell_accessible_action_interface_init (AtkActionIface *iface)
{
  parent_action_iface = g_type_interface_peek_parent (iface);

  iface->do_action = gtk_boolean_cell_accessible_do_action;
  iface->get_n_actions = gtk_boolean_cell_accessible_get_n_actions;
  iface->get_description = gtk_boolean_cell_accessible_get_description;
  iface->get_name = gtk_boolean_cell_accessible_action_get_name;
}


G_DEFINE_TYPE_EXTENDED (GtkBooleanCellAccessible, _gtk_boolean_cell_accessible, GTK_TYPE_RENDERER_CELL_ACCESSIBLE, 0,
                        G_IMPLEMENT_INTERFACE (ATK_TYPE_ACTION, gtk_boolean_cell_accessible_action_interface_init))


static AtkStateSet *
gtk_boolean_cell_accessible_ref_state_set (AtkObject *accessible)
{
  GtkBooleanCellAccessible *cell = GTK_BOOLEAN_CELL_ACCESSIBLE (accessible);
  AtkStateSet *state_set;

  state_set = ATK_OBJECT_CLASS (_gtk_boolean_cell_accessible_parent_class)->ref_state_set (accessible);

  if (cell->cell_value)
    atk_state_set_add_state (state_set, ATK_STATE_CHECKED);

  if (cell->cell_sensitive)
    atk_state_set_add_state (state_set, ATK_STATE_SENSITIVE);
  else
    atk_state_set_remove_state (state_set, ATK_STATE_SENSITIVE);

  return state_set;
}

static void
gtk_boolean_cell_accessible_update_cache (GtkCellAccessible *cell)
{
  GtkBooleanCellAccessible *boolean_cell = GTK_BOOLEAN_CELL_ACCESSIBLE (cell);
  gboolean active;
  gboolean sensitive;

  g_object_get (G_OBJECT (GTK_RENDERER_CELL_ACCESSIBLE (cell)->renderer),
                "active", &active,
                "sensitive", &sensitive,
                NULL);

  if (boolean_cell->cell_value != active)
    {
      boolean_cell->cell_value = !boolean_cell->cell_value;

      atk_object_notify_state_change (ATK_OBJECT (cell), ATK_STATE_CHECKED, active);
    }

  if (boolean_cell->cell_sensitive != sensitive)
    {
      boolean_cell->cell_sensitive = !boolean_cell->cell_sensitive;

      atk_object_notify_state_change (ATK_OBJECT (cell), ATK_STATE_CHECKED, sensitive);
    }
}

static void
_gtk_boolean_cell_accessible_class_init (GtkBooleanCellAccessibleClass *klass)
{
  GtkCellAccessibleClass *cell_class = GTK_CELL_ACCESSIBLE_CLASS (klass);
  AtkObjectClass *atkobject_class = ATK_OBJECT_CLASS (klass);

  atkobject_class->ref_state_set = gtk_boolean_cell_accessible_ref_state_set;

  cell_class->update_cache = gtk_boolean_cell_accessible_update_cache;
}

static void
_gtk_boolean_cell_accessible_init (GtkBooleanCellAccessible *cell)
{
}

