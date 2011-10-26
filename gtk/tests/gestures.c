/* gestures tests.
 *
 * Copyright (C) 2011 Carlos Garnacho  <carlos@lanedo.com>
 * Authors: Carlos Garnacho  <carlos@lanedo.com>
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

static void
gesture_detected_cb (GtkGesturesInterpreter *interpreter,
                     guint                   gesture_id,
                     gdouble                 confidence,
                     gpointer                user_data)
{
  gdouble *confidence_ret = user_data;

  *confidence_ret = confidence;
}

static void
append_event_to_interpreter (GtkGesturesInterpreter *interpreter,
                             gdouble                 x,
                             gdouble                 y)
{
  GdkDeviceManager *device_manager;
  GdkDevice *client_pointer;
  GdkDisplay *display;
  GdkEvent *event;

  display = gdk_display_get_default ();
  device_manager = gdk_display_get_device_manager (display);
  client_pointer = gdk_device_manager_get_client_pointer (device_manager);

  event = gdk_event_new (GDK_MOTION_NOTIFY);
  event->motion.x = x;
  event->motion.y = y;
  event->motion.x_root = x;
  event->motion.y_root = y;
  gdk_event_set_device (event, client_pointer);
  gdk_event_set_source_device (event, client_pointer);

  gtk_gestures_interpreter_feed_event (interpreter, event);
  gdk_event_free (event);
}

static void
test_empty_interpreter (void)
{
  GtkGesturesInterpreter *interpreter;
  gboolean gesture_detected;
  gdouble confidence = 0;

  interpreter = gtk_gestures_interpreter_new ();
  g_signal_connect (interpreter, "gesture-detected",
                    G_CALLBACK (gesture_detected_cb),
                    &confidence);

  /* Feed some events */
  append_event_to_interpreter (interpreter, 0, 0);
  append_event_to_interpreter (interpreter, 100, 0);
  append_event_to_interpreter (interpreter, 100, 100);
  append_event_to_interpreter (interpreter, 0, 100);

  gesture_detected = gtk_gestures_interpreter_finish (interpreter, NULL);

  g_assert (gesture_detected == FALSE);
  g_assert_cmpfloat (confidence, ==, 0);

  g_object_unref (interpreter);
}

static void
test_equality (void)
{
  GtkGesturesInterpreter *interpreter;
  gboolean gesture_detected;
  gdouble confidence = 0;
  guint gesture_id;

  interpreter = gtk_gestures_interpreter_new ();
  g_signal_connect (interpreter, "gesture-detected",
                    G_CALLBACK (gesture_detected_cb),
                    &confidence);

  gtk_gestures_interpreter_add_gesture (interpreter,
                                        GTK_GESTURE_SWIPE_RIGHT);

  /* Feed the events */
  append_event_to_interpreter (interpreter, 0, 0);
  append_event_to_interpreter (interpreter, 100, 0);

  gesture_detected = gtk_gestures_interpreter_finish (interpreter, &gesture_id);

  g_assert (gesture_detected != FALSE);
  g_assert_cmpuint (gesture_id, ==, GTK_GESTURE_SWIPE_RIGHT);
  g_assert_cmpfloat (confidence, ==, 1);

  g_object_unref (interpreter);
}

static void
test_events_ubiquity (void)
{
  GtkGesturesInterpreter *interpreter;
  gboolean gesture_detected;
  gdouble confidence = 0;
  guint gesture_id;

  interpreter = gtk_gestures_interpreter_new ();
  g_signal_connect (interpreter, "gesture-detected",
                    G_CALLBACK (gesture_detected_cb),
                    &confidence);

  gtk_gestures_interpreter_add_gesture (interpreter,
                                        GTK_GESTURE_SWIPE_RIGHT);

  /* Feed the events, huge scale */
  append_event_to_interpreter (interpreter, 0, 0);
  append_event_to_interpreter (interpreter, 1000, 0);

  gesture_detected = gtk_gestures_interpreter_finish (interpreter, &gesture_id);

  g_assert (gesture_detected != FALSE);
  g_assert_cmpuint (gesture_id, ==, GTK_GESTURE_SWIPE_RIGHT);
  g_assert_cmpfloat (confidence, ==, 1);

  /* Feed other events, displaced somewhere, and
   * at a different scale, it's the X displacement
   * to the right what counts.
   */
  append_event_to_interpreter (interpreter, 500, 120);
  append_event_to_interpreter (interpreter, 600, 120);

  gesture_detected = gtk_gestures_interpreter_finish (interpreter, &gesture_id);

  g_assert (gesture_detected != FALSE);
  g_assert_cmpuint (gesture_id, ==, GTK_GESTURE_SWIPE_RIGHT);
  g_assert_cmpfloat (confidence, ==, 1);

  g_object_unref (interpreter);
}

