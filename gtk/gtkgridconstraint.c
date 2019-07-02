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

#include "gtkintl.h"
#include "gtktypebuiltins.h"

typedef struct {
  GtkConstraintTarget *child;
  int left;
  int right;
  int top;
  int bottom;
} GtkGridConstraintChild;

struct _GtkGridConstraint {
  GObject parent;

  GtkConstraintLayout *layout;

  gboolean row_homogeneous;
  gboolean column_homogeneous;

  GPtrArray *children;
  GPtrArray *constraints;
};

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
  g_ptr_array_free (self->constraints, TRUE);

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
   self->constraints = g_ptr_array_new ();
}

GtkGridConstraint *
gtk_grid_constraint_new (void)
{
  return g_object_new (GTK_TYPE_GRID_CONSTRAINT, NULL);
}


/* Ensure that
 * child1.width / child1.colspan == child2.width / child2.colspan
 * (or equivalent for height )
 */
static void
add_homogeneous_constraint (GtkGridConstraintChild *child1,
                            GtkGridConstraintChild *child2,
                            GtkConstraintAttribute  attr,
                            GPtrArray              *constraints)
{
  int span1, span2;
  GtkConstraint *constraint;

  if (attr == GTK_CONSTRAINT_ATTRIBUTE_WIDTH)
    {
      span1 = child1->right - child1->left;
      span2 = child2->right - child2->left;
    }
  else
    {
      span1 = child1->bottom - child1->top;
      span2 = child2->bottom - child2->top;
    }

  constraint = gtk_constraint_new (child1->child, attr,
                                   GTK_CONSTRAINT_RELATION_EQ,
                                   child2->child, attr,
                                   (double) span1 / (double) span2,
                                   0.0,
                                   GTK_CONSTRAINT_STRENGTH_REQUIRED);
  g_ptr_array_add (constraints, constraint);
}

typedef struct {
  GtkConstraintTarget *target;
  GtkConstraintAttribute attr;
} Attach;

/* Ensure that the child variable @var is placed
 * at the grid edge @pos. If we already have a variable
 * that needs to end up there, we use it to assert
 * var = vars[top], otherwise we put @var in the
 * the list of variables.
 */ 
static void
add_child_constraint (GtkConstraintTarget    *target,
                      GtkConstraintAttribute  attr,
                      int                     pos,
                      GArray                 *vars,
                      GPtrArray              *constraints)
{
  Attach *attach;

  if (vars->len <= pos)
    g_array_set_size (vars, pos + 1);

  attach = &g_array_index (vars, Attach, pos);
  if (attach->target == NULL)
    {
      attach->target = target;
      attach->attr = attr;
    }
  else
    {
      GtkConstraint *constraint;

      constraint = gtk_constraint_new (target, attr,
                                       GTK_CONSTRAINT_RELATION_EQ,
                                       attach->target, attach->attr,
                                       1.0, 0.0,
                                       GTK_CONSTRAINT_STRENGTH_REQUIRED);
      g_ptr_array_add (constraints, constraint);
    }
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
  g_return_if_fail (self->layout == NULL);

  data = g_new0 (GtkGridConstraintChild, 1);

  data->child = GTK_CONSTRAINT_TARGET (child);
  data->left = left;
  data->right = right;
  data->top = top;
  data->bottom = bottom;

  g_ptr_array_add (self->children, data);
}

gboolean
gtk_grid_constraint_is_attached (GtkGridConstraint *self)
{
  return self->layout != NULL;
}

static void
create_constraints (GtkGridConstraint *self)
{
  GArray *rows;
  GArray *cols;
  int i;

  rows = g_array_new (FALSE, TRUE, sizeof (Attach));
  cols = g_array_new (FALSE, TRUE, sizeof (Attach));

  for (i = 0; i < self->children->len; i++)
    {
      GtkGridConstraintChild *child = g_ptr_array_index (self->children, i);

      add_child_constraint (child->child, GTK_CONSTRAINT_ATTRIBUTE_TOP, child->top, rows, self->constraints);
      add_child_constraint (child->child, GTK_CONSTRAINT_ATTRIBUTE_BOTTOM, child->bottom, rows, self->constraints);
      add_child_constraint (child->child, GTK_CONSTRAINT_ATTRIBUTE_LEFT, child->left, cols, self->constraints);
      add_child_constraint (child->child, GTK_CONSTRAINT_ATTRIBUTE_RIGHT, child->right, cols, self->constraints);
    }

  for (i = 1; i < self->children->len; i++)
    {
      GtkGridConstraintChild *child1 = g_ptr_array_index (self->children, i);
      GtkGridConstraintChild *child2 = g_ptr_array_index (self->children, i - 1);
      if (self->row_homogeneous)
        add_homogeneous_constraint (child1, child2, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT, self->constraints);
      if (self->column_homogeneous)
	add_homogeneous_constraint (child1, child2, GTK_CONSTRAINT_ATTRIBUTE_WIDTH, self->constraints);
    }

  g_array_free (rows, TRUE);
  g_array_free (cols, TRUE);
}

void
gtk_grid_constraint_attach (GtkGridConstraint   *self,
                            GtkConstraintLayout *layout)
{
  int i;

  g_return_if_fail (self->layout == NULL);

  self->layout = layout;

  create_constraints (self);

  for (i = 0; i < self->constraints->len; i++)
    {
      GtkConstraint *constraint = g_ptr_array_index (self->constraints, i);
      gtk_constraint_layout_add_constraint (layout, g_object_ref (constraint));
    }
}

void
gtk_grid_constraint_detach (GtkGridConstraint *self)
{
  int i;

  if (self->layout == NULL)
    return;

  for (i = 0; i < self->constraints->len; i++)
    {
      GtkConstraint *constraint = g_ptr_array_index (self->constraints, i);
      gtk_constraint_layout_remove_constraint (self->layout, constraint);
    }

  self->layout = NULL;
}
