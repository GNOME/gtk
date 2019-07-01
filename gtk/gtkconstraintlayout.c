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
 *
 * # How do constraints work
 *
 * Constraints are objects defining the relationship between attributes
 * of a widget; you can read the description of the #GtkConstraint
 * class to have a more in depth definition.
 *
 * By taking multiple constraints and applying them to the children of
 * a widget using #GtkConstraintLayout, it's possible to describe complex
 * layout policies; each constraint applied to a child or to the parent
 * widgets contributes to the full description of the layout, in terms of
 * parameters for resolving the value of each attribute.
 *
 * It is important to note that a layout is defined by the totality of
 * constraints; removing a child, or a constraint, from an existing layout
 * without changing the remaining constraints may result in an unstable
 * or unsolvable layout.
 *
 * Constraints have an implicit "reading order"; you should start describing
 * each edge of each child, as well as their relationship with the parent
 * container, from the top left (or top right, in RTL languages), horizontally
 * first, and then vertically.
 *
 * A constraint-based layout with too few constraints can become "unstable",
 * that is: have more than one solution. The behavior of an unstable layout
 * is undefined.
 *
 * A constraint-based layout with conflicting constraints may be unsolvable,
 * and lead to an unstable layout. You can use the #GtkConstraint:strength
 * property of #GtkConstraint to "nudge" the layout towards a solution.
 *
 * # GtkConstraintLayout as GtkBuildable
 *
 * GtkConstraintLayout implements the #GtkBuildable interface and has a
 * custom "constraints" element which allows describing constraints in a
 * GtkBuilder UI file.
 *
 * An example of a UI definition fragment specifying a constraint:
 *
 * |[
 *   <object class="GtkConstraintLayout">
 *     <constraints>
 *       <constraint target="button" target-attribute="start"
 *                   relation="eq"
 *                   source="super" source-attribute="start"
 *                   constant="12"
 *                   strength="required" />
 *       <constraint target="button" target-attribute="width"
 *                   relation="ge"
 *                   constant="250"
 *                   strength="strong" />
 *     </constraints>
 *   </object>
 * ]|
 *
 * The definition above will add two constraints to the GtkConstraintLayout:
 *
 *  - a required constraint between the leading edge of "button" and
 *    the leading edge of the widget using the constraint layout, plus
 *    12 pixels
 *  - a strong, constant constraint making the width of "button" greater
 *    than, or equal to 250 pixels
 *
 * The "target" and "target-attribute" attributes are required.
 *
 * The "source" and "source-attribute" attributes of the "constraint"
 * element are optional; if they are not specified, the constraint is
 * assumed to be a constant.
 *
 * The "relation" attribute is optional; if not specified, the constraint
 * is assumed to be an equality.
 *
 * The "strength" attribute is optional; if not specified, the constraint
 * is assumed to be required.
 *
 * The "source" and "target" attributes can be set to "super" to indicate
 * that the constraint target is the widget using the GtkConstraintLayout.
 *
 * # Using the Visual Format Language
 *
 * Complex constraints can be described using a compact syntax called VFL,
 * or *Visual Format Language*.
 *
 * The Visual Format Language describes all the constraints on a row or
 * column, typically starting from the leading edge towards the trailing
 * one. Each element of the layout is composed by "views", which identify
 * a #GtkConstraintTarget.
 *
 * For instance:
 *
 * |[
 *   [button]-[textField]
 * ]|
 *
 * Describes a constraint that binds the trailing edge of "button" to the
 * leading edge of "textField", leaving a default space between the two.
 *
 * Using VFL is also possible to specify predicates that describe constraints
 * on attributes like width and height:
 *
 * |[
 *   // Width must be greater than, or equal to 50
 *   [button(>=50)]
 *
 *   // Width of button1 must be equal to width of button2
 *   [button1(==button2)]
 * ]|
 *
 * The default orientation for a VFL description is horizontal, unless
 * otherwise specified:
 *
 * |[
 *   // horizontal orientation, default attribute: width
 *   H:[button(>=150)]
 *
 *   // vertical orientation, default attribute: height
 *   V:[button1(==button2)]
 * ]|
 *
 * It's also possible to specify multiple predicates, as well as their
 * strength:
 *
 * |[
 *   // minimum width of button must be 150
 *   // natural width of button can be 250
 *   [button(>=150@required, ==250@medium)]
 * ]|
 *
 * Finally, it's also possible to use simple arithmetic operators:
 *
 * |[
 *   // width of button1 must be equal to width of button2
 *   // divided by 2 plus 12
 *   [button1(button2 / 2 + 12)]
 * ]|
 */

#include "config.h"

#include "gtkconstraintlayout.h"
#include "gtkconstraintlayoutprivate.h"

#include "gtkconstraintprivate.h"
#include "gtkconstraintexpressionprivate.h"
#include "gtkconstraintguideprivate.h"
#include "gtkconstraintsolverprivate.h"
#include "gtkconstraintvflparserprivate.h"

#include "gtkbuildable.h"
#include "gtkbuilderprivate.h"
#include "gtkdebug.h"
#include "gtklayoutchild.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkwidgetprivate.h"

#include <string.h>
#include <errno.h>

enum {
  MIN_WIDTH,
  MIN_HEIGHT,
  NAT_WIDTH,
  NAT_HEIGHT,
  LAST_VALUE
};

struct _GtkConstraintLayoutChild
{
  GtkLayoutChild parent_instance;

  int values[LAST_VALUE];
  GtkConstraintRef *constraints[LAST_VALUE];

  /* HashTable<static string, Variable>; a hash table of variables,
   * one for each attribute; we use these to query and suggest the
   * values for the solver. The string is static and does not need
   * to be freed.
   */
  GHashTable *bound_attributes;
};

struct _GtkConstraintLayout
{
  GtkLayoutManager parent_instance;

  /* A pointer to the GtkConstraintSolver used by the layout manager;
   * we acquire one when the layout manager gets rooted, and release
   * it when it gets unrooted.
   */
  GtkConstraintSolver *solver;

  /* HashTable<static string, Variable>; a hash table of variables,
   * one for each attribute; we use these to query and suggest the
   * values for the solver. The string is static and does not need
   * to be freed.
   */
  GHashTable *bound_attributes;

  /* HashSet<GtkConstraint>; the set of constraints on the
   * parent widget, using the public API objects.
   */
  GHashTable *constraints;

  /* HashSet<GtkConstraintGuide> */
  GHashTable *guides;
};

G_DEFINE_TYPE (GtkConstraintLayoutChild, gtk_constraint_layout_child, GTK_TYPE_LAYOUT_CHILD)

GtkConstraintSolver *
gtk_constraint_layout_get_solver (GtkConstraintLayout *self)
{
  GtkWidget *widget;
  GtkRoot *root;

  if (self->solver != NULL)
    return self->solver;

  widget = gtk_layout_manager_get_widget (GTK_LAYOUT_MANAGER (self));
  if (widget == NULL)
    return NULL;

  root = gtk_widget_get_root (widget);
  if (root == NULL)
    return NULL;

  self->solver = gtk_root_get_constraint_solver (root);

  return self->solver;
}

static const char * const attribute_names[] = {
  [GTK_CONSTRAINT_ATTRIBUTE_NONE]     = "none",
  [GTK_CONSTRAINT_ATTRIBUTE_LEFT]     = "left",
  [GTK_CONSTRAINT_ATTRIBUTE_RIGHT]    = "right",
  [GTK_CONSTRAINT_ATTRIBUTE_TOP]      = "top",
  [GTK_CONSTRAINT_ATTRIBUTE_BOTTOM]   = "bottom",
  [GTK_CONSTRAINT_ATTRIBUTE_START]    = "start",
  [GTK_CONSTRAINT_ATTRIBUTE_END]      = "end",
  [GTK_CONSTRAINT_ATTRIBUTE_WIDTH]    = "width",
  [GTK_CONSTRAINT_ATTRIBUTE_HEIGHT]   = "height",
  [GTK_CONSTRAINT_ATTRIBUTE_CENTER_X] = "center-x",
  [GTK_CONSTRAINT_ATTRIBUTE_CENTER_Y] = "center-y",
  [GTK_CONSTRAINT_ATTRIBUTE_BASELINE] = "baseline",
};

