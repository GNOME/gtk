/* Entry/Delayed Search Entry
 *
 * GtkSearchEntry sets up GtkEntries ready for search. Search entries
 * have their "changed" signal delayed and should be used
 * when the searched operation is slow such as loads of entries
 * to search, or online searches.
 */

#include <gtk/gtk.h>

static void
search_changed_cb (GtkSearchEntry *entry,
                   GtkLabel       *result_label)
{
  const char *text;
  text = gtk_entry_get_text (GTK_ENTRY (entry));
  g_message ("search changed: %s", text);
  gtk_label_set_text (result_label, text ? text : "");
}

static void
changed_cb (GtkEditable *editable)
{
  const char *text;
  text = gtk_entry_get_text (GTK_ENTRY (editable));
  g_message ("changed: %s", text);
}

static gboolean
window_key_press_event_cb (GtkWidget    *widget,
			   GdkEvent     *event,
			   GtkSearchBar *bar)
{
  return gtk_search_bar_handle_event (bar, event);
}

static void
search_changed (GtkSearchEntry *entry,
                GtkLabel       *label)
{
  gtk_label_set_text (label, "search-changed");
}

static void
next_match (GtkSearchEntry *entry,
            GtkLabel       *label)
{
  gtk_label_set_text (label, "next-match");
}

static void
previous_match (GtkSearchEntry *entry,
                GtkLabel       *label)
{
  gtk_label_set_text (label, "previous-match");
}

static void
stop_search (GtkSearchEntry *entry,
             GtkLabel       *label)
{
  gtk_label_set_text (label, "stop-search");
}

GtkWidget *
do_search_entry2 (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkWidget *container;
  GtkWidget *searchbar;
  GtkWidget *button;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_title (GTK_WINDOW (window), "Delayed Search Entry");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
      gtk_widget_set_size_request (window, 200, -1);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 0);

      entry = gtk_search_entry_new ();
      container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_widget_set_halign (container, GTK_ALIGN_CENTER);
      gtk_box_pack_start (GTK_BOX (container), entry, FALSE, FALSE, 0);
      searchbar = gtk_search_bar_new ();
      gtk_search_bar_connect_entry (GTK_SEARCH_BAR (searchbar), GTK_ENTRY (entry));
      gtk_search_bar_set_show_close_button (GTK_SEARCH_BAR (searchbar), FALSE);
      gtk_container_add (GTK_CONTAINER (searchbar), container);
      gtk_box_pack_start (GTK_BOX (vbox), searchbar, FALSE, FALSE, 0);

      /* Hook the search bar to key presses */
      g_signal_connect (window, "key-press-event",
                        G_CALLBACK (window_key_press_event_cb), searchbar);

      /* Help */
      label = gtk_label_new ("Start Typing to search");
      gtk_box_pack_start (GTK_BOX (vbox), label, TRUE, TRUE, 0);

      /* Toggle button */
      button = gtk_toggle_button_new_with_label ("Search");
      g_object_bind_property (button, "active",
                              searchbar, "search-mode-enabled",
                              G_BINDING_BIDIRECTIONAL);
      gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 0);

      /* Result */
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);

      label = gtk_label_new ("Result:");
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_widget_set_margin_start (label, 6);
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

      label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

      g_signal_connect (entry, "search-changed",
                        G_CALLBACK (search_changed_cb), label);
      g_signal_connect (entry, "changed",
                        G_CALLBACK (changed_cb), label);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 0);

      label = gtk_label_new ("Signal:");
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_widget_set_margin_start (label, 6);
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

      label = gtk_label_new ("");
      gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);

      g_signal_connect (entry, "search-changed",
                        G_CALLBACK (search_changed), label);
      g_signal_connect (entry, "next-match",
                        G_CALLBACK (next_match), label);
      g_signal_connect (entry, "previous-match",
                        G_CALLBACK (previous_match), label);
      g_signal_connect (entry, "stop-search",
                        G_CALLBACK (stop_search), label);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}
