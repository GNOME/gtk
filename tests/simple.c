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

  window = gtk_widget_new (gtk_invisible_get_type (),
			   "GtkInvisible::screen", scr2,
			   NULL);
  if (scr2 != gtk_invisible_get_screen (GTK_INVISIBLE (window)))
    g_print ("Set property didn't work\n");
  else
    g_print ("Set property worked\n");

/*  gtk_widget_show (window);

  gtk_main ();*/

  return 0;
}
