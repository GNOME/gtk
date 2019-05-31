/* Awards
 *
 * This demo demonstrates how to use lists to show the awards you have collected
 * while exploring this demo.
 *
 */

#include <gtk/gtk.h>

/* Include the header for accessing the awards */
#include "award.h"

static GtkWidget *window = NULL;

GtkWidget *
do_awardview (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *sw, *listview;
      GListModel *list;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Awards");
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (window), sw);

      listview = gtk_list_view_new_with_factory (
          gtk_builder_list_item_factory_new_from_resource (NULL, "/awardview/awardlistitem.ui"));
      list = gtk_award_get_list ();
      gtk_list_view_set_model (GTK_LIST_VIEW (listview), list);
      g_object_unref (list);
      gtk_list_view_set_show_separators (GTK_LIST_VIEW (listview), TRUE);
      gtk_container_add (GTK_CONTAINER (sw), listview);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
