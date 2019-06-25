/*
 * Copyright 2019 Red Hat, Inc.
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
 */

#ifndef __GTK_GRID_CONSTRAINT_H__
#define __GTK_GRID_CONSTRAINT_H__

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define GTK_TYPE_GRID_CONSTRAINT (gtk_grid_constraint_get_type ())

/**
 * GtkGridConstraint:
 *
 * An object used for managing constraints for children in
 * a constraints layout that are to be arranged in a grid.
 */
GDK_AVAILABLE_IN_ALL
G_DECLARE_FINAL_TYPE (GtkGridConstraint, gtk_grid_constraint, GTK, GRID_CONSTRAINT, GObject)

GDK_AVAILABLE_IN_ALL
GtkGridConstraint * gtk_grid_constraint_new     (void);

GDK_AVAILABLE_IN_ALL
void                gtk_grid_constraint_add     (GtkGridConstraint   *self,
                                                 GtkWidget           *child,
                                                 int                  left,
                                                 int                  right,
                                                 int                  top,
                                                 int                  bottom);

G_END_DECLS

#endif  /* __GTK_GRID_CONSTRAINT_H__ */
