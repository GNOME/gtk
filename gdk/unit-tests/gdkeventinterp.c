/* GDK - The GIMP Drawing Kit
 *
 * interpolation.c: input event interpolation unit test
 *
 * Copyright © 2019 Yariv Barkan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gdk/gdk.h>
#include "gdkinternals.h"
#include "gdkinterptest.h"


/*
 * All the tests use constant speed movement, since that way the results should
 * be the same regardless of the interpolation method used. While currently only
 * linear interpolation is supported, that might change in the future, so using
 * constant speed makes the tests more future proof.
 */
void
interpolation_test_setup (Fixture       *fixture,
                          gconstpointer  user_data)
{
  const TestData *test_data = user_data;
  GArray *absolute_property_values;
  GArray *relative_property_values;
  GArray *accum_uninterp_relative;
  GdkEvent *event;
  int i;
  int j;

  GDK_NOTE (EVENTS, g_message ("Creating events for the test, event interval %.2f, total duration %.2f",
                               EVENT_INTERVAL, EVENT_INTERVAL * (NUM_EVENTS - 1)));

  /* Arrays to query event properties */
  absolute_property_values = g_array_new (FALSE, FALSE, sizeof (gdouble));
  relative_property_values = g_array_new (FALSE, FALSE, sizeof (gdouble));

  /* Initialize the fixture */
  fixture->interpolator = gdk_event_history_new ();
  fixture->accum_uninterpolated_relative_props = g_array_new (FALSE, FALSE, sizeof (gdouble));
  fixture->accum_interpolated_relative_props = g_array_new (FALSE, FALSE, sizeof (gdouble));

  /* Just to make things more readable */
  accum_uninterp_relative = fixture->accum_uninterpolated_relative_props;

  /* Create a dummy event just to know the number of the properties */
  event = make_event (test_data->event_type, 0, 0, INPUT_PHASE_UPDATE);
  gdk_event_get_absolute_values_for_interpolation (event, absolute_property_values);
  gdk_event_get_relative_values_for_interpolation (event, relative_property_values);
  gdk_event_free (event);

  /* Set the basis for calculating absolute and relative properties.
     These values were arbitrarily chosen. */
  fixture->absolute_prop_base_val = 7;
  fixture->relative_prop_base_val = 4;

  /* Initialize absolute values */
  for (i = 0; i < absolute_property_values->len; ++i)
    g_array_index (absolute_property_values, gdouble, i) = fixture->absolute_prop_base_val * (i + 1);

  /* Initialize relative values */
  for (i = 0; i < relative_property_values->len; ++i)
    g_array_index (relative_property_values, gdouble, i) = fixture->relative_prop_base_val * (i + 1);

  /* Zero-init the array of accumulated uninterpolated relative props */
  g_array_set_size (accum_uninterp_relative, relative_property_values->len);
  for (i = 0; i < accum_uninterp_relative->len; ++i)
    g_array_index (accum_uninterp_relative, gdouble, i) = 0;

  /* Add some simulated constant speed events */
  for (i = 0; i < NUM_EVENTS; ++i)
    {
      /* The events are EVENT_INTERVAL ms apart, starting at time EPOCH_START */
      event = make_event (test_data->event_type, EPOCH_START + i * EVENT_INTERVAL, 0, INPUT_PHASE_UPDATE);

      /* Assume that gdk_event_set_interpolated_values() can be trusted since
         it was already verified. We actually set uninterpolated values here. */
      gdk_event_set_interpolated_values (event, absolute_property_values, GDK_INTERP_ABSOLUTE);
      gdk_event_set_interpolated_values (event, relative_property_values, GDK_INTERP_RELATIVE);

      print_event (event, "Uninterpolated");

      /* Add to the event history */
      gdk_event_history_push (fixture->interpolator, event);

      /* We no longer need the original event */
      gdk_event_free (event);

      /* Accumulate the relative properties so we could later compare the
         accumulated values with the accumulated interpolated values */
      for (j = 0; j < accum_uninterp_relative->len; ++j)
        g_array_index (accum_uninterp_relative, gdouble, j) += g_array_index (relative_property_values, gdouble, j);

      /* Update the absolute values for the next event */
      for (j = 0; j < absolute_property_values->len; ++j)
        g_array_index (absolute_property_values, gdouble, j) += fixture->absolute_prop_base_val * (j + 1);
    }

  /* We could deduce these values from fixture->interpolator, however this is
     the object under test, so we can't "trust" it */
  fixture->newest_event_time = EPOCH_START + ((NUM_EVENTS - 1) * EVENT_INTERVAL);
  fixture->number_of_events_added = NUM_EVENTS;

  g_array_free (absolute_property_values, TRUE);
  g_array_free (relative_property_values, TRUE);
}