static void
test_opposite_gesture (void)
{
  GtkGesturesInterpreter *interpreter;
  gboolean gesture_detected;
  gdouble confidence = 1;

  interpreter = gtk_gestures_interpreter_new ();
  g_signal_connect (interpreter, "gesture-detected",
                    G_CALLBACK (gesture_detected_cb),
                    &confidence);

  gtk_gestures_interpreter_add_gesture (interpreter,
                                        GTK_GESTURE_SWIPE_RIGHT);

  /* Feed the events, swipe to left */
  append_event_to_interpreter (interpreter, 100, 0);
  append_event_to_interpreter (interpreter, 0, 0);

  gesture_detected = gtk_gestures_interpreter_finish (interpreter, NULL);

  g_assert (gesture_detected == FALSE);
  g_assert_cmpfloat (confidence, ==, 0);

  g_object_unref (interpreter);
}

static void
test_ignore_initial_orientation (void)
{
  GtkGesturesInterpreter *interpreter;
  gdouble initial_confidence, conf;
  gdouble confidence = 1;

  interpreter = gtk_gestures_interpreter_new ();
  g_signal_connect (interpreter, "gesture-detected",
                    G_CALLBACK (gesture_detected_cb),
                    &confidence);

  /* Use a circular gesture, which ignores initial orientation.
   * Stroke square rectangles at different orientations, which should
   * yield the same (lack of) confidence about the stroke.
   */
  gtk_gestures_interpreter_add_gesture (interpreter,
                                        GTK_GESTURE_CIRCULAR_CLOCKWISE);

  /* First rectangle */
  append_event_to_interpreter (interpreter, 0, 0);
  append_event_to_interpreter (interpreter, 100, 0);
  append_event_to_interpreter (interpreter, 100, 100);
  append_event_to_interpreter (interpreter, 0, 100);
  append_event_to_interpreter (interpreter, 0, 0);

  gtk_gestures_interpreter_finish (interpreter, NULL);
  initial_confidence = confidence;

  /* Second rectangle, completely flipped over */
  append_event_to_interpreter (interpreter, 100, 100);
  append_event_to_interpreter (interpreter, 0, 100);
  append_event_to_interpreter (interpreter, 0, 0);
  append_event_to_interpreter (interpreter, 100, 0);
  append_event_to_interpreter (interpreter, 100, 100);

  gtk_gestures_interpreter_finish (interpreter, NULL);
  conf = confidence;

  g_assert_cmpfloat (initial_confidence, ==, conf);

  /* Third rectangle, 45º degrees rotation, different scale */
  append_event_to_interpreter (interpreter, 50, 0);
  append_event_to_interpreter (interpreter, 100, 50);
  append_event_to_interpreter (interpreter, 50, 100);
  append_event_to_interpreter (interpreter, 0, 50);
  append_event_to_interpreter (interpreter, 50, 0);

  gtk_gestures_interpreter_finish (interpreter, NULL);
  conf = confidence;

  g_assert_cmpfloat (initial_confidence, ==, conf);

  g_object_unref (interpreter);
}

int
main (int argc, char *argv[])
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add_func ("/gestures/empty-interpreter", test_empty_interpreter);
  g_test_add_func ("/gestures/equality", test_equality);
  g_test_add_func ("/gestures/events-ubiquity", test_events_ubiquity);
  g_test_add_func ("/gestures/opposite-gesture", test_opposite_gesture);
  g_test_add_func ("/gestures/ignore-initial-orientation", test_ignore_initial_orientation);

  return g_test_run ();
}
