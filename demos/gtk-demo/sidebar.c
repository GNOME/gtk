/* Stack Sidebar
 *
 * GtkStackSidebar provides an automatic sidebar widget to control
 * navigation of a GtkStack object. This widget automatically updates it
 * content based on what is presently available in the GtkStack object,
 * and using the "title" child property to set the display labels.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

GtkWidget *
do_sidebar (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *sidebar;
  GtkWidget *stack;
  GtkWidget *box;
  GtkWidget *widget;
  GtkWidget *header;
  const gchar* pages[] = {
    "Welcome to GTK",
    "GtkStackSidebar Widget",
    "Automatic navigation",
    "Consistent appearance",
    "Scrolling",
    "Page 6",
    "Page 7",
    "Page 8",
    "Page 9",
    NULL
  };
  const gchar *c = NULL;
  guint i;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
      gtk_widget_set_size_request (window, 500, 350);

      header = gtk_header_bar_new ();
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR(header), TRUE);
      gtk_window_set_titlebar (GTK_WINDOW(window), header);
      gtk_window_set_title (GTK_WINDOW(window), "Stack Sidebar");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      sidebar = gtk_stack_sidebar_new ();
      gtk_container_add (GTK_CONTAINER (box), sidebar);

      stack = gtk_stack_new ();
      gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);
      gtk_stack_sidebar_set_stack (GTK_STACK_SIDEBAR (sidebar), GTK_STACK (stack));
      gtk_widget_set_hexpand (stack, TRUE);

      gtk_container_add (GTK_CONTAINER (box), stack);

      for (i=0; (c = *(pages+i)) != NULL; i++ )
        {
          if (i == 0)
            {
              widget = gtk_image_new_from_icon_name ("org.gtk.Demo4");
              gtk_style_context_add_class (gtk_widget_get_style_context (widget), "icon-dropshadow");
              gtk_image_set_pixel_size (GTK_IMAGE (widget), 256);
            }
          else
            {
              widget = gtk_label_new (c);
            }
          gtk_stack_add_named (GTK_STACK (stack), widget, c);
          g_object_set (gtk_stack_get_page (GTK_STACK (stack), widget), "title", c, NULL);
        }

       gtk_container_add (GTK_CONTAINER (window), box);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
