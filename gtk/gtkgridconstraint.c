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

enum {
  POS_LEFT,
  POS_RIGHT,
  POS_TOP,
  POS_BOTTOM,
  LAST_POS,
  SIZE_WIDTH = LAST_POS,
  SIZE_HEIGHT,
  LAST_CONSTRAINT
};

/* We maintain constraints of the form:
 *
 * child.top = row_x
 * child.bottom = row_y
 *
 * (and similar for columns). We avoid introducing
 * extra variables for rows and columns by keeping
 * track of the first child we encounter that ends
 * at a given position, and using that instead:
 *
 * child.top = first.bottom
 *
 * (assuming that @first is the first child we saw
 * that ends at row x (and has its bottom edge there).
 *
 * For homogeneous grids, we additionally maintain
 * constraints of the form:
 *
 * a.width / a.colspan = b.width / b.colspan
 *
 * (and similar for heights). We only maintain
 * these relations between a child and its
 * predecessor in the list of children.
 */

typedef struct {
  GtkConstraintTarget *child;
  int left;
  int right;
  int top;
  int bottom;

  /* We hold a ref on these */
  GtkConstraint *constraints[LAST_CONSTRAINT];
} GtkGridConstraintChild;

typedef struct {
  GtkConstraintTarget *target;
  GtkConstraintAttribute attr;
} Attach;

struct _GtkGridConstraint {
  GObject parent;

  GtkConstraintLayout *layout;

  gboolean row_homogeneous;
  gboolean column_homogeneous;

  /* List<GtkGridConstraintChild>, owned */
  GPtrArray *children;

  /* List<GtkConstaint>, not owned */
  GPtrArray *constraints;

  /* Array<Attach>, not owning Attach.target */
  GArray *rows;
  GArray *cols;
};

enum {
  PROP_ROW_HOMOGENEOUS = 1,
  PROP_COLUMN_HOMOGENEOUS,
  N_PROPERTIES
};

static GParamSpec *obj_props[N_PROPERTIES];

G_DEFINE_TYPE (GtkGridConstraint, gtk_grid_constraint, G_TYPE_OBJECT)

static void set_row_homogeneous    (GtkGridConstraint *self,
                                    gboolean           homogeneous);
static void set_column_homogeneous (GtkGridConstraint *self,
                                    gboolean           homogeneous);

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
      set_row_homogeneous (self, g_value_get_boolean (value));
      break;

    case PROP_COLUMN_HOMOGENEOUS:
      set_column_homogeneous (self, g_value_get_boolean (value));
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
gtk_grid_constraint_init (GtkGridConstraint *self)
{
  self->children = g_ptr_array_new_with_free_func (g_free);

  self->constraints = g_ptr_array_new ();

  self->rows = g_array_new (FALSE, TRUE, sizeof (Attach));
  self->cols = g_array_new (FALSE, TRUE, sizeof (Attach));
}