G_GNUC_PURE
static const char *
get_attribute_name (GtkConstraintAttribute attr)
{
  return attribute_names[attr];
}

static GtkConstraintAttribute
resolve_direction (GtkConstraintAttribute  attr,
                   GtkWidget              *widget)
{
  GtkTextDirection text_dir;

  /* Resolve the start/end attributes depending on the layout's text direction */

  if (widget)
    text_dir = gtk_widget_get_direction (widget);
  else
    text_dir = GTK_TEXT_DIR_LTR;

  if (attr == GTK_CONSTRAINT_ATTRIBUTE_START)
    {
      if (text_dir == GTK_TEXT_DIR_RTL)
        attr = GTK_CONSTRAINT_ATTRIBUTE_RIGHT;
      else
        attr = GTK_CONSTRAINT_ATTRIBUTE_LEFT;
    }
  else if (attr == GTK_CONSTRAINT_ATTRIBUTE_END)
    {
      if (text_dir == GTK_TEXT_DIR_RTL)
        attr = GTK_CONSTRAINT_ATTRIBUTE_LEFT;
      else
        attr = GTK_CONSTRAINT_ATTRIBUTE_RIGHT;
    }

  return attr;
}

GtkConstraintVariable *
gtk_constraint_layout_get_attribute (GtkConstraintLayout    *layout,
                                     GtkConstraintAttribute  attr,
                                     const char             *prefix,
                                     GtkWidget              *widget,
                                     GHashTable             *bound_attributes)
{
  const char *attr_name;
  GtkConstraintVariable *res;
  GtkConstraintSolver *solver = layout->solver;

  attr = resolve_direction (attr, widget);

  attr_name = get_attribute_name (attr);
  res = g_hash_table_lookup (bound_attributes, attr_name);
  if (res != NULL)
    return res;

  res = gtk_constraint_solver_create_variable (solver, prefix, attr_name, 0.0);
  g_hash_table_insert (bound_attributes, (gpointer) attr_name, res);

  /* Some attributes are really constraints computed from other
   * attributes, to avoid creating additional constraints from
   * the user's perspective
   */
  switch (attr)
    {
    /* right = left + width */
    case GTK_CONSTRAINT_ATTRIBUTE_RIGHT:
      {
        GtkConstraintExpressionBuilder builder;
        GtkConstraintVariable *left, *width;
        GtkConstraintExpression *expr;

        left = gtk_constraint_layout_get_attribute (layout, GTK_CONSTRAINT_ATTRIBUTE_LEFT, prefix, widget, bound_attributes);
        width = gtk_constraint_layout_get_attribute (layout, GTK_CONSTRAINT_ATTRIBUTE_WIDTH, prefix, widget, bound_attributes);

        gtk_constraint_expression_builder_init (&builder, solver);
        gtk_constraint_expression_builder_term (&builder, left);
        gtk_constraint_expression_builder_plus (&builder);
        gtk_constraint_expression_builder_term (&builder, width);
        expr = gtk_constraint_expression_builder_finish (&builder);

        gtk_constraint_solver_add_constraint (solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
      }
      break;

    /* bottom = top + height */
    case GTK_CONSTRAINT_ATTRIBUTE_BOTTOM:
      {
        GtkConstraintExpressionBuilder builder;
        GtkConstraintVariable *top, *height;
        GtkConstraintExpression *expr;

        top = gtk_constraint_layout_get_attribute (layout, GTK_CONSTRAINT_ATTRIBUTE_TOP, prefix, widget, bound_attributes);
        height = gtk_constraint_layout_get_attribute (layout, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT, prefix, widget, bound_attributes);

        gtk_constraint_expression_builder_init (&builder, solver);
        gtk_constraint_expression_builder_term (&builder, top);
        gtk_constraint_expression_builder_plus (&builder);
        gtk_constraint_expression_builder_term (&builder, height);
        expr = gtk_constraint_expression_builder_finish (&builder);

        gtk_constraint_solver_add_constraint (solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
      }
      break;

    /* centerX = (width / 2.0) + left*/
    case GTK_CONSTRAINT_ATTRIBUTE_CENTER_X:
      {
        GtkConstraintExpressionBuilder builder;
        GtkConstraintVariable *left, *width;
        GtkConstraintExpression *expr;

        left = gtk_constraint_layout_get_attribute (layout, GTK_CONSTRAINT_ATTRIBUTE_LEFT, prefix, widget, bound_attributes);
        width = gtk_constraint_layout_get_attribute (layout, GTK_CONSTRAINT_ATTRIBUTE_WIDTH, prefix, widget, bound_attributes);

        gtk_constraint_expression_builder_init (&builder, solver);
        gtk_constraint_expression_builder_term (&builder, width);
        gtk_constraint_expression_builder_divide_by (&builder);
        gtk_constraint_expression_builder_constant (&builder, 2.0);
        gtk_constraint_expression_builder_plus (&builder);
        gtk_constraint_expression_builder_term (&builder, left);
        expr = gtk_constraint_expression_builder_finish (&builder);

        gtk_constraint_solver_add_constraint (solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
      }
      break;

    /* centerY = (height / 2.0) + top */
    case GTK_CONSTRAINT_ATTRIBUTE_CENTER_Y:
      {
        GtkConstraintExpressionBuilder builder;
        GtkConstraintVariable *top, *height;
        GtkConstraintExpression *expr;

        top = gtk_constraint_layout_get_attribute (layout, GTK_CONSTRAINT_ATTRIBUTE_TOP, prefix, widget, bound_attributes);
        height = gtk_constraint_layout_get_attribute (layout, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT, prefix, widget, bound_attributes);

        gtk_constraint_expression_builder_init (&builder, solver);
        gtk_constraint_expression_builder_term (&builder, height);
        gtk_constraint_expression_builder_divide_by (&builder);
        gtk_constraint_expression_builder_constant (&builder, 2.0);
        gtk_constraint_expression_builder_plus (&builder);
        gtk_constraint_expression_builder_term (&builder, top);
        expr = gtk_constraint_expression_builder_finish (&builder);

        gtk_constraint_solver_add_constraint (solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
      }
      break;

    /* We do not allow negative sizes */
    case GTK_CONSTRAINT_ATTRIBUTE_WIDTH:
    case GTK_CONSTRAINT_ATTRIBUTE_HEIGHT:
      {
        GtkConstraintExpression *expr;

        expr = gtk_constraint_expression_new (0.0);
        gtk_constraint_solver_add_constraint (solver,
                                              res, GTK_CONSTRAINT_RELATION_GE, expr,
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
      }
      break;

    /* These are "pure" attributes */
    case GTK_CONSTRAINT_ATTRIBUTE_NONE:
    case GTK_CONSTRAINT_ATTRIBUTE_LEFT:
    case GTK_CONSTRAINT_ATTRIBUTE_TOP:
    case GTK_CONSTRAINT_ATTRIBUTE_BASELINE:
      break;

    /* These attributes must have been resolved to their real names */
    case GTK_CONSTRAINT_ATTRIBUTE_START:
    case GTK_CONSTRAINT_ATTRIBUTE_END:
      g_assert_not_reached ();
      break;

    default:
      break;
    }

  return res;
}

static GtkConstraintVariable *
get_child_attribute (GtkConstraintLayout    *layout,
                     GtkWidget              *widget,
                     GtkConstraintAttribute  attr)
{
  GtkConstraintLayoutChild *child_info;
  const char *prefix = gtk_widget_get_name (widget);

  child_info = GTK_CONSTRAINT_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (GTK_LAYOUT_MANAGER (layout), widget));

  return gtk_constraint_layout_get_attribute (layout, attr, prefix, widget, child_info->bound_attributes);
}

static void
gtk_constraint_layout_child_finalize (GObject *gobject)
{
  GtkConstraintLayoutChild *self = GTK_CONSTRAINT_LAYOUT_CHILD (gobject);

  g_clear_pointer (&self->bound_attributes, g_hash_table_unref);

  G_OBJECT_CLASS (gtk_constraint_layout_child_parent_class)->finalize (gobject);
}

static void
gtk_constraint_layout_child_class_init (GtkConstraintLayoutChildClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_constraint_layout_child_finalize;
}

static void
gtk_constraint_layout_child_init (GtkConstraintLayoutChild *self)
{
  self->bound_attributes =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           NULL,
                           (GDestroyNotify) gtk_constraint_variable_unref);
}

static void gtk_buildable_interface_init (GtkBuildableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GtkConstraintLayout, gtk_constraint_layout, GTK_TYPE_LAYOUT_MANAGER,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_BUILDABLE, gtk_buildable_interface_init))

static void
gtk_constraint_layout_finalize (GObject *gobject)
{
  GtkConstraintLayout *self = GTK_CONSTRAINT_LAYOUT (gobject);

  g_clear_pointer (&self->bound_attributes, g_hash_table_unref);
  g_clear_pointer (&self->constraints, g_hash_table_unref);
  g_clear_pointer (&self->guides, g_hash_table_unref);

  G_OBJECT_CLASS (gtk_constraint_layout_parent_class)->finalize (gobject);
}

static GtkConstraintVariable *
get_layout_attribute (GtkConstraintLayout    *self,
                      GtkWidget              *widget,
                      GtkConstraintAttribute  attr)
{
  GtkTextDirection text_dir;
  const char *attr_name;
  GtkConstraintVariable *res;

  /* Resolve the start/end attributes depending on the layout's text direction */
  if (attr == GTK_CONSTRAINT_ATTRIBUTE_START)
    {
      text_dir = gtk_widget_get_direction (widget);
      if (text_dir == GTK_TEXT_DIR_RTL)
        attr = GTK_CONSTRAINT_ATTRIBUTE_RIGHT;
      else
        attr = GTK_CONSTRAINT_ATTRIBUTE_LEFT;
    }
  else if (attr == GTK_CONSTRAINT_ATTRIBUTE_END)
    {
      text_dir = gtk_widget_get_direction (widget);
      if (text_dir == GTK_TEXT_DIR_RTL)
        attr = GTK_CONSTRAINT_ATTRIBUTE_LEFT;
      else
        attr = GTK_CONSTRAINT_ATTRIBUTE_RIGHT;
    }

  attr_name = get_attribute_name (attr);
  res = g_hash_table_lookup (self->bound_attributes, attr_name);
  if (res != NULL)
    return res;

  res = gtk_constraint_solver_create_variable (self->solver, "super", attr_name, 0.0);
  g_hash_table_insert (self->bound_attributes, (gpointer) attr_name, res);

  /* Some attributes are really constraints computed from other
   * attributes, to avoid creating additional constraints from
   * the user's perspective
   */
  switch (attr)
    {
    /* right = left + width */
    case GTK_CONSTRAINT_ATTRIBUTE_RIGHT:
      {
        GtkConstraintExpressionBuilder builder;
        GtkConstraintVariable *left, *width;
        GtkConstraintExpression *expr;

        left = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_LEFT);
        width = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_WIDTH);

        gtk_constraint_expression_builder_init (&builder, self->solver);
        gtk_constraint_expression_builder_term (&builder, left);
        gtk_constraint_expression_builder_plus (&builder);
        gtk_constraint_expression_builder_term (&builder, width);
        expr = gtk_constraint_expression_builder_finish (&builder);

        gtk_constraint_solver_add_constraint (self->solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
      }
      break;

    /* bottom = top + height */
    case GTK_CONSTRAINT_ATTRIBUTE_BOTTOM:
      {
        GtkConstraintExpressionBuilder builder;
        GtkConstraintVariable *top, *height;
        GtkConstraintExpression *expr;

        top = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_TOP);
        height = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT);

        gtk_constraint_expression_builder_init (&builder, self->solver);
        gtk_constraint_expression_builder_term (&builder, top);
        gtk_constraint_expression_builder_plus (&builder);
        gtk_constraint_expression_builder_term (&builder, height);
        expr = gtk_constraint_expression_builder_finish (&builder);

        gtk_constraint_solver_add_constraint (self->solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
      }
      break;

    /* centerX = left + (width / 2.0) */
    case GTK_CONSTRAINT_ATTRIBUTE_CENTER_X:
      {
        GtkConstraintExpressionBuilder builder;
        GtkConstraintVariable *left, *width;
        GtkConstraintExpression *expr;

        left = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_LEFT);
        width = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_WIDTH);

        gtk_constraint_expression_builder_init (&builder, self->solver);
        gtk_constraint_expression_builder_term (&builder, width);
        gtk_constraint_expression_builder_divide_by (&builder);
        gtk_constraint_expression_builder_constant (&builder, 2.0);
        gtk_constraint_expression_builder_plus (&builder);
        gtk_constraint_expression_builder_term (&builder, left);
        expr = gtk_constraint_expression_builder_finish (&builder);

        gtk_constraint_solver_add_constraint (self->solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
      }
      break;

    /* centerY = top + (height / 2.0) */
    case GTK_CONSTRAINT_ATTRIBUTE_CENTER_Y:
      {
        GtkConstraintExpressionBuilder builder;
        GtkConstraintVariable *top, *height;
        GtkConstraintExpression *expr;

        top = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_TOP);
        height = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT);

        gtk_constraint_expression_builder_init (&builder, self->solver);
        gtk_constraint_expression_builder_term (&builder, height);
        gtk_constraint_expression_builder_divide_by (&builder);
        gtk_constraint_expression_builder_constant (&builder, 2.0);
        gtk_constraint_expression_builder_plus (&builder);
        gtk_constraint_expression_builder_term (&builder, top);
        expr = gtk_constraint_expression_builder_finish (&builder);

        gtk_constraint_solver_add_constraint (self->solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
      }
      break;

    /* We do not allow negative sizes */
    case GTK_CONSTRAINT_ATTRIBUTE_WIDTH:
    case GTK_CONSTRAINT_ATTRIBUTE_HEIGHT:
      {
        GtkConstraintExpression *expr;

        expr = gtk_constraint_expression_new (0.0);
        gtk_constraint_solver_add_constraint (self->solver,
                                              res, GTK_CONSTRAINT_RELATION_GE, expr,
                                              GTK_CONSTRAINT_STRENGTH_REQUIRED);
      }
      break;

    /* These are "pure" attributes */
    case GTK_CONSTRAINT_ATTRIBUTE_NONE:
    case GTK_CONSTRAINT_ATTRIBUTE_LEFT:
    case GTK_CONSTRAINT_ATTRIBUTE_TOP:
    case GTK_CONSTRAINT_ATTRIBUTE_BASELINE:
      break;

    /* These attributes must have been resolved to their real names */
    case GTK_CONSTRAINT_ATTRIBUTE_START:
    case GTK_CONSTRAINT_ATTRIBUTE_END:
      g_assert_not_reached ();
      break;

    default:
      break;
    }

  return res;
}

