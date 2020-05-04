/*
 * Copyright © 2005 Imendio AB
 * Copyright © 2020 Red Hat, Inc.
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

#include "config.h"

#import "GdkMacosWindow.h"
#import "GdkMacosBaseView.h"

#include "gdkmacosdisplay-private.h"
#include "gdkmacoskeymap-private.h"
#include "gdkmacossurface-private.h"

#define GRIP_WIDTH 15
#define GRIP_HEIGHT 15
#define GDK_LION_RESIZE 5

static gboolean
test_resize (NSEvent         *event,
             GdkMacosSurface *surface,
             gint             x,
             gint             y)
{
  NSWindow *window;

  g_assert (event != NULL);
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  window = _gdk_macos_surface_get_native (surface);

  /* Resizing from the resize indicator only begins if an NSLeftMouseButton
   * event is received in the resizing area.
   */
  if ([event type] == NSLeftMouseDown &&
      [window showsResizeIndicator])
    {
      NSRect frame;

      /* If the resize indicator is visible and the event is in the lower
       * right 15x15 corner, we leave these events to Cocoa as to be
       * handled as resize events.  Applications may have widgets in this
       * area.  These will most likely be larger than 15x15 and for scroll
       * bars there are also other means to move the scroll bar.  Since
       * the resize indicator is the only way of resizing windows on Mac
       * OS, it is too important to not make functional.
       */
      frame = [[window contentView] bounds];
      if (x > frame.size.width - GRIP_WIDTH &&
          x < frame.size.width &&
          y > frame.size.height - GRIP_HEIGHT &&
          y < frame.size.height)
        return TRUE;
     }

  /* If we're on Lion and within 5 pixels of an edge, then assume that the
   * user wants to resize, and return NULL to let Quartz get on with it.
   * We check the selector isRestorable to see if we're on 10.7.  This
   * extra check is in case the user starts dragging before GDK recognizes
   * the grab.
   *
   * We perform this check for a button press of all buttons, because we
   * do receive, for instance, a right mouse down event for a GDK surface
   * for x-coordinate range [-3, 0], but we do not want to forward this
   * into GDK. Forwarding such events into GDK will confuse the pointer
   * window finding code, because there are no GdkSurfaces present in
   * the range [-3, 0].
   */
  if (([event type] == NSLeftMouseDown ||
       [event type] == NSRightMouseDown ||
       [event type] == NSOtherMouseDown))
    {
      if (x < GDK_LION_RESIZE ||
          x > GDK_SURFACE (surface)->width - GDK_LION_RESIZE ||
          y > GDK_SURFACE (surface)->height - GDK_LION_RESIZE)
        return TRUE;
    }

  return FALSE;
}

static guint32
get_time_from_ns_event (NSEvent *event)
{
  double time = [event timestamp];
  /* cast via double->uint64 conversion to make sure that it is
   * wrapped on 32-bit machines when it overflows
   */
  return (guint32) (guint64) (time * 1000.0);
}

static int
get_mouse_button_from_ns_event (NSEvent *event)
{
  NSInteger button = [event buttonNumber];

  switch (button)
    {
    case 0:
      return 1;
    case 1:
      return 3;
    case 2:
      return 2;
    default:
      return button + 1;
    }
}

static GdkModifierType
get_mouse_button_modifiers_from_ns_buttons (NSUInteger nsbuttons)
{
  GdkModifierType modifiers = 0;

  if (nsbuttons & (1 << 0))
    modifiers |= GDK_BUTTON1_MASK;
  if (nsbuttons & (1 << 1))
    modifiers |= GDK_BUTTON3_MASK;
  if (nsbuttons & (1 << 2))
    modifiers |= GDK_BUTTON2_MASK;
  if (nsbuttons & (1 << 3))
    modifiers |= GDK_BUTTON4_MASK;
  if (nsbuttons & (1 << 4))
    modifiers |= GDK_BUTTON5_MASK;

  return modifiers;
}

