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
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GDK_DISPLAY_PRIVATE_H__
#define __GDK_DISPLAY_PRIVATE_H__

#include "gdkdisplay.h"
#include "gdkwindow.h"
#include "gdkcursor.h"
#include "gdkinternals.h"

G_BEGIN_DECLS

#define GDK_DISPLAY_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_DISPLAY, GdkDisplayClass))
#define GDK_IS_DISPLAY_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GDK_TYPE_DISPLAY))
#define GDK_DISPLAY_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_DISPLAY, GdkDisplayClass))


typedef struct _GdkDisplayClass GdkDisplayClass;

/* Tracks information about the device grab on this display */
typedef struct
{
  GdkWindow *window;
  GdkWindow *native_window;
  gulong serial_start;
  gulong serial_end; /* exclusive, i.e. not active on serial_end */
  guint event_mask;
  guint32 time;
  GdkGrabOwnership ownership;

  guint activated : 1;
  guint implicit_ungrab : 1;
  guint owner_events : 1;
  guint implicit : 1;
} GdkDeviceGrabInfo;

/* Tracks information about a touch implicit grab on this display */
typedef struct
{
  GdkDevice *device;
  GdkEventSequence *sequence;

  GdkWindow *window;
  GdkWindow *native_window;
  gulong serial;
  guint event_mask;
  guint32 time;
} GdkTouchGrabInfo;

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
  GdkDevice *last_slave;
  guint need_touch_press_enter : 1;
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
  GdkDevice *core_pointer;  /* Core pointer device */

  guint event_pause_count;       /* How many times events are blocked */

  guint closed             : 1;  /* Whether this display has been closed */

  GArray *touch_implicit_grabs;
  GHashTable *device_grabs;
  GHashTable *motion_hint_info;
  GdkDeviceManager *device_manager;

  GHashTable *pointers_info;  /* GdkPointerWindowInfo for each device */
  guint32 last_event_time;    /* Last reported event time from server */

  guint double_click_time;  /* Maximum time between clicks in msecs */
  guint double_click_distance;   /* Maximum distance between clicks in pixels */

  guint has_gl_extension_texture_non_power_of_two : 1;
  guint has_gl_extension_texture_rectangle : 1;

  guint debug_updates     : 1;
  guint debug_updates_set : 1;

  GdkRenderingMode rendering_mode;
};

struct _GdkDisplayClass
{
  GObjectClass parent_class;

  GType window_type;          /* type for native windows for this display, set in class_init */

  const gchar *              (*get_name)           (GdkDisplay *display);
  GdkScreen *                (*get_default_screen) (GdkDisplay *display);
  void                       (*beep)               (GdkDisplay *display);
  void                       (*sync)               (GdkDisplay *display);
  void                       (*flush)              (GdkDisplay *display);
  gboolean                   (*has_pending)        (GdkDisplay *display);
  void                       (*queue_events)       (GdkDisplay *display);
  void                       (*make_default)       (GdkDisplay *display);
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
  GdkCursor *                (*get_cursor_for_surface)  (GdkDisplay    *display,
                                                         cairo_surface_t *surface,
                                                         gdouble          x,
                                                         gdouble          y);

  GList *                    (*list_devices)       (GdkDisplay *display);
  GdkAppLaunchContext *      (*get_app_launch_context) (GdkDisplay *display);

  void                       (*before_process_all_updates) (GdkDisplay *display);
  void                       (*after_process_all_updates)  (GdkDisplay *display);

  gulong                     (*get_next_serial) (GdkDisplay *display);

  void                       (*notify_startup_complete) (GdkDisplay  *display,
                                                         const gchar *startup_id);
  void                       (*event_data_copy) (GdkDisplay     *display,
                                                 const GdkEvent *event,
                                                 GdkEvent       *new_event);
  void                       (*event_data_free) (GdkDisplay     *display,
                                                 GdkEvent       *event);
  void                       (*create_window_impl) (GdkDisplay    *display,
                                                    GdkWindow     *window,
                                                    GdkWindow     *real_parent,
                                                    GdkScreen     *screen,
                                                    GdkEventMask   event_mask,
                                                    GdkWindowAttr *attributes,
                                                    gint           attributes_mask);

