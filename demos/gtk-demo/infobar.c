/* Info bar
 *
 * Info bar widgets are used to report important messages to the user.
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
on_bar_response (GtkInfoBar *info_bar,
                 gint        response_id,
                 gpointer    user_data)
{
  GtkWidget *dialog;

  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
				   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
				   GTK_MESSAGE_INFO,
				   GTK_BUTTONS_OK,
				   "You clicked a button on an info bar");
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "Your response has id %d", response_id);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

GtkWidget *
do_infobar (GtkWidget *do_widget)
{
  GtkWidget *frame;
  GtkWidget *bar;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Info Bars");

      g_signal_connect (window, "destroy", G_CALLBACK (gtk_widget_destroyed), &window);
      gtk_container_set_border_width (GTK_CONTAINER (window), 8);

      vbox = gtk_vbox_new (FALSE, 0);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      bar = gtk_info_bar_new ();
      gtk_box_pack_start (GTK_BOX (vbox), bar, FALSE, FALSE, 0);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_INFO);
      label = gtk_label_new ("This is an info bar with message type GTK_MESSAGE_INFO");
      gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label, FALSE, FALSE, 0);

      bar = gtk_info_bar_new ();
      gtk_box_pack_start (GTK_BOX (vbox), bar, FALSE, FALSE, 0);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_WARNING);
      label = gtk_label_new ("This is an info bar with message type GTK_MESSAGE_WARNING");
      gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label, FALSE, FALSE, 0);

      bar = gtk_info_bar_new_with_buttons (GTK_STOCK_OK, GTK_RESPONSE_OK, NULL);
      g_signal_connect (bar, "response", G_CALLBACK (on_bar_response), window);
      gtk_box_pack_start (GTK_BOX (vbox), bar, FALSE, FALSE, 0);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_QUESTION);
      label = gtk_label_new ("This is an info bar with message type GTK_MESSAGE_QUESTION");
      gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label, FALSE, FALSE, 0);

      bar = gtk_info_bar_new ();
      gtk_box_pack_start (GTK_BOX (vbox), bar, FALSE, FALSE, 0);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_ERROR);
      label = gtk_label_new ("This is an info bar with message type GTK_MESSAGE_ERROR");
      gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label, FALSE, FALSE, 0);

      bar = gtk_info_bar_new ();
      gtk_box_pack_start (GTK_BOX (vbox), bar, FALSE, FALSE, 0);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_OTHER);
      label = gtk_label_new ("This is an info bar with message type GTK_MESSAGE_OTHER");
      gtk_box_pack_start (GTK_BOX (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label, FALSE, FALSE, 0);

      frame = gtk_frame_new ("Info bars");
      gtk_box_pack_start (GTK_BOX (vbox), frame, FALSE, FALSE, 8);

      vbox2 = gtk_vbox_new (FALSE, 8);
      gtk_container_set_border_width (GTK_CONTAINER (vbox2), 8);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);

      /* Standard message dialog */
      label = gtk_label_new ("An example of different info bars");
      gtk_box_pack_start (GTK_BOX (vbox2), label, FALSE, FALSE, 0);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show_all (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
