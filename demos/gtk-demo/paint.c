/* Paint
 *
 * Demonstrates practical handling of drawing tablets in a real world
 * usecase.
 */
#include <glib/gi18n.h>
#include <gtk/gtk.h>

enum {
  COLOR_SET,
  N_SIGNALS
};

static guint area_signals[N_SIGNALS] = { 0, };

typedef struct
{
  GtkWidget parent_instance;
  cairo_surface_t *surface;
  cairo_t *cr;
  GdkRGBA draw_color;
  GtkPadController *pad_controller;
  gdouble brush_size;
} DrawingArea;

typedef struct
{
  GtkWidgetClass parent_class;
} DrawingAreaClass;

static GtkPadActionEntry pad_actions[] = {
  { GTK_PAD_ACTION_BUTTON, 1, -1, N_("Black"), "pad.black" },
  { GTK_PAD_ACTION_BUTTON, 2, -1, N_("Pink"), "pad.pink" },
  { GTK_PAD_ACTION_BUTTON, 3, -1, N_("Green"), "pad.green" },
  { GTK_PAD_ACTION_BUTTON, 4, -1, N_("Red"), "pad.red" },
  { GTK_PAD_ACTION_BUTTON, 5, -1, N_("Purple"), "pad.purple" },
  { GTK_PAD_ACTION_BUTTON, 6, -1, N_("Orange"), "pad.orange" },
  { GTK_PAD_ACTION_STRIP, -1, -1, N_("Brush size"), "pad.brush_size" },
};

static const gchar *pad_colors[] = {
  "black",
  "pink",
  "green",
  "red",
  "purple",
  "orange"
};

G_DEFINE_TYPE (DrawingArea, drawing_area, GTK_TYPE_WIDGET)

static void drawing_area_set_color (DrawingArea *area,
                                    GdkRGBA     *color);

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
drawing_area_size_allocate (GtkWidget *widget,
                            int        width,
                            int        height,
                            int        baseline)
{
  DrawingArea *area = (DrawingArea *) widget;

  drawing_area_ensure_surface (area, width, height);

  GTK_WIDGET_CLASS (drawing_area_parent_class)->size_allocate (widget, width, height, baseline);
}

static void
drawing_area_map (GtkWidget *widget)
{
  GtkAllocation allocation;

  GTK_WIDGET_CLASS (drawing_area_parent_class)->map (widget);

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

static void
drawing_area_snapshot (GtkWidget   *widget,
		       GtkSnapshot *snapshot)
{
  DrawingArea *area = (DrawingArea *) widget;
  GtkAllocation allocation;
  cairo_t *cr;

  gtk_widget_get_allocation (widget, &allocation);
  cr = gtk_snapshot_append_cairo (snapshot,
                                  &GRAPHENE_RECT_INIT (
                                    0, 0,
				    allocation.width,
				    allocation.height
                                  ));

  cairo_set_source_rgb (cr, 1, 1, 1);
  cairo_paint (cr);

  cairo_set_source_surface (cr, area->surface, 0, 0);
  cairo_paint (cr);

  cairo_set_source_rgb (cr, 0.6, 0.6, 0.6);
  cairo_rectangle (cr, 0, 0, allocation.width, allocation.height);
  cairo_stroke (cr);

  cairo_destroy (cr);
}

static void
on_pad_button_activate (GSimpleAction *action,
                        GVariant      *parameter,
                        DrawingArea   *area)
{
  const gchar *color = g_object_get_data (G_OBJECT (action), "color");
  GdkRGBA rgba;

  gdk_rgba_parse (&rgba, color);
  drawing_area_set_color (area, &rgba);
}

static void
on_pad_knob_change (GSimpleAction *action,
                    GVariant      *parameter,
                    DrawingArea   *area)
{
  gdouble value = g_variant_get_double (parameter);

  area->brush_size = value;
}

static void
drawing_area_unroot (GtkWidget *widget)
{
  DrawingArea *area = (DrawingArea *) widget;
  GtkWidget *toplevel;

  toplevel = GTK_WIDGET (gtk_widget_get_root (widget));

  if (area->pad_controller)
    {
      gtk_widget_remove_controller (toplevel, GTK_EVENT_CONTROLLER (area->pad_controller));
      area->pad_controller = NULL;
    }

  GTK_WIDGET_CLASS (drawing_area_parent_class)->unroot (widget);
}

static void
drawing_area_root (GtkWidget *widget)
{
  DrawingArea *area = (DrawingArea *) widget;
  GSimpleActionGroup *action_group;
  GSimpleAction *action;
  GtkWidget *toplevel;
  gint i;

  GTK_WIDGET_CLASS (drawing_area_parent_class)->root (widget);

  toplevel = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (area)));

  action_group = g_simple_action_group_new ();
  area->pad_controller = gtk_pad_controller_new (G_ACTION_GROUP (action_group), NULL);

  for (i = 0; i < G_N_ELEMENTS (pad_actions); i++)
    {
      if (pad_actions[i].type == GTK_PAD_ACTION_BUTTON)
        {
          action = g_simple_action_new (pad_actions[i].action_name, NULL);
          g_object_set_data (G_OBJECT (action), "color",
                             (gpointer) pad_colors[i]);
          g_signal_connect (action, "activate",
                            G_CALLBACK (on_pad_button_activate), area);
        }
      else
        {
          action = g_simple_action_new_stateful (pad_actions[i].action_name,
                                                 G_VARIANT_TYPE_DOUBLE, NULL);
          g_signal_connect (action, "activate",
                            G_CALLBACK (on_pad_knob_change), area);
        }

      g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (action));
      g_object_unref (action);
    }

  gtk_pad_controller_set_action_entries (area->pad_controller, pad_actions,
                                         G_N_ELEMENTS (pad_actions));

  gtk_widget_add_controller (toplevel, GTK_EVENT_CONTROLLER (area->pad_controller));
}

