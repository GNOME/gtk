/* gtkconstraintguide.c: Flexible space for constraints
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
 *
 * Author: Matthias Clasen
 */

#include "config.h"

#include "gtkconstraintguide.h"

#include "gtkconstraintguideprivate.h"
#include "gtkconstraintlayoutprivate.h"
#include "gtkconstraintexpressionprivate.h"
#include "gtkconstraintsolverprivate.h"

#include "gtkdebug.h"
#include "gtkintl.h"
#include "gtkprivate.h"


struct _GtkConstraintGuideClass {
  GObjectClass parent_class;
};

enum {
  GUIDE_PROP_MIN_WIDTH = 1,
  GUIDE_PROP_MIN_HEIGHT,
  GUIDE_PROP_NAT_WIDTH,
  GUIDE_PROP_NAT_HEIGHT,
  GUIDE_PROP_MAX_WIDTH,
  GUIDE_PROP_MAX_HEIGHT,
  LAST_GUIDE_PROP
};

static GParamSpec *guide_props[LAST_GUIDE_PROP];

static void
gtk_constraint_guide_constraint_target_iface_init (GtkConstraintTargetInterface *iface)
{
}

G_DEFINE_TYPE_WITH_CODE (GtkConstraintGuide, gtk_constraint_guide, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (GTK_TYPE_CONSTRAINT_TARGET,
                                                gtk_constraint_guide_constraint_target_iface_init))

static void
gtk_constraint_guide_init (GtkConstraintGuide *guide)
{
  guide->bound_attributes =
    g_hash_table_new_full (g_str_hash, g_str_equal,
                           NULL,
                           (GDestroyNotify) gtk_constraint_variable_unref);
}

void
gtk_constraint_guide_update (GtkConstraintGuide *guide,
                             GuideValue          index)
{
  GtkConstraintSolver *solver;
  GtkConstraintVariable *var;
  int attr[LAST_GUIDE_VALUE] = {
    GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
    GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
    GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
    GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
    GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
    GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
  };
  int relation[LAST_GUIDE_VALUE] = {
    GTK_CONSTRAINT_RELATION_GE,
    GTK_CONSTRAINT_RELATION_GE,
    GTK_CONSTRAINT_RELATION_EQ,
    GTK_CONSTRAINT_RELATION_EQ,
    GTK_CONSTRAINT_RELATION_LE,
    GTK_CONSTRAINT_RELATION_LE,
  };
  double weight[LAST_GUIDE_VALUE] = {
    GTK_CONSTRAINT_WEIGHT_REQUIRED,
    GTK_CONSTRAINT_WEIGHT_REQUIRED,
    GTK_CONSTRAINT_WEIGHT_MEDIUM,
    GTK_CONSTRAINT_WEIGHT_MEDIUM,
    GTK_CONSTRAINT_WEIGHT_REQUIRED,
    GTK_CONSTRAINT_WEIGHT_REQUIRED,
  };

  if (!guide->layout)
    return;

  solver = gtk_constraint_layout_get_solver (guide->layout);
  if (!solver)
    return;

  if (guide->constraints[index] != NULL)
    gtk_constraint_solver_remove_constraint (solver, guide->constraints[index]);

  var = gtk_constraint_layout_get_attribute (guide->layout, attr[index], "guide", NULL, guide->bound_attributes);
  guide->constraints[index] =
    gtk_constraint_solver_add_constraint (solver,
                                          var,
                                          relation[index],
                                          gtk_constraint_expression_new (guide->values[index]),
                                          weight[index]);
}

void
gtk_constraint_guide_detach (GtkConstraintGuide *guide)
{
  GtkConstraintSolver *solver;
  int i;

  if (!guide->layout)
    return;

  solver = gtk_constraint_layout_get_solver (guide->layout);
  if (!solver)
    return;

  for (i = 0; i < LAST_GUIDE_VALUE; i++)
    {
      gtk_constraint_solver_remove_constraint (solver, guide->constraints[i]);
      guide->constraints[i] = NULL;
    }

  g_hash_table_remove_all (guide->bound_attributes);
}