/*< private >
 * layout_add_constraint:
 * @self: a #GtkConstraintLayout
 * @constraint: a #GtkConstraint
 *
 * Turns a #GtkConstraint into a #GtkConstraintRef inside the
 * constraint solver associated to @self.
 *
 * If @self does not have a #GtkConstraintSolver, because it
 * has not been rooted yet, we just store the @constraint instance,
 * and we're going to call this function when the layout manager
 * gets rooted.
 */
static void
layout_add_constraint (GtkConstraintLayout *self,
                       GtkConstraint       *constraint)
{
  GtkConstraintVariable *target_attr, *source_attr;
  GtkConstraintExpressionBuilder builder;
  GtkConstraintExpression *expr;
  GtkConstraintSolver *solver;
  GtkConstraintAttribute attr;
  GtkConstraintTarget *target, *source;
  GtkWidget *layout_widget;

  if (gtk_constraint_is_attached (constraint))
    return;

  /* Once we pass the preconditions, we check if we can turn a GtkConstraint
   * into a GtkConstraintRef; if we can't, we keep a reference to the
   * constraint object and try later on
   */
  layout_widget = gtk_layout_manager_get_widget (GTK_LAYOUT_MANAGER (self));
  if (layout_widget == NULL)
    return;

  solver = gtk_constraint_layout_get_solver (self);
  if (solver == NULL)
    return;

  attr = gtk_constraint_get_target_attribute (constraint);
  target = gtk_constraint_get_target (constraint);
  if (target == NULL || target == GTK_CONSTRAINT_TARGET (layout_widget))
    {
      /* A NULL target widget is assumed to be referring to the layout itself */
      target_attr = get_layout_attribute (self, layout_widget, attr);
    }
  else if (GTK_IS_WIDGET (target) &&
           gtk_widget_get_parent (GTK_WIDGET (target)) == layout_widget)
    {
      target_attr = get_child_attribute (self, GTK_WIDGET (target), attr);
    }
  else if (GTK_IS_CONSTRAINT_GUIDE (target))
    {
      GtkConstraintGuide *guide;

      guide = (GtkConstraintGuide*)g_hash_table_lookup (self->guides, target);
      target_attr = gtk_constraint_guide_get_attribute (guide, attr);
    }
  else
    {
      g_critical ("Unknown target widget '%p'", target);
      target_attr = NULL;
    }

  if (target_attr == NULL)
    return;

  attr = gtk_constraint_get_source_attribute (constraint);
  source = gtk_constraint_get_source (constraint);

  /* The constraint is a constant */
  if (attr == GTK_CONSTRAINT_ATTRIBUTE_NONE)
    {
      source_attr = NULL;
    }
  else
    {
      if (source == NULL || source == GTK_CONSTRAINT_TARGET (layout_widget))
        {
          source_attr = get_layout_attribute (self, layout_widget, attr);
        }
      else if (GTK_IS_WIDGET (source) &&
               gtk_widget_get_parent (GTK_WIDGET (source)) == layout_widget)
        {
          source_attr = get_child_attribute (self, GTK_WIDGET (source), attr);
        }
      else if (GTK_IS_CONSTRAINT_GUIDE (source))
        {
          GtkConstraintGuide *guide;

          guide = (GtkConstraintGuide*)g_hash_table_lookup (self->guides, source);
          source_attr = gtk_constraint_guide_get_attribute (guide, attr);
        }
      else
        {
          g_critical ("Unknown source widget '%p'", source);
          source_attr = NULL;
          return;
        }
     }

  /* Build the expression */
  gtk_constraint_expression_builder_init (&builder, self->solver);

  if (source_attr != NULL)
    {
      gtk_constraint_expression_builder_term (&builder, source_attr);
      gtk_constraint_expression_builder_multiply_by (&builder);
      gtk_constraint_expression_builder_constant (&builder, gtk_constraint_get_multiplier (constraint));
      gtk_constraint_expression_builder_plus (&builder);
    }

  gtk_constraint_expression_builder_constant (&builder, gtk_constraint_get_constant (constraint));
  expr = gtk_constraint_expression_builder_finish (&builder);

  constraint->solver = solver;
  constraint->constraint_ref =
    gtk_constraint_solver_add_constraint (self->solver,
                                          target_attr,
                                          gtk_constraint_get_relation (constraint),
                                          expr,
                                          gtk_constraint_get_strength (constraint));
}

