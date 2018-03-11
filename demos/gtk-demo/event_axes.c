/* Touch and Drawing Tablets
 *
 * Demonstrates advanced handling of event information from exotic
 * input devices.
 *
 * On one hand, this snippet demonstrates management of drawing tablets,
 * those contain additional information for the pointer other than
 * X/Y coordinates. Tablet pads events are mapped to actions, which
 * are both defined and interpreted by the application.
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

#include <glib/gi18n.h>
#include <gtk/gtk.h>

typedef struct {
  GdkDevice *last_source;
  GdkDeviceTool *last_tool;
  gdouble *axes;
  GdkRGBA color;
  gdouble x;
  gdouble y;
} AxesInfo;

typedef struct {
  GHashTable *pointer_info; /* GdkDevice -> AxesInfo */
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

static GtkPadActionEntry pad_actions[] = {
  { GTK_PAD_ACTION_BUTTON, 1, -1, N_("Nuclear strike"), "pad.nuke" },
  { GTK_PAD_ACTION_BUTTON, 2, -1, N_("Release siberian methane reserves"), "pad.heat" },
  { GTK_PAD_ACTION_BUTTON, 3, -1, N_("Release solar flare"), "pad.fry" },
  { GTK_PAD_ACTION_BUTTON, 4, -1, N_("De-stabilize Oort cloud"), "pad.fall" },
  { GTK_PAD_ACTION_BUTTON, 5, -1, N_("Ignite WR-104"), "pad.burst" },
  { GTK_PAD_ACTION_BUTTON, 6, -1, N_("Lart whoever asks about this button"), "pad.lart" },
  { GTK_PAD_ACTION_RING, -1, -1, N_("Earth axial tilt"), "pad.tilt" },
  { GTK_PAD_ACTION_STRIP, -1, -1, N_("Extent of weak nuclear force"), "pad.dissolve" },
};

static const gchar *pad_action_results[] = {
  "☢",
  "♨",
  "☼",
  "☄",
  "⚡",
  "💫",
  "◑",
  "⚛"
};

static guint cur_color = 0;
static guint pad_action_timeout_id = 0;

static AxesInfo *
axes_info_new (void)
{
  AxesInfo *info;

  info = g_new0 (AxesInfo, 1);
  gdk_rgba_parse (&info->color, colors[cur_color]);

  cur_color = (cur_color + 1) % G_N_ELEMENTS (colors);

  return info;
}

static EventData *
event_data_new (void)
{
  EventData *data;

  data = g_new0 (EventData, 1);
  data->pointer_info = g_hash_table_new_full (NULL, NULL, NULL,
                                              (GDestroyNotify) g_free);
  data->touch_info = g_hash_table_new_full (NULL, NULL, NULL,
                                            (GDestroyNotify) g_free);

  return data;
}

static void
event_data_free (EventData *data)
{
  g_hash_table_destroy (data->pointer_info);
  g_hash_table_destroy (data->touch_info);
  g_free (data);
}

