/* Builder
 *
 * Demonstrates an interface loaded from a XML description.
 */

#include <gtk/gtk.h>

static GtkBuilder *builder;

G_MODULE_EXPORT void
quit_activate (GtkAction *action)
{
  GtkWidget *window;

  window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
  gtk_widget_destroy (window);
}

G_MODULE_EXPORT void
about_activate (GtkAction *action)
{
  GtkWidget *about_dlg;

  about_dlg = GTK_WIDGET (gtk_builder_get_object (builder, "aboutdialog1"));
  gtk_dialog_run (GTK_DIALOG (about_dlg));
  gtk_widget_hide (about_dlg);
}

G_MODULE_EXPORT void
help_activate (GtkAction *action)
{
  g_print ("Help not available\n");
}

GtkWidget *
do_builder (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GError *err = NULL;

  if (!window)
    {
      builder = gtk_builder_new ();
      gtk_builder_add_from_resource (builder, "/builder/demo.ui", &err);
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
