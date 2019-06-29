/* Constraints/VFL
 *
 * GtkConstraintLayout allows defining constraints using a
 * compact syntax called Visual Format Language, or VFL.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (VflGrid, vfl_grid, VFL, GRID, GtkWidget)

struct _VflGrid
{
  GtkWidget parent_instance;

  GtkWidget *button1, *button2;
  GtkWidget *button3;
};

G_DEFINE_TYPE (VflGrid, vfl_grid, GTK_TYPE_WIDGET)

static void
vfl_grid_destroy (GtkWidget *widget)
{
  VflGrid *self = VFL_GRID (widget);

  g_clear_pointer (&self->button1, gtk_widget_destroy);
  g_clear_pointer (&self->button2, gtk_widget_destroy);
  g_clear_pointer (&self->button3, gtk_widget_destroy);

  GTK_WIDGET_CLASS (vfl_grid_parent_class)->destroy (widget);
}

static void
vfl_grid_class_init (VflGridClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->destroy = vfl_grid_destroy;

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
 * Visual format:
 *
 *   H:|-8-[view1(==view2)-12-[view2]-8-|
 *   H:|-8-[view3]-8-|
 *   V:|-8-[view1]-12-[view3(==view1)]-8-|
 *   V:|-8-[view2]-12-[view3(==view2)]-8-|
 */
static void
build_constraints (VflGrid          *self,
                   GtkConstraintLayout *manager)
{
  const char * const vfl[] = {
    "H:|-[button1(==button2)]-12-[button2]-|",
    "H:|-[button3]-|",
    "V:|-[button1]-12-[button3(==button1)]-|",
    "V:|-[button2]-12-[button3(==button2)]-|",
  };
  GError *error = NULL;

  gtk_constraint_layout_add_constraints_from_description (manager, vfl, G_N_ELEMENTS (vfl),
                                                          8, 8,
                                                          &error,
                                                          "button1", self->button1,
                                                          "button2", self->button2,
                                                          "button3", self->button3,
                                                          NULL);
  if (error != NULL)
    {
      g_printerr ("VFL parsing error:\n%s", error->message);
      g_error_free (error);
    }
}

static void
vfl_grid_init (VflGrid *self)
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
do_constraints3 (GtkWidget *do_widget)
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

     grid = g_object_new (vfl_grid_get_type (), NULL);
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
