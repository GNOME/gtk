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

#ifndef __GDK_EVENTS_H__
#define __GDK_EVENTS_H__

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#include <gdk/gdkversionmacros.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkdnd.h>
#include <gdk/gdkdevice.h>
#include <gdk/gdkdevicetool.h>

G_BEGIN_DECLS


/**
 * SECTION:event_structs
 * @Short_description: Data structures specific to each type of event
 * @Title: Event Structures
 *
 * The event structures contain data specific to each type of event in GDK.
 *
 * > A common mistake is to forget to set the event mask of a widget so that
 * > the required events are received. See gtk_widget_set_events().
 */


#define GDK_TYPE_EVENT          (gdk_event_get_type ())
#define GDK_TYPE_EVENT_SEQUENCE (gdk_event_sequence_get_type ())

/**
 * GDK_PRIORITY_EVENTS:
 *
 * This is the priority that events from the X server are given in the
 * [GLib Main Loop][glib-The-Main-Event-Loop].
 */
#define GDK_PRIORITY_EVENTS	(G_PRIORITY_DEFAULT)

/**
 * GDK_PRIORITY_REDRAW:
 *
 * This is the priority that the idle handler processing window updates
 * is given in the
 * [GLib Main Loop][glib-The-Main-Event-Loop].
 */
#define GDK_PRIORITY_REDRAW     (G_PRIORITY_HIGH_IDLE + 20)

/**
 * GDK_EVENT_PROPAGATE:
 *
 * Use this macro as the return value for continuing the propagation of
 * an event handler.
 *
 * Since: 3.4
 */
#define GDK_EVENT_PROPAGATE     (FALSE)

/**
 * GDK_EVENT_STOP:
 *
 * Use this macro as the return value for stopping the propagation of
 * an event handler.
 *
 * Since: 3.4
 */
#define GDK_EVENT_STOP          (TRUE)

/**
 * GDK_BUTTON_PRIMARY:
 *
 * The primary button. This is typically the left mouse button, or the
 * right button in a left-handed setup.
 *
 * Since: 3.4
 */
#define GDK_BUTTON_PRIMARY      (1)

/**
 * GDK_BUTTON_MIDDLE:
 *
 * The middle button.
 *
 * Since: 3.4
 */
#define GDK_BUTTON_MIDDLE       (2)

/**
 * GDK_BUTTON_SECONDARY:
 *
 * The secondary button. This is typically the right mouse button, or the
 * left button in a left-handed setup.
 *
 * Since: 3.4
 */
#define GDK_BUTTON_SECONDARY    (3)



typedef struct _GdkEventAny	    GdkEventAny;
typedef struct _GdkEventExpose	    GdkEventExpose;
typedef struct _GdkEventVisibility  GdkEventVisibility;
typedef struct _GdkEventMotion	    GdkEventMotion;
typedef struct _GdkEventButton	    GdkEventButton;
typedef struct _GdkEventTouch       GdkEventTouch;
typedef struct _GdkEventScroll      GdkEventScroll;  
typedef struct _GdkEventKey	    GdkEventKey;
typedef struct _GdkEventFocus	    GdkEventFocus;
typedef struct _GdkEventCrossing    GdkEventCrossing;
typedef struct _GdkEventConfigure   GdkEventConfigure;
typedef struct _GdkEventProperty    GdkEventProperty;
typedef struct _GdkEventSelection   GdkEventSelection;
typedef struct _GdkEventOwnerChange GdkEventOwnerChange;
typedef struct _GdkEventProximity   GdkEventProximity;
typedef struct _GdkEventDND         GdkEventDND;
typedef struct _GdkEventWindowState GdkEventWindowState;
typedef struct _GdkEventSetting     GdkEventSetting;
typedef struct _GdkEventGrabBroken  GdkEventGrabBroken;
typedef struct _GdkEventTouchpadSwipe GdkEventTouchpadSwipe;
typedef struct _GdkEventTouchpadPinch GdkEventTouchpadPinch;
typedef struct _GdkEventPadButton   GdkEventPadButton;
typedef struct _GdkEventPadAxis     GdkEventPadAxis;
typedef struct _GdkEventPadGroupMode GdkEventPadGroupMode;

