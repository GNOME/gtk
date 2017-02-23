/*
 * Copyright © 2014 Canonical Ltd
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include "gdkmir-private.h"

static void
_gdk_mir_print_modifiers (unsigned int modifiers)
{
  g_printerr (" Modifiers");
  if ((modifiers & mir_input_event_modifier_alt) != 0)
    g_printerr (" alt");
  if ((modifiers & mir_input_event_modifier_alt_left) != 0)
    g_printerr (" alt-left");
  if ((modifiers & mir_input_event_modifier_alt_right) != 0)
    g_printerr (" alt-right");
  if ((modifiers & mir_input_event_modifier_shift) != 0)
    g_printerr (" shift");
  if ((modifiers & mir_input_event_modifier_shift_left) != 0)
    g_printerr (" shift-left");
  if ((modifiers & mir_input_event_modifier_shift_right) != 0)
    g_printerr (" shift-right");
  if ((modifiers & mir_input_event_modifier_sym) != 0)
    g_printerr (" sym");
  if ((modifiers & mir_input_event_modifier_function) != 0)
    g_printerr (" function");
  if ((modifiers & mir_input_event_modifier_ctrl) != 0)
    g_printerr (" ctrl");
  if ((modifiers & mir_input_event_modifier_ctrl_left) != 0)
    g_printerr (" ctrl-left");
  if ((modifiers & mir_input_event_modifier_ctrl_right) != 0)
    g_printerr (" ctrl-right");
  if ((modifiers & mir_input_event_modifier_meta) != 0)
    g_printerr (" meta");
  if ((modifiers & mir_input_event_modifier_meta_left) != 0)
    g_printerr (" meta-left");
  if ((modifiers & mir_input_event_modifier_meta_right) != 0)
    g_printerr (" meta-right");
  if ((modifiers & mir_input_event_modifier_caps_lock) != 0)
    g_printerr (" caps-lock");
  if ((modifiers & mir_input_event_modifier_num_lock) != 0)
    g_printerr (" num-lock");
  if ((modifiers & mir_input_event_modifier_scroll_lock) != 0)
    g_printerr (" scroll-lock");
  g_printerr ("\n");
}

static void
_gdk_mir_print_key_event (const MirInputEvent *event)
{
  const MirKeyboardEvent *keyboard_event = mir_input_event_get_keyboard_event (event);

  if (!keyboard_event)
    return;

  g_printerr ("KEY\n");
  g_printerr (" Device %lld\n", (long long int) mir_input_event_get_device_id (event));
  g_printerr (" Action ");
  switch (mir_keyboard_event_action (keyboard_event))
    {
    case mir_keyboard_action_down:
      g_printerr ("down");
      break;
    case mir_keyboard_action_up:
      g_printerr ("up");
      break;
    case mir_keyboard_action_repeat:
      g_printerr ("repeat");
      break;
    default:
      g_printerr ("%u", mir_keyboard_event_action (keyboard_event));
      break;
    }
  g_printerr ("\n");
  _gdk_mir_print_modifiers (mir_keyboard_event_modifiers (keyboard_event));
  g_printerr (" Key Code %i\n", mir_keyboard_event_key_code (keyboard_event));
  g_printerr (" Scan Code %i\n", mir_keyboard_event_scan_code (keyboard_event));
  g_printerr (" Event Time %lli\n", (long long int) mir_input_event_get_event_time (event));
}

static void
_gdk_mir_print_touch_event (const MirInputEvent *event)
{
  const MirTouchEvent *touch_event = mir_input_event_get_touch_event (event);
  guint i;
  guint n;

  if (!touch_event)
    return;

  g_printerr ("TOUCH\n");
  g_printerr (" Device %lld\n", (long long int) mir_input_event_get_device_id (event));
  g_printerr (" Event Time %lld\n", (long long int) mir_input_event_get_event_time (event));
  _gdk_mir_print_modifiers (mir_touch_event_modifiers (touch_event));
  n = mir_touch_event_point_count (touch_event);

  for (i = 0; i < n; i++)
    {
      g_printerr (" [%u] (%u/%u) ", mir_touch_event_id (touch_event, i), i + 1, n);
      switch (mir_touch_event_action (touch_event, i))
        {
        case mir_touch_action_down:
          g_printerr ("Down");
          break;
        case mir_touch_action_up:
          g_printerr ("Up");
          break;
        case mir_touch_action_change:
          g_printerr ("Change");
          break;
        default:
          g_printerr ("%u", mir_touch_event_action (touch_event, i));
          break;
        }
      switch (mir_touch_event_tooltype (touch_event, i))
        {
        default:
        case mir_touch_tooltype_unknown:
          g_printerr (" ? ");
          break;
        case mir_touch_tooltype_finger:
          g_printerr (" finger ");
          break;
        case mir_touch_tooltype_stylus:
          g_printerr (" stylus ");
          break;
        }
      g_printerr ("\n  x: %f y: %f P: %f A: %f B: %f d: %f\n",
                  mir_touch_event_axis_value (touch_event, i, mir_touch_axis_x),
                  mir_touch_event_axis_value (touch_event, i, mir_touch_axis_y),
                  mir_touch_event_axis_value (touch_event, i, mir_touch_axis_pressure),
                  mir_touch_event_axis_value (touch_event, i, mir_touch_axis_touch_major),
                  mir_touch_event_axis_value (touch_event, i, mir_touch_axis_touch_minor),
                  mir_touch_event_axis_value (touch_event, i, mir_touch_axis_size));
    }
}

static void
_gdk_mir_print_motion_event (const MirInputEvent *event)
{
  const MirPointerEvent *pointer_event = mir_input_event_get_pointer_event (event);

  if (!pointer_event)
    return;

  g_printerr ("MOTION\n");
  g_printerr (" Device %lld\n", (long long int) mir_input_event_get_device_id (event));
  g_printerr (" Action ");
  switch (mir_pointer_event_action (pointer_event))
    {
    case mir_pointer_action_button_down:
      g_printerr ("down");
      break;
    case mir_pointer_action_button_up:
      g_printerr ("up");
      break;
    case mir_pointer_action_enter:
      g_printerr ("enter");
      break;
    case mir_pointer_action_leave:
      g_printerr ("leave");
      break;
    case mir_pointer_action_motion:
      g_printerr ("motion");
      break;
    default:
      g_printerr ("%u", mir_pointer_event_action (pointer_event));
    }
  g_printerr ("\n");
  _gdk_mir_print_modifiers (mir_pointer_event_modifiers (pointer_event));
  g_printerr (" Button State");
  if (mir_pointer_event_button_state (pointer_event, mir_pointer_button_primary))
    g_printerr (" primary");
  if (mir_pointer_event_button_state (pointer_event, mir_pointer_button_secondary))
    g_printerr (" secondary");
  if (mir_pointer_event_button_state (pointer_event, mir_pointer_button_tertiary))
    g_printerr (" tertiary");
  if (mir_pointer_event_button_state (pointer_event, mir_pointer_button_back))
    g_printerr (" back");
  if (mir_pointer_event_button_state (pointer_event, mir_pointer_button_forward))
    g_printerr (" forward");
  g_printerr ("\n");
  g_printerr (" Offset (%f, %f)\n", mir_pointer_event_axis_value (pointer_event, mir_pointer_axis_x),
                                    mir_pointer_event_axis_value (pointer_event, mir_pointer_axis_y));
  g_printerr (" Scroll (%f, %f)\n", mir_pointer_event_axis_value (pointer_event, mir_pointer_axis_hscroll),
                                    mir_pointer_event_axis_value (pointer_event, mir_pointer_axis_vscroll));
  g_printerr (" Event Time %lli\n", (long long int) mir_input_event_get_event_time (event));
}

static void
_gdk_mir_print_input_event (const MirInputEvent *event)
{
  g_printerr ("INPUT\n");
}

static void
_gdk_mir_print_window_event (const MirWindowEvent *event)
{
  g_printerr ("WINDOW\n");
  g_printerr (" Attribute ");
  switch (mir_window_event_get_attribute (event))
    {
    case mir_window_attrib_type:
      g_printerr ("type");
      break;
    case mir_window_attrib_state:
      g_printerr ("state");
      break;
    case mir_window_attrib_swapinterval:
      g_printerr ("swapinterval");
      break;
    case mir_window_attrib_focus:
      g_printerr ("focus");
      break;
    default:
      g_printerr ("%u", mir_window_event_get_attribute (event));
      break;
    }
  g_printerr ("\n");
  g_printerr (" Value %i\n", mir_window_event_get_attribute_value (event));
}

static void
_gdk_mir_print_resize_event (const MirResizeEvent *event)
{
  g_printerr ("RESIZE\n");
  g_printerr (" Size (%i, %i)\n", mir_resize_event_get_width (event), mir_resize_event_get_height (event));
}

static void
_gdk_mir_print_close_event (void)
{
  g_printerr ("CLOSED\n");
}

void
_gdk_mir_print_event (const MirEvent *event)
{
  const MirInputEvent *input_event;

  switch (mir_event_get_type (event))
    {
    case mir_event_type_input:
      input_event = mir_event_get_input_event (event);

      switch (mir_input_event_get_type (input_event))
        {
          case mir_input_event_type_key:
            _gdk_mir_print_key_event (mir_event_get_input_event (event));
            break;
          case mir_input_event_type_touch:
            _gdk_mir_print_touch_event (mir_event_get_input_event (event));
            break;
          case mir_input_event_type_pointer:
            _gdk_mir_print_motion_event (mir_event_get_input_event (event));
            break;
          default:
            _gdk_mir_print_input_event (mir_event_get_input_event (event));
            break;
        }
      break;
    case mir_event_type_key:
      _gdk_mir_print_key_event (mir_event_get_input_event (event));
      break;
    case mir_event_type_motion:
      _gdk_mir_print_motion_event (mir_event_get_input_event (event));
      break;
    case mir_event_type_window:
      _gdk_mir_print_window_event (mir_event_get_window_event (event));
      break;
    case mir_event_type_resize:
      _gdk_mir_print_resize_event (mir_event_get_resize_event (event));
      break;
    case mir_event_type_close_window:
      _gdk_mir_print_close_event ();
      break;
    default:
      g_printerr ("EVENT %u\n", mir_event_get_type (event));
      break;
    }
}
