/* Popovers
 *
 * A bubble-like window containing contextual information or options.
 * GtkPopovers can be attached to any widget, and will be displayed
 * within the same window, but on top of all its content.
 */

#include <gtk/gtk.h>

static void
toggle_changed_cb (GtkToggleButton *button,
                   GtkWidget       *popover)
{
  gtk_widget_set_visible (popover,
                          gtk_toggle_button_get_active (button));
}

static GtkWidget *
create_popover (GtkWidget       *parent,
                GtkWidget       *child,
                GtkPositionType  pos)
{
  GtkWidget *popover;

  popover = gtk_popover_new (parent);
  gtk_popover_set_position (GTK_POPOVER (popover), pos);
  gtk_container_add (GTK_CONTAINER (popover), child);
  gtk_container_set_border_width (GTK_CONTAINER (popover), 6);
  gtk_widget_show (child);

  return popover;
}

static GtkWidget *
create_complex_popover (GtkWidget       *parent,
                        GtkPositionType  pos)
{
  GtkWidget *popover, *window, *content;
  GtkBuilder *builder;

  builder = gtk_builder_new ();
  gtk_builder_add_from_resource (builder, "/popover/popover.ui", NULL);
  window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
  content = gtk_bin_get_child (GTK_BIN (window));
  g_object_ref (content);
  gtk_container_remove (GTK_CONTAINER (gtk_widget_get_parent (content)),
                        content);
  gtk_widget_destroy (window);
  g_object_unref (builder);

  popover = create_popover (parent, content, GTK_POS_BOTTOM);
  g_object_unref (content);

  gtk_widget_set_size_request (popover, 200, -1);
  gtk_widget_set_vexpand (popover, TRUE);

  gtk_widget_set_margin_start (popover, 10);
  gtk_widget_set_margin_end (popover, 10);
  gtk_widget_set_margin_bottom (popover, 10);

  return popover;
}

static void
entry_size_allocate_cb (GtkEntry      *entry,
                        GtkAllocation *allocation,
                        gpointer       user_data)
{
  GtkEntryIconPosition popover_pos;
  GtkPopover *popover = user_data;
  cairo_rectangle_int_t rect;

  if (gtk_widget_is_visible (GTK_WIDGET (popover)))
    {
      popover_pos =
        GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (entry),
                                             "popover-icon-pos"));
      gtk_entry_get_icon_area (entry, popover_pos, &rect);
      gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
    }
}

static void
entry_icon_press_cb (GtkEntry             *entry,
                     GtkEntryIconPosition  icon_pos,
                     GdkEvent             *event,
                     gpointer              user_data)
{
  GtkWidget *popover = user_data;
  cairo_rectangle_int_t rect;

  gtk_entry_get_icon_area (entry, icon_pos, &rect);
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
  gtk_widget_show (popover);

  g_object_set_data (G_OBJECT (entry), "popover-icon-pos",
                     GUINT_TO_POINTER (icon_pos));
}

static void
day_selected_cb (GtkCalendar *calendar,
                 gpointer     user_data)
{
  cairo_rectangle_int_t rect;
  GtkAllocation allocation;
  GtkWidget *popover;
  GdkEvent *event;

  event = gtk_get_current_event ();

  if (event->type != GDK_BUTTON_PRESS)
    return;

  gdk_window_coords_to_parent (event->button.window,
                               event->button.x, event->button.y,
                               &event->button.x, &event->button.y);
  gtk_widget_get_allocation (GTK_WIDGET (calendar), &allocation);
  rect.x = event->button.x - allocation.x;
  rect.y = event->button.y - allocation.y;
  rect.width = rect.height = 1;

  popover = create_popover (GTK_WIDGET (calendar),
                            gtk_entry_new (),
                            GTK_POS_BOTTOM);
  gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);

  gtk_widget_show (popover);

  gdk_event_free (event);
}

GtkWidget *
do_popover (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *popover, *box, *widget;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 24);
      gtk_container_set_border_width (GTK_CONTAINER (box), 24);
      gtk_container_add (GTK_CONTAINER (window), box);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      widget = gtk_toggle_button_new_with_label ("Button");
      popover = create_popover (widget,
                                gtk_label_new ("This popover does not grab input"),
                                GTK_POS_TOP);
      gtk_popover_set_modal (GTK_POPOVER (popover), FALSE);
      g_signal_connect (widget, "toggled",
                        G_CALLBACK (toggle_changed_cb), popover);
      gtk_container_add (GTK_CONTAINER (box), widget);

      widget = gtk_entry_new ();
      popover = create_complex_popover (widget, GTK_POS_TOP);
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (widget),
                                         GTK_ENTRY_ICON_PRIMARY, "edit-find");
      gtk_entry_set_icon_from_icon_name (GTK_ENTRY (widget),
                                         GTK_ENTRY_ICON_SECONDARY, "edit-clear");

      g_signal_connect (widget, "icon-press",
                        G_CALLBACK (entry_icon_press_cb), popover);
      g_signal_connect (widget, "size-allocate",
                        G_CALLBACK (entry_size_allocate_cb), popover);
      gtk_container_add (GTK_CONTAINER (box), widget);

      widget = gtk_calendar_new ();
      g_signal_connect (widget, "day-selected",
                        G_CALLBACK (day_selected_cb), NULL);
      gtk_container_add (GTK_CONTAINER (box), widget);
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
