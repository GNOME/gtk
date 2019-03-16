/* gdkkeys-quartz.h
 *
 * Copyright (C) 2005-2007 Imendio AB
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_KEYS_QUARTZ_H__
#define __GDK_KEYS_QUARTZ_H__
#if MAC_OS_X_VERSION_MIN_REQUIRED < 101200
typedef enum
  {
    GDK_QUARTZ_FLAGS_CHANGED = NSFlagsChanged,
    GDK_QUARTZ_KEY_UP = NSKeyUp,
    GDK_QUARTZ_KEY_DOWN = NSKeyDown,
    GDK_QUARTZ_MOUSE_ENTERED = NSMouseEntered,
    GDK_QUARTZ_MOUSE_EXITED = NSMouseExited,
    GDK_QUARTZ_SCROLL_WHEEL = NSScrollWheel,
    GDK_QUARTZ_MOUSE_MOVED = NSMouseMoved,
    GDK_QUARTZ_OTHER_MOUSE_DRAGGED = NSOtherMouseDragged,
    GDK_QUARTZ_RIGHT_MOUSE_DRAGGED = NSRightMouseDragged,
    GDK_QUARTZ_LEFT_MOUSE_DRAGGED = NSLeftMouseDragged,
    GDK_QUARTZ_OTHER_MOUSE_UP = NSOtherMouseUp,
    GDK_QUARTZ_RIGHT_MOUSE_UP = NSRightMouseUp,
    GDK_QUARTZ_LEFT_MOUSE_UP = NSLeftMouseUp,
    GDK_QUARTZ_OTHER_MOUSE_DOWN = NSOtherMouseDown,
    GDK_QUARTZ_RIGHT_MOUSE_DOWN = NSRightMouseDown,
    GDK_QUARTZ_LEFT_MOUSE_DOWN = NSLeftMouseDown,
  } GdkQuartzEventType;

typedef enum
  {
    GDK_QUARTZ_ALTERNATE_KEY_MASK = NSAlternateKeyMask,
    GDK_QUARTZ_CONTROL_KEY_MASK = NSControlKeyMask,
    GDK_QUARTZ_SHIFT_KEY_MASK = NSShiftKeyMask,
    GDK_QUARTZ_ALPHA_SHIFT_KEY_MASK = NSAlphaShiftKeyMask,
    GDK_QUARTZ_COMMAND_KEY_MASK = NSCommandKeyMask,
    GDK_QUARTZ_ANY_EVENT_MASK = NSAnyEventMask,
  } GdkQuartzEventModifierFlags;


#else
typedef enum
  {
    GDK_QUARTZ_FLAGS_CHANGED = NSEventTypeFlagsChanged,
    GDK_QUARTZ_KEY_UP = NSEventTypeKeyUp,
    GDK_QUARTZ_KEY_DOWN = NSEventTypeKeyDown,
    GDK_QUARTZ_MOUSE_ENTERED = NSEventTypeMouseEntered,
    GDK_QUARTZ_MOUSE_EXITED = NSEventTypeMouseExited,
    GDK_QUARTZ_SCROLL_WHEEL = NSEventTypeScrollWheel,
    GDK_QUARTZ_MOUSE_MOVED = NSEventTypeMouseMoved,
    GDK_QUARTZ_OTHER_MOUSE_DRAGGED = NSEventTypeOtherMouseDragged,
    GDK_QUARTZ_RIGHT_MOUSE_DRAGGED = NSEventTypeRightMouseDragged,
    GDK_QUARTZ_LEFT_MOUSE_DRAGGED = NSEventTypeLeftMouseDragged,
    GDK_QUARTZ_OTHER_MOUSE_UP = NSEventTypeOtherMouseUp,
    GDK_QUARTZ_RIGHT_MOUSE_UP = NSEventTypeRightMouseUp,
    GDK_QUARTZ_LEFT_MOUSE_UP = NSEventTypeLeftMouseUp,
    GDK_QUARTZ_OTHER_MOUSE_DOWN = NSEventTypeOtherMouseDown,
    GDK_QUARTZ_RIGHT_MOUSE_DOWN = NSEventTypeRightMouseDown,
    GDK_QUARTZ_LEFT_MOUSE_DOWN = NSEventTypeLeftMouseDown,
  } GdkQuartzEventType;

typedef enum
  {
   GDK_QUARTZ_ALTERNATE_KEY_MASK = NSEventModifierFlagOption,
   GDK_QUARTZ_CONTROL_KEY_MASK = NSEventModifierFlagControl,
   GDK_QUARTZ_SHIFT_KEY_MASK = NSEventModifierFlagShift,
   GDK_QUARTZ_ALPHA_SHIFT_KEY_MASK = NSEventModifierFlagCapsLock,
   GDK_QUARTZ_COMMAND_KEY_MASK = NSEventModifierFlagCommand,
  } GdkQuartzEventModifierFlags;


#endif
#endif /* __GDK_KEYS_QUARTZ_H__ */
