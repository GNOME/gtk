/*
 * crossingevents.c: A test for crossing events
 *
 * Copyright (C) 2008 Cody Russell
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>
#include <string.h>

typedef struct {
  GtkWidget *window;
  GtkWidget *eventbox;
  GtkWidget *frame;
  GtkWidget *button;
  GtkWidget *check;
  gboolean   events_connected;
  GQueue    *queue;
} CrossingTest;

typedef struct {
  gboolean entered;
  gchar *name;
  gboolean synthesized;
  GdkCrossingMode mode;
  GdkNotifyType detail;
} CrossingEventData;

#define SLEEP_DURATION    100

void start_events (CrossingTest *test);
void stop_events (CrossingTest *test);

static gboolean
sleep_timeout_cb (gpointer data)
{
  gtk_main_quit ();
  return FALSE;
}

static void
sleep_in_main_loop (double fraction)
{
  /* process all pending idles and events */
  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, FALSE);
  /* sleeping probably isn't strictly necessary here */
  gdk_threads_add_timeout_full (G_MAXINT, fraction * SLEEP_DURATION, sleep_timeout_cb, NULL, NULL);
  gtk_main ();
  /* process any pending idles or events that arrived during sleep */
  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, FALSE);
}

void
set_cursor (GtkWidget *widget)
{
  int x, y, w, h;

  gdk_window_get_origin (widget->window, &x, &y);

  x += widget->allocation.x;
  y += widget->allocation.y;
  w = widget->allocation.width;
  h = widget->allocation.height;

  gdk_display_warp_pointer (gtk_widget_get_display (widget),
			    gtk_widget_get_screen (widget),
			    x + w / 2,
			    y + h / 2);

  sleep_in_main_loop (0.5);
}

static gboolean
on_enter (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
  CrossingTest *test = (CrossingTest*)user_data;

  CrossingEventData *evt = g_slice_new0 (CrossingEventData);
  evt->entered = TRUE;
  evt->name = g_strdup (gtk_widget_get_name (widget));
  evt->synthesized = event->send_event;
  evt->mode = event->mode;
  evt->detail = event->detail;

  if (!test->queue)
    test->queue = g_queue_new ();

  g_queue_push_tail (test->queue, evt);

  return FALSE;
}

static gboolean
on_leave (GtkWidget *widget, GdkEventCrossing *event, gpointer user_data)
{
  CrossingTest *test = (CrossingTest*)user_data;

  CrossingEventData *evt = g_slice_new0 (CrossingEventData);
  evt->entered = FALSE;
  evt->name = g_strdup (gtk_widget_get_name (widget));
  evt->synthesized = event->send_event;
  evt->mode = event->mode;
  evt->detail = event->detail;

  if (!test->queue)
    test->queue = g_queue_new ();

  g_queue_push_tail (test->queue, evt);

  return FALSE;
}

static void
on_check_toggled (GtkWidget *toggle, GtkWidget *button)
{
  gtk_widget_set_sensitive (button, gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)));
}

static void
sensitivity_setup (CrossingTest *test,
		   gconstpointer user_data)
{
  GtkWidget *frame;

  test->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_widget_set_name (test->window, "W");
  frame = gtk_frame_new ("Crossing Events");
  test->eventbox = gtk_event_box_new ();
  gtk_widget_set_name (test->eventbox, "E");

  GtkWidget *vbox = gtk_vbox_new (FALSE, 10);
  gtk_container_add (GTK_CONTAINER (test->window), frame);
  gtk_container_add (GTK_CONTAINER (frame), test->eventbox);
  gtk_container_add (GTK_CONTAINER (test->eventbox), vbox);

  test->button = gtk_button_new_with_label ("Click me!");
  gtk_widget_set_name (test->button, "B");
  gtk_box_pack_start (GTK_BOX (vbox), test->button, FALSE, TRUE, 0);

  test->check = gtk_check_button_new_with_label ("Sensitive?");
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (test->check), TRUE);
  g_signal_connect (G_OBJECT (test->check),
		    "toggled", G_CALLBACK (on_check_toggled), test->button);
  gtk_widget_set_name (test->check, "C");
  gtk_box_pack_start (GTK_BOX (vbox), test->check, FALSE, TRUE, 0);

  gtk_widget_show_all (test->window);

  gtk_window_move (GTK_WINDOW (test->window), 0, 0);

  sleep_in_main_loop (0.5);
}

