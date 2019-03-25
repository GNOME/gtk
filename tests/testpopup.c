#include <gtk/gtk.h>

static void
draw_popup (GtkDrawingArea  *da,
            cairo_t         *cr,
            int              width,
            int              height,
            gpointer         data)
{
  cairo_set_source_rgb (cr, 1, 0, 0);
  cairo_paint (cr);
}

static void
place_popup (GtkEventControllerMotion *motion,
             gdouble                   x,
             gdouble                   y,
             GtkWidget                *popup)
{
}

static gboolean
on_map (GtkWidget *parent)
{
  GtkWidget *popup, *da;
  GtkEventController *motion;

  popup = gtk_window_new (GTK_WINDOW_POPUP);
  da = gtk_drawing_area_new ();
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), draw_popup, NULL, NULL);
  gtk_container_add (GTK_CONTAINER (popup), da);

  gtk_widget_set_size_request (GTK_WIDGET (popup), 20, 20);
  gtk_window_set_transient_for (GTK_WINDOW (popup), GTK_WINDOW (parent));

  motion = gtk_event_controller_motion_new ();
  gtk_widget_add_controller (parent, motion);
  g_signal_connect (motion, "motion", G_CALLBACK (place_popup), popup);

  gtk_widget_show (popup);

  return FALSE;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  g_signal_connect (window, "destroy", gtk_main_quit, NULL);
  g_signal_connect (window, "map", G_CALLBACK (on_map), NULL);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
