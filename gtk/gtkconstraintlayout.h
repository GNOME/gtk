/* gtkconstraintlayout.h: Layout manager using constraints
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
#pragma once

#include <gtk/gtklayoutmanager.h>
#include <gtk/gtkconstraint.h>

G_BEGIN_DECLS

#define GTK_TYPE_CONSTRAINT_LAYOUT (gtk_constraint_layout_get_type ())
#define GTK_TYPE_CONSTRAINT_LAYOUT_CHILD (gtk_constraint_layout_child_get_type ())

/**
 * GtkConstraintLayout:
 *
 * A layout manager using #GtkConstraint to describe
 * relations between widgets.
 */
GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkConstraintLayout, gtk_constraint_layout, GTK, CONSTRAINT_LAYOUT, GtkLayoutManager)

GDK_AVAILABLE_IN_ALL
GtkLayoutManager *      gtk_constraint_layout_new               (void);

GDK_AVAILABLE_IN_ALL
void                    gtk_constraint_layout_add_constraint    (GtkConstraintLayout *manager,
                                                                 GtkConstraint       *constraint);
GDK_AVAILABLE_IN_ALL
void                    gtk_constraint_layout_remove_constraint (GtkConstraintLayout *manager,
                                                                 GtkConstraint       *constraint);

/**
 * GtkConstraintLayoutChild:
 *
 * A #GtkLayoutChild in a #GtkConstraintLayout.
 */
GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkConstraintLayoutChild, gtk_constraint_layout_child, GTK, CONSTRAINT_LAYOUT_CHILD, GtkLayoutChild)

G_END_DECLS
