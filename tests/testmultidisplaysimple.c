#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkinternals.h>

void
hello (GtkWidget * widget)
{
  GdkScreen *scr = gtk_widget_get_screen (widget);
  GdkDisplay *dpy = gdk_screen_get_display (scr);
  g_print ("Click from %s\n", gdk_display_get_name (dpy));
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *win2;
  GtkWidget *button, *butt2;
  GdkDisplay *dpy2;
  GdkScreen *scr2;

  gtk_init (&argc, &argv);
  /* Crude test insert 2nd machine name here */
  dpy2 = gdk_display_init_new (0, NULL, "diabolo:0.0");
  if(!dpy2)
  {
    printf ("impossible to open display aborting\n");
    exit(0);
  }
    
  scr2 = gdk_display_get_default_screen (dpy2);

  win2 = g_object_connect (gtk_widget_new (gtk_window_get_type (),
					   "screen", scr2,
					   "user_data", NULL,
					   "type", GTK_WINDOW_TOPLEVEL,
					   "title", "hello world",
					   "allow_grow", FALSE,
					   "allow_shrink", FALSE,
					   "border_width", 10,
					   NULL),
			   "signal::destroy", gtk_main_quit, NULL, NULL);
  butt2 = g_object_connect (gtk_widget_new (gtk_button_get_type (),
					    "GtkButton::label", "hello world",
					    "GtkWidget::parent", win2,
					    "GtkWidget::visible", TRUE,
					    NULL),
			    "signal::clicked", hello, win2, NULL);
  window = g_object_connect (gtk_widget_new (gtk_window_get_type (),
					     "user_data", NULL,
					     "type", GTK_WINDOW_TOPLEVEL,
					     "title", "hello world",
					     "allow_grow", FALSE,
					     "allow_shrink", FALSE,
					     "border_width", 10,
					     NULL),
			     "signal::destroy", gtk_main_quit, NULL, NULL);
  button = g_object_connect (gtk_widget_new (gtk_button_get_type (),
					     "GtkButton::label",
					     "hello world",
					     "GtkWidget::parent", window,
					     "GtkWidget::visible", TRUE,
					     NULL), "signal::clicked", hello,
			     window, NULL);

  gtk_widget_show (window);
  gtk_widget_show (win2);

  gtk_main ();

  return 0;
}