static void
sensitivity_teardown (CrossingTest *test,
		      gconstpointer user_data)
{
  stop_events (test);
  gtk_widget_destroy (test->window);

  if (test->queue != NULL)
    {
      g_queue_clear (test->queue);
      test->queue = NULL;
    }
}

void
start_events (CrossingTest *test)
{
  if (!test->events_connected)
    {
      g_object_connect (G_OBJECT (test->window),
			"signal::destroy", gtk_main_quit, NULL,
			"signal::enter-notify-event", on_enter, test,
			"signal::leave-notify-event", on_leave, test,
			NULL);
      g_object_connect (G_OBJECT (test->eventbox),
			"signal::enter-notify-event", on_enter, test,
			"signal::leave-notify-event", on_leave, test,
			NULL);
      g_object_connect (G_OBJECT (test->button),
			"signal::enter-notify-event", on_enter, test,
			"signal::leave-notify-event", on_leave, test,
			NULL);
      g_object_connect (G_OBJECT (test->check),
			"signal::enter-notify-event", on_enter, test,
			"signal::leave-notify-event", on_leave, test,
			NULL);
      test->events_connected = TRUE;
    }

  sleep_in_main_loop (0.5);
}

void
stop_events (CrossingTest *test)
{
  if (test->events_connected)
    {
      g_object_disconnect (G_OBJECT (test->window),
			   "any_signal", gtk_main_quit, NULL,
			   "any_signal", on_enter, test,
			   "any_signal", on_leave, test,
			   NULL);
      g_object_disconnect (G_OBJECT (test->eventbox),
			   "any_signal", on_enter, test,
			   "any_signal", on_leave, test,
			   NULL);
      g_object_disconnect (G_OBJECT (test->button),
			   "any_signal", on_enter, test,
			   "any_signal", on_leave, test,
			   NULL);
      g_object_disconnect (G_OBJECT (test->check),
			   "any_signal", G_CALLBACK (on_check_toggled), test->button,
			   "any_signal", on_enter, test,
			   "any_signal", on_leave, test,
			   NULL);
      test->events_connected = FALSE;
    }
}

void
move_cursor_away (CrossingTest *test)
{
  gdk_display_warp_pointer (gtk_widget_get_display (test->window),
                            gtk_widget_get_screen (test->window),
                            1000, -1000);

  sleep_in_main_loop (0.5);
}

void
check_event (CrossingTest *test,
	     const gchar *name,
	     gboolean entered,
	     gboolean synthesized,
	     GdkCrossingMode mode,
	     GdkNotifyType detail)
{
  CrossingEventData *evt;

  g_assert (test->queue != NULL);

  evt = g_queue_pop_head (test->queue);

  g_assert (evt->entered == entered);
  g_assert (strcmp (evt->name, name) == 0);
  g_assert (evt->synthesized == synthesized);
  g_assert (evt->mode == mode);

  if (evt->detail != detail)
    g_print ("%s %s event, detail %d, expected detail %d\n", 
             synthesized ? "synthesized" : "native",
             entered ? "enter" : "leave",
             evt->detail, detail);
 
  g_assert (evt->detail == detail);
}

/* Verify crossing events when moving into and out of a sensitive widget */
static void
cursor_on_sensitive (CrossingTest *test,
		     gconstpointer user_data)
{
  move_cursor_away (test);

  start_events (test);

  set_cursor (test->button);

  check_event (test,
	       "W",
	       TRUE,
	       FALSE,  /* native */
	       GDK_CROSSING_NORMAL,
	       GDK_NOTIFY_NONLINEAR_VIRTUAL);

  check_event (test,
	       "E",
	       TRUE,
	       FALSE,  /* native */
	       GDK_CROSSING_NORMAL,
	       GDK_NOTIFY_NONLINEAR_VIRTUAL);

  check_event (test,
	       "B",
	       TRUE,
	       FALSE,  /* native */
	       GDK_CROSSING_NORMAL,
	       GDK_NOTIFY_NONLINEAR);

  g_assert (g_queue_is_empty (test->queue));

  move_cursor_away (test);

  check_event (test,
	       "B",
	       FALSE,
	       FALSE,  /* native */
	       GDK_CROSSING_NORMAL,
	       GDK_NOTIFY_NONLINEAR);

  check_event (test,
	       "E",
	       FALSE,
	       FALSE,  /* native */
	       GDK_CROSSING_NORMAL,
	       GDK_NOTIFY_NONLINEAR_VIRTUAL);

  check_event (test,
	       "W",
	       FALSE,
	       FALSE,  /* native */
	       GDK_CROSSING_NORMAL,
	       GDK_NOTIFY_NONLINEAR_VIRTUAL);

  g_assert (g_queue_is_empty (test->queue));

  stop_events (test);
}

