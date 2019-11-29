#include <gdk/gdk.h>
#include "gdkinternals.h"

typedef struct _SmoothScrollData
{
  gdouble delta_x;
  gdouble delta_y;
  guint is_stop : 1;
} SmoothScrollData;

typedef struct _Fixture
{
  GdkEventInterpolation *interpolator;
  guint number_of_events_added;
  gdouble accum_x;
  gdouble accum_y;
  gdouble dx;
  gdouble dy;
} Fixture;

typedef struct _TestData
{
  gdouble display_interval;
} TestData;

/* Number of ms between consecutive input events */
#define EVENT_INTERVAL 10

/* Number of simulated input events. This is not the number of interpolated
   events, rather it's the number of the 'real' events */
#define NUM_EVENTS 5

/* EPOCH_START has to be larger than MAX(EVENT_INTERVAL, 12) because of the
   dummy event added by GdkEventInterpolation 12ms before the first 'real'
   event */
#define EPOCH_START 1000

/* For comparing deltas */
#define EPSILON 0.001

/* For comparing ratios */
#define RATIO_THRESHOLD 0.001


/*
 * make_event() is loosely based on _gdk_make_event()
 * It is used here to simulate 'real' events.
 */
GdkEvent *
make_event (GdkEventType type, guint32 time, GdkModifierType state, void* data)
{
  GdkEvent *event = gdk_event_new (type);

  event->any.send_event = FALSE;

  switch (type)
    {
    case GDK_MOTION_NOTIFY:
      event->motion.time = time;
      event->motion.axes = NULL;
      event->motion.state = state;
      break;

    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
      event->button.time = time;
      event->button.axes = NULL;
      event->button.state = state;
      break;

    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
      event->touch.time = time;
      event->touch.axes = NULL;
      event->touch.state = state;
      break;

    case GDK_SCROLL:
      event->scroll.time = time;
      event->scroll.state = state;
      event->scroll.direction = GDK_SCROLL_SMOOTH;
      event->scroll.x = 100;
      event->scroll.y = 200;
      event->scroll.x_root = 300;
      event->scroll.y_root = 400;
      event->scroll.delta_y = ((SmoothScrollData*)data)->delta_y;
      event->scroll.delta_x = ((SmoothScrollData*)data)->delta_x;
      event->scroll.is_stop = ((SmoothScrollData*)data)->is_stop;
      break;

    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
      event->key.time = time;
      event->key.state = state;
      break;

    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
      event->crossing.time = time;
      event->crossing.state = state;
      break;

    case GDK_PROPERTY_NOTIFY:
      event->property.time = time;
      event->property.state = state;
      break;

    case GDK_SELECTION_CLEAR:
    case GDK_SELECTION_REQUEST:
    case GDK_SELECTION_NOTIFY:
      event->selection.time = time;
      break;

    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
      event->proximity.time = time;
      break;

    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
      event->dnd.time = time;
      break;

    case GDK_TOUCHPAD_SWIPE:
      event->touchpad_swipe.time = time;
      event->touchpad_swipe.state = state;
      break;

    case GDK_TOUCHPAD_PINCH:
      event->touchpad_pinch.time = time;
      event->touchpad_pinch.state = state;
      break;

    case GDK_FOCUS_CHANGE:
    case GDK_CONFIGURE:
    case GDK_MAP:
    case GDK_UNMAP:
    case GDK_CLIENT_EVENT:
    case GDK_VISIBILITY_NOTIFY:
    case GDK_DELETE:
    case GDK_DESTROY:
    case GDK_EXPOSE:
    default:
      break;
    }

  return event;
}

/*
 * Right now we only simulate smooth scroll events.
 *
 * All the tests use constant speed movement, since that way the results should
 * be the same regardless of the interpolation method used. While currently only
 * linear interpolation is supported, that might change in the future, so using
 * constant speed makes the tests more future proof.
 */
