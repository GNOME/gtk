/* Theming/Style Classes
 *
 * GTK+ uses CSS for theming. Style classes can be associated
 * with widgets to inform the theme about intended rendering.
 *
 * This demo shows some common examples where theming features
 * of GTK+ are used for certain effects: primary toolbars,
 * inline toolbars and linked buttons.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

GtkWidget *
do_theming_style_classes (GtkWidget *do_widget)
{
  GtkWidget *grid;
  GtkBuilder *builder;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Style Classes");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      gtk_container_set_border_width (GTK_CONTAINER (window), 12);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      builder = gtk_builder_new_from_resource ("/theming_style_classes/theming.ui");

      grid = (GtkWidget *)gtk_builder_get_object (builder, "grid");
      gtk_widget_show_all (grid);
      gtk_container_add (GTK_CONTAINER (window), grid);
      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
