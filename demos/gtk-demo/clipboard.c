/* Clipboard
 *
 * GtkClipboard is used for clipboard handling. This demo shows how to
 * copy and paste text to and from the clipboard.
 */

#include <gtk/gtk.h>
#include <string.h>

static GtkWidget *window = NULL;

void
copy_button_clicked (GtkWidget *button,
		     gpointer   user_data)
{
  GtkWidget *entry;
  GtkClipboard *clipboard;

  entry = GTK_WIDGET (user_data);
  
  /* Get the clipboard object */
  clipboard = gtk_widget_get_clipboard (entry,
					GDK_SELECTION_CLIPBOARD);

  /* Set clipboard text */
  gtk_clipboard_set_text (clipboard, gtk_entry_get_text (GTK_ENTRY (entry)), -1);
}

void
paste_received (GtkClipboard *clipboard,
		const gchar  *text,
		gpointer      user_data)
{
  GtkWidget *entry;

  entry = GTK_WIDGET (user_data);
  
  /* Set the entry text */
  gtk_entry_set_text (GTK_ENTRY (entry), text);
}

void
paste_button_clicked (GtkWidget *button,
		     gpointer   user_data)
{
  GtkWidget *entry;
  GtkClipboard *clipboard;

  entry = GTK_WIDGET (user_data);
  
  /* Get the clipboard object */
  clipboard = gtk_widget_get_clipboard (entry,
					GDK_SELECTION_CLIPBOARD);

  /* Request the contents of the clipboard, contents_received will be
     called when we do get the contents.
   */
  gtk_clipboard_request_text (clipboard,
			      paste_received, entry);
}

GtkWidget *
do_clipboard (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *vbox, *hbox;
      GtkWidget *label;
      GtkWidget *entry, *button;
      
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_set_border_width (GTK_CONTAINER (vbox), 8);
      
      gtk_container_add (GTK_CONTAINER (window), vbox);

      label = gtk_label_new ("\"Copy\" will copy the text\nin the entry to the clipboard");
      
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      hbox = gtk_hbox_new (FALSE, 4);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      /* Create the first entry */
      entry = gtk_entry_new ();
      gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
      
      /* Create the button */
      button = gtk_button_new_from_stock (GTK_STOCK_COPY);
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (copy_button_clicked), entry);

      label = gtk_label_new ("\"Paste\" will paste the text from the clipboard to the entry");
      gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);

      hbox = gtk_hbox_new (FALSE, 4);
      gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
      gtk_box_pack_start (GTK_BOX (vbox), hbox, FALSE, FALSE, 0);

      /* Create the second entry */
      entry = gtk_entry_new ();
      gtk_box_pack_start (GTK_BOX (hbox), entry, TRUE, TRUE, 0);
      
      /* Create the button */
      button = gtk_button_new_from_stock (GTK_STOCK_PASTE);
      gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
      g_signal_connect (button, "clicked",
			G_CALLBACK (paste_button_clicked), entry);
      
    }

  if (!GTK_WIDGET_VISIBLE (window))
    gtk_widget_show_all (window);
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
