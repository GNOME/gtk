/* 
 * Copyright 2019  Red Hat, inc.
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
 * Author: Matthias Clasen
 */

#include "gtkgridconstraint.h"
#include "gtkconstraintsolverprivate.h"

typedef struct {
  GtkWidget *child;
  int left;
  int right;
  int top;
  int bottom;
} GtkGridConstraintChild;

struct _GtkGridConstraint {
  GObject parent;

  gboolean row_homogeneous;
  gboolean column_homogeneous;

  GPtrArray *children;

  GtkConstraintSolver *solver;
  GPtrArray           *refs;
};

gboolean gtk_grid_constraint_is_attached (GtkGridConstraint *constraint);
void gtk_grid_constraint_attach (GtkGridConstraint   *constraint,
                                 GtkConstraintSolver *solver,
                                 GPtrArray           *refs);
void gtk_grid_constraint_detach (GtkGridConstraint *constraint);
