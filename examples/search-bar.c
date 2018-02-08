#include <gtk/gtk.h>

static gboolean
window_key_pressed (GtkEventController *controller,
                    guint               keyval,
                    guint               keycode,
                    GdkModifierType     state,
                    GtkSearchBar       *search_bar)
{
  return gtk_search_bar_handle_event (search_bar,
                                      gtk_get_current_event ());
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
  GtkEventController *controller;

  window = gtk_application_window_new (app);
  gtk_widget_show (window);

  search_bar = gtk_search_bar_new ();
  gtk_container_add (GTK_CONTAINER (window), search_bar);
  gtk_widget_show (search_bar);

  box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  gtk_container_add (GTK_CONTAINER (search_bar), box);

  entry = gtk_search_entry_new ();
  gtk_widget_set_hexpand (entry, TRUE);
  gtk_box_pack_start (GTK_BOX (box), entry);

  menu_button = gtk_menu_button_new ();
  gtk_box_pack_start (GTK_BOX (box), menu_button);

  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (search_bar), GTK_ENTRY (entry));

  controller = gtk_event_controller_key_new (window);
  g_object_set_data_full (G_OBJECT (window), "controller", controller, g_object_unref);
  g_signal_connect (controller, "key-pressed",
                    G_CALLBACK (window_key_pressed), search_bar);
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