typedef struct _GdkEventSequence    GdkEventSequence;

typedef union  _GdkEvent	    GdkEvent;

/**
 * GdkEventFunc:
 * @event: the #GdkEvent to process.
 * @data: (closure): user data set when the event handler was installed with
 *   gdk_event_handler_set().
 *
 * Specifies the type of function passed to gdk_event_handler_set() to
 * handle all GDK events.
 */
typedef void (*GdkEventFunc) (GdkEvent *event,
			      gpointer	data);

/* Event filtering */

/**
 * GdkXEvent:
 *
 * Used to represent native events (XEvents for the X11
 * backend, MSGs for Win32).
 */
typedef void GdkXEvent;	  /* Can be cast to window system specific
			   * even type, XEvent on X11, MSG on Win32.
			   */

/**
 * GdkFilterReturn:
 * @GDK_FILTER_CONTINUE: event not handled, continue processing.
 * @GDK_FILTER_TRANSLATE: native event translated into a GDK event and stored
 *  in the `event` structure that was passed in.
 * @GDK_FILTER_REMOVE: event handled, terminate processing.
 *
 * Specifies the result of applying a #GdkFilterFunc to a native event.
 */
typedef enum {
  GDK_FILTER_CONTINUE,	  /* Event not handled, continue processesing */
  GDK_FILTER_TRANSLATE,	  /* Native event translated into a GDK event and
                             stored in the "event" structure that was
                             passed in */
  GDK_FILTER_REMOVE	  /* Terminate processing, removing event */
} GdkFilterReturn;

/**
 * GdkFilterFunc:
 * @xevent: the native event to filter.
 * @event: the GDK event to which the X event will be translated.
 * @data: (closure): user data set when the filter was installed.
 *
 * Specifies the type of function used to filter native events before they are
 * converted to GDK events.
 *
 * When a filter is called, @event is unpopulated, except for
 * `event->window`. The filter may translate the native
 * event to a GDK event and store the result in @event, or handle it without
 * translation. If the filter translates the event and processing should
 * continue, it should return %GDK_FILTER_TRANSLATE.
 *
 * Returns: a #GdkFilterReturn value.
 */
typedef GdkFilterReturn (*GdkFilterFunc) (GdkXEvent *xevent,
					  GdkEvent *event,
					  gpointer  data);

