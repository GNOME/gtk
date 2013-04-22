/* Stack
 *
 * GtkStack is a container that shows a single child at a time,
 * with nice transitions when the visible child changes.
 *
 * GtkStackSwitcher adds buttons to control which child is visible.
 */

#include <gtk/gtk.h>

static GtkBuilder *builder;

GtkWidget *
do_stack (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GError *err = NULL;

  if (!window)
    {
      builder = gtk_builder_new ();
      gtk_builder_add_from_resource (builder, "/stack/stack.ui", &err);
      if (err)
        {
          g_error ("ERROR: %s\n", err->message);
          return NULL;
        }
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }


  return window;
}
