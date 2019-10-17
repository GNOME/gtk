#include <gtk/gtk.h>

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
  gtk_widget_set_valign (search_bar, GTK_ALIGN_START);
  gtk_container_add (GTK_CONTAINER (window), search_bar);
  gtk_widget_show (search_bar);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (search_bar), box);

  entry = gtk_search_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_container_add (GTK_CONTAINER (box), entry);

  menu_button = gtk_menu_button_new ();
  gtk_container_add (GTK_CONTAINER (box), menu_button);

  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (search_bar), GTK_EDITABLE (entry));
  gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (search_bar), window);
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
