#include <gtk/gtk.h>

GdkDisplay *dpy2;

void
hello (void)
{
  gdk_display_close (dpy2);
  exit (0);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *button;
  GdkScreen *scr2;

  gtk_init (&argc, &argv);
  
  dpy2 = gdk_display_init_new (0, NULL, "diabolo:0.0");
  if(!dpy2)
  {
    printf ("impossible to open display aborting\n");
    exit(0);
  }
  scr2 = gdk_display_get_default_screen (dpy2);

  g_print ("scr2 = %x\n",scr2);

  window = gtk_widget_new (gtk_window_get_type (),
			   "screen", scr2,
			   NULL);
/*  if (scr2 != gtk_invisible_get_screen (GTK_INVISIBLE (window)))
    g_print ("Set property didn't work\n");
  else
    g_print ("Set property worked\n");
*/
  button = gtk_button_new_with_label ("Ducon");
  gtk_widget_show (button);
  g_object_connect (button, "signal::clicked", hello, NULL, NULL);
  gtk_container_add (GTK_CONTAINER (window), button);
  gtk_widget_show (window);

  gtk_main ();


  return 0;
}
