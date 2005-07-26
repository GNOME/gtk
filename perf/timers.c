/* Utility functions for timing widgets
 *
 * Authors:
 *   Federico Mena-Quintero <federico@novell.com>
 *
 * To measure how long it takes to fully map and expose a toplevel window, we
 * use a trick which Owen Taylor described on IRC one day:
 *
 *   1. Start a timer.
 *   2. Call gtk_widget_show_all() on the toplevel window.
 *   3. In the expose_event handler of the window, queue an idle handler with
 *      G_PRIORITY_HIGH.
 *   4. In the idle handler, change a property on the toplevel window.
 *   5. In the property_notify_event handler, stop the timer.
 */

#include <string.h>
#include <glib.h>
#include <gtk/gtkmain.h>
#include "timers.h"

struct timer_closure
{
  GTimer *timer;
  GtkWidget *widget;
  TimerReportFunc report_func;
  gpointer user_data;
};

static gboolean
widget_property_notify_event_cb (GtkWidget *widget, GdkEventProperty *event, gpointer data)
{
  struct timer_closure *closure;
  gdouble elapsed;

  closure = data;

  if (event->atom != gdk_atom_intern ("window_property_change", FALSE))
    return FALSE;

  /* Finish timing map/expose */

  elapsed = g_timer_elapsed (closure->timer, NULL);
  (* closure->report_func) (TIMER_REPORT_WIDGET_SHOW, elapsed, closure->user_data);

  /* Time destruction */
  
  g_timer_reset (closure->timer);
  gtk_widget_destroy (widget);
  elapsed = g_timer_elapsed (closure->timer, NULL);
  (* closure->report_func) (TIMER_REPORT_WIDGET_DESTRUCTION, elapsed, closure->user_data);

  gtk_main_quit (); /* This will get us back to the end of timer_time_widget() */
  return TRUE;
}

static gboolean
idle_after_expose_cb (gpointer data)
{
  struct timer_closure *closure;

  closure = data;

  gdk_property_change (closure->widget->window,
		       gdk_atom_intern ("window_property_change", FALSE),
		       gdk_atom_intern ("STRING", FALSE),
		       8,
		       GDK_PROP_MODE_REPLACE,
		       "hello",
		       strlen ("hello"));

  return FALSE;
}

static gboolean
widget_expose_event_cb (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
  struct timer_closure *closure;

  closure = data;

  g_idle_add_full (G_PRIORITY_HIGH, idle_after_expose_cb, closure, NULL);
  return FALSE;
}

static void
instrument_widget (GtkWidget *widget,
		   struct timer_closure *closure)
{
  g_signal_connect (widget, "expose-event",
		    G_CALLBACK (widget_expose_event_cb), closure);

  gtk_widget_add_events (widget, GDK_PROPERTY_CHANGE_MASK);
  g_signal_connect (widget, "property-notify-event",
		    G_CALLBACK (widget_property_notify_event_cb), closure);
}

void
timer_time_widget (TimerWidgetCreateFunc create_func,
		   TimerReportFunc       report_func,
		   gpointer              user_data)
{
  GTimer *timer;
  GtkWidget *widget;
  gdouble elapsed;
  struct timer_closure closure;

  g_assert (create_func != NULL);
  g_assert (report_func != NULL);

  /* Time creation */

  timer = g_timer_new ();
  widget = (* create_func) (user_data);
  g_assert (widget != NULL);
  g_assert (!GTK_WIDGET_VISIBLE (widget) && !GTK_WIDGET_MAPPED (widget));
  elapsed = g_timer_elapsed (timer, NULL);

  (* report_func) (TIMER_REPORT_WIDGET_CREATION, elapsed, user_data);

  /* Start timing map/expose */

  closure.timer = timer;
  closure.widget = widget;
  closure.report_func = report_func;
  closure.user_data = user_data;
  instrument_widget (widget, &closure);

  g_timer_reset (timer);
  gtk_widget_show_all (widget);
  gtk_main ();

  /* Expose time and destruction time get recorded in widget_property_notify_event_cb() */
}
