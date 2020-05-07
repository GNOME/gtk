/* Drag-and-Drop
 *
 * This demo shows dragging colors and widgets.
 * The items in this demo can be moved, recolored
 * and rotated.
 */

#include <gtk/gtk.h>

static GdkContentProvider *
prepare (GtkDragSource *source,
         double         x,
         double         y)
{
  GtkWidget *canvas;
  GtkWidget *item;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = gtk_widget_pick (canvas, x, y, GTK_PICK_DEFAULT);

  if (!GTK_IS_LABEL (item))
    return NULL;

  g_object_set_data (G_OBJECT (canvas), "dragged-item", item);

  return gdk_content_provider_new_typed (GTK_TYPE_WIDGET, item);
}

static void
drag_begin (GtkDragSource *source,
            GdkDrag       *drag)
{
  GtkWidget *canvas;
  GtkWidget *item;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = g_object_get_data (G_OBJECT (canvas), "dragged-item");

  gtk_widget_set_opacity (item, 0.5);
}

static void
drag_end (GtkDragSource *source,
          GdkDrag       *drag)
{
  GtkWidget *canvas;
  GtkWidget *item;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = g_object_get_data (G_OBJECT (canvas), "dragged-item");
  g_object_set_data (G_OBJECT (canvas), "dragged-item", NULL);

  gtk_widget_set_opacity (item, 1.0);
}

static void
drag_cancel (GtkDragSource       *source,
             GdkDrag             *drag,
             GdkDragCancelReason  reason)
{
  drag_end (source, drag);
}

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

static gboolean
drag_drop (GtkDropTarget *target,
           const GValue  *value,
           double         x,
           double         y)
{
  GtkWidget *item;
  TransformData *transform_data;
  GtkWidget *canvas;
  GtkWidget *last_child;

  item = g_value_get_object (value);
  transform_data = g_object_get_data (G_OBJECT (item), "transform-data");

  transform_data->x = x;
  transform_data->y = y;

  canvas = gtk_widget_get_parent (item);
  last_child = gtk_widget_get_last_child (canvas);
  if (item != last_child)
    gtk_widget_insert_after (item, canvas, last_child);

  apply_transform (item);

  return TRUE;
}

static double pos_x, pos_y;

static GtkWidget * canvas_item_new (double x, double y);

static void
new_item_cb (GtkWidget *button, gpointer data)
{
  GtkWidget *canvas = data;
  GtkWidget *item;

  item = canvas_item_new (pos_x, pos_y);
  gtk_container_add (GTK_CONTAINER (canvas), item);
  apply_transform (item);

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
edit_label_done (GtkWidget *entry, gpointer data)
{
  GtkWidget *canvas = gtk_widget_get_parent (entry);
  GtkWidget *label;
  int x, y;

  gtk_fixed_get_child_position (GTK_FIXED (canvas), entry, &x, &y);

  label = GTK_WIDGET (g_object_get_data (G_OBJECT (entry), "label"));
  gtk_label_set_text (GTK_LABEL (label), gtk_editable_get_text (GTK_EDITABLE (entry)));
  gtk_widget_show (label);

  gtk_fixed_remove (GTK_FIXED (canvas), entry);
}

static void
edit_cb (GtkWidget *button, GtkWidget *child)
{
  GtkWidget *canvas = gtk_widget_get_parent (child);
  int x, y;

  gtk_fixed_get_child_position (GTK_FIXED (canvas), child, &x, &y);

  if (GTK_IS_LABEL (child))
    {
      GtkWidget *entry = gtk_entry_new ();

      g_object_set_data (G_OBJECT (entry), "label", child);

      gtk_editable_set_text (GTK_EDITABLE (entry), gtk_label_get_text (GTK_LABEL (child)));
      gtk_editable_set_width_chars (GTK_EDITABLE (entry), 12);
      g_signal_connect (entry, "activate", G_CALLBACK (edit_label_done), NULL);
      gtk_fixed_put (GTK_FIXED (canvas), entry, x, y);
      gtk_widget_grab_focus (entry);
      gtk_widget_hide (child);
    }

  if (button)
    gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
delete_cb (GtkWidget *button, GtkWidget *child)
{
  GtkWidget *canvas = gtk_widget_get_parent (child);

  gtk_fixed_remove (GTK_FIXED (canvas), child);

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
pressed_cb (GtkGesture *gesture,
            int         n_press,
            double      x,
            double      y,
            gpointer    data)
{
  GtkWidget *widget;
  GtkWidget *child;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  child = gtk_widget_pick (widget, x, y, GTK_PICK_DEFAULT);

  if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)) == GDK_BUTTON_SECONDARY)
    {
      GtkWidget *menu;
      GtkWidget *box;
      GtkWidget *item;

      pos_x = x;
      pos_y = y;

      menu = gtk_popover_new ();
      gtk_widget_set_parent (menu, widget);
      gtk_popover_set_has_arrow (GTK_POPOVER (menu), FALSE);
      gtk_popover_set_pointing_to (GTK_POPOVER (menu), &(GdkRectangle){ x, y, 1, 1});
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (menu), box);

      item = gtk_button_new_with_label ("New");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      g_signal_connect (item, "clicked", G_CALLBACK (new_item_cb), widget);
      gtk_container_add (GTK_CONTAINER (box), item);

      item = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_container_add (GTK_CONTAINER (box), item);

      item = gtk_button_new_with_label ("Edit");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "clicked", G_CALLBACK (edit_cb), child);
      gtk_container_add (GTK_CONTAINER (box), item);

      item = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_container_add (GTK_CONTAINER (box), item);

      item = gtk_button_new_with_label ("Delete");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "clicked", G_CALLBACK (delete_cb), child);
      gtk_container_add (GTK_CONTAINER (box), item);

      gtk_popover_popup (GTK_POPOVER (menu));
    }
}

