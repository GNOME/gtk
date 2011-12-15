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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <gtk/gtk.h>
#include "gtkbooleancellaccessible.h"


G_DEFINE_TYPE (GtkBooleanCellAccessible, _gtk_boolean_cell_accessible, GTK_TYPE_RENDERER_CELL_ACCESSIBLE)


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

      if (active)
        _gtk_cell_accessible_add_state (GTK_CELL_ACCESSIBLE (cell), ATK_STATE_CHECKED, TRUE);
      else
        _gtk_cell_accessible_remove_state (GTK_CELL_ACCESSIBLE (cell), ATK_STATE_CHECKED, TRUE);
    }

  if (boolean_cell->cell_sensitive != sensitive)
    {
      boolean_cell->cell_sensitive = !boolean_cell->cell_sensitive;

      if (sensitive)
        _gtk_cell_accessible_add_state (GTK_CELL_ACCESSIBLE (cell), ATK_STATE_SENSITIVE, TRUE);
      else
        _gtk_cell_accessible_remove_state (GTK_CELL_ACCESSIBLE (cell), ATK_STATE_SENSITIVE, TRUE);
    }
}

static void
_gtk_boolean_cell_accessible_class_init (GtkBooleanCellAccessibleClass *klass)
{
  GtkCellAccessibleClass *cell_class = GTK_CELL_ACCESSIBLE_CLASS (klass);

  cell_class->update_cache = gtk_boolean_cell_accessible_update_cache;
}

static void
_gtk_boolean_cell_accessible_init (GtkBooleanCellAccessible *cell)
{
}

