#include <gtk/gtk.h>

static void
date_changed (GObject *calendar,
              GParamSpec *pspec,
              gpointer data)
{
  GDateTime *date;
  gchar *text;

  date = gtk_date_chooser_widget_get_date (GTK_DATE_CHOOSER_WIDGET (calendar));
  text = g_date_time_format (date, "selected: %x");
  g_print ("%s\n", text);
  g_free (text);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window, *calendar;

  gtk_init (NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);

  calendar = gtk_date_chooser_widget_new ();
  g_signal_connect (calendar, "notify::date",
                    G_CALLBACK (date_changed), NULL);

  gtk_container_add (GTK_CONTAINER (window), calendar);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
