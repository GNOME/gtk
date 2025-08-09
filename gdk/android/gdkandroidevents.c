/*
 * Copyright (c) 2024-2025 Florian "sp1rit" <sp1rit@disroot.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "gdkeventsprivate.h"

#include <android/input.h>

#include "gdkandroidinit-private.h"
#include "gdkandroiddisplay-private.h"
#include "gdkandroidseat-private.h"
#include "gdkandroiddevice-private.h"

#include "gdkandroidtoplevel.h"

#include "gdkandroidevents-private.h"

static GdkModifierType
gdk_android_events_meta_to_gdk (gint32 modifiers)
{
  GdkModifierType ret = 0;
  if (modifiers & AMETA_SHIFT_ON)
    ret |= GDK_SHIFT_MASK;
  if (modifiers & AMETA_CAPS_LOCK_ON)
    ret |= GDK_LOCK_MASK;
  if (modifiers & AMETA_CTRL_ON)
    ret |= GDK_CONTROL_MASK;
  if (modifiers & AMETA_ALT_ON)
    ret |= GDK_ALT_MASK;
  if (modifiers & AMETA_META_ON)
    ret |= GDK_META_MASK;
  return ret;
}

static GdkModifierType
gdk_android_events_buttons_to_gdkmods (gint32 buttons)
{
  GdkModifierType ret = 0;
  if (buttons & AMOTION_EVENT_BUTTON_PRIMARY)
    ret |= GDK_BUTTON1_MASK;
  if (buttons & AMOTION_EVENT_BUTTON_SECONDARY)
    ret |= GDK_BUTTON3_MASK; // X11 button numbering
  if (buttons & AMOTION_EVENT_BUTTON_TERTIARY)
    ret |= GDK_BUTTON2_MASK; // ditto
  if (buttons & AMOTION_EVENT_BUTTON_BACK)
    ret |= GDK_BUTTON4_MASK;
  if (buttons & AMOTION_EVENT_BUTTON_FORWARD)
    ret |= GDK_BUTTON5_MASK;
  return ret;
}

static guint
gdk_android_surface_long_hash (guint64 num)
{
  return (gint) (num ^ (num >> 32));
}

// Taken from Thomas Mueller on SO, licensed under CC BY-SA 4.0
// https://stackoverflow.com/a/12996028/10890264
static guint
gdk_android_events_int_hash (guint32 num)
{
  num = ((num >> 16) ^ num) * 0x45d9f3bu;
  num = ((num >> 16) ^ num) * 0x45d9f3bu;
  num = (num >> 16) ^ num;
  return num;
}

#define GDK_ANDROID_EVENTS_COMPARE_MASK(val, mask) \
  (((val) & (mask)) == (mask))

#define GDK_ANDROID_TOUCH_EVENT_TYPE_MASK (3) // least significant 2 bits
static GdkEventType
gdk_android_events_touch_action_to_gdk (const AInputEvent *event, size_t pointer_index)
{
  gint32 action = AMotionEvent_getAction (event);
  gint32 masked_action = action & AMOTION_EVENT_ACTION_MASK;
  if (masked_action == AMOTION_EVENT_ACTION_POINTER_DOWN ||
      masked_action == AMOTION_EVENT_ACTION_POINTER_UP)
    {
      size_t affected_pointer = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
      if (pointer_index != affected_pointer)
        return GDK_TOUCH_UPDATE;
      switch (masked_action)
        {
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
          return GDK_TOUCH_BEGIN;
        case AMOTION_EVENT_ACTION_POINTER_UP:
          return GDK_TOUCH_END;
        default:
          __builtin_unreachable ();
        }
    }

  jint event_type = masked_action & GDK_ANDROID_TOUCH_EVENT_TYPE_MASK;
  switch (event_type)
    {
    case AMOTION_EVENT_ACTION_DOWN:
      return GDK_TOUCH_BEGIN;
    case AMOTION_EVENT_ACTION_UP:
      return GDK_TOUCH_END;
    case AMOTION_EVENT_ACTION_MOVE:
      return GDK_TOUCH_UPDATE;
    case AMOTION_EVENT_ACTION_CANCEL:
      return GDK_TOUCH_CANCEL;
    default:
      __builtin_unreachable ();
    }
}

#define GDK_ANDROID_EVENTS_BUTTON_IS_DIFFERENT(state, prev, mask) \
  (((state) & (mask)) ^ ((prev) & (mask)))
static void
gdk_android_events_emit_button_press (gint32 mask, guint32 state, guint button,
                                      GdkAndroidSurface *surface,
                                      const AInputEvent *event, GdkDevice *dev,
                                      guint32 time, GdkModifierType mods,
                                      gdouble x, gdouble y)
{
  GdkDisplay *display = gdk_surface_get_display ((GdkSurface *) surface);
  GdkDeviceTool *tool = gdk_android_seat_get_device_tool (GDK_ANDROID_DISPLAY (display)->seat,
                                                          AMotionEvent_getToolType (event, 0));
  g_debug ("Mouse %u event: (%d & %d) %p [%s]: %s", button, mask, state, surface, G_OBJECT_TYPE_NAME (surface), (state & mask) != 0 ? "press" : "release");
  GdkEvent *ev = gdk_button_event_new ((state & mask) != 0 ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE,
                                       (GdkSurface *) surface, dev, tool,
                                       time, mods,
                                       button,
                                       x, y,
                                       gdk_android_seat_create_axes_from_motion_event (event, 0));
  gdk_android_seat_consume_event (display, ev);
}

void
gdk_android_events_handle_motion_event (GdkAndroidSurface *surface,
                                        jobject            motion_event,
                                        jint               event_identifier)
{
  GdkAndroidDisplay *display = (GdkAndroidDisplay *) gdk_surface_get_display ((GdkSurface *) surface);

  JNIEnv *env = gdk_android_get_env ();
  const AInputEvent *event = AMotionEvent_fromJava (env, motion_event);

  gint32 masked_action = AMotionEvent_getAction (event) & AMOTION_EVENT_ACTION_MASK;
  gint32 src = AInputEvent_getSource (event);

  GdkDevice *dev = gdk_seat_get_pointer ((GdkSeat *) display->seat);
  GdkAndroidDevice *dev_impl = GDK_ANDROID_DEVICE (dev);

  GdkModifierType mods = gdk_android_events_meta_to_gdk (AMotionEvent_getMetaState (event));
  mods |= gdk_android_events_buttons_to_gdkmods (dev_impl->button_state);

  gint64 time = AMotionEvent_getEventTime (event);

  // Update keyboard focus on motion events only for autohide surfaces
  // This *doesn't really* match the behaviour of Mutter (autohide popups
  // get keyboard focus on present, while non-autohide popups do not),
  // especially as motion events shouldn't update keyboard focus, but it'll
  // work in the grand scheme of things for now.
  if (GDK_IS_ANDROID_TOPLEVEL(surface) || ((GdkSurface *)surface)->autohide)
    {
      GdkDevice *keyboard = gdk_seat_get_keyboard ((GdkSeat *) display->seat);
      gdk_android_device_keyboard_maybe_update_surface_focus ((GdkAndroidDevice *) keyboard, surface);
    }

  if (GDK_ANDROID_EVENTS_COMPARE_MASK (src, AINPUT_SOURCE_TOUCHSCREEN))
    {
      // I think it might be better to drop the down time and only rely on the event identity
      guint base_sequence = gdk_android_surface_long_hash ((guint64) AMotionEvent_getDownTime (event));
      base_sequence ^= (guint) event_identifier;

      size_t pointers = AMotionEvent_getPointerCount (event);
      for (size_t i = 0; i < pointers; i++)
        {
          GdkEventType ev_type = gdk_android_events_touch_action_to_gdk (event, i);

          guint sequence = base_sequence ^ gdk_android_events_int_hash(AMotionEvent_getPointerId (event, i));
          gfloat x = AMotionEvent_getX (event, i) / surface->cfg.scale;
          gfloat y = AMotionEvent_getY (event, i) / surface->cfg.scale;

          GdkEvent *ev = gdk_touch_event_new (ev_type, GUINT_TO_POINTER (sequence), (GdkSurface *) surface,
                                              display->seat->logical_touchscreen,
                                              time, mods,
                                              x, y,
                                              gdk_android_seat_create_axes_from_motion_event (event, i), i == 0);
          gdk_android_seat_consume_event ((GdkDisplay *) display, ev);
        }
    }
  else if (GDK_ANDROID_EVENTS_COMPARE_MASK (src, AINPUT_SOURCE_CLASS_POINTER))
    {
      gfloat x = AMotionEvent_getX (event, 0) / surface->cfg.scale;
      gfloat y = AMotionEvent_getY (event, 0) / surface->cfg.scale;

      if (masked_action == AMOTION_EVENT_ACTION_SCROLL)
        {
          GdkEvent *ev = gdk_scroll_event_new ((GdkSurface *) surface, dev, NULL,
                                               time, mods,
                                               AMotionEvent_getAxisValue (event, AMOTION_EVENT_AXIS_HSCROLL, 0),
                                               AMotionEvent_getAxisValue (event, AMOTION_EVENT_AXIS_VSCROLL, 0),
                                               FALSE, // how am I supposed to know if the current scroll event is the last?
                                               GDK_SCROLL_UNIT_WHEEL,
                                               GDK_SCROLL_RELATIVE_DIRECTION_UNKNOWN);
          gdk_android_seat_consume_event ((GdkDisplay *) display, ev);
        }
      else if (masked_action == AMOTION_EVENT_ACTION_DOWN ||
               masked_action == AMOTION_EVENT_ACTION_UP ||
               masked_action == AMOTION_EVENT_ACTION_CANCEL)
        { // we have to treat cancel like a
          // button up event, as GDK does not
          // provide a cancel mechanism for
          // button events.
          gint32 tool_type = AMotionEvent_getToolType (event, 0);
          if (tool_type == AMOTION_EVENT_TOOL_TYPE_MOUSE || tool_type == AMOTION_EVENT_TOOL_TYPE_FINGER)
            {
              gint32 button_state = AMotionEvent_getButtonState (event);
              if (GDK_ANDROID_EVENTS_BUTTON_IS_DIFFERENT (button_state, dev_impl->button_state, AMOTION_EVENT_BUTTON_PRIMARY))
                gdk_android_events_emit_button_press (AMOTION_EVENT_BUTTON_PRIMARY, button_state,
                                                      GDK_BUTTON_PRIMARY,
                                                      surface, event, dev,
                                                      time, mods,
                                                      x, y);
              if (GDK_ANDROID_EVENTS_BUTTON_IS_DIFFERENT (button_state, dev_impl->button_state, AMOTION_EVENT_BUTTON_SECONDARY))
                gdk_android_events_emit_button_press (AMOTION_EVENT_BUTTON_SECONDARY, button_state,
                                                      GDK_BUTTON_SECONDARY,
                                                      surface, event, dev,
                                                      time, mods,
                                                      x, y);
              if (GDK_ANDROID_EVENTS_BUTTON_IS_DIFFERENT (button_state, dev_impl->button_state, AMOTION_EVENT_BUTTON_TERTIARY))
                gdk_android_events_emit_button_press (AMOTION_EVENT_BUTTON_TERTIARY, button_state,
                                                      GDK_BUTTON_MIDDLE,
                                                      surface, event, dev,
                                                      time, mods,
                                                      x, y);

              const guint update_mask = AMOTION_EVENT_BUTTON_PRIMARY | AMOTION_EVENT_BUTTON_SECONDARY | AMOTION_EVENT_BUTTON_TERTIARY;
              dev_impl->button_state = (dev_impl->button_state & ~update_mask) | (button_state & update_mask);
            }
          else if (tool_type == AMOTION_EVENT_TOOL_TYPE_STYLUS || tool_type == AMOTION_EVENT_TOOL_TYPE_ERASER)
            {
              GdkDeviceTool *tool = gdk_android_seat_get_device_tool (GDK_ANDROID_DISPLAY (display)->seat, tool_type);
              GdkEvent *ev = gdk_button_event_new (masked_action == AMOTION_EVENT_ACTION_DOWN ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE,
                                                   (GdkSurface *) surface, dev, tool,
                                                   time, mods,
                                                   GDK_BUTTON_PRIMARY,
                                                   x, y,
                                                   gdk_android_seat_create_axes_from_motion_event (event, 0));
              gdk_android_seat_consume_event ((GdkDisplay *) display, ev);

              // this will cause conflicts in cases where the mouse/touchpad and
              // a stylus is used at the same time, but I don't know if this is
              // worth handling
              if (masked_action == AMOTION_EVENT_ACTION_DOWN)
                dev_impl->button_state |= AMOTION_EVENT_BUTTON_PRIMARY;
              else
                dev_impl->button_state &= ~AMOTION_EVENT_BUTTON_PRIMARY;
            }
          gdk_android_device_maybe_update_surface ((GdkAndroidDevice *) dev, surface, mods, time, x, y);
        }
      else if (masked_action == AMOTION_EVENT_ACTION_BUTTON_PRESS ||
               masked_action == AMOTION_EVENT_ACTION_BUTTON_RELEASE)
        {
          // This code serves little purpose, as BUTTON_BACK and BUTTON_FORWARD are
          // are seemingly not actually passed to the application. Instead
          // BUTTON_BACK triggers the navigate back action and BUTTON_FORWARD does
          // (at least visibly) nothing.
          /*gint32 button_state = AMotionEvent_getButtonState(event);

          if (GDK_ANDROID_EVENTS_BUTTON_IS_DIFFERENT(button_state, dev_impl->button_state, AMOTION_EVENT_BUTTON_BACK))
                  gdk_android_events_emit_button_press(AMOTION_EVENT_BUTTON_BACK, 4, button_state, surface, event, dev, time, mods | GDK_BUTTON4_MASK, x, y);
          if (GDK_ANDROID_EVENTS_BUTTON_IS_DIFFERENT(button_state, dev_impl->button_state, AMOTION_EVENT_BUTTON_FORWARD))
                  gdk_android_events_emit_button_press(AMOTION_EVENT_BUTTON_FORWARD, 5, button_state, surface, event, dev, time, mods | GDK_BUTTON5_MASK, x, y);

          const guint update_mask = AMOTION_EVENT_BUTTON_BACK | AMOTION_EVENT_BUTTON_FORWARD;
          dev_impl->button_state = (dev_impl->button_state & ~update_mask) | (button_state & update_mask);*/
        }
      else if (masked_action == AMOTION_EVENT_ACTION_MOVE ||
               masked_action == AMOTION_EVENT_ACTION_HOVER_MOVE)
        {
          GdkDeviceTool *tool = gdk_android_seat_get_device_tool (display->seat,
                                                                  AMotionEvent_getToolType (event, 0));
          GdkEvent *ev = gdk_motion_event_new ((GdkSurface *) surface, dev, tool,
                                               time, mods,
                                               x, y,
                                               gdk_android_seat_create_axes_from_motion_event (event, 0));
          gdk_android_seat_consume_event ((GdkDisplay *) display, ev);

          // as changes in BUTTON_STYLUS_{PRIMARY,SECONDARY} do not emit a special
          // event, we'll have to check for changes during move events. This should
          // be fine, given that it's quite hard if not impossible (depending on the
          // tablet) to press a stylus button without causing a move event.
          gint32 button_state = AMotionEvent_getButtonState (event);
          if (GDK_ANDROID_EVENTS_BUTTON_IS_DIFFERENT (button_state, dev_impl->button_state, AMOTION_EVENT_BUTTON_STYLUS_PRIMARY))
            gdk_android_events_emit_button_press (AMOTION_EVENT_BUTTON_STYLUS_PRIMARY, button_state,
                                                  GDK_BUTTON_MIDDLE,
                                                  surface, event, dev,
                                                  time, mods,
                                                  x, y);
          if (GDK_ANDROID_EVENTS_BUTTON_IS_DIFFERENT (button_state, dev_impl->button_state, AMOTION_EVENT_BUTTON_STYLUS_SECONDARY))
            gdk_android_events_emit_button_press (AMOTION_EVENT_BUTTON_STYLUS_SECONDARY, button_state,
                                                  GDK_BUTTON_SECONDARY,
                                                  surface, event, dev,
                                                  time, mods,
                                                  x, y);
          const guint update_mask = AMOTION_EVENT_BUTTON_STYLUS_PRIMARY | AMOTION_EVENT_BUTTON_STYLUS_SECONDARY;
          dev_impl->button_state = (dev_impl->button_state & ~update_mask) | (button_state & update_mask);

          gdk_android_device_maybe_update_surface ((GdkAndroidDevice *) dev, surface, mods, time, x, y);
        }
      else if (masked_action == AMOTION_EVENT_ACTION_HOVER_ENTER ||
               masked_action == AMOTION_EVENT_ACTION_HOVER_EXIT)
        {
          // This would be a good place to put crossing events, however it seems like android also
          // produces hover enter/hover exit events when clicking the button.
          /*GdkEvent *ev = gdk_crossing_event_new ((masked_action == AMOTION_EVENT_ACTION_HOVER_ENTER) ? GDK_ENTER_NOTIFY : GDK_LEAVE_NOTIFY,
                                                                                     (GdkSurface*) surface, dev,
                                                                                     time, mods,
                                                                                     x, y,
                                                                                     GDK_CROSSING_NORMAL,
                                                                                     GDK_NOTIFY_UNKNOWN);
          gdk_android_seat_consume_event((GdkDisplay *)display, ev);*/
        }
      else
        {
          g_warning ("Unhandled pointer event: %d [%d] on %p [%s]", masked_action, src, surface, G_OBJECT_TYPE_NAME (surface));
        }
    }
  else if (GDK_ANDROID_EVENTS_COMPARE_MASK (src, AINPUT_SOURCE_JOYSTICK))
    {
      (*env)->PushLocalFrame (env, 2);
      jobject jdevice = (*env)->CallObjectMethod (env, motion_event, gdk_android_get_java_cache ()->a_input_event.get_device);
      struct
      {
        gint32 axis;
        guint index;
        gfloat min;
        gfloat max;
        GdkEvent *(*constructor) (GdkSurface *surface, GdkDevice *device, guint32 time, guint group, guint index, guint mode, double value);
      } pad_axes[] = {
        { AMOTION_EVENT_AXIS_WHEEL, 0, 0.f, 360.f, gdk_pad_event_new_ring }
      };
      for (gsize i = 0; i < G_N_ELEMENTS (pad_axes); i++)
        {
          gdouble value;
          if (gdk_android_seat_normalize_range (env, jdevice, event, 0, pad_axes[i].axis, pad_axes[i].min, pad_axes[i].max, &value) && value != 0.)
            { // the value != 0. check is less than ideal, as 0 is a legitimate value, but
              // android also returns 0 when the finger leaves the ring (and often just
              // randomly too)
              GdkEvent *ev = pad_axes[i].constructor ((GdkSurface *) surface, gdk_seat_get_keyboard ((GdkSeat *) display->seat),
                                                      time,
                                                      0, pad_axes[i].index, 0,
                                                      value);
              gdk_android_seat_consume_event ((GdkDisplay *) display, ev);
            }
        }
      (*env)->PopLocalFrame (env, NULL);
    }

  AInputEvent_release (event);
}

