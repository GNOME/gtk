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
  text = gtk_editable_get_text (GTK_EDITABLE (entry));
  g_message ("search changed: %s", text);
  gtk_label_set_text (result_label, text ? text : "");
}

static void
changed_cb (GtkEditable *editable)
{
  const char *text;
  text = gtk_editable_get_text (GTK_EDITABLE (editable));
  g_message ("changed: %s", text);
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
      window = gtk_window_new ();
      gtk_window_set_title (GTK_WINDOW (window), "Delayed Search Entry");
      gtk_window_set_transient_for (GTK_WINDOW (window), GTK_WINDOW (do_widget));
      gtk_window_set_resizable (GTK_WINDOW (window), TRUE);
      gtk_widget_set_size_request (window, 200, -1);

      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      entry = gtk_search_entry_new ();
      container = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_widget_set_halign (container, GTK_ALIGN_CENTER);
      gtk_container_add (GTK_CONTAINER (container), entry);
      searchbar = gtk_search_bar_new ();
      gtk_search_bar_connect_entry (GTK_SEARCH_BAR (searchbar), GTK_EDITABLE (entry));
      gtk_search_bar_set_show_close_button (GTK_SEARCH_BAR (searchbar), FALSE);
      gtk_container_add (GTK_CONTAINER (searchbar), container);
      gtk_container_add (GTK_CONTAINER (vbox), searchbar);

      /* Hook the search bar to key presses */
      gtk_search_bar_set_key_capture_widget (GTK_SEARCH_BAR (searchbar), window);

      /* Help */
      label = gtk_label_new ("Start Typing to search");
      gtk_container_add (GTK_CONTAINER (vbox), label);

      /* Toggle button */
      button = gtk_toggle_button_new_with_label ("Search");
      g_object_bind_property (button, "active",
                              searchbar, "search-mode-enabled",
                              G_BINDING_BIDIRECTIONAL);
      gtk_container_add (GTK_CONTAINER (vbox), button);

      /* Result */
      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_container_add (GTK_CONTAINER (vbox), hbox);

      label = gtk_label_new ("Result:");
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_widget_set_margin_start (label, 6);
      gtk_container_add (GTK_CONTAINER (hbox), label);

      label = gtk_label_new ("");
      gtk_container_add (GTK_CONTAINER (hbox), label);

      g_signal_connect (entry, "search-changed",
                        G_CALLBACK (search_changed_cb), label);
      g_signal_connect (entry, "changed",
                        G_CALLBACK (changed_cb), label);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 10);
      gtk_container_add (GTK_CONTAINER (vbox), hbox);

      label = gtk_label_new ("Signal:");
      gtk_label_set_xalign (GTK_LABEL (label), 0.0);
      gtk_widget_set_margin_start (label, 6);
      gtk_container_add (GTK_CONTAINER (hbox), label);

      label = gtk_label_new ("");
      gtk_container_add (GTK_CONTAINER (hbox), label);

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
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
