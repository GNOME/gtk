#include <gtk/gtk.h>

static void
start_resize (GtkGestureClick *gesture,
              int n_press,
              double x,
              double y,
              gpointer   data)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  GdkSurfaceEdge edge = GPOINTER_TO_INT (data);
  GdkSurface *surface;
  GdkEvent *event;
  guint button;
  guint32 timestamp;
  int xx = x;
  int yy = y;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  surface = gtk_native_get_surface (gtk_widget_get_native (widget));
  event = gtk_get_current_event ();
  gdk_event_get_button (event, &button);  
  timestamp = gdk_event_get_time (event);

  gtk_widget_translate_coordinates (widget, GTK_WIDGET (gtk_widget_get_root (widget)),
                                    xx, yy, &xx, &yy);
  gdk_surface_begin_resize_drag (surface, edge, button, xx, yy, timestamp);

  gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static GtkWidget *
resize_button (GdkSurfaceEdge edge)
{
  GtkWidget *button;
  GtkGesture *gesture;

  button = gtk_image_new_from_icon_name ("view-fullscreen-symbolic");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_vexpand (button, TRUE);
  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (start_resize), GINT_TO_POINTER (edge));
  gtk_widget_add_controller (button, GTK_EVENT_CONTROLLER (gesture));

  return button;
}

static void
start_move (GtkGestureClick *gesture,
            int n_press,
            double x,
            double y,
            gpointer   data)
{
  GtkWidget *widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  GdkSurface *surface;
  GdkEvent *event;
  guint button;
  guint32 timestamp;
  int xx = x;
  int yy = y;

  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);

  surface = gtk_native_get_surface (gtk_widget_get_native (widget));
  event = gtk_get_current_event ();
  gdk_event_get_button (event, &button);  
  timestamp = gdk_event_get_time (event);

  gtk_widget_translate_coordinates (widget, GTK_WIDGET (gtk_widget_get_root (widget)),
                                    xx, yy, &xx, &yy);
  gdk_surface_begin_move_drag (surface, button, xx, yy, timestamp);
  gtk_event_controller_reset (GTK_EVENT_CONTROLLER (gesture));
}

static GtkWidget *
move_button (void)
{
  GtkWidget *button;
  GtkGesture *gesture;

  button = gtk_image_new_from_icon_name ("view-grid-symbolic");
  gtk_widget_set_hexpand (button, TRUE);
  gtk_widget_set_vexpand (button, TRUE);
  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "pressed", G_CALLBACK (start_move), NULL);
  gtk_widget_add_controller (button, GTK_EVENT_CONTROLLER (gesture));

  return button;
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *grid;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_titlebar (GTK_WINDOW (window), gtk_header_bar_new ());

  grid = gtk_grid_new ();
  gtk_container_add (GTK_CONTAINER (window), grid);

  gtk_grid_attach (GTK_GRID (grid),
                   resize_button (GDK_SURFACE_EDGE_NORTH_WEST), 
                   0, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   resize_button (GDK_SURFACE_EDGE_NORTH), 
                   1, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   resize_button (GDK_SURFACE_EDGE_NORTH_EAST), 
                   2, 0, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   resize_button (GDK_SURFACE_EDGE_WEST), 
                   0, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   move_button (),
                   1, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   resize_button (GDK_SURFACE_EDGE_EAST), 
                   2, 1, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   resize_button (GDK_SURFACE_EDGE_SOUTH_WEST), 
                   0, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   resize_button (GDK_SURFACE_EDGE_SOUTH), 
                   1, 2, 1, 1);
  gtk_grid_attach (GTK_GRID (grid),
                   resize_button (GDK_SURFACE_EDGE_SOUTH_EAST), 
                   2, 2, 1, 1);

  gtk_widget_show (window);

  gtk_main ();

  return 0;
}