static void
update_child_constraint (GtkConstraintLayout       *self,
                         GtkConstraintLayoutChild  *child_info,
                         GtkWidget                 *child,
                         int                        index,
                         int                        value)
{

  GtkConstraintVariable *var;
  int attr[LAST_VALUE] = {
    GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
    GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
    GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
    GTK_CONSTRAINT_ATTRIBUTE_HEIGHT
  };
  int relation[LAST_VALUE] = {
    GTK_CONSTRAINT_RELATION_GE,
    GTK_CONSTRAINT_RELATION_GE,
    GTK_CONSTRAINT_RELATION_EQ,
    GTK_CONSTRAINT_RELATION_EQ
  };

  if (child_info->values[index] != value)
    {
      child_info->values[index] = value;

      if (child_info->constraints[index])
        gtk_constraint_solver_remove_constraint (self->solver,
                                                 child_info->constraints[index]);

      var = get_child_attribute (self, child, attr[index]);

      if (relation[index] == GTK_CONSTRAINT_RELATION_EQ)
        {
          gtk_constraint_variable_set_value (var, value);
          child_info->constraints[index] =
            gtk_constraint_solver_add_stay_variable (self->solver,
                                                     var,
                                                     GTK_CONSTRAINT_STRENGTH_MEDIUM);
        }
      else
        {
          child_info->constraints[index] =
            gtk_constraint_solver_add_constraint (self->solver,
                                                  var,
                                                  relation[index],
                                                  gtk_constraint_expression_new (value),
                                                  GTK_CONSTRAINT_STRENGTH_REQUIRED);
        }
    }
}

static void
gtk_constraint_layout_measure (GtkLayoutManager *manager,
                               GtkWidget        *widget,
                               GtkOrientation    orientation,
                               int               for_size,
                               int              *minimum,
                               int              *natural,
                               int              *minimum_baseline,
                               int              *natural_baseline)
{
  GtkConstraintLayout *self = GTK_CONSTRAINT_LAYOUT (manager);
  GtkConstraintVariable *size, *opposite_size;
  GtkConstraintSolver *solver;
  GtkWidget *child;
  int min_value;
  int nat_value;

  solver = gtk_constraint_layout_get_solver (self);
  if (solver == NULL)
    return;

  gtk_constraint_solver_freeze (solver);

  /* We measure each child in the layout and impose restrictions on the
   * minimum and natural size, so we can solve the size of the overall
   * layout later on
   */
  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkConstraintLayoutChild *info;
      GtkRequisition min_req, nat_req;

      if (!gtk_widget_should_layout (child))
        continue;

      gtk_widget_get_preferred_size (child, &min_req, &nat_req);

      info = GTK_CONSTRAINT_LAYOUT_CHILD (gtk_layout_manager_get_layout_child (manager, child));

      update_child_constraint (self, info, child, MIN_WIDTH, min_req.width);
      update_child_constraint (self, info, child, MIN_HEIGHT, min_req.height);
      update_child_constraint (self, info, child, NAT_WIDTH, nat_req.width);
      update_child_constraint (self, info, child, NAT_HEIGHT, nat_req.height);
    }

  gtk_constraint_solver_thaw (solver);

  switch (orientation)
    {
    case GTK_ORIENTATION_HORIZONTAL:
      size = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_WIDTH);
      opposite_size = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT);
      break;

    case GTK_ORIENTATION_VERTICAL:
      size = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT);
      opposite_size = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_WIDTH);
      break;

    default:
      g_assert_not_reached ();
    }

  g_assert (size != NULL && opposite_size != NULL);

  nat_value = gtk_constraint_variable_get_value (size);

  /* We impose a temporary value on the size and opposite size of the
   * layout, with a low weight to let the solver settle towards the
   * natural state of the system. Once we get the value out, we can
   * remove these constraints
   */
  gtk_constraint_solver_add_edit_variable (solver, size, GTK_CONSTRAINT_STRENGTH_STRONG * 2);
  if (for_size > 0)
    gtk_constraint_solver_add_edit_variable (solver, opposite_size, GTK_CONSTRAINT_STRENGTH_STRONG * 2);
  gtk_constraint_solver_begin_edit (solver);
  gtk_constraint_solver_suggest_value (solver, size, 0.0);
  if (for_size > 0)
    gtk_constraint_solver_suggest_value (solver, opposite_size, for_size);
  gtk_constraint_solver_resolve (solver);

  min_value = gtk_constraint_variable_get_value (size);

  gtk_constraint_solver_remove_edit_variable (solver, size);
  if (for_size > 0)
    gtk_constraint_solver_remove_edit_variable (solver, opposite_size);
  gtk_constraint_solver_end_edit (solver);

  GTK_NOTE (LAYOUT,
            g_print ("layout %p %s size: min %d nat %d (for opposite size: %d)\n",
                     self,
                     orientation == GTK_ORIENTATION_HORIZONTAL ? "horizontal" : "vertical",
                     min_value, nat_value,
                     for_size));

  if (minimum != NULL)
    *minimum = min_value;

  if (natural != NULL)
    *natural = nat_value;
}

