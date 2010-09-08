#include <gtk/gtk.h>

static gboolean
da_draw (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   user_data)
{
  GtkOffscreenWindow *offscreen = (GtkOffscreenWindow *)user_data;

  cairo_set_source_surface (cr,
                            gtk_offscreen_window_get_surface (offscreen),
                            50, 50);
  cairo_paint (cr);

  return FALSE;
}

static gboolean
offscreen_damage (GtkWidget      *widget,
                  GdkEventExpose *event,
                  GtkWidget      *da)
{
  gtk_widget_queue_draw (da);

  return TRUE;
}

static gboolean
da_button_press (GtkWidget *area, GdkEventButton *event, GtkWidget *button)
{
  gtk_widget_set_size_request (button, 150, 60);
  return TRUE;
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *button;
  GtkWidget *offscreen;
  GtkWidget *da;

  gtk_init (&argc, &argv);

  offscreen = gtk_offscreen_window_new ();

  button = gtk_button_new_with_label ("Test");
  gtk_widget_set_size_request (button, 50, 50);
  gtk_container_add (GTK_CONTAINER (offscreen), button);
  gtk_widget_show (button);

  gtk_widget_show (offscreen);

  /* Queue exposures and ensure they are handled so
   * that the result is uptodate for the first
   * expose of the window. If you want to get further
   * changes, also track damage on the offscreen
   * as done above.
   */
  gtk_widget_queue_draw (offscreen);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (window, "delete-event",
                    G_CALLBACK (gtk_main_quit), window);
  da = gtk_drawing_area_new ();
  gtk_container_add (GTK_CONTAINER (window), da);

  g_signal_connect (da,
                    "draw",
                    G_CALLBACK (da_draw),
                    offscreen);

  g_signal_connect (offscreen,
                    "damage-event",
                    G_CALLBACK (offscreen_damage),
                    da);

  gtk_widget_add_events (da, GDK_BUTTON_PRESS_MASK);
  g_signal_connect (da, "button_press_event", G_CALLBACK (da_button_press),
                    button);

  gtk_widget_show_all (window);

  gtk_main();

  return 0;
}
