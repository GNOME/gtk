/* Paintable/SVG
 *
 * This demo shows wrapping a librsvg RsvgHandle in a GdkPaintable
 * to display an SVG image in a GtkPicture that can be scaled by
 * resizing the window.
 *
 * It also demonstrates an implementation of GtkSymbolicPaintable
 * for rendering symbolic SVG icons. Note that symbolic recoloring
 * requires using a GtkImage as a widget.
 */

#include <gtk/gtk.h>
#include <librsvg/rsvg.h>

#include "svgpaintable.h"
#include "symbolicpaintable.h"

static void
image_clicked (GtkGestureClick *click,
               int n_press,
               double x,
               double y,
               GtkImage *image)
{
  GtkPathPaintable *paintable = GTK_PATH_PAINTABLE (gtk_image_get_paintable (image));
  guint state = gtk_path_paintable_get_state (paintable);
  guint max_state = gtk_path_paintable_get_max_state (paintable);

  if (state == GTK_PATH_PAINTABLE_STATE_EMPTY)
    gtk_path_paintable_set_state (paintable, 0);
  else if (state < max_state)
    gtk_path_paintable_set_state (paintable, state + 1);
  else
    gtk_path_paintable_set_state (paintable, GTK_PATH_PAINTABLE_STATE_EMPTY);
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
      GdkPaintable *paintable;
      GtkWidget *image;

      image = gtk_window_get_child (GTK_WINDOW (window));

      if (g_str_has_suffix (g_file_peek_path (file), "-symbolic.svg"))
        {
          paintable = GDK_PAINTABLE (symbolic_paintable_new (file));
          if (!GTK_IS_IMAGE (image))
            {
              image = gtk_image_new ();
              gtk_image_set_pixel_size (GTK_IMAGE (image), 64);
              gtk_window_set_child (GTK_WINDOW (window), image);
            }

          gtk_image_set_from_paintable (GTK_IMAGE (image), paintable);
        }
      else if (g_str_has_suffix (g_file_peek_path (file), ".gpa"))
        {
          GBytes *bytes = g_file_load_bytes (file, NULL, NULL, NULL);
          paintable = GDK_PAINTABLE (gtk_path_paintable_new_from_bytes (bytes, NULL));
          g_bytes_unref (bytes);
          if (!GTK_IS_IMAGE (image))
            {
              GtkEventController *controller;

              image = gtk_image_new ();
              gtk_image_set_pixel_size (GTK_IMAGE (image), 64);
              gtk_window_set_child (GTK_WINDOW (window), image);
              controller = GTK_EVENT_CONTROLLER (gtk_gesture_click_new ());
              g_signal_connect (controller, "pressed",
                                G_CALLBACK (image_clicked), image);
              gtk_widget_add_controller (image, controller);
            }

          gtk_image_set_from_paintable (GTK_IMAGE (image), paintable);
        }
      else
        {
          paintable = GDK_PAINTABLE (svg_paintable_new (file));
          if (!GTK_IS_PICTURE (image))
            {
              image = gtk_picture_new ();
              gtk_widget_set_size_request (image, 16, 16);
              gtk_window_set_child (GTK_WINDOW (window), image);
            }

          gtk_picture_set_paintable (GTK_PICTURE (image), paintable);
        }

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
  GFile *file;
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

      file = g_file_new_for_uri ("resource:///paintable_svg/org.gtk.gtk4.NodeEditor.Devel.svg");
      paintable = GDK_PAINTABLE (svg_paintable_new (file));
      gtk_picture_set_paintable (GTK_PICTURE (image), paintable);
      g_object_unref (paintable);
      g_object_unref (file);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
