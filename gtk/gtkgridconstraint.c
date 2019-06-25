/* gtkgridconstraint.c: Make a grid with constraints
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

#include "config.h"

#include "gtkgridconstraint.h"
#include "gtkgridconstraintprivate.h"

#include "gtkintl.h"
#include "gtktypebuiltins.h"

enum {
  PROP_ROW_HOMOGENEOUS = 1,
  PROP_COLUMN_HOMOGENEOUS,
  N_PROPERTIES
};

static GParamSpec *obj_props[N_PROPERTIES];

G_DEFINE_TYPE (GtkGridConstraint, gtk_grid_constraint, G_TYPE_OBJECT)

static void
gtk_constraint_set_property (GObject      *gobject,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  GtkGridConstraint *self = GTK_GRID_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_ROW_HOMOGENEOUS:
      self->row_homogeneous = g_value_get_boolean (value);
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      self->column_homogeneous = g_value_get_boolean (value);
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
  GtkGridConstraint *self = GTK_GRID_CONSTRAINT (gobject);

  switch (prop_id)
    {
    case PROP_ROW_HOMOGENEOUS:
      g_value_set_boolean (value, self->row_homogeneous);
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      g_value_set_boolean (value, self->column_homogeneous);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
      break;
    }
}

static void
gtk_constraint_finalize (GObject *gobject)
{
  GtkGridConstraint *self = GTK_GRID_CONSTRAINT (gobject);

  gtk_grid_constraint_detach (self);

  g_ptr_array_free (self->children, TRUE);

  G_OBJECT_CLASS (gtk_grid_constraint_parent_class)->finalize (gobject);
}

static void
gtk_grid_constraint_class_init (GtkGridConstraintClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->set_property = gtk_constraint_set_property;
  gobject_class->get_property = gtk_constraint_get_property;
  gobject_class->finalize = gtk_constraint_finalize;

  /**
   * GtkGridConstraint:row-homogeneous:
   *
   * Whether to make all rows the same height.
   */
  obj_props[PROP_ROW_HOMOGENEOUS] =
    g_param_spec_boolean ("row-homogeneous",
                          P_("Row homogeneous"),
                          P_("Row homogeneous"),
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS);

  /**
   * GtkGridConstraint:column-homogeneous:
   *
   * Whether to make all columns the same width.
   */
  obj_props[PROP_COLUMN_HOMOGENEOUS] =
    g_param_spec_boolean ("column-homogeneous",
                          P_("Column homogeneous"),
                          P_("Column homogeneous"),
                          FALSE,
                          G_PARAM_READWRITE |
                          G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, N_PROPERTIES, obj_props);
}

static void
gtk_grid_constraint_init (GtkGridConstraint *self)
{
   self->children = g_ptr_array_new_with_free_func (g_free);
}

GtkGridConstraint *
gtk_grid_constraint_new (void)
{
  return g_object_new (GTK_TYPE_GRID_CONSTRAINT, NULL);
}

void
gtk_grid_constraint_add (GtkGridConstraint *self,
                         GtkWidget         *child,
                         int                left,
                         int                right,
                         int                top,
                         int                bottom)
{
  GtkGridConstraintChild *data;

  g_return_if_fail (GTK_IS_GRID_CONSTRAINT (self));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (left < right);
  g_return_if_fail (top < bottom);
  g_return_if_fail (self->refs == NULL);

  data = g_new0 (GtkGridConstraintChild, 1);

  data->child = child;
  data->left = left;
  data->right = right;
  data->top = top;
  data->bottom = bottom;

  g_ptr_array_add (self->children, data);
}

gboolean
gtk_grid_constraint_is_attached (GtkGridConstraint *self)
{
  return self->refs != NULL;
}

void
gtk_grid_constraint_attach (GtkGridConstraint   *self,
                            GtkConstraintSolver *solver,
                            GPtrArray           *refs)
{
  g_return_if_fail (self->refs == NULL);

  self->solver = solver;
  self->refs = g_ptr_array_ref (refs);
}

void gtk_grid_constraint_detach (GtkGridConstraint *self)
{
  int i;

  if (self->refs == NULL)
    return;

  for (i = 0; i < self->refs->len; i++)
    {
      GtkConstraintRef *ref = g_ptr_array_index (self->refs, i);
      gtk_constraint_solver_remove_constraint (self->solver, ref);
    }

  g_clear_pointer (&self->refs, g_ptr_array_unref);
}