static void
update_axes_from_event (GdkEvent  *event,
                        EventData *data)
{
  GdkDevice *device, *source_device;
  GdkEventSequence *sequence;
  GdkDeviceTool *tool;
  GdkEventType type;
  gdouble x, y;
  AxesInfo *info;

  device = gdk_event_get_device (event);
  source_device = gdk_event_get_source_device (event);
  sequence = gdk_event_get_event_sequence (event);
  tool = gdk_event_get_device_tool (event);
  type = gdk_event_get_event_type (event);

  if (type == GDK_TOUCH_END ||
      type == GDK_TOUCH_CANCEL)
    {
      g_hash_table_remove (data->touch_info, sequence);
      return;
    }
  else if (type == GDK_LEAVE_NOTIFY)
    {
      g_hash_table_remove (data->pointer_info, device);
      return;
    }

  if (!source_device)
    return;

  if (!sequence)
    {
      info = g_hash_table_lookup (data->pointer_info, device);

      if (!info)
        {
          info = axes_info_new ();
          g_hash_table_insert (data->pointer_info, device, info);
        }
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

  if (info->last_tool != tool)
    info->last_tool = tool;

  g_clear_pointer (&info->axes, g_free);

  if (type == GDK_TOUCH_BEGIN ||
      type == GDK_TOUCH_UPDATE)
    {
      gboolean emulating_pointer;

      gdk_event_get_touch_emulating_pointer (event, &emulating_pointer);
      if (sequence && emulating_pointer)
        g_hash_table_remove (data->pointer_info, device);
    }
  if (type == GDK_MOTION_NOTIFY ||
      type == GDK_BUTTON_PRESS ||
      type == GDK_BUTTON_RELEASE)
    {
      gdouble *axes;
      guint n_axes;

      gdk_event_get_axes (event, &axes, &n_axes);
      info->axes = g_memdup (axes, sizeof (double) * n_axes);
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
                int            width,
                int            height)
{
  gdouble pressure, tilt_x, tilt_y, distance, wheel, rotation, slider;
  GdkAxisFlags axes = gdk_device_get_axes (info->last_source);

  cairo_save (cr);

  cairo_set_line_width (cr, 1);
  gdk_cairo_set_source_rgba (cr, &info->color);

  cairo_move_to (cr, 0, info->y);
  cairo_line_to (cr, width, info->y);
  cairo_move_to (cr, info->x, 0);
  cairo_line_to (cr, info->x, height);
  cairo_stroke (cr);

  cairo_translate (cr, info->x, info->y);

  if (!info->axes)
    {
      cairo_restore (cr);
      return;
    }

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

  if (axes & GDK_AXIS_FLAG_ROTATION)
    {
      gdk_device_get_axis (info->last_source, info->axes, GDK_AXIS_ROTATION,
                           &rotation);
      rotation *= 2 * G_PI;

      cairo_save (cr);
      cairo_rotate (cr, - G_PI / 2);
      cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
      cairo_set_line_width (cr, 5);

      cairo_new_sub_path (cr);
      cairo_arc (cr, 0, 0, 100, 0, rotation);
      cairo_stroke (cr);
      cairo_restore (cr);
    }

  if (axes & GDK_AXIS_FLAG_SLIDER)
    {
      cairo_pattern_t *pattern, *mask;

      gdk_device_get_axis (info->last_source, info->axes, GDK_AXIS_SLIDER,
                           &slider);

      cairo_save (cr);

      cairo_move_to (cr, 0, -10);
      cairo_rel_line_to (cr, 0, -50);
      cairo_rel_line_to (cr, 10, 0);
      cairo_rel_line_to (cr, -5, 50);
      cairo_close_path (cr);

      cairo_clip_preserve (cr);

      pattern = cairo_pattern_create_linear (0, -10, 0, -60);
      cairo_pattern_add_color_stop_rgb (pattern, 0, 0, 1, 0);
      cairo_pattern_add_color_stop_rgb (pattern, 1, 1, 0, 0);
      cairo_set_source (cr, pattern);
      cairo_pattern_destroy (pattern);

      mask = cairo_pattern_create_linear (0, -10, 0, -60);
      cairo_pattern_add_color_stop_rgba (mask, 0, 0, 0, 0, 1);
      cairo_pattern_add_color_stop_rgba (mask, slider, 0, 0, 0, 1);
      cairo_pattern_add_color_stop_rgba (mask, slider, 0, 0, 0, 0);
      cairo_pattern_add_color_stop_rgba (mask, 1, 0, 0, 0, 0);
      cairo_mask (cr, mask);
      cairo_pattern_destroy (mask);

      cairo_set_source_rgb (cr, 0, 0, 0);
      cairo_stroke (cr);

      cairo_restore (cr);
    }

  cairo_restore (cr);
}

static const gchar *
tool_type_to_string (GdkDeviceToolType tool_type)
{
  switch (tool_type)
    {
    case GDK_DEVICE_TOOL_TYPE_PEN:
      return "Pen";
    case GDK_DEVICE_TOOL_TYPE_ERASER:
      return "Eraser";
    case GDK_DEVICE_TOOL_TYPE_BRUSH:
      return "Brush";
    case GDK_DEVICE_TOOL_TYPE_PENCIL:
      return "Pencil";
    case GDK_DEVICE_TOOL_TYPE_AIRBRUSH:
      return "Airbrush";
    case GDK_DEVICE_TOOL_TYPE_MOUSE:
      return "Mouse";
    case GDK_DEVICE_TOOL_TYPE_LENS:
      return "Lens cursor";
    case GDK_DEVICE_TOOL_TYPE_UNKNOWN:
    default:
      return "Unknown";
    }
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

  if (info->last_tool)
    {
      const gchar *tool_type;
      guint64 serial;

      tool_type = tool_type_to_string (gdk_device_tool_get_tool_type (info->last_tool));
      serial = gdk_device_tool_get_serial (info->last_tool);
      g_string_append_printf (string, "\nTool: %s", tool_type);

      if (serial != 0)
        g_string_append_printf (string, ", Serial: %lx", serial);
    }

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

static void
draw_cb (GtkDrawingArea *da,
         cairo_t        *cr,
         int             width,
         int             height,
         gpointer        user_data)
{
  GtkWidget *widget = GTK_WIDGET (da);
  EventData *data = user_data;
  AxesInfo *info;
  GHashTableIter iter;
  gpointer key, value;
  gint y = 0;

  /* Draw Abs info */
  g_hash_table_iter_init (&iter, data->pointer_info);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      info = value;
      draw_axes_info (cr, info, width, height);
    }

  g_hash_table_iter_init (&iter, data->touch_info);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      info = value;
      draw_axes_info (cr, info, width, height);
    }

  /* Draw name, color legend and misc data */
  g_hash_table_iter_init (&iter, data->pointer_info);

  while (g_hash_table_iter_next (&iter, NULL, &value))
    {
      info = value;
      draw_device_info (widget, cr, NULL, &y, info);
    }

  g_hash_table_iter_init (&iter, data->touch_info);

  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      info = value;
      draw_device_info (widget, cr, key, &y, info);
    }
}

