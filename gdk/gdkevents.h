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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#if !defined (__GDK_H_INSIDE__) && !defined (GDK_COMPILATION)
#error "Only <gdk/gdk.h> can be included directly."
#endif

#ifndef __GDK_EVENTS_H__
#define __GDK_EVENTS_H__

#include <gdk/gdkcolor.h>
#include <gdk/gdktypes.h>
#include <gdk/gdkdnd.h>
#include <gdk/gdkdevice.h>

G_BEGIN_DECLS

#define GDK_TYPE_EVENT          (gdk_event_get_type ())

/**
 * GDK_PRIORITY_EVENTS:
 *
 * This is the priority that events from the X server are given in the
 * <link linkend="glib-The-Main-Event-Loop">GLib Main Loop</link>.
 */
#define GDK_PRIORITY_EVENTS	(G_PRIORITY_DEFAULT)

/**
 * GDK_PRIORITY_REDRAW:
 *
 * This is the priority that the idle handler processing window updates
 * is given in the
 * <link linkend="glib-The-Main-Event-Loop">GLib Main Loop</link>.
 */
#define GDK_PRIORITY_REDRAW     (G_PRIORITY_HIGH_IDLE + 20)


typedef struct _GdkEventAny	    GdkEventAny;
typedef struct _GdkEventExpose	    GdkEventExpose;
typedef struct _GdkEventNoExpose    GdkEventNoExpose;
typedef struct _GdkEventVisibility  GdkEventVisibility;
typedef struct _GdkEventMotion	    GdkEventMotion;
typedef struct _GdkEventButton	    GdkEventButton;
typedef struct _GdkEventScroll      GdkEventScroll;  
typedef struct _GdkEventKey	    GdkEventKey;
typedef struct _GdkEventFocus	    GdkEventFocus;
typedef struct _GdkEventCrossing    GdkEventCrossing;
typedef struct _GdkEventConfigure   GdkEventConfigure;
typedef struct _GdkEventProperty    GdkEventProperty;
typedef struct _GdkEventSelection   GdkEventSelection;
typedef struct _GdkEventOwnerChange GdkEventOwnerChange;
typedef struct _GdkEventProximity   GdkEventProximity;
typedef struct _GdkEventClient	    GdkEventClient;
typedef struct _GdkEventDND         GdkEventDND;
typedef struct _GdkEventWindowState GdkEventWindowState;
typedef struct _GdkEventSetting     GdkEventSetting;
typedef struct _GdkEventGrabBroken  GdkEventGrabBroken;

typedef union  _GdkEvent	    GdkEvent;

/**
 * GdkEventFunc:
 * @event: the #GdkEvent to process.
 * @data: user data set when the event handler was installed with
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
 * Used to represent native events (<type>XEvent</type>s for the X11
 * backend, <type>MSG</type>s for Win32).
 */
typedef void GdkXEvent;	  /* Can be cast to window system specific
			   * even type, XEvent on X11, MSG on Win32.
			   */

/**
 * GdkFilterReturn:
 * @GDK_FILTER_CONTINUE: event not handled, continue processing.
 * @GDK_FILTER_TRANSLATE: native event translated into a GDK event and stored
 *  in the <literal>event</literal> structure that was passed in.
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
 * @data: user data set when the filter was installed.
 *
 * Specifies the type of function used to filter native events before they are
 * converted to GDK events.
 *
 * When a filter is called, @event is unpopulated, except for
 * <literal>event->window</literal>. The filter may translate the native
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
 * @GDK_3BUTTON_PRESS: a mouse button has been clicked 3 times in a short period
 *   of time. Note that each click also generates a %GDK_BUTTON_PRESS event.
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
 * @GDK_NO_EXPOSE: indicates that the source region was completely available
 *   when parts of a drawable were copied. This is not very useful.
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
 * @GDK_EVENT_LAST: marks the end of the GdkEventType enumeration. Added in 2.18
 *
 * Specifies the type of the event.
 *
 * Do not confuse these events with the signals that GTK+ widgets emit.
 * Although many of these events result in corresponding signals being emitted,
 * the events are often transformed or filtered along the way.
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
  GDK_3BUTTON_PRESS	= 6,
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
  GDK_NO_EXPOSE		= 30,
  GDK_SCROLL            = 31,
  GDK_WINDOW_STATE      = 32,
  GDK_SETTING           = 33,
  GDK_OWNER_CHANGE      = 34,
  GDK_GRAB_BROKEN       = 35,
  GDK_DAMAGE            = 36,
  GDK_EVENT_LAST        /* helper variable for decls */
} GdkEventType;

