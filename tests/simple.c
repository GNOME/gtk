#include <gtk/gtk.h>


void
hello (void)
{
  g_print ("hello world\n");
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *button;
  GdkDisplay *dpy2;

  gtk_init (&argc, &argv);
  
  dpy2 = gdk_display_init_new (0, NULL, "diabolo:0.0");
  if(!dpy2)
  {
    printf ("impossible to open display aborting\n");
    exit(0);
  }

  window = gtk_color_selection_dialog_new("Toto");
  gtk_window_set_screen (GTK_WINDOW (window),  gdk_display_get_default_screen (dpy2));
	
	  
  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