static void
update_label_text (GtkWidget   *label,
                   const gchar *text)
{
  gchar *markup = NULL;

  if (text)
    markup = g_strdup_printf ("<span font='48.0'>%s</span>", text);
  gtk_label_set_markup (GTK_LABEL (label), markup);
  g_free (markup);
}

static gboolean
reset_label_text_timeout_cb (gpointer user_data)
{
  GtkWidget *label = user_data;

  update_label_text (label, NULL);
  pad_action_timeout_id = 0;
  return G_SOURCE_REMOVE;
}

static void
update_label_and_timeout (GtkWidget   *label,
                          const gchar *text)
{
  if (pad_action_timeout_id)
    g_source_remove (pad_action_timeout_id);

  update_label_text (label, text);
  pad_action_timeout_id = g_timeout_add (200, reset_label_text_timeout_cb, label);
}

static void
on_action_activate (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  GtkWidget *label = user_data;
  const gchar *result;
  gchar *str;

  result = g_object_get_data (G_OBJECT (action), "action-result");

  if (!parameter)
    update_label_and_timeout (label, result);
  else
    {
      str = g_strdup_printf ("%s %.2f", result, g_variant_get_double (parameter));
      update_label_and_timeout (label, str);
      g_free (str);
    }
}

static void
init_pad_controller (GtkWidget *window,
                     GtkWidget *label)
{
  GtkPadController *pad_controller;
  GSimpleActionGroup *action_group;
  GSimpleAction *action;
  gint i;

  action_group = g_simple_action_group_new ();
  pad_controller = gtk_pad_controller_new (G_ACTION_GROUP (action_group),
                                           NULL);

  for (i = 0; i < G_N_ELEMENTS (pad_actions); i++)
    {
      if (pad_actions[i].type == GTK_PAD_ACTION_BUTTON)
        {
          action = g_simple_action_new (pad_actions[i].action_name, NULL);
        }
      else
        {
          action = g_simple_action_new_stateful (pad_actions[i].action_name,
                                                 G_VARIANT_TYPE_DOUBLE, NULL);
        }

      g_signal_connect (action, "activate",
                        G_CALLBACK (on_action_activate), label);
      g_object_set_data (G_OBJECT (action), "action-result",
                         (gpointer) pad_action_results[i]);
      g_action_map_add_action (G_ACTION_MAP (action_group), G_ACTION (action));
      g_object_unref (action);
    }

  gtk_pad_controller_set_action_entries (pad_controller, pad_actions,
                                         G_N_ELEMENTS (pad_actions));
  gtk_widget_add_controller (window, GTK_EVENT_CONTROLLER (pad_controller));

  g_object_unref (action_group);
}

GtkWidget *
do_event_axes (GtkWidget *toplevel)
{
  static GtkWidget *window = NULL;
  EventData *event_data;
  GtkWidget *label;
  GtkWidget *overlay;
  GtkWidget *da;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Touch and Drawing Tablets");

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      gtk_widget_set_support_multidevice (window, TRUE);

      event_data = event_data_new ();
      g_object_set_data_full (G_OBJECT (window), "gtk-demo-event-data",
                              event_data, (GDestroyNotify) event_data_free);

      da = gtk_drawing_area_new ();
      gtk_drawing_area_set_content_width (GTK_DRAWING_AREA (da), 400);
      gtk_drawing_area_set_content_height (GTK_DRAWING_AREA (da), 400);
      gtk_drawing_area_set_draw_func (GTK_DRAWING_AREA (da), draw_cb, event_data, NULL);
      gtk_widget_set_can_focus (da, TRUE);
      gtk_widget_grab_focus (da);

      g_signal_connect (da, "event",
                        G_CALLBACK (event_cb), event_data);

      label = gtk_label_new ("");
      gtk_widget_set_halign (label, GTK_ALIGN_START);
      gtk_widget_set_valign (label, GTK_ALIGN_START);
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);

      overlay = gtk_overlay_new ();
      gtk_container_add (GTK_CONTAINER (window), overlay);
      gtk_container_add (GTK_CONTAINER (overlay), da);
      gtk_overlay_add_overlay (GTK_OVERLAY (overlay), label);

      init_pad_controller (da, label);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
