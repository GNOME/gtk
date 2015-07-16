#include <gtk/gtk.h>
#define GTK_COMPILATION
#include "gtk/gtkplacesviewprivate.h"

int
main (int argc, char *argv[])
{
  GtkWidget *win;
  GtkWidget *view;

  gtk_init (&argc, &argv);

  win = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (win), 400, 600);

  view = gtk_places_view_new ();

  gtk_container_add (GTK_CONTAINER (win), view);
  gtk_widget_show_all (win);

  g_signal_connect (win, "delete-event", G_CALLBACK (gtk_main_quit), win);

  gtk_main ();

  return 0;
}
