/* Cursors
 *
 * Demonstrates a useful set of available cursors.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
on_destroy (gpointer data)
{
  window = NULL;
}

GtkWidget *
do_cursors (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/cursors/cursors.ui");
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (on_destroy), NULL);
      g_object_set_data_full (G_OBJECT (window), "builder", builder, g_object_unref);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    {
      gtk_widget_destroy (window);
    }

  return window;
}
