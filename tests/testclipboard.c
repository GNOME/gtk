/*
 * Copyright (C) 2011  Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

GtkClipboard *clipboard;
GtkWidget *image;
GtkWidget *label;

#define SIZE 256.

static void
image_request_cb (GtkClipboard *clipboard,
                  GdkPixbuf *pixbuf,
                  gpointer data)
{
  GdkPixbuf *copy;
  int height;
  int width;
  gdouble factor;
  char *str;

  if (pixbuf != NULL)
    {
      height = gdk_pixbuf_get_height (pixbuf);
      width = gdk_pixbuf_get_width (pixbuf);

      factor = MAX ((SIZE / height), (SIZE / width));

      copy = gdk_pixbuf_scale_simple (pixbuf, width * factor, height * factor, GDK_INTERP_BILINEAR);
      gtk_image_set_from_pixbuf (GTK_IMAGE (image), copy);
      g_object_unref (copy);
      str = g_strdup_printf ("<b>Image</b> %d \342\234\225 %d", width, height);
    }
  else
    {
      str = g_strdup ("<b>No image data</b>");
    }
  gtk_label_set_markup (GTK_LABEL (label), str);
  g_free (str);
}

static void
update_display (void)
{
  gtk_clipboard_request_image (clipboard, image_request_cb, NULL);
}

static void
on_owner_change (GtkClipboard *clipboard,
                 GdkEvent     *event,
                 gpointer      user_data)
{
  update_display ();
}

static void
on_response (GtkDialog *dialog,
             gint       response_id,
             gpointer   user_data)
{
  switch (response_id)
    {
    case 0:
      /* copy large */
      {
        GtkIconTheme *theme;
        GdkPixbuf *pixbuf;
        theme = gtk_icon_theme_get_default ();
        pixbuf = gtk_icon_theme_load_icon (theme, "terminal", 1600, 0, NULL);
        gtk_clipboard_set_image (clipboard, pixbuf);
      }
      break;
    case 1:
      /* copy small */
      {
        GtkIconTheme *theme;
        GdkPixbuf *pixbuf;
        theme = gtk_icon_theme_get_default ();
        pixbuf = gtk_icon_theme_load_icon (theme, "terminal", 48, 0, NULL);
        gtk_clipboard_set_image (clipboard, pixbuf);
      }
      break;
    case GTK_RESPONSE_CLOSE:
    default:
      gtk_main_quit ();
      break;
    }
}

int
main (int argc, char **argv)
{
  GtkWidget *window;

  gtk_init (&argc, &argv);

  window = gtk_dialog_new_with_buttons ("Clipboard",
                                        NULL,
                                        0,
                                        "Copy Large", 0,
                                        "Copy Small", 1,
                                        "_Close", GTK_RESPONSE_CLOSE,
                                        NULL);

  image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))), image, FALSE, FALSE, 0);
  label = gtk_label_new ("No data found");
  gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))), label, FALSE, FALSE, 0);

  g_signal_connect (window, "response", G_CALLBACK (on_response), NULL);

  clipboard = gtk_clipboard_get_for_display (gtk_widget_get_display (window),
                                             GDK_SELECTION_CLIPBOARD);
  g_signal_connect (clipboard, "owner-change", G_CALLBACK (on_owner_change), NULL);

  update_display ();

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
