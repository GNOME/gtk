#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (DemoSlider, demo_slider, DEMO, SLIDER, GtkWidget)

struct _DemoSlider
{
  GtkWidget parent_instance;
};

struct _DemoSliderClass
{
  GtkWidgetClass parent_class;
};

G_DECLARE_FINAL_TYPE (DemoHighlight, demo_highlight, DEMO, HIGHLIGHT, GtkWidget)

struct _DemoHighlight
{
  GtkWidget parent_instance;
};

struct _DemoHighlightClass
{
  GtkWidgetClass parent_class;
};

G_DECLARE_FINAL_TYPE (DemoTrough, demo_trough, DEMO, TROUGH, GtkWidget)

struct _DemoTrough
{
  GtkWidget parent_instance;
};

struct _DemoTroughClass
{
  GtkWidgetClass parent_class;
};

G_DECLARE_FINAL_TYPE (DemoWidget, demo_widget, DEMO, WIDGET, GtkWidget)

struct _DemoWidget
{
  GtkWidget parent_instance;

  GtkWidget *trough;
  GtkWidget *highlight;
  GtkWidget *min_slider;
  GtkWidget *max_slider;

  GtkWidget *grab_location;

  double range_min;
  double range_max;
  double min_value;
  double max_value;

  gboolean shift;
};

enum
{
  PROP_RANGE_MIN = 1,
  PROP_RANGE_MAX,
  PROP_MIN_VALUE,
  PROP_MAX_VALUE,
  NUM_PROPERTIES
};

static GParamSpec *properties[NUM_PROPERTIES] = { NULL };

struct _DemoWidgetClass
{
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (DemoSlider, demo_slider, GTK_TYPE_WIDGET)

static void
demo_slider_init (DemoSlider *slider)
{
}

static void
demo_slider_class_init (DemoSliderClass *class)
{
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "slider");
}

static GtkWidget *
demo_slider_new (void)
{
  return g_object_new (demo_slider_get_type (), NULL);
}

G_DEFINE_TYPE (DemoHighlight, demo_highlight, GTK_TYPE_WIDGET)

static void
demo_highlight_init (DemoHighlight *highlight)
{
}

static void
demo_highlight_class_init (DemoHighlightClass *class)
{
  gtk_widget_class_set_css_name (GTK_WIDGET_CLASS (class), "highlight");
}

static GtkWidget *
demo_highlight_new (void)
{
  return g_object_new (demo_highlight_get_type (), NULL);
}

G_DEFINE_TYPE (DemoTrough, demo_trough, GTK_TYPE_WIDGET)

static void
demo_trough_init (DemoTrough *trough)
{
}

static void
demo_trough_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  DemoWidget *demo = DEMO_WIDGET (gtk_widget_get_parent (widget));
  int min1, nat1;
  int min2, nat2;
  int min3, nat3;

  gtk_widget_measure (demo->min_slider,
                      orientation, -1,
                      &min1, &nat1,
                      NULL, NULL);

  gtk_widget_measure (demo->max_slider,
                      orientation, -1,
                      &min2, &nat2,
                      NULL, NULL);

  gtk_widget_measure (demo->highlight,
                      orientation, for_size,
                      &min3, &nat3,
                      NULL, NULL);

  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      *minimum = MAX (min1 + min2, min3);
      *natural = MAX (nat1 + nat2, nat3);
    }
  else
    {
      *minimum = MAX (MAX (min1, min2), min3);
      *natural = MAX (MAX (nat1, nat2), min3);
    }
}

static void
allocate_slider (DemoWidget *demo,
                 GtkWidget  *slider,
                 int         x)
{
  int trough_height;
  int width, height;
  int y;

  gtk_widget_measure (slider,
                      GTK_ORIENTATION_HORIZONTAL, -1,
                      &width, NULL,
                      NULL, NULL);
  gtk_widget_measure (slider,
                      GTK_ORIENTATION_VERTICAL, -1,
                      &height, NULL,
                      NULL, NULL);

  trough_height = gtk_widget_get_height (demo->trough);

  y = floor ((trough_height - height) / 2);

  gtk_widget_size_allocate (slider,
                            &(GtkAllocation) { x, y, width, height},
                            -1);
}

