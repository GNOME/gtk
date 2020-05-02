/* Pickers
 *
 * These widgets are mainly intended for use in preference dialogs.
 * They allow to select colors, fonts, files, directories and applications.
 */

#include <gtk/gtk.h>

GtkWidget *
do_pickers (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *table, *label, *picker;

  if (!window)
  {
    window = gtk_window_new ();
    gtk_window_set_display (GTK_WINDOW (window),
                            gtk_widget_get_display (do_widget));
    gtk_window_set_title (GTK_WINDOW (window), "Pickers");

    g_signal_connect (window, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &window);

    table = gtk_grid_new ();
    gtk_widget_set_margin_start (table, 20);
    gtk_widget_set_margin_end (table, 20);
    gtk_widget_set_margin_top (table, 20);
    gtk_widget_set_margin_bottom (table, 20);
    gtk_grid_set_row_spacing (GTK_GRID (table), 3);
    gtk_grid_set_column_spacing (GTK_GRID (table), 10);
    gtk_window_set_child (GTK_WINDOW (window), table);

    label = gtk_label_new ("Color:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);
    picker = gtk_color_button_new ();
    gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);
    gtk_grid_attach (GTK_GRID (table), picker, 1, 0, 1, 1);

    label = gtk_label_new ("Font:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);
    picker = gtk_font_button_new ();
    gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);
    gtk_grid_attach (GTK_GRID (table), picker, 1, 1, 1, 1);

    label = gtk_label_new ("File:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);
    picker = gtk_file_chooser_button_new ("Pick a File",
                                          GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);
    gtk_grid_attach (GTK_GRID (table), picker, 1, 2, 1, 1);

    label = gtk_label_new ("Folder:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    picker = gtk_file_chooser_button_new ("Pick a Folder",
                                          GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_grid_attach (GTK_GRID (table), label, 0, 3, 1, 1);
    gtk_grid_attach (GTK_GRID (table), picker, 1, 3, 1, 1);

    label = gtk_label_new ("Mail:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);
    picker = gtk_app_chooser_button_new ("x-scheme-handler/mailto");
    gtk_app_chooser_button_set_show_dialog_item (GTK_APP_CHOOSER_BUTTON (picker), TRUE);
    gtk_grid_attach (GTK_GRID (table), label, 0, 4, 1, 1);
    gtk_grid_attach (GTK_GRID (table), picker, 1, 4, 1, 1);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
