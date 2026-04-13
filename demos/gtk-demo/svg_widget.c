/* SVG
 *
 * This demo shows using a GtkSvgWidget to display a
 * scalable, animated, interactive SVG image.
 */

#include <gtk/gtk.h>

static void
open_response_cb (GObject      *source,
                  GAsyncResult *result,
                  void         *data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  GtkWidget *window = data;
  GFile *file;

  file = gtk_file_dialog_open_finish (dialog, result, NULL);
  if (file)
    {
      GtkSvgWidget *widget;
      GBytes *bytes;

      widget = GTK_SVG_WIDGET (gtk_window_get_child (GTK_WINDOW (window)));

      bytes = g_file_load_bytes (file, NULL, NULL, NULL);
      gtk_svg_widget_load_from_bytes (widget, bytes);
      g_bytes_unref (bytes);

      g_object_unref (file);
    }
}

static void
show_file_open (GtkWidget *button,
                GtkWidget *window)
{
  GtkFileFilter *filter;
  GtkFileDialog *dialog;
  GListStore *filters;

  dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, "Open SVG");

  filter = gtk_file_filter_new ();
  gtk_file_filter_add_mime_type (filter, "image/svg+xml");
  gtk_file_filter_add_mime_type (filter, "image/x-gtk-path-animation");
  gtk_file_filter_add_pattern (filter, "*.gpa");
  filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  g_list_store_append (filters, filter);
  g_object_unref (filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  g_object_unref (filters);

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (gtk_widget_get_root (button)),
                        NULL,
                        open_response_cb, window);
}

static void
launched_cb (GObject      *source,
             GAsyncResult *result,
             gpointer      data)
{
  GtkUriLauncher *launcher = GTK_URI_LAUNCHER (source);
  GtkWindow *window = data;
  GError *error = NULL;

  if (!gtk_uri_launcher_launch_finish (launcher, result, &error))
    {
      if (g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_FAILED))
        {
          GtkAlertDialog *alert = gtk_alert_dialog_new ("Opening url failed");
          gtk_alert_dialog_set_detail (alert, error->message);
          g_error_free (error);
          gtk_alert_dialog_show (alert, window);
          g_object_unref (alert);
        }
    }
}

static void
activate_cb (GtkSvgWidget *widget,
             const char   *id,
             const char   *url,
             GtkWindow    *window)
{
  if (url && g_str_has_prefix (url, "http://"))
    {
      GtkUriLauncher *launcher = gtk_uri_launcher_new (url);
      gtk_uri_launcher_launch (launcher, window, NULL, launched_cb, window);
      g_object_unref (launcher);
    }
}

static GtkWidget *window;

GtkWidget *
do_svg_widget (GtkWidget *do_widget)
{
  GtkWidget *header;
  GtkWidget *button;
  GtkWidget *widget;

  if (!window)
    {
      window = gtk_window_new ();
      header = gtk_header_bar_new ();
      gtk_window_set_titlebar (GTK_WINDOW (window), header);
      gtk_window_set_default_size (GTK_WINDOW (window), 330, 330);
      gtk_window_set_title (GTK_WINDOW (window), "SVG");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      button = gtk_button_new_with_mnemonic ("_Open");
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);

      g_signal_connect (button, "clicked", G_CALLBACK (show_file_open), window);

      widget = GTK_WIDGET (gtk_svg_widget_new ());
      gtk_window_set_child (GTK_WINDOW (window), widget);

      g_object_set (widget, "resource", "/svg_widget/gtk-logo-interactive.svg", NULL);
      gtk_widget_set_has_tooltip (widget, TRUE);

      g_signal_connect (widget, "activate", G_CALLBACK (activate_cb), window);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
