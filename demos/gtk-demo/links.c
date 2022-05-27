/* Links
 *
 * GtkLabel can show hyperlinks. The default action is to call
 * gtk_show_uri() on their URI, but it is possible to override
 * this with a custom handler.
 */

#include <gtk/gtk.h>

static void
response_cb (GtkWidget *dialog,
             int        response_id,
             gpointer   data)
{
  gtk_window_destroy (GTK_WINDOW (dialog));
}

static gboolean
activate_link (GtkWidget   *label,
               const char *uri,
               gpointer     data)
{
  if (g_strcmp0 (uri, "keynav") == 0)
    {
      GtkWidget *dialog;
      GtkWidget *parent;

      parent = GTK_WIDGET (gtk_widget_get_root (label));
      dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (parent),
                 GTK_DIALOG_DESTROY_WITH_PARENT,
                 GTK_MESSAGE_INFO,
                 GTK_BUTTONS_OK,
                 "Keyboard navigation");
      gtk_message_dialog_format_secondary_markup (GTK_MESSAGE_DIALOG (dialog),
                 "The term <i>keynav</i> is a shorthand for "
                 "keyboard navigation and refers to the process of using "
                 "a program (exclusively) via keyboard input.");
      gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);

      gtk_window_present (GTK_WINDOW (dialog));
      g_signal_connect (dialog, "response", G_CALLBACK (response_cb), NULL);

      return TRUE;
    }

  return FALSE;
}

GtkWidget *
do_links (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *label;

  if (!window)
    {
      window = gtk_window_new ();
      gtk_window_set_display (GTK_WINDOW (window),
                              gtk_widget_get_display (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Links");
      gtk_window_set_resizable (GTK_WINDOW (window), FALSE);
      g_object_add_weak_pointer (G_OBJECT (window), (gpointer *)&window);

      label = gtk_label_new ("Some <a href=\"http://en.wikipedia.org/wiki/Text\""
                             "title=\"plain text\">text</a> may be marked up "
                             "as hyperlinks, which can be clicked "
                             "or activated via <a href=\"keynav\">keynav</a> "
                             "and they work fine with other markup, like when "
                             "linking to <a href=\"http://www.flathub.org/\"><b>"
                             "<span letter_spacing=\"1024\" underline=\"none\" color=\"pink\" background=\"darkslategray\">Flathub</span>"
                             "</b></a>.");
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      gtk_label_set_max_width_chars (GTK_LABEL (label), 40);
      gtk_label_set_wrap (GTK_LABEL (label), TRUE);
      gtk_label_set_wrap_mode (GTK_LABEL (label), PANGO_WRAP_WORD);
      g_signal_connect (label, "activate-link", G_CALLBACK (activate_link), NULL);
      gtk_widget_set_margin_start (label, 20);
      gtk_widget_set_margin_end (label, 20);
      gtk_widget_set_margin_top (label, 20);
      gtk_widget_set_margin_bottom (label, 20);
      gtk_window_set_child (GTK_WINDOW (window), label);
      gtk_widget_show (label);
    }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_window_destroy (GTK_WINDOW (window));

  return window;
}