/**
 * GdkEventType:
 * @GDK_NOTHING: a special code to indicate a null event.
 * @GDK_DELETE: the window manager has requested that the toplevel window be
 *   hidden or destroyed, usually when the user clicks on a special icon in the
 *   title bar.
 * @GDK_DESTROY: the window has been destroyed.
 * @GDK_EXPOSE: all or part of the window has become visible and needs to be
 *   redrawn.
 * @GDK_MOTION_NOTIFY: the pointer (usually a mouse) has moved.
 * @GDK_BUTTON_PRESS: a mouse button has been pressed.
 * @GDK_2BUTTON_PRESS: a mouse button has been double-clicked (clicked twice
 *   within a short period of time). Note that each click also generates a
 *   %GDK_BUTTON_PRESS event.
 * @GDK_DOUBLE_BUTTON_PRESS: alias for %GDK_2BUTTON_PRESS, added in 3.6.
 * @GDK_3BUTTON_PRESS: a mouse button has been clicked 3 times in a short period
 *   of time. Note that each click also generates a %GDK_BUTTON_PRESS event.
 * @GDK_TRIPLE_BUTTON_PRESS: alias for %GDK_3BUTTON_PRESS, added in 3.6.
 * @GDK_BUTTON_RELEASE: a mouse button has been released.
 * @GDK_KEY_PRESS: a key has been pressed.
 * @GDK_KEY_RELEASE: a key has been released.
 * @GDK_ENTER_NOTIFY: the pointer has entered the window.
 * @GDK_LEAVE_NOTIFY: the pointer has left the window.
 * @GDK_FOCUS_CHANGE: the keyboard focus has entered or left the window.
 * @GDK_CONFIGURE: the size, position or stacking order of the window has changed.
 *   Note that GTK+ discards these events for %GDK_WINDOW_CHILD windows.
 * @GDK_MAP: the window has been mapped.
 * @GDK_UNMAP: the window has been unmapped.
 * @GDK_PROPERTY_NOTIFY: a property on the window has been changed or deleted.
 * @GDK_SELECTION_CLEAR: the application has lost ownership of a selection.
 * @GDK_SELECTION_REQUEST: another application has requested a selection.
 * @GDK_SELECTION_NOTIFY: a selection has been received.
 * @GDK_PROXIMITY_IN: an input device has moved into contact with a sensing
 *   surface (e.g. a touchscreen or graphics tablet).
 * @GDK_PROXIMITY_OUT: an input device has moved out of contact with a sensing
 *   surface.
 * @GDK_DRAG_ENTER: the mouse has entered the window while a drag is in progress.
 * @GDK_DRAG_LEAVE: the mouse has left the window while a drag is in progress.
 * @GDK_DRAG_MOTION: the mouse has moved in the window while a drag is in
 *   progress.
 * @GDK_DRAG_STATUS: the status of the drag operation initiated by the window
 *   has changed.
 * @GDK_DROP_START: a drop operation onto the window has started.
 * @GDK_DROP_FINISHED: the drop operation initiated by the window has completed.
 * @GDK_CLIENT_EVENT: a message has been received from another application.
 * @GDK_VISIBILITY_NOTIFY: the window visibility status has changed.
 * @GDK_SCROLL: the scroll wheel was turned
 * @GDK_WINDOW_STATE: the state of a window has changed. See #GdkWindowState
 *   for the possible window states
 * @GDK_SETTING: a setting has been modified.
 * @GDK_OWNER_CHANGE: the owner of a selection has changed. This event type
 *   was added in 2.6
 * @GDK_GRAB_BROKEN: a pointer or keyboard grab was broken. This event type
 *   was added in 2.8.
 * @GDK_DAMAGE: the content of the window has been changed. This event type
 *   was added in 2.14.
 * @GDK_TOUCH_BEGIN: A new touch event sequence has just started. This event
 *   type was added in 3.4.
 * @GDK_TOUCH_UPDATE: A touch event sequence has been updated. This event type
 *   was added in 3.4.
 * @GDK_TOUCH_END: A touch event sequence has finished. This event type
 *   was added in 3.4.
 * @GDK_TOUCH_CANCEL: A touch event sequence has been canceled. This event type
 *   was added in 3.4.
 * @GDK_TOUCHPAD_SWIPE: A touchpad swipe gesture event, the current state
 *   is determined by its phase field. This event type was added in 3.18.
 * @GDK_TOUCHPAD_PINCH: A touchpad pinch gesture event, the current state
 *   is determined by its phase field. This event type was added in 3.18.
 * @GDK_PAD_BUTTON_PRESS: A tablet pad button press event. This event type
 *   was added in 3.22.
 * @GDK_PAD_BUTTON_RELEASE: A tablet pad button release event. This event type
 *   was added in 3.22.
 * @GDK_PAD_RING: A tablet pad axis event from a "ring". This event type was
 *   added in 3.22.
 * @GDK_PAD_STRIP: A tablet pad axis event from a "strip". This event type was
 *   added in 3.22.
 * @GDK_PAD_GROUP_MODE: A tablet pad group mode change. This event type was
 *   added in 3.22.
 * @GDK_EVENT_LAST: marks the end of the GdkEventType enumeration. Added in 2.18
 *
 * Specifies the type of the event.
 *
 * Do not confuse these events with the signals that GTK+ widgets emit.
 * Although many of these events result in corresponding signals being emitted,
 * the events are often transformed or filtered along the way.
 *
 * In some language bindings, the values %GDK_2BUTTON_PRESS and
 * %GDK_3BUTTON_PRESS would translate into something syntactically
 * invalid (eg `Gdk.EventType.2ButtonPress`, where a
 * symbol is not allowed to start with a number). In that case, the
 * aliases %GDK_DOUBLE_BUTTON_PRESS and %GDK_TRIPLE_BUTTON_PRESS can
 * be used instead.
 */
