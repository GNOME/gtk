/* Pickers
 * #Keywords: GtkColorDialog, GtkFontDialog, GtkFileDialog
 *
 * These widgets are mainly intended for use in preference dialogs.
 * They allow to select colors, fonts and applications.
 *
 * This demo shows both the default appearance for these dialogs,
 * as well as some of the customizations that are possible.
 */

#include <gtk/gtk.h>

static void
file_opened (GObject *source,
             GAsyncResult *result,
             void *data)
{
  GFile *file;
  GError *error = NULL;
  char *name;

  file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (source), result, &error);

  if (!file)
    {
      g_print ("%s\n", error->message);
      g_error_free (error);
      return;
    }

  name = g_file_get_basename (file);
  gtk_label_set_label (GTK_LABEL (data), name);
  g_free (name);
}

static gboolean
abort_mission (gpointer data)
{
  GCancellable *cancellable = data;

  g_cancellable_cancel (cancellable);

  return G_SOURCE_REMOVE;
}

static void
open_file (GtkButton *picker,
           GtkLabel  *label)
{
  GtkWindow *parent = GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (picker)));
  GtkFileDialog *dialog;
  GCancellable *cancellable;

  dialog = gtk_file_dialog_new ();

  cancellable = g_cancellable_new ();

  g_timeout_add_seconds_full (G_PRIORITY_DEFAULT,
                              20,
                              abort_mission, g_object_ref (cancellable), g_object_unref);

  gtk_file_dialog_open (dialog, parent, NULL, cancellable, file_opened, label);

  g_object_unref (cancellable);
  g_object_unref (dialog);
}

GtkWidget *
do_pickers (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *table, *label, *picker, *button;

  if (!window)
  {
    window = gtk_window_new ();
    gtk_window_set_display (GTK_WINDOW (window),
                            gtk_widget_get_display (do_widget));
    gtk_window_set_title (GTK_WINDOW (window), "Pickers");
    g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

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
    gtk_grid_attach (GTK_GRID (table), label, 0, 0, 1, 1);

    picker = gtk_color_dialog_button_new (gtk_color_dialog_new ());
    gtk_grid_attach (GTK_GRID (table), picker, 1, 0, 1, 1);

    label = gtk_label_new ("Font:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_grid_attach (GTK_GRID (table), label, 0, 1, 1, 1);

    picker = gtk_font_dialog_button_new (gtk_font_dialog_new ());
    gtk_grid_attach (GTK_GRID (table), picker, 1, 1, 1, 1);

    label = gtk_label_new ("File:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);
    gtk_grid_attach (GTK_GRID (table), label, 0, 2, 1, 1);

    picker = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
    button = gtk_button_new_from_icon_name ("document-open-symbolic");
    label = gtk_label_new ("None");
    gtk_label_set_xalign (GTK_LABEL (label), 0.);
    gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_hexpand (label, TRUE);
    g_signal_connect (button, "clicked", G_CALLBACK (open_file), label);
    gtk_box_append (GTK_BOX (picker), label);
    gtk_box_append (GTK_BOX (picker), button);
    gtk_grid_attach (GTK_GRID (table), picker, 1, 2, 1, 1);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

    label = gtk_label_new ("Mail:");
    gtk_widget_set_halign (label, GTK_ALIGN_START);
    gtk_widget_set_valign (label, GTK_ALIGN_CENTER);
    gtk_widget_set_hexpand (label, TRUE);

    picker = gtk_app_chooser_button_new ("x-scheme-handler/mailto");
    gtk_app_chooser_button_set_show_dialog_item (GTK_APP_CHOOSER_BUTTON (picker), TRUE);

G_GNUC_END_IGNORE_DEPRECATIONS

    gtk_grid_attach (GTK_GRID (table), label, 0, 3, 1, 1);
    gtk_grid_attach (GTK_GRID (table), picker, 1, 3, 1, 1);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
