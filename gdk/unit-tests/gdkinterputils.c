/* GDK - The GIMP Drawing Kit
 *
 * gdkinterputils.c: input event interpolation unit test utilities
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


static GdkTouchpadGesturePhase
input_phase_to_touchpad_phase (InputPhase input_phase)
{
  switch (input_phase)
    {
    case INPUT_PHASE_BEGIN:
      return GDK_TOUCHPAD_GESTURE_PHASE_BEGIN;
    case INPUT_PHASE_UPDATE:
      return GDK_TOUCHPAD_GESTURE_PHASE_UPDATE;
    case INPUT_PHASE_END:
      return GDK_TOUCHPAD_GESTURE_PHASE_END;
    case INPUT_PHASE_CANCEL:
      return GDK_TOUCHPAD_GESTURE_PHASE_CANCEL;
    default:
      g_assert_not_reached ();
    }
}

/*
 * make_event() is loosely based on _gdk_make_event()
 * It is used here to simulate 'real' events.
 */
GdkEvent *
make_event (GdkEventType type,
            guint32 time,
            GdkModifierType state,
            InputPhase input_phase)
{
  GdkEvent *event = gdk_event_new (type);

  event->any.send_event = FALSE;

  gdk_event_set_time (event, time);
  gdk_event_set_state (event, state);

  switch (type)
    {
    case GDK_SCROLL:
      event->scroll.direction = GDK_SCROLL_SMOOTH;
      event->scroll.x = 100;
      event->scroll.y = 200;
      event->scroll.x_root = 300;
      event->scroll.y_root = 400;
      event->scroll.is_stop = (input_phase == INPUT_PHASE_END);
      break;

    case GDK_TOUCHPAD_SWIPE:
      event->touchpad_swipe.x = 100;
      event->touchpad_swipe.y = 200;
      event->touchpad_swipe.x_root = 300;
      event->touchpad_swipe.y_root = 400;
      event->touchpad_swipe.phase = input_phase_to_touchpad_phase (input_phase);
      event->touchpad_swipe.n_fingers = 2;
      break;

    case GDK_TOUCHPAD_PINCH:
      event->touchpad_pinch.x = 100;
      event->touchpad_pinch.y = 200;
      event->touchpad_pinch.x_root = 300;
      event->touchpad_pinch.y_root = 400;
      event->touchpad_pinch.phase = input_phase_to_touchpad_phase (input_phase);
      event->touchpad_pinch.n_fingers = 2;
      break;

    case GDK_MOTION_NOTIFY:
    case GDK_TOUCH_BEGIN:
    case GDK_TOUCH_UPDATE:
    case GDK_TOUCH_END:
    case GDK_TOUCH_CANCEL:
    case GDK_BUTTON_PRESS:
    case GDK_2BUTTON_PRESS:
    case GDK_3BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_KEY_PRESS:
    case GDK_KEY_RELEASE:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    case GDK_PROPERTY_NOTIFY:
    case GDK_SELECTION_CLEAR:
    case GDK_SELECTION_REQUEST:
    case GDK_SELECTION_NOTIFY:
    case GDK_PROXIMITY_IN:
    case GDK_PROXIMITY_OUT:
    case GDK_DRAG_ENTER:
    case GDK_DRAG_LEAVE:
    case GDK_DRAG_MOTION:
    case GDK_DRAG_STATUS:
    case GDK_DROP_START:
    case GDK_DROP_FINISHED:
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
      g_assert_not_reached ();
    }

  return event;
}

static GString *
dump_props (GdkEvent                 *event,
            GdkInterpolationCategory  category)
{
  /* Arrays to query event properties */
  GString *result;
  GArray *property_names;
  GArray *property_values;
  gdouble value;
  char *name;
  int i;

  result = g_string_new("");

  property_values = g_array_new (FALSE, FALSE, sizeof (gdouble));
  property_names = g_array_new (FALSE, FALSE, sizeof (char *));

  gdk_event_get_interpolation_prop_names (event, property_names, category);
  gdk_event_get_values_for_interpolation (event, property_values, category);

  g_assert_cmpuint (property_names->len, ==, property_values->len);

  for (i = 0; i < property_names->len; i++)
    {
      name = g_array_index (property_names, char *, i);
      value = g_array_index (property_values, gdouble, i);
      g_string_append_printf (result, " %s = %.2f", name, value);
    }

  /* Release resources */
  g_array_free (property_values, TRUE);
  g_array_free (property_names, TRUE);

  return result;
}

void
print_event (GdkEvent *event, char *prefix)
{
  GString *absolute_props;
  GString *relative_props;
  char *event_type_name;

  switch (event->type)
    {
    case GDK_SCROLL:
      event_type_name = "Scroll";
      break;
    case GDK_TOUCHPAD_SWIPE:
      event_type_name = "Swipe";
      break;
    case GDK_TOUCHPAD_PINCH:
      event_type_name = "Pinch";
      break;
    default:
      event_type_name = "Unhandled";
      break;
    }

  absolute_props = dump_props (event, GDK_INTERP_ABSOLUTE);
  relative_props = dump_props (event, GDK_INTERP_RELATIVE);

  GDK_NOTE (EVENTS, g_message ("%s %s time = %d%s%s",
                               prefix,
                               event_type_name,
                               gdk_event_get_time (event),
                               absolute_props->str,
                               relative_props->str));

  g_string_free (absolute_props, TRUE);
  g_string_free (relative_props, TRUE);
}
