#include <gtk/gtk.h>

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *header;
  GtkWidget *title;
  GtkWidget *button;
  GtkWidget *box;
  GtkWidget *image;
  GIcon *icon;

  gtk_init (NULL, NULL);

  if (g_getenv ("DARK"))
    g_object_set (gtk_settings_get_default (), "gtk-application-prefer-dark-theme", TRUE, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);

  header = gtk_header_bar_new ();
  gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (header), TRUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (header), "titlebar");

  title = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (title), "<b>Welcome to Facebook - Log in, sign up or learn more</b>");
  gtk_label_set_ellipsize (GTK_LABEL (title), PANGO_ELLIPSIZE_END);
  gtk_widget_set_margin_left (title, 6);
  gtk_widget_set_margin_right (title, 6);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (header), title);

  button = gtk_button_new ();
  icon = g_themed_icon_new ("mail-send-receive-symbolic");
  image = gtk_image_new_from_gicon (icon, GTK_ICON_SIZE_BUTTON);
  g_object_unref (icon);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (box), "linked");
  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), gtk_arrow_new (GTK_ARROW_LEFT, GTK_SHADOW_NONE));
  gtk_container_add (GTK_CONTAINER (box), button);
  button = gtk_button_new ();
  gtk_container_add (GTK_CONTAINER (button), gtk_arrow_new (GTK_ARROW_RIGHT, GTK_SHADOW_NONE));
  gtk_container_add (GTK_CONTAINER (box), button);

  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), box);

  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  gtk_container_add (GTK_CONTAINER (window), gtk_text_view_new ());
  gtk_widget_show_all (window);

  gtk_main ();

  gtk_widget_destroy (window);

  return 0;
}
