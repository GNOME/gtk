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

#ifndef __GDK_EVENTS_PRIVATE_H__
#define __GDK_EVENTS_PRIVATE_H__

#include <gdk/gdktypes.h>
#include <gdk/gdkdrag.h>
#include <gdk/gdkdevice.h>
#include <gdk/gdkdevicetool.h>

#define GDK_EVENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_EVENT, GdkEventClass))
#define GDK_IS_EVENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_EVENT))
#define GDK_EVENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_EVENT, GdkEventClass))

typedef struct _GdkEventClass GdkEventClass;

struct _GdkEventClass
{
  GObjectClass object_class;
};

/*
 * GdkEventAny:
 * @type: the type of the event.
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 *
 * Contains the fields which are common to all event structs.
 * Any event pointer can safely be cast to a pointer to a #GdkEventAny to
 * access these fields.
 */
struct _GdkEventAny
{
  GObject parent_instance;
  GdkEventType type;
  GdkSurface *surface;
  guint16 flags;
  gint8 send_event;
  GdkDevice *device;
  GdkDevice *source_device;
  GdkDisplay *display;
  GObject *target;
};

/*
 * GdkEventMotion:
 * @type: the type of the event.
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @time: the time of the event in milliseconds.
 * @x: the x coordinate of the pointer relative to the surface.
 * @y: the y coordinate of the pointer relative to the surface.
 * @axes: @x, @y translated to the axes of @device, or %NULL if @device is
 *   the mouse.
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See #GdkModifierType.
 * @device: the master device that the event originated from. Use
 * gdk_event_get_source_device() to get the slave device.
 *   screen.
 *
 * Generated when the pointer moves.
 */
struct _GdkEventMotion
{
  GdkEventAny any;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble *axes;
  guint state;
  GdkDeviceTool *tool;
  GList *history;
};

/*
 * GdkEventButton:
 * @type: the type of the event (%GDK_BUTTON_PRESS or %GDK_BUTTON_RELEASE).
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @time: the time of the event in milliseconds.
 * @x: the x coordinate of the pointer relative to the surface.
 * @y: the y coordinate of the pointer relative to the surface.
 * @axes: @x, @y translated to the axes of @device, or %NULL if @device is
 *   the mouse.
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See #GdkModifierType.
 * @button: the button which was pressed or released, numbered from 1 to 5.
 *   Normally button 1 is the left mouse button, 2 is the middle button,
 *   and 3 is the right button. On 2-button mice, the middle button can
 *   often be simulated by pressing both mouse buttons together.
 * @device: the master device that the event originated from. Use
 * gdk_event_get_source_device() to get the slave device.
 *   screen.
 *
 * Used for button press and button release events. The
 * @type field will be one of %GDK_BUTTON_PRESS or %GDK_BUTTON_RELEASE,
 */
struct _GdkEventButton
{
  GdkEventAny any;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble *axes;
  guint state;
  guint button;
  GdkDeviceTool *tool;
};

/*
 * GdkEventTouch:
 * @type: the type of the event (%GDK_TOUCH_BEGIN, %GDK_TOUCH_UPDATE,
 *   %GDK_TOUCH_END, %GDK_TOUCH_CANCEL)
 * @surface: the surface which received the event
 * @send_event: %TRUE if the event was sent explicitly.
 * @time: the time of the event in milliseconds.
 * @x: the x coordinate of the pointer relative to the surface
 * @y: the y coordinate of the pointer relative to the surface
 * @axes: @x, @y translated to the axes of @device, or %NULL if @device is
 *   the mouse
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See #GdkModifierType
 * @sequence: the event sequence that the event belongs to
 * @emulating_pointer: whether the event should be used for emulating
 *   pointer event
 * @device: the master device that the event originated from. Use
 * gdk_event_get_source_device() to get the slave device.
 *   screen
 *
 * Used for touch events.
 * @type field will be one of %GDK_TOUCH_BEGIN, %GDK_TOUCH_UPDATE,
 * %GDK_TOUCH_END or %GDK_TOUCH_CANCEL.
 *
 * Touch events are grouped into sequences by means of the @sequence
 * field, which can also be obtained with gdk_event_get_event_sequence().
 * Each sequence begins with a %GDK_TOUCH_BEGIN event, followed by
 * any number of %GDK_TOUCH_UPDATE events, and ends with a %GDK_TOUCH_END
 * (or %GDK_TOUCH_CANCEL) event. With multitouch devices, there may be
 * several active sequences at the same time.
 */
