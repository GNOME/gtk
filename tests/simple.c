#include <config.h>
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

  /* FIXME: This is not allowable - what is this supposed to be? */
  /*  gdk_progclass = g_strdup ("XTerm"); */
  gtk_init (&argc, &argv);
  
  window = g_object_connect (gtk_widget_new (gtk_window_get_type (),
					     "user_data", NULL,
					     "type", GTK_WINDOW_TOPLEVEL,
					     "title", "hello world",
					     "allow_grow", FALSE,
					     "allow_shrink", FALSE,
					     "border_width", 10,
					     NULL),
			     "signal::destroy", gtk_main_quit, NULL,
			     NULL);
  button = g_object_connect (gtk_widget_new (gtk_button_get_type (),
					     "GtkButton::label", "hello world",
					     "GtkWidget::parent", window,
					     "GtkWidget::visible", TRUE,
					     NULL),
			     "signal::clicked", hello, NULL,
			     NULL);
  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
