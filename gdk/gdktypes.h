/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GDK_TYPES_H__
#define __GDK_TYPES_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

/* GDK uses "glib". (And so does GTK).
 */
#include <glib.h>
#include <pango/pango.h>
#include <glib-object.h>
#include <cairo.h>

/* The system specific file gdkconfig.h contains such configuration
 * settings that are needed not only when compiling GDK (or GTK)
 * itself, but also occasionally when compiling programs that use GDK
 * (or GTK). One such setting is what windowing API backend is in use.
 */
#include <gdk/gdkconfig.h>

/* some common magic values */

/**
 * GDK_CURRENT_TIME:
 *
 * Represents the current time, and can be used anywhere a time is expected.
 */
#define GDK_CURRENT_TIME     0L

/**
 * GDK_PARENT_RELATIVE:
 *
 * A special value, indicating that the background
 * for a window should be inherited from the parent window.
 */
#define GDK_PARENT_RELATIVE  1L



G_BEGIN_DECLS


/* Type definitions for the basic structures.
 */
typedef struct _GdkPoint              GdkPoint;

/**
 * GdkRectangle:
 *
 * Defines the position and size of a rectangle. It is identical to
 * #cairo_rectangle_int_t.
 */
typedef cairo_rectangle_int_t         GdkRectangle;

/**
 * GdkAtom:
 *
 * An opaque type representing a string as an index into a table
 * of strings on the X server.
 */
typedef struct _GdkAtom            *GdkAtom;

/**
 * GDK_ATOM_TO_POINTER:
 * @atom: a #GdkAtom.
 *
 * Converts a #GdkAtom into a pointer type.
 */
#define GDK_ATOM_TO_POINTER(atom) (atom)

/**
 * GDK_POINTER_TO_ATOM:
 * @ptr: a pointer containing a #GdkAtom.
 *
 * Extracts a #GdkAtom from a pointer. The #GdkAtom must have been
 * stored in the pointer with GDK_ATOM_TO_POINTER().
 */
#define GDK_POINTER_TO_ATOM(ptr)  ((GdkAtom)(ptr))

#define _GDK_MAKE_ATOM(val) ((GdkAtom)GUINT_TO_POINTER(val))

/**
 * GDK_NONE:
 *
 * A null value for #GdkAtom, used in a similar way as
 * `None` in the Xlib API.
 */
#define GDK_NONE            _GDK_MAKE_ATOM (0)

/* Forward declarations of commonly used types */
typedef struct _GdkColor              GdkColor;
typedef struct _GdkRGBA               GdkRGBA;
typedef struct _GdkCursor             GdkCursor;
typedef struct _GdkVisual             GdkVisual;
typedef struct _GdkDevice             GdkDevice;
typedef struct _GdkDragContext        GdkDragContext;

typedef struct _GdkDisplayManager     GdkDisplayManager;
typedef struct _GdkDeviceManager      GdkDeviceManager;
typedef struct _GdkDisplay            GdkDisplay;
typedef struct _GdkScreen             GdkScreen;
typedef struct _GdkWindow             GdkWindow;
typedef struct _GdkKeymap             GdkKeymap;
typedef struct _GdkAppLaunchContext   GdkAppLaunchContext;

typedef struct _GdkGLContext          GdkGLContext;

/**
 * GdkByteOrder:
 * @GDK_LSB_FIRST: The values are stored with the least-significant byte
 *   first. For instance, the 32-bit value 0xffeecc would be stored
 *   in memory as 0xcc, 0xee, 0xff, 0x00.
 * @GDK_MSB_FIRST: The values are stored with the most-significant byte
 *   first. For instance, the 32-bit value 0xffeecc would be stored
 *   in memory as 0x00, 0xff, 0xee, 0xcc.
 *
 * A set of values describing the possible byte-orders
 * for storing pixel values in memory.
 */
typedef enum
{
  GDK_LSB_FIRST,
  GDK_MSB_FIRST
} GdkByteOrder;

/* Types of modifiers.
 */
