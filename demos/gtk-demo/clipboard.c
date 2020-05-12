/* Clipboard
 *
 * GdkClipboard is used for clipboard handling. This demo shows how to
 * copy and paste text to and from the clipboard.
 *
 * It also shows how to transfer images via the clipboard or via
 * drag-and-drop, and how to make clipboard contents persist after
 * the application exits. Clipboard persistence requires a clipboard
 * manager to run.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include "demoimage.h"

static GtkWidget *window = NULL;

static void
copy_button_clicked (GtkWidget *button,
                     gpointer   user_data)
{
  GtkWidget *entry;
  GdkClipboard *clipboard;

  entry = GTK_WIDGET (user_data);

  /* Get the clipboard object */
  clipboard = gtk_widget_get_clipboard (entry);

  /* Set clipboard text */
  gdk_clipboard_set_text (clipboard, gtk_editable_get_text (GTK_EDITABLE (entry)));
}

static void
paste_received (GObject      *source_object,
                GAsyncResult *result,
                gpointer      user_data)
{
  GdkClipboard *clipboard;
  GtkWidget *entry;
  char *text;
  GError *error = NULL;

  clipboard = GDK_CLIPBOARD (source_object);
  entry = GTK_WIDGET (user_data);

  /* Get the resulting text of the read operation */
  text = gdk_clipboard_read_text_finish (clipboard, result, &error);

  if (text)
    {
      /* Set the entry text */
      gtk_editable_set_text (GTK_EDITABLE (entry), text);
      g_free (text);
    }
  else
    {
      GtkWidget *dialog;

      /* Show an error about why pasting failed.
       * Usually you probably want to ignore such failures,
       * but for demonstration purposes, we show the error.
       */
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "Could not paste text: %s",
                                       error->message);
      g_signal_connect (dialog, "response",
                        G_CALLBACK (gtk_window_destroy), NULL);
      gtk_widget_show (dialog);

      g_error_free (error);
    }
}

static void
paste_button_clicked (GtkWidget *button,
                      gpointer   user_data)
{
  GtkWidget *entry;
  GdkClipboard *clipboard;

  entry = GTK_WIDGET (user_data);

  /* Get the clipboard object */
  clipboard = gtk_widget_get_clipboard (entry);

  /* Request the contents of the clipboard, contents_received will be
     called when we do get the contents.
   */
  gdk_clipboard_read_text_async (clipboard, NULL, paste_received, entry);
}

GtkWidget *
do_clipboard (GtkWidget *do_widget)
{
  if (!window)
    {
      GtkWidget *vbox, *hbox;
      GtkWidget *label;
      GtkWidget *entry, *button;
      GtkWidget *image;

      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Clipboard");
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_set_margin_start (vbox, 8);
      gtk_widget_set_margin_end (vbox, 8);
      gtk_widget_set_margin_top (vbox, 8);
      gtk_widget_set_margin_bottom (vbox, 8);

      gtk_window_set_child (GTK_WINDOW (window), vbox);

      label = gtk_label_new ("\"Copy\" will copy the text\nin the entry to the clipboard");

      gtk_box_append (GTK_BOX (vbox), label);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_widget_set_margin_start (hbox, 8);
      gtk_widget_set_margin_end (hbox, 8);
      gtk_widget_set_margin_top (hbox, 8);
      gtk_widget_set_margin_bottom (hbox, 8);
      gtk_box_append (GTK_BOX (vbox), hbox);

      /* Create the first entry */
      entry = gtk_entry_new ();
      gtk_box_append (GTK_BOX (hbox), entry);

      /* Create the button */
      button = gtk_button_new_with_mnemonic (_("_Copy"));
      gtk_box_append (GTK_BOX (hbox), button);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (copy_button_clicked), entry);

      label = gtk_label_new ("\"Paste\" will paste the text from the clipboard to the entry");
      gtk_box_append (GTK_BOX (vbox), label);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_widget_set_margin_start (hbox, 8);
      gtk_widget_set_margin_end (hbox, 8);
      gtk_widget_set_margin_top (hbox, 8);
      gtk_widget_set_margin_bottom (hbox, 8);
      gtk_box_append (GTK_BOX (vbox), hbox);

      /* Create the second entry */
      entry = gtk_entry_new ();
      gtk_box_append (GTK_BOX (hbox), entry);

      /* Create the button */
      button = gtk_button_new_with_mnemonic (_("_Paste"));
      gtk_box_append (GTK_BOX (hbox), button);
      g_signal_connect (button, "clicked",
                        G_CALLBACK (paste_button_clicked), entry);

      label = gtk_label_new ("Images can be transferred via the clipboard, too");
      gtk_box_append (GTK_BOX (vbox), label);

      hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
      gtk_widget_set_margin_start (hbox, 8);
      gtk_widget_set_margin_end (hbox, 8);
      gtk_widget_set_margin_top (hbox, 8);
      gtk_widget_set_margin_bottom (hbox, 8);
      gtk_box_append (GTK_BOX (vbox), hbox);

      /* Create the first image */
      image = demo_image_new ("dialog-warning");
      gtk_box_append (GTK_BOX (hbox), image);

      /* Create the second image */
      image = demo_image_new ("process-stop");
      gtk_box_append (GTK_BOX (hbox), image);

      /* Create the third image */
      image = demo_image_new ("weather-clear");
      gtk_box_append (GTK_BOX (hbox), image);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
