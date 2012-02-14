/* Entry/Entry Buffer
 *
 * GtkEntryBuffer provides the text content in a GtkEntry.
 *
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

GtkWidget *
do_entry_buffer (GtkWidget *do_widget)
{
  GtkWidget *content_area;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkEntryBuffer *buffer;

  if (!window)
  {
    window = gtk_dialog_new_with_buttons ("GtkEntryBuffer",
                                          GTK_WINDOW (do_widget),
                                          0,
                                          GTK_STOCK_CLOSE,
                                          GTK_RESPONSE_NONE,
                                          NULL);
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

    g_signal_connect (window, "response",
                      G_CALLBACK (gtk_widget_destroy), NULL);
    g_signal_connect (window, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &window);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_pack_start (GTK_BOX (content_area), vbox, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

    label = gtk_label_new (NULL);
    gtk_label_set_markup (GTK_LABEL (label), "Entries share a buffer. Typing in one is reflected in the other.");
    gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

    /* Create a buffer */
    buffer = gtk_entry_buffer_new (NULL, 0);

    /* Create our first entry */
    entry = gtk_entry_new_with_buffer (buffer);
    gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

    /* Create the second entry */
    entry = gtk_entry_new_with_buffer (buffer);
    gtk_entry_set_visibility (GTK_ENTRY (entry), FALSE);
    gtk_box_pack_start (GTK_BOX (vbox), entry, FALSE, FALSE, 0);

    g_object_unref (buffer);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}
