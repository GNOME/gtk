/* Event Axes
 *
 * Demonstrates advanced handling of event information from exotic
 * input devices.
 *
 * On one hand, this snippet demonstrates management of input axes,
 * those contain additional information for the pointer other than
 * X/Y coordinates.
 *
 * Input axes are dependent on hardware devices, on linux/unix you
 * can see the device axes through xinput list <device>. Each time
 * a different hardware device is used to move the pointer, the
 * master device will be updated to match the axes it provides,
 * these changes can be tracked through GdkDevice::changed, or
 * checking gdk_event_get_source_device().
 *
 * On the other hand, this demo handles basic multitouch events,
 * each event coming from an specific touchpoint will contain a
 * GdkEventSequence that's unique for its lifetime, so multiple
 * touchpoints can be tracked.
 */

#include <gtk/gtk.h>

typedef struct {
  GdkDevice *last_source;
  GHashTable *axes; /* axis label atom -> value */
  GdkRGBA color;
  gdouble x;
  gdouble y;
} AxesInfo;

typedef struct {
  AxesInfo *pointer_info;
  GHashTable *touch_info; /* GdkEventSequence -> AxesInfo */
} EventData;

const gchar *colors[] = {
  "black",
  "orchid",
  "fuchsia",
  "indigo",
  "thistle",
  "sienna",
  "azure",
  "plum",
  "lime",
  "navy",
  "maroon",
  "burlywood"
};

static guint cur_color = 0;

static AxesInfo *
axes_info_new (void)
{
  AxesInfo *info;

  info = g_new0 (AxesInfo, 1);
  gdk_rgba_parse (&info->color, colors[cur_color]);
  info->axes = g_hash_table_new_full (NULL, NULL, NULL,
                                      (GDestroyNotify) g_free);

  cur_color = (cur_color + 1) % G_N_ELEMENTS (colors);

  return info;
}

static void
axes_info_free (AxesInfo *info)
{
  g_hash_table_destroy (info->axes);
  g_free (info);
}

static gboolean
axes_info_lookup (AxesInfo    *info,
                  const gchar *axis_label,
                  gdouble     *value)
{
  gdouble *val;
  GdkAtom atom;

  atom = gdk_atom_intern (axis_label, FALSE);

  if (atom == GDK_NONE)
    return FALSE;

  val = g_hash_table_lookup (info->axes, GDK_ATOM_TO_POINTER (atom));

  if (!val)
    return FALSE;

  *value = *val;
  return TRUE;
}

static EventData *
event_data_new (void)
{
  EventData *data;

  data = g_new0 (EventData, 1);
  data->touch_info = g_hash_table_new_full (NULL, NULL, NULL,
                                            (GDestroyNotify) axes_info_free);

  return data;
}

static void
event_data_free (EventData *data)
{
  if (data->pointer_info)
    axes_info_free (data->pointer_info);
  g_hash_table_destroy (data->touch_info);
  g_free (data);
}

static void
update_axes_from_event (GdkEvent  *event,
                        EventData *data)
{
  GdkDevice *device, *source_device;
  GdkEventSequence *sequence;
  gdouble x, y, value;
  GList *l, *axes;
  AxesInfo *info;

  device = gdk_event_get_device (event);
  source_device = gdk_event_get_source_device (event);
  sequence = gdk_event_get_event_sequence (event);

  if (event->type == GDK_TOUCH_END)
    {
      g_hash_table_remove (data->touch_info, sequence);
      return;
    }
  else if (event->type == GDK_LEAVE_NOTIFY)
    {
      if (data->pointer_info)
        axes_info_free (data->pointer_info);
      data->pointer_info = NULL;
      return;
    }

  if (!sequence)
    {
      if (!data->pointer_info)
        data->pointer_info = axes_info_new ();
      info = data->pointer_info;
    }
  else
    {
      info = g_hash_table_lookup (data->touch_info, sequence);

      if (!info)
        {
          info = axes_info_new ();
          g_hash_table_insert (data->touch_info, sequence, info);
        }
    }

  if (info->last_source != source_device)
    {
      g_hash_table_remove_all (info->axes);
      info->last_source = source_device;
    }

  if (event->type == GDK_TOUCH_BEGIN ||
      event->type == GDK_TOUCH_UPDATE ||
      event->type == GDK_MOTION_NOTIFY ||
      event->type == GDK_BUTTON_PRESS ||
      event->type == GDK_BUTTON_RELEASE)
    {
      axes = gdk_device_list_axes (device);

      if (sequence && event->touch.emulating_pointer)
        {
          if (data->pointer_info)
            axes_info_free (data->pointer_info);
          data->pointer_info = NULL;
        }

      for (l = axes; l; l = l->next)
        {
          gdouble *ptr;

          /* All those event types are compatible wrt axes position in the struct */
          if (!gdk_device_get_axis_value (device, event->motion.axes,
                                          l->data, &value))
            continue;

          ptr = g_new0 (gdouble, 1);
          *ptr = value;
          g_hash_table_insert (info->axes, GDK_ATOM_TO_POINTER (l->data), ptr);
        }

      g_list_free (axes);
    }

  if (gdk_event_get_coords (event, &x, &y))
    {
      info->x = x;
      info->y = y;
    }
}

static gboolean
event_cb (GtkWidget *widget,
          GdkEvent  *event,
          gpointer   user_data)
{
  update_axes_from_event (event, user_data);
  gtk_widget_queue_draw (widget);
  return FALSE;
}