static void
demo_trough_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  DemoWidget *demo = DEMO_WIDGET (gtk_widget_get_parent (widget));
  int min, max;

  min = floor (width * (demo->min_value - demo->range_min) / (demo->range_max - demo->range_min));
  max = floor (width * (demo->max_value - demo->range_min) / (demo->range_max - demo->range_min));

  allocate_slider (demo, demo->min_slider, min);
  allocate_slider (demo, demo->max_slider, max);

  gtk_widget_size_allocate (demo->highlight,
                            &(GtkAllocation) { min, 0, max - min, height},
                            -1);
}

static void
demo_trough_class_init (DemoTroughClass *class)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  widget_class->measure = demo_trough_measure;
  widget_class->size_allocate = demo_trough_size_allocate;

  gtk_widget_class_set_css_name (widget_class, "trough");
}

static GtkWidget *
demo_trough_new (void)
{
  return g_object_new (demo_trough_get_type (), NULL);
}

G_DEFINE_TYPE (DemoWidget, demo_widget, GTK_TYPE_WIDGET)

static void
click_gesture_pressed (GtkGestureClick *gesture,
                       guint            n_press,
                       double           x,
                       double           y,
                       DemoWidget      *demo)
{
  guint button;
  GdkModifierType state;

  demo->grab_location = gtk_widget_pick (GTK_WIDGET (demo), x, y, 0);

  button = gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));
  state = gtk_event_controller_get_current_event_state (GTK_EVENT_CONTROLLER (gesture));

  demo->shift = (button == GDK_BUTTON_PRIMARY && ((state & GDK_SHIFT_MASK) != 0)) ||
                 button == GDK_BUTTON_SECONDARY;
}

static void
click_gesture_released (GtkGestureClick *gesture,
                        guint            n_press,
                        double           x,
                        double           y,
                        DemoWidget      *demo)
{
  demo->grab_location = NULL;
}

static void
drag_gesture_begin (GtkGestureDrag *gesture,
                    double          offset_x,
                    double          offset_y,
                    DemoWidget     *demo)
{
  if (demo->grab_location == demo->min_slider ||
      demo->grab_location == demo->max_slider)
    gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
drag_gesture_update (GtkGestureDrag *gesture,
                     double          offset_x,
                     double          offset_y,
                     DemoWidget     *demo)
{
  double start_x, start_y;
  int width;
  double value;
  double size;

  gtk_gesture_drag_get_start_point (gesture, &start_x, &start_y);

  width = gtk_widget_get_width (GTK_WIDGET (demo));

  value = ((start_x + offset_x) / width) * (demo->range_max - demo->range_min) + demo->range_min;
  value = CLAMP (value, demo->range_min, demo->range_max);
  size = demo->max_value - demo->min_value;

  if (demo->shift)
    {
      if (demo->grab_location == demo->min_slider)
        {
          demo->max_value = MIN (demo->range_max, value + size);
          demo->min_value = demo->max_value - size;
        }
      else if (demo->grab_location == demo->max_slider)
        {
          demo->min_value = MAX (demo->range_min, value - size);
          demo->max_value = demo->min_value + size;
        }
    }
  else
    {
      if (demo->grab_location == demo->min_slider)
        {
          demo->min_value = value;
          demo->max_value = MAX (demo->max_value, value);
        }
      else if (demo->grab_location == demo->max_slider)
        {
          demo->min_value = MIN (demo->min_value, value);
          demo->max_value = value;
        }
    }

  g_object_notify_by_pspec (G_OBJECT (demo), properties[PROP_MIN_VALUE]);
  g_object_notify_by_pspec (G_OBJECT (demo), properties[PROP_MAX_VALUE]);

  gtk_widget_queue_allocate (GTK_WIDGET (demo));
}

static void
demo_widget_init (DemoWidget *demo)
{
  GtkGesture *click_gesture, *drag_gesture;

  demo->range_min = 0;
  demo->range_max = 0;
  demo->min_value = 0;
  demo->max_value = 0;

  demo->trough = demo_trough_new ();
  gtk_widget_set_parent (demo->trough, GTK_WIDGET (demo));
  demo->highlight = demo_highlight_new ();
  gtk_widget_set_parent (demo->highlight, demo->trough);
  demo->min_slider = demo_slider_new ();
  gtk_widget_set_parent (demo->min_slider, demo->trough);
  demo->max_slider = demo_slider_new ();
  gtk_widget_set_parent (demo->max_slider, demo->trough);

  drag_gesture = gtk_gesture_drag_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (drag_gesture), 0);
  g_signal_connect (drag_gesture, "drag-begin", G_CALLBACK (drag_gesture_begin), demo);
  g_signal_connect (drag_gesture, "drag-update", G_CALLBACK (drag_gesture_update), demo);
  gtk_widget_add_controller (GTK_WIDGET (demo), GTK_EVENT_CONTROLLER (drag_gesture));

  click_gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click_gesture), 0);
  g_signal_connect (click_gesture, "pressed", G_CALLBACK (click_gesture_pressed), demo);
  g_signal_connect (click_gesture, "released", G_CALLBACK (click_gesture_released), demo);
  gtk_widget_add_controller (GTK_WIDGET (demo), GTK_EVENT_CONTROLLER (click_gesture));

  gtk_gesture_group (click_gesture, drag_gesture);
}