typedef enum
{
  GDK_NOTHING		= -1,
  GDK_DELETE		= 0,
  GDK_DESTROY		= 1,
  GDK_EXPOSE		= 2,
  GDK_MOTION_NOTIFY	= 3,
  GDK_BUTTON_PRESS	= 4,
  GDK_2BUTTON_PRESS	= 5,
  GDK_DOUBLE_BUTTON_PRESS = GDK_2BUTTON_PRESS,
  GDK_3BUTTON_PRESS	= 6,
  GDK_TRIPLE_BUTTON_PRESS = GDK_3BUTTON_PRESS,
  GDK_BUTTON_RELEASE	= 7,
  GDK_KEY_PRESS		= 8,
  GDK_KEY_RELEASE	= 9,
  GDK_ENTER_NOTIFY	= 10,
  GDK_LEAVE_NOTIFY	= 11,
  GDK_FOCUS_CHANGE	= 12,
  GDK_CONFIGURE		= 13,
  GDK_MAP		= 14,
  GDK_UNMAP		= 15,
  GDK_PROPERTY_NOTIFY	= 16,
  GDK_SELECTION_CLEAR	= 17,
  GDK_SELECTION_REQUEST = 18,
  GDK_SELECTION_NOTIFY	= 19,
  GDK_PROXIMITY_IN	= 20,
  GDK_PROXIMITY_OUT	= 21,
  GDK_DRAG_ENTER        = 22,
  GDK_DRAG_LEAVE        = 23,
  GDK_DRAG_MOTION       = 24,
  GDK_DRAG_STATUS       = 25,
  GDK_DROP_START        = 26,
  GDK_DROP_FINISHED     = 27,
  GDK_CLIENT_EVENT	= 28,
  GDK_VISIBILITY_NOTIFY = 29,
  GDK_SCROLL            = 31,
  GDK_WINDOW_STATE      = 32,
  GDK_SETTING           = 33,
  GDK_OWNER_CHANGE      = 34,
  GDK_GRAB_BROKEN       = 35,
  GDK_DAMAGE            = 36,
  GDK_TOUCH_BEGIN       = 37,
  GDK_TOUCH_UPDATE      = 38,
  GDK_TOUCH_END         = 39,
  GDK_TOUCH_CANCEL      = 40,
  GDK_TOUCHPAD_SWIPE    = 41,
  GDK_TOUCHPAD_PINCH    = 42,
  GDK_PAD_BUTTON_PRESS  = 43,
  GDK_PAD_BUTTON_RELEASE = 44,
  GDK_PAD_RING          = 45,
  GDK_PAD_STRIP         = 46,
  GDK_PAD_GROUP_MODE    = 47,
  GDK_EVENT_LAST        /* helper variable for decls */
} GdkEventType;

/**
 * GdkVisibilityState:
 * @GDK_VISIBILITY_UNOBSCURED: the window is completely visible.
 * @GDK_VISIBILITY_PARTIAL: the window is partially visible.
 * @GDK_VISIBILITY_FULLY_OBSCURED: the window is not visible at all.
 *
 * Specifies the visiblity status of a window for a #GdkEventVisibility.
 */
typedef enum
{
  GDK_VISIBILITY_UNOBSCURED,
  GDK_VISIBILITY_PARTIAL,
  GDK_VISIBILITY_FULLY_OBSCURED
} GdkVisibilityState;

