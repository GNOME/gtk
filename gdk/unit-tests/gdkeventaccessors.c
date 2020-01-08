/* GDK - The GIMP Drawing Kit
 *
 * gdkeventsinterp.c: input event interpolation unit test
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


static void
validate_absolute_prop_names (GdkEvent *event,
                              GArray   *property_names)
{
  switch (event->type)
    {
    case GDK_SCROLL:
      g_assert (event->scroll.direction == GDK_SCROLL_SMOOTH);
      g_assert_cmpuint (property_names->len, ==, 0);
      break;
    default:
      /* Event type is unsupported */
      g_assert_not_reached ();
      break;
    }
}

static void
validate_absolute_prop_values (GdkEvent *event,
                               GArray   *property_values,
                               gdouble   coef)
{
  switch (event->type)
    {
    case GDK_SCROLL:
      g_assert (event->scroll.direction == GDK_SCROLL_SMOOTH);
      g_assert_cmpuint (property_values->len, ==, 0);
      break;
    default:
      /* Event type is unsupported */
      g_assert_not_reached ();
      break;
    }
}

static void
validate_relative_prop_names (GdkEvent *event,
                              GArray   *property_names)
{
  switch (event->type)
    {
    case GDK_SCROLL:
      g_assert (event->scroll.direction == GDK_SCROLL_SMOOTH);
      g_assert_cmpuint (property_names->len, ==, 2);
      g_assert_cmpstr (g_array_index (property_names, char *, 0), ==, "delta_x");
      g_assert_cmpstr (g_array_index (property_names, char *, 1), ==, "delta_y");
      break;
    default:
      /* Event type is unsupported */
      g_assert_not_reached ();
      break;
    }
}

static void
validate_relative_prop_values (GdkEvent *event,
                               GArray   *property_values,
                               gdouble   coef)
{
  switch (event->type)
    {
    case GDK_SCROLL:
      g_assert (event->scroll.direction == GDK_SCROLL_SMOOTH);
      g_assert_cmpuint (property_values->len, ==, 2);
      g_assert_cmpfloat_with_epsilon (event->scroll.delta_x, 1.0 * coef, EPSILON);
      g_assert_cmpfloat_with_epsilon (event->scroll.delta_y, 2.0 * coef, EPSILON);
      break;
    default:
      /* Event type is unsupported */
      g_assert_not_reached ();
      break;
    }
}

static void
validate_prop_names (GdkEvent                 *event,
                     GArray                   *names,
                     GdkInterpolationCategory  category)
{
  switch (category)
    {
    case GDK_INTERP_ABSOLUTE:
      return validate_absolute_prop_names (event, names);
    case GDK_INTERP_RELATIVE:
      return validate_relative_prop_names (event, names);
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
validate_prop_values (GdkEvent                 *event,
                      GArray                   *values,
                      gdouble                   coef,
                      GdkInterpolationCategory  category)
{
  switch (category)
    {
    case GDK_INTERP_ABSOLUTE:
      return validate_absolute_prop_values (event, values, coef);
    case GDK_INTERP_RELATIVE:
      return validate_relative_prop_values (event, values, coef);
    default:
      g_assert_not_reached ();
      break;
    }
}

static void
test_event_accessors (GdkEvent                 *event,
                      GdkInterpolationCategory  category)
{
  GArray *property_values;
  GArray *property_names;
  gdouble *value;
  int i;

  g_assert_nonnull (event);

  property_values = g_array_new (FALSE, FALSE, sizeof (gdouble));
  property_names = g_array_new (FALSE, FALSE, sizeof (char *));

  /*
   * Verify accessors
   */

  gdk_event_get_interpolation_prop_names (event, property_names, category);
  gdk_event_get_values_for_interpolation (event, property_values, category);

  g_assert_cmpuint (property_names->len, ==, property_values->len);

  /* Initial values should all be 0.0 */
  validate_prop_names (event, property_names, category);
  validate_prop_values (event, property_values, 0.0, category);

  print_event (event, category == GDK_INTERP_ABSOLUTE ? "Pre Absolute" : "Pre Relative");

  /* Define new prop values to a 1-based series of consecutive numbers */
  for (i = 0; i < property_values->len; i++)
    {
      value = &g_array_index (property_values, gdouble, i);
      *value = i + 1.0;
    }

  /* Set the values and validate */
  gdk_event_set_interpolated_values (event, property_values, category);
  validate_prop_values (event, property_values, 1.0, category);

  print_event (event, category == GDK_INTERP_ABSOLUTE ? "Post Absolute" : "Post Relative");

  /* Release resources */
  g_array_free (property_values, TRUE);
  g_array_free (property_names, TRUE);
}

/*
 * FIXME create the event in the test setup function, free the event in the
 *       test cleanup function and do the test in the action function.
 */
void
interpolation_test_events_accessors (Fixture       *fixture,
                                    gconstpointer  test_data)
{
  GdkEvent *event;

  /* Smooth scroll */
  GDK_NOTE (EVENTS, g_message ("Smooth scroll event"));

  event = make_event (GDK_SCROLL, 0, 0, INPUT_PHASE_UPDATE);
  test_event_accessors (event, GDK_INTERP_ABSOLUTE);
  test_event_accessors (event, GDK_INTERP_RELATIVE);
  gdk_event_free (event);

  /* Done */
  GDK_NOTE (EVENTS, g_message ("All good"));
}
