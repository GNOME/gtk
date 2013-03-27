#include <gtk/gtk.h>

static const gchar css[] =
 ".background { "
 " background-image: -gtk-gradient (linear, center top, center bottom, "
 "      from (red), "
 "      to (blue)); "
 " border-radius: 10px 10px 0px 0px; "
 " border-width: 0px; "
 "}"
 ".titlebar:backdrop { "
 " background-image: none; "
 " background-color: @bg_color; "
 " border-radius: 10px 10px 0px 0px; "
 "}"
 ".titlebar { "
 " background-image: -gtk-gradient (linear, center top, center bottom, "
 "      from (white), "
 "      to (@bg_color)); "
 " border-radius: 10px 10px 0px 0px; "
 "}";

static void
change_title (GtkButton *button, gpointer data)
{
  GtkWidget *headerbar = GTK_WIDGET (data);

  if (gtk_header_bar_get_custom_title (GTK_HEADER_BAR (headerbar)) == NULL)
    {
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (headerbar), gtk_check_button_new_with_label ("Middle"));
    }
  else
    {
      gtk_header_bar_set_custom_title (GTK_HEADER_BAR (headerbar), NULL);
      gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar), "Middle");
    }
}

static void
change_subtitle (GtkButton *button, gpointer data)
{
  GtkWidget *headerbar = GTK_WIDGET (data);

  if (gtk_header_bar_get_subtitle (GTK_HEADER_BAR (headerbar)) == NULL)
    {
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (headerbar), "(subtle subtitle)");
    }
  else
    {
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (headerbar), NULL);
    }
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *header;
  GtkWidget *footer;
  GtkWidget *button;
  GtkWidget *image;
  GtkWidget *content;
  GtkCssProvider *provider;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  header = gtk_header_bar_new ();
  gtk_style_context_add_class (gtk_widget_get_style_context (header), "titlebar");
  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1, NULL);
  gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (window),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);

  gtk_header_bar_set_title (GTK_HEADER_BAR (header), "Example header");

  button = gtk_button_new_from_stock (GTK_STOCK_CLOSE);
  gtk_style_context_add_class (gtk_widget_get_style_context (button), "suggested-action");
  g_signal_connect (button, "clicked", G_CALLBACK (gtk_main_quit), NULL);

  gtk_header_bar_pack_end (GTK_HEADER_BAR (header), button);

  button = gtk_button_new ();
  image = gtk_image_new_from_icon_name ("bookmark-new-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (button), image);

  gtk_header_bar_pack_start (GTK_HEADER_BAR (header), button);

  gtk_window_set_titlebar (GTK_WINDOW (window), header);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  footer = gtk_header_bar_new ();
  button = gtk_button_new_with_label ("Start");
  g_signal_connect (button, "clicked", G_CALLBACK (change_title), footer);
  gtk_header_bar_pack_start (GTK_HEADER_BAR (footer), button);
  gtk_header_bar_set_custom_title (GTK_HEADER_BAR (footer), gtk_check_button_new_with_label ("Middle"));
  button = gtk_button_new_with_label ("End 1");
  g_signal_connect (button, "clicked", G_CALLBACK (change_subtitle), header);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (footer), button);
  gtk_header_bar_pack_end (GTK_HEADER_BAR (footer), gtk_button_new_with_label ("End 2"));
  gtk_box_pack_end (GTK_BOX (box), footer, FALSE, FALSE, 0);

  content = gtk_image_new_from_icon_name ("start-here-symbolic", GTK_ICON_SIZE_DIALOG);
  gtk_image_set_pixel_size (GTK_IMAGE (content), 512);

  gtk_box_pack_start (GTK_BOX (box), content, FALSE, TRUE, 0);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
