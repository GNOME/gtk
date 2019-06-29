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
 * and lead to an unstable layout.
 *
 */

#include "config.h"

#include "gtkconstraintlayout.h"
#include "gtkconstraintlayoutprivate.h"

#include "gtkconstraintprivate.h"
#include "gtkgridconstraintprivate.h"
#include "gtkconstraintexpressionprivate.h"
#include "gtkconstraintsolverprivate.h"
#include "gtklayoutchild.h"
#include "gtkconstraintguideprivate.h"

#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtkprivate.h"
#include "gtksizerequest.h"
#include "gtkwidgetprivate.h"

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

  /* HashSet<GtkGridConstraint> */
  GHashTable *grid_constraints;
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

  //g_print ("create variable %s.%s\n", prefix, attr_name);
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

        //g_print ("add constraint %s.right = %s.left + %s.width\n", prefix, prefix, prefix);
        gtk_constraint_solver_add_constraint (solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
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

        //g_print ("add constraint %s.bottom = %s.top + %s.height\n", prefix, prefix, prefix);
        gtk_constraint_solver_add_constraint (solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
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

        //g_print ("add constraint %s.centerx = %s.width/2 + %s.left\n", prefix, prefix, prefix);
        gtk_constraint_solver_add_constraint (solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
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

        //g_print ("add constraint %s.centery = %s.height/2 + %s.top\n", prefix, prefix, prefix);
        gtk_constraint_solver_add_constraint (solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
      }
      break;

    /* We do not allow negative sizes */
    case GTK_CONSTRAINT_ATTRIBUTE_WIDTH:
    case GTK_CONSTRAINT_ATTRIBUTE_HEIGHT:
      {
        GtkConstraintExpression *expr;

        expr = gtk_constraint_expression_new (0.0);
        //g_print ("add constraint %s.%s >= 0\n", prefix, attr == GTK_CONSTRAINT_ATTRIBUTE_WIDTH ? "width" : "height");
        gtk_constraint_solver_add_constraint (solver,
                                              res, GTK_CONSTRAINT_RELATION_GE, expr,
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
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

G_DEFINE_TYPE (GtkConstraintLayout, gtk_constraint_layout, GTK_TYPE_LAYOUT_MANAGER)

static void
gtk_constraint_layout_finalize (GObject *gobject)
{
  GtkConstraintLayout *self = GTK_CONSTRAINT_LAYOUT (gobject);

  g_clear_pointer (&self->bound_attributes, g_hash_table_unref);
  g_clear_pointer (&self->constraints, g_hash_table_unref);
  g_clear_pointer (&self->guides, g_hash_table_unref);
  g_clear_pointer (&self->grid_constraints, g_hash_table_unref);

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

  //g_print ("create variable super.%s\n", attr_name);
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

        //g_print ("add constraint super.right = super.left + super.width\n");
        gtk_constraint_solver_add_constraint (self->solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
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

        //g_print ("add constraint super.bottom = super.top + super.height\n");
        gtk_constraint_solver_add_constraint (self->solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
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

        //g_print ("add constraint super.centerx = super.left + super.width/2\n");
        gtk_constraint_solver_add_constraint (self->solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
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

        //g_print ("add constraint super.centery = super.top + super.height/2\n");
        gtk_constraint_solver_add_constraint (self->solver,
                                              res, GTK_CONSTRAINT_RELATION_EQ, expr,
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
      }
      break;

    /* We do not allow negative sizes */
    case GTK_CONSTRAINT_ATTRIBUTE_WIDTH:
    case GTK_CONSTRAINT_ATTRIBUTE_HEIGHT:
      {
        GtkConstraintExpression *expr;

        expr = gtk_constraint_expression_new (0.0);
        //g_print ("add constraint super.%s >= 0\n", attr == GTK_CONSTRAINT_ATTRIBUTE_WIDTH ? "width" : "height");
        gtk_constraint_solver_add_constraint (self->solver,
                                              res, GTK_CONSTRAINT_RELATION_GE, expr,
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
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
  //g_print ("add constraint\n");
  constraint->constraint_ref =
    gtk_constraint_solver_add_constraint (self->solver,
                                          target_attr,
                                          gtk_constraint_get_relation (constraint),
                                          expr,
                                          gtk_constraint_get_weight (constraint));
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
        {
          g_print ("remove constraint\n");
          gtk_constraint_solver_remove_constraint (self->solver,
                                                   child_info->constraints[index]);
        }

      var = get_child_attribute (self, child, attr[index]);

      if (relation[index] == GTK_CONSTRAINT_RELATION_EQ)
        {
          g_print ("set variable\n");
          gtk_constraint_variable_set_value (var, value);
          g_print ("add stay variable\n");
          child_info->constraints[index] =
            gtk_constraint_solver_add_stay_variable (self->solver,
                                                     var,
                                                     GTK_CONSTRAINT_WEIGHT_MEDIUM);
        }
      else
        {
          g_print ("add constraint\n");
          child_info->constraints[index] =
            gtk_constraint_solver_add_constraint (self->solver,
                                                  var,
                                                  relation[index],
                                                  gtk_constraint_expression_new (value),
                                                  GTK_CONSTRAINT_WEIGHT_REQUIRED);
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
  gint64 start, end;

  solver = gtk_constraint_layout_get_solver (self);
  if (solver == NULL)
    return;

  start = g_get_monotonic_time ();
  g_print ("start measure %s\n", orientation ? "v" : "h");
  g_print ("freeze\n");
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

  g_print ("thaw\n");
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
  g_print ("add edit variable\n");
  gtk_constraint_solver_add_edit_variable (solver, size, GTK_CONSTRAINT_WEIGHT_STRONG * 2);
  if (for_size > 0)
    {
      g_print ("add edit variable\n");
      gtk_constraint_solver_add_edit_variable (solver, opposite_size, GTK_CONSTRAINT_WEIGHT_STRONG * 2);
    }
  g_print ("begin edit\n");
  gtk_constraint_solver_begin_edit (solver);
  g_print ("suggest value\n");
  gtk_constraint_solver_suggest_value (solver, size, 0.0);
  if (for_size > 0)
    {
      g_print ("suggest value\n");
      gtk_constraint_solver_suggest_value (solver, opposite_size, for_size);
    }
  g_print ("resolve\n");
  gtk_constraint_solver_resolve (solver);

  min_value = gtk_constraint_variable_get_value (size);

  g_print ("remove edit variable\n");
  gtk_constraint_solver_remove_edit_variable (solver, size);
  if (for_size > 0)
    {
      g_print ("remove edit variable\n");
      gtk_constraint_solver_remove_edit_variable (solver, opposite_size);
    }
  g_print ("end edit\n");
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

  end = g_get_monotonic_time ();
  g_print ("end measure (%ld ms)\n", (end - start) / 1000);
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
  gint64 start, end;

  start = g_get_monotonic_time ();
  g_print ("start allocate\n");

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

  g_print ("set variable\n");
  gtk_constraint_variable_set_value (layout_top, 0.0);
  g_print ("add stay variable\n");
  stay_t = gtk_constraint_solver_add_stay_variable (solver,
                                                    layout_top,
                                                    GTK_CONSTRAINT_WEIGHT_REQUIRED);
  g_print ("set variable\n");
  gtk_constraint_variable_set_value (layout_left, 0.0);
  g_print ("add stay variable\n");
  stay_l = gtk_constraint_solver_add_stay_variable (solver,
                                                    layout_left,
                                                    GTK_CONSTRAINT_WEIGHT_REQUIRED);
  g_print ("set variable\n");
  gtk_constraint_variable_set_value (layout_width, width);
  g_print ("add stay variable\n");
  stay_w = gtk_constraint_solver_add_stay_variable (solver,
                                                    layout_width,
                                                    GTK_CONSTRAINT_WEIGHT_REQUIRED);
  g_print ("set variable\n");
  gtk_constraint_variable_set_value (layout_height, height);
  g_print ("add stay variable\n");
  stay_h = gtk_constraint_solver_add_stay_variable (solver,
                                                    layout_height,
                                                    GTK_CONSTRAINT_WEIGHT_REQUIRED);
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
  g_print ("remove constraint\n");
  gtk_constraint_solver_remove_constraint (solver, stay_w);
  g_print ("remove constraint\n");
  gtk_constraint_solver_remove_constraint (solver, stay_h);
  g_print ("remove constraint\n");
  gtk_constraint_solver_remove_constraint (solver, stay_t);
  g_print ("remove constraint\n");
  gtk_constraint_solver_remove_constraint (solver, stay_l);

  end = g_get_monotonic_time ();
  g_print ("end allocate (%ld ms)\n", (end - start) / 1000);

  {
    g_autofree char *stats = gtk_constraint_solver_statistics (solver);
    g_print ("Stats: %s\n", stats);
  }
}

static void layout_add_grid_constraint (GtkConstraintLayout *manager,
                                        GtkGridConstraint   *constraint);

static void
gtk_constraint_layout_root (GtkLayoutManager *manager)
{
  GtkConstraintLayout *self = GTK_CONSTRAINT_LAYOUT (manager);
  GHashTableIter iter;
  GtkWidget *widget;
  GtkRoot *root;
  gpointer key;
  gint64 start = g_get_monotonic_time ();

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

  g_hash_table_iter_init (&iter, self->grid_constraints);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GtkGridConstraint *constraint = key;
      layout_add_grid_constraint (self, constraint);
    }

  g_print ("root (%ld ms)\n", (g_get_monotonic_time () - start) / 1000);
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

  g_hash_table_iter_init (&iter, self->grid_constraints);
  while (g_hash_table_iter_next (&iter, &key, NULL))
    {
      GtkGridConstraint *constraint = key;
      gtk_grid_constraint_detach (constraint);
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

  self->grid_constraints =
    g_hash_table_new_full (NULL, NULL,
                           (GDestroyNotify) g_object_unref,
                           NULL);
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
 * @manager: a #GtkConstraintLayout
 * @constraint: (transfer full): a #GtkConstraint
 *
 * Adds a #GtkConstraint to the layout manager.
 *
 * The #GtkConstraint:source and #GtkConstraint:target
 * properties of @constraint can be:
 *
 *  - set to %NULL to indicate that the constraint refers to the
 *    widget using @manager
 *  - set to the #GtkWidget using @manager
 *  - set to a child of the #GtkWidget using @manager
 *
 * The @manager acquires the ownership of @constraint after calling
 * this function.
 */
void
gtk_constraint_layout_add_constraint (GtkConstraintLayout *manager,
                                      GtkConstraint       *constraint)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_LAYOUT (manager));
  g_return_if_fail (GTK_IS_CONSTRAINT (constraint));
  g_return_if_fail (!gtk_constraint_is_attached (constraint));

  layout_add_constraint (manager, constraint);

  g_hash_table_add (manager->constraints, constraint);

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (manager));
}

/**
 * gtk_constraint_layout_remove_constraint:
 * @manager: a #GtkConstraintLayout
 * @constraint: a #GtkConstraint
 *
 * Removes @constraint from the layout manager,
 * so that it no longer influences the layout.
 */
void
gtk_constraint_layout_remove_constraint (GtkConstraintLayout *manager,
                                         GtkConstraint       *constraint)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_LAYOUT (manager));
  g_return_if_fail (GTK_IS_CONSTRAINT (constraint));
  g_return_if_fail (gtk_constraint_is_attached (constraint));

  gtk_constraint_detach (constraint);
  g_hash_table_remove (manager->constraints, constraint);

  gtk_layout_manager_layout_changed (GTK_LAYOUT_MANAGER (manager));
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
 * The @manager acquires the ownership of @guide after calling
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

void
gtk_constraint_layout_add_grid_constraint (GtkConstraintLayout *manager,
                                           GtkGridConstraint   *constraint)
{
  g_return_if_fail (GTK_IS_CONSTRAINT_LAYOUT (manager));
  g_return_if_fail (GTK_IS_GRID_CONSTRAINT (constraint));
  g_return_if_fail (!gtk_grid_constraint_is_attached (constraint));

  layout_add_grid_constraint (manager, constraint);

  g_hash_table_add (manager->grid_constraints, constraint);
}

static GtkConstraintVariable **
allocate_variables (GtkConstraintSolver *solver,
                    const char          *name,
                    int                  n)
{
  GtkConstraintVariable **vars;
  int i;

  vars = g_new (GtkConstraintVariable *, n);
  for (i = 0; i < n; i++)
    {
      char *vname = g_strdup_printf ("%s%d", name, i);
      vars[i] = gtk_constraint_solver_create_variable (solver, NULL, vname, 0.0);
    }

  return vars;
}

static void
add_homogeneous_constraints (GtkConstraintSolver    *solver,
                             GtkConstraintVariable **vars,
                             int                     n_vars,
                             GPtrArray              *refs)
{
  int i;

  for (i = 2; i < n_vars; i++)
    {
      GtkConstraintExpressionBuilder builder;
      GtkConstraintRef *ref;

      gtk_constraint_expression_builder_init (&builder, solver);
      gtk_constraint_expression_builder_term (&builder, vars[i]);
      gtk_constraint_expression_builder_plus (&builder);
      gtk_constraint_expression_builder_term (&builder, vars[i - 2]);
      gtk_constraint_expression_builder_divide_by (&builder);
      gtk_constraint_expression_builder_constant (&builder, 2.0);

      ref = gtk_constraint_solver_add_constraint (solver,
                                                  vars[i - 1],
                                                  GTK_CONSTRAINT_RELATION_EQ,
                                                  gtk_constraint_expression_builder_finish (&builder),
                                                  GTK_CONSTRAINT_WEIGHT_REQUIRED);
      g_ptr_array_add (refs, ref);
    }
}

static void
add_child_constraints (GtkConstraintLayout     *self,
                       GtkConstraintSolver     *solver,
                       GtkGridConstraintChild  *child,
                       GtkConstraintVariable  **rows,
                       GtkConstraintVariable  **cols,
                       GPtrArray               *refs)
{
  GtkConstraintVariable *var;
  GtkConstraintVariable *var1;
  GtkConstraintRef *ref;

  var = get_child_attribute (self, child->child, GTK_CONSTRAINT_ATTRIBUTE_LEFT);
  var1 = cols[child->left];

  ref = gtk_constraint_solver_add_constraint (solver,
                                              var,
                                              GTK_CONSTRAINT_RELATION_EQ,
                                              gtk_constraint_expression_new_from_variable (var1),
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
  g_ptr_array_add (refs, ref);

  var = get_child_attribute (self, child->child, GTK_CONSTRAINT_ATTRIBUTE_RIGHT);
  var1 = cols[child->right];

  ref = gtk_constraint_solver_add_constraint (solver,
                                              var,
                                              GTK_CONSTRAINT_RELATION_EQ,
                                              gtk_constraint_expression_new_from_variable (var1),
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
  g_ptr_array_add (refs, ref);

  var = get_child_attribute (self, child->child, GTK_CONSTRAINT_ATTRIBUTE_TOP);
  var1 = rows[child->top];

  ref = gtk_constraint_solver_add_constraint (solver,
                                              var,
                                              GTK_CONSTRAINT_RELATION_EQ,
                                              gtk_constraint_expression_new_from_variable (var1),
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
  g_ptr_array_add (refs, ref);

  var = get_child_attribute (self, child->child, GTK_CONSTRAINT_ATTRIBUTE_BOTTOM);
  var1 = rows[child->bottom];

  ref = gtk_constraint_solver_add_constraint (solver,
                                              var,
                                              GTK_CONSTRAINT_RELATION_EQ,
                                              gtk_constraint_expression_new_from_variable (var1),
                                              GTK_CONSTRAINT_WEIGHT_REQUIRED);
  g_ptr_array_add (refs, ref);
}

static void
layout_add_grid_constraint (GtkConstraintLayout *manager,
                            GtkGridConstraint   *constraint)
{
  GtkWidget *layout_widget;
  GtkConstraintSolver *solver;
  GtkConstraintVariable **rows;
  GtkConstraintVariable **cols;
  int n_rows, n_cols;
  GPtrArray *refs;
  int i;

  if (gtk_grid_constraint_is_attached (constraint))
    return;

  layout_widget = gtk_layout_manager_get_widget (GTK_LAYOUT_MANAGER (manager));
  if (layout_widget == NULL)
    return;

  solver = gtk_constraint_layout_get_solver (manager);
  if (solver == NULL)
    return;

  gtk_constraint_solver_freeze (solver);

  refs = g_ptr_array_new ();

  n_rows = n_cols = 0;
  for (i = 0; i < constraint->children->len; i++)
    {
      GtkGridConstraintChild *child = g_ptr_array_index (constraint->children, i);
      n_rows = MAX (n_rows, child->bottom);
      n_cols = MAX (n_cols, child->right);
    }
  n_rows++;
  n_cols++;

  rows = allocate_variables (solver, "row", n_rows);
  cols = allocate_variables (solver, "col", n_cols);

  if (constraint->row_homogeneous)
    add_homogeneous_constraints (solver, rows, n_rows, refs);
  if (constraint->column_homogeneous)
    add_homogeneous_constraints (solver, cols, n_cols, refs);

  for (i = 0; i < constraint->children->len; i++)
    {
      GtkGridConstraintChild *child = g_ptr_array_index (constraint->children, i);
      add_child_constraints (manager, solver, child, rows, cols, refs);
    }

  gtk_grid_constraint_attach (constraint, solver, refs);

  g_free (rows);
  g_free (cols);
  g_ptr_array_unref (refs);

  gtk_constraint_solver_thaw (solver);
}
