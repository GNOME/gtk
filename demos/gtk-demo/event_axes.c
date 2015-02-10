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
  gdouble *axes;
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

  cur_color = (cur_color + 1) % G_N_ELEMENTS (colors);

  return info;
}

static void
axes_info_free (AxesInfo *info)
{
  g_free (info);
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
  GdkDevice *source_device;
  GdkEventSequence *sequence;
  gdouble x, y;
  AxesInfo *info;

  source_device = gdk_event_get_source_device (event);
  sequence = gdk_event_get_event_sequence (event);

  if (event->type == GDK_TOUCH_END ||
      event->type == GDK_TOUCH_CANCEL)
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
    info->last_source = source_device;

  if (event->type == GDK_TOUCH_BEGIN ||
      event->type == GDK_TOUCH_UPDATE ||
      event->type == GDK_MOTION_NOTIFY ||
      event->type == GDK_BUTTON_PRESS ||
      event->type == GDK_BUTTON_RELEASE)
    {
      if (sequence && event->touch.emulating_pointer)
        {
          if (data->pointer_info)
            axes_info_free (data->pointer_info);
          data->pointer_info = NULL;
        }

      if (info->axes)
        g_free (info->axes);

      info->axes =
        g_memdup (event->motion.axes,
                  sizeof(gdouble) * gdk_device_get_n_axes (source_device));
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
  gdouble pressure, tilt_x, tilt_y, distance, wheel;
  GdkAxisFlags axes = gdk_device_get_axes (info->last_source);

  cairo_save (cr);

  cairo_set_line_width (cr, 1);
  gdk_cairo_set_source_rgba (cr, &info->color);

  cairo_move_to (cr, 0, info->y);
  cairo_line_to (cr, allocation->width, info->y);
  cairo_move_to (cr, info->x, 0);
  cairo_line_to (cr, info->x, allocation->height);
  cairo_stroke (cr);

  cairo_translate (cr, info->x, info->y);

  if (axes & GDK_AXIS_FLAG_PRESSURE)
    {
      cairo_pattern_t *pattern;

      gdk_device_get_axis (info->last_source, info->axes, GDK_AXIS_PRESSURE,
                           &pressure);

      pattern = cairo_pattern_create_radial (0, 0, 0, 0, 0, 100);
      cairo_pattern_add_color_stop_rgba (pattern, pressure, 1, 0, 0, pressure);
      cairo_pattern_add_color_stop_rgba (pattern, 1, 0, 0, 1, 0);

      cairo_set_source (cr, pattern);

      cairo_arc (cr, 0, 0, 100, 0, 2 * G_PI);
      cairo_fill (cr);

      cairo_pattern_destroy (pattern);
    }

  if (axes & GDK_AXIS_FLAG_XTILT &&
      axes & GDK_AXIS_FLAG_YTILT)
    {
      gdk_device_get_axis (info->last_source, info->axes, GDK_AXIS_XTILT,
                           &tilt_x);
      gdk_device_get_axis (info->last_source, info->axes, GDK_AXIS_YTILT,
                           &tilt_y);

      render_arrow (cr, tilt_x * 100, tilt_y * 100, "Tilt");
    }

  if (axes & GDK_AXIS_FLAG_DISTANCE)
    {
      double dashes[] = { 5.0, 5.0 };
      cairo_text_extents_t extents;

      gdk_device_get_axis (info->last_source, info->axes, GDK_AXIS_DISTANCE,
                           &distance);

      cairo_save (cr);

      cairo_move_to (cr, distance * 100, 0);

      cairo_set_source_rgb (cr, 0.0, 0.0, 0.0);
      cairo_set_dash (cr, dashes, 2, 0.0);
      cairo_arc (cr, 0, 0, distance * 100, 0, 2 * G_PI);
      cairo_stroke (cr);

      cairo_move_to (cr, 0, -distance * 100);
      cairo_text_extents (cr, "Distance", &extents);
      cairo_rel_move_to (cr, -extents.width / 2, 0);
      cairo_show_text (cr, "Distance");

      cairo_move_to (cr, 0, 0);

      cairo_restore (cr);
    }

  if (axes & GDK_AXIS_FLAG_WHEEL)
    {
      gdk_device_get_axis (info->last_source, info->axes, GDK_AXIS_WHEEL,
                           &wheel);

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
      gtk_window_set_title (GTK_WINDOW (window), "Event Axes");
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
    gtk_widget_destroy (window);

  return window;
}
