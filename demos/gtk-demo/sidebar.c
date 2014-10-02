/* Sidebar
 *
 * GtkSidebar provides an automatic sidebar widget to control navigation
 * of a GtkStack object. This widget automatically updates it content
 * based on what is presently available in the GtkStack object, and
 * using the "title" child property to set the display labels.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static GtkWidget *window = NULL;

GtkWidget *
do_sidebar (GtkWidget *do_widget)
{
  GtkWidget *sidebar;
  GtkWidget *stack;
  GtkWidget *box;
  GtkWidget *widget;
  GtkWidget *header;
  const gchar* pages[] = {
    "Welcome to GTK+",
    "GtkSidebar Widget",
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
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR(header), TRUE);
      gtk_window_set_titlebar (GTK_WINDOW(window), header);
      gtk_window_set_title (GTK_WINDOW(window), "Sidebar demo");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      sidebar = gtk_sidebar_new ();
      gtk_box_pack_start (GTK_BOX (box), sidebar, FALSE, FALSE, 0);

      stack = gtk_stack_new ();
      gtk_stack_set_transition_type (GTK_STACK (stack), GTK_STACK_TRANSITION_TYPE_SLIDE_UP_DOWN);
      gtk_sidebar_set_stack (GTK_SIDEBAR (sidebar), GTK_STACK (stack));

      /* Separator between sidebar and stack */
      widget = gtk_separator_new (GTK_ORIENTATION_VERTICAL);
      gtk_box_pack_start (GTK_BOX(box), widget, FALSE, FALSE, 0);

      gtk_box_pack_start (GTK_BOX (box), stack, TRUE, TRUE, 0);

      for (i=0; (c = *(pages+i)) != NULL; i++ )
        {
          if (i == 0)
            {
              widget = gtk_image_new_from_icon_name ("help-about", GTK_ICON_SIZE_MENU);
              gtk_image_set_pixel_size (GTK_IMAGE (widget), 256);
            }
          else
            {
              widget = gtk_label_new (c);
            }
          gtk_stack_add_named (GTK_STACK (stack), widget, c);
          gtk_container_child_set (GTK_CONTAINER (stack), widget, "title", c, NULL);
        }

       gtk_container_add (GTK_CONTAINER (window), box);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}