struct _GdkEventTouch
{
  GdkEventAny any;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble *axes;
  guint state;
  GdkEventSequence *sequence;
  gboolean emulating_pointer;
};

/*
 * GdkEventScroll:
 * @type: the type of the event (%GDK_SCROLL).
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @time: the time of the event in milliseconds.
 * @x: the x coordinate of the pointer relative to the surface.
 * @y: the y coordinate of the pointer relative to the surface.
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See #GdkModifierType.
 * @direction: the direction to scroll to (one of %GDK_SCROLL_UP,
 *   %GDK_SCROLL_DOWN, %GDK_SCROLL_LEFT, %GDK_SCROLL_RIGHT or
 *   %GDK_SCROLL_SMOOTH).
 * @device: the master device that the event originated from. Use
 * gdk_event_get_source_device() to get the slave device.
 * @delta_x: the x coordinate of the scroll delta
 * @delta_y: the y coordinate of the scroll delta
 *
 * Generated from button presses for the buttons 4 to 7. Wheel mice are
 * usually configured to generate button press events for buttons 4 and 5
 * when the wheel is turned.
 *
 * Some GDK backends can also generate “smooth” scroll events, which
 * can be recognized by the %GDK_SCROLL_SMOOTH scroll direction. For
 * these, the scroll deltas can be obtained with
 * gdk_event_get_scroll_deltas().
 */
struct _GdkEventScroll
{
  GdkEventAny any;
  guint32 time;
  gdouble x;
  gdouble y;
  guint state;
  GdkScrollDirection direction;
  gdouble delta_x;
  gdouble delta_y;
  guint is_stop : 1;
  GdkDeviceTool *tool;
};

/*
 * GdkEventKey:
 * @type: the type of the event (%GDK_KEY_PRESS or %GDK_KEY_RELEASE).
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @time: the time of the event in milliseconds.
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See #GdkModifierType.
 * @keyval: the key that was pressed or released. See the
 *   `gdk/gdkkeysyms.h` header file for a
 *   complete list of GDK key codes.
 * @hardware_keycode: the raw code of the key that was pressed or released.
 * @group: the keyboard group.
 * @is_modifier: a flag that indicates if @hardware_keycode is mapped to a
 *   modifier
 *
 * Describes a key press or key release event.
 */
struct _GdkEventKey
{
  GdkEventAny any;
  guint32 time;
  guint state;
  guint keyval;
  guint16 hardware_keycode;
  guint16 key_scancode;
  guint8 group;
  guint is_modifier : 1;
};

/*
 * GdkEventCrossing:
 * @type: the type of the event (%GDK_ENTER_NOTIFY or %GDK_LEAVE_NOTIFY).
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @child_surface: the surface that was entered or left.
 * @time: the time of the event in milliseconds.
 * @x: the x coordinate of the pointer relative to the surface.
 * @y: the y coordinate of the pointer relative to the surface.
 * @mode: the crossing mode (%GDK_CROSSING_NORMAL, %GDK_CROSSING_GRAB,
 *  %GDK_CROSSING_UNGRAB, %GDK_CROSSING_GTK_GRAB, %GDK_CROSSING_GTK_UNGRAB or
 *  %GDK_CROSSING_STATE_CHANGED).  %GDK_CROSSING_GTK_GRAB, %GDK_CROSSING_GTK_UNGRAB,
 *  and %GDK_CROSSING_STATE_CHANGED were added in 2.14 and are always synthesized,
 *  never native.
 * @detail: the kind of crossing that happened (%GDK_NOTIFY_INFERIOR,
 *  %GDK_NOTIFY_ANCESTOR, %GDK_NOTIFY_VIRTUAL, %GDK_NOTIFY_NONLINEAR or
 *  %GDK_NOTIFY_NONLINEAR_VIRTUAL).
 * @focus: %TRUE if @surface is the focus surface or an inferior.
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See #GdkModifierType.
 *
 * Generated when the pointer enters or leaves a surface.
 */
