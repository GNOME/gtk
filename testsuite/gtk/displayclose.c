#include <gtk/gtk.h>

int
main (int argc, char **argv)
{
  const gchar *display_name;
  GdkDisplay *display;
  GtkWidget *win, *but;

  g_log_set_always_fatal (G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL);

  if (!gtk_parse_args (&argc, &argv))
    return 1;

  display_name = gdk_get_display_arg_name();
  display = gdk_display_open(display_name);

  if (!display)
    return 1;

  gdk_display_manager_set_default_display (gdk_display_manager_get (), display);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (win, "destroy",
		    G_CALLBACK (gtk_main_quit), NULL);
  g_signal_connect (win, "delete-event",
                    G_CALLBACK (gtk_widget_destroy), NULL);

  but = gtk_button_new_with_label ("Try to Exit");
  g_signal_connect_swapped (but, "clicked",
			    G_CALLBACK (gtk_widget_destroy), win);
  gtk_container_add (GTK_CONTAINER (win), but);

  gtk_widget_show_all (win);

  gtk_test_widget_wait_for_draw (win);

  gdk_display_close (display);

  return 0;
}
