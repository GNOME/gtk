
#include <stdlib.h>
#include <gtk/gtk.h>
#include "tictactoe.h"

void win( GtkWidget *widget,
          gpointer   data )
{
  g_print ("Yay!\n");
  tictactoe_clear (TICTACTOE (widget));
}

int main( int   argc,
          char *argv[] )
{
  GtkWidget *window;
  GtkWidget *ttt;
  
  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  
  gtk_window_set_title (GTK_WINDOW (window), "Aspect Frame");
  
  g_signal_connect (G_OBJECT (window), "destroy",
		    G_CALLBACK (exit), NULL);
  
  gtk_container_set_border_width (GTK_CONTAINER (window), 10);

  ttt = tictactoe_new ();
  
  gtk_container_add (GTK_CONTAINER (window), ttt);
  gtk_widget_show (ttt);

  g_signal_connect (G_OBJECT (ttt), "tictactoe",
		    G_CALLBACK (win), NULL);

  gtk_widget_show (window);
  
  gtk_main ();
  
  return 0;
}

