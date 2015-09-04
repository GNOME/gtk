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

static GtkDateChooserDayOptions
get_day_options (GtkDateChooserWidget *calendar,
                 GDateTime            *date,
                 gpointer              data)
{
  GtkDateChooserDayOptions options;

  options = GTK_DATE_CHOOSER_DAY_NONE;

  if (g_date_time_get_day_of_week (date) == 6 ||
      g_date_time_get_day_of_week (date) == 7)
    options |= GTK_DATE_CHOOSER_DAY_WEEKEND;

  /* 4th of July */
  if (g_date_time_get_month (date) == 7 &&
      g_date_time_get_day_of_month (date) == 4)
    options |= GTK_DATE_CHOOSER_DAY_HOLIDAY;

  /* Bastille day */
  if (g_date_time_get_month (date) == 7 &&
      g_date_time_get_day_of_month (date) == 14)
    options |= GTK_DATE_CHOOSER_DAY_HOLIDAY;

  /* my birthday */
  if (g_date_time_get_month (date) == 3 &&
      g_date_time_get_day_of_month (date) == 1)
    options |= GTK_DATE_CHOOSER_DAY_MARKED;

  return options;
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
  gtk_date_chooser_widget_set_day_options_callback (GTK_DATE_CHOOSER_WIDGET (calendar),
                                                    get_day_options,
                                                    NULL,
                                                    NULL);

  gtk_container_add (GTK_CONTAINER (window), calendar);

  gtk_widget_show_all (window);

  gtk_main ();

  return 0;
}
