#include <gtk/gtk.h>

static void
spawn_side_window (GtkWidget *button)
{
  GtkWidget *window, *headerbar, *checkbox;

  window = gtk_application_window_new (GTK_APPLICATION (g_application_get_default ()));
  gtk_window_set_title (GTK_WINDOW (window), "Side window");

  headerbar = gtk_header_bar_new ();
  gtk_header_bar_set_use_native_controls (GTK_HEADER_BAR (headerbar), TRUE);
  gtk_window_set_titlebar (GTK_WINDOW (window), headerbar);

  checkbox = gtk_check_button_new_with_label ("Click me to do things");
  gtk_window_set_child (GTK_WINDOW (window), checkbox);

  gtk_window_present (GTK_WINDOW (window));
}

static void
activated (GtkApplication *app)
{
  GtkWidget *window, *headerbar, *button;

  window = gtk_application_window_new (app);
  gtk_window_set_title (GTK_WINDOW (window), "Top window");
  gtk_window_set_default_size (GTK_WINDOW (window), 480, 480);

  headerbar = gtk_header_bar_new ();
  gtk_header_bar_set_use_native_controls (GTK_HEADER_BAR (headerbar), TRUE);
  gtk_window_set_titlebar (GTK_WINDOW (window), headerbar);

  button = gtk_button_new_with_label ("Spawn another window");
  gtk_window_set_child (GTK_WINDOW (window), button);
  g_signal_connect (button, "clicked", G_CALLBACK (spawn_side_window), NULL);

  gtk_window_present (GTK_WINDOW (window));
}

int
main (int argc, char **argv)
{
  GtkApplication *app;
  int status;

  app = gtk_application_new ("com.example.App", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect (app, "activate", G_CALLBACK (activated), NULL);

  status = g_application_run (G_APPLICATION (app), argc, argv);
  g_object_unref (app);
  return status;
}

