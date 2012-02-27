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


G_DEFINE_TYPE (GtkBooleanCellAccessible, _gtk_boolean_cell_accessible, GTK_TYPE_RENDERER_CELL_ACCESSIBLE)


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