static void
gtk_constraint_finalize (GObject *gobject)
{
  GtkGridConstraint *self = GTK_GRID_CONSTRAINT (gobject);

  g_array_free (self->rows, TRUE);
  g_array_free (self->cols, TRUE);

  if (self->layout)
    gtk_grid_constraint_detach (self);
  g_assert (self->constraints->len == 0);
  g_ptr_array_free (self->constraints, TRUE);

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
remove_child_constraint (GtkGridConstraint      *self,
                         GtkGridConstraintChild *child,
                         int                     pos)
{
  if (child->constraints[pos] == NULL)
    return;

  if (self->layout)
    gtk_constraint_layout_remove_constraint (self->layout,
                                             child->constraints[pos]);
  g_object_unref (child->constraints[pos]);
  child->constraints[pos] = NULL;
}

/* Ensure that the child variable @var is placed
 * at the grid edge @pos. If we already have a variable
 * that needs to end up there, we use it to assert
 * var = vars[top], otherwise we put @var in the
 * the list of variables.
 *
 * @attr may be one of LEFT/RIGHT/TOP/BOTTOM here.
 */
static void
add_child_constraint (GtkGridConstraint      *self,
                      GtkGridConstraintChild *child,
                      GtkConstraintAttribute  attr,
                      int                     pos)
{
  Attach *attach;
  GArray *vars;
  int cpos;

  switch ((int)attr)
    {
    case GTK_CONSTRAINT_ATTRIBUTE_LEFT:
      vars = self->cols;
      cpos = POS_LEFT;
      break;
    case GTK_CONSTRAINT_ATTRIBUTE_RIGHT:
      vars = self->cols;
      cpos = POS_RIGHT;
      break;
    case GTK_CONSTRAINT_ATTRIBUTE_TOP:
      vars = self->rows;
      cpos = POS_TOP;
      break;
    case GTK_CONSTRAINT_ATTRIBUTE_BOTTOM:
      vars = self->rows;
      cpos = POS_BOTTOM;
      break;
    default:
      g_assert_not_reached ();
    }

  if (vars->len <= pos)
    g_array_set_size (vars, pos + 1);

  attach = &g_array_index (vars, Attach, pos);
  if (attach->target == NULL)
    {
      attach->target = child->child;
      attach->attr = attr;
    }
  else
    {
      g_assert (child->constraints[cpos] == NULL);

      child->constraints[cpos] = gtk_constraint_new (child->child, attr,
                                       GTK_CONSTRAINT_RELATION_EQ,
                                       attach->target, attach->attr,
                                       1.0, 0.0,
                                       GTK_CONSTRAINT_STRENGTH_REQUIRED);
      if (self->layout)
        gtk_constraint_layout_add_constraint (self->layout,
                                              child->constraints[cpos]);
    }
}

/* Create the contraint:
 *
 * child1.width/child1.colspan = child2.width/child2.colspan
 *
 * @attr can be WIDTH or HEIGHT here.
 */
static void
add_homogeneous_constraint (GtkGridConstraint      *self,
                            GtkGridConstraintChild *child1,
                            GtkGridConstraintChild *child2,
                            GtkConstraintAttribute  attr)
{
  int span1, span2;
  int pos;

  if (attr == GTK_CONSTRAINT_ATTRIBUTE_WIDTH)
    {
      pos = SIZE_WIDTH;
      span1 = child1->right - child1->left;
      span2 = child2->right - child2->left;
    }
  else
    {
      pos = SIZE_HEIGHT;
      span1 = child1->bottom - child1->top;
      span2 = child2->bottom - child2->top;
    }

  g_assert (child1->constraints[pos] == NULL);
 
  child1->constraints[pos] = gtk_constraint_new (child1->child, attr,
                                   GTK_CONSTRAINT_RELATION_EQ,
                                   child2->child, attr,
                                   (double) span1 / (double) span2,
                                   0.0,
                                   GTK_CONSTRAINT_STRENGTH_REQUIRED);

  if (self->layout)
    gtk_constraint_layout_add_constraint (self->layout,
                                          child1->constraints[pos]);
}

static void
set_row_homogeneous (GtkGridConstraint *self,
                     gboolean           homogeneous)
{
  GtkGridConstraintChild *child1;
  GtkGridConstraintChild *child2;
  int i;

  if (self->row_homogeneous == homogeneous)
    return;

  self->row_homogeneous = homogeneous;

  for (i = 1; i < self->children->len; i++)
    {
      child1 = g_ptr_array_index (self->children, i);
      child2 = g_ptr_array_index (self->children, i - 1);
      if (homogeneous)
        add_homogeneous_constraint (self, child1, child2, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT);
      else
        remove_child_constraint (self, child1, SIZE_HEIGHT);
    }

  g_object_notify (G_OBJECT (self), "row-homogeneous");
}

static void
set_column_homogeneous (GtkGridConstraint *self,
                        gboolean           homogeneous)
{
  GtkGridConstraintChild *child1;
  GtkGridConstraintChild *child2;
  int i;

  if (self->column_homogeneous == homogeneous)
    return;

  self->column_homogeneous = homogeneous;

  for (i = 1; i < self->children->len; i++)
    {
      child1 = g_ptr_array_index (self->children, i);
      child2 = g_ptr_array_index (self->children, i - 1);
      if (homogeneous)
        add_homogeneous_constraint (self, child1, child2, GTK_CONSTRAINT_ATTRIBUTE_WIDTH);
      else
        remove_child_constraint (self, child1, SIZE_WIDTH);
    }

  g_object_notify (G_OBJECT (self), "column-homogeneous");
}

/* Fix up attachment constraints for the removal of @target.
 * We are fixing up the attachment at row/col @rows and
 * position @pos. If @target was not the representative
 * for this position, there is nothing to do (all of @targets
 * constraints are already removed). Otherwise, look over
 * all children that are attached to @target for this position,
 * pick a new representative, and fix up the constraints
 * for all others to attach to the new representative.
 */
static void
fix_up_attach (GtkGridConstraint   *self,
               GtkConstraintTarget *target,
               gboolean             rows,
               int                  pos)
{
  int i;
  Attach *attach;

  if (rows)
    attach = &g_array_index (self->rows, Attach, pos);
  else
    attach = &g_array_index (self->cols, Attach, pos);

  if (attach->target != target)
    return;

  attach->target = NULL;

  for (i = 0; i < self->children->len; i++)
    {
      int cpos;
      GtkGridConstraintChild *child;
      GtkConstraintAttribute attr;

      child = g_ptr_array_index (self->children, i);
      if (rows && child->top == pos)
        {
          cpos = POS_TOP;
          attr = GTK_CONSTRAINT_ATTRIBUTE_TOP;
        }
      else if (rows && child->bottom == pos)
        {
          cpos = POS_BOTTOM;
          attr = GTK_CONSTRAINT_ATTRIBUTE_BOTTOM;
        }
      else if (!rows && child->left == pos)
        {
          cpos = POS_LEFT;
          attr = GTK_CONSTRAINT_ATTRIBUTE_LEFT;
        }
      else if (!rows && child->right == pos)
        {
          cpos = POS_LEFT;
          attr = GTK_CONSTRAINT_ATTRIBUTE_LEFT;
        }
      else
        continue;

      remove_child_constraint (self, child, cpos);

      if (attach->target == NULL)
        {
          attach->target = child->child;
          attach->attr = attr;
        }
      else
        {
          child->constraints[cpos] =
            gtk_constraint_new (child->child, attr,
                                GTK_CONSTRAINT_RELATION_EQ,
                                attach->target, attach->attr,
                                1.0, 0.0,
                                GTK_CONSTRAINT_STRENGTH_REQUIRED);
          if (self->layout)
            gtk_constraint_layout_add_constraint (self->layout,
                                                  g_object_ref (child->constraints[cpos]));

        }
    }
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
  GtkGridConstraintChild *child1;
  GtkGridConstraintChild *child2;

  g_return_if_fail (GTK_IS_GRID_CONSTRAINT (self));
  g_return_if_fail (GTK_IS_WIDGET (child));
  g_return_if_fail (left < right);
  g_return_if_fail (top < bottom);
  g_return_if_fail (self->layout == NULL);

  child1 = g_new0 (GtkGridConstraintChild, 1);

  child1->child = GTK_CONSTRAINT_TARGET (child);
  child1->left = left;
  child1->right = right;
  child1->top = top;
  child1->bottom = bottom;

  if (self->children->len > 0)
    child2 = g_ptr_array_index (self->children, self->children->len - 1);
  else
    child2 = NULL;

  add_child_constraint (self, child1, GTK_CONSTRAINT_ATTRIBUTE_TOP, top);
  add_child_constraint (self, child1, GTK_CONSTRAINT_ATTRIBUTE_BOTTOM, bottom);
  add_child_constraint (self, child1, GTK_CONSTRAINT_ATTRIBUTE_LEFT, left);
  add_child_constraint (self, child1, GTK_CONSTRAINT_ATTRIBUTE_RIGHT, right);

  if (self->row_homogeneous && child2)
    add_homogeneous_constraint (self, child1, child2, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT);
  if (self->column_homogeneous && child2)
    add_homogeneous_constraint (self, child1, child2, GTK_CONSTRAINT_ATTRIBUTE_WIDTH);

  g_ptr_array_add (self->children, child1);
}

void
gtk_grid_constraint_remove (GtkGridConstraint *self,
                            GtkWidget         *child)
{
  GtkGridConstraintChild *child1 = NULL;
  GtkGridConstraintChild *next = NULL;
  GtkGridConstraintChild *prev = NULL;
  GtkConstraintTarget *target = (GtkConstraintTarget *)child;
  int i;
  int left, right, top, bottom;

  for (i = 0; i < self->children->len; i++)
    {
      child1 = g_ptr_array_index (self->children, i);
      if (child1->child == target)
        break;
      child1 = NULL;
    }

  if (child1 == NULL)
    return;

  if (i > 0)
    prev = g_ptr_array_index (self->children, i - 1);
  if (i + 1 < self->children->len)
    next = g_ptr_array_index (self->children, i + 1);

  for (i = 0; i < LAST_CONSTRAINT; i++)
    remove_child_constraint (self, child1, i);

  top = child1->top;
  bottom = child1->bottom;
  left = child1->left;
  right = child1->right;

  g_ptr_array_remove (self->children, child1);

  fix_up_attach (self, target, TRUE, top);
  fix_up_attach (self, target, TRUE, bottom);
  fix_up_attach (self, target, FALSE, right);
  fix_up_attach (self, target, FALSE, left);

  if (self->column_homogeneous && next)
    {
      remove_child_constraint (self, next, SIZE_WIDTH);
      if (prev)
        add_homogeneous_constraint (self, next, prev, GTK_CONSTRAINT_ATTRIBUTE_WIDTH);
    }
  if (self->row_homogeneous && next)
    {
      remove_child_constraint (self, next, SIZE_HEIGHT);
      if (prev)
        add_homogeneous_constraint (self, next, prev, GTK_CONSTRAINT_ATTRIBUTE_HEIGHT);
    }
}

gboolean
gtk_grid_constraint_is_attached (GtkGridConstraint *self)
{
  return self->layout != NULL;
}

void
gtk_grid_constraint_attach (GtkGridConstraint   *self,
                            GtkConstraintLayout *layout)
{
  int i, j;

  g_return_if_fail (self->layout == NULL);

  self->layout = layout;

  for (i = 0; i < self->children->len; i++)
    {
      GtkGridConstraintChild *child = g_ptr_array_index (self->children, i);
      for (j = 0; j < LAST_CONSTRAINT; j++)
        {
          if (child->constraints[j] != NULL)
            gtk_constraint_layout_add_constraint (self->layout,
                                     g_object_ref (child->constraints[j]));
        }
    }
}

void
gtk_grid_constraint_detach (GtkGridConstraint *self)
{
  int i, j;

  if (self->layout == NULL)
    return;

  for (i = 0; i < self->children->len; i++)
    {
      GtkGridConstraintChild *child = g_ptr_array_index (self->children, i);
      for (j = 0; j < LAST_CONSTRAINT; j++)
        {
          if (child->constraints[j] != NULL)
            gtk_constraint_layout_remove_constraint (self->layout,
                                                     child->constraints[j]);
        }
    }

  self->layout = NULL;
}
