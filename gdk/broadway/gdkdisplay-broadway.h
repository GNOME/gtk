/*
 * gdkdisplay-broadway.h
 * 
 * Copyright 2001 Sun Microsystems Inc. 
 *
 * Erwann Chenede <erwann.chenede@sun.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_BROADWAY_DISPLAY__
#define __GDK_BROADWAY_DISPLAY__

#include "gdkdisplayprivate.h"
#include "gdkkeys.h"
#include "gdkwindow.h"
#include "gdkinternals.h"
#include "gdkmain.h"
#include "broadway.h"

G_BEGIN_DECLS

typedef struct _GdkBroadwayDisplay GdkBroadwayDisplay;
typedef struct _GdkBroadwayDisplayClass GdkBroadwayDisplayClass;

typedef struct BroadwayInput BroadwayInput;

#define GDK_TYPE_BROADWAY_DISPLAY              (gdk_broadway_display_get_type())
#define GDK_BROADWAY_DISPLAY(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), GDK_TYPE_BROADWAY_DISPLAY, GdkBroadwayDisplay))
#define GDK_BROADWAY_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_BROADWAY_DISPLAY, GdkBroadwayDisplayClass))
#define GDK_IS_BROADWAY_DISPLAY(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), GDK_TYPE_BROADWAY_DISPLAY))
#define GDK_IS_BROADWAY_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_BROADWAY_DISPLAY))
#define GDK_BROADWAY_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_BROADWAY_DISPLAY, GdkBroadwayDisplayClass))

typedef struct {
  char type;
  guint32 serial;
  guint64 time;
} BroadwayInputBaseMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  guint32 mouse_window_id; /* The real window, not taking grabs into account */
  guint32 event_window_id;
  int root_x;
  int root_y;
  int win_x;
  int win_y;
  guint32 state;
} BroadwayInputPointerMsg;

typedef struct {
  BroadwayInputPointerMsg pointer;
  guint32 mode;
} BroadwayInputCrossingMsg;

typedef struct {
  BroadwayInputPointerMsg pointer;
  guint32 button;
} BroadwayInputButtonMsg;

typedef struct {
  BroadwayInputPointerMsg pointer;
  int dir;
} BroadwayInputScrollMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  guint32 state;
  int key;
} BroadwayInputKeyMsg;

typedef struct {
  BroadwayInputBaseMsg base;
  int res;
} BroadwayInputGrabReply;

typedef struct {
  BroadwayInputBaseMsg base;
  int id;
  int x;
  int y;
  int width;
  int height;
} BroadwayInputConfigureNotify;

typedef struct {
  BroadwayInputBaseMsg base;
  int width;
  int height;
} BroadwayInputScreenResizeNotify;

typedef struct {
  BroadwayInputBaseMsg base;
  int id;
} BroadwayInputDeleteNotify;

typedef union {
  BroadwayInputBaseMsg base;
  BroadwayInputPointerMsg pointer;
  BroadwayInputCrossingMsg crossing;
  BroadwayInputButtonMsg button;
  BroadwayInputScrollMsg scroll;
  BroadwayInputKeyMsg key;
  BroadwayInputGrabReply grab_reply;
  BroadwayInputConfigureNotify configure_notify;
  BroadwayInputDeleteNotify delete_notify;
  BroadwayInputScreenResizeNotify screen_resize_notify;
} BroadwayInputMsg;

struct _GdkBroadwayDisplay
{
  GdkDisplay parent_instance;
  GdkScreen *default_screen;
  GdkScreen **screens;

  GHashTable *id_ht;
  GList *toplevels;

  GSource *event_source;
  GdkWindow *mouse_in_toplevel;
  int last_x, last_y; /* in root coords */
  guint32 last_state;
  GdkWindow *real_mouse_in_toplevel; /* Not affected by grabs */

  /* Keyboard related information */
  GdkKeymap *keymap;

  /* drag and drop information */
  GdkDragContext *current_dest_drag;

  /* Input device */
  /* input GdkDevice list */
  GList *input_devices;

  /* The offscreen window that has the pointer in it (if any) */
  GdkWindow *active_offscreen_window;

  GSocketService *service;
  BroadwayOutput *output;
  guint32 saved_serial;
  guint64 last_seen_time;
  BroadwayInput *input;
  GList *input_messages;
  guint process_input_idle;

  /* Explicit pointer grabs: */
  GdkWindow *pointer_grab_window;
  guint32 pointer_grab_time;
  gboolean pointer_grab_owner_events;

  /* Future data, from the currently queued events */
  int future_root_x;
  int future_root_y;
  GdkModifierType future_state;
  int future_mouse_in_toplevel;
};

struct _GdkBroadwayDisplayClass
{
  GdkDisplayClass parent_class;
};

GType      gdk_broadway_display_get_type            (void);

G_END_DECLS

#endif				/* __GDK_BROADWAY_DISPLAY__ */
