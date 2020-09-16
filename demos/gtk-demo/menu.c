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
      GtkWidget *sw;
      GtkWidget *widget;

      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Menu");
      gtk_window_set_default_size (GTK_WINDOW (window), 600, 400);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      sw = gtk_scrolled_window_new ();
      gtk_window_set_child (GTK_WINDOW (window), sw);

      widget = demo3_widget_new ("/transparent/portland-rose.jpg");
      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), widget);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
