#include <gtk/gtk.h>
#include <string.h>

static GdkClipboard *clipboard;

static void
clear (GtkWidget *entry)
{
  gdk_clipboard_clear (clipboard);
}

/* text */

static void
copy_text (GtkWidget *w)
{
  gdk_clipboard_set_text (clipboard, gtk_entry_get_text (GTK_ENTRY (w)));
}

static void
text_received (GObject      *source,
               GAsyncResult *res,
               gpointer      data)
{
  gchar *text;
  GtkWidget *entry = data;
  GError *error = NULL;

  text = gdk_clipboard_get_text_finish (GDK_CLIPBOARD (source), res, &error);
  if (!text)
    {
      g_print ("error receiving text: %s\n", error ? error->message : "no error set");
      if (error)
        g_error_free (error);
      return;
    }

  gtk_entry_set_text (GTK_ENTRY (entry), text);
  g_free (text);
}

static void
paste_text (GtkWidget *w)
{
  gdk_clipboard_get_text_async (clipboard, NULL, text_received, w);
}

static void
has_text (GtkWidget *w)
{
  gtk_widget_set_sensitive (w, gdk_clipboard_text_available (clipboard));
}

/* image */

static void
copy_image (GtkWidget *w)
{
  gdk_clipboard_set_image (clipboard, gtk_image_get_pixbuf (GTK_IMAGE (w)));
}

static void
image_received (GObject      *source,
                GAsyncResult *res,
                gpointer      data)
{
  GtkWidget *image = data;
  GdkPixbuf *pixbuf;
  GError *error = NULL;

  pixbuf = gdk_clipboard_get_image_finish (GDK_CLIPBOARD (source), res, &error);
  if (!pixbuf)
    {
      g_print ("error receiving image: %s\n", error ? error->message : "no error set");
      if (error)
        g_error_free (error);
      return;
    }

  gtk_image_set_from_pixbuf (GTK_IMAGE (image), pixbuf);
  g_object_unref (pixbuf);
}

static void
paste_image (GtkWidget *w)
{
  gdk_clipboard_get_image_async (clipboard, NULL, image_received, w);
}

static void
has_image (GtkWidget *w)
{
  gtk_widget_set_sensitive (w, gdk_clipboard_image_available (clipboard));
}

/* data */

static void
provider (GdkClipboard  *clipboard,
          const gchar   *content_type,
          GOutputStream *output,
          gpointer       data)
{
  gchar *text = data;

  g_output_stream_write_all (output, text, strlen (text), NULL, NULL, NULL);
  g_output_stream_close (output, NULL, NULL);      
}

static void
copy_data (GtkWidget *w)
{
  const gchar *types[2] = { "foo/bar", NULL };

  gdk_clipboard_set_data (clipboard, types, provider,
                          (gpointer)g_strdup (gtk_entry_get_text (GTK_ENTRY (w))), g_free);
}

static void
data_received (GObject      *source,
               GAsyncResult *res,
               gpointer      data)
{
  GInputStream *is;
  GError *error = NULL;
  gchar buffer[128];
  gssize size;
  GtkWidget *w = data;

  is = gdk_clipboard_get_data_finish (GDK_CLIPBOARD (source), res, &error);
  if (!is)
    return;

  size = g_input_stream_read (is, buffer, 128, NULL, NULL);
  if (size == -1)
    {
      g_object_unref (is);
      return;
    }

  gtk_entry_set_text (GTK_ENTRY (w), buffer);
  g_object_unref (is);
}

static void
paste_data (GtkWidget *w)
{
  gdk_clipboard_get_data_async (clipboard, "foo/bar", NULL, data_received, w);
}

static void
has_data (GtkWidget *w)
{
  gtk_widget_set_sensitive (w, gdk_clipboard_data_available (clipboard, "foo/bar"));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *box;
  GtkWidget *entry;
  GtkWidget *button;
  GtkWidget *image;

  gtk_init (NULL, NULL);

  clipboard = gdk_display_get_clipboard (gdk_display_get_default ());

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (vbox), box);
  entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (box), entry);
  button = gtk_button_new_with_label ("Copy");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (copy_text), entry);
  button = gtk_button_new_with_label ("Paste");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (paste_text), entry);
  g_signal_connect_swapped (clipboard, "changed", G_CALLBACK (has_text), button);
  button = gtk_button_new_with_label ("Clear");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (clear), entry);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (vbox), box);
  image = gtk_image_new ();
  if (argc > 1)
    gtk_image_set_from_file (GTK_IMAGE (image), argv[1]);
  else
    gtk_image_set_from_resource (GTK_IMAGE (image), "/org/gtk/libgtk/theme/Adwaita/assets/slider-vert-scale-has-marks-above@2.png");
  gtk_container_add (GTK_CONTAINER (box), image);
  button = gtk_button_new_with_label ("Copy");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (copy_image), image);
  button = gtk_button_new_with_label ("Paste");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (paste_image), image);
  g_signal_connect_swapped (clipboard, "changed", G_CALLBACK (has_image), button);
  button = gtk_button_new_with_label ("Clear");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (clear), image);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add (GTK_CONTAINER (vbox), box);
  entry = gtk_entry_new ();
  gtk_container_add (GTK_CONTAINER (box), entry);
  button = gtk_button_new_with_label ("Copy");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (copy_data), entry);
  button = gtk_button_new_with_label ("Paste");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (paste_data), entry);
  g_signal_connect_swapped (clipboard, "changed", G_CALLBACK (has_data), button);
  button = gtk_button_new_with_label ("Clear");
  gtk_container_add (GTK_CONTAINER (box), button);
  g_signal_connect_swapped (button, "clicked", G_CALLBACK (clear), entry);

  g_signal_emit_by_name (clipboard, "changed", 0);

  gtk_widget_show_all (window);

  gtk_main ();
  
  return 0;
}