static void
gtk_constraint_layout_allocate (GtkLayoutManager *manager,
                                GtkWidget        *widget,
                                int               width,
                                int               height,
                                int               baseline)
{
  GtkConstraintLayout *self = GTK_CONSTRAINT_LAYOUT (manager);
  GtkConstraintRef *stay_w, *stay_h, *stay_t, *stay_l;
  GtkConstraintSolver *solver;
  GtkConstraintVariable *layout_top, *layout_height;
  GtkConstraintVariable *layout_left, *layout_width;
  GtkWidget *child;

  solver = gtk_constraint_layout_get_solver (self);
  if (solver == NULL)
    return;

  /* We add required stay constraints to ensure that the layout remains
   * within the bounds of the allocation
   */
  layout_top = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_TOP);
  layout_left = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_LEFT);
  layout_width = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_WIDTH);
  layout_height = get_layout_attribute (self, widget, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT);

  gtk_constraint_variable_set_value (layout_top, 0.0);
  stay_t = gtk_constraint_solver_add_stay_variable (solver,
                                                    layout_top,
                                                    GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_variable_set_value (layout_left, 0.0);
  stay_l = gtk_constraint_solver_add_stay_variable (solver,
                                                    layout_left,
                                                    GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_variable_set_value (layout_width, width);
  stay_w = gtk_constraint_solver_add_stay_variable (solver,
                                                    layout_width,
                                                    GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_variable_set_value (layout_height, height);
  stay_h = gtk_constraint_solver_add_stay_variable (solver,
                                                    layout_height,
                                                    GTK_CONSTRAINT_STRENGTH_REQUIRED);
  GTK_NOTE (LAYOUT,
            g_print ("Layout [%p]: { .x: %g, .y: %g, .w: %g, .h: %g }\n",
                     self,
                     gtk_constraint_variable_get_value (layout_left),
                     gtk_constraint_variable_get_value (layout_top),
                     gtk_constraint_variable_get_value (layout_width),
                     gtk_constraint_variable_get_value (layout_height)));

  for (child = _gtk_widget_get_first_child (widget);
       child != NULL;
       child = _gtk_widget_get_next_sibling (child))
    {
      GtkConstraintVariable *var_top, *var_left, *var_width, *var_height;
      GtkConstraintVariable *var_baseline;
      GtkAllocation child_alloc;
      int child_baseline = -1;

      if (!gtk_widget_should_layout (child))
        continue;

      /* Retrieve all the values associated with the child */
      var_top = get_child_attribute (self, child, GTK_CONSTRAINT_ATTRIBUTE_TOP);
      var_left = get_child_attribute (self, child, GTK_CONSTRAINT_ATTRIBUTE_LEFT);
      var_width = get_child_attribute (self, child, GTK_CONSTRAINT_ATTRIBUTE_WIDTH);
      var_height = get_child_attribute (self, child, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT);
      var_baseline = get_child_attribute (self, child, GTK_CONSTRAINT_ATTRIBUTE_BASELINE);

      GTK_NOTE (LAYOUT,
                g_print ("Allocating child '%s'[%p] with { .x: %g, .y: %g, .w: %g, .h: %g, .b: %g }\n",
                         gtk_widget_get_name (child), child,
                         gtk_constraint_variable_get_value (var_left),
                         gtk_constraint_variable_get_value (var_top),
                         gtk_constraint_variable_get_value (var_width),
                         gtk_constraint_variable_get_value (var_height),
                         gtk_constraint_variable_get_value (var_baseline)));

      child_alloc.x = floor (gtk_constraint_variable_get_value (var_left));
      child_alloc.y = floor (gtk_constraint_variable_get_value (var_top));
      child_alloc.width = ceil (gtk_constraint_variable_get_value (var_width));
      child_alloc.height = ceil (gtk_constraint_variable_get_value (var_height));

      if (gtk_constraint_variable_get_value (var_baseline) > 0)
        child_baseline = floor (gtk_constraint_variable_get_value (var_baseline));

      gtk_widget_size_allocate (GTK_WIDGET (child),
                                &child_alloc,
                                child_baseline);
    }

#ifdef G_ENABLE_DEBUG
  if (GTK_DEBUG_CHECK (LAYOUT))
    {
      GHashTableIter iter;
      gpointer key;
      g_hash_table_iter_init (&iter, self->guides);
      while (g_hash_table_iter_next (&iter, &key, NULL))
        {
          GtkConstraintGuide *guide = key;
          GtkConstraintVariable *var_top, *var_left, *var_width, *var_height;
          var_top = gtk_constraint_guide_get_attribute (guide, GTK_CONSTRAINT_ATTRIBUTE_TOP);
          var_left = gtk_constraint_guide_get_attribute (guide, GTK_CONSTRAINT_ATTRIBUTE_LEFT);
          var_width = gtk_constraint_guide_get_attribute (guide, GTK_CONSTRAINT_ATTRIBUTE_WIDTH);
          var_height = gtk_constraint_guide_get_attribute (guide, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT);
          g_print ("Allocating guide '%s'[%p] with { .x: %g .y: %g .w: %g .h: %g }\n",
                   gtk_constraint_guide_get_name (guide), guide,
                   gtk_constraint_variable_get_value (var_left),
                   gtk_constraint_variable_get_value (var_top),
                   gtk_constraint_variable_get_value (var_width),
                   gtk_constraint_variable_get_value (var_height));
        }
    }
#endif

  /* The allocation stay constraints are not needed any more */
  gtk_constraint_solver_remove_constraint (solver, stay_w);
  gtk_constraint_solver_remove_constraint (solver, stay_h);
  gtk_constraint_solver_remove_constraint (solver, stay_t);
  gtk_constraint_solver_remove_constraint (solver, stay_l);
}

static void
gtk_constraint_layout_root (GtkLayoutManager *manager)
{
  GtkConstraintLayout *self = GTK_CONSTRAINT_LAYOUT (manager);
  GHashTableIter iter;
  GtkWidget *widget;
  GtkRoot *root;
  gpointer key;

  widget = gtk_layout_manager_get_widget (manager);
  root = gtk_widget_get_root (widget);

  self->solver = gtk_root_get_constraint_solver (root);

  /* Now that we have a solver, attach all constraints we have */
  g_hash_table_iter_init (&iter, self->constraints);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GtkConstraint *constraint = key;
      layout_add_constraint (self, constraint);
    }

  g_hash_table_iter_init (&iter, self->guides);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GtkConstraintGuide *guide = key;
      gtk_constraint_guide_update (guide);
    }
}

static void
gtk_constraint_layout_unroot (GtkLayoutManager *manager)
{
  GtkConstraintLayout *self = GTK_CONSTRAINT_LAYOUT (manager);
  GHashTableIter iter;
  gpointer key;

  /* Detach all constraints we're holding, as we're removing the layout
   * from the global solver, and they should not contribute to the other
   * layouts
   */
  g_hash_table_iter_init (&iter, self->constraints);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GtkConstraint *constraint = key;
      gtk_constraint_detach (constraint);
    }

  g_hash_table_iter_init (&iter, self->guides);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GtkConstraintGuide *guide = key;
      gtk_constraint_guide_detach (guide);
    }

  self->solver = NULL;
}

static void
gtk_constraint_layout_class_init (GtkConstraintLayoutClass *klass)
{
  GtkLayoutManagerClass *manager_class = GTK_LAYOUT_MANAGER_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = gtk_constraint_layout_finalize;

  manager_class->layout_child_type = GTK_TYPE_CONSTRAINT_LAYOUT_CHILD;
  manager_class->measure = gtk_constraint_layout_measure;
  manager_class->allocate = gtk_constraint_layout_allocate;
  manager_class->root = gtk_constraint_layout_root;
  manager_class->unroot = gtk_constraint_layout_unroot;
}