void
interpolation_test_teardown (Fixture       *fixture,
                             gconstpointer  test_data)
{
  GDK_NOTE (EVENTS, g_message ("Test Teardown"));

  gdk_event_history_free (fixture->interpolator);
  g_array_free (fixture->accum_uninterpolated_relative_props, TRUE);
  g_array_free (fixture->accum_interpolated_relative_props, TRUE);
}

void
interpolation_test_history_properties (Fixture       *fixture,
                                       gconstpointer  test_data)
{
  GdkEventHistory *interpolator = fixture->interpolator;
  guint length;
  guint32 time;
  guint32 average_event_interval;

  GDK_NOTE (EVENTS, g_message ("Testing history properties"));

  /* Ensure that we have all expected events so far as well as the dummy event */
  length = gdk_event_history_length (interpolator);
  g_assert_cmpuint (length, ==, fixture->number_of_events_added + 1);

  /* Timestamp of the most recent event */
  time = gdk_event_history_newest_event_time (interpolator);
  g_assert_cmpuint (time, ==, EPOCH_START + (fixture->number_of_events_added - 1) * EVENT_INTERVAL);

  /* Average time between consecutive events. gdk_event_history_push() adds a
     dummy event about 12ms before the first real one */
  average_event_interval = gdk_event_history_average_event_interval (interpolator);
  g_assert_cmpuint (average_event_interval, >=, MIN (EVENT_INTERVAL, 12));
  g_assert_cmpuint (average_event_interval, <=, MAX (EVENT_INTERVAL, 12));

  /* No start event yet */
  GdkEvent *start_event = gdk_event_history_get_start_event (interpolator);
  g_assert_null (start_event);

  /* No stop event yet */
  GdkEvent *stop_event = gdk_event_history_get_stop_event (interpolator);
  g_assert_null (stop_event);
}

/*
 * validate_interpolated_absolute_props() is a helper function for interpolation_test_constant_speed()
 */
static void
validate_interpolated_absolute_props (Fixture  *fixture,
                                      gdouble   interpolation_point,
                                      GdkEvent *interpolated_event,
                                      gdouble   display_interval)
{
  GArray *property_values;
  int i;

  /* Query absolute event properties */
  property_values = g_array_new (FALSE, FALSE, sizeof (gdouble));
  gdk_event_get_absolute_values_for_interpolation (interpolated_event, property_values);

  /* The input device moves at constant speed in this test. If it moves N pixels in EVENT_INTERVAL ms,
     we expect it to move N / EVENT_INTERVAL per ms. */
  for (i = 0; i < property_values->len; ++i)
    g_assert_cmpfloat_with_epsilon (g_array_index (property_values, gdouble, i) /
                                     (fixture->absolute_prop_base_val * (i + 1) *
                                       (1 + ((interpolation_point - EPOCH_START) /
                                             EVENT_INTERVAL))),
                                    1.0, RATIO_THRESHOLD);

  g_array_free (property_values, TRUE);
}

/*
 * validate_interpolated_relative_props() is a helper function for interpolation_test_constant_speed()
 */
static void
validate_interpolated_relative_props (Fixture  *fixture,
                                      gdouble   interpolation_point,
                                      GdkEvent *interpolated_event,
                                      gdouble   display_interval)
{
  GArray *property_values;
  int i;

  /* Query relative event properties */
  property_values = g_array_new (FALSE, FALSE, sizeof (gdouble));
  gdk_event_get_relative_values_for_interpolation (interpolated_event, property_values);

  /* Validate the interpolated deltas */
  if (interpolation_point == EPOCH_START)
    {
      /* The first interpolated event should have the same displacement as the input events
         because its timestamp falls exactly on the first input event */
      for (i = 0; i < property_values->len; ++i)
        g_assert_cmpfloat_with_epsilon (g_array_index (property_values, gdouble, i) /
                                        (fixture->relative_prop_base_val * (i + 1)),
                                        1.0, RATIO_THRESHOLD);
    }
  else
    {
      /* The input device moves at constant speed in this test. If it moves N pixels in EVENT_INTERVAL ms,
         we expect it to move N / EVENT_INTERVAL per ms. So in a display frame it should move
         display_interval * (N / EVENT_INTERVAL). Put differently it's (N * display_interval / EVENT_INTERVAL). */
      for (i = 0; i < property_values->len; ++i)
        g_assert_cmpfloat_with_epsilon (g_array_index (property_values, gdouble, i) /
                                        (fixture->relative_prop_base_val * (i + 1) * display_interval / EVENT_INTERVAL),
                                        1.0, RATIO_THRESHOLD);
    }

  g_array_free (property_values, TRUE);
}