/**
 * GdkModifierType:
 * @GDK_SHIFT_MASK: the Shift key.
 * @GDK_LOCK_MASK: a Lock key (depending on the modifier mapping of the
 *  X server this may either be CapsLock or ShiftLock).
 * @GDK_CONTROL_MASK: the Control key.
 * @GDK_MOD1_MASK: the fourth modifier key (it depends on the modifier
 *  mapping of the X server which key is interpreted as this modifier, but
 *  normally it is the Alt key).
 * @GDK_MOD2_MASK: the fifth modifier key (it depends on the modifier
 *  mapping of the X server which key is interpreted as this modifier).
 * @GDK_MOD3_MASK: the sixth modifier key (it depends on the modifier
 *  mapping of the X server which key is interpreted as this modifier).
 * @GDK_MOD4_MASK: the seventh modifier key (it depends on the modifier
 *  mapping of the X server which key is interpreted as this modifier).
 * @GDK_MOD5_MASK: the eighth modifier key (it depends on the modifier
 *  mapping of the X server which key is interpreted as this modifier).
 * @GDK_BUTTON1_MASK: the first mouse button.
 * @GDK_BUTTON2_MASK: the second mouse button.
 * @GDK_BUTTON3_MASK: the third mouse button.
 * @GDK_BUTTON4_MASK: the fourth mouse button.
 * @GDK_BUTTON5_MASK: the fifth mouse button.
 * @GDK_MODIFIER_RESERVED_13_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_14_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_15_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_16_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_17_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_18_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_19_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_20_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_21_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_22_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_23_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_24_MASK: A reserved bit flag; do not use in your own code
 * @GDK_MODIFIER_RESERVED_25_MASK: A reserved bit flag; do not use in your own code
 * @GDK_SUPER_MASK: the Super modifier. Since 2.10
 * @GDK_HYPER_MASK: the Hyper modifier. Since 2.10
 * @GDK_META_MASK: the Meta modifier. Since 2.10
 * @GDK_MODIFIER_RESERVED_29_MASK: A reserved bit flag; do not use in your own code
 * @GDK_RELEASE_MASK: not used in GDK itself. GTK+ uses it to differentiate
 *  between (keyval, modifiers) pairs from key press and release events.
 * @GDK_MODIFIER_MASK: a mask covering all modifier types.
 *
 * A set of bit-flags to indicate the state of modifier keys and mouse buttons
 * in various event types. Typical modifier keys are Shift, Control, Meta,
 * Super, Hyper, Alt, Compose, Apple, CapsLock or ShiftLock.
 *
 * Like the X Window System, GDK supports 8 modifier keys and 5 mouse buttons.
 *
 * Since 2.10, GDK recognizes which of the Meta, Super or Hyper keys are mapped
 * to Mod2 - Mod5, and indicates this by setting %GDK_SUPER_MASK,
 * %GDK_HYPER_MASK or %GDK_META_MASK in the state field of key events.
 *
 * Note that GDK may add internal values to events which include
 * reserved values such as %GDK_MODIFIER_RESERVED_13_MASK.  Your code
 * should preserve and ignore them.  You can use %GDK_MODIFIER_MASK to
 * remove all reserved values.
 */
