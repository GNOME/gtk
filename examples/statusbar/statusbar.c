
#include <stdlib.h>
#include <gtk/gtk.h>
#include <glib.h>

GtkWidget *status_bar;

static void push_item( GtkWidget *widget,
                       gpointer   data )
{
  static int count = 1;
  gchar *buff;

  buff = g_strdup_printf ("Item %d", count++);
  gtk_statusbar_push (GTK_STATUSBAR (status_bar), GPOINTER_TO_INT (data), buff);
  g_free (buff);
}

static void pop_item( GtkWidget *widget,
                      gpointer   data )
{
  gtk_statusbar_pop (GTK_STATUSBAR (status_bar), GPOINTER_TO_INT (data));
}

int main( int   argc,
          char *argv[] )
{

    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *button;

    gint context_id;

    gtk_init (&argc, &argv);

    /* create a new window */
    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_widget_set_size_request (GTK_WIDGET (window), 200, 100);
    gtk_window_set_title (GTK_WINDOW (window), "GTK Statusbar Example");
    g_signal_connect (window, "delete-event",
                      G_CALLBACK (exit), NULL);

    vbox = gtk_vbox_new (FALSE, 1);
    gtk_container_add (GTK_CONTAINER (window), vbox);
    gtk_widget_show (vbox);

    status_bar = gtk_statusbar_new ();
    gtk_box_pack_start (GTK_BOX (vbox), status_bar, TRUE, TRUE, 0);
    gtk_widget_show (status_bar);

    context_id = gtk_statusbar_get_context_id(
                          GTK_STATUSBAR (status_bar), "Statusbar example");

    button = gtk_button_new_with_label ("push item");
    g_signal_connect (button, "clicked",
                      G_CALLBACK (push_item), GINT_TO_POINTER (context_id));
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 2);
    gtk_widget_show (button);

    button = gtk_button_new_with_label ("pop last item");
    g_signal_connect (button, "clicked",
                      G_CALLBACK (pop_item), GINT_TO_POINTER (context_id));
    gtk_box_pack_start (GTK_BOX (vbox), button, TRUE, TRUE, 2);
    gtk_widget_show (button);

    /* always display the window as the last step so it all splashes on
     * the screen at once. */
    gtk_widget_show (window);

    gtk_main ();

    return 0;
}
