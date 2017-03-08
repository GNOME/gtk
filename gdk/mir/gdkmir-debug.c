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

#include <mir_toolkit/events/window_placement.h>

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
    case mir_window_attrib_dpi:
      g_printerr ("dpi");
      break;
    case mir_window_attrib_visibility:
      g_printerr ("visibility");
      break;
    case mir_window_attrib_preferred_orientation:
      g_printerr ("preferred_orientation");
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
_gdk_mir_print_prompt_session_state_change_event (const MirPromptSessionEvent *event)
{
  g_printerr ("PROMPT_SESSION_STATE_CHANGE\n");
  g_printerr (" State ");

  switch (mir_prompt_session_event_get_state (event))
    {
    case mir_prompt_session_state_stopped:
      g_printerr ("stopped");
      break;
    case mir_prompt_session_state_started:
      g_printerr ("started");
      break;
    case mir_prompt_session_state_suspended:
      g_printerr ("suspended");
      break;
    default:
      g_printerr ("%u", mir_prompt_session_event_get_state (event));
      break;
    }

  g_printerr ("\n");
}

static void
_gdk_mir_print_orientation_event (const MirOrientationEvent *event)
{
  g_printerr ("ORIENTATION\n");
  g_printerr (" Direction ");

  switch (mir_orientation_event_get_direction (event))
    {
    case mir_orientation_normal:
      g_printerr ("normal");
      break;
    case mir_orientation_left:
      g_printerr ("left");
      break;
    case mir_orientation_inverted:
      g_printerr ("inverted");
      break;
    case mir_orientation_right:
      g_printerr ("right");
      break;
    default:
      g_printerr ("%u", mir_orientation_event_get_direction (event));
      break;
    }

  g_printerr ("\n");
}

static void
_gdk_mir_print_close_event (void)
{
  g_printerr ("CLOSED\n");
}

static void
_gdk_mir_print_keymap_event (const MirKeymapEvent *event)
{
  g_printerr ("KEYMAP\n");
}

static void
_gdk_mir_print_window_output_event (const MirWindowOutputEvent *event)
{
  g_printerr ("WINDOW_OUTPUT\n");
  g_printerr (" DPI %d\n", mir_window_output_event_get_dpi (event));
  g_printerr (" Form Factor ");

  switch (mir_window_output_event_get_form_factor (event))
    {
    case mir_form_factor_unknown:
      g_printerr ("unknown");
      break;
    case mir_form_factor_phone:
      g_printerr ("phone");
      break;
    case mir_form_factor_tablet:
      g_printerr ("tablet");
      break;
    case mir_form_factor_monitor:
      g_printerr ("monitor");
      break;
    case mir_form_factor_tv:
      g_printerr ("tv");
      break;
    case mir_form_factor_projector:
      g_printerr ("projector");
      break;
    default:
      g_printerr ("%u", mir_window_output_event_get_form_factor (event));
      break;
    }

  g_printerr ("\n");
  g_printerr (" Scale %f\n", mir_window_output_event_get_scale (event));
  g_printerr (" Refresh Rate %lf\n", mir_window_output_event_get_refresh_rate (event));
  g_printerr (" Output ID %u\n", mir_window_output_event_get_output_id (event));
}

