/* Paint
 *
 * Demonstrates practical handling of drawing tablets in a real world
 * usecase.
 */
#include <gtk/gtk.h>

typedef struct
{
  GtkEventBox parent_instance;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkRGBA draw_color;

  GtkGesture *stylus_gesture;
} DrawingArea;

typedef struct
{
  GtkEventBoxClass parent_class;
} DrawingAreaClass;

G_DEFINE_TYPE (DrawingArea, drawing_area, GTK_TYPE_EVENT_BOX)

static void
drawing_area_ensure_surface (DrawingArea *area,
                             gint         width,
                             gint         height)
{
  if (!area->surface ||
      cairo_image_surface_get_width (area->surface) != width ||
      cairo_image_surface_get_height (area->surface) != height)
    {
      cairo_surface_t *surface;

      surface = cairo_image_surface_create (CAIRO_FORMAT_ARGB32,
                                            width, height);
      if (area->surface)
        {
          cairo_t *cr;

          cr = cairo_create (surface);
          cairo_set_source_surface (cr, area->surface, 0, 0);
          cairo_paint (cr);

          cairo_surface_destroy (area->surface);
          cairo_destroy (area->cr);
          cairo_destroy (cr);
        }

      area->surface = surface;
      area->cr = cairo_create (surface);
    }
}

static void
drawing_area_size_allocate (GtkWidget     *widget,
                            GtkAllocation *allocation)
{
  DrawingArea *area = (DrawingArea *) widget;

  drawing_area_ensure_surface (area, allocation->width, allocation->height);

  GTK_WIDGET_CLASS (drawing_area_parent_class)->size_allocate (widget, allocation);
}

static void
drawing_area_map (GtkWidget *widget)
{
  GtkAllocation allocation;

  GTK_WIDGET_CLASS (drawing_area_parent_class)->map (widget);

  gdk_window_set_event_compression (gtk_widget_get_window (widget), TRUE);

  gtk_widget_get_allocation (widget, &allocation);
  drawing_area_ensure_surface ((DrawingArea *) widget,
                               allocation.width, allocation.height);
}

static void
drawing_area_unmap (GtkWidget *widget)
{
  DrawingArea *area = (DrawingArea *) widget;

  g_clear_pointer (&area->cr, cairo_destroy);
  g_clear_pointer (&area->surface, cairo_surface_destroy);

  GTK_WIDGET_CLASS (drawing_area_parent_class)->unmap (widget);
}

static gboolean
drawing_area_draw (GtkWidget *widget,
		   cairo_t   *cr)
{
  DrawingArea *area = (DrawingArea *) widget;
  GtkAllocation allocation;

  gtk_widget_get_allocation (widget, &allocation);

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  cairo_set_source_surface (cr, area->surface, 0, 0);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0.6, 0.6, 0.6);
  cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
  cairo_stroke (cr);

  return TRUE;
}

static void
drawing_area_class_init (DrawingAreaClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->size_allocate = drawing_area_size_allocate;
  widget_class->draw = drawing_area_draw;
  widget_class->map = drawing_area_map;
  widget_class->unmap = drawing_area_unmap;
}

static void
drawing_area_apply_stroke (DrawingArea   *area,
                           GdkDeviceTool *tool,
                           gdouble        x,
                           gdouble        y,
                           gdouble        pressure)
{
  if (gdk_device_tool_get_tool_type (tool) == GDK_DEVICE_TOOL_TYPE_ERASER)
    {
      cairo_set_line_width (area->cr, 10 * pressure);
      cairo_set_operator (area->cr, CAIRO_OPERATOR_DEST_OUT);
    }
  else
    {
      cairo_set_line_width (area->cr, 4 * pressure);
      cairo_set_operator (area->cr, CAIRO_OPERATOR_SATURATE);
    }

  cairo_set_source_rgba (area->cr, area->draw_color.red,
                         area->draw_color.green, area->draw_color.blue,
                         area->draw_color.alpha * pressure);

  //cairo_set_source_rgba (area->cr, 0, 0, 0, pressure);

  cairo_line_to (area->cr, x, y);
  cairo_stroke (area->cr);
  cairo_move_to (area->cr, x, y);
}

static void
stylus_gesture_down (GtkGestureStylus *gesture,
                     gdouble           x,
                     gdouble           y,
                     DrawingArea      *area)
{
  cairo_new_path (area->cr);
}

static void
stylus_gesture_motion (GtkGestureStylus *gesture,
                       gdouble           x,
                       gdouble           y,
                       DrawingArea      *area)
{
  GdkDeviceTool *tool;
  gdouble pressure;

  tool = gtk_gesture_stylus_get_device_tool (gesture);

  if (!gtk_gesture_stylus_get_axis (gesture, GDK_AXIS_PRESSURE, &pressure))
    pressure = 1;

  drawing_area_apply_stroke (area, tool, x, y, pressure);
  gtk_widget_queue_draw (GTK_WIDGET (area));
}

static void
drawing_area_init (DrawingArea *area)
{
  const GdkRGBA draw_rgba = { 0, 0, 0, 1 };
  gtk_event_box_set_visible_window (GTK_EVENT_BOX (area), TRUE);

  area->stylus_gesture = gtk_gesture_stylus_new (GTK_WIDGET (area));
  g_signal_connect (area->stylus_gesture, "down",
                    G_CALLBACK (stylus_gesture_down), area);
  g_signal_connect (area->stylus_gesture, "motion",
                    G_CALLBACK (stylus_gesture_motion), area);

  area->draw_color = draw_rgba;
}

GtkWidget *
drawing_area_new (void)
{
  return g_object_new (drawing_area_get_type (), NULL);
}

void
drawing_area_set_color (DrawingArea *area,
                        GdkRGBA     *color)
{
  area->draw_color = *color;
}

static void
color_button_color_set (GtkColorButton *button,
                        DrawingArea    *draw_area)
{
  GdkRGBA color;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &color);
  drawing_area_set_color (draw_area, &color);
}

GtkWidget *
do_paint (GtkWidget *toplevel)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *draw_area, *headerbar, *colorbutton;
      const GdkRGBA draw_rgba = { 0, 0, 0, 1 };

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      draw_area = drawing_area_new ();
      gtk_container_add (GTK_CONTAINER (window), draw_area);

      headerbar = gtk_header_bar_new ();
      gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar), "Paint");
      gtk_header_bar_set_show_close_button (GTK_HEADER_BAR (headerbar), TRUE);

      colorbutton = gtk_color_button_new ();
      g_signal_connect (colorbutton, "color-set",
                        G_CALLBACK (color_button_color_set), draw_area);
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colorbutton),
                                  &draw_rgba);

      gtk_header_bar_pack_end (GTK_HEADER_BAR (headerbar), colorbutton);
      gtk_window_set_titlebar (GTK_WINDOW (window), headerbar);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}