/**
 * GdkTouchpadGesturePhase:
 * @GDK_TOUCHPAD_GESTURE_PHASE_BEGIN: The gesture has begun.
 * @GDK_TOUCHPAD_GESTURE_PHASE_UPDATE: The gesture has been updated.
 * @GDK_TOUCHPAD_GESTURE_PHASE_END: The gesture was finished, changes
 *   should be permanently applied.
 * @GDK_TOUCHPAD_GESTURE_PHASE_CANCEL: The gesture was cancelled, all
 *   changes should be undone.
 *
 * Specifies the current state of a touchpad gesture. All gestures are
 * guaranteed to begin with an event with phase %GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
 * followed by 0 or several events with phase %GDK_TOUCHPAD_GESTURE_PHASE_UPDATE.
 *
 * A finished gesture may have 2 possible outcomes, an event with phase
 * %GDK_TOUCHPAD_GESTURE_PHASE_END will be emitted when the gesture is
 * considered successful, this should be used as the hint to perform any
 * permanent changes.

 * Cancelled gestures may be so for a variety of reasons, due to hardware
 * or the compositor, or due to the gesture recognition layers hinting the
 * gesture did not finish resolutely (eg. a 3rd finger being added during
 * a pinch gesture). In these cases, the last event will report the phase
 * %GDK_TOUCHPAD_GESTURE_PHASE_CANCEL, this should be used as a hint
 * to undo any visible/permanent changes that were done throughout the
 * progress of the gesture.
 *
 * See also #GdkEventTouchpadSwipe and #GdkEventTouchpadPinch.
 *
 */
typedef enum
{
  GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
  GDK_TOUCHPAD_GESTURE_PHASE_UPDATE,
  GDK_TOUCHPAD_GESTURE_PHASE_END,
  GDK_TOUCHPAD_GESTURE_PHASE_CANCEL
} GdkTouchpadGesturePhase;

/**
 * GdkScrollDirection:
 * @GDK_SCROLL_UP: the window is scrolled up.
 * @GDK_SCROLL_DOWN: the window is scrolled down.
 * @GDK_SCROLL_LEFT: the window is scrolled to the left.
 * @GDK_SCROLL_RIGHT: the window is scrolled to the right.
 * @GDK_SCROLL_SMOOTH: the scrolling is determined by the delta values
 *   in #GdkEventScroll. See gdk_event_get_scroll_deltas(). Since: 3.4
 *
 * Specifies the direction for #GdkEventScroll.
 */
typedef enum
{
  GDK_SCROLL_UP,
  GDK_SCROLL_DOWN,
  GDK_SCROLL_LEFT,
  GDK_SCROLL_RIGHT,
  GDK_SCROLL_SMOOTH
} GdkScrollDirection;

/**
 * GdkNotifyType:
 * @GDK_NOTIFY_ANCESTOR: the window is entered from an ancestor or
 *   left towards an ancestor.
 * @GDK_NOTIFY_VIRTUAL: the pointer moves between an ancestor and an
 *   inferior of the window.
 * @GDK_NOTIFY_INFERIOR: the window is entered from an inferior or
 *   left towards an inferior.
 * @GDK_NOTIFY_NONLINEAR: the window is entered from or left towards
 *   a window which is neither an ancestor nor an inferior.
 * @GDK_NOTIFY_NONLINEAR_VIRTUAL: the pointer moves between two windows
 *   which are not ancestors of each other and the window is part of
 *   the ancestor chain between one of these windows and their least
 *   common ancestor.
 * @GDK_NOTIFY_UNKNOWN: an unknown type of enter/leave event occurred.
 *
 * Specifies the kind of crossing for #GdkEventCrossing.
 *
 * See the X11 protocol specification of LeaveNotify for
 * full details of crossing event generation.
 */