struct _GdkEventCrossing
{
  GdkEventAny any;
  GdkSurface *child_surface;
  guint32 time;
  gdouble x;
  gdouble y;
  GdkCrossingMode mode;
  GdkNotifyType detail;
  gboolean focus;
  guint state;
  GObject *related_target;
};

/*
 * GdkEventFocus:
 * @type: the type of the event (%GDK_FOCUS_CHANGE).
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @in: %TRUE if the surface has gained the keyboard focus, %FALSE if
 *   it has lost the focus.
 * @mode: the crossing mode
 * @detail: the kind of crossing that happened
 *
 * Describes a change of keyboard focus.
 */
struct _GdkEventFocus
{
  GdkEventAny any;
  gint16 in;
  GdkCrossingMode mode;
  GdkNotifyType detail;
  GObject *related_target;
};

/*
 * GdkEventConfigure:
 * @type: the type of the event (%GDK_CONFIGURE).
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @x: the new x coordinate of the surface, relative to its parent.
 * @y: the new y coordinate of the surface, relative to its parent.
 * @width: the new width of the surface.
 * @height: the new height of the surface.
 *
 * Generated when a surface size or position has changed.
 */
struct _GdkEventConfigure
{
  GdkEventAny any;
  gint x, y;
  gint width;
  gint height;
};

/*
 * GdkEventProximity:
 * @type: the type of the event (%GDK_PROXIMITY_IN or %GDK_PROXIMITY_OUT).
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @time: the time of the event in milliseconds.
 * @device: the master device that the event originated from. Use
 * gdk_event_get_source_device() to get the slave device.
 *
 * Proximity events are generated when using GDK’s wrapper for the
 * XInput extension. The XInput extension is an add-on for standard X
 * that allows you to use nonstandard devices such as graphics tablets.
 * A proximity event indicates that the stylus has moved in or out of
 * contact with the tablet, or perhaps that the user’s finger has moved
 * in or out of contact with a touch screen.
 *
 * This event type will be used pretty rarely. It only is important for
 * XInput aware programs that are drawing their own cursor.
 */
struct _GdkEventProximity
{
  GdkEventAny any;
  guint32 time;
  GdkDeviceTool *tool;
};

/*
 * GdkEventGrabBroken:
 * @type: the type of the event (%GDK_GRAB_BROKEN)
 * @surface: the surface which received the event, i.e. the surface
 *   that previously owned the grab
 * @send_event: %TRUE if the event was sent explicitly.
 * @keyboard: %TRUE if a keyboard grab was broken, %FALSE if a pointer
 *   grab was broken
 * @implicit: %TRUE if the broken grab was implicit
 * @grab_surface: If this event is caused by another grab in the same
 *   application, @grab_surface contains the new grab surface. Otherwise
 *   @grab_surface is %NULL.
 *
 * Generated when a pointer or keyboard grab is broken. On X11, this happens
 * when the grab surface becomes unviewable (i.e. it or one of its ancestors
 * is unmapped), or if the same application grabs the pointer or keyboard
 * again. Note that implicit grabs (which are initiated by button presses)
 * can also cause #GdkEventGrabBroken events.
 */
struct _GdkEventGrabBroken {
  GdkEventAny any;
  gboolean keyboard;
  gboolean implicit;
  GdkSurface *grab_surface;
};

/*
 * GdkEventDND:
 * @type: the type of the event (%GDK_DRAG_ENTER, %GDK_DRAG_LEAVE,
 *   %GDK_DRAG_MOTION or %GDK_DROP_START)
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @drop: the #GdkDrop for the current DND operation.
 * @time: the time of the event in milliseconds.
 *
 * Generated during DND operations.
 */
