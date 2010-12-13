/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2010 Red Hat, Inc.
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

#ifndef __GDK_DISPLAY_PRIVATE_H__
#define __GDK_DISPLAY_PRIVATE_H__

#include "gdkdisplay.h"
#include "gdkcursor.h"

G_BEGIN_DECLS

typedef struct _GdkDisplayClass GdkDisplayClass;

/* Tracks information about the keyboard grab on this display */
typedef struct
{
  GdkWindow *window;
  GdkWindow *native_window;
  gulong serial;
  gboolean owner_events;
  guint32 time;
} GdkKeyboardGrabInfo;

/* Tracks information about the pointer grab on this display */
typedef struct
{
  GdkWindow *window;
  GdkWindow *native_window;
  gulong serial_start;
  gulong serial_end; /* exclusive, i.e. not active on serial_end */
  gboolean owner_events;
  guint event_mask;
  gboolean implicit;
  guint32 time;
  GdkGrabOwnership ownership;

  guint activated : 1;
  guint implicit_ungrab : 1;
} GdkDeviceGrabInfo;

/* Tracks information about which window and position the pointer last was in.
 * This is useful when we need to synthesize events later.
 * Note that we track toplevel_under_pointer using enter/leave events,
 * so in the case of a grab, either with owner_events==FALSE or with the
 * pointer in no clients window the x/y coordinates may actually be outside
 * the window.
 */
typedef struct
{
  GdkWindow *toplevel_under_pointer; /* toplevel window containing the pointer, */
                                     /* tracked via native events */
  GdkWindow *window_under_pointer;   /* window that last got a normal enter event */
  gdouble toplevel_x, toplevel_y;
  guint32 state;
  guint32 button;
} GdkPointerWindowInfo;

typedef struct
{
  guint32 button_click_time[2]; /* last 2 button click times */
  GdkWindow *button_window[2];  /* last 2 windows to receive button presses */
  gint button_number[2];        /* last 2 buttons to be pressed */
  gint button_x[2];             /* last 2 button click positions */
  gint button_y[2];
} GdkMultipleClickInfo;

struct _GdkDisplay
{
  GObject parent_instance;

  GList *queued_events;
  GList *queued_tail;

  /* Information for determining if the latest button click
   * is part of a double-click or triple-click
   */
  GHashTable *multiple_click_info;
  guint double_click_time;  /* Maximum time between clicks in msecs */
  GdkDevice *core_pointer;  /* Core pointer device */

  const GdkDisplayDeviceHooks *device_hooks; /* Hooks for querying pointer */

  guint closed             : 1;  /* Whether this display has been closed */
  guint ignore_core_events : 1;  /* Don't send core motion and button event */

  guint double_click_distance;   /* Maximum distance between clicks in pixels */

  GHashTable *device_grabs;
  GHashTable *motion_hint_info;

  GHashTable *pointers_info;  /* GdkPointerWindowInfo for each device */
  guint32 last_event_time;    /* Last reported event time from server */

  GdkDeviceManager *device_manager;
};

struct _GdkDisplayClass
{
  GObjectClass parent_class;

  G_CONST_RETURN gchar *     (*get_name)           (GdkDisplay *display);
  gint                       (*get_n_screens)      (GdkDisplay *display);
  GdkScreen *                (*get_screen)         (GdkDisplay *display,
                                                    gint        screen_num);
  GdkScreen *                (*get_default_screen) (GdkDisplay *display);
  void                       (*beep)               (GdkDisplay *display);
  void                       (*sync)               (GdkDisplay *display);
  void                       (*flush)              (GdkDisplay *display);
  gboolean                   (*has_pending)        (GdkDisplay *display);
  void                       (*queue_events)       (GdkDisplay *display);
  GdkWindow *                (*get_default_group)  (GdkDisplay *display);
  gboolean                   (*supports_selection_notification) (GdkDisplay *display);
  gboolean                   (*request_selection_notification)  (GdkDisplay *display,
                                                                 GdkAtom     selection);
  gboolean                   (*supports_shapes)       (GdkDisplay *display);
  gboolean                   (*supports_input_shapes) (GdkDisplay *display);
  gboolean                   (*supports_composite)    (GdkDisplay *display);
  gboolean                   (*supports_cursor_alpha) (GdkDisplay *display);
  gboolean                   (*supports_cursor_color) (GdkDisplay *display);