static void
change_sensitive_to_insensitive (CrossingTest *test,
				 gconstpointer user_data)
{
  move_cursor_away (test);
  set_cursor (test->button);

  start_events (test);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (test->check), FALSE);

  check_event (test,
	       "B",
	       FALSE,
	       TRUE,  /* synthesized */
	       GDK_CROSSING_STATE_CHANGED,
	       GDK_NOTIFY_ANCESTOR);

  check_event (test,
               "E",
	       FALSE,
	       TRUE,  /* synthesized */
               GDK_CROSSING_STATE_CHANGED,
               GDK_NOTIFY_VIRTUAL);

  check_event (test,
               "W",
	       FALSE,
	       TRUE,  /* synthesized */
               GDK_CROSSING_STATE_CHANGED,
               GDK_NOTIFY_VIRTUAL);

  g_assert (g_queue_is_empty (test->queue));

  stop_events (test);
}

static void
change_insensitive_to_sensitive (CrossingTest *test,
				 gconstpointer user_data)
{
  move_cursor_away (test);
  set_cursor (test->button);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (test->check), FALSE);

  start_events (test);

  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (test->check), TRUE);

  check_event (test,
               "W",
               TRUE,
               TRUE,  /* synthesized */
               GDK_CROSSING_STATE_CHANGED,
               GDK_NOTIFY_VIRTUAL);

  check_event (test,
               "E",
               TRUE,
	       TRUE,  /* synthesized */
               GDK_CROSSING_STATE_CHANGED,
               GDK_NOTIFY_VIRTUAL);

  check_event (test,
               "B",
               TRUE,
	       TRUE,  /* synthesized */
               GDK_CROSSING_STATE_CHANGED,
               GDK_NOTIFY_ANCESTOR);

  g_assert (g_queue_is_empty (test->queue));

  stop_events (test);
}

static void
cursor_from_insensitive_to_sensitive (CrossingTest *test,
				      gconstpointer user_data)
{
  set_cursor (test->button);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (test->check), FALSE);

  start_events (test);

  set_cursor (test->check);

  check_event (test,
               "C",
               TRUE,
               FALSE,  /* native */
               GDK_CROSSING_NORMAL,
               GDK_NOTIFY_NONLINEAR);

  g_assert (g_queue_is_empty (test->queue));

  stop_events (test);
}

static void
cursor_from_sensitive_to_insensitive (CrossingTest *test,
				      gconstpointer user_data)
{
  set_cursor (test->check);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (test->check), FALSE);

  start_events (test);

  set_cursor (test->button);

  check_event (test,
               "C",
               FALSE,
               FALSE,  /* native */
               GDK_CROSSING_NORMAL,
               GDK_NOTIFY_NONLINEAR);

  g_assert (g_queue_is_empty (test->queue));

  stop_events (test);
}

static void
add_gtk_grab (CrossingTest *test,
	      gconstpointer user_data)
{
  set_cursor (test->button);

  start_events (test);

  gtk_grab_add (test->check);

  check_event (test,
	       "B",
	       FALSE,
	       TRUE,   /* synthesized */
	       GDK_CROSSING_GTK_GRAB,
	       GDK_NOTIFY_ANCESTOR);

  check_event (test,
               "E",
               FALSE,
	       TRUE,   /* synthesized */
	       GDK_CROSSING_GTK_GRAB,
	       GDK_NOTIFY_ANCESTOR);

  check_event (test,
               "W",
               FALSE,
               TRUE,   /* synthesized */
               GDK_CROSSING_GTK_GRAB,
	       GDK_NOTIFY_ANCESTOR);

  g_assert (g_queue_is_empty (test->queue));

  stop_events (test);
}

