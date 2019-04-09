/* gtkconstraint.c: Constraint between two widgets
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
 * SECTION:gtkconstraint
 * @Title: GtkConstraint
 * @Short_description: The description of a constraint
 *
 * #GtkConstraint describes a constraint between an attribute on a widget
 * and another attribute on another widget, expressed as a linear equation
 * like:
 *
 * |[
 *   target.attr1 = source.attr2 × multiplier + constant
 * ]|
 *
 * Each #GtkConstraint is part of a system that will be solved by a
 * #GtkConstraintLayout in order to allocate and position each child widget.
 *
 * The source and target widgets, as well as their attributes, of a
 * #GtkConstraint instance are immutable after creation.
 */

#include "config.h"

#include "gtkconstraint.h"
#include "gtkconstrainttypesprivate.h"

struct _GtkConstraint
{
  GObject parent_instance;

  GtkConstraintAttribute target_attribute;
  GtkConstraintAttribute source_attribute;

  GtkWidget *target_widget;
  GtkWidget *source_widget;

  GtkConstraintRelation relation;

  double multiplier;
  double constant;

  int strength;

  /* A reference to the real constraint inside the
   * GtkConstraintSolver, so we can remove it when
   * finalizing the GtkConstraint instance
   */
  GtkConstraintRef *constraint_ref;
};

enum {
  PROP_TARGET_WIDGET = 1,
  PROP_TARGET_ATTRIBUTE,
  PROP_RELATION,
  PROP_SOURCE_WIDGET,
  PROP_SOURCE_ATTRIBUTE,
  PROP_MULTIPLIER,
  PROP_CONSTANT,
  PROP_STRENGTH,
  PROP_ACTIVE,
  PROP_ATTACHED,

  N_PROPERTIES
};

static GParamSpec *obj_props[N_PROPERTIES];

G_DEFINE_TYPE (GtkConstraint, gtk_constraint, G_TYPE_OBJECT)

static void
gtk_constraint_class_init (GtkConstraintClass *klass)
{
}

static void
gtk_constraint_init (GtkConstraint *self)
{
}

GtkConstraint *
gtk_constraint_new (GtkWidget              *target_widget,
                    GtkConstraintAttribute  target_attribute,
                    GtkConstraintRelation   relation,
                    GtkWidget              *source_widget,
                    GtkConstraintAttribute  source_attribute,
                    double                  multiplier,
                    double                  constant,
                    int                     strength)
{
}

GtkConstraint *
gtk_constraint_new_constant (GtkWidget              *target_widget,
                             GtkConstraintAttribute  target_attribute,
                             GtkConstraintRelation   relation,
                             double                  constant,
                             int                     strength)
{
}

GtkWidget *
gtk_constraint_get_target_widget (GtkConstraint *constraint)
{
}

GtkConstraintAttribute
gtk_constraint_get_target_attribute (GtkConstraint *constraint)
{
}

GtkWidget *
gtk_constraint_get_source_widget (GtkConstraint *constraint)
{
}

GtkConstraintAttribute
gtk_constraint_get_source_attribute (GtkConstraint *constraint)
{
}

GtkConstraintRelation
gtk_constraint_get_relation (GtkConstraint *constraint)
{
}

double
gtk_constraint_get_multiplier (GtkConstraint *constraint)
{
}

double
gtk_constraint_get_constant (GtkConstraint *constraint)
{
}

int
gtk_constraint_get_strength (GtkConstraint *constraint)
{
}

gboolean
gtk_constraint_is_required (GtkConstraint *constraint)
{
}

gboolean
gtk_constraint_is_attached (GtkConstraint *constraint)
{
}

void
gtk_constraint_set_active (GtkConstraint *constraint,
                           gboolean       is_active)
{
}

gboolean
gtk_constraint_get_active (GtkConstraint *constraint)
{
}
