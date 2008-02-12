#include <stdio.h>
#include <gtk/gtk.h>
#include "gtkwidgetprofiler.h"
#include "widgets.h"

#define ITERS 100000

static GtkWidget *
create_widget_cb (GtkWidgetProfiler *profiler, gpointer data)
{
  return appwindow_new ();
}

static void
report_cb (GtkWidgetProfiler *profiler, GtkWidgetProfilerReport report, GtkWidget *widget, gdouble elapsed, gpointer data)
{
  const char *type;

  switch (report) {
  case GTK_WIDGET_PROFILER_REPORT_CREATE:
    type = "widget creation";
    break;

  case GTK_WIDGET_PROFILER_REPORT_MAP:
    type = "widget map";
    break;

  case GTK_WIDGET_PROFILER_REPORT_EXPOSE:
    type = "widget expose";
    break;

  case GTK_WIDGET_PROFILER_REPORT_DESTROY:
    type = "widget destruction";
    break;

  default:
    g_assert_not_reached ();
    type = NULL;
  }

  fprintf (stdout, "%s: %g sec\n", type, elapsed);

  if (report == GTK_WIDGET_PROFILER_REPORT_DESTROY)
    fputs ("\n", stdout);
}

int
main (int argc, char **argv)
{
  GtkWidgetProfiler *profiler;

  gtk_init (&argc, &argv);

  profiler = gtk_widget_profiler_new ();
  g_signal_connect (profiler, "create-widget",
		    G_CALLBACK (create_widget_cb), NULL);
  g_signal_connect (profiler, "report",
		    G_CALLBACK (report_cb), NULL);

  gtk_widget_profiler_set_num_iterations (profiler, ITERS);

/*   gtk_widget_profiler_profile_boot (profiler); */
  gtk_widget_profiler_profile_expose (profiler);
  
  return 0;
}
