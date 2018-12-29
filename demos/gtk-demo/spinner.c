/* Spinner
 *
 * GtkSpinner allows to show that background activity is on-going.
 *
 */

#include <glib/gi18n.h>
#include <gtk/gtk.h>

static GtkWidget *spinner_sensitive = NULL;
static GtkWidget *spinner_unsensitive = NULL;

static void
on_play_clicked (GtkButton *button, gpointer user_data)
{
  gtk_spinner_start (GTK_SPINNER (spinner_sensitive));
  gtk_spinner_start (GTK_SPINNER (spinner_unsensitive));
}

static void
on_stop_clicked (GtkButton *button, gpointer user_data)
{
  gtk_spinner_stop (GTK_SPINNER (spinner_sensitive));
  gtk_spinner_stop (GTK_SPINNER (spinner_unsensitive));
}

GtkWidget *
do_spinner (GtkWidget *do_widget)
{
  static GtkWidget *window = NULL;
  GtkWidget *content_area;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *spinner;

  if (!window)
  {
    window = gtk_dialog_new_with_buttons ("Spinner",
                                          GTK_WINDOW (do_widget),
                                          0,
                                          _("_Close"),
                                          GTK_RESPONSE_NONE,
                                          NULL);
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

    g_signal_connect (window, "response",
                      G_CALLBACK (gtk_widget_destroy), NULL);
    g_signal_connect (window, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &window);

    content_area = gtk_dialog_get_content_area (GTK_DIALOG (window));

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
    g_object_set (vbox, "margin", 5, NULL);
    gtk_container_add (GTK_CONTAINER (content_area), vbox);

    /* Sensitive */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    spinner = gtk_spinner_new ();
    gtk_container_add (GTK_CONTAINER (hbox), spinner);
    gtk_container_add (GTK_CONTAINER (hbox), gtk_entry_new ());
    gtk_container_add (GTK_CONTAINER (vbox), hbox);
    spinner_sensitive = spinner;

    /* Disabled */
    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
    spinner = gtk_spinner_new ();
    gtk_container_add (GTK_CONTAINER (hbox), spinner);
    gtk_container_add (GTK_CONTAINER (hbox), gtk_entry_new ());
    gtk_container_add (GTK_CONTAINER (vbox), hbox);
    spinner_unsensitive = spinner;
    gtk_widget_set_sensitive (hbox, FALSE);

    button = gtk_button_new_with_label (_("Play"));
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_play_clicked), spinner);
    gtk_container_add (GTK_CONTAINER (vbox), button);

    button = gtk_button_new_with_label (_("Stop"));
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_stop_clicked), spinner);
    gtk_container_add (GTK_CONTAINER (vbox), button);

    /* Start by default to test for:
     * https://bugzilla.gnome.org/show_bug.cgi?id=598496 */
    on_play_clicked (NULL, NULL);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show (window);
  else
    gtk_widget_destroy (window);

  return window;
}