static GdkModifierType
get_mouse_button_modifiers_from_ns_event (NSEvent *event)
{
  GdkModifierType state = 0;
  int button;

  /* This maps buttons 1 to 5 to GDK_BUTTON[1-5]_MASK */
  button = get_mouse_button_from_ns_event (event);
  if (button >= 1 && button <= 5)
    state = (1 << (button + 7));

  return state;
}

static GdkModifierType
get_keyboard_modifiers_from_ns_flags (NSUInteger nsflags)
{
  GdkModifierType modifiers = 0;

  if (nsflags & NSAlphaShiftKeyMask)
    modifiers |= GDK_LOCK_MASK;
  if (nsflags & NSShiftKeyMask)
    modifiers |= GDK_SHIFT_MASK;
  if (nsflags & NSControlKeyMask)
    modifiers |= GDK_CONTROL_MASK;
  if (nsflags & NSAlternateKeyMask)
    modifiers |= GDK_ALT_MASK;
  if (nsflags & NSCommandKeyMask)
    modifiers |= GDK_SUPER_MASK;

  return modifiers;
}

static GdkModifierType
get_keyboard_modifiers_from_ns_event (NSEvent *nsevent)
{
  return get_keyboard_modifiers_from_ns_flags ([nsevent modifierFlags]);
}

GdkModifierType
_gdk_macos_display_get_current_mouse_modifiers (GdkMacosDisplay *self)
{
  return get_mouse_button_modifiers_from_ns_buttons ([NSEvent pressedMouseButtons]);
}

GdkModifierType
_gdk_macos_display_get_current_keyboard_modifiers (GdkMacosDisplay *self)
{
  return get_keyboard_modifiers_from_ns_flags ([NSEvent modifierFlags]);
}

static GdkEvent *
fill_button_event (GdkMacosDisplay *display,
                   GdkMacosSurface *surface,
                   NSEvent         *nsevent,
                   gint             x,
                   gint             y)
{
  GdkSeat *seat;
  GdkEventType type;
  GdkModifierType state;

  g_assert (GDK_IS_MACOS_DISPLAY (display));
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  seat = gdk_display_get_default_seat (GDK_DISPLAY (display));
  state = get_keyboard_modifiers_from_ns_event (nsevent) |
         _gdk_macos_display_get_current_mouse_modifiers (display);

  switch ([nsevent type])
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
      type = GDK_BUTTON_PRESS;
      state &= ~get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      type = GDK_BUTTON_RELEASE;
      state |= get_mouse_button_modifiers_from_ns_event (nsevent);
      break;

    default:
      g_assert_not_reached ();
    }

  return gdk_button_event_new (type,
                               GDK_SURFACE (surface),
                               gdk_seat_get_pointer (seat),
                               NULL,
                               NULL,
                               get_time_from_ns_event (nsevent),
                               state,
                               get_mouse_button_from_ns_event (nsevent),
                               x,
                               y,
                               NULL);
}

static GdkEvent *
synthesize_crossing_event (GdkMacosDisplay *display,
                           GdkMacosSurface *surface,
                           NSEvent         *nsevent,
                           gint             x,
                           gint             y)
{
  GdkEventType event_type;
  GdkModifierType state;
  GdkSeat *seat;

  switch ([nsevent type])
    {
    case NSMouseEntered:
      event_type = GDK_ENTER_NOTIFY;
      break;

    case NSMouseExited:
      event_type = GDK_LEAVE_NOTIFY;
      break;

    default:
      return NULL;
    }

  state = get_keyboard_modifiers_from_ns_event (nsevent) |
         _gdk_macos_display_get_current_mouse_modifiers (display);
  seat = gdk_display_get_default_seat (GDK_DISPLAY (display));

  return gdk_crossing_event_new (event_type,
                                 surface,
                                 gdk_seat_get_pointer (seat),
                                 NULL,
                                 get_time_from_ns_event (nsevent),
                                 state,
                                 x,
                                 y,
                                 GDK_CROSSING_NORMAL,
                                 GDK_NOTIFY_NONLINEAR);
}

