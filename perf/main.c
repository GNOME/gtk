#include <stdio.h>
#include <gtk/gtk.h>
#include "appwindow.h"
#include "timers.h"

#define ITERS 20

static GtkWidget *
create_cb (gpointer data)
{
  return appwindow_new ();
}

static void
report_cb (TimerReport report, gdouble elapsed, gpointer data)
{
  const char *type;

  switch (report) {
  case TIMER_REPORT_WIDGET_CREATION:
    type = "widget creation";
    break;

  case TIMER_REPORT_WIDGET_SHOW:
    type = "widget show";
    break;

  case TIMER_REPORT_WIDGET_DESTRUCTION:
    type = "widget destruction";
    break;

  default:
    g_assert_not_reached ();
    type = NULL;
  }

  fprintf (stderr, "%s: %g sec\n", type, elapsed);

  if (report == TIMER_REPORT_WIDGET_DESTRUCTION)
    fputs ("\n", stderr);
}

int
main (int argc, char **argv)
{
  int i;

  gtk_init (&argc, &argv);

  for (i = 0; i < ITERS; i++)
    timer_time_widget (create_cb, report_cb, NULL);
  
  return 0;
}
