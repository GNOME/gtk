/* Paintable/SVG
 *
 * This demo shows using GtkSvg to display an SVG image in
 * a GtkPicture that can be scaled by resizing the window.
 */

#include <gtk/gtk.h>

static void
image_clicked (GtkGestureClick *click,
               int n_press,
               double x,
               double y,
               GtkImage *image)
{
  GtkSvg *paintable = GTK_SVG (gtk_image_get_paintable (image));
  guint state = gtk_svg_get_state (paintable);
  guint n_states = gtk_svg_get_n_states (paintable);

  if (state == GTK_SVG_STATE_EMPTY)
    gtk_svg_set_state (paintable, 0);
  else if (state + 1 < n_states)
    gtk_svg_set_state (paintable, state + 1);
  else
    gtk_svg_set_state (paintable, GTK_SVG_STATE_EMPTY);
}

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
      GBytes *bytes;
      GdkPaintable *paintable;
      GtkWidget *image;
      GtkEventController *controller;

      bytes = g_file_load_bytes (file, NULL, NULL, NULL);
      paintable = GDK_PAINTABLE (gtk_svg_new_from_bytes (bytes));
      g_bytes_unref (bytes);

      image = gtk_picture_new ();
      gtk_window_set_child (GTK_WINDOW (window), image);

      controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
      g_signal_connect (controller, "pressed",
                        G_CALLBACK (image_clicked), image);
      gtk_widget_add_controller (image, controller);

      gtk_picture_set_paintable (GTK_PICTURE (image), paintable);

      g_object_unref (paintable);
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
  gtk_file_dialog_set_title (dialog, "Open svg image");

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

static GtkWidget *window;

GtkWidget *
do_paintable_svg (GtkWidget *do_widget)
{
  GtkWidget *header;
  GtkWidget *image;
  GtkWidget *button;
  GdkPaintable *paintable;

  if (!window)
    {
      window = gtk_window_new ();
      header = gtk_header_bar_new ();
      gtk_window_set_titlebar (GTK_WINDOW (window), header);
      gtk_window_set_default_size (GTK_WINDOW (window), 330, 330);
      gtk_window_set_title (GTK_WINDOW (window), "Paintable — SVG");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      button = gtk_button_new_with_mnemonic ("_Open");
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);

      image = gtk_picture_new ();
      gtk_widget_set_size_request (image, 16, 16);

      g_signal_connect (button, "clicked", G_CALLBACK (show_file_open), window);

      gtk_window_set_child (GTK_WINDOW (window), image);

      paintable = GDK_PAINTABLE (gtk_svg_new_from_resource ("/paintable_svg/org.gtk.gtk4.NodeEditor.Devel.svg"));
      gtk_picture_set_paintable (GTK_PICTURE (image), paintable);
      g_object_unref (paintable);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
