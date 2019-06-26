/* Constraints
 *
 * GtkConstraintLayout provides a layout manager that uses relations
 * between widgets (also known as "constraints") to compute the position
 * and size of each child.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (SimpleGrid, simple_grid, SIMPLE, GRID, GtkWidget)

struct _SimpleGrid
{
  GtkWidget parent_instance;

  GtkWidget *button1, *button2;
  GtkWidget *button3;
};

G_DEFINE_TYPE (SimpleGrid, simple_grid, GTK_TYPE_WIDGET)

static void
simple_grid_destroy (GtkWidget *widget)
{
  SimpleGrid *self = SIMPLE_GRID (widget);

  g_clear_pointer (&self->button1, gtk_widget_destroy);
  g_clear_pointer (&self->button2, gtk_widget_destroy);
  g_clear_pointer (&self->button3, gtk_widget_destroy);

  GTK_WIDGET_CLASS (simple_grid_parent_class)->destroy (widget);
}

static void
simple_grid_class_init (SimpleGridClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = simple_grid_destroy;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_CONSTRAINT_LAYOUT);
}

/* Layout:
 *
 *   +-----------------------------+
 *   | +-----------+ +-----------+ |
 *   | |  Child 1  | |  Child 2  | |
 *   | +-----------+ +-----------+ |
 *   | +-------------------------+ |
 *   | |         Child 3         | |
 *   | +-------------------------+ |
 *   +-----------------------------+
 *
 * Constraints:
 *
 *   super.start = child1.start - 8
 *   child1.width = child2.width
 *   child1.end = child2.start - 12
 *   child2.end = super.end - 8
 *   super.start = child3.start - 8
 *   child3.end = super.end - 8
 *   super.top = child1.top - 8
 *   super.top = child2.top - 8
 *   child1.bottom = child3.top - 12
 *   child2.bottom = child3.top - 12
 *   child3.height = child1.height
 *   child3.height = child2.height
 *   child3.bottom = super.bottom - 8
 *
 */
static void
build_constraints (SimpleGrid          *self,
                   GtkConstraintLayout *manager)
{
  GtkConstraintGuide *guide;

  guide = g_object_new (GTK_TYPE_CONSTRAINT_GUIDE,
                        "min-width", 10,
                        "min-height", 10,
                        "nat-width", 100,
                        "nat-height", 10,
                        NULL);
  gtk_constraint_layout_add_guide (manager, guide);

  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new_constant (GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
                        GTK_CONSTRAINT_RELATION_LE,
                        200.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button2),
                        GTK_CONSTRAINT_ATTRIBUTE_WIDTH,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (guide),
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (guide),
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button2),
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button2),
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        GTK_CONSTRAINT_RELATION_EQ,
                        NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_START,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        GTK_CONSTRAINT_RELATION_EQ,
                        NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_END,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button2),
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        1.0,
                        -12.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button2),
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_TOP,
                        1.0,
                        -12.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button1),
                        GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
                        GTK_CONSTRAINT_RELATION_EQ,
                        GTK_CONSTRAINT_TARGET (self->button2),
                        GTK_CONSTRAINT_ATTRIBUTE_HEIGHT,
                        1.0,
                        0.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
  gtk_constraint_layout_add_constraint (manager,
    gtk_constraint_new (GTK_CONSTRAINT_TARGET (self->button3),
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        GTK_CONSTRAINT_RELATION_EQ,
                        NULL,
                        GTK_CONSTRAINT_ATTRIBUTE_BOTTOM,
                        1.0,
                        -8.0,
                        GTK_CONSTRAINT_STRENGTH_REQUIRED));
}

static void
simple_grid_init (SimpleGrid *self)
{
  GtkWidget *widget = GTK_WIDGET (self);

  self->button1 = gtk_button_new_with_label ("Child 1");
  gtk_widget_set_parent (self->button1, widget);
  gtk_widget_set_name (self->button1, "button1");

  self->button2 = gtk_button_new_with_label ("Child 2");
  gtk_widget_set_parent (self->button2, widget);
  gtk_widget_set_name (self->button2, "button2");

  self->button3 = gtk_button_new_with_label ("Child 3");
  gtk_widget_set_parent (self->button3, widget);
  gtk_widget_set_name (self->button3, "button3");

  GtkLayoutManager *manager = gtk_widget_get_layout_manager (GTK_WIDGET (self));
  build_constraints (self, GTK_CONSTRAINT_LAYOUT (manager));
}

GtkWidget *
do_constraints (GtkWidget *do_widget)
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

     grid = g_object_new (simple_grid_get_type (), NULL);
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