static void
_gdk_mir_print_input_device_state_event (const MirInputDeviceStateEvent *event)
{
  MirPointerButtons buttons;
  MirInputEventModifiers modifiers;
  gint i;
  gint j;

  g_printerr ("INPUT_DEVICE_STATE\n");
  g_printerr (" Pointer Buttons\n");
  buttons = mir_input_device_state_event_pointer_buttons (event);

  if (buttons == 0)
    g_printerr ("  none\n");
  else
    {
      if (buttons & mir_pointer_button_primary)
        g_printerr ("  primary\n");
      if (buttons & mir_pointer_button_secondary)
        g_printerr ("  secondary\n");
      if (buttons & mir_pointer_button_tertiary)
        g_printerr ("  tertiary\n");
      if (buttons & mir_pointer_button_back)
        g_printerr ("  back\n");
      if (buttons & mir_pointer_button_forward)
        g_printerr ("  forward\n");
      if (buttons & mir_pointer_button_side)
        g_printerr ("  side\n");
      if (buttons & mir_pointer_button_extra)
        g_printerr ("  extra\n");
      if (buttons & mir_pointer_button_task)
        g_printerr ("  task\n");
    }

  g_printerr (" Pointer Axis\n");
  g_printerr ("  X %f\n", mir_input_device_state_event_pointer_axis (event, mir_pointer_axis_x));
  g_printerr ("  Y %f\n", mir_input_device_state_event_pointer_axis (event, mir_pointer_axis_y));
  g_printerr ("  V Scroll %f\n", mir_input_device_state_event_pointer_axis (event, mir_pointer_axis_vscroll));
  g_printerr ("  H Scroll %f\n", mir_input_device_state_event_pointer_axis (event, mir_pointer_axis_hscroll));
  g_printerr ("  Relative X %f\n", mir_input_device_state_event_pointer_axis (event, mir_pointer_axis_relative_x));
  g_printerr ("  Relative Y %f\n", mir_input_device_state_event_pointer_axis (event, mir_pointer_axis_relative_y));
  g_printerr (" Time %ld\n", mir_input_device_state_event_time (event));
  g_printerr (" Event Modifiers\n");
  modifiers = mir_input_device_state_event_modifiers (event);

  if (modifiers & mir_input_event_modifier_none)
    g_printerr ("  none\n");
  if (modifiers & mir_input_event_modifier_alt)
    g_printerr ("  alt\n");
  if (modifiers & mir_input_event_modifier_alt_left)
    g_printerr ("  alt_left\n");
  if (modifiers & mir_input_event_modifier_alt_right)
    g_printerr ("  alt_right\n");
  if (modifiers & mir_input_event_modifier_shift)
    g_printerr ("  shift\n");
  if (modifiers & mir_input_event_modifier_shift_left)
    g_printerr ("  shift_left\n");
  if (modifiers & mir_input_event_modifier_shift_right)
    g_printerr ("  shift_right\n");
  if (modifiers & mir_input_event_modifier_sym)
    g_printerr ("  sym\n");
  if (modifiers & mir_input_event_modifier_function)
    g_printerr ("  function\n");
  if (modifiers & mir_input_event_modifier_ctrl)
    g_printerr ("  ctrl\n");
  if (modifiers & mir_input_event_modifier_ctrl_left)
    g_printerr ("  ctrl_left\n");
  if (modifiers & mir_input_event_modifier_ctrl_right)
    g_printerr ("  ctrl_right\n");
  if (modifiers & mir_input_event_modifier_meta)
    g_printerr ("  meta\n");
  if (modifiers & mir_input_event_modifier_meta_left)
    g_printerr ("  meta_left\n");
  if (modifiers & mir_input_event_modifier_meta_right)
    g_printerr ("  meta_right\n");
  if (modifiers & mir_input_event_modifier_caps_lock)
    g_printerr ("  caps_lock\n");
  if (modifiers & mir_input_event_modifier_num_lock)
    g_printerr ("  num_lock\n");
  if (modifiers & mir_input_event_modifier_scroll_lock)
    g_printerr ("  scroll_lock\n");

  for (i = 0; i < mir_input_device_state_event_device_count (event); i++)
    {
      g_printerr (" Device %ld\n", mir_input_device_state_event_device_id (event, i));

      for (j = 0; j < mir_input_device_state_event_device_pressed_keys_count (event, i); j++)
        g_printerr ("  Pressed %u\n", mir_input_device_state_event_device_pressed_keys_for_index (event, i, j));

      g_printerr ("  Pointer Buttons\n");
      buttons = mir_input_device_state_event_device_pointer_buttons (event, i);

      if (buttons == 0)
        g_printerr ("   none\n");
      else
        {
          if (buttons & mir_pointer_button_primary)
            g_printerr ("   primary\n");
          if (buttons & mir_pointer_button_secondary)
            g_printerr ("   secondary\n");
          if (buttons & mir_pointer_button_tertiary)
            g_printerr ("   tertiary\n");
          if (buttons & mir_pointer_button_back)
            g_printerr ("   back\n");
          if (buttons & mir_pointer_button_forward)
            g_printerr ("   forward\n");
          if (buttons & mir_pointer_button_side)
            g_printerr ("   side\n");
          if (buttons & mir_pointer_button_extra)
            g_printerr ("   extra\n");
          if (buttons & mir_pointer_button_task)
            g_printerr ("   task\n");
        }
    }
}

static void
_gdk_mir_print_window_placement_event (const MirWindowPlacementEvent *event)
{
  MirRectangle rect = mir_window_placement_get_relative_position (event);

  g_printerr ("WINDOW_PLACEMENT\n");
  g_printerr (" X %d\n", rect.left);
  g_printerr (" Y %d\n", rect.top);
  g_printerr (" Width %u\n", rect.width);
  g_printerr (" Height %u\n", rect.height);
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
    case mir_event_type_prompt_session_state_change:
      _gdk_mir_print_prompt_session_state_change_event (mir_event_get_prompt_session_event (event));
      break;
    case mir_event_type_orientation:
      _gdk_mir_print_orientation_event (mir_event_get_orientation_event (event));
      break;
    case mir_event_type_close_window:
      _gdk_mir_print_close_event ();
      break;
    case mir_event_type_keymap:
      _gdk_mir_print_keymap_event (mir_event_get_keymap_event (event));
      break;
    case mir_event_type_window_output:
      _gdk_mir_print_window_output_event (mir_event_get_window_output_event (event));
      break;
    case mir_event_type_input_device_state:
      _gdk_mir_print_input_device_state_event (mir_event_get_input_device_state_event (event));
      break;
    case mir_event_type_window_placement:
      _gdk_mir_print_window_placement_event (mir_event_get_window_placement_event (event));
      break;
    default:
      g_printerr ("EVENT %u\n", mir_event_get_type (event));
      break;
    }
}
