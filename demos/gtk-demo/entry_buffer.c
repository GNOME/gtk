/* Entry/Entry Buffer
 *
 * GtkEntryBuffer provides the text content in a GtkEntry.
 * Applications can provide their own buffer implementation,
 * e.g. to provide secure handling for passwords in memory.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

GtkWidget *
do_entry_buffer (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *vbox;
  GtkWidget *label;
  GtkWidget *entry;
  GtkEntryBuffer *buffer;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Entry Buffer");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_add (GTK_CONTAINER (window), vbox);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

      label = gtk_label_new (NULL);
      gtk_label_set_markup (GTK_LABEL (label),
                            "Entries share a buffer. Typing in one is reflected in the other.");
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
