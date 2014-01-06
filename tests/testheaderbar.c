#include <gtk/gtk.h>

static const gchar css[] =
 ".main.background { "
 " background-image: -gtk-gradient (linear, center top, center bottom, "
 "      from (red), "
 "      to (blue)); "
 " border-width: 0px; "
 "}"
 ".titlebar.backdrop { "
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
on_bookmark_clicked (GtkButton *button, gpointer data)
{
  GtkWindow *window = GTK_WINDOW (data);
  GtkWidget *chooser;

  chooser = gtk_file_chooser_dialog_new ("File Chooser Test",
                                         window,
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         "_Close",
                                         GTK_RESPONSE_CLOSE,
                                         NULL);

  g_signal_connect (chooser, "response",
                    G_CALLBACK (gtk_widget_destroy), NULL);

  gtk_widget_show (chooser);
}

static GtkWidget *header;

static void
change_subtitle (GtkButton *button, gpointer data)
{
  if (!GTK_IS_HEADER_BAR (header))
    return;

  if (gtk_header_bar_get_subtitle (GTK_HEADER_BAR (header)) == NULL)
    {
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (header), "(subtle subtitle)");
    }
  else
    {
      gtk_header_bar_set_subtitle (GTK_HEADER_BAR (header), NULL);
    }
}

static void
toggle_fullscreen (GtkButton *button, gpointer data)
{
  GtkWidget *window = GTK_WIDGET (data);
  static gboolean fullscreen = FALSE;

  if (fullscreen)
    {
      gtk_window_unfullscreen (GTK_WINDOW (window));
      fullscreen = FALSE;
    }
  else
    {
      gtk_window_fullscreen (GTK_WINDOW (window));
      fullscreen = TRUE;
    }
}

static void
change_header (GtkButton *button, gpointer data)
{
  GtkWidget *window = GTK_WIDGET (data);
  GtkWidget *label;
  GtkWidget *widget;
  GtkWidget *image;
  GtkWidget *box;

  if (button && gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button)))
    {
      header = gtk_event_box_new ();
      gtk_style_context_add_class (gtk_widget_get_style_context (header), "titlebar");
      gtk_style_context_add_class (gtk_widget_get_style_context (header), "header-bar");
      box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      g_object_set (box, "margin", 10, NULL);
      gtk_container_add (GTK_CONTAINER (header), box);
      label = gtk_label_new ("Label");
      gtk_box_pack_start (GTK_BOX (box), label, FALSE, TRUE, 0);
      widget = gtk_level_bar_new ();
      gtk_level_bar_set_value (GTK_LEVEL_BAR (widget), 0.4);
      gtk_box_pack_start (GTK_BOX (box), widget, TRUE, TRUE, 0);
      gtk_widget_show_all (header);
    }
  else
    {
      header = gtk_header_bar_new ();
      gtk_style_context_add_class (gtk_widget_get_style_context (header), "titlebar");
      gtk_header_bar_set_title (GTK_HEADER_BAR (header), "Example header");

      widget = gtk_button_new_with_label ("_Close");
      gtk_button_set_use_underline (GTK_BUTTON (widget), TRUE);
      gtk_style_context_add_class (gtk_widget_get_style_context (widget), "suggested-action");
      g_signal_connect (widget, "clicked", G_CALLBACK (gtk_main_quit), NULL);

      gtk_header_bar_pack_end (GTK_HEADER_BAR (header), widget);

      widget= gtk_button_new ();
      image = gtk_image_new_from_icon_name ("bookmark-new-symbolic", GTK_ICON_SIZE_BUTTON);
      g_signal_connect (widget, "clicked", G_CALLBACK (on_bookmark_clicked), window);
      gtk_container_add (GTK_CONTAINER (widget), image);

      gtk_header_bar_pack_start (GTK_HEADER_BAR (header), widget);
      gtk_widget_show_all (header);
    }

  gtk_window_set_titlebar (GTK_WINDOW (window), header);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *box;
  GtkWidget *footer;
  GtkWidget *button;
  GtkWidget *content;
  GtkCssProvider *provider;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_style_context_add_class (gtk_widget_get_style_context (window), "main");

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1, NULL);
  gtk_style_context_add_provider_for_screen (gtk_widget_get_screen (window),
                                             GTK_STYLE_PROVIDER (provider),
                                             GTK_STYLE_PROVIDER_PRIORITY_USER);


  change_header (NULL, window);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  footer = gtk_action_bar_new ();
  gtk_action_bar_set_center_widget (GTK_ACTION_BAR (footer), gtk_check_button_new_with_label ("Middle"));
  button = gtk_toggle_button_new_with_label ("Custom");
  g_signal_connect (button, "clicked", G_CALLBACK (change_header), window);
  gtk_action_bar_pack_start (GTK_ACTION_BAR (footer), button);
  button = gtk_button_new_with_label ("Subtitle");
  g_signal_connect (button, "clicked", G_CALLBACK (change_subtitle), NULL);
  gtk_action_bar_pack_end (GTK_ACTION_BAR (footer), button);
  button = gtk_button_new_with_label ("Fullscreen");
  gtk_action_bar_pack_end (GTK_ACTION_BAR (footer), button);
  g_signal_connect (button, "clicked", G_CALLBACK (toggle_fullscreen), window);
  gtk_box_pack_end (GTK_BOX (box), footer, FALSE, FALSE, 0);

  content = gtk_image_new_from_icon_name ("start-here-symbolic", GTK_ICON_SIZE_DIALOG);
  gtk_image_set_pixel_size (GTK_IMAGE (content), 512);

  gtk_box_pack_start (GTK_BOX (box), content, FALSE, TRUE, 0);

  gtk_widget_show_all (window);

  gtk_main ();

  gtk_widget_destroy (window);

  return 0;
}