static GdkEvent *
fill_key_event (GdkMacosDisplay *display,
                GdkMacosSurface *surface,
                NSEvent         *nsevent,
                GdkEventType     type)
{
#if 1
  return NULL;
#else
  GdkTranslatedKey translated;
  GdkTranslatedKey no_lock;
  GdkModifierType modifiers;
  gboolean is_modifier;
  GdkSeat *seat;
  guint keycode;

  g_assert (GDK_IS_MACOS_DISPLAY (display));
  g_assert (GDK_IS_MACOS_SURFACE (surface));
  g_assert (nsevent != NULL);

  seat = gdk_display_get_default_seat (GDK_DISPLAY (display));

  return gdk_key_event_new (type,
                            GDK_SURFACE (surface),
                            gdk_seat_get_keyboard (seat),
                            NULL,
                            get_time_from_ns_event (nsevent),
                            [nsevent keyCode],
                            modifiers,
                            is_modifier,
                            &translated,
                            &no_lock);
#endif
}

static GdkEvent *
fill_pinch_event (GdkMacosDisplay *display,
                  GdkMacosSurface *surface,
                  NSEvent         *nsevent,
                  int              x,
                  int              y)
{
  static double last_scale = 1.0;
  static enum {
    FP_STATE_IDLE,
    FP_STATE_UPDATE
  } last_state = FP_STATE_IDLE;
  GdkSeat *seat;
  GdkTouchpadGesturePhase phase;
  gdouble angle_delta = 0.0;

  g_assert (GDK_IS_MACOS_DISPLAY (display));
  g_assert (GDK_IS_MACOS_SURFACE (surface));

  /* fill_pinch_event handles the conversion from the two OSX gesture events
   * NSEventTypeMagnfiy and NSEventTypeRotate to the GDK_TOUCHPAD_PINCH event.
   * The normal behavior of the OSX events is that they produce as sequence of
   *   1 x NSEventPhaseBegan,
   *   n x NSEventPhaseChanged,
   *   1 x NSEventPhaseEnded
   * This can happen for both the Magnify and the Rotate events independently.
   * As both events are summarized in one GDK_TOUCHPAD_PINCH event sequence, a
   * little state machine handles the case of two NSEventPhaseBegan events in
   * a sequence, e.g. Magnify(Began), Magnify(Changed)..., Rotate(Began)...
   * such that PINCH(STARTED), PINCH(UPDATE).... will not show a second
   * PINCH(STARTED) event.
   */

  switch ([nsevent phase])
    {
    case NSEventPhaseBegan:
      switch (last_state)
        {
        case FP_STATE_IDLE:
          phase = GDK_TOUCHPAD_GESTURE_PHASE_BEGIN;
          last_state = FP_STATE_UPDATE;
          last_scale = 1.0;
          break;
        case FP_STATE_UPDATE:
          /* We have already received a PhaseBegan event but no PhaseEnded
             event. This can happen, e.g. Magnify(Began), Magnify(Change)...
             Rotate(Began), Rotate (Change),...., Magnify(End) Rotate(End)
          */
          phase = GDK_TOUCHPAD_GESTURE_PHASE_UPDATE;
          break;
        }
      break;

    case NSEventPhaseChanged:
      phase = GDK_TOUCHPAD_GESTURE_PHASE_UPDATE;
      break;

    case NSEventPhaseEnded:
      phase = GDK_TOUCHPAD_GESTURE_PHASE_END;
      switch (last_state)
        {
        case FP_STATE_IDLE:
          /* We are idle but have received a second PhaseEnded event.
             This can happen because we have Magnify and Rotate OSX
             event sequences. We just send a second end GDK_PHASE_END.
          */
          break;
        case FP_STATE_UPDATE:
          last_state = FP_STATE_IDLE;
          break;
        }
      break;

    case NSEventPhaseCancelled:
      phase = GDK_TOUCHPAD_GESTURE_PHASE_CANCEL;
      last_state = FP_STATE_IDLE;
      break;

    case NSEventPhaseMayBegin:
    case NSEventPhaseStationary:
      phase = GDK_TOUCHPAD_GESTURE_PHASE_CANCEL;
      break;

    default:
      g_assert_not_reached ();
      break;
    }

  switch ([nsevent type])
    {
    case NSEventTypeMagnify:
      last_scale *= [nsevent magnification] + 1.0;
      angle_delta = 0.0;
      break;

    case NSEventTypeRotate:
      angle_delta = - [nsevent rotation] * G_PI / 180.0;
      break;

    default:
      g_assert_not_reached ();
    }

  seat = gdk_display_get_default_seat (GDK_DISPLAY (display));

  return gdk_touchpad_event_new_pinch (GDK_SURFACE (surface),
                                       gdk_seat_get_pointer (seat),
                                       NULL,
                                       get_time_from_ns_event (nsevent),
                                       get_keyboard_modifiers_from_ns_event (nsevent),
                                       phase,
                                       x,
                                       y,
                                       2,
                                       0.0,
                                       0.0,
                                       last_scale,
                                       angle_delta);

}

