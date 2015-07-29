/* Text View/Markup
 *
 * GtkTextBuffer lets you define your own tags that can influence
 * text formatting in a variety of ways. In this example, we show
 * that GtkTextBuffer can load Pango markup and automatically generate
 * suitable tags.
 */

#include <gtk/gtk.h>

GtkWidget *
do_markup (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *view;
      GtkWidget *sw;
      GtkTextBuffer *buffer;
      GtkTextIter iter;
      GBytes *bytes;
      const gchar *markup;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_default_size (GTK_WINDOW (window), 450, 450);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_window_set_title (GTK_WINDOW (window), "Markup");

      view = gtk_text_view_new ();
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (view), GTK_WRAP_WORD);
      gtk_text_view_set_left_margin (GTK_TEXT_VIEW (view), 10);
      gtk_text_view_set_right_margin (GTK_TEXT_VIEW (view), 10);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_container_add (GTK_CONTAINER (window), sw);
      gtk_container_add (GTK_CONTAINER (sw), view);

      bytes = g_resources_lookup_data ("/markup/markup.txt", 0, NULL);
      markup = (const gchar *)g_bytes_get_data (bytes, NULL);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
      gtk_text_buffer_get_start_iter (buffer, &iter);
      gtk_text_buffer_insert_markup (buffer, &iter, markup, -1);

      g_bytes_unref (bytes);

      gtk_widget_show_all (sw);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