static void
remove_gtk_grab (CrossingTest *test,
		 gconstpointer user_data)
{
  set_cursor (test->button);

  gtk_grab_add (test->check);

  start_events (test);

  gtk_grab_remove (test->check);

  check_event (test,
               "B",
               TRUE,
               TRUE,   /* synthesized */
               GDK_CROSSING_GTK_UNGRAB,
	       GDK_NOTIFY_ANCESTOR);

  check_event (test,
               "E",
               TRUE,
               TRUE,   /* synthesized */
	       GDK_CROSSING_GTK_UNGRAB,
               GDK_NOTIFY_ANCESTOR);

  check_event (test,
               "W",
               TRUE,
               TRUE,   /* synthesized */
               GDK_CROSSING_GTK_UNGRAB,
               GDK_NOTIFY_ANCESTOR);

  g_assert (g_queue_is_empty (test->queue));

  stop_events (test);
}

static void
cursor_from_shadowed_to_unshadowed (CrossingTest *test,
				    gconstpointer user_data)
{
  set_cursor (test->button);

  gtk_grab_add (test->check);

  start_events (test);

  set_cursor (test->check);

  check_event (test,
               "C",
               FALSE,
               FALSE,   /* native */
               GDK_CROSSING_NORMAL,
               GDK_NOTIFY_NONLINEAR);

  check_event (test,
               "C",
               TRUE,
               FALSE,   /* native */
               GDK_CROSSING_NORMAL,
               GDK_NOTIFY_NONLINEAR);

  g_assert (g_queue_is_empty (test->queue));

  stop_events (test);
}

static void
cursor_from_unshadowed_to_shadowed (CrossingTest *test,
				    gconstpointer user_data)
{
  set_cursor (test->check);

  gtk_grab_add (test->check);

  start_events (test);

  set_cursor (test->button);

  check_event (test,
               "C",
               FALSE,
               FALSE,   /* native */
               GDK_CROSSING_NORMAL,
               GDK_NOTIFY_NONLINEAR);

  check_event (test,
               "C",
               TRUE,
               FALSE,   /* native */
               GDK_CROSSING_NORMAL,
               GDK_NOTIFY_NONLINEAR);

  g_assert (g_queue_is_empty (test->queue));

  stop_events (test);
}

int
main (int    argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add ("/crossings/cursor-on-sensitive", CrossingTest, NULL,
  	      sensitivity_setup, cursor_on_sensitive, sensitivity_teardown);

  g_test_add ("/crossings/change-sensitive-to-insensitive", CrossingTest, NULL,
	      sensitivity_setup, change_sensitive_to_insensitive, sensitivity_teardown);

  g_test_add ("/crossings/cursor-from-insensitive-to-sensitive", CrossingTest, NULL,
	      sensitivity_setup, cursor_from_insensitive_to_sensitive, sensitivity_teardown);

  g_test_add ("/crossings/cursor-from-sensitive-to-insensitive", CrossingTest, NULL,
	      sensitivity_setup, cursor_from_sensitive_to_insensitive, sensitivity_teardown);

  g_test_add ("/crossings/change-insensitive-to-sensitive", CrossingTest, NULL,
	      sensitivity_setup, change_insensitive_to_sensitive, sensitivity_teardown);

  g_test_add ("/crossings/add-gtk-grab", CrossingTest, NULL,
	      sensitivity_setup, add_gtk_grab, sensitivity_teardown);

  g_test_add ("/crossings/remove-gtk-grab", CrossingTest, NULL,
	      sensitivity_setup, remove_gtk_grab, sensitivity_teardown);

  g_test_add ("/crossings/cursor-from-shadowed-to-unshadowed", CrossingTest, NULL,
	      sensitivity_setup, cursor_from_shadowed_to_unshadowed, sensitivity_teardown);

  g_test_add ("/crossings/cursor-from-unshadowed-to-shadowed", CrossingTest, NULL,
	      sensitivity_setup, cursor_from_unshadowed_to_shadowed, sensitivity_teardown);

  return g_test_run ();
}
