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

void
_gdk_mir_print_modifiers (unsigned int modifiers)
{
  g_printerr (" Modifiers");
  if ((modifiers & mir_key_modifier_alt) != 0)
    g_printerr (" alt");
  if ((modifiers & mir_key_modifier_alt_left) != 0)
    g_printerr (" alt-left");
  if ((modifiers & mir_key_modifier_alt_right) != 0)
    g_printerr (" alt-right");
  if ((modifiers & mir_key_modifier_shift) != 0)
    g_printerr (" shift");
  if ((modifiers & mir_key_modifier_shift_left) != 0)
    g_printerr (" shift-left");
  if ((modifiers & mir_key_modifier_shift_right) != 0)
    g_printerr (" shift-right");
  if ((modifiers & mir_key_modifier_sym) != 0)
    g_printerr (" sym");
  if ((modifiers & mir_key_modifier_function) != 0)
    g_printerr (" function");
  if ((modifiers & mir_key_modifier_ctrl) != 0)
    g_printerr (" ctrl");
  if ((modifiers & mir_key_modifier_ctrl_left) != 0)
    g_printerr (" ctrl-left");
  if ((modifiers & mir_key_modifier_ctrl_right) != 0)
    g_printerr (" ctrl-right");
  if ((modifiers & mir_key_modifier_meta) != 0)
    g_printerr (" meta");
  if ((modifiers & mir_key_modifier_meta_left) != 0)
    g_printerr (" meta-left");
  if ((modifiers & mir_key_modifier_meta_right) != 0)
    g_printerr (" meta-right");
  if ((modifiers & mir_key_modifier_caps_lock) != 0)
    g_printerr (" caps-lock");
  if ((modifiers & mir_key_modifier_num_lock) != 0)
    g_printerr (" num-lock");
  if ((modifiers & mir_key_modifier_scroll_lock) != 0)
    g_printerr (" scroll-lock");
  g_printerr ("\n");
}

void
_gdk_mir_print_key_event (const MirKeyEvent *event)
{
  g_printerr ("KEY\n");
  g_printerr (" Device %i\n", event->device_id);
  g_printerr (" Source %i\n", event->source_id);
  g_printerr (" Action ");
  switch (event->action)
    {
    case mir_key_action_down:
      g_printerr ("down");
      break;
    case mir_key_action_up:
      g_printerr ("up");
      break;
    case mir_key_action_multiple:
      g_printerr ("multiple");
      break;
    default:
      g_printerr ("%u", event->action);
      break;
    }
  g_printerr ("\n");
  g_printerr (" Flags");
  if ((event->flags & mir_key_flag_woke_here) != 0)
    g_printerr (" woke-here");
  if ((event->flags & mir_key_flag_soft_keyboard) != 0)
    g_printerr (" soft-keyboard");
  if ((event->flags & mir_key_flag_keep_touch_mode) != 0)
    g_printerr (" keep-touch-mode");
  if ((event->flags & mir_key_flag_from_system) != 0)
    g_printerr (" from-system");
  if ((event->flags & mir_key_flag_editor_action) != 0)
    g_printerr (" editor-action");
  if ((event->flags & mir_key_flag_canceled) != 0)
    g_printerr (" canceled");
  if ((event->flags & mir_key_flag_virtual_hard_key) != 0)
    g_printerr (" virtual-hard-key");
  if ((event->flags & mir_key_flag_long_press) != 0)
    g_printerr (" long-press");
  if ((event->flags & mir_key_flag_canceled_long_press) != 0)
    g_printerr (" canceled-long-press");
  if ((event->flags & mir_key_flag_tracking) != 0)
    g_printerr (" tracking");
  if ((event->flags & mir_key_flag_fallback) != 0)
    g_printerr (" fallback");
  g_printerr ("\n");
  _gdk_mir_print_modifiers (event->modifiers);
  g_printerr (" Key Code %i\n", event->key_code);
  g_printerr (" Scan Code %i\n", event->scan_code);
  g_printerr (" Repeat Count %i\n", event->repeat_count);
  g_printerr (" Down Time %lli\n", (long long int) event->down_time);
  g_printerr (" Event Time %lli\n", (long long int) event->event_time);
  g_printerr (" Is System Key %s\n", event->is_system_key ? "true" : "false");
}