typedef enum
{
  GDK_NOTIFY_ANCESTOR		= 0,
  GDK_NOTIFY_VIRTUAL		= 1,
  GDK_NOTIFY_INFERIOR		= 2,
  GDK_NOTIFY_NONLINEAR		= 3,
  GDK_NOTIFY_NONLINEAR_VIRTUAL	= 4,
  GDK_NOTIFY_UNKNOWN		= 5
} GdkNotifyType;

/**
 * GdkCrossingMode:
 * @GDK_CROSSING_NORMAL: crossing because of pointer motion.
 * @GDK_CROSSING_GRAB: crossing because a grab is activated.
 * @GDK_CROSSING_UNGRAB: crossing because a grab is deactivated.
 * @GDK_CROSSING_GTK_GRAB: crossing because a GTK+ grab is activated.
 * @GDK_CROSSING_GTK_UNGRAB: crossing because a GTK+ grab is deactivated.
 * @GDK_CROSSING_STATE_CHANGED: crossing because a GTK+ widget changed
 *   state (e.g. sensitivity).
 * @GDK_CROSSING_TOUCH_BEGIN: crossing because a touch sequence has begun,
 *   this event is synthetic as the pointer might have not left the window.
 * @GDK_CROSSING_TOUCH_END: crossing because a touch sequence has ended,
 *   this event is synthetic as the pointer might have not left the window.
 * @GDK_CROSSING_DEVICE_SWITCH: crossing because of a device switch (i.e.
 *   a mouse taking control of the pointer after a touch device), this event
 *   is synthetic as the pointer didn’t leave the window.
 *
 * Specifies the crossing mode for #GdkEventCrossing.
 */
typedef enum
{
  GDK_CROSSING_NORMAL,
  GDK_CROSSING_GRAB,
  GDK_CROSSING_UNGRAB,
  GDK_CROSSING_GTK_GRAB,
  GDK_CROSSING_GTK_UNGRAB,
  GDK_CROSSING_STATE_CHANGED,
  GDK_CROSSING_TOUCH_BEGIN,
  GDK_CROSSING_TOUCH_END,
  GDK_CROSSING_DEVICE_SWITCH
} GdkCrossingMode;

/**
 * GdkPropertyState:
 * @GDK_PROPERTY_NEW_VALUE: the property value was changed.
 * @GDK_PROPERTY_DELETE: the property was deleted.
 *
 * Specifies the type of a property change for a #GdkEventProperty.
 */
typedef enum
{
  GDK_PROPERTY_NEW_VALUE,
  GDK_PROPERTY_DELETE
} GdkPropertyState;

/**
 * GdkWindowState:
 * @GDK_WINDOW_STATE_WITHDRAWN: the window is not shown.
 * @GDK_WINDOW_STATE_ICONIFIED: the window is minimized.
 * @GDK_WINDOW_STATE_MAXIMIZED: the window is maximized.
 * @GDK_WINDOW_STATE_STICKY: the window is sticky.
 * @GDK_WINDOW_STATE_FULLSCREEN: the window is maximized without
 *   decorations.
 * @GDK_WINDOW_STATE_ABOVE: the window is kept above other windows.
 * @GDK_WINDOW_STATE_BELOW: the window is kept below other windows.
 * @GDK_WINDOW_STATE_FOCUSED: the window is presented as focused (with active decorations).
 * @GDK_WINDOW_STATE_TILED: the window is in a tiled state, Since 3.10
 *
 * Specifies the state of a toplevel window.
 */
typedef enum
{
  GDK_WINDOW_STATE_WITHDRAWN  = 1 << 0,
  GDK_WINDOW_STATE_ICONIFIED  = 1 << 1,
  GDK_WINDOW_STATE_MAXIMIZED  = 1 << 2,
  GDK_WINDOW_STATE_STICKY     = 1 << 3,
  GDK_WINDOW_STATE_FULLSCREEN = 1 << 4,
  GDK_WINDOW_STATE_ABOVE      = 1 << 5,
  GDK_WINDOW_STATE_BELOW      = 1 << 6,
  GDK_WINDOW_STATE_FOCUSED    = 1 << 7,
  GDK_WINDOW_STATE_TILED      = 1 << 8
} GdkWindowState;