static void
interpolation_test_setup (Fixture       *fixture,
                          gconstpointer  test_data)
{
  SmoothScrollData scroll_data;
  GdkEvent *event;
  int i;

  /* Each event will have a constant movement of 4px, 8px */
  fixture->interpolator = gdk_event_interpolation_new ();
  fixture->accum_x = 0;
  fixture->accum_y = 0;
  fixture->dx = 4;
  fixture->dy = 8;

  /* All simulated events will have the same properties */
  scroll_data.delta_x = fixture->dx;
  scroll_data.delta_y = fixture->dy;
  scroll_data.is_stop = 0;

  /* Add some simulated constant speed scroll events */
  for (i = 0; i < NUM_EVENTS; ++i)
    {
      /* The events are EVENT_INTERVAL ms apart, starting at time EPOCH_START */
      event = make_event (GDK_SCROLL, EPOCH_START + i * EVENT_INTERVAL, 0, &scroll_data);

      fixture->accum_x += event->scroll.delta_x;
      fixture->accum_y += event->scroll.delta_y;

      GDK_NOTE (EVENTS, g_message ("time = %d delta_x = %f delta_y = %f accum_x = %f accum_y = %f",
                                   event->scroll.time,
                                   event->scroll.delta_x,
                                   event->scroll.delta_y,
                                   fixture->accum_x,
                                   fixture->accum_y));

      gdk_event_interpolation_history_push (fixture->interpolator, event);

      /* We no longer need the original event */
      gdk_event_free (event);
    }

  /* We could deduce that number from fixture->interpolator however this is the
     object under test, so we can't "trust" it */
  fixture->number_of_events_added = NUM_EVENTS;
}

static void
interpolation_test_teardown (Fixture       *fixture,
                             gconstpointer  test_data)
{
  gdk_event_interpolation_free (fixture->interpolator);
}

static void
interpolation_test_history_properties (Fixture       *fixture,
                                       gconstpointer  test_data)
{
  GdkEventInterpolation *interpolator = fixture->interpolator;
  guint length;
  guint32 time;
  guint32 average_event_interval;

  /* Ensure that we have all expected events so far as well as the dummy event */
  length = gdk_event_interpolation_history_length (interpolator);
  g_assert_cmpuint (length, ==, fixture->number_of_events_added + 1);

  /* Timestamp of the most recent event */
  time = gdk_event_interpolation_newest_event_time (interpolator);
  g_assert_cmpuint (time, ==, EPOCH_START + (fixture->number_of_events_added - 1) * EVENT_INTERVAL);

  /* gdk_event_interpolation adds a dummy event about 12ms before the first real one */
  average_event_interval = gdk_event_interpolation_average_event_interval (interpolator);
  g_assert_cmpuint (average_event_interval, >=, MIN (EVENT_INTERVAL, 12));
  g_assert_cmpuint (average_event_interval, <=, MAX (EVENT_INTERVAL, 12));

  /* No stop event yet */
  GdkEvent *stop_event = gdk_event_interpolation_get_stop_event (interpolator);
  g_assert_null (stop_event);
}

/*
 * validate_interpolated_event() is a helper function for interpolation_test_constant_speed()
 */
static void
validate_interpolated_event (Fixture  *fixture,
                             gdouble   interpolation_point,
                             GdkEvent *interpolated_event,
                             gdouble   display_interval)
{
  /* Validate the interpolated deltas */
  if (interpolation_point == EPOCH_START)
    {
      /* The first interpolated event should have the same displacement as the input events
         because its timestamp falls exactly on the first input event */
      g_assert_cmpfloat_with_epsilon (interpolated_event->scroll.delta_x / fixture->dx, 1.0, RATIO_THRESHOLD);
      g_assert_cmpfloat_with_epsilon (interpolated_event->scroll.delta_y / fixture->dy, 1.0, RATIO_THRESHOLD);
    }
  else
    {
      /* The input device moves at constant speed in this test. If it moves N pixels in EVENT_INTERVAL ms,
         we expect it to move N / EVENT_INTERVAL per ms. So in a display frame it should move
         display_interval * (N / EVENT_INTERVAL). Put differently it's (N * display_interval / EVENT_INTERVAL). */
      g_assert_cmpfloat_with_epsilon (interpolated_event->scroll.delta_x / (fixture->dx * display_interval / EVENT_INTERVAL), 1.0, RATIO_THRESHOLD);
      g_assert_cmpfloat_with_epsilon (interpolated_event->scroll.delta_y / (fixture->dy * display_interval / EVENT_INTERVAL), 1.0, RATIO_THRESHOLD);
    }
}

/*
 * interpolate_point() is a helper function for interpolation_test_constant_speed()
 */
