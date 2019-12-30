#include <gtk/gtk.h>

static GdkContentProvider *
prepare (GtkDragSource *source,  double x, double y)
{
  GtkWidget *canvas;
  GtkWidget *item;
  GdkContentProvider *provider;
  GBytes *bytes;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = gtk_widget_pick (canvas, x, y, GTK_PICK_DEFAULT);

  if (!GTK_IS_LABEL (item))
    return NULL;

  bytes = g_bytes_new (&item, sizeof (gpointer));
  provider = gdk_content_provider_new_for_bytes ("CANVAS_ITEM", bytes);
  g_bytes_unref (bytes);

  g_object_set_data (G_OBJECT (canvas), "dragged-item", item);

  return provider;
}

static void
drag_begin (GtkDragSource *source, GdkDrag *drag)
{
  GtkWidget *canvas;
  GtkWidget *item;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = g_object_get_data (G_OBJECT (canvas), "dragged-item");

  gtk_widget_set_opacity (item, 0.5);
}

static void
drag_end (GtkDragSource *source, GdkDrag *drag)
{
  GtkWidget *canvas;
  GtkWidget *item;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = g_object_get_data (G_OBJECT (canvas), "dragged-item");
  g_object_set_data (G_OBJECT (canvas), "dragged-item", NULL);

  gtk_widget_set_opacity (item, 1.0);
}

typedef struct {
  GtkWidget *canvas;
  double x;
  double y;
} DropData;

typedef struct {
  double x, y;
  double angle;
  double delta;
} TransformData;

static void
apply_transform (GtkWidget *item)
{
  GtkWidget *canvas = gtk_widget_get_parent (item);
  TransformData *data;
  GskTransform *transform;

  data = g_object_get_data (G_OBJECT (item), "transform-data");
  transform = gsk_transform_rotate (gsk_transform_translate (NULL, &(graphene_point_t){data->x, data->y}),
                                    data->angle + data->delta);
  gtk_fixed_set_child_transform (GTK_FIXED (canvas), item, transform);
  gsk_transform_unref (transform);
}

static void
got_data (GObject      *source,
          GAsyncResult *result,
          gpointer      user_data)
{
  GdkDrop *drop = GDK_DROP (source);
  DropData *data = user_data;
  GInputStream *stream;
  GBytes *bytes;
  GtkWidget *item;
  const char *mime_type;
  GError *error = NULL;
  TransformData *transform_data;
  GtkWidget *canvas;
  GtkWidget *last_child;

  stream = gdk_drop_read_finish (drop, result, &mime_type, &error);
  bytes = g_input_stream_read_bytes (stream, sizeof (gpointer), NULL, NULL);
  item = (gpointer) *(gpointer *)g_bytes_get_data (bytes, NULL);

  transform_data = g_object_get_data (G_OBJECT (item), "transform-data");

  transform_data->x = data->x;
  transform_data->y = data->y;

  canvas = gtk_widget_get_parent (item);
  last_child = gtk_widget_get_last_child (canvas);
  if (item != last_child)
    gtk_widget_insert_after (item, canvas, last_child);

  apply_transform (item);

  gdk_drop_finish (drop, GDK_ACTION_MOVE);

  g_bytes_unref (bytes);
  g_object_unref (stream);
  g_free (data);
}

static gboolean
drag_drop (GtkDropTarget *dest,
           GdkDrop       *drop,
           int            x,
           int            y)
{
  DropData *data;

  data = g_new (DropData, 1);
  data->canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest));
  data->x = x;
  data->y = y;

  gdk_drop_read_async (drop, (const char *[]){"CANVAS_ITEM", NULL}, G_PRIORITY_DEFAULT, NULL, got_data, data);

  return TRUE;
}

static GtkWidget *
canvas_new (void)
{
  GtkWidget *canvas;
  GtkDragSource *source;
  GtkDropTarget *dest;
  GdkContentFormats *formats;

  canvas = gtk_fixed_new ();
  gtk_widget_set_hexpand (canvas, TRUE);
  gtk_widget_set_vexpand (canvas, TRUE);
  gtk_style_context_add_class (gtk_widget_get_style_context (canvas), "frame");

  source = gtk_drag_source_new ();
  gtk_drag_source_set_actions (source, GDK_ACTION_MOVE);
  g_signal_connect (source, "prepare", G_CALLBACK (prepare), NULL);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), NULL);
  g_signal_connect (source, "drag-end", G_CALLBACK (drag_end), NULL);
  gtk_widget_add_controller (canvas, GTK_EVENT_CONTROLLER (source));

  formats = gdk_content_formats_new ((const char *[]){"CANVAS_ITEM", NULL}, 1);
  dest = gtk_drop_target_new (formats, GDK_ACTION_MOVE);
  g_signal_connect (dest, "drag-drop", G_CALLBACK (drag_drop), NULL);
  gtk_widget_add_controller (canvas, GTK_EVENT_CONTROLLER (dest));
  gdk_content_formats_unref (formats);

  return canvas;
}

static void
set_color (GtkWidget *item,
           GdkRGBA   *color)
{
  char *css;
  char *str;
  GtkStyleContext *context;
  GtkCssProvider *provider;

  str = gdk_rgba_to_string (color);
  css = g_strdup_printf ("* { background: %s; padding: 10px; }", str);

  context = gtk_widget_get_style_context (item);
  provider = g_object_get_data (G_OBJECT (context), "style-provider");
  if (provider)
    gtk_style_context_remove_provider (context, GTK_STYLE_PROVIDER (provider));

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider (gtk_widget_get_style_context (item), GTK_STYLE_PROVIDER (provider), 800);
  g_object_set_data_full (G_OBJECT (context), "style-provider", provider, g_object_unref);

  g_free (str);
  g_free (css);
}