static void
render_arrow (cairo_t     *cr,
              gdouble      x_diff,
              gdouble      y_diff,
              const gchar *label)
{
  cairo_save (cr);

  cairo_set_source_rgb (cr, 0, 0, 0);
  cairo_new_path (cr);
  cairo_move_to (cr, 0, 0);
  cairo_line_to (cr, x_diff, y_diff);
  cairo_stroke (cr);

  cairo_move_to (cr, x_diff, y_diff);
  cairo_show_text (cr, label);

  cairo_restore (cr);
}

static void
draw_axes_info (cairo_t       *cr,
                AxesInfo      *info,
                GtkAllocation *allocation)
{
  gdouble pressure, tilt_x, tilt_y, wheel;

  cairo_save (cr);

  cairo_set_line_width (cr, 1);
  gdk_cairo_set_source_rgba (cr, &info->color);

  cairo_move_to (cr, 0, info->y);
  cairo_line_to (cr, allocation->width, info->y);
  cairo_move_to (cr, info->x, 0);
  cairo_line_to (cr, info->x, allocation->height);
  cairo_stroke (cr);

  cairo_translate (cr, info->x, info->y);

  if (axes_info_lookup (info, "Abs Pressure", &pressure))
    {
      cairo_pattern_t *pattern;

      pattern = cairo_pattern_create_radial (0, 0, 0, 0, 0, 100);
      cairo_pattern_add_color_stop_rgba (pattern, pressure, 1, 0, 0, pressure);
      cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 1, 0);

      cairo_set_source (cr, pattern);

      cairo_arc (cr, 0, 0, 100, 0, 2 * G_PI);
      cairo_fill (cr);

      cairo_pattern_destroy (pattern);
    }

  if (axes_info_lookup (info, "Abs Tilt X", &tilt_x) &&
      axes_info_lookup (info, "Abs Tilt Y", &tilt_y))
    render_arrow (cr, tilt_x * 100, tilt_y * 100, "Tilt");

  if (axes_info_lookup (info, "Abs Wheel", &wheel))
    {
      cairo_save (cr);
      cairo_set_line_width (cr, 10);
      cairo_set_source_rgba (cr, 0, 0, 0, 0.5);

      cairo_new_sub_path (cr);
      cairo_arc (cr, 0, 0, 100, 0, wheel * 2 * G_PI);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

  cairo_restore (cr);
}

static void
draw_device_info (GtkWidget        *widget,
                  cairo_t          *cr,
                  GdkEventSequence *sequence,
                  gint             *y,
                  AxesInfo         *info)
{
  PangoLayout *layout;
  GString *string;
  gint height;

  cairo_save (cr);

  string = g_string_new (NULL);
  g_string_append_printf (string, "Source: %s",
                          gdk_device_get_name (info->last_source));

  if (sequence)
    g_string_append_printf (string, "\nSequence: %d",
                            GPOINTER_TO_UINT (sequence));

  cairo_move_to (cr, 10, *y);
  layout = gtk_widget_create_pango_layout (widget, string->str);
  pango_cairo_show_layout (cr, layout);
  cairo_stroke (cr);

  pango_layout_get_pixel_size (layout, NULL, &height);

  gdk_cairo_set_source_rgba (cr, &info->color);
  cairo_set_line_width (cr, 10);
  cairo_move_to (cr, 0, *y);

  *y = *y + height;
  cairo_line_to (cr, 0, *y);
  cairo_stroke (cr);

  cairo_restore (cr);

  g_object_unref (layout);
  g_string_free (string, TRUE);
}

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   user_data)
{
  EventData *data = user_data;
  GtkAllocation allocation;
  AxesInfo *touch_info;
  GHashTableIter iter;
  gpointer key, value;
  gint y = 0;

  gtk_widget_get_allocation (widget, &allocation);

  /* Draw Abs info */
  if (data->pointer_info)
    draw_axes_info (cr, data->pointer_info, &allocation);

  g_hash_table_iter_init (&iter, data->touch_info);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      touch_info = value;
      draw_axes_info (cr, touch_info, &allocation);
    }

  /* Draw name, color legend and misc data */
  if (data->pointer_info)
    draw_device_info (widget, cr, NULL, &y, data->pointer_info);

  g_hash_table_iter_init (&iter, data->touch_info);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      touch_info = value;
      draw_device_info (widget, cr, key, &y, touch_info);
    }

  return FALSE;
}

GtkWidget *
do_event_axes (GtkWidget *toplevel)
{
  static GtkWidget *window = NULL;
  EventData *event_data;
  GtkWidget *box;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 400);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      box = gtk_event_box_new ();
      gtk_container_add (GTK_CONTAINER (window), box);
      gtk_widget_add_events (box,
			     GDK_POINTER_MOTION_MASK |
			     GDK_BUTTON_PRESS_MASK |
			     GDK_BUTTON_RELEASE_MASK |
			     GDK_SMOOTH_SCROLL_MASK |
			     GDK_TOUCH_MASK);

      event_data = event_data_new ();
      g_object_set_data_full (G_OBJECT (box), "gtk-demo-event-data",
                              event_data, (GDestroyNotify) event_data_free);

      g_signal_connect (box, "event",
                        G_CALLBACK (event_cb), event_data);
      g_signal_connect (box, "draw",
                        G_CALLBACK (draw_cb), event_data);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
