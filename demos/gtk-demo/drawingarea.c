/* Drawing Area
 *
 * GtkDrawingArea is a blank area where you can draw custom displays
 * of various kinds.
 *
 * This demo has two drawing areas. The checkerboard area shows
 * how you can just draw something; all you have to do is set a function
 * via gtk_drawing_area_set_draw_func(), as shown here.
 *
 * The "scribble" area is a bit more advanced, and shows how to handle
 * events such as button presses and mouse motion. Click the mouse
 * and drag in the scribble area to draw squiggles. Resize the window
 * to clear the area.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;
/* Pixmap for scribble area, to store current scribbles */
static cairo_surface_t *surface = NULL;

/* Create a new surface of the appropriate size to store our scribbles */
static void
create_surface (GtkWidget *widget)
{
  cairo_t *cr;

  if (surface)
    cairo_surface_destroy (surface);

  surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                        gtk_widget_get_width (widget),
                                        gtk_widget_get_height (widget));

  /* Initialize the surface to white */
  cr = cairo_create (surface);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  cairo_destroy (cr);
}

static void
scribble_size_allocate (GtkWidget *widget)
{
  create_surface (widget);
}

/* Redraw the screen from the surface */
static void
scribble_draw (GtkDrawingArea *da,
               cairo_t        *cr,
               int             width,
               int             height,
               gpointer        data)
{
  cairo_set_source_surface (cr, surface, 0, 0);
  cairo_paint (cr);
}

/* Draw a rectangle on the screen */
static void
draw_brush (GtkWidget *widget,
            gdouble    x,
            gdouble    y)
{
  GdkRectangle update_rect;
  cairo_t *cr;

  if (surface == NULL ||
      cairo_image_surface_get_width (surface) != gtk_widget_get_width (widget) ||
      cairo_image_surface_get_height (surface) != gtk_widget_get_height (widget))
    create_surface (widget);

  update_rect.x = x - 3;
  update_rect.y = y - 3;
  update_rect.width = 6;
  update_rect.height = 6;

  /* Paint to the surface, where we store our state */
  cr = cairo_create (surface);

  gdk_cairo_rectangle (cr, &update_rect);
  cairo_fill (cr);

  cairo_destroy (cr);

  gtk_widget_queue_draw (widget);
}

static double start_x;
static double start_y;

static void
drag_begin (GtkGestureDrag *gesture,
            double          x,
            double          y,
            GtkWidget      *area)
{
  start_x = x;
  start_y = y;

  draw_brush (area, x, y);
}

static void
drag_update (GtkGestureDrag *gesture,
             double          x,
             double          y,
             GtkWidget      *area)
{
  draw_brush (area, start_x + x, start_y + y);
}

static void
drag_end (GtkGestureDrag *gesture,
          double          x,
          double          y,
          GtkWidget      *area)
{
  draw_brush (area, start_x + x, start_y + y);
}

static void
checkerboard_draw (GtkDrawingArea *da,
                   cairo_t        *cr,
                   int             width,
                   int             height,
                   gpointer        data)
{
  gint i, j, xcount, ycount;

#define CHECK_SIZE 10
#define SPACING 2

  /* At the start of a draw handler, a clip region has been set on
   * the Cairo context, and the contents have been cleared to the
   * widget's background color. The docs for
   * gdk_surface_begin_paint_region() give more details on how this
   * works.
   */

  xcount = 0;
  i = SPACING;
  while (i < width)
    {
      j = SPACING;
      ycount = xcount % 2; /* start with even/odd depending on row */
      while (j < height)
        {
          if (ycount % 2)
            cairo_set_source_rgb (cr, 0.45777, 0, 0.45777);
          else
            cairo_set_source_rgb (cr, 1, 1, 1);

          /* If we're outside the clip, this will do nothing.
           */
          cairo_rectangle (cr, i, j, CHECK_SIZE, CHECK_SIZE);
          cairo_fill (cr);

          j += CHECK_SIZE + SPACING;
          ++ycount;
        }

      i += CHECK_SIZE + SPACING;
      ++xcount;
    }
}

static void
close_window (void)
{
  window = NULL;

  if (surface)
    cairo_surface_destroy (surface);
  surface = NULL;
}

GtkWidget *
do_drawingarea (GtkWidget *do_widget)
{
  GtkWidget *frame;
  GtkWidget *vbox;
  GtkWidget *da;
  GtkWidget *label;
  GtkGesture *drag;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Drawing Area");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (close_window), NULL);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      gtk_widget_set_margin_start (vbox, 16);
      gtk_widget_set_margin_end (vbox, 16);
      gtk_widget_set_margin_top (vbox, 16);
      gtk_widget_set_margin_bottom (vbox, 16);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      /*
       * Create the checkerboard area
       */

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "<u>Checkerboard pattern</u>");
      gtk_container_add (GTK_CONTAINER (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_widget_set_vexpand (frame, TRUE);
      gtk_container_add (GTK_CONTAINER (vbox), frame);

      da = gtk_drawing_area_new ();
      gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (da), 100);
      gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), 100);
      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), checkerboard_draw, NULL, NULL);
      gtk_container_add (GTK_CONTAINER (frame), da);

      /*
       * Create the scribble area
       */

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "<u>Scribble area</u>");
      gtk_container_add (GTK_CONTAINER (vbox), label);

      frame = gtk_frame_new (NULL);
      gtk_widget_set_vexpand (frame, TRUE);
      gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
      gtk_container_add (GTK_CONTAINER (vbox), frame);

      da = gtk_drawing_area_new ();
      gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (da), 100);
      gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), 100);
      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), scribble_draw, NULL, NULL);
      gtk_container_add (GTK_CONTAINER (frame), da);

      g_signal_connect (da, "size-allocate",
                        G_CALLBACK (scribble_size_allocate), NULL);

      drag = gtk_gesture_drag_new ();
      gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (drag), GDK_BUTTON_PRIMARY);
      gtk_widget_add_controller (da, GTK_EVENT_CONTROLLER (drag));

      g_signal_connect (drag, "drag-begin", G_CALLBACK (drag_begin), da);
      g_signal_connect (drag, "drag-update", G_CALLBACK (drag_update), da);
      g_signal_connect (drag, "drag-end", G_CALLBACK (drag_end), da);

    }

  if (!gtk_widget_get_visible (window))
      gtk_widget_show (window);
  else
      gtk_widget_destroy (window);

  return window;
}