static void
got_color (GObject *source,
           GAsyncResult *result,
           gpointer data)
{
  GdkDrop *drop = GDK_DROP (source);
  GtkDropTarget *dest = data;
  GtkWidget *item = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest));
  const GValue *value;
  GdkRGBA *color;

  value = gdk_drop_read_value_finish (drop, result, NULL);
  color = g_value_get_boxed (value);
  set_color (item, color);

  gdk_drop_finish (drop, GDK_ACTION_COPY);
}

static gboolean
item_drag_drop (GtkDropTarget *dest,
                GdkDrop       *drop,
               int            x,
               int            y)
{
  if (gtk_drop_target_find_mimetype (dest))
    {
      gdk_drop_read_value_async (drop, GDK_TYPE_RGBA, G_PRIORITY_DEFAULT, NULL, got_color, dest);
      return TRUE;
    }

  return FALSE;
}

static gboolean
item_drag_motion (GtkDropTarget *dest,
                  GdkDrop       *drop)
{
  if (gtk_drop_target_find_mimetype (dest) != NULL)
    {
      gdk_drop_status (drop, GDK_ACTION_COPY);
      return TRUE;
    }

  return FALSE;
}

static void
angle_changed (GtkGestureRotate *gesture,
               double            angle,
               double            delta)
{
  GtkWidget *item = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  TransformData *data = g_object_get_data (G_OBJECT (item), "transform-data");

  data->delta = angle / M_PI * 180.0;

  apply_transform (item);
}

static void
rotate_done (GtkGesture *gesture)
{
  GtkWidget *item = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  TransformData *data = g_object_get_data (G_OBJECT (item), "transform-data");

  data->angle = data->angle + data->delta;
  data->delta = 0;
}

static void
click_done (GtkGesture *gesture)
{
  GtkWidget *item = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  GtkWidget *canvas = gtk_widget_get_parent (item);
  GtkWidget *last_child;

  last_child = gtk_widget_get_last_child (canvas);
  if (item != last_child)
    gtk_widget_insert_after (item, canvas, last_child);
}

static GtkWidget *
canvas_item_new (int i,
                 double x,
                 double y,
                 double angle)
{
  GtkWidget *widget;
  char *label;
  char *id;
  TransformData *transform_data;
  GdkRGBA rgba;
  GtkDropTarget *dest;
  GdkContentFormats *formats;
  GtkGesture *gesture;

  label = g_strdup_printf ("Item %d", i);
  id = g_strdup_printf ("item%d", i);

  gdk_rgba_parse (&rgba, "yellow");

  widget = gtk_label_new (label);
  gtk_style_context_add_class (gtk_widget_get_style_context (widget), "frame");
  gtk_widget_set_name (widget, id);

  set_color (widget, &rgba);
  transform_data = g_new0 (TransformData, 1);
  transform_data->x = x;
  transform_data->y = y;
  transform_data->angle = angle;
  g_object_set_data_full (G_OBJECT (widget), "transform-data", transform_data, g_free);

  g_free (label);
  g_free (id);

  formats = gdk_content_formats_new_for_gtype (GDK_TYPE_RGBA);
  dest = gtk_drop_target_new (formats, GDK_ACTION_COPY);
  g_signal_connect (dest, "drag-drop", G_CALLBACK (item_drag_drop), NULL);
  g_signal_connect (dest, "accept", G_CALLBACK (item_drag_motion), NULL);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (dest));
  gdk_content_formats_unref (formats);

  gesture = gtk_gesture_rotate_new ();
  g_signal_connect (gesture, "angle-changed", G_CALLBACK (angle_changed), NULL);
  g_signal_connect (gesture, "end", G_CALLBACK (rotate_done), NULL);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "released", G_CALLBACK (click_done), NULL);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));
  
  return widget;
}

int main (int argc, char *argv[])
{
  GtkWidget *window;
  GtkWidget *sw;
  GtkWidget *canvas;
  GtkWidget *widget;
  GtkWidget *box, *box2, *box3;
  const char *colors[] = {
    "red", "green", "blue", "magenta", "orange", "gray", "black", "yellow",
    "white", "gray", "brown", "pink",  "cyan", "bisque", "gold", "maroon",
    "navy", "orchid", "olive", "peru", "salmon", "silver", "wheat",
    NULL
  };
  int i;
  int x, y;

  gtk_init ();

  widget = gtk_color_button_new ();
  gtk_widget_destroy (widget);
  
  window = gtk_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_container_add (GTK_CONTAINER (box), box2);

  canvas = canvas_new ();
  gtk_container_add (GTK_CONTAINER (box2), canvas);

  x = y = 40;
  for (i = 0; i < 4; i++)
    {
      GtkWidget *item;

      item = canvas_item_new (i, x, y, 0);
      gtk_container_add (GTK_CONTAINER (canvas), item);
      apply_transform (item);

      x += 150;
      y += 100;
    }

  sw = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                  GTK_POLICY_AUTOMATIC,
                                  GTK_POLICY_NEVER);
  gtk_container_add (GTK_CONTAINER (box), sw);

  box3 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_style_context_add_class (gtk_widget_get_style_context (box3), "linked");
  gtk_container_add (GTK_CONTAINER (sw), box3);

  for (i = 0; colors[i]; i++)
    {
      GdkRGBA rgba;
      GtkWidget *swatch;

      gdk_rgba_parse (&rgba, colors[i]);

      swatch = g_object_new (g_type_from_name ("GtkColorSwatch"),
                             "rgba", &rgba,
                             "selectable", FALSE,
                             NULL);
      gtk_container_add (GTK_CONTAINER (box3), swatch);
    }

  gtk_widget_show (window);

  while (TRUE)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
