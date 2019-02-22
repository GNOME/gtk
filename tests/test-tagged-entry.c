#include <gtk/gtk.h>

static GtkEntryTag *toggle_tag;

static void
on_tag_clicked (GtkEntryTag *tag,
                gpointer useless)
{
  g_print ("tag clicked: %s\n", gtk_entry_tag_get_label (tag));
}

static void
on_tag_button_clicked (GtkEntryTag *tag,
                       GtkTaggedEntry *entry)
{
  g_print ("tag button clicked: %s\n", gtk_entry_tag_get_label (tag));
  gtk_tagged_entry_remove_tag (entry, tag);
}

static void
on_toggle_visible (GtkButton *button,
                   GtkWidget *entry)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  g_print ("%s tagged entry\n", active ? "show" : "hide");
  gtk_widget_set_visible (entry, active);
}

static void
on_toggle_tag (GtkButton *button,
               GtkTaggedEntry *entry)
{
  gboolean active;

  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));

  if (active)
    {
      g_print ("adding tag 'Toggle Tag'\n");
      gtk_tagged_entry_insert_tag (entry, toggle_tag, 0);
    }
  else
    {
      g_print ("removing tag 'Toggle Tag'\n");
      gtk_tagged_entry_remove_tag (entry, toggle_tag);
    }
}

gint
main (gint argc,
      gchar ** argv)
{
  GtkWidget *window, *box, *entry, *toggle_visible_button, *toggle_tag_button;
  GtkEntryTag *tag;

  gtk_init ();

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_size_request (window, 300, 20);

  box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add (GTK_CONTAINER (window), box);

  entry = GTK_WIDGET (gtk_tagged_entry_new ());
  gtk_container_add (GTK_CONTAINER (box), entry);

  tag = gtk_entry_tag_new ("Blah1");
  g_object_set (tag, "has-close-button", TRUE, NULL);
  g_signal_connect(tag, "clicked", G_CALLBACK (on_tag_clicked), NULL);
  g_signal_connect(tag, "button-clicked", G_CALLBACK (on_tag_button_clicked), entry);

  gtk_tagged_entry_add_tag (GTK_TAGGED_ENTRY (entry), tag);

  tag = gtk_entry_tag_new ("Blah2");
  g_object_set (tag, "has-close-button", TRUE, NULL);
  g_signal_connect(tag, "clicked", G_CALLBACK (on_tag_clicked), NULL);
  g_signal_connect(tag, "button-clicked", G_CALLBACK (on_tag_button_clicked), entry);
  gtk_tagged_entry_insert_tag (GTK_TAGGED_ENTRY (entry), tag, -1);

  tag = gtk_entry_tag_new ("Blah3");
  g_signal_connect(tag, "clicked", G_CALLBACK (on_tag_clicked), NULL);
  g_signal_connect(tag, "button-clicked", G_CALLBACK (on_tag_button_clicked), entry);
  gtk_tagged_entry_insert_tag (GTK_TAGGED_ENTRY (entry), tag, 0);

  toggle_visible_button = gtk_toggle_button_new_with_label ("Visible");
  gtk_widget_set_vexpand (toggle_visible_button, TRUE);
  gtk_widget_set_valign (toggle_visible_button, GTK_ALIGN_END);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle_visible_button), TRUE);
  g_signal_connect (toggle_visible_button, "toggled",
                    G_CALLBACK (on_toggle_visible), entry);
  gtk_container_add (GTK_CONTAINER (box), toggle_visible_button);

  gtk_tagged_entry_add_tag (entry, g_object_new (GTK_TYPE_SPINNER, "active", TRUE, NULL));
  toggle_tag = gtk_entry_tag_new ("Toggle Tag");
  g_signal_connect(toggle_tag, "clicked", G_CALLBACK (on_tag_clicked), NULL);
  g_signal_connect(toggle_tag, "button-clicked", G_CALLBACK (on_tag_button_clicked), NULL);
  g_object_ref_sink (toggle_tag);

  toggle_tag_button = gtk_toggle_button_new_with_label ("Toggle Tag");
  g_signal_connect (toggle_tag_button, "toggled",
                    G_CALLBACK (on_toggle_tag), entry);
  gtk_container_add (GTK_CONTAINER (box), toggle_tag_button);

  gtk_widget_show (window);
  gtk_main ();

  gtk_widget_destroy (window);

  return 0;
}
