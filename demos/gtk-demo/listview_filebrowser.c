/* Lists/File browser
 *
 * This demo shows off the different layouts that are quickly achievable
 * with GtkGridView by implementing a file browser with different views.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
up_clicked_cb (GtkButton        *button,
               GtkDirectoryList *list)
{
  GFile *file;

  file = g_file_get_parent (gtk_directory_list_get_file (list));
  if (file == NULL)
    return;

  gtk_directory_list_set_file (list, file);
}

static void
view_button_clicked_cb (GtkButton   *button,
                        GtkListView *view)
{
  g_print ("Somebody clicked me!");
}

GtkWidget *
do_listview_filebrowser (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *view;
      GtkBuilder *builder;
      GtkDirectoryList *dirlist;
      GFile *file;

      builder = gtk_builder_new_from_resource ("/listview_filebrowser/listview_filebrowser.ui");
      gtk_builder_add_callback_symbols (builder,
                                        "up_clicked_cb", G_CALLBACK (up_clicked_cb),
                                        "view_button_clicked_cb", G_CALLBACK (view_button_clicked_cb),
                                        NULL);
      gtk_builder_connect_signals (builder, NULL);
      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      /* Create the model and fill it with the contents of the current directory */
      file = g_file_new_for_path (g_get_current_dir ());
      dirlist = GTK_DIRECTORY_LIST (gtk_builder_get_object (builder, "dirlist"));
      gtk_directory_list_set_file (dirlist, file);
      g_object_unref (file);

      /* grab focus in the view */
      view = GTK_WIDGET (gtk_builder_get_object (builder, "view"));
      gtk_widget_grab_focus (view);

      g_object_unref (builder);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
