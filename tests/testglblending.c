#include <stdlib.h>
#include <gtk/gtk.h>

#include "gtkgears.h"

int
main (int argc, char *argv[])
{
  GtkWidget *window, *fixed, *gears, *spinner;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "Test GL/gtk inter-blending");
  gtk_window_set_default_size (GTK_WINDOW (window), 250, 250);
  gtk_container_set_border_width (GTK_CONTAINER (window), 12);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);

  fixed = gtk_fixed_new ();
  gtk_container_add (GTK_CONTAINER (window), fixed);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 90, 80);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 100, 80);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 110, 80);


  gears = gtk_gears_new ();
  gtk_widget_set_size_request (gears, 70, 50);
  gtk_fixed_put (GTK_FIXED (fixed), gears, 60, 100);

  gears = gtk_gears_new ();
  gtk_gl_area_set_has_alpha (GTK_GL_AREA (gears), TRUE);
  gtk_widget_set_size_request (gears, 70, 50);
  gtk_fixed_put (GTK_FIXED (fixed), gears, 120, 100);


  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 90, 110);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 100, 110);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 110, 110);


  gears = gtk_gears_new ();
  gtk_widget_set_size_request (gears, 70, 50);
  gtk_fixed_put (GTK_FIXED (fixed), gears, 60, 130);

  gears = gtk_gears_new ();
  gtk_gl_area_set_has_alpha (GTK_GL_AREA (gears), TRUE);
  gtk_widget_set_size_request (gears, 70, 50);
  gtk_fixed_put (GTK_FIXED (fixed), gears, 120, 130);


  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 90, 150);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 100, 150);

  spinner = gtk_spinner_new ();
  gtk_spinner_start (GTK_SPINNER (spinner));
  gtk_widget_set_size_request (spinner, 50, 50);
  gtk_fixed_put (GTK_FIXED (fixed), spinner, 110, 150);

  gtk_widget_show_all (window);

  gtk_main ();

  return EXIT_SUCCESS;
}
