/* Constraints/Grid
 *
 * GtkConstraintLayout lets you define complex layouts
 * like grids.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (ComplexGrid, complex_grid, COMPLEX, GRID, GtkWidget)

struct _ComplexGrid
{
  GtkWidget parent_instance;

  GtkWidget *button1, *button2, *button3;
  GtkWidget *button4, *button5;
};

G_DEFINE_TYPE (ComplexGrid, complex_grid, GTK_TYPE_WIDGET)

static void
complex_grid_destroy (GtkWidget *widget)
{
  ComplexGrid *self = COMPLEX_GRID (widget);

  g_clear_pointer (&self->button1, gtk_widget_destroy);
  g_clear_pointer (&self->button2, gtk_widget_destroy);
  g_clear_pointer (&self->button3, gtk_widget_destroy);
  g_clear_pointer (&self->button4, gtk_widget_destroy);
  g_clear_pointer (&self->button5, gtk_widget_destroy);

  GTK_WIDGET_CLASS (complex_grid_parent_class)->destroy (widget);
}

static void
complex_grid_class_init (ComplexGridClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = complex_grid_destroy;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_CONSTRAINT_LAYOUT);
}

/* Layout:
 *
 *   +--------------------------------------+
 *   |             +-----------+            |
 *   |             |  Child 4  |            |
 *   | +-----------+-----------+----------+ |
 *   | |  Child 1  |  Child 2  |  Child 3 | |
 *   | +-----------+-----------+----------+ |
 *   |             |  Child 5  |            |
 *   |             +-----------+            |
 *   +--------------------------------------+
 *
 */
static void
build_constraints (ComplexGrid         *self,
                   GtkConstraintLayout *manager)
{
  GtkGridConstraint *constraint;
  GtkConstraint *s;

  s = gtk_constraint_new (NULL, GTK_CONSTRAINT_ATTRIBUTE_LEFT,
                          GTK_CONSTRAINT_RELATION_EQ,
                          self->button1, GTK_CONSTRAINT_ATTRIBUTE_LEFT,
                          1.0, 0.0,
                          GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_layout_add_constraint (manager, s);

  s = gtk_constraint_new (self->button3, GTK_CONSTRAINT_ATTRIBUTE_RIGHT,
                          GTK_CONSTRAINT_RELATION_EQ,
                          NULL, GTK_CONSTRAINT_ATTRIBUTE_RIGHT,
                          1.0, 0.0,
                          GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_layout_add_constraint (manager, s);

  s = gtk_constraint_new (NULL, GTK_CONSTRAINT_ATTRIBUTE_TOP,
                          GTK_CONSTRAINT_RELATION_EQ,
                          self->button4, GTK_CONSTRAINT_ATTRIBUTE_TOP,
                          1.0, 0.0,
                          GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_layout_add_constraint (manager, s);

  s = gtk_constraint_new (NULL, GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                          GTK_CONSTRAINT_RELATION_EQ,
                          self->button5, GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                          1.0, 0.0,
                          GTK_CONSTRAINT_STRENGTH_REQUIRED);
  gtk_constraint_layout_add_constraint (manager, s);

  constraint = gtk_grid_constraint_new ();
  g_object_set (constraint, "column-homogeneous", TRUE, NULL);
  gtk_grid_constraint_add (constraint,
                           self->button1,
                           0, 1, 0, 1);
  gtk_grid_constraint_add (constraint,
                           self->button2,
                           1, 2, 0, 1);
  gtk_grid_constraint_add (constraint,
                           self->button3,
                           2, 3, 0, 1);
  gtk_constraint_layout_add_grid_constraint (manager, constraint);

  constraint = gtk_grid_constraint_new ();
  g_object_set (constraint, "row-homogeneous", TRUE, NULL);
  gtk_grid_constraint_add (constraint,
                           self->button4,
                           0, 1, 0, 1);
  gtk_grid_constraint_add (constraint,
                           self->button2,
                           0, 1, 1, 2);
  gtk_grid_constraint_add (constraint,
                           self->button5,
                           0, 1, 2, 3);
  gtk_constraint_layout_add_grid_constraint (manager, constraint);
}

static void
complex_grid_init (ComplexGrid *self)
{
  GtkWidget *widget = GTK_WIDGET (self);
  GtkLayoutManager *manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));

  self->button1 = gtk_button_new_with_label ("Child 1");
  gtk_widget_set_parent (self->button1, widget);
  gtk_widget_set_name (self->button1, "button1");

  self->button2 = gtk_button_new_with_label ("Child 2");
  gtk_widget_set_parent (self->button2, widget);
  gtk_widget_set_name (self->button2, "button2");

  self->button3 = gtk_button_new_with_label ("Child 3");
  gtk_widget_set_parent (self->button3, widget);
  gtk_widget_set_name (self->button3, "button3");

  self->button4 = gtk_button_new_with_label ("Child 4");
  gtk_widget_set_parent (self->button4, widget);
  gtk_widget_set_name (self->button4, "button4");

  self->button5 = gtk_button_new_with_label ("Child 5");
  gtk_widget_set_parent (self->button5, widget);
  gtk_widget_set_name (self->button5, "button5");

  build_constraints (self, GTK_CONSTRAINT_LAYOUT (manager));
}

GtkWidget *
do_constraints2 (GtkWidget *do_widget)
{
 static GtkWidget *window;

 if (!window)
   {
     GtkWidget *header, *box, *grid, *button;

     window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
     gtk_window_set_display (GTK_WINDOW (window), gtk_widget_get_display (do_widget));

     header = gtk_header_bar_new ();
     gtk_header_bar_set_title (GTK_HEADER_BAR (header), "Constraints");
     gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), FALSE);
     gtk_window_set_titlebar (GTK_WINDOW (window), header);
     g_signal_connect (window, "destroy",
                       G_CALLBACK (gtk_widget_destroyed), &window);

     box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
     gtk_container_add (GTK_CONTAINER (window), box);

     grid = g_object_new (complex_grid_get_type (), NULL);
     gtk_widget_set_hexpand (grid, TRUE);
     gtk_widget_set_vexpand (grid, TRUE);
     gtk_container_add (GTK_CONTAINER (box), grid);

     button = gtk_button_new_with_label ("Close");
     gtk_container_add (GTK_CONTAINER (box), button);
     gtk_widget_set_hexpand (grid, TRUE);
     g_signal_connect_swapped (button, "clicked",
                               G_CALLBACK (gtk_widget_destroy), window);
   }

 if (!gtk_widget_get_visible (window))
   gtk_widget_show (window);
 else
   gtk_widget_destroy (window);

 return window;
}
