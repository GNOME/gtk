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
  GdkEvent *ret = NULL;
  NSEventType event_type;
  NSWindow *nswindow;
  GdkMacosSurface *surface;
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

  nswindow = [nsevent window];

  /* Ignore events for windows not created by GDK. */
  if (nswindow && ![[nswindow contentView] isKindOfClass:[GdkMacosBaseView class]])
    return NULL;

  /* Ignore events for ones with no windows */
  if (!nswindow)
    return NULL;

  /* Ignore events and break grabs while the window is being
   * dragged. This is a workaround for the window getting events for
   * the window title.
   */
  if ([(GdkMacosWindow *)nswindow isInMove])
    {
      _gdk_macos_display_break_all_grabs (self, get_time_from_ns_event (nsevent));
      return NULL;
    }

  /* Also when in a manual resize or move , we ignore events so that
   * these are pushed to GdkMacosNSWindow's sendEvent handler.
   */
  if ([(GdkMacosWindow *)nswindow isInManualResizeOrMove])
    return NULL;

  /* Get the location of the event within the toplevel */
  point = [nsevent locationInWindow];
  _gdk_macos_display_from_display_coords (self, point.x, point.y, &x, &y);

  /* Find the right GDK surface to send the event to, taking grabs and
   * event masks into consideration.
   */
  if (!(surface = [(GdkMacosWindow *)nswindow getGdkSurface]))
    return NULL;

  /* Quartz handles resizing on its own, so we want to stay out of the way. */
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

  switch (event_type)
    {
    case NSLeftMouseDown:
    case NSRightMouseDown:
    case NSOtherMouseDown:
    case NSLeftMouseUp:
    case NSRightMouseUp:
    case NSOtherMouseUp:
      ret = fill_button_event (self, surface, nsevent, x, y);
      break;

    case NSLeftMouseDragged:
    case NSRightMouseDragged:
    case NSOtherMouseDragged:
    case NSMouseMoved:
      ret = fill_motion_event (self, surface, nsevent, x, y);
      break;

    case NSMouseExited:
      [[NSCursor arrowCursor] set];
      /* fall through */
    case NSMouseEntered:
      ret = synthesize_crossing_event (self, surface, nsevent, x, y);
      break;

    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged: {
      GdkEventType type = _gdk_macos_keymap_get_event_type (nsevent);

      if (type)
        ret = fill_key_event (self, surface, nsevent, type);

      break;
    }

    default:
      break;
    }

#if 0
  switch (event_type)
    {

    case NSScrollWheel:
      {
        GdkScrollDirection direction;
        float dx;
        float dy;
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER
        if (gdk_quartz_osx_version() >= GDK_OSX_LION &&
            [nsevent hasPreciseScrollingDeltas])
          {
            dx = [nsevent scrollingDeltaX];
            dy = [nsevent scrollingDeltaY];
            direction = GDK_SCROLL_SMOOTH;

            fill_scroll_event (window, event, nsevent, x, y, x_root, y_root,
                               -dx, -dy, direction);

            /* Fall through for scroll buttons emulation */
          }
#endif
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
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_7_AND_LATER
            if (gdk_quartz_osx_version() >= GDK_OSX_LION &&
                [nsevent hasPreciseScrollingDeltas])
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
#endif
              fill_scroll_event (window, event, nsevent,
                                 x, y, x_root, y_root,
                                 dx, dy, direction);
          }
      }
      break;
#ifdef AVAILABLE_MAC_OS_X_VERSION_10_8_AND_LATER
    case NSEventTypeMagnify:
    case NSEventTypeRotate:
      /* Event handling requires [NSEvent phase] which was introduced in 10.7 */
      /* However - Tests on 10.7 showed that phase property does not work     */
      if (gdk_quartz_osx_version () >= GDK_OSX_MOUNTAIN_LION)
        fill_pinch_event (window, event, nsevent, x, y, x_root, y_root);
      else
        return_val = FALSE;
      break;
#endif
    case NSMouseExited:
      if (SURFACE_IS_TOPLEVEL (window))
          [[NSCursor arrowCursor] set];
      /* fall through */
    case NSMouseEntered:
      return_val = synthesize_crossing_event (window, event, nsevent, x, y, x_root, y_root);
      break;

    case NSKeyDown:
    case NSKeyUp:
    case NSFlagsChanged:
      {
        GdkEventType type;

        type = _gdk_quartz_keys_event_type (nsevent);
        if (type == GDK_NOTHING)
          return_val = FALSE;
        else
          fill_key_event (window, event, nsevent, type);
      }
      break;

    default:
      /* Ignore everything elsee. */
      return_val = FALSE;
      break;
    }

 done:
  if (return_val)
    {
      if (event->any.surface)
        g_object_ref (event->any.surface);
      if (((event->any.type == GDK_ENTER_NOTIFY) ||
           (event->any.type == GDK_LEAVE_NOTIFY)) &&
          (event->crossing.child_window != NULL))
        g_object_ref (event->crossing.child_window);
    }
  else
    {
      /* Mark this event as having no resources to be freed */
      event->any.surface = NULL;
      event->any.type = GDK_NOTHING;
    }
#endif

  return ret;
}