typedef enum
{
  GDK_VISIBILITY_UNOBSCURED,
  GDK_VISIBILITY_PARTIAL,
  GDK_VISIBILITY_FULLY_OBSCURED
} GdkVisibilityState;

typedef enum
{
  GDK_SCROLL_UP,
  GDK_SCROLL_DOWN,
  GDK_SCROLL_LEFT,
  GDK_SCROLL_RIGHT
} GdkScrollDirection;

/* Types of enter/leave notifications.
 *   Ancestor:
 *   Virtual:
 *   Inferior:
 *   Nonlinear:
 *   NonlinearVirtual:
 *   Unknown: An unknown type of enter/leave event occurred.
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

/* Enter/leave event modes.
 *   NotifyNormal
 *   NotifyGrab
 *   NotifyUngrab
 */
typedef enum
{
  GDK_CROSSING_NORMAL,
  GDK_CROSSING_GRAB,
  GDK_CROSSING_UNGRAB,
  GDK_CROSSING_GTK_GRAB,
  GDK_CROSSING_GTK_UNGRAB,
  GDK_CROSSING_STATE_CHANGED
} GdkCrossingMode;

typedef enum
{
  GDK_PROPERTY_NEW_VALUE,
  GDK_PROPERTY_DELETE
} GdkPropertyState;

typedef enum
{
  GDK_WINDOW_STATE_WITHDRAWN  = 1 << 0,
  GDK_WINDOW_STATE_ICONIFIED  = 1 << 1,
  GDK_WINDOW_STATE_MAXIMIZED  = 1 << 2,
  GDK_WINDOW_STATE_STICKY     = 1 << 3,
  GDK_WINDOW_STATE_FULLSCREEN = 1 << 4,
  GDK_WINDOW_STATE_ABOVE      = 1 << 5,
  GDK_WINDOW_STATE_BELOW      = 1 << 6
} GdkWindowState;

typedef enum
{
  GDK_SETTING_ACTION_NEW,
  GDK_SETTING_ACTION_CHANGED,
  GDK_SETTING_ACTION_DELETED
} GdkSettingAction;

typedef enum
{
  GDK_OWNER_CHANGE_NEW_OWNER,
  GDK_OWNER_CHANGE_DESTROY,
  GDK_OWNER_CHANGE_CLOSE
} GdkOwnerChange;

struct _GdkEventAny
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
};

struct _GdkEventExpose
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkRectangle area;
  cairo_region_t *region;
  gint count; /* If non-zero, how many more events follow. */
};

struct _GdkEventNoExpose
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
};

struct _GdkEventVisibility
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkVisibilityState state;
};

struct _GdkEventMotion
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble *axes;
  guint state;
  gint16 is_hint;
  GdkDevice *device;
  gdouble x_root, y_root;
};

struct _GdkEventButton
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble *axes;
  guint state;
  guint button;
  GdkDevice *device;
  gdouble x_root, y_root;
};

struct _GdkEventScroll
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 time;
  gdouble x;
  gdouble y;
  guint state;
  GdkScrollDirection direction;
  GdkDevice *device;
  gdouble x_root, y_root;
};

struct _GdkEventKey
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 time;
  guint state;
  guint keyval;
  gint length;
  gchar *string;
  guint16 hardware_keycode;
  guint8 group;
  guint is_modifier : 1;
};

struct _GdkEventCrossing
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkWindow *subwindow;
  guint32 time;
  gdouble x;
  gdouble y;
  gdouble x_root;
  gdouble y_root;
  GdkCrossingMode mode;
  GdkNotifyType detail;
  gboolean focus;
  guint state;
};

struct _GdkEventFocus
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  gint16 in;
};

struct _GdkEventConfigure
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  gint x, y;
  gint width;
  gint height;
};

struct _GdkEventProperty
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkAtom atom;
  guint32 time;
  guint state;
};

struct _GdkEventSelection
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkAtom selection;
  GdkAtom target;
  GdkAtom property;
  guint32 time;
  GdkNativeWindow requestor;
};

struct _GdkEventOwnerChange
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkNativeWindow owner;
  GdkOwnerChange reason;
  GdkAtom selection;
  guint32 time;
  guint32 selection_time;
};

/* This event type will be used pretty rarely. It only is important
   for XInput aware programs that are drawing their own cursor */

struct _GdkEventProximity
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  guint32 time;
  GdkDevice *device;
};

