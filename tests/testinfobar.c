#include <gtk/gtk.h>

static void
on_button_visible_toggled (GtkToggleButton *button,
                           void            *user_data)
{
  GtkInfoBar *info_bar = GTK_INFO_BAR (user_data);

  gtk_widget_set_visible (GTK_WIDGET (info_bar),
                          gtk_toggle_button_get_active (button));
}

static void
on_button_revealed_toggled (GtkToggleButton *button,
                            void            *user_data)
{
  GtkInfoBar *info_bar = GTK_INFO_BAR (user_data);

  gtk_info_bar_set_revealed (info_bar,
                             gtk_toggle_button_get_active (button));
}

static void
on_activate (GApplication *application,
             void         *user_data)
{
  GtkWidget *window;
  GtkWidget *box;

  GtkWidget *info_bar;
  GtkWidget *content_area;
  GtkWidget *label;

  GtkWidget *button_visible;
  GtkWidget *button_revealed;

  info_bar = gtk_info_bar_new ();
  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (info_bar));
  label = gtk_label_new ("Hello!\n\nI am a GtkInfoBar");
  gtk_container_add (GTK_CONTAINER (content_area), label);

  button_visible = gtk_toggle_button_new_with_label ("Toggle ::visible");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_visible), TRUE);
  g_signal_connect (button_visible, "toggled",
                    G_CALLBACK (on_button_visible_toggled), info_bar);

  button_revealed = gtk_toggle_button_new_with_label ("Toggle ::revealed");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button_revealed), TRUE);
  g_signal_connect (button_revealed, "toggled",
                    G_CALLBACK (on_button_revealed_toggled), info_bar);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_add (GTK_CONTAINER (box), button_visible);
  gtk_container_add (GTK_CONTAINER (box), button_revealed);
  gtk_container_add (GTK_CONTAINER (box), info_bar);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_add (GTK_CONTAINER (window), box);
  gtk_widget_show_all (window);
  gtk_application_add_window (GTK_APPLICATION (application),
                              GTK_WINDOW (window));
}

int
main (int   argc,
      char *argv[])
{
  GtkApplication *application;
  int result;

  application = gtk_application_new ("org.gtk.test.infobar",
                                     G_APPLICATION_FLAGS_NONE);
  g_signal_connect (application, "activate", G_CALLBACK (on_activate), NULL);

  result = g_application_run (G_APPLICATION (application), argc, argv);
  g_object_unref (application);
  return result;
}
