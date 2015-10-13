/* Shortcuts Window
 *
 * GtkShortcutsWindow is a window that provides a help overlay
 * for shortcuts and gestures in an application.
 */

#include <gtk/gtk.h>

static void
show_clock_shortcuts (GtkWidget *window)
{
  g_object_set (window, "view-name", NULL, NULL);
  gtk_widget_show (window);
}

static void
show_clock_shortcuts_stopwatch (GtkWidget *window)
{
  g_object_set (window, "view-name", "stopwatch", NULL);
  gtk_widget_show (window);
}

GtkWidget *
do_shortcutswindow (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  static gboolean icons_added = FALSE;

  if (!icons_added)
    {
      icons_added = TRUE;
      gtk_icon_theme_add_resource_path (gtk_icon_theme_get_default (), "/icons");
    }

  g_type_ensure (G_TYPE_FILE_ICON);

  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/shortcuts/shortcuts.ui");
      gtk_builder_add_callback_symbols (builder,
                                        "show_clock_shortcuts", G_CALLBACK (show_clock_shortcuts),
                                        "show_clock_shortcuts_stopwatch", G_CALLBACK (show_clock_shortcuts_stopwatch),
                                        NULL);
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}
