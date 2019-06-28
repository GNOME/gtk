/* gtkconstraintsolverprivate.h: Constraint solver based on the Cassowary method
 * Copyright 2019  GNOME Foundation
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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

#include "gtkconstrainttypesprivate.h"

G_BEGIN_DECLS

#ifdef GTK_TEST_EXTERNAL

#define GTK_TYPE_CONSTRAINT_SOLVER (g_type_from_name ("GtkConstraintSolver"))
#define GTK_CONSTRAINT_SOLVER(solver)                (G_TYPE_CHECK_INSTANCE_CAST ((solver), GTK_TYPE_CONSTRAINT_SOLVER, GtkConstraintSolver))
#define GTK_CONSTRAINT_SOLVER_CLASS(klass)           (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_CONSTRAINT_SOLVER, GtkConstraintSolverClass))
#define GTK_IS_CONSTRAINT_SOLVER(solver)             (G_TYPE_CHECK_INSTANCE_TYPE ((solver), GTK_TYPE_CONSTRAINT_SOLVER))
#define GTK_IS_CONSTRAINT_SOLVER_CLASS(klass)        (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_CONSTRAINT_SOLVER))
#define GTK_CONSTRAINT_SOLVER_GET_CLASS(obj)         (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_CONSTRAINT_SOLVER, GtkConstraintSolverClass))

typedef struct _GtkConstraintSolver GtkConstraintSolver;
typedef struct _GtkConstraintSolverClass GtkConstraintSolverClass;

struct _GtkConstraintSolverClass {
  GObjectClass parent_class;
};

#else

#define GTK_TYPE_CONSTRAINT_SOLVER (gtk_constraint_solver_get_type())

G_DECLARE_FINAL_TYPE (GtkConstraintSolver, gtk_constraint_solver, GTK, CONSTRAINT_SOLVER, GObject)

#endif

/* Symbolic weight thresholds
 *
 * Constraint weights live on a continuum, but we use thresholds for simplicity's
 * sake, so we don't have to necessarily reason in terms of numeric values.
 *
 * The public API has a similar approach, where the symbolic constants are negative
 * values, and positive values are explicit weights. We map those values into
 * numeric values that the GtkConstraintSolver can plug into the linear equations
 * tableau.
 */
#define GTK_CONSTRAINT_WEIGHT_REQUIRED  (make_weight (1000, 1000, 1000, 1))
#define GTK_CONSTRAINT_WEIGHT_STRONG    (make_weight (   1,    0,    0, 1))
#define GTK_CONSTRAINT_WEIGHT_MEDIUM    (make_weight (   0,    1,    0, 1))
#define GTK_CONSTRAINT_WEIGHT_WEAK      (make_weight (   0,    0,    1, 1))

G_GNUC_PURE
static inline double
make_weight (double a,
             double b,
             double c,
             double w)
{
  double res = 0;

  res += CLAMP (a * w, 0, 1000) * 1000000;
  res += CLAMP (b * w, 0, 1000) * 1000;
  res += CLAMP (c * w, 0, 1000);

  return res;
}

GtkConstraintSolver *
gtk_constraint_solver_new (void);

void
gtk_constraint_solver_freeze (GtkConstraintSolver *solver);

void
gtk_constraint_solver_thaw (GtkConstraintSolver *solver);

void
gtk_constraint_solver_resolve (GtkConstraintSolver *solver);

GtkConstraintVariable *
gtk_constraint_solver_create_variable (GtkConstraintSolver *solver,
                                       const char          *prefix,
                                       const char          *name,
                                       double               value);

GtkConstraintRef *
gtk_constraint_solver_add_constraint (GtkConstraintSolver     *solver,
                                      GtkConstraintVariable   *variable,
                                      GtkConstraintRelation    relation,
                                      GtkConstraintExpression *expression,
                                      double                   strength);

void
gtk_constraint_solver_remove_constraint (GtkConstraintSolver *solver,
                                         GtkConstraintRef    *reference);

GtkConstraintRef *
gtk_constraint_solver_add_stay_variable (GtkConstraintSolver   *solver,
                                         GtkConstraintVariable *variable,
                                         double                 strength);

void
gtk_constraint_solver_remove_stay_variable (GtkConstraintSolver   *solver,
                                            GtkConstraintVariable *variable);

gboolean
gtk_constraint_solver_has_stay_variable (GtkConstraintSolver   *solver,
                                         GtkConstraintVariable *variable);

GtkConstraintRef *
gtk_constraint_solver_add_edit_variable (GtkConstraintSolver   *solver,
                                         GtkConstraintVariable *variable,
                                         double                 strength);

void
gtk_constraint_solver_remove_edit_variable (GtkConstraintSolver   *solver,
                                            GtkConstraintVariable *variable);

gboolean
gtk_constraint_solver_has_edit_variable (GtkConstraintSolver   *solver,
                                         GtkConstraintVariable *variable);

void
gtk_constraint_solver_suggest_value (GtkConstraintSolver   *solver,
                                     GtkConstraintVariable *variable,
                                     double                 value);

void
gtk_constraint_solver_begin_edit (GtkConstraintSolver *solver);

void
gtk_constraint_solver_end_edit (GtkConstraintSolver *solver);

void
gtk_constraint_solver_note_added_variable (GtkConstraintSolver *self,
                                           GtkConstraintVariable *variable,
                                           GtkConstraintVariable *subject);

void
gtk_constraint_solver_note_removed_variable (GtkConstraintSolver *self,
                                             GtkConstraintVariable *variable,
                                             GtkConstraintVariable *subject);

void
gtk_constraint_solver_clear (GtkConstraintSolver *solver);

char *
gtk_constraint_solver_to_string (GtkConstraintSolver *solver);

G_END_DECLS