/**
 * GdkSettingAction:
 * @GDK_SETTING_ACTION_NEW: a setting was added.
 * @GDK_SETTING_ACTION_CHANGED: a setting was changed.
 * @GDK_SETTING_ACTION_DELETED: a setting was deleted.
 *
 * Specifies the kind of modification applied to a setting in a
 * #GdkEventSetting.
 */
typedef enum
{
  GDK_SETTING_ACTION_NEW,
  GDK_SETTING_ACTION_CHANGED,
  GDK_SETTING_ACTION_DELETED
} GdkSettingAction;

/**
 * GdkOwnerChange:
 * @GDK_OWNER_CHANGE_NEW_OWNER: some other app claimed the ownership
 * @GDK_OWNER_CHANGE_DESTROY: the window was destroyed
 * @GDK_OWNER_CHANGE_CLOSE: the client was closed
 *
 * Specifies why a selection ownership was changed.
 */
typedef enum
{
  GDK_OWNER_CHANGE_NEW_OWNER,
  GDK_OWNER_CHANGE_DESTROY,
  GDK_OWNER_CHANGE_CLOSE
} GdkOwnerChange;

GDK_AVAILABLE_IN_ALL
GType     gdk_event_get_type            (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_3_14
GType     gdk_event_sequence_get_type   (void) G_GNUC_CONST;

GDK_AVAILABLE_IN_ALL
gboolean  gdk_events_pending	 	(void);
GDK_AVAILABLE_IN_ALL
GdkEvent* gdk_event_get			(void);

GDK_AVAILABLE_IN_ALL
GdkEvent* gdk_event_peek                (void);
GDK_AVAILABLE_IN_ALL
void      gdk_event_put	 		(const GdkEvent *event);

GDK_AVAILABLE_IN_ALL
GdkEvent* gdk_event_new                 (GdkEventType    type);
GDK_AVAILABLE_IN_ALL
GdkEvent* gdk_event_copy     		(const GdkEvent *event);
GDK_AVAILABLE_IN_ALL
void	  gdk_event_free     		(GdkEvent 	*event);

GDK_AVAILABLE_IN_3_10
GdkWindow *gdk_event_get_window         (const GdkEvent *event);

GDK_AVAILABLE_IN_ALL
guint32   gdk_event_get_time            (const GdkEvent  *event);
GDK_AVAILABLE_IN_ALL
gboolean  gdk_event_get_state           (const GdkEvent  *event,
                                         GdkModifierType *state);
GDK_AVAILABLE_IN_ALL
gboolean  gdk_event_get_coords		(const GdkEvent  *event,
					 gdouble	 *x_win,
					 gdouble	 *y_win);
GDK_AVAILABLE_IN_3_92
void      gdk_event_set_coords          (GdkEvent *event,
                                         gdouble   x,
                                         gdouble   y);
GDK_AVAILABLE_IN_ALL
gboolean  gdk_event_get_root_coords	(const GdkEvent *event,
					 gdouble	*x_root,
					 gdouble	*y_root);
GDK_AVAILABLE_IN_3_2
gboolean  gdk_event_get_button          (const GdkEvent *event,
                                         guint          *button);
GDK_AVAILABLE_IN_3_2
gboolean  gdk_event_get_click_count     (const GdkEvent *event,
                                         guint          *click_count);
GDK_AVAILABLE_IN_3_2
gboolean  gdk_event_get_keyval          (const GdkEvent *event,
                                         guint          *keyval);
GDK_AVAILABLE_IN_3_2
gboolean  gdk_event_get_keycode         (const GdkEvent *event,
                                         guint16        *keycode);
GDK_AVAILABLE_IN_3_2
gboolean gdk_event_get_scroll_direction (const GdkEvent *event,
                                         GdkScrollDirection *direction);
GDK_AVAILABLE_IN_3_4
gboolean  gdk_event_get_scroll_deltas   (const GdkEvent *event,
                                         gdouble         *delta_x,
                                         gdouble         *delta_y);

GDK_AVAILABLE_IN_3_20
gboolean  gdk_event_is_scroll_stop_event (const GdkEvent *event);

GDK_AVAILABLE_IN_ALL
gboolean  gdk_event_get_axis            (const GdkEvent  *event,
                                         GdkAxisUse       axis_use,
                                         gdouble         *value);
GDK_AVAILABLE_IN_ALL
void       gdk_event_set_device         (GdkEvent        *event,
                                         GdkDevice       *device);
GDK_AVAILABLE_IN_ALL
GdkDevice* gdk_event_get_device         (const GdkEvent  *event);
GDK_AVAILABLE_IN_ALL
void       gdk_event_set_source_device  (GdkEvent        *event,
                                         GdkDevice       *device);
GDK_AVAILABLE_IN_ALL
GdkDevice* gdk_event_get_source_device  (const GdkEvent  *event);
GDK_AVAILABLE_IN_ALL
void       gdk_event_request_motions    (const GdkEventMotion *event);
GDK_AVAILABLE_IN_3_4
gboolean   gdk_event_triggers_context_menu (const GdkEvent *event);

GDK_AVAILABLE_IN_ALL
gboolean  gdk_events_get_distance       (GdkEvent        *event1,
                                         GdkEvent        *event2,
                                         gdouble         *distance);
GDK_AVAILABLE_IN_ALL
gboolean  gdk_events_get_angle          (GdkEvent        *event1,
                                         GdkEvent        *event2,
                                         gdouble         *angle);
GDK_AVAILABLE_IN_ALL
gboolean  gdk_events_get_center         (GdkEvent        *event1,
                                         GdkEvent        *event2,
                                         gdouble         *x,
                                         gdouble         *y);

GDK_AVAILABLE_IN_ALL
void	  gdk_event_handler_set 	(GdkEventFunc    func,
					 gpointer        data,
					 GDestroyNotify  notify);

GDK_AVAILABLE_IN_ALL
void       gdk_event_set_screen         (GdkEvent        *event,
                                         GdkScreen       *screen);
GDK_AVAILABLE_IN_ALL
GdkScreen *gdk_event_get_screen         (const GdkEvent  *event);

GDK_AVAILABLE_IN_3_4
GdkEventSequence *gdk_event_get_event_sequence (const GdkEvent *event);

GDK_AVAILABLE_IN_3_10
GdkEventType gdk_event_get_event_type   (const GdkEvent *event);

GDK_AVAILABLE_IN_3_20
GdkSeat  *gdk_event_get_seat            (const GdkEvent *event);

GDK_AVAILABLE_IN_ALL
void	  gdk_set_show_events		(gboolean	 show_events);
GDK_AVAILABLE_IN_ALL
gboolean  gdk_get_show_events		(void);

GDK_AVAILABLE_IN_ALL
gboolean gdk_setting_get                (const gchar    *name,
                                         GValue         *value);

GDK_AVAILABLE_IN_3_22
GdkDeviceTool *gdk_event_get_device_tool (const GdkEvent *event);

GDK_AVAILABLE_IN_3_22
void           gdk_event_set_device_tool (GdkEvent       *event,
                                          GdkDeviceTool  *tool);

GDK_AVAILABLE_IN_3_22
int            gdk_event_get_scancode    (GdkEvent *event);

GDK_AVAILABLE_IN_3_22
gboolean       gdk_event_get_pointer_emulated (GdkEvent *event);

GDK_AVAILABLE_IN_3_92
void           gdk_event_set_user_data (GdkEvent *event,
                                        GObject  *user_data);

G_END_DECLS

#endif /* __GDK_EVENTS_H__ */
