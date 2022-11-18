#include <gtk/gtk.h>

static void
test_calendar_set_day (void)
{
  GtkWidget *cal;
  GTimeZone *tz;
  GDateTime *dt;
  GDateTime *dt2;

  cal = gtk_calendar_new ();

  tz = g_time_zone_new_identifier ("MET");
  g_assert_nonnull (tz);
  dt = g_date_time_new (tz, 1970, 3, 1, 0, 0, 0);
  g_assert_nonnull (dt);

  gtk_calendar_select_day (GTK_CALENDAR (cal), dt);

  dt2 = gtk_calendar_get_date (GTK_CALENDAR (cal));
  g_assert_true (g_date_time_equal (dt, dt2));

  g_date_time_unref (dt);
  g_date_time_unref (dt2);
  g_time_zone_unref (tz);
}

static void
test_calendar_properties (void)
{
  GtkWidget *cal;
  GDateTime *dt2;

  cal = gtk_calendar_new ();

  g_object_set (cal,
                "year", 1970,
                "month", 2, /* March */
                "day", 1,
                NULL);

  dt2 = gtk_calendar_get_date (GTK_CALENDAR (cal));
  g_assert_cmpint (g_date_time_get_year (dt2), ==, 1970);
  g_assert_cmpint (g_date_time_get_month (dt2), ==, 3);
  g_assert_cmpint (g_date_time_get_day_of_month (dt2), ==, 1);

  g_date_time_unref (dt2);
}

int
main (int argc, char *argv[])
{
  gtk_init ();
  (g_test_init) (&argc, &argv, NULL);

  g_test_add_func ("/calendar/set_day", test_calendar_set_day);
  g_test_add_func ("/calendar/properties", test_calendar_properties);

  return g_test_run ();
}
