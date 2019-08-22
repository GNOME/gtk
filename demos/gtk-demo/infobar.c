/* Info Bars
 *
 * Info bar widgets are used to report important messages to the user.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static void
on_bar_response (GtkInfoBar *info_bar,
                 gint        response_id,
                 gpointer    user_data)
{
  GtkWidget *dialog;
  GtkWidget *window;

  if (response_id == GTK_RESPONSE_CLOSE)
    {
      gtk_info_bar_set_revealed (info_bar, FALSE);
      return;
    }

  window = GTK_WIDGET (gtk_widget_get_root (GTK_WIDGET (info_bar)));
  dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                   GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_INFO,
                                   GTK_BUTTONS_OK,
                                   "You clicked a button on an info bar");
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
                                            "Your response has id %d", response_id);

  g_signal_connect_swapped (dialog,
                            "response",
                            G_CALLBACK (gtk_widget_destroy),
                            dialog);

  gtk_widget_show (dialog);
}

GtkWidget *
do_infobar (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *frame;
  GtkWidget *bar;
  GtkWidget *vbox;
  GtkWidget *vbox2;
  GtkWidget *label;
  GtkWidget *actions;
  GtkWidget *button;

  if (!window)
    {
      actions = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Info Bars");

      g_signal_connect (window, "destroy", G_CALLBACK (gtk_widget_destroyed), &window);

      vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      g_object_set (vbox, "margin", 8, NULL);
      gtk_container_add (GTK_CONTAINER (window), vbox);

      bar = gtk_info_bar_new ();
      gtk_container_add (GTK_CONTAINER (vbox), bar);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_INFO);
      label = gtk_label_new ("This is an info bar with message type GTK_MESSAGE_INFO");
      gtk_label_set_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_xalign (GTK_LABEL (label), 0);
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label);

      button = gtk_toggle_button_new_with_label ("Message");
      g_object_bind_property (bar, "revealed", button, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      gtk_container_add (GTK_CONTAINER (actions), button);

      bar = gtk_info_bar_new ();
      gtk_container_add (GTK_CONTAINER (vbox), bar);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_WARNING);
      label = gtk_label_new ("This is an info bar with message type GTK_MESSAGE_WARNING");
      gtk_label_set_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_xalign (GTK_LABEL (label), 0);
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label);

      button = gtk_toggle_button_new_with_label ("Warning");
      g_object_bind_property (bar, "revealed", button, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      gtk_container_add (GTK_CONTAINER (actions), button);

      bar = gtk_info_bar_new_with_buttons (_("_OK"), GTK_RESPONSE_OK, NULL);
      gtk_info_bar_set_show_close_button (GTK_INFO_BAR (bar), TRUE);
      g_signal_connect (bar, "response", G_CALLBACK (on_bar_response), window);
      gtk_container_add (GTK_CONTAINER (vbox), bar);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_QUESTION);
      label = gtk_label_new ("This is an info bar with message type GTK_MESSAGE_QUESTION");
      gtk_label_set_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_xalign (GTK_LABEL (label), 0);
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label);

      button = gtk_toggle_button_new_with_label ("Question");
      g_object_bind_property (bar, "revealed", button, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      gtk_container_add (GTK_CONTAINER (actions), button);

      bar = gtk_info_bar_new ();
      gtk_container_add (GTK_CONTAINER (vbox), bar);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_ERROR);
      label = gtk_label_new ("This is an info bar with message type GTK_MESSAGE_ERROR");
      gtk_label_set_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_xalign (GTK_LABEL (label), 0);
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label);

      button = gtk_toggle_button_new_with_label ("Error");
      g_object_bind_property (bar, "revealed", button, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);

      gtk_container_add (GTK_CONTAINER (actions), button);

      bar = gtk_info_bar_new ();
      gtk_container_add (GTK_CONTAINER (vbox), bar);
      gtk_info_bar_set_message_type (GTK_INFO_BAR (bar), GTK_MESSAGE_OTHER);
      label = gtk_label_new ("This is an info bar with message type GTK_MESSAGE_OTHER");
      gtk_label_set_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_xalign (GTK_LABEL (label), 0);
      gtk_container_add (GTK_CONTAINER (gtk_info_bar_get_content_area (GTK_INFO_BAR (bar))), label);

      button = gtk_toggle_button_new_with_label ("Other");
      g_object_bind_property (bar, "revealed", button, "active", G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
      gtk_container_add (GTK_CONTAINER (actions), button);

      frame = gtk_frame_new ("Info bars");
      gtk_widget_set_margin_top (frame, 8);
      gtk_widget_set_margin_bottom (frame, 8);
      gtk_container_add (GTK_CONTAINER (vbox), frame);

      vbox2 = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
      g_object_set (vbox2, "margin", 8, NULL);
      gtk_container_add (GTK_CONTAINER (frame), vbox2);

      /* Standard message dialog */
      label = gtk_label_new ("An example of different info bars");
      gtk_container_add (GTK_CONTAINER (vbox2), label);

      gtk_container_add (GTK_CONTAINER (vbox2), actions);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
