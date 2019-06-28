/* Copyright (C) 2019 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */
#include <gtk/gtk.h>

#define GTK_TYPE_GIZMO                 (gtk_gizmo_get_type ())
#define GTK_GIZMO(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTK_TYPE_GIZMO, GtkGizmo))
#define GTK_GIZMO_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GTK_TYPE_GIZMO, GtkGizmoClass))
#define GTK_IS_GIZMO(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_GIZMO))
#define GTK_IS_GIZMO_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_GIZMO))
#define GTK_GIZMO_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_GIZMO, GtkGizmoClass))

typedef struct _GtkGizmo GtkGizmo;

struct _GtkGizmo {
  GtkWidget parent;

  const char *name;
  int min_width;
  int min_height;
  int nat_width;
  int nat_height;
  int width;
  int height;
};

typedef GtkWidgetClass GtkGizmoClass;

G_DEFINE_TYPE (GtkGizmo, gtk_gizmo, GTK_TYPE_WIDGET);

static void
gtk_gizmo_measure (GtkWidget      *widget,
                   GtkOrientation  orientation,
                   int             for_size,
                   int            *minimum,
                   int            *natural,
                   int            *minimum_baseline,
                   int            *natural_baseline)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = self->min_width;
      *natural = self->nat_width;
    }
  else
    {
      *minimum = self->min_height;
      *natural = self->nat_height;
    }
}

static void
gtk_gizmo_size_allocate (GtkWidget *widget,
                         int        width,
                         int        height,
                         int        baseline)
{
  GtkGizmo *self = GTK_GIZMO (widget);

  self->width = width;
  self->height = height;
}

static void
gtk_gizmo_class_init (GtkGizmoClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->measure = gtk_gizmo_measure;
  widget_class->size_allocate = gtk_gizmo_size_allocate;
}

static void
gtk_gizmo_init (GtkGizmo *self)
{
}

/* Create a layout with three children
 *
 * +--------+--------+
 * | child1 | child2 |
 * +--------+--------+
 * |      child3     |
 * +-----------------+
 *
 * Verify that
 * - the layout has the expected min and nat sizes
 * - the children get their >=nat width when the layout does
 * - test that allocating the layout larger keeps
 *   child1 and child2 at the same size
 */
static void
test_simple_layout (void)
{
  GtkWidget *window;
  GtkWidget *parent;
  GtkLayoutManager *layout;
  GtkConstraintLayout *manager;
  GtkGizmo *child1;
  GtkGizmo *child2;
  GtkGizmo *child3;
  int minimum, natural;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  parent = g_object_new (GTK_TYPE_GIZMO, NULL);
  gtk_container_add (GTK_CONTAINER (window), parent);

  layout = gtk_constraint_layout_new ();
  gtk_widget_set_layout_manager (parent, layout);
  manager = GTK_CONSTRAINT_LAYOUT (layout);

  child1 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child2 = g_object_new (GTK_TYPE_GIZMO, NULL);
  child3 = g_object_new (GTK_TYPE_GIZMO, NULL);

  child1->name = "child1";
  child1->min_width = 10;
  child1->min_height = 10;
  child1->nat_width = 50;
  child1->nat_height = 50;
  child2->name = "child2";
  child2->min_width = 20;
  child2->min_height = 20;
  child2->nat_width = 50;
  child2->nat_height = 50;
  child3->name = "child3";
  child3->min_width = 50;
  child3->min_height = 10;
  child3->nat_width = 50;
  child3->nat_height = 50;

  gtk_widget_set_parent (GTK_WIDGET (child1), parent);
  gtk_widget_set_parent (GTK_WIDGET (child2), parent);
  gtk_widget_set_parent (GTK_WIDGET (child3), parent);

  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        GTK_CONSTRAINT_RELATION_EQ,
                        child1,
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (child1,
                        GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
                        GTK_CONSTRAINT_RELATION_EQ,
                        child2,
                        GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (child1,
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        GTK_CONSTRAINT_RELATION_EQ,
                        child2,
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (child2,
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        GTK_CONSTRAINT_RELATION_EQ,
                        NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        GTK_CONSTRAINT_RELATION_EQ,
                        child3,
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (child3,
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        GTK_CONSTRAINT_RELATION_EQ,
                        NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        GTK_CONSTRAINT_RELATION_EQ,
                        child1,
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        GTK_CONSTRAINT_RELATION_EQ,
                        child2,
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (child1,
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        GTK_CONSTRAINT_RELATION_EQ,
                        child3,
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (child2,
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        GTK_CONSTRAINT_RELATION_EQ,
                        child3,
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (child3,
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        GTK_CONSTRAINT_RELATION_EQ,
                        NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));

#if 0
  gtk_widget_show (window);

  g_timeout_add (1000, (GSourceFunc)gtk_main_quit, NULL);
  gtk_main ();
#endif

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_HORIZONTAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 50);
  g_assert_cmpint (natural, ==, 100);

  gtk_layout_manager_measure (layout,
                              parent,
                              GTK_ORIENTATION_VERTICAL,
                              -1,
                              &minimum,
                              &natural,
                              NULL,
                              NULL);

  g_assert_cmpint (minimum, ==, 40);
  g_assert_cmpint (natural, ==, 100);

  gtk_layout_manager_allocate (layout, parent, 100, 100, 0);

  g_assert_cmpint (child1->width, ==, 50);
  g_assert_cmpint (child2->width, ==, 50);
  g_assert_cmpint (child3->width, ==, 100);

  g_assert_cmpint (child1->height, ==, 50);
  g_assert_cmpint (child2->height, ==, 50);
  g_assert_cmpint (child3->height, ==, 50);

  gtk_widget_unparent (GTK_WIDGET (child1));
  gtk_widget_unparent (GTK_WIDGET (child2));
  gtk_widget_unparent (GTK_WIDGET (child3));

  gtk_widget_destroy (parent);
}

int
main (int   argc,
      char *argv[])
{
  gtk_test_init (&argc, &argv);

  g_test_add_func ("/constraint-layout/simple", test_simple_layout);


  return g_test_run();
}
