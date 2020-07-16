/* GDK - The GIMP Drawing Kit
 *
 * gdkunittestmain.c: input event interpolation unit test
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


int
main (int argc, char *argv[])
{
  g_test_init (&argc, &argv, NULL);

  gdk_init (NULL, NULL);

  g_test_bug_base ("https://gitlab.gnome.org/GNOME/gtk/issues");

  /* For 60Hz display the display interval would be about 16.66ms, however it's
     easier to use whole numbers for the test. The actual numbers don't matter,
     the important thing is the ratio between the input events interval and the
     display interval. */

  g_test_add ("/interpolation/interpolation_test_events_accessors",
              Fixture, NULL,
              NULL,
              interpolation_test_events_accessors,
              NULL);

  static TestData scroll_history_properties;
  scroll_history_properties.event_type = GDK_SCROLL;
  scroll_history_properties.display_interval = EVENT_INTERVAL * 1.0;
  g_test_add ("/interpolation/scroll_history_properties",
              Fixture, &scroll_history_properties,
              interpolation_test_setup,
              interpolation_test_history_properties,
              interpolation_test_teardown);

  static TestData scroll_display_is_slower;
  scroll_display_is_slower.event_type = GDK_SCROLL;
  scroll_display_is_slower.display_interval = EVENT_INTERVAL * 1.5;
  g_test_add ("/interpolation/scroll_display_is_slower",
              Fixture, &scroll_display_is_slower,
              interpolation_test_setup,
              interpolation_test_constant_speed,
              interpolation_test_teardown);

  static TestData scroll_same_interval;
  scroll_same_interval.event_type = GDK_SCROLL;
  scroll_same_interval.display_interval = EVENT_INTERVAL * 1.0;
  g_test_add ("/interpolation/scroll_same_interval",
              Fixture, &scroll_same_interval,
              interpolation_test_setup,
              interpolation_test_constant_speed,
              interpolation_test_teardown);

  static TestData scroll_display_is_faster;
  scroll_display_is_faster.event_type = GDK_SCROLL;
  scroll_display_is_faster.display_interval = EVENT_INTERVAL * 0.5;
  g_test_add ("/interpolation/scroll_display_is_faster",
              Fixture, &scroll_display_is_faster,
              interpolation_test_setup,
              interpolation_test_constant_speed,
              interpolation_test_teardown);

  return g_test_run ();
}
