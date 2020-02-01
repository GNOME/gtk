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
  g_object_unref (builder);
}

void
shortcuts_builder_shortcuts (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-builder", NULL);
}

void
shortcuts_gedit_shortcuts (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-gedit", NULL);
}

void
shortcuts_clocks_shortcuts (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-clocks", NULL);
}

void
shortcuts_clocks_shortcuts_stopwatch (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-clocks", "stopwatch");
}

void
shortcuts_boxes_shortcuts (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-boxes", NULL);
}

void
shortcuts_boxes_shortcuts_wizard (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-boxes", "wizard");
}

void
shortcuts_boxes_shortcuts_display (GtkWidget *window)
{
  show_shortcuts (window, "shortcuts-boxes", "display");
}

GtkWidget *
do_shortcuts (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  static gboolean icons_added = FALSE;

  if (!icons_added)
    {
      icons_added = TRUE;
      gtk_icon_theme_add_resource_path (gtk_icon_theme_get_for_display (gtk_widget_get_display (do_widget)), "/icons");
    }

  g_type_ensure (G_TYPE_FILE_ICON);

  if (!window)
    {
      GtkBuilder *builder;

      builder = gtk_builder_new_from_resource ("/shortcuts/shortcuts.ui");
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window1"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
