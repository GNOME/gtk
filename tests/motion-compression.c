#include <gtk/gtk.h>
#include <math.h>

GtkAdjustment *adjustment;
int cursor_x, cursor_y;

static void
motion_cb (GtkEventControllerMotion *motion,
           gdouble                   x,
           gdouble                   y,
           GtkWidget                *widget)
{
  float processing_ms = gtk_adjustment_get_value (adjustment);
  g_usleep (processing_ms * 1000);

  cursor_x = x;
  cursor_y = y;
  gtk_widget_queue_draw (widget);
}

static void
on_draw (GtkDrawingArea *da,
         cairo_t        *cr,
         int             width,
         int             height,
         gpointer        data)
{
  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0, 0.5, 0.5);

  cairo_arc (cr, cursor_x, cursor_y, 10, 0, 2 * M_PI);
  cairo_stroke (cr);
}

int
main (int argc, char **argv)
{
  GtkWidget *window;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *scale;
  GtkWidget *da;
  GtkEventController *controller;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 300, 300);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  da = gtk_drawing_area_new ();
  gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), on_draw, NULL, NULL);
  gtk_widget_set_vexpand (da, TRUE);
  gtk_container_add (GTK_CONTAINER (vbox), da);

  label = gtk_label_new ("Event processing time (ms):");
  gtk_widget_set_halign (label, GTK_ALIGN_CENTER);
  gtk_container_add (GTK_CONTAINER (vbox), label);

  adjustment = gtk_adjustment_new (20, 0, 200, 1, 10, 0);
  scale = gtk_scale_new (GTK_ORIENTATION_HORIZONTAL, adjustment);
  gtk_container_add (GTK_CONTAINER (vbox), scale);

  controller = gtk_event_controller_motion_new ();
  g_signal_connect (controller, "motion",
                    G_CALLBACK (motion_cb), da);
  gtk_widget_add_controller (da, controller);

  g_signal_connect (window, "destroy",
                    G_CALLBACK (gtk_main_quit), NULL);

  gtk_widget_show (window);
  gtk_main ();

  return 0;
}