  GdkKeymap *                (*get_keymap)         (GdkDisplay    *display);
  void                       (*push_error_trap)    (GdkDisplay    *display);
  gint                       (*pop_error_trap)     (GdkDisplay    *display,
                                                    gboolean       ignore);

  GdkWindow *                (*get_selection_owner) (GdkDisplay   *display,
                                                     GdkAtom       selection);
  gboolean                   (*set_selection_owner) (GdkDisplay   *display,
                                                     GdkWindow    *owner,
                                                     GdkAtom       selection,
                                                     guint32       time,
                                                     gboolean      send_event);
  void                       (*send_selection_notify) (GdkDisplay *dispay,
                                                       GdkWindow        *requestor,
                                                       GdkAtom          selection,
                                                       GdkAtom          target,
                                                       GdkAtom          property,
                                                       guint32          time);
  gint                       (*get_selection_property) (GdkDisplay  *display,
                                                        GdkWindow   *requestor,
                                                        guchar     **data,
                                                        GdkAtom     *type,
                                                        gint        *format);
  void                       (*convert_selection)      (GdkDisplay  *display,
                                                        GdkWindow   *requestor,
                                                        GdkAtom      selection,
                                                        GdkAtom      target,
                                                        guint32      time);

  gint                   (*text_property_to_utf8_list) (GdkDisplay     *display,
                                                        GdkAtom         encoding,
                                                        gint            format,
                                                        const guchar   *text,
                                                        gint            length,
                                                        gchar        ***list);
  gchar *                (*utf8_to_string_target)      (GdkDisplay     *display,
                                                        const gchar    *text);

  gboolean               (*make_gl_context_current)    (GdkDisplay        *display,
                                                        GdkGLContext      *context);

  /* Signals */
  void                   (*opened)                     (GdkDisplay     *display);
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
void                _gdk_display_add_touch_grab       (GdkDisplay       *display,
                                                       GdkDevice        *device,
                                                       GdkEventSequence *sequence,
                                                       GdkWindow        *window,
                                                       GdkWindow        *native_window,
                                                       GdkEventMask      event_mask,
                                                       unsigned long     serial_start,
                                                       guint32           time);
GdkTouchGrabInfo *  _gdk_display_has_touch_grab       (GdkDisplay       *display,
                                                       GdkDevice        *device,
                                                       GdkEventSequence *sequence,
                                                       gulong            serial);
gboolean            _gdk_display_end_touch_grab       (GdkDisplay       *display,
                                                       GdkDevice        *device,
                                                       GdkEventSequence *sequence);
void                _gdk_display_enable_motion_hints  (GdkDisplay       *display,
                                                       GdkDevice        *device);
GdkPointerWindowInfo * _gdk_display_get_pointer_info  (GdkDisplay       *display,
                                                       GdkDevice        *device);
void                _gdk_display_pointer_info_foreach (GdkDisplay       *display,
                                                       GdkDisplayPointerInfoForeach func,
                                                       gpointer          user_data);
gulong              _gdk_display_get_next_serial      (GdkDisplay       *display);
void                _gdk_display_pause_events         (GdkDisplay       *display);
void                _gdk_display_unpause_events       (GdkDisplay       *display);
void                _gdk_display_event_data_copy      (GdkDisplay       *display,
                                                       const GdkEvent   *event,
                                                       GdkEvent         *new_event);
void                _gdk_display_event_data_free      (GdkDisplay       *display,
                                                       GdkEvent         *event);
void                _gdk_display_create_window_impl   (GdkDisplay       *display,
                                                       GdkWindow        *window,
                                                       GdkWindow        *real_parent,
                                                       GdkScreen        *screen,
                                                       GdkEventMask      event_mask,
                                                       GdkWindowAttr    *attributes,
                                                       gint              attributes_mask);
GdkWindow *         _gdk_display_create_window        (GdkDisplay       *display);

gboolean            gdk_display_make_gl_context_current  (GdkDisplay        *display,
                                                          GdkGLContext      *context);

G_END_DECLS

#endif  /* __GDK_DISPLAY_PRIVATE_H__ */
