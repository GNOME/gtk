/* Printing/Page Setup
 *
 * GtkPageSetupUnixDialog can be used if page setup is needed
 * independent of a full printing dialog.
 */

#include <math.h>
#include <gtk/gtk.h>
#include <gtk/gtkunixprint.h>

static void
done_cb (GtkDialog *dialog, gint response, gpointer data)
{
  gtk_widget_destroy (GTK_WIDGET (dialog));
}

GtkPageSetupUnixDialog *
do_pagesetup (GtkWidget *do_widget)
{
  static GtkPageSetupUnixDialog *window = NULL;

  if (!window)
    {
      window = gtk_page_setup_unix_dialog_new ("Page Setup", GTK_WINDOW (do_widget));
      g_signal_connect (window, "destroy", G_CALLBACK (gtk_widget_destroyed), &window);
      g_signal_connect (window, "response", G_CALLBACK (done_cb), NULL);
    }

  if (!gtk_widget_get_visible (GTK_WIDGET (window)))
    gtk_widget_show (GTK_WIDGET (window));
  else
    gtk_widget_destroy (GTK_WIDGET (window));

  return window;
}