struct _GdkEventDND {
  GdkEventAny any;
  GdkDrop *drop;

  guint32 time;
  double x;
  double y;
};

/*
 * GdkEventTouchpadSwipe:
 * @type: the type of the event (%GDK_TOUCHPAD_SWIPE)
 * @surface: the surface which received the event
 * @send_event: %TRUE if the event was sent explicitly
 * @phase: (type GdkTouchpadGesturePhase): the current phase of the gesture
 * @n_fingers: The number of fingers triggering the swipe
 * @time: the time of the event in milliseconds
 * @x: The X coordinate of the pointer
 * @y: The Y coordinate of the pointer
 * @dx: Movement delta in the X axis of the swipe focal point
 * @dy: Movement delta in the Y axis of the swipe focal point
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See #GdkModifierType.
 *
 * Generated during touchpad swipe gestures.
 */
struct _GdkEventTouchpadSwipe {
  GdkEventAny any;
  gint8 phase;
  gint8 n_fingers;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble dx;
  gdouble dy;
  guint state;
};

/*
 * GdkEventTouchpadPinch:
 * @type: the type of the event (%GDK_TOUCHPAD_PINCH)
 * @surface: the surface which received the event
 * @send_event: %TRUE if the event was sent explicitly
 * @phase: (type GdkTouchpadGesturePhase): the current phase of the gesture
 * @n_fingers: The number of fingers triggering the pinch
 * @time: the time of the event in milliseconds
 * @x: The X coordinate of the pointer
 * @y: The Y coordinate of the pointer
 * @dx: Movement delta in the X axis of the swipe focal point
 * @dy: Movement delta in the Y axis of the swipe focal point
 * @angle_delta: The angle change in radians, negative angles
 *   denote counter-clockwise movements
 * @scale: The current scale, relative to that at the time of
 *   the corresponding %GDK_TOUCHPAD_GESTURE_PHASE_BEGIN event
 * @state: (type GdkModifierType): a bit-mask representing the state of
 *   the modifier keys (e.g. Control, Shift and Alt) and the pointer
 *   buttons. See #GdkModifierType.
 *
 * Generated during touchpad swipe gestures.
 */
struct _GdkEventTouchpadPinch {
  GdkEventAny any;
  gint8 phase;
  gint8 n_fingers;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble dx;
  gdouble dy;
  gdouble angle_delta;
  gdouble scale;
  guint state;
};

/*
 * GdkEventPadButton:
 * @type: the type of the event (%GDK_PAD_BUTTON_PRESS or %GDK_PAD_BUTTON_RELEASE).
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @time: the time of the event in milliseconds.
 * @group: the pad group the button belongs to. A %GDK_SOURCE_TABLET_PAD device
 *   may have one or more groups containing a set of buttons/rings/strips each.
 * @button: The pad button that was pressed.
 * @mode: The current mode of @group. Different groups in a %GDK_SOURCE_TABLET_PAD
 *   device may have different current modes.
 *
 * Generated during %GDK_SOURCE_TABLET_PAD button presses and releases.
 */
struct _GdkEventPadButton {
  GdkEventAny any;
  guint32 time;
  guint group;
  guint button;
  guint mode;
};

/*
 * GdkEventPadAxis:
 * @type: the type of the event (%GDK_PAD_RING or %GDK_PAD_STRIP).
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @time: the time of the event in milliseconds.
 * @group: the pad group the ring/strip belongs to. A %GDK_SOURCE_TABLET_PAD
 *   device may have one or more groups containing a set of buttons/rings/strips
 *   each.
 * @index: number of strip/ring that was interacted. This number is 0-indexed.
 * @mode: The current mode of @group. Different groups in a %GDK_SOURCE_TABLET_PAD
 *   device may have different current modes.
 * @value: The current value for the given axis.
 *
 * Generated during %GDK_SOURCE_TABLET_PAD interaction with tactile sensors.
 */
