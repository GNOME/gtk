#include <gtk/gtk.h>

static gboolean
draw_popup (GtkWidget *widget,
            cairo_t   *cr,
            gpointer   data)
{
  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_paint (cr);

  return FALSE;
}

static gboolean
place_popup (GtkWidget *parent,
             GdkEvent  *event,
             GtkWidget *popup)
{
  GdkEventMotion *ev_motion = (GdkEventMotion *) event;
  gint width, height;

  gtk_window_get_size (GTK_WINDOW (popup), &width, &height);
  gtk_window_move (GTK_WINDOW (popup),
                   (int) ev_motion->x_root - width / 2,
                   (int) ev_motion->y_root - height / 2);

  return FALSE;
}

static gboolean
on_map_event (GtkWidget *parent,
              GdkEvent  *event,
              gpointer   data)
{
  GtkWidget *popup;

  popup = gtk_window_new (GTK_WINDOW_POPUP);

  gtk_widget_set_size_request (GTK_WIDGET (popup), 20, 20);
  gtk_widget_set_app_paintable (GTK_WIDGET (popup), TRUE);
  gtk_window_set_transient_for (GTK_WINDOW (popup), GTK_WINDOW (parent));
  g_signal_connect (popup, "draw", G_CALLBACK (draw_popup), NULL);
  g_signal_connect (parent, "motion-notify-event", G_CALLBACK (place_popup), popup);

  gtk_widget_show (popup);

  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;

  gtk_init (&argc, &argv);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  gtk_widget_set_events (window, GDK_POINTER_MOTION_MASK);
  g_signal_connect (window, "destroy", gtk_main_quit, NULL);
  g_signal_connect (window, "map-event", G_CALLBACK (on_map_event), NULL);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
