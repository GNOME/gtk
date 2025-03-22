#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

static void
test_calendar_set_get_properties (void)
{
  GtkWidget *calendar;
  GDateTime *date_in;
  GDateTime *date_out;

  calendar = gtk_calendar_new ();
  date_in = g_date_time_new_from_iso8601 ("1970-01-01T00:00:00Z", NULL);
  g_object_set (calendar, "date", date_in, NULL);
  g_object_get (calendar, "date", &date_out, NULL);
  g_assert_true (g_date_time_equal (date_in, date_out));
  g_date_time_unref (date_out);
  g_date_time_unref (date_in);
}

static void
test_calendar_set_date (void)
{
  GtkWidget *calendar;
  GDateTime *date_in;
  GDateTime *date_out;
  calendar = gtk_calendar_new ();
  date_in = g_date_time_new_from_iso8601 ("2110-11-03T20:20:05Z", NULL);
  gtk_calendar_set_date (GTK_CALENDAR (calendar), date_in);
  g_object_get (calendar, "date", &date_out, NULL);
  g_assert_true (g_date_time_equal (date_in, date_out));
  g_date_time_unref (date_out);
  g_date_time_unref (date_in);
}

static void
test_calendar_get_date (void)
{
  GtkWidget *calendar;
  GDateTime *date_in;
  GDateTime *date_out;
  calendar = gtk_calendar_new ();
  date_in = g_date_time_new_from_iso8601 ("0010-11-25T10:20:30Z", NULL);
  g_object_set (calendar, "date", date_in, NULL);
  date_out = gtk_calendar_get_date (GTK_CALENDAR (calendar));
  g_assert_true (g_date_time_equal (date_in, date_out));
  g_date_time_unref (date_out);
  g_date_time_unref (date_in);
}

static void
test_calendar_set_get_year (void)
{
  GtkWidget *calendar;
  int year;

  calendar = gtk_calendar_new ();
  gtk_calendar_set_day (GTK_CALENDAR (calendar), 10); /* avoid days that don't exist in all years */

  gtk_calendar_set_year (GTK_CALENDAR (calendar), 2024);
  year = gtk_calendar_get_year (GTK_CALENDAR (calendar));
  g_assert_cmpint (year, ==, 2024);
}

static void
test_calendar_set_get_month (void)
{
  GtkWidget *calendar;
  int month;

  calendar = gtk_calendar_new ();
  gtk_calendar_set_day (GTK_CALENDAR (calendar), 10); /* avoid days that don't exist in all months */

  gtk_calendar_set_month (GTK_CALENDAR (calendar), 1); /* February */
  month = gtk_calendar_get_month (GTK_CALENDAR (calendar));
  g_assert_cmpint (month, ==, 1);
}

static void
test_calendar_set_get_day (void)
{
  GtkWidget *calendar;
  int day;

  calendar = gtk_calendar_new ();
  gtk_calendar_set_day (GTK_CALENDAR (calendar), 10);

  gtk_calendar_set_day (GTK_CALENDAR (calendar), 11);
  day = gtk_calendar_get_day (GTK_CALENDAR (calendar));
  g_assert_cmpint (day, ==, 11);
}

int
main (int argc, char *argv[])
{
  gtk_init ();
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/calendar/set_get_properties", test_calendar_set_get_properties);
  g_test_add_func ("/calendar/set_date", test_calendar_set_date);
  g_test_add_func ("/calendar/get_date", test_calendar_get_date);
  g_test_add_func ("/calendar/set_get_day", test_calendar_set_get_day);
  g_test_add_func ("/calendar/set_get_month", test_calendar_set_get_month);
  g_test_add_func ("/calendar/set_get_year", test_calendar_set_get_year);

  return g_test_run ();
}