static void
gtk_constraint_layout_init (GtkConstraintLayout *self)
{
  /* The bound variables in the solver */
  self->bound_attributes =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           NULL,
                           (GDestroyNotify) gtk_constraint_variable_unref);

  /* The GtkConstraint instances we own */
  self->constraints =
    g_hash_table_new_full (NULL, NULL,
                           (GDestroyNotify) g_object_unref,
                           NULL);

  self->guides =
    g_hash_table_new_full (NULL, NULL,
                           (GDestroyNotify) g_object_unref,
                           NULL);
}

typedef struct {
  GtkConstraintLayout *layout;
  GtkBuilder *builder;
  GList *constraints;
} ConstraintsParserData;

typedef struct {
  char *source_name;
  char *source_attr;
  char *target_name;
  char *target_attr;
  char *relation;
  char *strength;
  double constant;
  double multiplier;
} ConstraintData;

static void
constraint_data_free (gpointer _data)
{
  ConstraintData *data = _data;

  g_free (data->source_name);
  g_free (data->source_attr);
  g_free (data->target_name);
  g_free (data->target_attr);
  g_free (data->relation);
  g_free (data->strength);

  g_free (data);
}

static void
parse_double (const char *string,
              double     *value_p,
              double      default_value)
{
  double value;
  char *endptr;
  int saved_errno;

  if (string == NULL || string[0] == '\0')
    {
      *value_p = default_value;
      return;
    }

  saved_errno = errno;
  errno = 0;
  value = g_ascii_strtod (string, &endptr);
  if (errno == 0 && endptr != string)
    *value_p = value;
  else
    *value_p = default_value;

  errno = saved_errno;
}

static GtkConstraint *
constraint_data_to_constraint (const ConstraintData *data,
                               GtkBuilder           *builder,
                               GError              **error)
{
  gpointer source, target;
  int source_attr, target_attr;
  int relation, strength;
  gboolean res;

  if (g_strcmp0 (data->source_name, "super") == 0)
    source = NULL;
  else if (data->source_name == NULL)
    {
      if (data->source_attr != NULL)
        {
          g_set_error (error, GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Constraints without 'source' must also not "
                       "have a 'source-attribute' attribute");
          return NULL;
        }

      source = NULL;
    }
  else
    source = gtk_builder_get_object (builder, data->source_name);

  if (g_strcmp0 (data->target_name, "super") == 0)
    target = NULL;
  else
    {
      target = gtk_builder_get_object (builder, data->target_name);

      if (target == NULL)
        {
          g_set_error (error, GTK_BUILDER_ERROR,
                       GTK_BUILDER_ERROR_INVALID_VALUE,
                       "Unable to find target '%s' for constraint",
                       data->target_name);
          return NULL;
        }
    }

  if (data->source_attr != NULL)
    {
      res = _gtk_builder_enum_from_string (GTK_TYPE_CONSTRAINT_ATTRIBUTE,
                                           data->source_attr,
                                           &source_attr,
                                           error);
      if (!res)
        return NULL;
    }
  else
    source_attr = GTK_CONSTRAINT_ATTRIBUTE_NONE;

  res = _gtk_builder_enum_from_string (GTK_TYPE_CONSTRAINT_ATTRIBUTE,
                                       data->target_attr,
                                       &target_attr,
                                       error);
  if (!res)
    return NULL;

  if (data->relation != NULL)
    {
      res = _gtk_builder_enum_from_string (GTK_TYPE_CONSTRAINT_RELATION,
                                           data->relation,
                                           &relation,
                                           error);
      if (!res)
        return NULL;
    }
  else
    relation = GTK_CONSTRAINT_RELATION_EQ;

  if (data->strength != NULL)
    {
      res = _gtk_builder_enum_from_string (GTK_TYPE_CONSTRAINT_STRENGTH,
                                           data->strength,
                                           &strength,
                                           error);
    }
  else
    strength = GTK_CONSTRAINT_STRENGTH_REQUIRED;

  if (source != NULL && source_attr != GTK_CONSTRAINT_ATTRIBUTE_NONE)
    return gtk_constraint_new (target, target_attr,
                               relation,
                               source, source_attr,
                               data->multiplier,
                               data->constant,
                               strength);

  return gtk_constraint_new_constant (target, target_attr,
                                      relation,
                                      data->constant,
                                      strength);
}

static void
constraints_start_element (GMarkupParseContext  *context,
                           const char           *element_name,
                           const char          **attr_names,
                           const char          **attr_values,
                           gpointer              user_data,
                           GError              **error)
{
  ConstraintsParserData *data = user_data;

  if (strcmp (element_name, "constraints") == 0)
    {
      if (!_gtk_builder_check_parent (data->builder, context, "object", error))
        return;

      if (!g_markup_collect_attributes (element_name, attr_names, attr_values, error,
                                        G_MARKUP_COLLECT_INVALID, NULL, NULL,
                                        G_MARKUP_COLLECT_INVALID))
        _gtk_builder_prefix_error (data->builder, context, error);
    }
  else if (strcmp (element_name, "constraint") == 0)
    {
      const char *target_name, *target_attribute;
      const char *relation_str = NULL;
      const char *source_name = NULL, *source_attribute = NULL;
      const char *multiplier_str = NULL, *constant_str = NULL;
      const char *strength_str = NULL;
      ConstraintData *cdata;

      if (!_gtk_builder_check_parent (data->builder, context, "constraints", error))
        return;

      if (!g_markup_collect_attributes (element_name, attr_names, attr_values, error,
                                        G_MARKUP_COLLECT_STRING, "target", &target_name,
                                        G_MARKUP_COLLECT_STRING, "target-attribute", &target_attribute,
                                        G_MARKUP_COLLECT_STRING, "relation", &relation_str,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "source", &source_name,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "source-attribute", &source_attribute,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "multiplier", &multiplier_str,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "constant", &constant_str,
                                        G_MARKUP_COLLECT_STRING | G_MARKUP_COLLECT_OPTIONAL, "strength", &strength_str,
                                        G_MARKUP_COLLECT_INVALID))
        {
          _gtk_builder_prefix_error (data->builder, context, error);
          return;
        }

      cdata = g_new0 (ConstraintData, 1);
      cdata->target_name = g_strdup (target_name);
      cdata->target_attr = g_strdup (target_attribute);
      cdata->relation = g_strdup (relation_str);
      cdata->source_name = g_strdup (source_name);
      cdata->source_attr = g_strdup (source_attribute);
      parse_double (multiplier_str, &cdata->multiplier, 1.0);
      parse_double (constant_str, &cdata->constant, 0.0);
      cdata->strength = g_strdup (strength_str);

      data->constraints = g_list_prepend (data->constraints, cdata);
    }
  else
    {
      _gtk_builder_error_unhandled_tag (data->builder, context,
                                        "GtkConstraintLayout", element_name,
                                        error);
    }
}

static void
constraints_end_element (GMarkupParseContext  *context,
                         const char           *element_name,
                         gpointer              user_data,
                         GError              **error)
{
}

static const GMarkupParser constraints_parser = {
  constraints_start_element,
  constraints_end_element,
  NULL,
};

static gboolean
gtk_constraint_layout_custom_tag_start (GtkBuildable  *buildable,
                                        GtkBuilder    *builder,
                                        GObject       *child,
                                        const char    *element_name,
                                        GMarkupParser *parser,
                                        gpointer      *parser_data)
{
  if (strcmp (element_name, "constraints") == 0)
    {
      ConstraintsParserData *data = g_new (ConstraintsParserData, 1);

      data->layout = g_object_ref (GTK_CONSTRAINT_LAYOUT (buildable));
      data->builder = builder;
      data->constraints = NULL;

      *parser = constraints_parser;
      *parser_data = data;

      return TRUE;
    }

  return FALSE;
}

static void
gtk_constraint_layout_custom_tag_end (GtkBuildable *buildable,
                                      GtkBuilder   *builder,
                                      GObject      *child,
                                      const char   *element_name,
                                      gpointer      data)
{
}

