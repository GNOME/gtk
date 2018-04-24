/* Benchmark/Fishbowl
 *
 * This demo models the fishbowl demos seen on the web in a GTK way.
 * It's also a neat little tool to see how fast your computer (or
 * your GTK version) is.
 */

#include <gtk/gtk.h>

#include "gtkfishbowl.h"

GtkWidget *
do_fishbowl (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilder *builder;
      GtkWidget *bowl;

      g_type_ensure (GTK_TYPE_FISHBOWL);

      builder = gtk_builder_new_from_resource ("/fishbowl/fishbowl.ui");
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      bowl = GTK_WIDGET (gtk_builder_get_object (builder, "bowl"));
      gtk_fishbowl_set_use_icons (GTK_FISHBOWL (bowl), TRUE);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_widget_realize (window);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);


  return window;
}