struct _GdkEventPadAxis {
  GdkEventAny any;
  guint32 time;
  guint group;
  guint index;
  guint mode;
  gdouble value;
};

/*
 * GdkEventPadGroupMode:
 * @type: the type of the event (%GDK_PAD_GROUP_MODE).
 * @surface: the surface which received the event.
 * @send_event: %TRUE if the event was sent explicitly.
 * @time: the time of the event in milliseconds.
 * @group: the pad group that is switching mode. A %GDK_SOURCE_TABLET_PAD
 *   device may have one or more groups containing a set of buttons/rings/strips
 *   each.
 * @mode: The new mode of @group. Different groups in a %GDK_SOURCE_TABLET_PAD
 *   device may have different current modes.
 *
 * Generated during %GDK_SOURCE_TABLET_PAD mode switches in a group.
 */
struct _GdkEventPadGroupMode {
  GdkEventAny any;
  guint32 time;
  guint group;
  guint mode;
};

/*
 * GdkEvent:
 * @type: the #GdkEventType
 * @any: a #GdkEventAny
 * @motion: a #GdkEventMotion
 * @button: a #GdkEventButton
 * @touch: a #GdkEventTouch
 * @scroll: a #GdkEventScroll
 * @key: a #GdkEventKey
 * @crossing: a #GdkEventCrossing
 * @focus_change: a #GdkEventFocus
 * @configure: a #GdkEventConfigure
 * @proximity: a #GdkEventProximity
 * @dnd: a #GdkEventDND
 * @grab_broken: a #GdkEventGrabBroken
 * @touchpad_swipe: a #GdkEventTouchpadSwipe
 * @touchpad_pinch: a #GdkEventTouchpadPinch
 * @pad_button: a #GdkEventPadButton
 * @pad_axis: a #GdkEventPadAxis
 * @pad_group_mode: a #GdkEventPadGroupMode
 *
 * A #GdkEvent contains a union of all of the event types,
 * and allows access to the data fields in a number of ways.
 *
 * The event type is always the first field in all of the event types, and
 * can always be accessed with the following code, no matter what type of
 * event it is:
 * |[<!-- language="C" -->
 *   GdkEvent *event;
 *   GdkEventType type;
 *
 *   type = event->type;
 * ]|
 *
 * To access other fields of the event, the pointer to the event
 * can be cast to the appropriate event type, or the union member
 * name can be used. For example if the event type is %GDK_BUTTON_PRESS
 * then the x coordinate of the button press can be accessed with:
 * |[<!-- language="C" -->
 *   GdkEvent *event;
 *   gdouble x;
 *
 *   x = ((GdkEventButton*)event)->x;
 * ]|
 * or:
 * |[<!-- language="C" -->
 *   GdkEvent *event;
 *   gdouble x;
 *
 *   x = event->button.x;
 * ]|
 */
union _GdkEvent
{
  GdkEventAny		    any;
  GdkEventMotion	    motion;
  GdkEventButton	    button;
  GdkEventTouch             touch;
  GdkEventScroll            scroll;
  GdkEventKey		    key;
  GdkEventCrossing	    crossing;
  GdkEventFocus		    focus_change;
  GdkEventConfigure	    configure;
  GdkEventProximity	    proximity;
  GdkEventDND               dnd;
  GdkEventGrabBroken        grab_broken;
  GdkEventTouchpadSwipe     touchpad_swipe;
  GdkEventTouchpadPinch     touchpad_pinch;
  GdkEventPadButton         pad_button;
  GdkEventPadAxis           pad_axis;
  GdkEventPadGroupMode      pad_group_mode;
};

void           gdk_event_set_target              (GdkEvent *event,
                                                  GObject  *user_data);
GObject *      gdk_event_get_target              (const GdkEvent *event);
void           gdk_event_set_related_target       (GdkEvent *event,
                                                  GObject  *user_data);
GObject *      gdk_event_get_related_target      (const GdkEvent *event);

gboolean       check_event_sanity (GdkEvent *event);