static void
interpolate_point (Fixture *fixture,
                   gdouble  interpolation_point,
                   gdouble  display_interval,
                   gdouble *accum_x,
                   gdouble *accum_y)
{
  GdkEventInterpolation *interpolator = fixture->interpolator;
  GdkEvent *interpolated_event;
  guint32 newest_event_time;

  /* Synthesize an interpolated event. interpolation_point is in us, convert to ms */
  interpolated_event =
    gdk_event_interpolation_interpolate_event (interpolator,
                                               interpolation_point * 1000);

  g_assert_nonnull (interpolated_event);

  *accum_x += interpolated_event->scroll.delta_x;
  *accum_y += interpolated_event->scroll.delta_y;

  GDK_NOTE (EVENTS, g_message ("interpolation_point = %f delta_x = %f delta_y = %f accum_x = %f accum_y = %f",
                               interpolation_point,
                               interpolated_event->scroll.delta_x,
                               interpolated_event->scroll.delta_y,
                               *accum_x,
                               *accum_y));

  /* interpolation_point is given in us while event time is in ms. We don't
     validate points later than the most recent event - this will be done later
     by comparing the accumulated deltas */
  newest_event_time = gdk_event_interpolation_newest_event_time (interpolator);
  if (interpolation_point * 1000 < newest_event_time)
    validate_interpolated_event (fixture, interpolation_point, interpolated_event, display_interval);

  gdk_event_free (interpolated_event);
}

static void
interpolation_test_constant_speed (Fixture       *fixture,
                                   gconstpointer  user_data)
{
  const TestData *test_data = user_data;
  gdouble interpolation_point;
  const gdouble duration = EVENT_INTERVAL * (fixture->number_of_events_added - 1);
  gdouble accum_x;
  gdouble accum_y;

  GDK_NOTE (EVENTS, g_message ("display interval = %f duration = %f", test_data->display_interval, duration));

  accum_x = 0;
  accum_y = 0;

  /* Generate interpolated events at display frame rate */
  for (interpolation_point = EPOCH_START;
       interpolation_point < EPOCH_START + duration;
       interpolation_point += test_data->display_interval)
    {
      interpolate_point (fixture,
                         interpolation_point,
                         test_data->display_interval,
                         &accum_x,
                         &accum_y);
    }

  /* Usually the previous interpolation point will lie before the newest event,
     so we need one more interpolated event to "drain" the history */
  interpolate_point (fixture,
                     interpolation_point,
                     test_data->display_interval,
                     &accum_x,
                     &accum_y);

  /* Verify that the accumulated interpolated events equal the accumulated original events */
  g_assert_cmpfloat_with_epsilon (accum_x / fixture->accum_x, 1.0, RATIO_THRESHOLD);
  g_assert_cmpfloat_with_epsilon (accum_y / fixture->accum_y, 1.0, RATIO_THRESHOLD);
}

int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  gdk_init (NULL, NULL);

  g_test_bug_base ("https://gitlab.gnome.org/GNOME/gtk/issues");

  g_test_add ("/interpolation/history_properties",
              Fixture, NULL,
              interpolation_test_setup,
              interpolation_test_history_properties,
              interpolation_test_teardown);

  /* For 60Hz display the display interval would be about 16.66ms, however it's
     easier to use whole numbers for the test. The actual numbers don't matter,
     the important thing is the ratio between the input events interval and the
     display interval. */

  TestData display_is_slower;
  display_is_slower.display_interval = EVENT_INTERVAL * 1.5;
  g_test_add ("/interpolation/display_is_slower",
              Fixture, &display_is_slower,
              interpolation_test_setup,
              interpolation_test_constant_speed,
              interpolation_test_teardown);

  TestData same_interval;
  same_interval.display_interval = EVENT_INTERVAL * 1.0;
  g_test_add ("/interpolation/same_interval",
              Fixture, &same_interval,
              interpolation_test_setup,
              interpolation_test_constant_speed,
              interpolation_test_teardown);

  TestData display_is_faster;
  display_is_faster.display_interval = EVENT_INTERVAL * 0.5;
  g_test_add ("/interpolation/display_is_faster",
              Fixture, &display_is_faster,
              interpolation_test_setup,
              interpolation_test_constant_speed,
              interpolation_test_teardown);

  return g_test_run ();
}
