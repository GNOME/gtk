/* Menu
 * #Keywords: action, zoom
 *
 * Demonstrates how to add a context menu to a custom widget
 * and connect it with widget actions.
 *
 * The custom widget we create here is similar to a GtkPicture,
 * but allows setting a zoom level for the displayed paintable.
 *
 * Our context menu has items to change the zoom level.
 */

#include <gtk/gtk.h>
#include "demo3widget.h"


GtkWidget *
do_menu (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;

  if (!window)
    {
      GtkWidget *box;
      GtkWidget *box2;
      GtkWidget *sw;
      GtkWidget *widget;
      GtkWidget *scale;
      GtkWidget *dropdown;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Menu");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_window_set_child (GTK_WINDOW (window), box);

      sw = gtk_scrolled_window_new ();
      gtk_widget_set_vexpand (sw, TRUE);
      gtk_box_append (GTK_BOX (box), sw);

      widget = demo3_widget_new ("/transparent/portland-rose.jpg");
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), widget);

      box2 = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_append (GTK_BOX (box), box2);

      scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0.01, 10.0, 0.1);
      gtk_range_set_value (GTK_RANGE (scale), 1.0);
      gtk_widget_set_hexpand (scale, TRUE);
      gtk_box_append (GTK_BOX (box2), scale);

      dropdown = gtk_drop_down_new (G_LIST_MODEL (gtk_string_list_new ((const char *[]){ "Linear", "Nearest", "Trilinear", NULL })), NULL);
      gtk_box_append (GTK_BOX (box2), dropdown);

      g_object_bind_property (dropdown, "selected", widget, "filter", G_BINDING_DEFAULT);
 
      g_object_bind_property (gtk_range_get_adjustment (GTK_RANGE (scale)), "value",
                              widget, "scale",
                              G_BINDING_BIDIRECTIONAL);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_set_visible (window, TRUE);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
