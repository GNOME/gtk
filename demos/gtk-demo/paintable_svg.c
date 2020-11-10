/* Paintable/SVG
 *
 * This demo shows wrapping a librsvg RsvgHandle in a GdkPaintable
 * to display an SVG image that can be scaled by resizing the window.
 *
 * This demo relies on librsvg, which GTK itself does not link against.
 */

#include <gtk/gtk.h>
#include <librsvg/rsvg.h>

#include "svgpaintable.h"


static void
file_set (GtkFileChooserButton *button,
          GtkWidget            *picture)
{
  GFile *file;
  GdkPaintable *paintable;

  file = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (button));

  paintable = svg_paintable_new (file);
  gtk_picture_set_paintable (GTK_PICTURE (picture), paintable);

  g_object_unref (paintable);
  g_object_unref (file);
}

static GtkWidget *window;

GtkWidget *
do_paintable_svg (GtkWidget *do_widget)
{
  GtkWidget *header;
  GtkWidget *picture;
  GtkFileFilter *filter;
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

      button = gtk_file_chooser_button_new ("Select an SVG file", GTK_FILE_CHOOSER_ACTION_OPEN);
      filter = gtk_file_filter_new ();
      gtk_file_filter_add_mime_type (filter, "image/svg+xml");
      gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (button), filter);
      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);

      picture = gtk_picture_new ();
      gtk_picture_set_can_shrink (GTK_PICTURE (picture), TRUE);
      gtk_widget_set_size_request (picture, 16, 16);

      g_signal_connect (button, "file-set", G_CALLBACK (file_set), picture);

      gtk_window_set_child (GTK_WINDOW (window), picture);

      file = g_file_new_for_uri ("resource:///paintable_svg/org.gtk.gtk4.NodeEditor.Devel.svg");
      paintable = svg_paintable_new (file);
      gtk_picture_set_paintable (GTK_PICTURE (picture), paintable);
      g_object_unref (paintable);
      g_object_unref (file);
    }

  if (!gtk_widget_get_visible (window))
    gtk_window_present (GTK_WINDOW (window));
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
