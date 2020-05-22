/* Drag-and-Drop
 *
 * This demo shows dragging colors and widgets.
 * The items in this demo can be moved, recolored
 * and rotated.
 */

#include <gtk/gtk.h>

G_DECLARE_FINAL_TYPE (CanvasItem, canvas_item, CANVAS, ITEM, GtkWidget)

struct _CanvasItem {
  GtkWidget parent;

  GtkWidget *label;

  double x, y;
  double angle;
  double delta;
};

struct _CanvasItemClass {
  GtkWidgetClass parent_class;
};

G_DEFINE_TYPE (CanvasItem, canvas_item, GTK_TYPE_WIDGET)

static int n_items = 0;

static void
set_color (CanvasItem *item,
           GdkRGBA    *color)
{
  char *css;
  char *str;
  GtkStyleContext *context;
  GtkCssProvider *provider;

  str = gdk_rgba_to_string (color);
  css = g_strdup_printf ("* { background: %s; padding: 10px; }", str);

  context = gtk_widget_get_style_context (item->label);
  provider = g_object_get_data (G_OBJECT (context), "style-provider");
  if (provider)
    gtk_style_context_remove_provider (context, GTK_STYLE_PROVIDER (provider));

  provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_data (provider, css, -1);
  gtk_style_context_add_provider (gtk_widget_get_style_context (item->label), GTK_STYLE_PROVIDER (provider), 800);
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
  CanvasItem *item = CANVAS_ITEM (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (dest)));

  set_color (item, g_value_get_boxed (value));

  return TRUE;
}

static void
apply_transform (CanvasItem *item)
{
  GtkWidget *canvas = gtk_widget_get_parent (GTK_WIDGET (item));
  GskTransform *transform;

  transform = gsk_transform_rotate (gsk_transform_translate (NULL, &(graphene_point_t){item->x, item->y}),
                                    item->angle + item->delta);
  gtk_fixed_set_child_transform (GTK_FIXED (canvas), GTK_WIDGET (item), transform);
  gsk_transform_unref (transform);
}

static void
angle_changed (GtkGestureRotate *gesture,
               double            angle,
               double            delta)
{
  CanvasItem *item = CANVAS_ITEM (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)));

  item->delta = angle / M_PI * 180.0;

  apply_transform (item);
}

static void
rotate_done (GtkGesture *gesture)
{
  CanvasItem *item = CANVAS_ITEM (gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (gesture)));

  item->angle = item->angle + item->delta;
  item->delta = 0;
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

static void
canvas_item_init (CanvasItem *item)
{
  char *text;
  char *id;
  GdkRGBA rgba;
  GtkDropTarget *dest;
  GtkGesture *gesture;

  n_items++;

  text = g_strdup_printf ("Item %d", n_items);
  item->label = gtk_label_new (text);
  g_free (text);

  gtk_widget_set_parent (item->label, GTK_WIDGET (item));

  gtk_widget_add_css_class (item->label, "frame");

  id = g_strdup_printf ("item%d", n_items);
  gtk_widget_set_name (item->label, id);
  g_free (id);

  gdk_rgba_parse (&rgba, "yellow");
  set_color (item, &rgba);

  item->x = 0;
  item->y = 0;
  item->angle = 0;

  dest = gtk_drop_target_new (GDK_TYPE_RGBA, GDK_ACTION_COPY);
  g_signal_connect (dest, "drop", G_CALLBACK (item_drag_drop), NULL);
  gtk_widget_add_controller (GTK_WIDGET (item), GTK_EVENT_CONTROLLER (dest));

  gesture = gtk_gesture_rotate_new ();
  g_signal_connect (gesture, "angle-changed", G_CALLBACK (angle_changed), NULL);
  g_signal_connect (gesture, "end", G_CALLBACK (rotate_done), NULL);
  gtk_widget_add_controller (GTK_WIDGET (item), GTK_EVENT_CONTROLLER (gesture));

  gesture = gtk_gesture_click_new ();
  g_signal_connect (gesture, "released", G_CALLBACK (click_done), NULL);
  gtk_widget_add_controller (GTK_WIDGET (item), GTK_EVENT_CONTROLLER (gesture));
}

static void
canvas_item_dispose (GObject *object)
{
  CanvasItem *item = CANVAS_ITEM (object);

  g_clear_pointer (&item->label, gtk_widget_unparent);

  G_OBJECT_CLASS (canvas_item_parent_class)->dispose (object);
}

static void
canvas_item_class_init (CanvasItemClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (class);

  object_class->dispose = canvas_item_dispose;

  gtk_widget_class_set_layout_manager_type (widget_class, GTK_TYPE_BIN_LAYOUT);
}

static GtkWidget *
canvas_item_new (double x,
                 double y)
{
  CanvasItem *item = g_object_new (canvas_item_get_type (), NULL);
  item->x = x;
  item->y = y;

  return GTK_WIDGET (item);
}

static GdkPaintable *
canvas_item_get_drag_icon (CanvasItem *item)
{
  return gtk_widget_paintable_new (item->label);
}