static void
drawing_area_class_init (DrawingAreaClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  widget_class->size_allocate = drawing_area_size_allocate;
  widget_class->snapshot = drawing_area_snapshot;
  widget_class->map = drawing_area_map;
  widget_class->unmap = drawing_area_unmap;
  widget_class->root = drawing_area_root;
  widget_class->unroot = drawing_area_unroot;

  area_signals[COLOR_SET] =
    g_signal_new ("color-set",
                  G_TYPE_FROM_CLASS (widget_class),
                  G_SIGNAL_RUN_FIRST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 1, GDK_TYPE_RGBA);
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
      cairo_set_line_width (area->cr, 10 * pressure * area->brush_size);
      cairo_set_operator (area->cr, CAIRO_OPERATOR_DEST_OUT);
    }
  else
    {
      cairo_set_line_width (area->cr, 4 * pressure * area->brush_size);
      cairo_set_operator (area->cr, CAIRO_OPERATOR_SATURATE);
    }

  cairo_set_source_rgba (area->cr, area->draw_color.red,
                         area->draw_color.green, area->draw_color.blue,
                         area->draw_color.alpha * pressure);

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
  GdkTimeCoord *backlog;
  GdkDeviceTool *tool;
  gdouble pressure;
  guint n_items;

  tool = gtk_gesture_stylus_get_device_tool (gesture);

  if (gtk_gesture_stylus_get_backlog (gesture, &backlog, &n_items))
    {
      guint i;

      for (i = 0; i < n_items; i++)
        {
          drawing_area_apply_stroke (area, tool,
                                     backlog[i].axes[GDK_AXIS_X],
                                     backlog[i].axes[GDK_AXIS_Y],
                                     backlog[i].axes[GDK_AXIS_PRESSURE]);
        }

      g_free (backlog);
    }
  else
    {
      if (!gtk_gesture_stylus_get_axis (gesture, GDK_AXIS_PRESSURE, &pressure))
        pressure = 1;

      drawing_area_apply_stroke (area, tool, x, y, pressure);
    }

  gtk_widget_queue_draw (GTK_WIDGET (area));
}

static void
drawing_area_init (DrawingArea *area)
{
  GtkGesture *gesture;

  gesture = gtk_gesture_stylus_new ();
  g_signal_connect (gesture, "down",
                    G_CALLBACK (stylus_gesture_down), area);
  g_signal_connect (gesture, "motion",
                    G_CALLBACK (stylus_gesture_motion), area);
  gtk_widget_add_controller (GTK_WIDGET (area), GTK_EVENT_CONTROLLER (gesture));

  area->draw_color = (GdkRGBA) { 0, 0, 0, 1 };
}

GtkWidget *
drawing_area_new (void)
{
  return g_object_new (drawing_area_get_type (), NULL);
}

static void
drawing_area_set_color (DrawingArea *area,
                        GdkRGBA     *color)
{
  if (gdk_rgba_equal (&area->draw_color, color))
    return;

  area->draw_color = *color;
  g_signal_emit (area, area_signals[COLOR_SET], 0, &area->draw_color);
}

static void
color_button_color_set (GtkColorButton *button,
                        DrawingArea    *draw_area)
{
  GdkRGBA color;

  gtk_color_chooser_get_rgba (GTK_COLOR_CHOOSER (button), &color);
  drawing_area_set_color (draw_area, &color);
}

static void
drawing_area_color_set (DrawingArea    *area,
                        GdkRGBA        *color,
                        GtkColorButton *button)
{
  gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (button), color);
}

GtkWidget *
do_paint (GtkWidget *toplevel)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *draw_area, *headerbar, *colorbutton;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

      draw_area = drawing_area_new ();
      gtk_container_add (GTK_CONTAINER (window), draw_area);

      headerbar = gtk_header_bar_new ();
      gtk_header_bar_set_title (GTK_HEADER_BAR (headerbar), "Paint");
      gtk_header_bar_set_show_title_buttons (GTK_HEADER_BAR (headerbar), TRUE);

      colorbutton = gtk_color_button_new ();
      g_signal_connect (colorbutton, "color-set",
                        G_CALLBACK (color_button_color_set), draw_area);
      g_signal_connect (draw_area, "color-set",
                        G_CALLBACK (drawing_area_color_set), colorbutton);
      gtk_color_chooser_set_rgba (GTK_COLOR_CHOOSER (colorbutton),
                                  &(GdkRGBA) { 0, 0, 0, 1 });

      gtk_header_bar_pack_end (GTK_HEADER_BAR (headerbar), colorbutton);
      gtk_window_set_titlebar (GTK_WINDOW (window), headerbar);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