static void
gtk_constraint_guide_set_property (GObject      *gobject,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  GtkConstraintGuide *self = GTK_CONSTRAINT_GUIDE (gobject);
  int val;
  GuideValue index;

  switch (prop_id)
    {
    case GUIDE_PROP_MIN_WIDTH:
    case GUIDE_PROP_MIN_HEIGHT:
    case GUIDE_PROP_NAT_WIDTH:
    case GUIDE_PROP_NAT_HEIGHT:
    case GUIDE_PROP_MAX_WIDTH:
    case GUIDE_PROP_MAX_HEIGHT:
      val = g_value_get_int (value);
      index = prop_id - 1;
      if (self->values[index] != val)
        {
          self->values[index] = val;
          g_object_notify_by_pspec (gobject, pspec);
          gtk_constraint_guide_update (self, index);
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_constraint_guide_get_property (GObject    *gobject,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  GtkConstraintGuide *self = GTK_CONSTRAINT_GUIDE (gobject);

  switch (prop_id)
    {
    case GUIDE_PROP_MIN_WIDTH:
    case GUIDE_PROP_MIN_HEIGHT:
    case GUIDE_PROP_NAT_WIDTH:
    case GUIDE_PROP_NAT_HEIGHT:
    case GUIDE_PROP_MAX_WIDTH:
    case GUIDE_PROP_MAX_HEIGHT:
      g_value_set_int (value, self->values[prop_id - 1]);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_constraint_guide_finalize (GObject *object)
{
  GtkConstraintGuide *self = GTK_CONSTRAINT_GUIDE (object);

  g_clear_pointer (&self->bound_attributes, g_hash_table_unref);

  G_OBJECT_CLASS (gtk_constraint_guide_parent_class)->finalize (object);
}

static void
gtk_constraint_guide_class_init (GtkConstraintGuideClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->finalize = gtk_constraint_guide_finalize;
  object_class->set_property = gtk_constraint_guide_set_property;
  object_class->get_property = gtk_constraint_guide_get_property;

  guide_props[GUIDE_PROP_MIN_WIDTH] =
      g_param_spec_int ("min-width",
                        "Minimum width",
                        "Minimum width",
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);
  guide_props[GUIDE_PROP_MIN_HEIGHT] =
      g_param_spec_int ("min-height",
                        "Minimum height",
                        "Minimum height",
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);
  guide_props[GUIDE_PROP_NAT_WIDTH] =
      g_param_spec_int ("nat-width",
                        "Natural width",
                        "Natural width",
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);
  guide_props[GUIDE_PROP_NAT_HEIGHT] =
      g_param_spec_int ("nat-height",
                        "Natural height",
                        "Natural height",
                        0, G_MAXINT, 0,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);
  guide_props[GUIDE_PROP_MAX_WIDTH] =
      g_param_spec_int ("max-width",
                        "Maximum width",
                        "Maximum width",
                        0, G_MAXINT, G_MAXINT,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);
  guide_props[GUIDE_PROP_MAX_HEIGHT] =
      g_param_spec_int ("max-height",
                        "Maximum height",
                        "Maximum height",
                        0, G_MAXINT, G_MAXINT,
                        G_PARAM_READWRITE|
                        G_PARAM_EXPLICIT_NOTIFY);

  g_object_class_install_properties (object_class, LAST_GUIDE_PROP, guide_props);
}

/**
 * gtk_constraint_guide_new:
 *
 * Creates a new #GtkConstraintGuide object.
 *
 * Return: a new #GtkConstraintGuide object.
 */
GtkConstraintGuide *
gtk_constraint_guide_new (void)
{
  return g_object_new (GTK_TYPE_CONSTRAINT_GUIDE, NULL);
}


