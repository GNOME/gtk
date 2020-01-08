/* GDK - The GIMP Drawing Kit
 *
 * gdkinterptest.h: input event interpolation unit test
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

#ifndef __GDK_INTERP_TEST_H__
#define __GDK_INTERP_TEST_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdk.h>
#include "gdkinternals.h"
#include "gdk/gdkeventhistoryprivate.h"

G_BEGIN_DECLS

/*
 * Generic enum for input phases.
 * Corresponds to GdkTouchpadGesturePhase for touchpad gestures, is_stop for
 * precise scroll, GDK_TOUCH_* for touchscreen events etc.
 */
typedef enum
{
  INPUT_PHASE_BEGIN,
  INPUT_PHASE_UPDATE,
  INPUT_PHASE_END,
  INPUT_PHASE_CANCEL
} InputPhase;

typedef struct _Fixture
{
  GdkEventHistory *interpolator;
  guint number_of_events_added;

  GArray *accum_uninterpolated_relative_props;
  GArray *accum_interpolated_relative_props;

  gdouble absolute_prop_base_val;
  gdouble relative_prop_base_val;

  guint32 newest_event_time;
} Fixture;

typedef struct _TestData
{
  GdkEventType event_type;

  /* FIXME replace display_interval with display_interval_ratio */
  gdouble display_interval;
} TestData;

/* Number of ms between consecutive input events */
#define EVENT_INTERVAL 10.0

/* Number of simulated input events. This is not the number of interpolated
   events, rather it's the number of the 'real' events */
#define NUM_EVENTS 7

/* EPOCH_START has to be larger than MAX(EVENT_INTERVAL, 12) because of the
   dummy event added by GdkEventHistory 12ms before the first 'real' event */
#define EPOCH_START 1000

/* For comparing deltas */
#define EPSILON 0.001

/* For comparing ratios */
#define RATIO_THRESHOLD 0.001

void
print_event (GdkEvent *event, char *prefix);

GdkEvent *
make_event (GdkEventType type,
            guint32 time,
            GdkModifierType state,
            InputPhase input_phase);

void
interpolation_test_events_accessors (Fixture       *fixture,
                                    gconstpointer  test_data);

void
interpolation_test_setup (Fixture       *fixture,
                          gconstpointer  user_data);

void
interpolation_test_teardown (Fixture       *fixture,
                             gconstpointer  test_data);

void
interpolation_test_history_properties (Fixture       *fixture,
                                       gconstpointer  test_data);

void
interpolation_test_constant_speed (Fixture       *fixture,
                                   gconstpointer  user_data);

G_END_DECLS

#endif /* __GDK_INTERP_TEST_H__ */
