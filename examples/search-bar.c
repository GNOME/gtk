#include <gtk/gtk.h>

static gboolean
window_key_press_event_cb (GtkWidget *window,
    GdkEvent *event,
    GtkSearchBar *search_bar)
{
  return gtk_search_bar_handle_event (search_bar, event);
}

static void
activate_cb (GtkApplication *app,
    gpointer user_data)
{
  GtkWidget *window;
  GtkWidget *search_bar;
  GtkWidget *box;
  GtkWidget *entry;
  GtkWidget *menu_button;

  window = gtk_application_window_new (app);
  gtk_widget_show (window);

  search_bar = gtk_search_bar_new ();
  gtk_container_add (GTK_CONTAINER (window), search_bar);
  gtk_widget_show (search_bar);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (search_bar), box);
  gtk_widget_show (box);

  entry = gtk_search_entry_new ();
  gtk_box_pack_start (GTK_BOX (box), entry, TRUE, TRUE, 0);
  gtk_widget_show (entry);

  menu_button = gtk_menu_button_new ();
  gtk_box_pack_start (GTK_BOX (box), menu_button, FALSE, FALSE, 0);
  gtk_widget_show (menu_button);

  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (search_bar), GTK_ENTRY (entry));

  g_signal_connect (window, "key-press-event",
      G_CALLBACK (window_key_press_event_cb), search_bar);
}

gint
main (gint argc,
    gchar *argv[])
{
  GtkApplication *app;

  app = gtk_application_new ("org.gtk.Example.GtkSearchBar",
      G_APPLICATION_FLAGS_NONE);
  g_signal_connect (app, "activate",
      G_CALLBACK (activate_cb), NULL);

  return g_application_run (G_APPLICATION (app), argc, argv);
}
