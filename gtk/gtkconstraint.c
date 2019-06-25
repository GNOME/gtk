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

#include "gtkconstraintprivate.h"
#include "gtkconstraintsolverprivate.h"
#include "gtkintl.h"
#include "gtktypebuiltins.h"
#include "gtkwidget.h"

enum {
  PROP_TARGET_WIDGET = 1,
  PROP_TARGET_ATTRIBUTE,
  PROP_RELATION,
  PROP_SOURCE_WIDGET,
  PROP_SOURCE_ATTRIBUTE,
  PROP_MULTIPLIER,
  PROP_CONSTANT,
  PROP_STRENGTH,

  N_PROPERTIES
};

static GParamSpec *obj_props[N_PROPERTIES];

G_DEFINE_TYPE (GtkConstraint, gtk_constraint, G_TYPE_OBJECT)

static void
gtk_constraint_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkConstraint *self = GTK_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_TARGET_WIDGET:
      self->target_widget = g_value_get_object (value);
      break;

    case PROP_TARGET_ATTRIBUTE:
      self->target_attribute = g_value_get_enum (value);
      break;

    case PROP_RELATION:
      self->relation = g_value_get_enum (value);
      break;

    case PROP_SOURCE_WIDGET:
      self->source_widget = g_value_get_object (value);
      break;

    case PROP_SOURCE_ATTRIBUTE:
      self->source_attribute = g_value_get_enum (value);
      break;

    case PROP_MULTIPLIER:
      self->multiplier = g_value_get_double (value);
      break;

    case PROP_CONSTANT:
      self->constant = g_value_get_double (value);
      break;

    case PROP_STRENGTH:
      self->strength = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_constraint_get_property (GObject    *gobject,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  GtkConstraint *self = GTK_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_TARGET_WIDGET:
      g_value_set_object (value, self->target_widget);
      break;

    case PROP_TARGET_ATTRIBUTE:
      g_value_set_enum (value, self->target_attribute);
      break;

    case PROP_RELATION:
      g_value_set_enum (value, self->relation);
      break;

    case PROP_SOURCE_WIDGET:
      g_value_set_object (value, self->source_widget);
      break;

    case PROP_SOURCE_ATTRIBUTE:
      g_value_set_enum (value, self->source_attribute);
      break;

    case PROP_MULTIPLIER:
      g_value_set_double (value, self->multiplier);
      break;

    case PROP_CONSTANT:
      g_value_set_double (value, self->constant);
      break;

    case PROP_STRENGTH:
      g_value_set_int (value, self->strength);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_constraint_finalize (GObject *gobject)
{
  GtkConstraint *self = GTK_CONSTRAINT (gobject);

  gtk_constraint_detach (self);

  G_OBJECT_CLASS (gtk_constraint_parent_class)->finalize (gobject);
}

static void
gtk_constraint_class_init (GtkConstraintClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_constraint_set_property;
  gobject_class->get_property = gtk_constraint_get_property;
  gobject_class->finalize = gtk_constraint_finalize;

  /**
   * GtkConstraint:target-widget:
   *
   * The target widget of the constraint.
   *
   * The constraint will set the #GtkConstraint:target-attribute of the
   * target widget using the #GtkConstraint:source-attribute of the source
   * widget.
   */
  obj_props[PROP_TARGET_WIDGET] =
    g_param_spec_object ("target-widget",
                         P_("Target Widget"),
                         P_("The target widget of the constraint"),
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);
  /**
   * GtkConstraint:target-attribute:
   *
   * The attribute of the #GtkConstraint:target-widget set by the constraint.
   */
  obj_props[PROP_TARGET_ATTRIBUTE] =
    g_param_spec_enum ("target-attribute",
                       P_("Target Attribute"),
                       P_("The attribute of the target widget set by the constraint"),
                       GTK_TYPE_CONSTRAINT_ATTRIBUTE,
                       GTK_CONSTRAINT_ATTRIBUTE_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_CONSTRUCT_ONLY);
  /**
   * GtkConstraint:relation:
   *
   * The relation order between the terms of the constraint.
   */
  obj_props[PROP_RELATION] =
    g_param_spec_enum ("relation",
                       P_("Relation"),
                       P_("The relation between the source and target attributes"),
                       GTK_TYPE_CONSTRAINT_RELATION,
                       GTK_CONSTRAINT_RELATION_EQ,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_CONSTRUCT_ONLY);
  /**
   * GtkConstraint:source-widget:
   *
   * The source widget of the constraint.
   *
   * The constraint will set the #GtkConstraint:target-attribute of the
   * target widget using the #GtkConstraint:source-attribute of the source
   * widget.
   */
  obj_props[PROP_SOURCE_WIDGET] =
    g_param_spec_object ("source-widget",
                         P_("Source Widget"),
                         P_("The source widget of the constraint"),
                         GTK_TYPE_WIDGET,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);
  /**
   * GtkConstraint:source-attribute:
   *
   * The attribute of the #GtkConstraint:source-widget read by the constraint.
   */
  obj_props[PROP_SOURCE_ATTRIBUTE] =
    g_param_spec_enum ("source-attribute",
                       P_("Source Attribute"),
                       P_("The attribute of the source widget set by the constraint"),
                       GTK_TYPE_CONSTRAINT_ATTRIBUTE,
                       GTK_CONSTRAINT_ATTRIBUTE_NONE,
                       G_PARAM_READWRITE |
                       G_PARAM_STATIC_STRINGS |
                       G_PARAM_CONSTRUCT_ONLY);
  /**
   * GtkConstraint:multiplier:
   *
   * The multiplication factor to be applied to the
   * #GtkConstraint:source-attribue.
   */
  obj_props[PROP_MULTIPLIER] =
    g_param_spec_double ("multiplier",
                         P_("Multiplier"),
                         P_("The multiplication factor to be applied to the source attribute"),
                         -G_MAXDOUBLE, G_MAXDOUBLE, 1.0,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);
  /**
   * GtkConstraint:constant:
   *
   * The constant value to be added to the #GtkConstraint:source-attribute.
   */
  obj_props[PROP_CONSTANT] =
    g_param_spec_double ("constant",
                         P_("Constant"),
                         P_("The constant to be added to the source attribute"),
                         -G_MAXDOUBLE, G_MAXDOUBLE, 0.0,
                         G_PARAM_READWRITE |
                         G_PARAM_STATIC_STRINGS |
                         G_PARAM_CONSTRUCT_ONLY);
  /**
   * GtkConstraint:strength:
   *
   * The strength of the constraint.
   *
   * The strength can be expressed either using one of the symbolic values
   * of the #GtkConstraintStrength enumeration, or any positive integer
   * value.
   */
  obj_props[PROP_STRENGTH] =
    g_param_spec_int ("strength",
                      P_("Strength"),
                      P_("The strength of the constraint"),
                      GTK_CONSTRAINT_STRENGTH_WEAK, G_MAXINT,
                      GTK_CONSTRAINT_STRENGTH_REQUIRED,
                      G_PARAM_READWRITE |
                      G_PARAM_STATIC_STRINGS |
                      G_PARAM_CONSTRUCT_ONLY);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, obj_props);
}