GdkEvent * gdk_event_button_new         (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         GdkDeviceTool   *tool,
                                         guint32          time,
                                         GdkModifierType  state,
                                         guint            button,
                                         double           x,
                                         double           y,
                                         double          *axes);
                             
GdkEvent * gdk_event_motion_new         (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         GdkDeviceTool   *tool,
                                         guint32          time,
                                         GdkModifierType  state,
                                         double           x,
                                         double           y,
                                         double          *axes);

GdkEvent * gdk_event_crossing_new       (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         guint32          time,
                                         GdkModifierType  state,
                                         double           x,
                                         double           y,
                                         GdkCrossingMode  mode,
                                         GdkNotifyType    notify);
                                          
GdkEvent * gdk_event_proximity_new      (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         GdkDeviceTool   *tool,
                                         guint32          time);

GdkEvent * gdk_event_key_new            (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         guint32          time,
                                         GdkModifierType  state,
                                         guint            keyval,
                                         guint16          keycode,
                                         guint16          scancode,
                                         guint8           group,
                                         gboolean         is_modifier);

GdkEvent * gdk_event_focus_new          (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         gboolean         focus_in);

GdkEvent * gdk_event_configure_new      (GdkSurface      *surface,
                                         int              width,
                                         int              height);

GdkEvent * gdk_event_delete_new         (GdkSurface      *surface);

GdkEvent * gdk_event_scroll_new         (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         GdkDeviceTool   *tool,
                                         guint32          time,
                                         GdkModifierType  state,
                                         double           delta_x,
                                         double           delta_y,
                                         gboolean         is_stop);

GdkEvent * gdk_event_discrete_scroll_new (GdkSurface         *surface,
                                          GdkDevice          *device,
                                          GdkDevice          *source_device,
                                          GdkDeviceTool      *tool,
                                          guint32             time,
                                          GdkModifierType     state,
                                          GdkScrollDirection  direction,
                                          gboolean            emulated);

GdkEvent * gdk_event_touch_new          (GdkEventType      type,
                                         GdkEventSequence *sequence,
                                         GdkSurface       *surface,
                                         GdkDevice        *device,
                                         GdkDevice        *source_device,
                                         guint32           time,
                                         GdkModifierType   state,
                                         double            x,
                                         double            y,
                                         double           *axes,
                                         gboolean          emulating);
 
GdkEvent * gdk_event_touchpad_swipe_new (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         guint32          time,
                                         GdkModifierType  state,
                                         double           x,
                                         double           y,
                                         int              n_fingers,
                                         double           dx,
                                         double           dy);

GdkEvent * gdk_event_touchpad_pinch_new (GdkSurface              *surface,
                                         GdkDevice               *device,
                                         GdkDevice               *source_device,
                                         guint32                  time,
                                         GdkModifierType          state,
                                         GdkTouchpadGesturePhase  phase,
                                         double                   x,
                                         double                   y,
                                         int                      n_fingers,
                                         double                   dx,
                                         double                   dy,
                                         double                   scale,
                                         double                   angle_delta);

GdkEvent * gdk_event_pad_ring_new       (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         guint32          time,
                                         guint            group,
                                         guint            index,
                                         guint            mode,
                                         double           value);

GdkEvent * gdk_event_pad_strip_new      (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         guint32          time,
                                         guint            group,
                                         guint            index,
                                         guint            mode,
                                         double           value);

GdkEvent * gdk_event_pad_button_new     (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         guint32          time,
                                         guint            group,
                                         guint            button,
                                         guint            mode);

GdkEvent * gdk_event_pad_group_mode_new (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         guint32          time,
                                         guint            group,
                                         guint            mode);

GdkEvent * gdk_event_drag_new           (GdkEventType     type,
                                         GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDrop         *drop,
                                         guint32          time,
                                         double           x,
                                         double           y);

GdkEvent * gdk_event_grab_broken_new    (GdkSurface      *surface,
                                         GdkDevice       *device,
                                         GdkDevice       *source_device,
                                         GdkSurface      *grab_surface,
                                         gboolean         implicit);


#endif /* __GDK_EVENTS_PRIVATE_H__ */