static void
validate_interpolated_event (Fixture  *fixture,
                             gdouble   interpolation_point,
                             GdkEvent *interpolated_event,
                             gdouble   display_interval)
{
  /* Verify the interpolated properties */
  validate_interpolated_absolute_props (fixture, interpolation_point, interpolated_event, display_interval);
  validate_interpolated_relative_props (fixture, interpolation_point, interpolated_event, display_interval);

  /* The event timestamp should match the interpolation point */
  g_assert_cmpfloat_with_epsilon (interpolation_point / gdk_event_get_time (interpolated_event), 1.0, RATIO_THRESHOLD);
}

/*
 * interpolate_point() is a helper function for interpolation_test_constant_speed()
 */
static void
interpolate_point (Fixture *fixture,
                   gdouble  interpolation_point,
                   gdouble  display_interval)
{
  GdkEventHistory *interpolator = fixture->interpolator;
  GdkEvent *interpolated_event;
  GArray *property_values;
  GArray *accum_interp_relative;
  int i;

  property_values = g_array_new (FALSE, FALSE, sizeof (char *));

  /* Just to make things more readable */
  accum_interp_relative = fixture->accum_interpolated_relative_props;

  /* Synthesize an interpolated event. interpolation_point is in ms, convert to us */
  interpolated_event =
    gdk_event_history_interpolate_event (interpolator,
                                         interpolation_point * 1000);

  g_assert_nonnull (interpolated_event);

  /* Accumulate the relative properties so we could later compare the
     accumulated values with the accumulated interpolated values */
  gdk_event_get_relative_values_for_interpolation (interpolated_event, property_values);
  for (i = 0; i < accum_interp_relative->len; ++i)
    g_array_index (accum_interp_relative, gdouble, i) += g_array_index (property_values, gdouble, i);

  print_event (interpolated_event, "Interpolated");

  /* We don't validate points later than the most recent event - this will be
     done later by comparing the accumulated deltas */
  if (interpolation_point <= fixture->newest_event_time)
    validate_interpolated_event (fixture, interpolation_point, interpolated_event, display_interval);

  gdk_event_free (interpolated_event);
  g_array_free (property_values, TRUE);
}

void
interpolation_test_constant_speed (Fixture       *fixture,
                                   gconstpointer  user_data)
{
  const TestData *test_data = user_data;
  gdouble interpolation_point;
  const gdouble duration = EVENT_INTERVAL * (fixture->number_of_events_added - 1);
  GArray *accum_interp_relative;
  GArray *accum_uninterp_relative;
  int i;

  GDK_NOTE (EVENTS, g_message ("Testing constant speed input. Display interval = %.2f Total duration = %.2f",
                               test_data->display_interval, duration));

  /* Generate interpolated events at display frame rate. Here interpolation_point
     is specified in milliseconds for simplicity. However in "real" usage, for
     example in GdkWindow, the interpolation point is the frame time which is
     measured in microseconds. */
  for (interpolation_point = EPOCH_START;
       interpolation_point < EPOCH_START + duration;
       interpolation_point += test_data->display_interval)
    {
      interpolate_point (fixture,
                         interpolation_point,
                         test_data->display_interval);
    }

  /* Usually the previous interpolation point will lie before the newest event,
     so we need one more interpolated event to "drain" the history */
  interpolate_point (fixture,
                     interpolation_point,
                     test_data->display_interval);

  accum_interp_relative = fixture->accum_interpolated_relative_props;
  accum_uninterp_relative = fixture->accum_uninterpolated_relative_props;

  /* Verify that the accumulated interpolated events equal the accumulated original events */
  for (i = 0; i < accum_interp_relative->len; ++i)
    g_assert_cmpfloat_with_epsilon (g_array_index (accum_interp_relative, gdouble, i) /
                                    g_array_index (accum_uninterp_relative, gdouble, i),
                                    1.0, RATIO_THRESHOLD);
}

