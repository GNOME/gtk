#include <gtk/gtk.h>
#include <gdk/gdkprivate.h>


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

  gdk_progclass = g_strdup ("XTerm");
  gtk_init (&argc, &argv);

  window = gtk_widget_new (gtk_window_get_type (),
			   "GtkObject::user_data", NULL,
			   "GtkWindow::type", GTK_WINDOW_TOPLEVEL,
			   "GtkWindow::title", "hello world",
			   "GtkWindow::allow_grow", FALSE,
			   "GtkWindow::allow_shrink", FALSE,
			   "GtkContainer::border_width", 10,
			   NULL);
  button = gtk_widget_new (gtk_button_get_type (),
			   "GtkButton::label", "hello world",
			   "GtkObject::signal::clicked", hello, NULL,
			   "GtkWidget::parent", window,
			   "GtkWidget::visible", TRUE,
			   NULL);
  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