static GdkEvent *
fill_motion_event (GdkMacosDisplay *display,
                   GdkMacosSurface *surface,
                   NSEvent         *nsevent,
                   int              x,
                   int              y)
{
  GdkSeat *seat;
  GdkModifierType state;

  g_assert (GDK_IS_MACOS_SURFACE (surface));
  g_assert (nsevent != NULL);

  seat = gdk_display_get_default_seat (GDK_DISPLAY (display));
  state = get_keyboard_modifiers_from_ns_event (nsevent) |
          _gdk_macos_display_get_current_mouse_modifiers (display);

  return gdk_motion_event_new (GDK_SURFACE (surface),
                               gdk_seat_get_pointer (seat),
                               NULL,
                               NULL,
                               get_time_from_ns_event (nsevent),
                               state,
                               x,
                               y,
                               NULL);
}


GdkEvent *
_gdk_macos_display_translate (GdkMacosDisplay *self,
                              NSEvent         *nsevent)
{
  GdkMacosSurface *surface;
  GdkMacosWindow *window;
  NSEventType event_type;
  NSWindow *nswindow;
  GdkEvent *ret = NULL;
  NSPoint point;
  int x, y;

  g_return_val_if_fail (GDK_IS_MACOS_DISPLAY (self), NULL);
  g_return_val_if_fail (nsevent != NULL, NULL);

  /* There is no support for real desktop wide grabs, so we break
   * grabs when the application loses focus (gets deactivated).
   */
  event_type = [nsevent type];
  if (event_type == NSAppKitDefined)
    {
      if ([nsevent subtype] == NSApplicationDeactivatedEventType)
        _gdk_macos_display_break_all_grabs (self, get_time_from_ns_event (nsevent));

      /* This could potentially be used to break grabs when clicking
       * on the title. The subtype 20 is undocumented so it's probably
       * not a good idea: else if (subtype == 20) break_all_grabs ();
       */

      /* Leave all AppKit events to AppKit. */
      return NULL;
    }

  /* Make sure the event has a window */
  if (!(nswindow = [nsevent window]))
    return NULL;

  /* Ignore unless it is for a GdkMacosWindow */
  if (!GDK_IS_MACOS_WINDOW (nswindow))
    return NULL;

  window = (GdkMacosWindow *)nswindow;

  /* Ignore events and break grabs while the window is being
   * dragged. This is a workaround for the window getting events for
   * the window title.
   */
  if ([window isInMove])
    {
      _gdk_macos_display_break_all_grabs (self, get_time_from_ns_event (nsevent));
      return NULL;
    }

  /* Also when in a manual resize or move , we ignore events so that
   * these are pushed to GdkMacosNSWindow's sendEvent handler.
   */
  if ([window isInManualResizeOrMove])
    return NULL;

  /* Make sure we have a GdkSurface */
  if (!(surface = [window getGdkSurface]))
    return NULL;

  /* Get the location of the event within the toplevel */
  point = [nsevent locationInWindow];
  _gdk_macos_display_from_display_coords (self, point.x, point.y, &x, &y);

  /* Quartz handles resizing on its own, so stay out of the way. */
  if (test_resize (nsevent, surface, x, y))
    return NULL;

  /* If the app is not active leave the event to AppKit so the window gets
   * focused correctly and don't do click-through (so we behave like most
   * native apps). If the app is active, we focus the window and then handle
   * the event, also to match native apps.
   */
  if ((event_type == NSRightMouseDown ||
       event_type == NSOtherMouseDown ||
       event_type == NSLeftMouseDown))
    {
      if (![NSApp isActive])
        {
          [NSApp activateIgnoringOtherApps:YES];
          return FALSE;
        }
      else if (![nswindow isKeyWindow])
        {
          [nswindow makeKeyWindow];
        }
    }

#if 0
  g_print ("Type: %d Surface: %s %d,%d %dx%d\n",
           event_type,
           G_OBJECT_TYPE_NAME (surface),
           GDK_SURFACE (surface)->x,
           GDK_SURFACE (surface)->y,
           GDK_SURFACE (surface)->width,
           GDK_SURFACE (surface)->height);
#endif

  switch (event_type)
    {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:
    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:
      ret = fill_button_event (self, surface, nsevent, x, y);
      break;

    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeMouseMoved:
      ret = fill_motion_event (self, surface, nsevent, x, y);
      break;

    case NSEventTypeMagnify:
    case NSEventTypeRotate:
      ret = fill_pinch_event (self, surface, nsevent, x, y);
      break;

    case NSEventTypeMouseExited:
      [[NSCursor arrowCursor] set];
      /* fall through */
    case NSEventTypeMouseEntered:
      ret = synthesize_crossing_event (self, surface, nsevent, x, y);
      break;

    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp:
    case NSEventTypeFlagsChanged: {
      GdkEventType type = _gdk_macos_keymap_get_event_type (nsevent);

      if (type)
        ret = fill_key_event (self, surface, nsevent, type);

      break;
    }

    case NSEventTypeScrollWheel:
      //ret = fill_scroll_event (self, surface, nsevent, x, y);
      break;

    default:
      break;
    }

#if 0
    case NSScrollWheel:
      {
        GdkScrollDirection direction;
        float dx;
        float dy;

        if ([nsevent hasPreciseScrollingDeltas])
          {
            dx = [nsevent scrollingDeltaX];
            dy = [nsevent scrollingDeltaY];
            direction = GDK_SCROLL_SMOOTH;

            fill_scroll_event (window, event, nsevent, x, y, x_root, y_root,
                               -dx, -dy, direction);

            /* Fall through for scroll buttons emulation */
          }

        dx = [nsevent deltaX];
        dy = [nsevent deltaY];

        if (dy != 0.0)
          {
            if (dy < 0.0)
              direction = GDK_SCROLL_DOWN;
            else
              direction = GDK_SCROLL_UP;

            dy = fabs (dy);
            dx = 0.0;
          }
        else if (dx != 0.0)
          {
            if (dx < 0.0)
              direction = GDK_SCROLL_RIGHT;
            else
              direction = GDK_SCROLL_LEFT;

            dx = fabs (dx);
            dy = 0.0;
          }

        if (dx != 0.0 || dy != 0.0)
          {
            if ([nsevent hasPreciseScrollingDeltas])
              {
                GdkEvent *emulated_event;

                emulated_event = gdk_event_new (GDK_SCROLL);
                gdk_event_set_pointer_emulated (emulated_event, TRUE);
                fill_scroll_event (window, emulated_event, nsevent,
                                   x, y, x_root, y_root,
                                   dx, dy, direction);
                append_event (emulated_event, TRUE);
              }
            else
              {
                fill_scroll_event (window, event, nsevent,
                                   x, y, x_root, y_root,
                                   dx, dy, direction);
              }
          }
      }
      break;
#endif

  return ret;
}