static void
demo_widget_measure (GtkWidget      *widget,
                     GtkOrientation  orientation,
                     int             for_size,
                     int            *minimum,
                     int            *natural,
                     int            *minimum_baseline,
                     int            *natural_baseline)
{
  DemoWidget *demo = DEMO_WIDGET (widget);

  gtk_widget_measure (demo->trough,
                      orientation, -1,
                      minimum, natural,
                      NULL, NULL);
}

static void
demo_widget_size_allocate (GtkWidget *widget,
                           int        width,
                           int        height,
                           int        baseline)
{
  DemoWidget *demo = DEMO_WIDGET (widget);
  int min_height;

  gtk_widget_measure (demo->trough,
                      GTK_ORIENTATION_VERTICAL, -1,
                      &min_height, NULL,
                      NULL, NULL);

  gtk_widget_size_allocate (demo->trough,
                            &(GtkAllocation) { 0, 0, width, min_height},
                            -1);
}

static void
demo_widget_dispose (GObject *object)
{
  DemoWidget *demo = DEMO_WIDGET (object);

  g_clear_pointer (&demo->trough, gtk_widget_unparent);

  G_OBJECT_CLASS (demo_widget_parent_class)->dispose (object);
}

static void
demo_widget_set_range (DemoWidget *demo,
                       double      range_min,
                       double      range_max)
{
  double value;

  g_return_if_fail (range_min <= range_max);

  g_object_freeze_notify (G_OBJECT (demo));

  if (demo->range_min != range_min)
    {
      demo->range_min = range_min;
      g_object_notify_by_pspec (G_OBJECT (demo), properties[PROP_RANGE_MIN]);
    }

  if (demo->range_max != range_max)
    {
      demo->range_max = range_max;
      g_object_notify_by_pspec (G_OBJECT (demo), properties[PROP_RANGE_MAX]);
    }

  value = CLAMP (demo->min_value, range_min, range_max);
  if (demo->min_value != value)
    {
      demo->min_value = value;
      g_object_notify_by_pspec (G_OBJECT (demo), properties[PROP_MIN_VALUE]);
    }

  value = CLAMP (demo->max_value, range_min, range_max);
  if (demo->max_value != value)
    {
      demo->max_value = value;
      g_object_notify_by_pspec (G_OBJECT (demo), properties[PROP_MAX_VALUE]);
    }

  g_object_thaw_notify (G_OBJECT (demo));
}

static void
demo_widget_set_values (DemoWidget *demo,
                        double      min_value,
                        double      max_value)
{
  double value;

  g_return_if_fail (min_value <= max_value);

  g_object_freeze_notify (G_OBJECT (demo));

  value = CLAMP (min_value, demo->range_min, demo->range_max);
  if (demo->min_value != value)
    {
      demo->min_value = value;
      g_object_notify_by_pspec (G_OBJECT (demo), properties[PROP_MIN_VALUE]);
    }

  value = CLAMP (max_value, demo->range_min, demo->range_max);
  if (demo->max_value != value)
    {
      demo->max_value = value;
      g_object_notify_by_pspec (G_OBJECT (demo), properties[PROP_MAX_VALUE]);
    }

  g_object_thaw_notify (G_OBJECT (demo));
}

