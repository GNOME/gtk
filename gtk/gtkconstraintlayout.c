/* gtkconstraintlayout.c: Layout manager using constraints
 * Copyright 2019  GNOME Foundation
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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Emmanuele Bassi
 */

/**
 * SECTION: gtkconstraintlayout
 * @Title: GtkConstraintLayout
 * @Short_description: A layout manager using constraints
 *
 * GtkConstraintLayout is a layout manager that uses relations between
 * widget attributes, expressed via #GtkConstraint instances, to measure
 * and allocate widgets.
 */

#include "config.h"

#include "gtkconstraintlayout.h"

#include "gtkdebug.h"
#include "gtkprivate.h"

G_DEFINE_TYPE (GtkConstraintLayout, gtk_constraint_layout, GTK_TYPE_LAYOUT_MANAGER)

static void
gtk_constraint_layout_class_init (GtkConstraintLayoutClass *klass)
{
}

static void
gtk_constraint_layout_init (GtkConstraintLayout *self)
{
}

GtkLayoutManager *
gtk_constraint_layout_new (void)
{
  return g_object_new (GTK_TYPE_CONSTRAINT_LAYOUT, NULL);
}
