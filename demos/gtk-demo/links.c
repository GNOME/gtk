/* Links
 *
 * GtkLabel can show hyperlinks. The default action is to call
 * gtk_show_uri() on their URI, but it is possible to override
 * this with a custom handler.
 */

#include <gtk/gtk.h>

static void
response_cb (GtkWidget *dialog,
             gint       response_id,
             gpointer   data)
{
  gtk_widget_destroy (dialog);
}

static gboolean
activate_link (GtkWidget   *label,
               const gchar *uri,
               gpointer     data)
{
  if (g_strcmp0 (uri, "keynav") == 0)
    {
      GtkWidget *dialog;
      GtkWidget *parent;

      parent = gtk_widget_get_toplevel (label);
      dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW (parent),
                 GTK_DIALOG_DESTROY_WITH_PARENT,
                 GTK_MESSAGE_INFO,
                 GTK_BUTTONS_OK,
                 "The term <i>keynav</i> is a shorthand for "
                 "keyboard navigation and refers to the process of using "
                 "a program (exclusively) via keyboard input.");

      gtk_window_present (GTK_WINDOW (dialog));
      g_signal_connect (dialog, "response", G_CALLBACK (response_cb), NULL);

      return TRUE;
    }

  return FALSE;
}

static GtkWidget *window = NULL;

GtkWidget *
do_links (GtkWidget *do_widget)
{
  GtkWidget *label;

  if (!window)
    {
      window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
      gtk_window_set_screen (GTK_WINDOW (window),
                             gtk_widget_get_screen (do_widget));
      gtk_window_set_title (GTK_WINDOW (window), "Links");
      gtk_container_set_border_width (GTK_CONTAINER (window), 12);
      g_signal_connect (window, "destroy",
                        G_CALLBACK (gtk_widget_destroyed), &window);

      label = gtk_label_new ("Some <a href=\"http://en.wikipedia.org/wiki/Text\""
                             "title=\"plain text\">text</a> may be marked up\n"
                             "as hyperlinks, which can be clicked\n"
                             "or activated via <a href=\"keynav\">keynav</a>\n"
                             "and they work fine with other markup, like when\n"
                             "searching on <a href=\"http://www.google.com/\">"
                             "<span color=\"#0266C8\">G</span><span color=\"#F90101\">o</span>"
                             "<span color=\"#F2B50F\">o</span><span color=\"#0266C8\">g</span>"
                             "<span color=\"#00933B\">l</span><span color=\"#F90101\">e</span>"
                             "</a>.");
      gtk_label_set_use_markup (GTK_LABEL (label), TRUE);
      g_signal_connect (label, "activate-link", G_CALLBACK (activate_link), NULL);
      gtk_container_add (GTK_CONTAINER (window), label);
      gtk_widget_show (label);
    }

  if (!gtk_widget_get_visible (window))
    {
      gtk_widget_show (window);
    }
  else
    {
      gtk_widget_destroy (window);
      window = NULL;
    }

  return window;
}
