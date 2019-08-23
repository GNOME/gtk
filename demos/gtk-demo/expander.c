/* Expander
 *
 * GtkExpander allows to provide additional content that is initially hidden.
 * This is also known as "disclosure triangle".
 *
 * This example also shows how to make the window resizable only if the expander
 * is expanded.
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static GtkWidget *window = NULL;

static void
response_cb (GtkDialog *dialog, gint response_id)
{
  gtk_widget_destroy (window);
  window = NULL;
}

static void
expander_cb (GtkExpander *expander, GParamSpec *pspec, GtkWindow *dialog)
{
  gtk_window_set_resizable (dialog, gtk_expander_get_expanded (expander));
}

GtkWidget *
do_expander (GtkWidget *do_widget)
{
  GtkWidget *toplevel;
  GtkWidget *area;
  GtkWidget *expander;
  GtkWidget *label;
  GtkWidget *sw;
  GtkWidget *tv;
  GtkTextBuffer *buffer;

  if (!window)
    {
      toplevel = GTK_WIDGET (gtk_widget_get_root (do_widget));
      window = gtk_message_dialog_new_with_markup (GTK_WINDOW (toplevel),
                                                   0,
                                                   GTK_MESSAGE_ERROR,
                                                   GTK_BUTTONS_CLOSE,
                                                   "<big><b>%s</b></big>",
                                                   "Something went wrong");
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (window),
                                                "Here are some more details "
                                                "but not the full story.");

      area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (window));

      label = gtk_widget_get_last_child (area);
      gtk_label_set_wrap (GTK_LABEL (label), FALSE);
      gtk_widget_set_vexpand (label, FALSE);

      expander = gtk_expander_new ("Details:");
      gtk_widget_set_vexpand (expander, TRUE);
      sw = gtk_scrolled_window_new (NULL, NULL);
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (sw), 100);
      gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (sw), GTK_SHADOW_IN);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);

      tv = gtk_text_view_new ();
      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));
      gtk_text_view_set_editable (GTK_TEXT_VIEW (tv), FALSE);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (tv), GTK_WRAP_WORD);
      gtk_text_buffer_set_text (GTK_TEXT_BUFFER (buffer),
                                "Finally, the full story with all details. "
                                "And all the inside information, including "
                                "error codes, etc etc. Pages of information, "
                                "you might have to scroll down to read it all, "
                                "or even resize the window - it works !\n"
                                "A second paragraph will contain even more "
                                "innuendo, just to make you scroll down or "
                                "resize the window. Do it already !", -1);
      gtk_container_add (GTK_CONTAINER (sw), tv);
      gtk_container_add (GTK_CONTAINER (expander), sw);
      gtk_container_add (GTK_CONTAINER (area), expander);
      g_signal_connect (expander, "notify::expanded",
                        G_CALLBACK (expander_cb), window);

      g_signal_connect (window, "response", G_CALLBACK (response_cb), NULL);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