static void
gtk_constraint_init (GtkConstraint *self)
{
  self->multiplier = 1.0;
  self->constant = 0.0;

  self->target_attribute = GTK_CONSTRAINT_ATTRIBUTE_NONE;
  self->source_attribute = GTK_CONSTRAINT_ATTRIBUTE_NONE;
  self->relation = GTK_CONSTRAINT_RELATION_EQ;
  self->strength = GTK_CONSTRAINT_STRENGTH_REQUIRED;
}

/**
 * gtk_constraint_new:
 * @target_widget: (nullable): a #GtkWidget
 * @target_attribute: the attribute of @target_widget to be set
 * @relation: the relation equivalence between @target_attribute and @source_attribute
 * @source_widget: (nullable): a #GtkWidget
 * @source_attribute: the attribute of @source_widget to be read
 * @multiplier: a multiplication factor to be applied to @source_attribute
 * @constant: a constant factor to be added to @source_attribute
 * @strength: the strength of the constraint
 *
 * Creates a new #GtkConstraint representing a relation between a layout
 * attribute on a source #GtkWidget and a layout attribute on a target
 * #GtkWidget.
 *
 * Returns: the newly created #GtkConstraint
 */
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
  g_return_val_if_fail (target_widget == NULL || GTK_IS_WIDGET (target_widget), NULL);
  g_return_val_if_fail (source_widget == NULL || GTK_IS_WIDGET (source_widget), NULL);

  return g_object_new (GTK_TYPE_CONSTRAINT,
                       "target-widget", target_widget,
                       "target-attribute", target_attribute,
                       "relation", relation,
                       "source-widget", source_widget,
                       "source-attribute", source_attribute,
                       "multiplier", multiplier,
                       "constant", constant,
                       "strength", strength,
                       NULL);
}

/**
 * gtk_constraint_new_constant:
 * @target_widget: (nullable): a #GtkWidget
 * @target_attribute: the attribute of @target_widget to be set
 * @relation: the relation equivalence between @target_attribute and @constant
 * @constant: a constant factor to be set on @target_attribute
 * @strength: the strength of the constraint
 *
 * Creates a new #GtkConstraint representing a relation between a layout
 * attribute on a target #GtkWidget and a constant value.
 *
 * Returns: the newly created #GtkConstraint
 */