static void
demo_widget_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  DemoWidget *demo = DEMO_WIDGET (object);
  double v;

  switch (prop_id)
    {
    case PROP_RANGE_MIN:
      v = g_value_get_double (value);
      demo_widget_set_range (demo, v, MAX (v, demo->range_max));
      break;

    case PROP_RANGE_MAX:
      v = g_value_get_double (value);
      demo_widget_set_range (demo, MIN (v, demo->range_min), v);
      break;

    case PROP_MIN_VALUE:
      v = g_value_get_double (value);
      demo_widget_set_values (demo, v, MAX (v, demo->max_value));
      break;

    case PROP_MAX_VALUE:
      v = g_value_get_double (value);
      demo_widget_set_values (demo, MIN (v, demo->min_value), v);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
demo_widget_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  DemoWidget *demo = DEMO_WIDGET (object);

  switch (prop_id)
    {
    case PROP_RANGE_MIN:
      g_value_set_double (value, demo->range_min);
      break;

    case PROP_RANGE_MAX:
      g_value_set_double (value, demo->range_max);
      break;

    case PROP_MIN_VALUE:
      g_value_set_double (value, demo->min_value);
      break;

    case PROP_MAX_VALUE:
      g_value_set_double (value, demo->max_value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
demo_widget_class_init (DemoWidgetClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = demo_widget_dispose;
  object_class->set_property = demo_widget_set_property;
  object_class->get_property = demo_widget_get_property;

  widget_class->measure = demo_widget_measure;
  widget_class->size_allocate = demo_widget_size_allocate;

  properties[PROP_RANGE_MIN] = g_param_spec_double ("range-min",
                                                    "Range Min",
                                                    "Range Min",
                                                    -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                    G_PARAM_READWRITE);
  properties[PROP_RANGE_MAX] = g_param_spec_double ("range-max",
                                                    "Range Max",
                                                    "Range Max",
                                                    -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                    G_PARAM_READWRITE);
  properties[PROP_MIN_VALUE] = g_param_spec_double ("min-value",
                                                    "Min Value",
                                                    "Min Value",
                                                    -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                    G_PARAM_READWRITE);
  properties[PROP_MAX_VALUE] = g_param_spec_double ("max-value",
                                                    "Max Value",
                                                    "Max Value",
                                                    -G_MAXDOUBLE, G_MAXDOUBLE, 0,
                                                    G_PARAM_READWRITE);

  gtk_widget_class_set_css_name (widget_class, "scale");
}

static GtkWidget *
demo_widget_new (void)
{
  return g_object_new (demo_widget_get_type (), NULL);
}

static void
update_range_label (GObject    *object,
                    GParamSpec *pspec,
                    gpointer    data)
{
  DemoWidget *demo = DEMO_WIDGET (object);

  if (pspec->name == g_intern_static_string ("range-min") ||
      pspec->name == g_intern_static_string ("range-max"))
    {
      char *text;

      text = g_strdup_printf ("Allowed values: [%.1f, %.1f]\n", demo->range_min, demo->range_max);
      gtk_label_set_label (GTK_LABEL (data), text);
      g_free (text);
    }
}

static void
update_values_label (GObject    *object,
                     GParamSpec *pspec,
                     gpointer    data)
{
  DemoWidget *demo = DEMO_WIDGET (object);

  if (pspec->name == g_intern_static_string ("min-value") ||
      pspec->name == g_intern_static_string ("max-value"))
    {
      char *text;

      text = g_strdup_printf ("Selected range: [%.1f, %.1f]\n", demo->min_value, demo->max_value);
      gtk_label_set_label (GTK_LABEL (data), text);
      g_free (text);
    }
}

int
main (int argc, char *argv[])
{
  GtkWindow *window;
  GtkWidget *box;
  GtkWidget *demo;
  GtkWidget *label;

  gtk_init ();

  window = GTK_WINDOW (gtk_window_new ());
  gtk_window_set_title (window, "Pick a range");

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 10);
  gtk_window_set_child (window, box);

  demo = demo_widget_new ();
  gtk_widget_set_halign (demo, GTK_ALIGN_FILL);
  gtk_widget_set_valign (demo, GTK_ALIGN_CENTER);
  gtk_widget_set_hexpand (demo, TRUE);

  gtk_box_append (GTK_BOX (box), demo);

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  g_signal_connect (demo, "notify", G_CALLBACK (update_range_label), label);
  gtk_box_append (GTK_BOX (box), label);

  label = gtk_label_new ("");
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  g_signal_connect (demo, "notify", G_CALLBACK (update_values_label), label);
  gtk_box_append (GTK_BOX (box), label);

  demo_widget_set_range (DEMO_WIDGET (demo), 0, 1000);
  demo_widget_set_values (DEMO_WIDGET (demo), 100, 500);

  gtk_window_present (window);

  while (g_list_model_get_n_items (gtk_window_get_toplevels ()) > 0)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
