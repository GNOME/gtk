/* Spinner
 *
 * GtkSpinner allows to show that background activity is on-going.
 *
 */

#include <gtk/gtk.h>

static GtkWidget *window = NULL;
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
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *button;
  GtkWidget *spinner;

  if (!window)
  {
    window = gtk_dialog_new_with_buttons ("GtkSpinner",
                                          GTK_WINDOW (do_widget),
                                          0,
                                          GTK_STOCK_CLOSE,
                                          GTK_RESPONSE_NONE,
                                          NULL);
    gtk_window_set_resizable (GTK_WINDOW (window), FALSE);

    g_signal_connect (window, "response",
                      G_CALLBACK (gtk_widget_destroy), NULL);
    g_signal_connect (window, "destroy",
                      G_CALLBACK (gtk_widget_destroyed), &window);

    vbox = gtk_vbox_new (FALSE, 5);
    gtk_box_pack_start (GTK_BOX (gtk_dialog_get_content_area (GTK_DIALOG (window))), vbox, TRUE, TRUE, 0);
    gtk_container_set_border_width (GTK_CONTAINER (vbox), 5);

    /* Sensitive */
    hbox = gtk_hbox_new (FALSE, 5);
    spinner = gtk_spinner_new ();
    gtk_container_add (GTK_CONTAINER (hbox), spinner);
    gtk_container_add (GTK_CONTAINER (hbox), gtk_entry_new ());
    gtk_container_add (GTK_CONTAINER (vbox), hbox);
    spinner_sensitive = spinner;

    /* Disabled */
    hbox = gtk_hbox_new (FALSE, 5);
    spinner = gtk_spinner_new ();
    gtk_container_add (GTK_CONTAINER (hbox), spinner);
    gtk_container_add (GTK_CONTAINER (hbox), gtk_entry_new ());
    gtk_container_add (GTK_CONTAINER (vbox), hbox);
    spinner_unsensitive = spinner;
    gtk_widget_set_sensitive (hbox, FALSE);

    button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_PLAY);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_play_clicked), spinner);
    gtk_container_add (GTK_CONTAINER (vbox), button);

    button = gtk_button_new_from_stock (GTK_STOCK_MEDIA_STOP);
    g_signal_connect (G_OBJECT (button), "clicked",
                      G_CALLBACK (on_stop_clicked), spinner);
    gtk_container_add (GTK_CONTAINER (vbox), button);

    /* Start by default to test for:
     * https://bugzilla.gnome.org/show_bug.cgi?id=598496 */
    on_play_clicked (NULL, NULL);
  }

  if (!gtk_widget_get_visible (window))
    gtk_widget_show_all (window);
  else
    gtk_widget_destroy (window);

  return window;
}