static void
released_cb (GtkGesture *gesture,
             int         n_press,
             double      x,
             double      y,
             gpointer    data)
{
  GtkWidget *widget;
  GtkWidget *child;

  widget = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture));
  child = gtk_widget_pick (widget, x, y, 0);

  if (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture)) == GDK_BUTTON_PRIMARY)
    {
      if (child != NULL && child != widget)
        edit_cb (NULL, child);
    }
}

static GtkWidget *
canvas_new (void)
{
  GtkWidget *canvas;
  GtkDragSource *source;
  GtkDropTarget *dest;
  GtkGesture *gesture;

  canvas = gtk_fixed_new ();
  gtk_widget_set_hexpand (canvas, TRUE);
  gtk_widget_set_vexpand (canvas, TRUE);
  gtk_widget_add_css_class (canvas, "frame");

  source = gtk_drag_source_new ();
  gtk_drag_source_set_actions (source, GDK_ACTION_MOVE);
  g_signal_connect (source, "prepare", G_CALLBACK (prepare), NULL);
  g_signal_connect (source, "drag-begin", G_CALLBACK (drag_begin), NULL);
  g_signal_connect (source, "drag-end", G_CALLBACK (drag_end), NULL);
  g_signal_connect (source, "drag-cancel", G_CALLBACK (drag_cancel), NULL);
  gtk_widget_add_controller (canvas, GTK_EVENT_CONTROLLER (source));

  dest = gtk_drop_target_new (GTK_TYPE_WIDGET, GDK_ACTION_MOVE);
  g_signal_connect (dest, "drop", G_CALLBACK (drag_drop), NULL);
  gtk_widget_add_controller (canvas, GTK_EVENT_CONTROLLER (dest));

  gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), 0);
  g_signal_connect (gesture, "pressed", G_CALLBACK (pressed_cb), NULL);
  g_signal_connect (gesture, "released", G_CALLBACK (released_cb), NULL);
  gtk_widget_add_controller (canvas, GTK_EVENT_CONTROLLER (gesture));

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

static gboolean
item_drag_drop (GtkDropTarget *dest,
                const GValue  *value,
                double         x,
                double         y)
{
  GtkWidget *item = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest));

  set_color (item, g_value_get_boxed (value));

  return TRUE;
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

static int n_items = 0;

static GtkWidget *
canvas_item_new (double x,
                 double y)
{
  GtkWidget *widget;
  char *label;
  char *id;
  TransformData *transform_data;
  GdkRGBA rgba;
  GtkDropTarget *dest;
  GtkGesture *gesture;

  n_items++;

  label = g_strdup_printf ("Item %d", n_items);
  id = g_strdup_printf ("item%d", n_items);

  gdk_rgba_parse (&rgba, "yellow");

  widget = gtk_label_new (label);
  gtk_widget_add_css_class (widget, "frame");
  gtk_widget_set_name (widget, id);

  set_color (widget, &rgba);
  transform_data = g_new0 (TransformData, 1);
  transform_data->x = x;
  transform_data->y = y;
  transform_data->angle = 0.0;
  g_object_set_data_full (G_OBJECT (widget), "transform-data", transform_data, g_free);

  g_free (label);
  g_free (id);

  dest = gtk_drop_target_new (GDK_TYPE_RGBA, GDK_ACTION_COPY);
  g_signal_connect (dest, "drop", G_CALLBACK (item_drag_drop), NULL);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (dest));

  gesture = gtk_gesture_rotate_new ();
  g_signal_connect (gesture, "angle-changed", G_CALLBACK (angle_changed), NULL);
  g_signal_connect (gesture, "end", G_CALLBACK (rotate_done), NULL);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "released", G_CALLBACK (click_done), NULL);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));
  
  return widget;
}

static GtkWidget *window = NULL;

GtkWidget *
do_dnd (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *sw;
      GtkWidget *canvas;
      GtkWidget *box, *box2, *box3;
      const char *colors[] = {
        "red", "green", "blue", "magenta", "orange", "gray", "black", "yellow",
        "white", "gray", "brown", "pink",  "cyan", "bisque", "gold", "maroon",
        "navy", "orchid", "olive", "peru", "salmon", "silver", "wheat",
        NULL
      };
      int i;
      int x, y;

      g_type_ensure (GTK_TYPE_COLOR_BUTTON);

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Drag-and-Drop");
      gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add (GTK_CONTAINER (box), box2);

      canvas = canvas_new ();
      gtk_container_add (GTK_CONTAINER (box2), canvas);

      n_items = 0;

      x = y = 40;
      for (i = 0; i < 4; i++)
        {
          GtkWidget *item;

          item = canvas_item_new (x, y);
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
      gtk_widget_add_css_class (box3, "linked");
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), box3);

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
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