typedef enum
{
  GDK_SHIFT_MASK    = 1 << 0,
  GDK_LOCK_MASK     = 1 << 1,
  GDK_CONTROL_MASK  = 1 << 2,
  GDK_MOD1_MASK     = 1 << 3,
  GDK_MOD2_MASK     = 1 << 4,
  GDK_MOD3_MASK     = 1 << 5,
  GDK_MOD4_MASK     = 1 << 6,
  GDK_MOD5_MASK     = 1 << 7,
  GDK_BUTTON1_MASK  = 1 << 8,
  GDK_BUTTON2_MASK  = 1 << 9,
  GDK_BUTTON3_MASK  = 1 << 10,
  GDK_BUTTON4_MASK  = 1 << 11,
  GDK_BUTTON5_MASK  = 1 << 12,

  GDK_MODIFIER_RESERVED_13_MASK  = 1 << 13,
  GDK_MODIFIER_RESERVED_14_MASK  = 1 << 14,
  GDK_MODIFIER_RESERVED_15_MASK  = 1 << 15,
  GDK_MODIFIER_RESERVED_16_MASK  = 1 << 16,
  GDK_MODIFIER_RESERVED_17_MASK  = 1 << 17,
  GDK_MODIFIER_RESERVED_18_MASK  = 1 << 18,
  GDK_MODIFIER_RESERVED_19_MASK  = 1 << 19,
  GDK_MODIFIER_RESERVED_20_MASK  = 1 << 20,
  GDK_MODIFIER_RESERVED_21_MASK  = 1 << 21,
  GDK_MODIFIER_RESERVED_22_MASK  = 1 << 22,
  GDK_MODIFIER_RESERVED_23_MASK  = 1 << 23,
  GDK_MODIFIER_RESERVED_24_MASK  = 1 << 24,
  GDK_MODIFIER_RESERVED_25_MASK  = 1 << 25,

  /* The next few modifiers are used by XKB, so we skip to the end.
   * Bits 15 - 25 are currently unused. Bit 29 is used internally.
   */
  
  GDK_SUPER_MASK    = 1 << 26,
  GDK_HYPER_MASK    = 1 << 27,
  GDK_META_MASK     = 1 << 28,
  
  GDK_MODIFIER_RESERVED_29_MASK  = 1 << 29,

  GDK_RELEASE_MASK  = 1 << 30,

  /* Combination of GDK_SHIFT_MASK..GDK_BUTTON5_MASK + GDK_SUPER_MASK
     + GDK_HYPER_MASK + GDK_META_MASK + GDK_RELEASE_MASK */
  GDK_MODIFIER_MASK = 0x5c001fff
} GdkModifierType;

/**
 * GdkModifierIntent:
 * @GDK_MODIFIER_INTENT_PRIMARY_ACCELERATOR: the primary modifier used to invoke
 *  menu accelerators.
 * @GDK_MODIFIER_INTENT_CONTEXT_MENU: the modifier used to invoke context menus.
 *  Note that mouse button 3 always triggers context menus. When this modifier
 *  is not 0, it additionally triggers context menus when used with mouse button 1.
 * @GDK_MODIFIER_INTENT_EXTEND_SELECTION: the modifier used to extend selections
 *  using `modifier`-click or `modifier`-cursor-key
 * @GDK_MODIFIER_INTENT_MODIFY_SELECTION: the modifier used to modify selections,
 *  which in most cases means toggling the clicked item into or out of the selection.
 * @GDK_MODIFIER_INTENT_NO_TEXT_INPUT: when any of these modifiers is pressed, the
 *  key event cannot produce a symbol directly. This is meant to be used for
 *  input methods, and for use cases like typeahead search.
 * @GDK_MODIFIER_INTENT_SHIFT_GROUP: the modifier that switches between keyboard
 *  groups (AltGr on X11/Windows and Option/Alt on OS X).
 *
 * This enum is used with gdk_keymap_get_modifier_mask()
 * in order to determine what modifiers the
 * currently used windowing system backend uses for particular
 * purposes. For example, on X11/Windows, the Control key is used for
 * invoking menu shortcuts (accelerators), whereas on Apple computers
 * it’s the Command key (which correspond to %GDK_CONTROL_MASK and
 * %GDK_MOD2_MASK, respectively).
 *
 * Since: 3.4
 **/
typedef enum
{
  GDK_MODIFIER_INTENT_PRIMARY_ACCELERATOR,
  GDK_MODIFIER_INTENT_CONTEXT_MENU,
  GDK_MODIFIER_INTENT_EXTEND_SELECTION,
  GDK_MODIFIER_INTENT_MODIFY_SELECTION,
  GDK_MODIFIER_INTENT_NO_TEXT_INPUT,
  GDK_MODIFIER_INTENT_SHIFT_GROUP
} GdkModifierIntent;

