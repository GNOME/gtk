/* Entry/Delayed Search Entry
 *
 * GtkSearchEntry sets up GtkEntries ready for search. Search entries
 * have their "changed" signal delayed and should be used
 * when the searched operation is slow such as loads of entries
 * to search, or online searches.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
search_entry_destroyed (GtkWidget  *widget)
{
  window = NULL;
}

static void
search_changed_cb (GtkSearchEntry *entry,
                   GtkLabel       *result_label)
{
  const char *text;
  text = gtk_entry_get_text (GTK_ENTRY (entry));
  g_message ("search changed: %s", text);
  gtk_label_set_text (result_label, text ? text : "");
}

GtkWidget *
do_search_entry2 (GtkWidget *do_widget)
{
  GtkWidget *content_area;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_dialog_new_with_buttons ("Search Entry #2",
                                            GTK_WINDOW (do_widget),
                                            0,
                                            GTK_STOCK_CLOSE,
                                            GTK_RESPONSE_NONE,
                                            NULL);
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

      g_signal_connect (window, "response",
                        G_CALLBACK (gtk_widget_destroy), NULL);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (search_entry_destroyed), &window);

      content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label), "Search entry demo #2");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      /* Create our entry */
      entry = gtk_search_entry_new ();
      gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

      /* Result */
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);

      label = gtk_label_new ("Result:");
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

      label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

      g_signal_connect (entry, "changed",
                        G_CALLBACK (search_changed_cb), label);

      /* Give the focus to the close button */
      button = gtk_dialog_get_widget_for_response (GTK_DIALOG (window), GTK_RESPONSE_NONE);
      gtk_widget_grab_focus (button);
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