  gboolean                   (*supports_clipboard_persistence)  (GdkDisplay *display);
  void                       (*store_clipboard)    (GdkDisplay    *display,
                                                    GdkWindow     *clipboard_window,
                                                    guint32        time_,
                                                    const GdkAtom *targets,
                                                    gint           n_targets);

  void                       (*get_default_cursor_size) (GdkDisplay *display,
                                                         guint      *width,
                                                         guint      *height);
  void                       (*get_maximal_cursor_size) (GdkDisplay *display,
                                                         guint      *width,
                                                         guint      *height);
  GdkCursor *                (*get_cursor_for_type)     (GdkDisplay    *display,
                                                         GdkCursorType  type);
  GdkCursor *                (*get_cursor_for_name)     (GdkDisplay    *display,
                                                         const gchar   *name);
  GdkCursor *                (*get_cursor_for_pixbuf)   (GdkDisplay    *display,
                                                         GdkPixbuf     *pixbuf,
                                                         gint           x,
                                                         gint           y);

  GList *                    (*list_devices)       (GdkDisplay *display);
  gboolean                   (*send_client_message) (GdkDisplay     *display,
                                                     GdkEvent       *event,
                                                     GdkNativeWindow winid);
  void                       (*add_client_message_filter) (GdkDisplay   *display,
                                                           GdkAtom       message_type,
                                                           GdkFilterFunc func,
                                                           gpointer      data);
  GdkAppLaunchContext *      (*get_app_launch_context) (GdkDisplay *display);
  GdkNativeWindow            (*get_drag_protocol)      (GdkDisplay      *display,
                                                        GdkNativeWindow  winid,
                                                        GdkDragProtocol *protocol,
                                                        guint           *version);

  /* Signals */
  void (*closed) (GdkDisplay *display,
                  gboolean    is_error);
};


typedef void (* GdkDisplayPointerInfoForeach) (GdkDisplay           *display,
                                               GdkDevice            *device,
                                               GdkPointerWindowInfo *device_info,
                                               gpointer              user_data);

void                _gdk_display_device_grab_update   (GdkDisplay *display,
                                                       GdkDevice  *device,
                                                       GdkDevice  *source_device,
                                                       gulong      current_serial);
GdkDeviceGrabInfo * _gdk_display_get_last_device_grab (GdkDisplay *display,
                                                       GdkDevice  *device);
GdkDeviceGrabInfo * _gdk_display_add_device_grab      (GdkDisplay       *display,
                                                       GdkDevice        *device,
                                                       GdkWindow        *window,
                                                       GdkWindow        *native_window,
                                                       GdkGrabOwnership  grab_ownership,
                                                       gboolean          owner_events,
                                                       GdkEventMask      event_mask,
                                                       gulong            serial_start,
                                                       guint32           time,
                                                       gboolean          implicit);
GdkDeviceGrabInfo * _gdk_display_has_device_grab      (GdkDisplay       *display,
                                                       GdkDevice        *device,
                                                       gulong            serial);
gboolean            _gdk_display_end_device_grab      (GdkDisplay       *display,
                                                       GdkDevice        *device,
                                                       gulong            serial,
                                                       GdkWindow        *if_child,
                                                       gboolean          implicit);
gboolean            _gdk_display_check_grab_ownership (GdkDisplay       *display,
                                                       GdkDevice        *device,
                                                       gulong            serial);
void                _gdk_display_enable_motion_hints  (GdkDisplay       *display,
                                                       GdkDevice        *device);
GdkPointerWindowInfo * _gdk_display_get_pointer_info  (GdkDisplay       *display,
                                                       GdkDevice        *device);
void                _gdk_display_pointer_info_foreach (GdkDisplay       *display,
                                                       GdkDisplayPointerInfoForeach func,
                                                       gpointer          user_data);


G_END_DECLS

#endif  /* __GDK_DISPLAY_PRIVATE_H__ */