GtkConstraint *
gtk_constraint_new_constant (GtkWidget              *target_widget,
                             GtkConstraintAttribute  target_attribute,
                             GtkConstraintRelation   relation,
                             double                  constant,
                             int                     strength)
{
  g_return_val_if_fail (target_widget == NULL || GTK_IS_WIDGET (target_widget), NULL);

  return g_object_new (GTK_TYPE_CONSTRAINT,
                       "target-widget", target_widget,
                       "target-attribute", target_attribute,
                       "relation", relation,
                       "source-attribute", GTK_CONSTRAINT_ATTRIBUTE_NONE,
                       "constant", constant,
                       "strength", strength,
                       NULL);
}

/**
 * gtk_constraint_get_target_widget:
 * @constraint: a #GtkConstraint
 *
 * Retrieves the target widget for the @constraint.
 *
 * Returns: (transfer none) (nullable): a #GtkWidget
 */
GtkWidget *
gtk_constraint_get_target_widget (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), NULL);

  return constraint->target_widget;
}

GtkConstraintAttribute
gtk_constraint_get_target_attribute (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), GTK_CONSTRAINT_ATTRIBUTE_NONE);

  return constraint->target_attribute;
}

/**
 * gtk_constraint_get_source_widget:
 * @constraint: a #GtkConstraint
 *
 * Retrieves the source widget for the @constraint.
 *
 * Returns: (transfer none) (nullable): a #GtkWidget
 */
GtkWidget *
gtk_constraint_get_source_widget (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), NULL);

  return constraint->source_widget;
}

GtkConstraintAttribute
gtk_constraint_get_source_attribute (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), GTK_CONSTRAINT_ATTRIBUTE_NONE);

  return constraint->source_attribute;
}

GtkConstraintRelation
gtk_constraint_get_relation (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), GTK_CONSTRAINT_RELATION_EQ);

  return constraint->relation;
}

double
gtk_constraint_get_multiplier (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), 1.0);

  return constraint->multiplier;
}

double
gtk_constraint_get_constant (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), 0.0);

  return constraint->constant;
}

int
gtk_constraint_get_strength (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), GTK_CONSTRAINT_STRENGTH_REQUIRED);

  return constraint->strength;
}

/*< private >
 * gtk_constraint_get_weight:
 * @constraint: a #GtkConstraint
 *
 * Computes the weight of the @constraint to be used with
 * #GtkConstraintSolver.
 *
 * Returns: the weight of the constraint
 */
double
gtk_constraint_get_weight (GtkConstraint *constraint)
{
  if (constraint->strength > 0)
    return constraint->strength;

  switch (constraint->strength)
    {
    case GTK_CONSTRAINT_STRENGTH_REQUIRED:
      return GTK_CONSTRAINT_WEIGHT_REQUIRED;

    case GTK_CONSTRAINT_STRENGTH_STRONG:
      return GTK_CONSTRAINT_WEIGHT_STRONG;

    case GTK_CONSTRAINT_STRENGTH_MEDIUM:
      return GTK_CONSTRAINT_WEIGHT_MEDIUM;

    case GTK_CONSTRAINT_STRENGTH_WEAK:
      return GTK_CONSTRAINT_WEIGHT_WEAK;

    default:
      g_assert_not_reached ();
    }

  return 0;
}

gboolean
gtk_constraint_is_required (GtkConstraint *constraint)
{
  g_return_val_if_fail (GTK_IS_CONSTRAINT (constraint), FALSE);

  return constraint->strength == GTK_CONSTRAINT_STRENGTH_REQUIRED;
}

gboolean
gtk_constraint_is_attached (GtkConstraint *constraint)
{
  return constraint->constraint_ref != NULL;
}

void
gtk_constraint_attach (GtkConstraint       *constraint,
                       GtkConstraintSolver *solver,
                       GtkConstraintRef    *ref)
{
  g_return_if_fail (GTK_IS_CONSTRAINT (constraint));
  g_return_if_fail (GTK_IS_CONSTRAINT_SOLVER (solver));
  g_return_if_fail (ref != NULL);

  constraint->constraint_ref = ref;
  constraint->solver = solver;
}

void
gtk_constraint_detach (GtkConstraint *constraint)
{
  g_return_if_fail (GTK_IS_CONSTRAINT (constraint));

  if (constraint->constraint_ref == NULL)
    return;

  gtk_constraint_solver_remove_constraint (constraint->solver, constraint->constraint_ref);
  constraint->constraint_ref = NULL;
  constraint->solver = NULL;
}

typedef struct _GtkConstraintTargetInterface GtkConstraintTargetInterface;

struct _GtkConstraintTargetInterface
{
  GTypeInterface g_iface;
};

G_DEFINE_INTERFACE (GtkConstraintTarget, gtk_constraint_target, G_TYPE_OBJECT)

static void
gtk_constraint_target_default_init (GtkConstraintTargetInterface *iface)
{
}
