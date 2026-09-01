/* This example is used to test (delayed) file drop on macOS.
 *
 * To test, drop one or more files (from Finder) on the window.
 *
 * https://gitlab.gnome.org/GNOME/gtk/-/work_items/8293
 */

#include <gtk/gtk.h>

static gboolean
on_drop(GtkDropTarget *target, const GValue *value, gdouble x, gdouble y, gpointer data)
{
  g_print ("on_drop\n");
  if (G_VALUE_HOLDS(value, GDK_TYPE_FILE_LIST))
    {
      GSList *files = g_value_get_boxed(value);

      for (GSList *iter = files; iter; iter = iter->next)
        {
          GFile *file = iter->data;
          gchar *path = g_file_get_path(file);
          g_print("Dropped file: %s\n", path);
          g_free(path);
        }
      return TRUE;
    }
  return FALSE;
}

static void
activate(GtkApplication *app, gpointer user_data)
{
  GtkWidget *window = gtk_application_window_new(app);
  gtk_window_set_title(GTK_WINDOW(window), "file drop");
  gtk_window_set_default_size(GTK_WINDOW(window), 400, 300);

  GtkDropTarget *drop_target = gtk_drop_target_new(G_TYPE_STRING, GDK_ACTION_COPY);
  gtk_drop_target_set_gtypes(drop_target, (GType[1]){GDK_TYPE_FILE_LIST}, 1);
  g_signal_connect(drop_target, "drop", G_CALLBACK(on_drop), NULL);
  gtk_widget_add_controller(window, GTK_EVENT_CONTROLLER(drop_target));

  gtk_window_present(GTK_WINDOW(window));
}

int
main(int argc, char **argv)
{
  GtkApplication *app = gtk_application_new("org.gtk.test-file-drop", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  return g_application_run(G_APPLICATION(app), argc, argv);
}

