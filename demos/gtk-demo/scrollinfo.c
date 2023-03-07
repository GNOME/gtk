/* ScrollInfo
 *
 * GtkScrollInfo allows you to pass scrolling information to many
 * scrollable widgets is a container that shows a single child at a time,
 * with nice transitions when the visible child changes.
 *
 * GtkStackSwitcher adds buttons to control which child is visible.
 */

#include <gtk/gtk.h>

static GtkScrollInfo *scroll;

static void
viewport_x_width_changed (GtkAdjustment *x,
                          GtkAdjustment *width)
{
  graphene_rect_t viewport = *gtk_scroll_info_get_viewport (scroll);
  viewport.origin.x = gtk_adjustment_get_value (x);
  viewport.size.width = MIN (gtk_adjustment_get_value (width), 1.0 - viewport.origin.x);
  gtk_scroll_info_set_viewport (scroll, &viewport);
}

static void
viewport_y_height_changed (GtkAdjustment *y,
                           GtkAdjustment *height)
{
  graphene_rect_t viewport = *gtk_scroll_info_get_viewport (scroll);
  viewport.origin.y = gtk_adjustment_get_value (y);
  viewport.size.height = MIN (gtk_adjustment_get_value (height), 1.0 - viewport.origin.y);
  gtk_scroll_info_set_viewport (scroll, &viewport);
}

static void
enabled_changed (GtkCheckButton *horizontal,
                 GParamSpec     *unused,
                 GtkCheckButton *vertical)
{
  gtk_scroll_info_set_enable_horizontal (scroll, gtk_check_button_get_active (horizontal));
  gtk_scroll_info_set_enable_vertical (scroll, gtk_check_button_get_active (vertical));
}

static void
do_scroll (GtkButton *button,
           GtkWidget *widget)
{
  GtkWidget *viewport = gtk_widget_get_parent (gtk_widget_get_parent (widget));

  gtk_viewport_scroll_to (GTK_VIEWPORT (viewport),
                          widget,
                          scroll);
}

GtkWidget *
do_scrollinfo (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkBuilderScope *scope;
      GtkBuilder *builder;
      GError *error = NULL;

      scroll = gtk_scroll_info_new ();

      scope = gtk_builder_cscope_new ();
      gtk_builder_cscope_add_callback (scope, viewport_x_width_changed);
      gtk_builder_cscope_add_callback (scope, viewport_y_height_changed);
      gtk_builder_cscope_add_callback (scope, enabled_changed);
      gtk_builder_cscope_add_callback (scope, do_scroll);
      builder = gtk_builder_new ();
      gtk_builder_set_scope (builder, scope);
      if (!gtk_builder_add_from_resource (builder, "/scrollinfo/scrollinfo.ui", &error))
        g_error ("%s", error->message);

      window = GTK_WIDGET (gtk_builder_get_object (builder, "window"));
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      g_object_unref (builder);
      g_object_unref (scope);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));


  return window;
}