static GdkContentProvider *
prepare (GtkDragSource *source,
         double         x,
         double         y)
{
  GtkWidget *canvas;
  GtkWidget *item;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = gtk_widget_pick (canvas, x, y, GTK_PICK_DEFAULT);

  item = gtk_widget_get_ancestor (item, canvas_item_get_type ());
  if (!item)
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
  GdkPaintable *paintable;

  canvas = gtk_event_controller_get_widget (GTK_EVENT_CONTROLLER (source));
  item = g_object_get_data (G_OBJECT (canvas), "dragged-item");

  paintable = canvas_item_get_drag_icon (CANVAS_ITEM (item));
  gtk_drag_source_set_icon (source, paintable, 0, 0);
  g_object_unref (paintable);

  gtk_widget_set_opacity (item, 0.3);
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

static gboolean
drag_cancel (GtkDragSource       *source,
             GdkDrag             *drag,
             GdkDragCancelReason  reason)
{
  return FALSE;
}

static gboolean
drag_drop (GtkDropTarget *target,
           const GValue  *value,
           double         x,
           double         y)
{
  CanvasItem *item;
  GtkWidget *canvas;
  GtkWidget *last_child;

  item = g_value_get_object (value);

  item->x = x;
  item->y = y;

  canvas = gtk_widget_get_parent (GTK_WIDGET (item));
  last_child = gtk_widget_get_last_child (canvas);
  if (GTK_WIDGET (item) != last_child)
    gtk_widget_insert_after (GTK_WIDGET (item), canvas, last_child);

  apply_transform (item);

  return TRUE;
}

static double pos_x, pos_y;

static void
new_item_cb (GtkWidget *button, gpointer data)
{
  GtkWidget *canvas = data;
  GtkWidget *item;

  item = canvas_item_new (pos_x, pos_y);
  gtk_fixed_put (GTK_FIXED (canvas), item, 0, 0);
  apply_transform (CANVAS_ITEM (item));

  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (button, GTK_TYPE_POPOVER)));
}

static void
edit_label_done (GtkWidget *entry, gpointer data)
{
  GtkWidget *canvas = gtk_widget_get_parent (entry);
  CanvasItem *item;
  int x, y;

  gtk_fixed_get_child_position (GTK_FIXED (canvas), entry, &x, &y);

  item = CANVAS_ITEM (g_object_get_data (G_OBJECT (entry), "item"));
  gtk_label_set_text (GTK_LABEL (item->label), gtk_editable_get_text (GTK_EDITABLE (entry)));
  gtk_widget_show (GTK_WIDGET (item));

  gtk_fixed_remove (GTK_FIXED (canvas), entry);
}

static void
edit_cb (GtkWidget *button, GtkWidget *child)
{
  GtkWidget *canvas = gtk_widget_get_parent (child);
  CanvasItem *item = CANVAS_ITEM (child);
  GtkWidget *entry;
  double x, y;

  gtk_widget_translate_coordinates (child, canvas, 0, 0, &x, &y);

  entry = gtk_entry_new ();

  g_object_set_data (G_OBJECT (entry), "item", item);

  gtk_editable_set_text (GTK_EDITABLE (entry),
                         gtk_label_get_text (GTK_LABEL (item->label)));

  gtk_editable_set_width_chars (GTK_EDITABLE (entry), 12);
  g_signal_connect (entry, "activate", G_CALLBACK (edit_label_done), NULL);
  gtk_fixed_put (GTK_FIXED (canvas), entry, x, y);
  gtk_widget_grab_focus (entry);
  gtk_widget_hide (child);

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
  child = gtk_widget_get_ancestor (child, canvas_item_get_type ());

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
      gtk_popover_set_child (GTK_POPOVER (menu), box);

      item = gtk_button_new_with_label ("New");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      g_signal_connect (item, "clicked", G_CALLBACK (new_item_cb), widget);
      gtk_box_append (GTK_BOX (box), item);

      item = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box), item);

      item = gtk_button_new_with_label ("Edit");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "clicked", G_CALLBACK (edit_cb), child);
      gtk_box_append (GTK_BOX (box), item);

      item = gtk_separator_new (GTK_ORIENTATION_HORIZONTAL);
      gtk_box_append (GTK_BOX (box), item);

      item = gtk_button_new_with_label ("Delete");
      gtk_button_set_has_frame (GTK_BUTTON (item), FALSE);
      gtk_widget_set_sensitive (item, child != NULL && child != widget);
      g_signal_connect (item, "clicked", G_CALLBACK (delete_cb), child);
      gtk_box_append (GTK_BOX (box), item);

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
  child = gtk_widget_get_ancestor (child, canvas_item_get_type ());

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

static GtkWidget *window = NULL;

GtkWidget *
do_dnd (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *button;
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

      button = gtk_color_button_new ();
      g_object_unref (g_object_ref_sink (button));

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Drag-and-Drop");
      gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (box), box2);

      canvas = canvas_new ();
      gtk_box_append (GTK_BOX (box2), canvas);

      n_items = 0;

      x = y = 40;
      for (i = 0; i < 4; i++)
        {
          GtkWidget *item;

          item = canvas_item_new (x, y);
          gtk_fixed_put (GTK_FIXED (canvas), item, 0, 0);
          apply_transform (CANVAS_ITEM (item));

          x += 150;
          y += 100;
        }

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_AUTOMATIC,
                                      GTK_POLICY_NEVER);
      gtk_box_append (GTK_BOX (box), sw);

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
          gtk_box_append (GTK_BOX (box3), swatch);
        }
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
