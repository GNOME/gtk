/* gtkshortcutscolumn.c
 *
 * Copyright (C) 2015 Christian Hergert <christian@hergert.me>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "gtkshortcutscolumnprivate.h"
#include "gtkorientable.h"


struct _GtkShortcutsColumn
{
  GtkBox parent_instance;
};

struct _GtkShortcutsColumnClass
{
  GtkBoxClass parent_class;
};

G_DEFINE_TYPE (GtkShortcutsColumn, gtk_shortcuts_column, GTK_TYPE_BOX)

static void
gtk_shortcuts_column_class_init (GtkShortcutsColumnClass *klass)
{
}

static void
gtk_shortcuts_column_init (GtkShortcutsColumn *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_box_set_spacing (GTK_BOX (self), 22);
}