static void
gtk_constraint_layout_custom_finished (GtkBuildable *buildable,
                                       GtkBuilder   *builder,
                                       GObject      *child,
                                       const char   *element_name,
                                       gpointer      user_data)
{
  ConstraintsParserData *data = user_data;

  if (strcmp (element_name, "constraints") == 0)
    {
      GList *l;

      data->constraints = g_list_reverse (data->constraints);
      for (l = data->constraints; l != NULL; l = l->next)
        {
          const ConstraintData *cdata = l->data;
          GtkConstraint *c;
          GError *error = NULL;

          c = constraint_data_to_constraint (cdata, builder, &error);
          if (error != NULL)
            {
              g_critical ("Unable to parse constraint definition '%s.%s [%s] %s.%s * %g + %g': %s",
                          cdata->target_name, cdata->target_attr,
                          cdata->relation,
                          cdata->source_name, cdata->source_attr,
                          cdata->multiplier,
                          cdata->constant,
                          error->message);
              g_error_free (error);
              continue;
            }

          gtk_constraint_layout_add_constraint (data->layout, c);
        }

      g_list_free_full (data->constraints, constraint_data_free);
      g_object_unref (data->layout);
      g_free (data);
    }
}

static void
gtk_buildable_interface_init (GtkBuildableIface *iface)
{
  iface->custom_tag_start = gtk_constraint_layout_custom_tag_start;
  iface->custom_tag_end = gtk_constraint_layout_custom_tag_end;
  iface->custom_finished = gtk_constraint_layout_custom_finished;
}

/**
 * gtk_constraint_layout_new:
 *
 * Creates a new #GtkConstraintLayout layout manager.
 *
 * Returns: the newly created #GtkConstraintLayout
 */
GtkLayoutManager *
gtk_constraint_layout_new (void)
{
  return g_object_new (GTK_TYPE_CONSTRAINT_LAYOUT, NULL);
}

/**
 * gtk_constraint_layout_add_constraint:
 * @layout: a #GtkConstraintLayout
 * @constraint: (transfer full): a #GtkConstraint
 *
 * Adds a #GtkConstraint to the layout manager.
 *
 * The #GtkConstraint:source and #GtkConstraint:target
 * properties of @constraint can be:
 *
 *  - set to %NULL to indicate that the constraint refers to the
 *    widget using @layout
 *  - set to the #GtkWidget using @layout
 *  - set to a child of the #GtkWidget using @layout
 *  - set to a guide that is part of @layout
 *
 * The @layout acquires the ownership of @constraint after calling
 * this function.
 */
void
gtk_constraint_layout_add_constraint (GtkConstraintLayout *layout,
                                      GtkConstraint       *constraint)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_LAYOUT (layout));
  g_return_if_fail (GTK_IS_CONSTRAINT (constraint));
  g_return_if_fail (!gtk_constraint_is_attached (constraint));

  layout_add_constraint (layout, constraint);

  g_hash_table_add (layout->constraints, constraint);

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (layout));
}

/**
 * gtk_constraint_layout_remove_constraint:
 * @layout: a #GtkConstraintLayout
 * @constraint: a #GtkConstraint
 *
 * Removes @constraint from the layout manager,
 * so that it no longer influences the layout.
 */
void
gtk_constraint_layout_remove_constraint (GtkConstraintLayout *layout,
                                         GtkConstraint       *constraint)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_LAYOUT (layout));
  g_return_if_fail (GTK_IS_CONSTRAINT (constraint));
  g_return_if_fail (gtk_constraint_is_attached (constraint));

  gtk_constraint_detach (constraint);
  g_hash_table_remove (layout->constraints, constraint);

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (layout));
}

/**
 * gtk_constraint_layout_remove_all_constraints:
 * @layout: a #GtkConstraintLayout
 *
 * Removes all constraints from the layout manager.
 */
void
gtk_constraint_layout_remove_all_constraints (GtkConstraintLayout *layout)
{
  GHashTableIter iter;
  gpointer key;

  g_return_if_fail (GTK_IS_CONSTRAINT_LAYOUT (layout));

  g_hash_table_iter_init (&iter, layout->constraints);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GtkConstraint *constraint = key;

      gtk_constraint_detach (constraint);
      g_hash_table_iter_remove (&iter);
    }

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (layout));
}

/**
 * gtk_constraint_layout_add_guide:
 * @layout: a #GtkConstraintLayout
 * @guide: (transfer full): a #GtkConstraintGuide object
 *
 * Adds a guide to @layout. A guide can be used as
 * the source or target of constraints, like a widget,
 * but it is not visible.
 *
 * The @layout acquires the ownership of @guide after calling
 * this function.
 */
void
gtk_constraint_layout_add_guide (GtkConstraintLayout *layout,
                                 GtkConstraintGuide  *guide)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_LAYOUT (layout));
  g_return_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide));
  g_return_if_fail (gtk_constraint_guide_get_layout (guide) == NULL);

  gtk_constraint_guide_set_layout (guide, layout);
  g_hash_table_add (layout->guides, guide);

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (layout));
}

/**
 * gtk_constraint_layout_remove_guide:
 * @layout: a #GtkConstraintManager
 * @guide: a #GtkConstraintGuide object
 *
 * Removes @guide from the layout manager,
 * so that it no longer influences the layout.
 */
void
gtk_constraint_layout_remove_guide (GtkConstraintLayout *layout,
                                    GtkConstraintGuide  *guide)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_LAYOUT (layout));
  g_return_if_fail (GTK_IS_CONSTRAINT_GUIDE (guide));
  g_return_if_fail (gtk_constraint_guide_get_layout (guide) == layout);

  gtk_constraint_guide_detach (guide);

  gtk_constraint_guide_set_layout (guide, NULL);
  g_hash_table_remove (layout->guides, guide);

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (layout));
}

static GtkConstraintAttribute
attribute_from_name (const char *name)
{
  if (name == NULL || *name == '\0')
    return GTK_CONSTRAINT_ATTRIBUTE_NONE;

  /* We sadly need to special case these two because the name does
   * not match the VFL grammar rules
   */
  if (strcmp (name, "centerX") == 0)
    return GTK_CONSTRAINT_ATTRIBUTE_CENTER_X;

  if (strcmp (name, "centerY") == 0)
    return GTK_CONSTRAINT_ATTRIBUTE_CENTER_Y;

  for (int i = 0; i < G_N_ELEMENTS (attribute_names); i++)
    {
      if (strcmp (attribute_names[i], name) == 0)
        return i;
    }

  return GTK_CONSTRAINT_ATTRIBUTE_NONE;
}

GQuark
gtk_constraint_vfl_parser_error_quark (void)
{
  return g_quark_from_static_string ("gtk-constraint-vfl-parser-error-quark");
}