struct _GdkEventClient
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkAtom message_type;
  gushort data_format;
  union {
    char b[20];
    short s[10];
    long l[5];
  } data;
};

struct _GdkEventSetting
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkSettingAction action;
  char *name;
};

struct _GdkEventWindowState
{
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkWindowState changed_mask;
  GdkWindowState new_window_state;
};

struct _GdkEventGrabBroken {
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  gboolean keyboard;
  gboolean implicit;
  GdkWindow *grab_window;
};

/* Event types for DND */

struct _GdkEventDND {
  GdkEventType type;
  GdkWindow *window;
  gint8 send_event;
  GdkDragContext *context;

  guint32 time;
  gshort x_root, y_root;
};

union _GdkEvent
{
  GdkEventType		    type;
  GdkEventAny		    any;
  GdkEventExpose	    expose;
  GdkEventNoExpose	    no_expose;
  GdkEventVisibility	    visibility;
  GdkEventMotion	    motion;
  GdkEventButton	    button;
  GdkEventScroll            scroll;
  GdkEventKey		    key;
  GdkEventCrossing	    crossing;
  GdkEventFocus		    focus_change;
  GdkEventConfigure	    configure;
  GdkEventProperty	    property;
  GdkEventSelection	    selection;
  GdkEventOwnerChange  	    owner_change;
  GdkEventProximity	    proximity;
  GdkEventClient	    client;
  GdkEventDND               dnd;
  GdkEventWindowState       window_state;
  GdkEventSetting           setting;
  GdkEventGrabBroken        grab_broken;
};

GType     gdk_event_get_type            (void) G_GNUC_CONST;

gboolean  gdk_events_pending	 	(void);
GdkEvent* gdk_event_get			(void);

GdkEvent* gdk_event_peek                (void);
void      gdk_event_put	 		(const GdkEvent *event);

GdkEvent* gdk_event_new                 (GdkEventType    type);
GdkEvent* gdk_event_copy     		(const GdkEvent *event);
void	  gdk_event_free     		(GdkEvent 	*event);

guint32   gdk_event_get_time            (const GdkEvent  *event);
gboolean  gdk_event_get_state           (const GdkEvent  *event,
                                         GdkModifierType *state);
gboolean  gdk_event_get_coords		(const GdkEvent  *event,
					 gdouble	 *x_win,
					 gdouble	 *y_win);
gboolean  gdk_event_get_root_coords	(const GdkEvent  *event,
					 gdouble	 *x_root,
					 gdouble	 *y_root);
gboolean  gdk_event_get_axis            (const GdkEvent  *event,
                                         GdkAxisUse       axis_use,
                                         gdouble         *value);
void       gdk_event_set_device         (GdkEvent        *event,
                                         GdkDevice       *device);
GdkDevice* gdk_event_get_device         (const GdkEvent  *event);
void      gdk_event_request_motions     (const GdkEventMotion *event);

gboolean  gdk_events_get_distance       (GdkEvent        *event1,
                                         GdkEvent        *event2,
                                         gdouble         *distance);
gboolean  gdk_events_get_angle          (GdkEvent        *event1,
                                         GdkEvent        *event2,
                                         gdouble         *angle);
gboolean  gdk_events_get_center         (GdkEvent        *event1,
                                         GdkEvent        *event2,
                                         gdouble         *x,
                                         gdouble         *y);

void	  gdk_event_handler_set 	(GdkEventFunc    func,
					 gpointer        data,
					 GDestroyNotify  notify);

void       gdk_event_set_screen         (GdkEvent        *event,
                                         GdkScreen       *screen);
GdkScreen *gdk_event_get_screen         (const GdkEvent  *event);

void	  gdk_set_show_events		(gboolean	 show_events);
gboolean  gdk_get_show_events		(void);

#ifndef GDK_MULTIHEAD_SAFE

gboolean gdk_setting_get                           (const gchar *name,
                                                    GValue          *value);
void gdk_add_client_message_filter                 (GdkAtom          message_type,
                                                    GdkFilterFunc    func,
                                                    gpointer         data);
gboolean gdk_event_send_client_message             (GdkEvent        *event,
                                                    GdkNativeWindow  winid);
void     gdk_event_send_clientmessage_toall        (GdkEvent        *event);

#endif /* GDK_MULTIHEAD_SAFE */

gboolean gdk_event_send_client_message_for_display (GdkDisplay      *display,
                                                    GdkEvent        *event,
                                                    GdkNativeWindow  winid);

G_END_DECLS

#endif /* __GDK_EVENTS_H__ */
