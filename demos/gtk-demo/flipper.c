/* Flipper
 * #Keywords: Rotation, Rotate, Orientation
 *
 * Demonstrates the GtkFlipper widget, which makes it easy to apply orientation
 * changes to widgets.
 */

#include <gtk/gtk.h>

GtkWidget *
do_flipper (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/flipper/flipper.ui");

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window), gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
