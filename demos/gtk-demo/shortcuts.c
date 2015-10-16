/* Shortcuts Window
 *
 * GtkShortcutsWindow is a window that provides a help overlay
 * for shortcuts and gestures in an application.
 */

#include <gtk/gtk.h>

static void
show_shortcuts (GtkWidget   *window,
                const gchar *id,
                const gchar *view)
{
  GtkBuilder *builder;
  GtkWidget *overlay;
  gchar *path;

  path = g_strdup_printf ("/shortcuts/%s.ui", id);
  builder = gtk_builder_new_from_resource (path);
  g_free (path);
  overlay = GTK_WIDGET (gtk_builder_get_object (builder, id));
  gtk_window_set_transient_for (GTK_WINDOW (overlay), GTK_WINDOW (window));
  g_object_set (overlay, "view-name", view, NULL);
  gtk_widget_show (overlay);
}

static void
builder_shortcuts (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-builder", NULL);
}

static void
gedit_shortcuts (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-gedit", NULL);
}

static void
clocks_shortcuts (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-clocks", NULL);
}

static void
clocks_shortcuts_stopwatch (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-clocks", "stopwatch");
}

GtkWidget *
do_shortcuts (GtkWidget *do_widget)
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
                                        "builder_shortcuts", G_CALLBACK (builder_shortcuts),
                                        "gedit_shortcuts", G_CALLBACK (gedit_shortcuts),
                                        "clocks_shortcuts", G_CALLBACK (clocks_shortcuts),
                                        "clocks_shortcuts_stopwatch", G_CALLBACK (clocks_shortcuts_stopwatch),
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