/**
 * gtk_constraint_layout_add_constraints_from_descriptionv: (rename-to gtk_constraint_layout_add_constraints_from_description)
 * @layout: a #GtkConstraintLayout
 * @lines: (array length=n_lines): an array of Visual Format Language lines
 *   defining a set of constraints
 * @n_lines: the number of lines
 * @hspacing: default horizontal spacing value, or -1 for the fallback value
 * @vspacing: default vertical spacing value, or -1 for the fallback value
 * @views: (element-type utf8 Gtk.ConstraintTarget): a dictionary of [ name, target ]
 *   pairs; the `name` keys map to the view names in the VFL lines, while
 *   the `target` values map to children of the widget using a #GtkConstraintLayout, or guides
 * @error: return location for a #GError
 *
 * Creates a list of constraints from a formal description using a compact
 * description syntax called VFL, or "Visual Format Language".
 *
 * The Visual Format Language is based on Apple's AutoLayout [VFL](https://developer.apple.com/library/content/documentation/UserExperience/Conceptual/AutolayoutPG/VisualFormatLanguage.html).
 *
 * The @views dictionary is used to match #GtkConstraintTargets to the symbolic
 * view name inside the VFL.
 *
 * The VFL grammar is:
 *
 * |[<!-- language="plain" -->
 *        <visualFormatString> = (<orientation>)?
 *                               (<superview><connection>)?
 *                               <view>(<connection><view>)*
 *                               (<connection><superview>)?
 *               <orientation> = 'H' | 'V'
 *                 <superview> = '|'
 *                <connection> = '' | '-' <predicateList> '-' | '-'
 *             <predicateList> = <simplePredicate> | <predicateListWithParens>
 *           <simplePredicate> = <metricName> | <positiveNumber>
 *   <predicateListWithParens> = '(' <predicate> (',' <predicate>)* ')'
 *                 <predicate> = (<relation>)? <objectOfPredicate> (<operatorList>)? ('@' <priority>)?
 *                  <relation> = '==' | '<=' | '>='
 *         <objectOfPredicate> = <constant> | <viewName> | ('.' <attributeName>)?
 *                  <priority> = <positiveNumber> | 'required' | 'strong' | 'medium' | 'weak'
 *                  <constant> = <number>
 *              <operatorList> = (<multiplyOperator>)? (<addOperator>)?
 *          <multiplyOperator> = [ '*' | '/' ] <positiveNumber>
 *               <addOperator> = [ '+' | '-' ] <positiveNumber>
 *                  <viewName> = [A-Za-z_]([A-Za-z0-9_]*) // A C identifier
 *                <metricName> = [A-Za-z_]([A-Za-z0-9_]*) // A C identifier
 *             <attributeName> = 'top' | 'bottom' | 'left' | 'right' | 'width' | 'height' |
 *                               'start' | 'end' | 'centerX' | 'centerY' | 'baseline'
 *            <positiveNumber> // A positive real number parseable by g_ascii_strtod()
 *                    <number> // A real number parseable by g_ascii_strtod()
 * ]|
 *
 * **Note**: The VFL grammar used by GTK is slightly different than the one
 * defined by Apple, as it can use symbolic values for the constraint's
 * strength instead of numeric values; additionally, GTK allows adding
 * simple arithmetic operations inside predicates.
 *
 * Examples of VFL descriptions are:
 *
 * |[<!-- language="plain" -->
 *   // Default spacing
 *   [button]-[textField]
 *
 *   // Width constraint
 *   [button(>=50)]
 *
 *   // Connection to super view
 *   |-50-[purpleBox]-50-|
 *
 *   // Vertical layout
 *   V:[topField]-10-[bottomField]
 *
 *   // Flush views
 *   [maroonView][blueView]
 *
 *   // Priority
 *   [button(100@strong)]
 *
 *   // Equal widths
 *   [button1(==button2)]
 *
 *   // Multiple predicates
 *   [flexibleButton(>=70,<=100)]
 *
 *   // A complete line of layout
 *   |-[find]-[findNext]-[findField(>=20)]-|
 *
 *   // Operators
 *   [button1(button2 / 3 + 50)]
 *
 *   // Named attributes
 *   [button1(==button2.height)]
 * ]|
 *
 * Returns: (transfer container) (element-type GtkConstraint): the list of
 *   #GtkConstraints that were added to the layout
 */
GList *
gtk_constraint_layout_add_constraints_from_descriptionv (GtkConstraintLayout *layout,
                                                         const char * const   lines[],
                                                         gsize                n_lines,
                                                         int                  hspacing,
                                                         int                  vspacing,
                                                         GHashTable          *views,
                                                         GError             **error)
{
  GtkConstraintVflParser *parser;
  GList *res = NULL;

  g_return_val_if_fail (GTK_IS_CONSTRAINT_LAYOUT (layout), NULL);
  g_return_val_if_fail (lines != NULL, NULL);
  g_return_val_if_fail (views != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  parser = gtk_constraint_vfl_parser_new ();
  gtk_constraint_vfl_parser_set_default_spacing (parser, hspacing, vspacing);
  gtk_constraint_vfl_parser_set_views (parser, views);

  for (gsize i = 0; i < n_lines; i++)
    {
      const char *line = lines[i];
      GError *internal_error = NULL;

      gtk_constraint_vfl_parser_parse_line (parser, line, -1, &internal_error);
      if (internal_error != NULL)
        {
          int offset = gtk_constraint_vfl_parser_get_error_offset (parser);
          int range = gtk_constraint_vfl_parser_get_error_range (parser);
          char *squiggly = NULL;

          if (range > 0)
            {
              squiggly = g_new (char, range + 1);

              for (int r = 0; r < range; i++)
                squiggly[r] = '~';

              squiggly[range] = '\0';
            }

          g_set_error (error, GTK_CONSTRAINT_VFL_PARSER_ERROR,
                       internal_error->code,
                       "%" G_GSIZE_FORMAT ":%d: %s\n"
                       "%s\n"
                       "%*s^%s",
                       i, offset + 1,
                       internal_error->message,
                       line,
                       offset, " ", squiggly != NULL ? squiggly : "");

          g_free (squiggly);
          g_error_free (internal_error);
          gtk_constraint_vfl_parser_free (parser);
          return res;
        }

      int n_constraints = 0;
      GtkConstraintVfl *constraints = gtk_constraint_vfl_parser_get_constraints (parser, &n_constraints);
      for (int j = 0; j < n_constraints; j++)
        {
          const GtkConstraintVfl *c = &constraints[j];
          gpointer source, target;
          GtkConstraintAttribute source_attr, target_attr;

          target = g_hash_table_lookup (views, c->view1);
          target_attr = attribute_from_name (c->attr1);

          if (c->view2 != NULL)
            source = g_hash_table_lookup (views, c->view2);
          else
            source = NULL;

          if (c->attr2 != NULL)
            source_attr = attribute_from_name (c->attr2);
          else
            source_attr = GTK_CONSTRAINT_ATTRIBUTE_NONE;

          GtkConstraint *constraint =
            gtk_constraint_new (target, target_attr,
                                c->relation,
                                source, source_attr,
                                c->multiplier,
                                c->constant,
                                c->strength);

          layout_add_constraint (layout, constraint);
          g_hash_table_add (layout->constraints, constraint);

          res = g_list_prepend (res, constraint);
        }

      g_free (constraints);
    }

  gtk_constraint_vfl_parser_free (parser);

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (layout));

  return res;
}

/**
 * gtk_constraint_layout_add_constraints_from_description:
 * @layout: a #GtkConstraintLayout
 * @lines: (array length=n_lines): an array of Visual Format Language lines
 *   defining a set of constraints
 * @n_lines: the number of lines
 * @hspacing: default horizontal spacing value, or -1 for the fallback value
 * @vspacing: default vertical spacing value, or -1 for the fallback value
 * @error: return location for a #GError
 * @first_view: the name of a view in the VFL description, followed by the
 *   #GtkConstraintTarget to which it maps
 * @...: a %NULL-terminated list of view names and #GtkConstraintTargets
 *
 * Creates a list of constraints they formal description using a compact
 * description syntax called VFL, or "Visual Format Language".
 *
 * This function is a convenience wrapper around
 * gtk_constraint_layout_add_constraints_from_descriptionv(), using
 * variadic arguments to populate the view/target map.
 *
 * Returns: (transfer container) (element-type GtkConstraint): the list of
 *   #GtkConstraints that were added to the layout
 */
GList *
gtk_constraint_layout_add_constraints_from_description (GtkConstraintLayout *layout,
                                                        const char * const   lines[],
                                                        gsize                n_lines,
                                                        int                  hspacing,
                                                        int                  vspacing,
                                                        GError             **error,
                                                        const char          *first_view,
                                                        ...)
{
  GtkConstraintVflParser *parser;
  GHashTable *views;
  const char *view;
  GList *res;
  va_list args;

  g_return_val_if_fail (GTK_IS_CONSTRAINT_LAYOUT (layout), NULL);
  g_return_val_if_fail (lines != NULL, NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);
  g_return_val_if_fail (first_view != NULL, NULL);

  parser = gtk_constraint_vfl_parser_new ();
  gtk_constraint_vfl_parser_set_default_spacing (parser, hspacing, vspacing);

  views = g_hash_table_new (g_str_hash, g_str_equal);

  va_start (args, first_view);

  view = first_view;
  while (view != NULL)
    {
      GtkConstraintTarget *target = va_arg (args, GtkConstraintTarget *);

      if (target == NULL)
        break;

      g_hash_table_insert (views, (gpointer) view, target);

      view = va_arg (args, const char *);
    }

  va_end (args);

  res =
    gtk_constraint_layout_add_constraints_from_descriptionv (layout, lines, n_lines,
                                                             hspacing, vspacing,
                                                             views,
                                                             error);

  g_hash_table_unref (views);

  return res;
}
