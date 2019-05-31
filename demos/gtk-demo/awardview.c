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

#if 0
static void
create_listitem (GtkListItem *item, gpointer user_data)
{
  GtkWidget *label;

  label = gtk_label_new (NULL);
  g_object_set (label, "margin", 6, NULL); /* omg, why do we need to do this?! */
  gtk_label_set_xalign (GTK_LABEL (label), 0);
  gtk_container_add (GTK_CONTAINER (item), label);
}

static void
bind_listitem (GtkListItem *item, gpointer user_data)
{
  GtkAward *award = gtk_list_item_get_item (item);

  gtk_label_set_text (GTK_LABEL (gtk_bin_get_child (GTK_BIN (item))),
                      gtk_award_get_title (award));
}
#endif

GtkWidget *
do_awardview (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *sw, *listview;
      GListModel *list;

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Awards");
      gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_container_add (GTK_CONTAINER (window), sw);

      listview = gtk_list_view_new ();
#if 0
      gtk_list_view_set_functions (GTK_LIST_VIEW (listview),
                                   create_listitem,
                                   bind_listitem,
                                   NULL, NULL);
#else
      gtk_list_view_set_factory_from_resource (GTK_LIST_VIEW (listview),
                                               "/awardview/awardlistitem.ui");
#endif
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