void
gdk_android_events_handle_key_event (GdkAndroidSurface *surface,
                                     jobject            key_event)
{
  GdkAndroidDisplay *display = (GdkAndroidDisplay *) gdk_surface_get_display ((GdkSurface *) surface);

  JNIEnv *env = gdk_android_get_env ();
  const AInputEvent *event = AKeyEvent_fromJava (env, key_event);

  gint32 action = AKeyEvent_getAction (event);
  // The AKeyState enum values UP and DOWN are actually inversed
  // when the key is depressed the keystate is 0 (UP), while the
  // keystate becomes 1 (DOWN) once the key was released again.
  GdkEventType event_type = (action == AKEY_STATE_UP || action == AKEY_STATE_VIRTUAL) ? GDK_KEY_PRESS : GDK_KEY_RELEASE;

  GdkDevice *dev = gdk_seat_get_keyboard ((GdkSeat *) display->seat);
  gdk_android_device_keyboard_maybe_update_surface_focus ((GdkAndroidDevice *) dev, surface);

  GdkModifierType mods = gdk_android_events_meta_to_gdk (AKeyEvent_getMetaState (event));
  mods |= gdk_android_events_buttons_to_gdkmods (((GdkAndroidDevice *) display->seat->logical_pointer)->button_state);

  gint64 time = AKeyEvent_getEventTime (event);

  gint32 keycode = AKeyEvent_getKeyCode (event);

  if (keycode >= AKEYCODE_BUTTON_1 && keycode <= AKEYCODE_BUTTON_16)
    {
      // Key Event might be a Pad Button
      GdkEvent *ev = gdk_pad_event_new_button (event_type == GDK_KEY_PRESS ? GDK_PAD_BUTTON_PRESS : GDK_PAD_BUTTON_RELEASE,
                                               (GdkSurface *) surface, dev,
                                               time,
                                               0, keycode - AKEYCODE_BUTTON_1, 0);
      gdk_android_seat_consume_event ((GdkDisplay *) display, ev);
      goto cleanup;
    }

  GdkTranslatedKey translated;
  if (!gdk_keymap_translate_keyboard_state (display->keymap, keycode, mods, 0,
                                            &translated.keyval, (gint *) &translated.layout,
                                            (gint *) &translated.level, &translated.consumed))
    goto cleanup;

  // TODO: do no_caps translation properly

  GdkEvent *ev = gdk_key_event_new (event_type,
                                    (GdkSurface *) surface, dev,
                                    time, keycode,
                                    mods & ~translated.consumed, FALSE,
                                    &translated, &translated, NULL);
  gdk_android_seat_consume_event ((GdkDisplay *) display, ev);
cleanup:
  AInputEvent_release (event);
}
