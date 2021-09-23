/* Color Profiles
 *
 * Demonstrates support for color profiles.
 *
 * The test images used here are taken from http://displaycal.net/icc-color-management-test/
 * and are licensed under the Creative Commons BY-SA 4.0 International License
 */

#include <gtk/gtk.h>



GtkWidget*
do_colorprofiles (GtkWidget *do_widget)
{
  static GtkWidget *window;

  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/colorprofiles/colorprofiles.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);
      g_object_unref (builder);

    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