typedef enum
{
  GDK_OK          = 0,
  GDK_ERROR       = -1,
  GDK_ERROR_PARAM = -2,
  GDK_ERROR_FILE  = -3,
  GDK_ERROR_MEM   = -4
} GdkStatus;

/**
 * GdkGrabStatus:
 * @GDK_GRAB_SUCCESS: the resource was successfully grabbed.
 * @GDK_GRAB_ALREADY_GRABBED: the resource is actively grabbed by another client.
 * @GDK_GRAB_INVALID_TIME: the resource was grabbed more recently than the
 *  specified time.
 * @GDK_GRAB_NOT_VIEWABLE: the grab window or the @confine_to window are not
 *  viewable.
 * @GDK_GRAB_FROZEN: the resource is frozen by an active grab of another client.
 * @GDK_GRAB_FAILED: the grab failed for some other reason.
 *
 * Returned by gdk_device_grab(), gdk_pointer_grab() and gdk_keyboard_grab() to
 * indicate success or the reason for the failure of the grab attempt.
 */
typedef enum
{
  GDK_GRAB_SUCCESS         = 0,
  GDK_GRAB_ALREADY_GRABBED = 1,
  GDK_GRAB_INVALID_TIME    = 2,
  GDK_GRAB_NOT_VIEWABLE    = 3,
  GDK_GRAB_FROZEN          = 4,
  GDK_GRAB_FAILED          = 5
} GdkGrabStatus;

/**
 * GdkGrabOwnership:
 * @GDK_OWNERSHIP_NONE: All other devices’ events are allowed.
 * @GDK_OWNERSHIP_WINDOW: Other devices’ events are blocked for the grab window.
 * @GDK_OWNERSHIP_APPLICATION: Other devices’ events are blocked for the whole application.
 *
 * Defines how device grabs interact with other devices.
 */
typedef enum
{
  GDK_OWNERSHIP_NONE,
  GDK_OWNERSHIP_WINDOW,
  GDK_OWNERSHIP_APPLICATION
} GdkGrabOwnership;

/**
 * GdkEventMask:
 * @GDK_EXPOSURE_MASK: receive expose events
 * @GDK_POINTER_MOTION_MASK: receive all pointer motion events
 * @GDK_POINTER_MOTION_HINT_MASK: deprecated. see the explanation above
 * @GDK_BUTTON_MOTION_MASK: receive pointer motion events while any button is pressed
 * @GDK_BUTTON1_MOTION_MASK: receive pointer motion events while 1 button is pressed
 * @GDK_BUTTON2_MOTION_MASK: receive pointer motion events while 2 button is pressed
 * @GDK_BUTTON3_MOTION_MASK: receive pointer motion events while 3 button is pressed
 * @GDK_BUTTON_PRESS_MASK: receive button press events
 * @GDK_BUTTON_RELEASE_MASK: receive button release events
 * @GDK_KEY_PRESS_MASK: receive key press events
 * @GDK_KEY_RELEASE_MASK: receive key release events
 * @GDK_ENTER_NOTIFY_MASK: receive window enter events
 * @GDK_LEAVE_NOTIFY_MASK: receive window leave events
 * @GDK_FOCUS_CHANGE_MASK: receive focus change events
 * @GDK_STRUCTURE_MASK: receive events about window configuration change
 * @GDK_PROPERTY_CHANGE_MASK: receive property change events
 * @GDK_VISIBILITY_NOTIFY_MASK: receive visibility change events
 * @GDK_PROXIMITY_IN_MASK: receive proximity in events
 * @GDK_PROXIMITY_OUT_MASK: receive proximity out events
 * @GDK_SUBSTRUCTURE_MASK: receive events about window configuration changes of
 *   child windows
 * @GDK_SCROLL_MASK: receive scroll events
 * @GDK_TOUCH_MASK: receive touch events. Since 3.4
 * @GDK_SMOOTH_SCROLL_MASK: receive smooth scrolling events. Since 3.4
 * @GDK_ALL_EVENTS_MASK: the combination of all the above event masks.
 *
 * A set of bit-flags to indicate which events a window is to receive.
 * Most of these masks map onto one or more of the #GdkEventType event types
 * above.
 *
 * %GDK_POINTER_MOTION_HINT_MASK is deprecated. It is a special mask
 * to reduce the number of %GDK_MOTION_NOTIFY events received. When using
 * %GDK_POINTER_MOTION_HINT_MASK, fewer %GDK_MOTION_NOTIFY events will
 * be sent, some of which are marked as a hint (the is_hint member is
 * %TRUE). To receive more motion events after a motion hint event,
 * the application needs to asks for more, by calling
 * gdk_event_request_motions().
 * 
 * Since GTK 3.8, motion events are already compressed by default, independent
 * of this mechanism. This compression can be disabled with
 * gdk_window_set_event_compression(). See the documentation of that function
 * for details.
 *
 * If %GDK_TOUCH_MASK is enabled, the window will receive touch events
 * from touch-enabled devices. Those will come as sequences of #GdkEventTouch
 * with type %GDK_TOUCH_UPDATE, enclosed by two events with
 * type %GDK_TOUCH_BEGIN and %GDK_TOUCH_END (or %GDK_TOUCH_CANCEL).
 * gdk_event_get_event_sequence() returns the event sequence for these
 * events, so different sequences may be distinguished.
 */
