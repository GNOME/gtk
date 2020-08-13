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
response_cb (GtkDialog *dialog, int response_id)
{
  gtk_window_destroy (GTK_WINDOW (window));
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
  GtkTextIter start;
  GtkTextIter end;
  GtkTextTag *tag;
  GdkPaintable *paintable;

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
      sw = gtk_scrolled_window_new ();
      gtk_scrolled_window_set_min_content_height (GTK_SCROLLED_WINDOW (sw), 100);
      gtk_scrolled_window_set_has_frame (GTK_SCROLLED_WINDOW (sw), TRUE);
      gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (sw),
                                      GTK_POLICY_NEVER,
                                      GTK_POLICY_AUTOMATIC);
      gtk_scrolled_window_set_propagate_natural_height (GTK_SCROLLED_WINDOW (sw), TRUE);
      gtk_widget_set_vexpand (sw, TRUE);

      tv = gtk_text_view_new ();

      g_object_set (tv,
                    "left-margin", 10,
                    "right-margin", 10,
                    "top-margin", 10,
                    "bottom-margin", 10,
                    NULL);

      buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (tv));
      gtk_text_view_set_editable (GTK_TEXT_VIEW (tv), FALSE);
      gtk_text_view_set_cursor_visible (GTK_TEXT_VIEW (tv), FALSE);
      gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (tv), GTK_WRAP_WORD);
      gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (tv), 2);
      gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (tv), 2);

      gtk_text_buffer_set_text (buffer,
                                "Finally, the full story with all details. "
                                "And all the inside information, including "
                                "error codes, etc etc. Pages of information, "
                                "you might have to scroll down to read it all, "
                                "or even resize the window - it works !\n"
                                "A second paragraph will contain even more "
                                "innuendo, just to make you scroll down or "
                                "resize the window.\n"
                                "Do it already!\n", -1);

      gtk_text_buffer_get_end_iter (buffer, &start);
      paintable = GDK_PAINTABLE (gdk_texture_new_from_resource ("/cursors/images/gtk_logo_cursor.png"));
      gtk_text_buffer_insert_paintable (buffer, &start, paintable);
      g_object_unref (paintable);
      gtk_text_iter_backward_char (&start);

      gtk_text_buffer_get_end_iter (buffer, &end);
      tag = gtk_text_buffer_create_tag (buffer, NULL,
                                        "pixels-above-lines", 200,
                                        "justification", GTK_JUSTIFY_RIGHT,
                                        NULL);
      gtk_text_buffer_apply_tag (buffer, tag, &start, &end);

      gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (sw), tv);
      gtk_expander_set_child (GTK_EXPANDER (expander), sw);
      gtk_box_append (GTK_BOX (area), expander);
      g_signal_connect (expander, "notify::expanded",
                        G_CALLBACK (expander_cb), window);

      g_signal_connect (window, "response", G_CALLBACK (response_cb), NULL);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