void
_gdk_mir_print_motion_event (const MirMotionEvent *event)
{
  size_t i;

  g_printerr ("MOTION\n");
  g_printerr (" Device %i\n", event->device_id);
  g_printerr (" Source %i\n", event->source_id);
  g_printerr (" Action ");
  switch (event->action)
    {
    case mir_motion_action_down:
      g_printerr ("down");
      break;
    case mir_motion_action_up:
      g_printerr ("up");
      break;
    case mir_motion_action_move:
      g_printerr ("move");
      break;
    case mir_motion_action_cancel:
      g_printerr ("cancel");
      break;
    case mir_motion_action_outside:
      g_printerr ("outside");
      break;
    case mir_motion_action_pointer_down:
      g_printerr ("pointer-down");
      break;
    case mir_motion_action_pointer_up:
      g_printerr ("pointer-up");
      break;
    case mir_motion_action_hover_move:
      g_printerr ("hover-move");
      break;
    case mir_motion_action_scroll:
      g_printerr ("scroll");
      break;
    case mir_motion_action_hover_enter:
      g_printerr ("hover-enter");
      break;
    case mir_motion_action_hover_exit:
      g_printerr ("hover-exit");
      break;
    default:
      g_printerr ("%u", event->action);
    }
  g_printerr ("\n");
  g_printerr (" Flags");
  switch (event->flags)
    {
    case mir_motion_flag_window_is_obscured:
      g_printerr (" window-is-obscured");
      break;
    }
  g_printerr ("\n");
  _gdk_mir_print_modifiers (event->modifiers);
  g_printerr (" Edge Flags %i\n", event->edge_flags);
  g_printerr (" Button State");
  switch (event->button_state)
    {
    case mir_motion_button_primary:
      g_printerr (" primary");
      break;
    case mir_motion_button_secondary:
      g_printerr (" secondary");
      break;
    case mir_motion_button_tertiary:
      g_printerr (" tertiary");
      break;
    case mir_motion_button_back:
      g_printerr (" back");
      break;
    case mir_motion_button_forward:
      g_printerr (" forward");
      break;
    }
  g_printerr ("\n");
  g_printerr (" Offset (%f, %f)\n", event->x_offset, event->y_offset);
  g_printerr (" Precision (%f, %f)\n", event->x_precision, event->y_precision);
  g_printerr (" Down Time %lli\n", (long long int) event->down_time);
  g_printerr (" Event Time %lli\n", (long long int) event->event_time);
  g_printerr (" Pointer Coordinates\n");
  for (i = 0; i < event->pointer_count; i++)
    {
      g_printerr ("  ID=%i location=(%f, %f) raw=(%f, %f) touch=(%f, %f) size=%f pressure=%f orientation=%f scroll=(%f, %f) tool=",
                  event->pointer_coordinates[i].id,
                  event->pointer_coordinates[i].x, event->pointer_coordinates[i].y,
                  event->pointer_coordinates[i].raw_x, event->pointer_coordinates[i].raw_y,
                  event->pointer_coordinates[i].touch_major, event->pointer_coordinates[i].touch_minor,
                  event->pointer_coordinates[i].size,
                  event->pointer_coordinates[i].pressure,
                  event->pointer_coordinates[i].orientation,
                  event->pointer_coordinates[i].hscroll, event->pointer_coordinates[i].vscroll);
      switch (event->pointer_coordinates[i].tool_type)
        {
        case mir_motion_tool_type_unknown:
          g_printerr ("unknown");
          break;
        case mir_motion_tool_type_finger:
          g_printerr ("finger");
          break;
        case mir_motion_tool_type_stylus:
          g_printerr ("stylus");
          break;
        case mir_motion_tool_type_mouse:
          g_printerr ("mouse");
          break;
        case mir_motion_tool_type_eraser:
          g_printerr ("eraser");
          break;
        default:
          g_printerr ("%u", event->pointer_coordinates[i].tool_type);
          break;
        }
      g_printerr ("\n");
    }
}

void
_gdk_mir_print_surface_event (const MirSurfaceEvent *event)
{
  g_printerr ("SURFACE\n");
  g_printerr (" Surface %i\n", event->id);
  g_printerr (" Attribute ");
  switch (event->attrib)
    {
    case mir_surface_attrib_type:
      g_printerr ("type");
      break;
    case mir_surface_attrib_state:
      g_printerr ("state");
      break;
    case mir_surface_attrib_swapinterval:
      g_printerr ("swapinterval");
      break;
    case mir_surface_attrib_focus:
      g_printerr ("focus");
      break;
    default:
      g_printerr ("%u", event->attrib);
      break;
    }
  g_printerr ("\n");
  g_printerr (" Value %i\n", event->value);
}

void
_gdk_mir_print_resize_event (const MirResizeEvent *event)
{
  g_printerr ("RESIZE\n");
  g_printerr (" Surface %i\n", event->surface_id);
  g_printerr (" Size (%i, %i)\n", event->width, event->height);
}

void
_gdk_mir_print_event (const MirEvent *event)
{
  switch (event->type)
    {
    case mir_event_type_key:
      _gdk_mir_print_key_event (&event->key);
      break;
    case mir_event_type_motion:
      _gdk_mir_print_motion_event (&event->motion);
      break;
    case mir_event_type_surface:
      _gdk_mir_print_surface_event (&event->surface);
      break;
    case mir_event_type_resize:
      _gdk_mir_print_resize_event (&event->resize);
      break;
    default:
      g_printerr ("EVENT %u\n", event->type);
      break;
    }
}
