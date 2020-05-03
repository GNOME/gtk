/* Header Bar
 *
 * GtkHeaderBar is a container that is suitable for implementing
 * window titlebars. One of its features is that it can position
 * a title (and optional subtitle) centered with regard to the
 * full width, regardless of variable-width content at the left
 * or right.
 *
 * It is commonly used with gtk_window_set_titlebar()
 */

#include <gtk/gtk.h>

GtkWidget *
do_headerbar (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *header;
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *image;
  GIcon *icon;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),  gtk_widget_get_display (do_widget));
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

      header = gtk_header_bar_new ();
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (header), TRUE);
      gtk_header_bar_set_title (GTK_HEADER_BAR (header), "Welcome to Facebook - Log in, sign up or learn more");
      gtk_header_bar_set_has_subtitle (GTK_HEADER_BAR (header), FALSE);

      button = gtk_button_new ();
      icon = g_themed_icon_new ("mail-send-receive-symbolic");
      image = gtk_image_new_from_gicon (icon);
      g_object_unref (icon);
      gtk_button_set_child (GTK_BUTTON (button), image);
      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_widget_add_css_class (box, "linked");
      button = gtk_button_new ();
      gtk_button_set_child (GTK_BUTTON (button), gtk_image_new_from_icon_name ("pan-start-symbolic"));
      gtk_container_add (GTK_CONTAINER (box), button);
      button = gtk_button_new ();
      gtk_button_set_child (GTK_BUTTON (button), gtk_image_new_from_icon_name ("pan-end-symbolic"));
      gtk_container_add (GTK_CONTAINER (box), button);

      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), box);

      gtk_window_set_titlebar (GTK_WINDOW (window), header);

      gtk_container_add (GTK_CONTAINER (window), gtk_text_view_new ());
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