typedef enum
{
  GDK_EXPOSURE_MASK             = 1 << 1,
  GDK_POINTER_MOTION_MASK       = 1 << 2,
  GDK_POINTER_MOTION_HINT_MASK  = 1 << 3,
  GDK_BUTTON_MOTION_MASK        = 1 << 4,
  GDK_BUTTON1_MOTION_MASK       = 1 << 5,
  GDK_BUTTON2_MOTION_MASK       = 1 << 6,
  GDK_BUTTON3_MOTION_MASK       = 1 << 7,
  GDK_BUTTON_PRESS_MASK         = 1 << 8,
  GDK_BUTTON_RELEASE_MASK       = 1 << 9,
  GDK_KEY_PRESS_MASK            = 1 << 10,
  GDK_KEY_RELEASE_MASK          = 1 << 11,
  GDK_ENTER_NOTIFY_MASK         = 1 << 12,
  GDK_LEAVE_NOTIFY_MASK         = 1 << 13,
  GDK_FOCUS_CHANGE_MASK         = 1 << 14,
  GDK_STRUCTURE_MASK            = 1 << 15,
  GDK_PROPERTY_CHANGE_MASK      = 1 << 16,
  GDK_VISIBILITY_NOTIFY_MASK    = 1 << 17,
  GDK_PROXIMITY_IN_MASK         = 1 << 18,
  GDK_PROXIMITY_OUT_MASK        = 1 << 19,
  GDK_SUBSTRUCTURE_MASK         = 1 << 20,
  GDK_SCROLL_MASK               = 1 << 21,
  GDK_TOUCH_MASK                = 1 << 22,
  GDK_SMOOTH_SCROLL_MASK        = 1 << 23,
  GDK_ALL_EVENTS_MASK           = 0xFFFFFE
} GdkEventMask;

/**
 * GdkPoint:
 * @x: the x coordinate of the point.
 * @y: the y coordinate of the point.
 *
 * Defines the x and y coordinates of a point.
 */
struct _GdkPoint
{
  gint x;
  gint y;
};

/**
 * GdkGLProfile:
 * @GDK_GL_PROFILE_DEFAULT: ...
 * @GDK_GL_PROFILE_LEGACY: ...
 * @GDK_GL_PROFILE_3_2_CORE: ...
 *
 * ...
 */
typedef enum {
  GDK_GL_PROFILE_DEFAULT,
  GDK_GL_PROFILE_LEGACY,
  GDK_GL_PROFILE_3_2_CORE
} GdkGLProfile;

typedef enum {
  GDK_GL_ERROR_NOT_AVAILABLE,
  GDK_GL_ERROR_UNSUPPORTED_FORMAT,
  GDK_GL_ERROR_UNSUPPORTED_PROFILE
} GdkGLError;

G_END_DECLS

#endif /* __GDK_TYPES_H__ */
