/* GDK - The GIMP Drawing Kit
 * Copyright (C) 2009 Carlos Garnacho <carlosg@gnome.org>
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

#include "config.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <string.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdktypes.h>
#include "gdkprivate-wayland.h"
#include "gdkseat-wayland.h"
#include "gdkwayland.h"
#include "gdkkeysyms.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanagerprivate.h"
#include "gdkseatprivate.h"
#include "pointer-gestures-unstable-v1-client-protocol.h"

#include <xkbcommon/xkbcommon.h>

#include <linux/input.h>

#include <sys/time.h>
#include <sys/mman.h>

#define BUTTON_BASE (BTN_LEFT - 1) /* Used to translate to 1-indexed buttons */

typedef struct _GdkWaylandTouchData GdkWaylandTouchData;
typedef struct _GdkWaylandPointerFrameData GdkWaylandPointerFrameData;

struct _GdkWaylandTouchData
{
  uint32_t id;
  gdouble x;
  gdouble y;
  GdkWindow *window;
  uint32_t touch_down_serial;
  guint initial_touch : 1;
};

struct _GdkWaylandPointerFrameData
{
  GdkEvent *event;

  /* Specific to the scroll event */
  gdouble delta_x, delta_y;
  int32_t discrete_x, discrete_y;
  gint8 is_scroll_stop;
};

struct _GdkWaylandSeat
{
  GdkSeat parent_instance;

  guint32 id;
  struct wl_seat *wl_seat;
  struct wl_pointer *wl_pointer;
  struct wl_keyboard *wl_keyboard;
  struct wl_touch *wl_touch;
  struct zwp_pointer_gesture_swipe_v1 *wp_pointer_gesture_swipe;
  struct zwp_pointer_gesture_pinch_v1 *wp_pointer_gesture_pinch;

  GdkDisplay *display;
  GdkDeviceManager *device_manager;

  GdkDevice *master_pointer;
  GdkDevice *master_keyboard;
  GdkDevice *pointer;
  GdkDevice *keyboard;
  GdkDevice *touch_master;
  GdkDevice *touch;
  GdkCursor *cursor;
  GdkKeymap *keymap;

  GHashTable *touches;

  GdkModifierType key_modifiers;
  GdkModifierType button_modifiers;
  GdkWindow *pointer_focus;
  GdkWindow *keyboard_focus;
  GdkAtom pending_selection;
  double surface_x, surface_y;
  uint32_t time;
  uint32_t enter_serial;
  uint32_t button_press_serial;
  GdkWindow *pointer_grab_window;
  uint32_t pointer_grab_time;
  gboolean have_server_repeat;
  uint32_t server_repeat_rate;
  uint32_t server_repeat_delay;

  struct wl_callback *repeat_callback;
  guint32 repeat_timer;
  guint32 repeat_key;
  guint32 repeat_count;
  GSettings *keyboard_settings;

  guint cursor_timeout_id;
  guint cursor_image_index;
  guint cursor_image_delay;

  struct gtk_primary_selection_device *primary_data_device;
  struct wl_data_device *data_device;
  GdkDragContext *drop_context;

  struct wl_surface *pointer_surface;
  guint current_output_scale;
  GSList *pointer_surface_outputs;

  /* Source/dest for non-local dnd */
  GdkWindow *foreign_dnd_window;

  /* Some tracking on gesture events */
  guint gesture_n_fingers;
  gdouble gesture_scale;

  GdkCursor *grab_cursor;

  /* Accumulated event data for a pointer frame */
  GdkWaylandPointerFrameData pointer_frame;
};

G_DEFINE_TYPE (GdkWaylandSeat, gdk_wayland_seat, GDK_TYPE_SEAT)

struct _GdkWaylandDevice
{
  GdkDevice parent_instance;
  GdkWaylandTouchData *emulating_touch; /* Only used on wd->touch_master */
};

struct _GdkWaylandDeviceClass
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkWaylandDevice, gdk_wayland_device, GDK_TYPE_DEVICE)

#define GDK_TYPE_WAYLAND_DEVICE_MANAGER        (gdk_wayland_device_manager_get_type ())
#define GDK_WAYLAND_DEVICE_MANAGER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_WAYLAND_DEVICE_MANAGER, GdkWaylandDeviceManager))
#define GDK_WAYLAND_DEVICE_MANAGER_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_WAYLAND_DEVICE_MANAGER, GdkWaylandDeviceManagerClass))
#define GDK_IS_WAYLAND_DEVICE_MANAGER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_WAYLAND_DEVICE_MANAGER))
#define GDK_IS_WAYLAND_DEVICE_MANAGER_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_WAYLAND_DEVICE_MANAGER))
#define GDK_WAYLAND_DEVICE_MANAGER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_WAYLAND_DEVICE_MANAGER, GdkWaylandDeviceManagerClass))

#define GDK_SLOT_TO_EVENT_SEQUENCE(s) ((GdkEventSequence *) GUINT_TO_POINTER((s) + 1))
#define GDK_EVENT_SEQUENCE_TO_SLOT(s) (GPOINTER_TO_UINT(s) - 1)

typedef struct _GdkWaylandDeviceManager GdkWaylandDeviceManager;
typedef struct _GdkWaylandDeviceManagerClass GdkWaylandDeviceManagerClass;

struct _GdkWaylandDeviceManager
{
  GdkDeviceManager parent_object;
  GList *devices;
};

struct _GdkWaylandDeviceManagerClass
{
  GdkDeviceManagerClass parent_class;
};

static void deliver_key_event (GdkWaylandSeat       *seat,
                               uint32_t              time_,
                               uint32_t              key,
                               uint32_t              state);
GType gdk_wayland_device_manager_get_type (void);

G_DEFINE_TYPE (GdkWaylandDeviceManager,
	       gdk_wayland_device_manager, GDK_TYPE_DEVICE_MANAGER)

static gboolean
gdk_wayland_device_get_history (GdkDevice      *device,
                                GdkWindow      *window,
                                guint32         start,
                                guint32         stop,
                                GdkTimeCoord ***events,
                                gint           *n_events)
{
  return FALSE;
}

static void
gdk_wayland_device_get_state (GdkDevice       *device,
                              GdkWindow       *window,
                              gdouble         *axes,
                              GdkModifierType *mask)
{
  gdouble x, y;

  gdk_window_get_device_position_double (window, device, &x, &y, mask);

  if (axes)
    {
      axes[0] = x;
      axes[1] = y;
    }
}

static void
gdk_wayland_seat_stop_window_cursor_animation (GdkWaylandSeat *seat)
{
  if (seat->cursor_timeout_id > 0)
    {
      g_source_remove (seat->cursor_timeout_id);
      seat->cursor_timeout_id = 0;
    }
  seat->cursor_image_index = 0;
  seat->cursor_image_delay = 0;
}

static gboolean
gdk_wayland_seat_update_window_cursor (GdkWaylandSeat *seat)
{
  struct wl_buffer *buffer;
  int x, y, w, h, scale;
  guint next_image_index, next_image_delay;
  gboolean retval = G_SOURCE_REMOVE;

  if (seat->cursor)
    {
      buffer = _gdk_wayland_cursor_get_buffer (seat->cursor,
                                               seat->cursor_image_index,
                                               &x, &y, &w, &h, &scale);
    }
  else
    {
      seat->cursor_timeout_id = 0;
      return retval;
    }

  if (!seat->wl_pointer)
    return retval;

  if (buffer)
    {
      wl_surface_attach (seat->pointer_surface, buffer, 0, 0);
      wl_surface_set_buffer_scale (seat->pointer_surface, scale);
      wl_surface_damage (seat->pointer_surface,  0, 0, w, h);
      wl_surface_commit (seat->pointer_surface);

      wl_pointer_set_cursor (seat->wl_pointer,
                             seat->enter_serial,
                             seat->pointer_surface,
                             x, y);
    }
  else
    {
      wl_pointer_set_cursor (seat->wl_pointer,
                             seat->enter_serial,
                             NULL,
                             0, 0);

      wl_surface_attach (seat->pointer_surface, NULL, 0, 0);
      wl_surface_commit (seat->pointer_surface);
    }

  next_image_index =
    _gdk_wayland_cursor_get_next_image_index (seat->cursor,
                                              seat->cursor_image_index,
                                              &next_image_delay);

  if (next_image_index != seat->cursor_image_index)
    {
      if (next_image_delay != seat->cursor_image_delay)
        {
          guint id;

          gdk_wayland_seat_stop_window_cursor_animation (seat);

          /* Queue timeout for next frame */
          id = g_timeout_add (next_image_delay,
                              (GSourceFunc)gdk_wayland_seat_update_window_cursor,
                              seat);
          g_source_set_name_by_id (id, "[gtk+] gdk_wayland_seat_update_window_cursor");
          seat->cursor_timeout_id = id;
        }
      else
        retval = G_SOURCE_CONTINUE;

      seat->cursor_image_index = next_image_index;
      seat->cursor_image_delay = next_image_delay;
    }
  else
    gdk_wayland_seat_stop_window_cursor_animation (seat);

  return retval;
}

static void
gdk_wayland_device_set_window_cursor (GdkDevice *device,
                                      GdkWindow *window,
                                      GdkCursor *cursor)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));

  if (device == seat->touch_master)
    return;

  if (seat->grab_cursor)
    cursor = seat->grab_cursor;

  /* Setting the cursor to NULL means that we should use
   * the default cursor
   */
  if (!cursor)
    {
      guint scale = seat->current_output_scale;
      cursor =
        _gdk_wayland_display_get_cursor_for_type_with_scale (seat->display,
                                                             GDK_LEFT_PTR,
                                                             scale);
    }
  else
    _gdk_wayland_cursor_set_scale (cursor, seat->current_output_scale);

  if (cursor == seat->cursor)
    return;

  gdk_wayland_seat_stop_window_cursor_animation (seat);

  if (seat->cursor)
    g_object_unref (seat->cursor);

  seat->cursor = g_object_ref (cursor);

  gdk_wayland_seat_update_window_cursor (seat);
}

static void
gdk_wayland_device_warp (GdkDevice *device,
                         GdkScreen *screen,
                         gdouble    x,
                         gdouble    y)
{
}

static void
get_coordinates (GdkWaylandSeat       *seat,
                 double               *x,
                 double               *y,
                 double               *x_root,
                 double               *y_root)
{
  int root_x, root_y;

  if (x)
    *x = seat->surface_x;
  if (y)
    *y = seat->surface_y;

  if (seat->pointer_focus)
    {
      gdk_window_get_root_coords (seat->pointer_focus,
                                  seat->surface_x,
                                  seat->surface_y,
                                  &root_x, &root_y);
    }
  else
    {
      root_x = seat->surface_x;
      root_y = seat->surface_y;
    }

  if (x_root)
    *x_root = root_x;
  if (y_root)
    *y_root = root_y;
}

static void
gdk_wayland_device_query_state (GdkDevice        *device,
                                GdkWindow        *window,
                                GdkWindow       **root_window,
                                GdkWindow       **child_window,
                                gdouble          *root_x,
                                gdouble          *root_y,
                                gdouble          *win_x,
                                gdouble          *win_y,
                                GdkModifierType  *mask)
{
  GdkWaylandSeat *seat;
  GdkScreen *default_screen;

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  default_screen = gdk_display_get_default_screen (seat->display);

  if (root_window)
    *root_window = gdk_screen_get_root_window (default_screen);
  if (child_window)
    *child_window = seat->pointer_focus;
  if (mask)
    *mask = seat->button_modifiers | seat->key_modifiers;

  get_coordinates (seat, win_x, win_y, root_x, root_y);
}

static void
emulate_crossing (GdkWindow       *window,
                  GdkWindow       *subwindow,
                  GdkDevice       *device,
                  GdkEventType     type,
                  GdkCrossingMode  mode,
                  guint32          time_)
{
  GdkEvent *event;

  event = gdk_event_new (type);
  event->crossing.window = window ? g_object_ref (window) : NULL;
  event->crossing.subwindow = subwindow ? g_object_ref (subwindow) : NULL;
  event->crossing.time = time_;
  event->crossing.mode = mode;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, device);
  gdk_event_set_seat (event, gdk_device_get_seat (device));

  gdk_window_get_device_position_double (window, device,
                                         &event->crossing.x, &event->crossing.y,
                                         &event->crossing.state);
  event->crossing.x_root = event->crossing.x;
  event->crossing.y_root = event->crossing.y;

  _gdk_wayland_display_deliver_event (gdk_window_get_display (window), event);
}

static void
emulate_touch_crossing (GdkWindow           *window,
                        GdkWindow           *subwindow,
                        GdkDevice           *device,
                        GdkDevice           *source,
                        GdkWaylandTouchData *touch,
                        GdkEventType         type,
                        GdkCrossingMode      mode,
                        guint32              time_)
{
  GdkEvent *event;

  event = gdk_event_new (type);
  event->crossing.window = window ? g_object_ref (window) : NULL;
  event->crossing.subwindow = subwindow ? g_object_ref (subwindow) : NULL;
  event->crossing.time = time_;
  event->crossing.mode = mode;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, source);
  gdk_event_set_seat (event, gdk_device_get_seat (device));

  event->crossing.x = touch->x;
  event->crossing.y = touch->y;
  event->crossing.x_root = event->crossing.x;
  event->crossing.y_root = event->crossing.y;

  _gdk_wayland_display_deliver_event (gdk_window_get_display (window), event);
}

static void
emulate_focus (GdkWindow *window,
               GdkDevice *device,
               gboolean   focus_in,
               guint32    time_)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (window);
  event->focus_change.in = focus_in;
  gdk_event_set_device (event, device);
  gdk_event_set_source_device (event, device);
  gdk_event_set_seat (event, gdk_device_get_seat (device));

  _gdk_wayland_display_deliver_event (gdk_window_get_display (window), event);
}

static void
device_emit_grab_crossing (GdkDevice       *device,
                           GdkWindow       *from,
                           GdkWindow       *to,
                           GdkCrossingMode  mode,
                           guint32          time_)
{
  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      if (from)
        emulate_focus (from, device, FALSE, time_);
      if (to)
        emulate_focus (to, device, TRUE, time_);
    }
  else
    {
      if (from)
        emulate_crossing (from, to, device, GDK_LEAVE_NOTIFY, mode, time_);
      if (to)
        emulate_crossing (to, from, device, GDK_ENTER_NOTIFY, mode, time_);
    }
}

static GdkWindow *
gdk_wayland_device_get_focus (GdkDevice *device)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));

  if (device == wayland_seat->master_keyboard)
    return wayland_seat->keyboard_focus;
  else if (device == wayland_seat->master_pointer)
    return wayland_seat->pointer_focus;
  else if (device == wayland_seat->touch_master &&
           GDK_WAYLAND_DEVICE(device)->emulating_touch)
    return GDK_WAYLAND_DEVICE(device)->emulating_touch->window;
  else
    return NULL;
}

static GdkGrabStatus
gdk_wayland_device_grab (GdkDevice    *device,
                         GdkWindow    *window,
                         gboolean      owner_events,
                         GdkEventMask  event_mask,
                         GdkWindow    *confine_to,
                         GdkCursor    *cursor,
                         guint32       time_)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkWindow *prev_focus = gdk_wayland_device_get_focus (device);

  if (prev_focus != window)
    device_emit_grab_crossing (device, prev_focus, window, GDK_CROSSING_GRAB, time_);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      return GDK_GRAB_SUCCESS;
    }
  else
    {
      /* Device is a pointer */
      if (wayland_seat->pointer_grab_window != NULL &&
          time_ != 0 && wayland_seat->pointer_grab_time > time_)
        {
          return GDK_GRAB_ALREADY_GRABBED;
        }

      if (time_ == 0)
        time_ = wayland_seat->time;

      wayland_seat->pointer_grab_window = window;
      wayland_seat->pointer_grab_time = time_;
      _gdk_wayland_window_set_grab_seat (window, GDK_SEAT (wayland_seat));

      g_clear_object (&wayland_seat->cursor);

      if (cursor)
        wayland_seat->cursor = g_object_ref (cursor);

      gdk_wayland_seat_update_window_cursor (wayland_seat);
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_wayland_device_ungrab (GdkDevice *device,
                           guint32    time_)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  GdkDisplay *display;
  GdkDeviceGrabInfo *grab;
  GdkWindow *focus, *prev_focus = NULL;

  display = gdk_device_get_display (device);

  grab = _gdk_display_get_last_device_grab (display, device);

  if (grab)
    {
      grab->serial_end = grab->serial_start;
      prev_focus = grab->window;
    }

  focus = gdk_wayland_device_get_focus (device);

  if (focus != prev_focus)
    device_emit_grab_crossing (device, prev_focus, focus, GDK_CROSSING_UNGRAB, time_);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
    }
  else
    {
      /* Device is a pointer */
      gdk_wayland_seat_update_window_cursor (wayland_seat);

      if (wayland_seat->pointer_grab_window)
        _gdk_wayland_window_set_grab_seat (wayland_seat->pointer_grab_window,
                                           NULL);
    }
}

static GdkWindow *
gdk_wayland_device_window_at_position (GdkDevice       *device,
                                       gdouble         *win_x,
                                       gdouble         *win_y,
                                       GdkModifierType *mask,
                                       gboolean         get_toplevel)
{
  GdkWaylandSeat *seat;
  GdkWindow *window = NULL;

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));

  if (device == seat->master_pointer)
    {
      if (win_x)
        *win_x = seat->surface_x;
      if (win_y)
        *win_y = seat->surface_y;

      if (mask)
        *mask |= seat->button_modifiers;

      window = seat->pointer_focus;
    }
  else if (device == seat->touch_master)
    {
      GdkWaylandTouchData *touch;

      touch = GDK_WAYLAND_DEVICE(device)->emulating_touch;

      if (touch)
        {
          if (win_x)
            *win_x = touch->x;
          if (win_y)
            *win_y = touch->y;
          if (mask)
            *mask |= GDK_BUTTON1_MASK;

          window = touch->window;
        }
    }

  if (mask)
    *mask = seat->key_modifiers;

  return window;
}

static void
gdk_wayland_device_select_window_events (GdkDevice    *device,
                                         GdkWindow    *window,
                                         GdkEventMask  event_mask)
{
}

static void
gdk_wayland_device_class_init (GdkWaylandDeviceClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_wayland_device_get_history;
  device_class->get_state = gdk_wayland_device_get_state;
  device_class->set_window_cursor = gdk_wayland_device_set_window_cursor;
  device_class->warp = gdk_wayland_device_warp;
  device_class->query_state = gdk_wayland_device_query_state;
  device_class->grab = gdk_wayland_device_grab;
  device_class->ungrab = gdk_wayland_device_ungrab;
  device_class->window_at_position = gdk_wayland_device_window_at_position;
  device_class->select_window_events = gdk_wayland_device_select_window_events;
}

static void
gdk_wayland_device_init (GdkWaylandDevice *device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_core);

  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_Y, 0, 0, 1);
}

/**
 * gdk_wayland_device_get_wl_seat:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_seat of a #GdkDevice.
 *
 * Returns: (transfer none): a Wayland wl_seat
 *
 * Since: 3.10
 */
struct wl_seat *
gdk_wayland_device_get_wl_seat (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_seat;
}

/**
 * gdk_wayland_device_get_wl_pointer:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_pointer of a #GdkDevice.
 *
 * Returns: (transfer none): a Wayland wl_pointer
 *
 * Since: 3.10
 */
struct wl_pointer *
gdk_wayland_device_get_wl_pointer (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_pointer;
}

/**
 * gdk_wayland_device_get_wl_keyboard:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_keyboard of a #GdkDevice.
 *
 * Returns: (transfer none): a Wayland wl_keyboard
 *
 * Since: 3.10
 */
struct wl_keyboard *
gdk_wayland_device_get_wl_keyboard (GdkDevice *device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->wl_keyboard;
}

GdkKeymap *
_gdk_wayland_device_get_keymap (GdkDevice *device)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));
  return seat->keymap;
}

static void
emit_selection_owner_change (GdkWindow *window,
                             GdkAtom    atom)
{
  GdkEvent *event;

  event = gdk_event_new (GDK_OWNER_CHANGE);
  event->owner_change.window = g_object_ref (window);
  event->owner_change.owner = NULL;
  event->owner_change.reason = GDK_OWNER_CHANGE_NEW_OWNER;
  event->owner_change.selection = atom;
  event->owner_change.time = GDK_CURRENT_TIME;
  event->owner_change.selection_time = GDK_CURRENT_TIME;

  gdk_event_put (event);
  gdk_event_free (event);
}

static void
data_device_data_offer (void                  *data,
                        struct wl_data_device *data_device,
                        struct wl_data_offer  *offer)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("data device data offer, data device %p, offer %p",
                       data_device, offer));

  gdk_wayland_selection_ensure_offer (seat->display, offer);
}

static void
data_device_enter (void                  *data,
                   struct wl_data_device *data_device,
                   uint32_t               serial,
                   struct wl_surface     *surface,
                   wl_fixed_t             x,
                   wl_fixed_t             y,
                   struct wl_data_offer  *offer)
{
  GdkWaylandSeat *seat = data;
  GdkWindow *dest_window, *dnd_owner;
  GdkAtom selection;

  dest_window = wl_surface_get_user_data (surface);

  if (!GDK_IS_WINDOW (dest_window))
    return;

  GDK_NOTE (EVENTS,
            g_message ("data device enter, data device %p serial %u, surface %p, x %f y %f, offer %p",
                       data_device, serial, surface, wl_fixed_to_double (x), wl_fixed_to_double (y), offer));

  /* Update pointer state, so device state queries work during DnD */
  seat->pointer_focus = g_object_ref (dest_window);
  seat->surface_x = wl_fixed_to_double (x);
  seat->surface_y = wl_fixed_to_double (y);

  gdk_wayland_drop_context_update_targets (seat->drop_context);

  selection = gdk_drag_get_selection (seat->drop_context);
  dnd_owner = gdk_selection_owner_get_for_display (seat->display, selection);

  if (!dnd_owner)
    dnd_owner = seat->foreign_dnd_window;

  _gdk_wayland_drag_context_set_source_window (seat->drop_context, dnd_owner);

  _gdk_wayland_drag_context_set_dest_window (seat->drop_context,
                                             dest_window, serial);
  _gdk_wayland_drag_context_set_coords (seat->drop_context,
                                        wl_fixed_to_double (x),
                                        wl_fixed_to_double (y));
  _gdk_wayland_drag_context_emit_event (seat->drop_context, GDK_DRAG_ENTER,
                                        GDK_CURRENT_TIME);

  gdk_wayland_selection_set_offer (seat->display, selection, offer);

  emit_selection_owner_change (dest_window, selection);
}

static void
data_device_leave (void                  *data,
                   struct wl_data_device *data_device)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("data device leave, data device %p", data_device));

  if (!gdk_drag_context_get_dest_window (seat->drop_context))
    return;

  seat->pointer_focus = NULL;

  _gdk_wayland_drag_context_set_coords (seat->drop_context, -1, -1);
  _gdk_wayland_drag_context_emit_event (seat->drop_context, GDK_DRAG_LEAVE,
                                        GDK_CURRENT_TIME);
  _gdk_wayland_drag_context_set_dest_window (seat->drop_context, NULL, 0);
}

static void
data_device_motion (void                  *data,
                    struct wl_data_device *data_device,
                    uint32_t               time,
                    wl_fixed_t             x,
                    wl_fixed_t             y)
{
  GdkWaylandSeat *seat = data;

  g_debug (G_STRLOC ": %s data_device = %p, time = %d, x = %f, y = %f",
           G_STRFUNC, data_device, time, wl_fixed_to_double (x), wl_fixed_to_double (y));

  if (!gdk_drag_context_get_dest_window (seat->drop_context))
    return;

  /* Update pointer state, so device state queries work during DnD */
  seat->surface_x = wl_fixed_to_double (x);
  seat->surface_y = wl_fixed_to_double (y);

  gdk_wayland_drop_context_update_targets (seat->drop_context);
  _gdk_wayland_drag_context_set_coords (seat->drop_context,
                                        wl_fixed_to_double (x),
                                        wl_fixed_to_double (y));
  _gdk_wayland_drag_context_emit_event (seat->drop_context,
                                        GDK_DRAG_MOTION, time);
}

static void
data_device_drop (void                  *data,
                  struct wl_data_device *data_device)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("data device drop, data device %p", data_device));

  _gdk_wayland_drag_context_emit_event (seat->drop_context,
                                        GDK_DROP_START, GDK_CURRENT_TIME);
}

static void
data_device_selection (void                  *data,
                       struct wl_data_device *wl_data_device,
                       struct wl_data_offer  *offer)
{
  GdkWaylandSeat *seat = data;
  GdkAtom selection;

  GDK_NOTE (EVENTS,
            g_message ("data device selection, data device %p, data offer %p",
                       wl_data_device, offer));

  selection = gdk_atom_intern_static_string ("CLIPBOARD");
  gdk_wayland_selection_set_offer (seat->display, selection, offer);

  /* If we already have keyboard focus, the selection was targeted at the
   * focused surface. If we don't we will receive keyboard focus directly after
   * this, so lets wait and find out what window will get the focus before
   * emitting the owner-changed event.
   */
  if (seat->keyboard_focus)
    emit_selection_owner_change (seat->keyboard_focus, selection);
  else
    seat->pending_selection = selection;
}

static const struct wl_data_device_listener data_device_listener = {
  data_device_data_offer,
  data_device_enter,
  data_device_leave,
  data_device_motion,
  data_device_drop,
  data_device_selection
};

static void
primary_selection_data_offer (void                                *data,
                              struct gtk_primary_selection_device *gtk_primary_selection_device,
                              struct gtk_primary_selection_offer  *gtk_primary_offer)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("primary selection offer, device %p, data offer %p",
                       gtk_primary_selection_device, gtk_primary_offer));

  gdk_wayland_selection_ensure_primary_offer (seat->display, gtk_primary_offer);
}

static void
primary_selection_selection (void                                *data,
                             struct gtk_primary_selection_device *gtk_primary_selection_device,
                             struct gtk_primary_selection_offer  *gtk_primary_offer)
{
  GdkWaylandSeat *seat = data;
  GdkAtom selection;

  GDK_NOTE (EVENTS,
            g_message ("primary selection selection, device %p, data offer %p",
                       gtk_primary_selection_device, gtk_primary_offer));

  selection = gdk_atom_intern_static_string ("PRIMARY");
  gdk_wayland_selection_set_offer (seat->display, selection, gtk_primary_offer);
  emit_selection_owner_change (seat->keyboard_focus, selection);
}

static const struct gtk_primary_selection_device_listener primary_selection_device_listener = {
  primary_selection_data_offer,
  primary_selection_selection,
};

static GdkEvent *
create_scroll_event (GdkWaylandSeat *seat,
                     gboolean        emulated)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;

  event = gdk_event_new (GDK_SCROLL);
  event->scroll.window = g_object_ref (seat->pointer_focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  event->scroll.time = seat->time;
  event->scroll.state = seat->button_modifiers | seat->key_modifiers;
  gdk_event_set_screen (event, display->screen);

  _gdk_event_set_pointer_emulated (event, emulated);

  get_coordinates (seat,
                   &event->scroll.x,
                   &event->scroll.y,
                   &event->scroll.x_root,
                   &event->scroll.y_root);

  return event;
}

static void
flush_discrete_scroll_event (GdkWaylandSeat     *seat,
                             GdkScrollDirection  direction)
{
  GdkEvent *event;

  event = create_scroll_event (seat, TRUE);
  event->scroll.direction = direction;

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
flush_smooth_scroll_event (GdkWaylandSeat *seat,
                           gdouble         delta_x,
                           gdouble         delta_y,
                           gboolean        is_stop)
{
  GdkEvent *event;

  event = create_scroll_event (seat, FALSE);
  event->scroll.direction = GDK_SCROLL_SMOOTH;
  event->scroll.delta_x = delta_x;
  event->scroll.delta_y = delta_y;
  event->scroll.is_stop = is_stop;

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
flush_scroll_event (GdkWaylandSeat             *seat,
                    GdkWaylandPointerFrameData *pointer_frame)
{
  gboolean is_stop = FALSE;

  if (pointer_frame->discrete_x || pointer_frame->discrete_y)
    {
      GdkScrollDirection direction;

      if (pointer_frame->discrete_x > 0)
        direction = GDK_SCROLL_LEFT;
      else if (pointer_frame->discrete_x < 0)
        direction = GDK_SCROLL_RIGHT;
      else if (pointer_frame->discrete_y > 0)
        direction = GDK_SCROLL_UP;
      else
        direction = GDK_SCROLL_DOWN;

      flush_discrete_scroll_event (seat, direction);
      pointer_frame->discrete_x = 0;
      pointer_frame->discrete_y = 0;
    }

  if (pointer_frame->is_scroll_stop ||
      pointer_frame->delta_x != 0 ||
      pointer_frame->delta_y != 0)
    {
      /* Axes can stop independently, if we stop on one axis but have a
       * delta on the other, we don't count it as a stop event.
       */
      if (pointer_frame->is_scroll_stop &&
          pointer_frame->delta_x == 0 &&
          pointer_frame->delta_y == 0)
        is_stop = TRUE;

      flush_smooth_scroll_event (seat,
                                 pointer_frame->delta_x,
                                 pointer_frame->delta_y,
                                 is_stop);

      pointer_frame->delta_x = 0;
      pointer_frame->delta_y = 0;
      pointer_frame->is_scroll_stop = FALSE;
    }
}

static void
gdk_wayland_seat_flush_frame_event (GdkWaylandSeat *seat)
{
  if (seat->pointer_frame.event)
    {
      _gdk_wayland_display_deliver_event (gdk_seat_get_display (GDK_SEAT (seat)),
                                          seat->pointer_frame.event);
      seat->pointer_frame.event = NULL;
    }
  else
    flush_scroll_event (seat, &seat->pointer_frame);
}

static GdkEvent *
gdk_wayland_seat_get_frame_event (GdkWaylandSeat *seat,
                                  GdkEventType    evtype)
{
  if (seat->pointer_frame.event &&
      seat->pointer_frame.event->type != evtype)
    gdk_wayland_seat_flush_frame_event (seat);

  seat->pointer_frame.event = gdk_event_new (evtype);
  return seat->pointer_frame.event;
}

static void
pointer_handle_enter (void              *data,
                      struct wl_pointer *pointer,
                      uint32_t           serial,
                      struct wl_surface *surface,
                      wl_fixed_t         sx,
                      wl_fixed_t         sy)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (seat->display);

  if (!surface)
    return;

  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  seat->pointer_focus = wl_surface_get_user_data(surface);
  g_object_ref(seat->pointer_focus);

  seat->button_modifiers = 0;

  seat->surface_x = wl_fixed_to_double (sx);
  seat->surface_y = wl_fixed_to_double (sy);
  seat->enter_serial = serial;

  event = gdk_wayland_seat_get_frame_event (seat, GDK_ENTER_NOTIFY);
  event->crossing.window = g_object_ref (seat->pointer_focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, gdk_device_get_seat (seat->master_pointer));
  event->crossing.subwindow = NULL;
  event->crossing.time = (guint32)(g_get_monotonic_time () / 1000);
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  event->crossing.focus = TRUE;
  event->crossing.state = 0;

  gdk_wayland_seat_update_window_cursor (seat);

  get_coordinates (seat,
                   &event->crossing.x,
                   &event->crossing.y,
                   &event->crossing.x_root,
                   &event->crossing.y_root);

  GDK_NOTE (EVENTS,
            g_message ("enter, seat %p surface %p",
                       seat, seat->pointer_focus));

  if (wayland_display->seat_version < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_leave (void              *data,
                      struct wl_pointer *pointer,
                      uint32_t           serial,
                      struct wl_surface *surface)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (seat->display);

  if (!surface)
    return;

  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  if (!seat->pointer_focus)
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  event = gdk_wayland_seat_get_frame_event (seat, GDK_LEAVE_NOTIFY);
  event->crossing.window = g_object_ref (seat->pointer_focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->crossing.subwindow = NULL;
  event->crossing.time = (guint32)(g_get_monotonic_time () / 1000);
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_NONLINEAR;
  event->crossing.focus = TRUE;
  event->crossing.state = 0;

  gdk_wayland_seat_update_window_cursor (seat);

  get_coordinates (seat,
                   &event->crossing.x,
                   &event->crossing.y,
                   &event->crossing.x_root,
                   &event->crossing.y_root);

  GDK_NOTE (EVENTS,
            g_message ("leave, seat %p surface %p",
                       seat, seat->pointer_focus));

  g_object_unref (seat->pointer_focus);
  if (seat->cursor)
    gdk_wayland_seat_stop_window_cursor_animation (seat);

  seat->pointer_focus = NULL;

  if (wayland_display->seat_version < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_motion (void              *data,
                       struct wl_pointer *pointer,
                       uint32_t           time,
                       wl_fixed_t         sx,
                       wl_fixed_t         sy)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;

  if (!seat->pointer_focus)
    return;

  seat->time = time;
  seat->surface_x = wl_fixed_to_double (sx);
  seat->surface_y = wl_fixed_to_double (sy);

  event = gdk_wayland_seat_get_frame_event (seat, GDK_MOTION_NOTIFY);
  event->motion.window = g_object_ref (seat->pointer_focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->motion.time = time;
  event->motion.axes = NULL;
  event->motion.state = seat->button_modifiers | seat->key_modifiers;
  event->motion.is_hint = 0;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (seat,
                   &event->motion.x,
                   &event->motion.y,
                   &event->motion.x_root,
                   &event->motion.y_root);

  GDK_NOTE (EVENTS,
            g_message ("motion %f %f, seat %p state %d",
                       wl_fixed_to_double (sx), wl_fixed_to_double (sy),
		       seat, event->motion.state));

  if (display->seat_version < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_button (void              *data,
                       struct wl_pointer *pointer,
                       uint32_t           serial,
                       uint32_t           time,
                       uint32_t           button,
                       uint32_t           state)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;
  uint32_t modifier;
  int gdk_button;

  if (!seat->pointer_focus)
    return;

  _gdk_wayland_display_update_serial (display, serial);

  switch (button)
    {
    case BTN_LEFT:
      gdk_button = GDK_BUTTON_PRIMARY;
      break;
    case BTN_MIDDLE:
      gdk_button = GDK_BUTTON_MIDDLE;
      break;
    case BTN_RIGHT:
      gdk_button = GDK_BUTTON_SECONDARY;
      break;
    default:
       /* For compatibility reasons, all additional buttons go after the old 4-7 scroll ones */
      gdk_button = button - BUTTON_BASE + 4;
      break;
    }

  seat->time = time;
  if (state)
    seat->button_press_serial = serial;

  event = gdk_wayland_seat_get_frame_event (seat,
                                            state ? GDK_BUTTON_PRESS :
                                            GDK_BUTTON_RELEASE);
  event->button.window = g_object_ref (seat->pointer_focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->button.time = time;
  event->button.axes = NULL;
  event->button.state = seat->button_modifiers | seat->key_modifiers;
  event->button.button = gdk_button;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (seat,
                   &event->button.x,
                   &event->button.y,
                   &event->button.x_root,
                   &event->button.y_root);

  modifier = 1 << (8 + gdk_button - 1);
  if (state)
    seat->button_modifiers |= modifier;
  else
    seat->button_modifiers &= ~modifier;

  GDK_NOTE (EVENTS,
	    g_message ("button %d %s, seat %p state %d",
		       event->button.button,
		       state ? "press" : "release",
                       seat,
                       event->button.state));

  if (display->seat_version < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_axis (void              *data,
                     struct wl_pointer *pointer,
                     uint32_t           time,
                     uint32_t           axis,
                     wl_fixed_t         value)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_frame;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  if (!seat->pointer_focus)
    return;

  /* get the delta and convert it into the expected range */
  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      pointer_frame->delta_y = wl_fixed_to_double (value) / 10.0;
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      pointer_frame->delta_x = wl_fixed_to_double (value) / 10.0;
      break;
    default:
      g_return_if_reached ();
    }

  seat->time = time;

  GDK_NOTE (EVENTS,
            g_message ("scroll, axis %d, value %f, seat %p",
                       axis, wl_fixed_to_double (value) / 10.0,
                       seat));

  if (display->seat_version < WL_POINTER_HAS_FRAME)
    gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_frame (void              *data,
                      struct wl_pointer *pointer)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("frame, seat %p", seat));

  gdk_wayland_seat_flush_frame_event (seat);
}

static void
pointer_handle_axis_source (void                        *data,
                            struct wl_pointer           *pointer,
                            enum wl_pointer_axis_source  source)
{
  GdkWaylandSeat *seat = data;

  if (!seat->pointer_focus)
    return;

  /* We don't need to handle the scroll source right now. It only has real
   * meaning for 'finger' (to trigger kinetic scrolling). The axis_stop
   * event will generate the zero delta required to trigger kinetic
   * scrolling, so explicity handling the source is not required.
   */

  GDK_NOTE (EVENTS,
            g_message ("axis source %d, seat %p", source, seat));
}

static void
pointer_handle_axis_stop (void              *data,
                          struct wl_pointer *pointer,
                          uint32_t           time,
                          uint32_t           axis)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_frame;

  if (!seat->pointer_focus)
    return;

  seat->time = time;

  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      pointer_frame->delta_y = 0;
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      pointer_frame->delta_x = 0;
      break;
    default:
      g_return_if_reached ();
    }

  pointer_frame->is_scroll_stop = TRUE;

  GDK_NOTE (EVENTS,
            g_message ("axis stop, seat %p", seat));
}

static void
pointer_handle_axis_discrete (void              *data,
                              struct wl_pointer *pointer,
                              uint32_t           axis,
                              int32_t            value)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandPointerFrameData *pointer_frame = &seat->pointer_frame;

  if (!seat->pointer_focus)
    return;

  switch (axis)
    {
    case WL_POINTER_AXIS_VERTICAL_SCROLL:
      pointer_frame->discrete_y = value;
      break;
    case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
      pointer_frame->discrete_x = value;
      break;
    default:
      g_return_if_reached ();
    }

  GDK_NOTE (EVENTS,
            g_message ("discrete scroll, axis %d, value %d, seat %p",
                       axis, value, seat));
}

static void
keyboard_handle_keymap (void               *data,
                        struct wl_keyboard *keyboard,
                        uint32_t            format,
                        int                 fd,
                        uint32_t            size)
{
  GdkWaylandSeat *seat = data;

  _gdk_wayland_keymap_update_from_fd (seat->keymap, format, fd, size);

  g_signal_emit_by_name (seat->keymap, "keys-changed");
  g_signal_emit_by_name (seat->keymap, "state-changed");
  g_signal_emit_by_name (seat->keymap, "direction-changed");
}

static void
keyboard_handle_enter (void               *data,
                       struct wl_keyboard *keyboard,
                       uint32_t            serial,
                       struct wl_surface  *surface,
                       struct wl_array    *keys)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  if (!surface)
    return;

  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  _gdk_wayland_display_update_serial (display, serial);

  seat->keyboard_focus = wl_surface_get_user_data (surface);
  g_object_ref (seat->keyboard_focus);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (seat->keyboard_focus);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = TRUE;
  gdk_event_set_device (event, seat->master_keyboard);
  gdk_event_set_source_device (event, seat->keyboard);
  gdk_event_set_seat (event, gdk_device_get_seat (seat->master_pointer));

  GDK_NOTE (EVENTS,
            g_message ("focus in, seat %p surface %p",
                       seat, seat->keyboard_focus));

  _gdk_wayland_display_deliver_event (seat->display, event);

  if (seat->pending_selection != GDK_NONE)
    {
      emit_selection_owner_change (seat->keyboard_focus,
                                   seat->pending_selection);
      seat->pending_selection = GDK_NONE;
    }
}

static void stop_key_repeat (GdkWaylandSeat *seat);

static void
keyboard_handle_leave (void               *data,
                       struct wl_keyboard *keyboard,
                       uint32_t            serial,
                       struct wl_surface  *surface)
{
  GdkWaylandSeat *seat = data;
  GdkEvent *event;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  if (!seat->keyboard_focus)
    return;

  /* gdk_window_is_destroyed() might already return TRUE for
   * seat->keyboard_focus here, which would happen if we destroyed the
   * window before loosing keyboard focus.
   */

  stop_key_repeat (seat);

  _gdk_wayland_display_update_serial (display, serial);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (seat->keyboard_focus);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = FALSE;
  gdk_event_set_device (event, seat->master_keyboard);
  gdk_event_set_source_device (event, seat->keyboard);
  gdk_event_set_seat (event, gdk_device_get_seat (seat->master_keyboard));

  g_object_unref (seat->keyboard_focus);
  seat->keyboard_focus = NULL;

  GDK_NOTE (EVENTS,
            g_message ("focus out, seat %p surface %p",
                       seat, seat->keyboard_focus));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static gboolean keyboard_repeat (gpointer data);

static void
translate_keyboard_string (GdkEventKey *event)
{
  gunichar c = 0;
  gchar buf[7];

  /* Fill in event->string crudely, since various programs
   * depend on it.
   */
  event->string = NULL;

  if (event->keyval != GDK_KEY_VoidSymbol)
    c = gdk_keyval_to_unicode (event->keyval);

  if (c)
    {
      gsize bytes_written;
      gint len;

      /* Apply the control key - Taken from Xlib */
      if (event->state & GDK_CONTROL_MASK)
        {
          if ((c >= '@' && c < '\177') || c == ' ')
            c &= 0x1F;
          else if (c == '2')
            {
              event->string = g_memdup ("\0\0", 2);
              event->length = 1;
              buf[0] = '\0';
              return;
            }
          else if (c >= '3' && c <= '7')
            c -= ('3' - '\033');
          else if (c == '8')
            c = '\177';
          else if (c == '/')
            c = '_' & 0x1F;
        }

      len = g_unichar_to_utf8 (c, buf);
      buf[len] = '\0';

      event->string = g_locale_from_utf8 (buf, len,
                                          NULL, &bytes_written,
                                          NULL);
      if (event->string)
        event->length = bytes_written;
    }
  else if (event->keyval == GDK_KEY_Escape)
    {
      event->length = 1;
      event->string = g_strdup ("\033");
    }
  else if (event->keyval == GDK_KEY_Return ||
           event->keyval == GDK_KEY_KP_Enter)
    {
      event->length = 1;
      event->string = g_strdup ("\r");
    }

  if (!event->string)
    {
      event->length = 0;
      event->string = g_strdup ("");
    }
}

static GSettings *
get_keyboard_settings (GdkWaylandSeat *seat)
{
  if (!seat->keyboard_settings)
    {
      GSettingsSchemaSource *source;
      GSettingsSchema *schema;

      source = g_settings_schema_source_get_default ();
      schema = g_settings_schema_source_lookup (source, "org.gnome.settings-daemon.peripherals.keyboard", FALSE);
      if (schema != NULL)
        {
          seat->keyboard_settings = g_settings_new_full (schema, NULL, NULL);
          g_settings_schema_unref (schema);
        }
    }

  return seat->keyboard_settings;
}

static gboolean
get_key_repeat (GdkWaylandSeat *seat,
                guint          *delay,
                guint          *interval)
{
  gboolean repeat;

  if (seat->have_server_repeat)
    {
      if (seat->server_repeat_rate > 0)
        {
          repeat = TRUE;
          *delay = seat->server_repeat_delay;
          *interval = (1000 / seat->server_repeat_rate);
        }
      else
        {
          repeat = FALSE;
        }
    }
  else
    {
      GSettings *keyboard_settings = get_keyboard_settings (seat);

      if (keyboard_settings)
        {
          repeat = g_settings_get_boolean (keyboard_settings, "repeat");
          *delay = g_settings_get_uint (keyboard_settings, "delay");
          *interval = g_settings_get_uint (keyboard_settings, "repeat-interval");
        }
      else
        {
          repeat = TRUE;
          *delay = 400;
          *interval = 80;
        }
    }

  return repeat;
}

static void
stop_key_repeat (GdkWaylandSeat *seat)
{
  if (seat->repeat_timer)
    {
      g_source_remove (seat->repeat_timer);
      seat->repeat_timer = 0;
    }

  g_clear_pointer (&seat->repeat_callback, wl_callback_destroy);
}

static void
deliver_key_event (GdkWaylandSeat *seat,
                   uint32_t        time_,
                   uint32_t        key,
                   uint32_t        state)
{
  GdkEvent *event;
  struct xkb_state *xkb_state;
  struct xkb_keymap *xkb_keymap;
  GdkKeymap *keymap;
  xkb_keysym_t sym;
  guint delay, interval, timeout;

  stop_key_repeat (seat);

  keymap = seat->keymap;
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);
  xkb_keymap = _gdk_wayland_keymap_get_xkb_keymap (keymap);

  sym = xkb_state_key_get_one_sym (xkb_state, key);

  if (sym == XKB_KEY_NoSymbol)
    return;

  seat->time = time_;
  seat->key_modifiers = gdk_keymap_get_modifier_state (keymap);

  event = gdk_event_new (state ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
  event->key.window = seat->keyboard_focus ? g_object_ref (seat->keyboard_focus) : NULL;
  gdk_event_set_device (event, seat->master_keyboard);
  gdk_event_set_source_device (event, seat->keyboard);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->key.time = time_;
  event->key.state = seat->button_modifiers | seat->key_modifiers;
  event->key.group = 0;
  event->key.hardware_keycode = key;
  event->key.keyval = sym;
  event->key.is_modifier = _gdk_wayland_keymap_key_is_modifier (keymap, key);

  translate_keyboard_string (&event->key);

  _gdk_wayland_display_deliver_event (seat->display, event);

  GDK_NOTE (EVENTS,
            g_message ("keyboard event, code %d, sym %d, "
                       "string %s, mods 0x%x",
                       event->key.hardware_keycode, event->key.keyval,
                       event->key.string, event->key.state));

  if (state == 0)
    return;

  if (!xkb_keymap_key_repeats (xkb_keymap, key))
    return;

  if (!get_key_repeat (seat, &delay, &interval))
    return;

  seat->repeat_count++;
  seat->repeat_key = key;

  if (seat->repeat_count == 1)
    timeout = delay;
  else
    timeout = interval;

  seat->repeat_timer =
    gdk_threads_add_timeout (timeout, keyboard_repeat, seat);
  g_source_set_name_by_id (seat->repeat_timer, "[gtk+] keyboard_repeat");
}

static void
sync_after_repeat_callback (void               *data,
                            struct wl_callback *callback,
                            uint32_t            time)
{
  GdkWaylandSeat *seat = data;

  g_clear_pointer (&seat->repeat_callback, wl_callback_destroy);

  deliver_key_event (seat, seat->time, seat->repeat_key, 1);
}

static const struct wl_callback_listener sync_after_repeat_callback_listener = {
  sync_after_repeat_callback
};

static gboolean
keyboard_repeat (gpointer data)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  /* Ping the server and wait for the timeout.  We won't process
   * key repeat until it responds, since a hung server could lead
   * to a delayed key release event. We don't want to generate
   * repeat events long after the user released the key, just because
   * the server is tardy in telling us the user released the key.
   */
  seat->repeat_callback = wl_display_sync (display->wl_display);

  wl_callback_add_listener (seat->repeat_callback,
                            &sync_after_repeat_callback_listener,
                            seat);

  seat->repeat_timer = 0;

  return G_SOURCE_REMOVE;
}

static void
keyboard_handle_key (void               *data,
                     struct wl_keyboard *keyboard,
                     uint32_t            serial,
                     uint32_t            time,
                     uint32_t            key,
                     uint32_t            state_w)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  if (!seat->keyboard_focus)
    return;

  seat->repeat_count = 0;
  _gdk_wayland_display_update_serial (display, serial);
  deliver_key_event (data, time, key + 8, state_w);
}

static void
keyboard_handle_modifiers (void               *data,
                           struct wl_keyboard *keyboard,
                           uint32_t            serial,
                           uint32_t            mods_depressed,
                           uint32_t            mods_latched,
                           uint32_t            mods_locked,
                           uint32_t            group)
{
  GdkWaylandSeat *seat = data;
  GdkKeymap *keymap;
  struct xkb_state *xkb_state;
  PangoDirection direction;

  keymap = seat->keymap;
  direction = gdk_keymap_get_direction (keymap);
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);
  seat->key_modifiers = mods_depressed | mods_latched | mods_locked;

  xkb_state_update_mask (xkb_state, mods_depressed, mods_latched, mods_locked, group, 0, 0);

  g_signal_emit_by_name (keymap, "state-changed");
  if (direction != gdk_keymap_get_direction (keymap))
    g_signal_emit_by_name (keymap, "direction-changed");
}

static void
keyboard_handle_repeat_info (void               *data,
                             struct wl_keyboard *keyboard,
                             int32_t             rate,
                             int32_t             delay)
{
  GdkWaylandSeat *seat = data;

  seat->have_server_repeat = TRUE;
  seat->server_repeat_rate = rate;
  seat->server_repeat_delay = delay;
}

static GdkWaylandTouchData *
gdk_wayland_seat_add_touch (GdkWaylandSeat    *seat,
                            uint32_t           id,
                            struct wl_surface *surface)
{
  GdkWaylandTouchData *touch;

  touch = g_new0 (GdkWaylandTouchData, 1);
  touch->id = id;
  touch->window = wl_surface_get_user_data (surface);
  touch->initial_touch = (g_hash_table_size (seat->touches) == 0);

  g_hash_table_insert (seat->touches, GUINT_TO_POINTER (id), touch);

  return touch;
}

static GdkWaylandTouchData *
gdk_wayland_seat_get_touch (GdkWaylandSeat *seat,
                            uint32_t        id)
{
  return g_hash_table_lookup (seat->touches, GUINT_TO_POINTER (id));
}

static void
gdk_wayland_seat_remove_touch (GdkWaylandSeat *seat,
                               uint32_t        id)
{
  g_hash_table_remove (seat->touches, GUINT_TO_POINTER (id));
}

static GdkEvent *
_create_touch_event (GdkWaylandSeat       *seat,
                     GdkWaylandTouchData  *touch,
                     GdkEventType          evtype,
                     uint32_t              time)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  gint x_root, y_root;
  GdkEvent *event;

  event = gdk_event_new (evtype);
  event->touch.window = g_object_ref (touch->window);
  gdk_event_set_device (event, seat->touch_master);
  gdk_event_set_source_device (event, seat->touch);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->touch.time = time;
  event->touch.state = seat->button_modifiers | seat->key_modifiers;
  gdk_event_set_screen (event, display->screen);
  event->touch.sequence = GDK_SLOT_TO_EVENT_SEQUENCE (touch->id);

  if (touch->initial_touch)
    {
      _gdk_event_set_pointer_emulated (event, TRUE);
      event->touch.emulating_pointer = TRUE;
    }

  gdk_window_get_root_coords (touch->window,
                              touch->x, touch->y,
                              &x_root, &y_root);

  event->touch.x = touch->x;
  event->touch.y = touch->y;
  event->touch.x_root = x_root;
  event->touch.y_root = y_root;

  return event;
}

static void
touch_handle_down (void              *data,
                   struct wl_touch   *wl_touch,
                   uint32_t           serial,
                   uint32_t           time,
                   struct wl_surface *wl_surface,
                   int32_t            id,
                   wl_fixed_t         x,
                   wl_fixed_t         y)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  _gdk_wayland_display_update_serial (display, serial);

  touch = gdk_wayland_seat_add_touch (seat, id, wl_surface);
  touch->x = wl_fixed_to_double (x);
  touch->y = wl_fixed_to_double (y);
  touch->touch_down_serial = serial;

  event = _create_touch_event (seat, touch, GDK_TOUCH_BEGIN, time);

  if (touch->initial_touch)
    {
      emulate_touch_crossing (touch->window, NULL,
                              seat->touch_master, seat->touch, touch,
                              GDK_ENTER_NOTIFY, GDK_CROSSING_NORMAL, time);
      GDK_WAYLAND_DEVICE(seat->touch_master)->emulating_touch = touch;
    }

  GDK_NOTE (EVENTS,
            g_message ("touch begin %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
touch_handle_up (void            *data,
                 struct wl_touch *wl_touch,
                 uint32_t         serial,
                 uint32_t         time,
                 int32_t          id)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  _gdk_wayland_display_update_serial (display, serial);

  touch = gdk_wayland_seat_get_touch (seat, id);
  event = _create_touch_event (seat, touch, GDK_TOUCH_END, time);

  GDK_NOTE (EVENTS,
            g_message ("touch end %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (seat->display, event);

  if (touch->initial_touch)
    {
      emulate_touch_crossing (touch->window, NULL,
                              seat->touch_master, seat->touch, touch,
                              GDK_LEAVE_NOTIFY, GDK_CROSSING_NORMAL, time);
      GDK_WAYLAND_DEVICE(seat->touch_master)->emulating_touch = NULL;
    }

  gdk_wayland_seat_remove_touch (seat, id);
}

static void
touch_handle_motion (void            *data,
                     struct wl_touch *wl_touch,
                     uint32_t         time,
                     int32_t          id,
                     wl_fixed_t       x,
                     wl_fixed_t       y)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  touch = gdk_wayland_seat_get_touch (seat, id);
  touch->x = wl_fixed_to_double (x);
  touch->y = wl_fixed_to_double (y);

  event = _create_touch_event (seat, touch, GDK_TOUCH_UPDATE, time);

  GDK_NOTE (EVENTS,
            g_message ("touch update %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
touch_handle_frame (void            *data,
                    struct wl_touch *wl_touch)
{
}

static void
touch_handle_cancel (void            *data,
                     struct wl_touch *wl_touch)
{
  GdkWaylandSeat *wayland_seat = data;
  GdkWaylandTouchData *touch;
  GHashTableIter iter;
  GdkEvent *event;

  if (GDK_WAYLAND_DEVICE (wayland_seat->touch_master)->emulating_touch)
    {
      touch = GDK_WAYLAND_DEVICE (wayland_seat->touch_master)->emulating_touch;
      GDK_WAYLAND_DEVICE (wayland_seat->touch_master)->emulating_touch = NULL;
      emulate_touch_crossing (touch->window, NULL,
                              wayland_seat->touch_master, wayland_seat->touch,
                              touch, GDK_LEAVE_NOTIFY, GDK_CROSSING_NORMAL,
                              GDK_CURRENT_TIME);
    }

  g_hash_table_iter_init (&iter, wayland_seat->touches);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
    {
      event = _create_touch_event (wayland_seat, touch, GDK_TOUCH_CANCEL,
                                   GDK_CURRENT_TIME);
      _gdk_wayland_display_deliver_event (wayland_seat->display, event);
      g_hash_table_iter_remove (&iter);
    }

  GDK_NOTE (EVENTS, g_message ("touch cancel"));
}

static void
emit_gesture_swipe_event (GdkWaylandSeat          *seat,
                          GdkTouchpadGesturePhase  phase,
                          guint32                  _time,
                          guint32                  n_fingers,
                          gdouble                  dx,
                          gdouble                  dy)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;

  if (!seat->pointer_focus)
    return;

  seat->time = _time;

  event = gdk_event_new (GDK_TOUCHPAD_SWIPE);
  event->touchpad_swipe.phase = phase;
  event->touchpad_swipe.window = g_object_ref (seat->pointer_focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, GDK_SEAT (seat));
  event->touchpad_swipe.time = _time;
  event->touchpad_swipe.state = seat->button_modifiers | seat->key_modifiers;
  gdk_event_set_screen (event, display->screen);
  event->touchpad_swipe.dx = dx;
  event->touchpad_swipe.dy = dy;
  event->touchpad_swipe.n_fingers = n_fingers;

  get_coordinates (seat,
                   &event->touchpad_swipe.x,
                   &event->touchpad_swipe.y,
                   &event->touchpad_swipe.x_root,
                   &event->touchpad_swipe.y_root);

  GDK_NOTE (EVENTS,
            g_message ("swipe event %d, coords: %f %f, seat %p state %d",
                       event->type, event->touchpad_swipe.x,
                       event->touchpad_swipe.y, seat,
                       event->touchpad_swipe.state));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
gesture_swipe_begin (void                                *data,
                     struct zwp_pointer_gesture_swipe_v1 *swipe,
                     uint32_t                             serial,
                     uint32_t                             time,
                     struct wl_surface                   *surface,
                     uint32_t                             fingers)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  _gdk_wayland_display_update_serial (display, serial);

  emit_gesture_swipe_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
                            time, fingers, 0, 0);
  seat->gesture_n_fingers = fingers;
}

static void
gesture_swipe_update (void                                *data,
                      struct zwp_pointer_gesture_swipe_v1 *swipe,
                      uint32_t                             time,
                      wl_fixed_t                           dx,
                      wl_fixed_t                           dy)
{
  GdkWaylandSeat *seat = data;

  emit_gesture_swipe_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_UPDATE,
                            time,
                            seat->gesture_n_fingers,
                            wl_fixed_to_double (dx),
                            wl_fixed_to_double (dy));
}

static void
gesture_swipe_end (void                                *data,
                   struct zwp_pointer_gesture_swipe_v1 *swipe,
                   uint32_t                             serial,
                   uint32_t                             time,
                   int32_t                              cancelled)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkTouchpadGesturePhase phase;

  _gdk_wayland_display_update_serial (display, serial);

  phase = (cancelled) ?
    GDK_TOUCHPAD_GESTURE_PHASE_CANCEL :
    GDK_TOUCHPAD_GESTURE_PHASE_END;

  emit_gesture_swipe_event (seat, phase, time,
                            seat->gesture_n_fingers, 0, 0);
}

static void
emit_gesture_pinch_event (GdkWaylandSeat          *seat,
                          GdkTouchpadGesturePhase  phase,
                          guint32                  _time,
                          guint                    n_fingers,
                          gdouble                  dx,
                          gdouble                  dy,
                          gdouble                  scale,
                          gdouble                  angle_delta)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkEvent *event;

  if (!seat->pointer_focus)
    return;

  seat->time = _time;

  event = gdk_event_new (GDK_TOUCHPAD_PINCH);
  event->touchpad_pinch.phase = phase;
  event->touchpad_pinch.window = g_object_ref (seat->pointer_focus);
  gdk_event_set_device (event, seat->master_pointer);
  gdk_event_set_source_device (event, seat->pointer);
  gdk_event_set_seat (event, gdk_device_get_seat (seat->master_pointer));
  event->touchpad_pinch.time = _time;
  event->touchpad_pinch.state = seat->button_modifiers | seat->key_modifiers;
  gdk_event_set_screen (event, display->screen);
  event->touchpad_pinch.dx = dx;
  event->touchpad_pinch.dy = dy;
  event->touchpad_pinch.scale = scale;
  event->touchpad_pinch.angle_delta = angle_delta * G_PI / 180;
  event->touchpad_pinch.n_fingers = n_fingers;

  get_coordinates (seat,
                   &event->touchpad_pinch.x,
                   &event->touchpad_pinch.y,
                   &event->touchpad_pinch.x_root,
                   &event->touchpad_pinch.y_root);

  GDK_NOTE (EVENTS,
            g_message ("pinch event %d, coords: %f %f, seat %p state %d",
                       event->type, event->touchpad_pinch.x,
                       event->touchpad_pinch.y, seat,
                       event->touchpad_pinch.state));

  _gdk_wayland_display_deliver_event (seat->display, event);
}

static void
gesture_pinch_begin (void                                *data,
                     struct zwp_pointer_gesture_pinch_v1 *pinch,
                     uint32_t                             serial,
                     uint32_t                             time,
                     struct wl_surface                   *surface,
                     uint32_t                             fingers)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);

  _gdk_wayland_display_update_serial (display, serial);
  emit_gesture_pinch_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_BEGIN,
                            time, fingers, 0, 0, 1, 0);
  seat->gesture_n_fingers = fingers;
}

static void
gesture_pinch_update (void                                *data,
                      struct zwp_pointer_gesture_pinch_v1 *pinch,
                      uint32_t                             time,
                      wl_fixed_t                           dx,
                      wl_fixed_t                           dy,
                      wl_fixed_t                           scale,
                      wl_fixed_t                           rotation)
{
  GdkWaylandSeat *seat = data;

  emit_gesture_pinch_event (seat,
                            GDK_TOUCHPAD_GESTURE_PHASE_UPDATE, time,
                            seat->gesture_n_fingers,
                            wl_fixed_to_double (dx),
                            wl_fixed_to_double (dy),
                            wl_fixed_to_double (scale),
                            wl_fixed_to_double (rotation));
}

static void
gesture_pinch_end (void                                *data,
                   struct zwp_pointer_gesture_pinch_v1 *pinch,
                   uint32_t                             serial,
                   uint32_t                             time,
                   int32_t                              cancelled)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (seat->display);
  GdkTouchpadGesturePhase phase;

  _gdk_wayland_display_update_serial (display, serial);

  phase = (cancelled) ?
    GDK_TOUCHPAD_GESTURE_PHASE_CANCEL :
    GDK_TOUCHPAD_GESTURE_PHASE_END;

  emit_gesture_pinch_event (seat, phase,
                            time, seat->gesture_n_fingers,
                            0, 0, 1, 0);
}

static const struct wl_pointer_listener pointer_listener = {
  pointer_handle_enter,
  pointer_handle_leave,
  pointer_handle_motion,
  pointer_handle_button,
  pointer_handle_axis,
  pointer_handle_frame,
  pointer_handle_axis_source,
  pointer_handle_axis_stop,
  pointer_handle_axis_discrete,
};

static const struct wl_keyboard_listener keyboard_listener = {
  keyboard_handle_keymap,
  keyboard_handle_enter,
  keyboard_handle_leave,
  keyboard_handle_key,
  keyboard_handle_modifiers,
  keyboard_handle_repeat_info,
};

static const struct wl_touch_listener touch_listener = {
  touch_handle_down,
  touch_handle_up,
  touch_handle_motion,
  touch_handle_frame,
  touch_handle_cancel
};

static const struct zwp_pointer_gesture_swipe_v1_listener gesture_swipe_listener = {
  gesture_swipe_begin,
  gesture_swipe_update,
  gesture_swipe_end
};

static const struct zwp_pointer_gesture_pinch_v1_listener gesture_pinch_listener = {
  gesture_pinch_begin,
  gesture_pinch_update,
  gesture_pinch_end
};

static void
seat_handle_capabilities (void                    *data,
                          struct wl_seat          *wl_seat,
                          enum wl_seat_capability  caps)
{
  GdkWaylandSeat *seat = data;
  GdkWaylandDeviceManager *device_manager = GDK_WAYLAND_DEVICE_MANAGER (seat->device_manager);
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (seat->display);

  GDK_NOTE (MISC,
            g_message ("seat %p with %s%s%s", wl_seat,
                       (caps & WL_SEAT_CAPABILITY_POINTER) ? " pointer, " : "",
                       (caps & WL_SEAT_CAPABILITY_KEYBOARD) ? " keyboard, " : "",
                       (caps & WL_SEAT_CAPABILITY_TOUCH) ? " touch" : ""));

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !seat->wl_pointer)
    {
      seat->wl_pointer = wl_seat_get_pointer (wl_seat);
      wl_pointer_set_user_data (seat->wl_pointer, seat);
      wl_pointer_add_listener (seat->wl_pointer, &pointer_listener, seat);

      seat->pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                    "name", "Wayland Pointer",
                                    "type", GDK_DEVICE_TYPE_SLAVE,
                                    "input-source", GDK_SOURCE_MOUSE,
                                    "input-mode", GDK_MODE_SCREEN,
                                    "has-cursor", TRUE,
                                    "display", seat->display,
                                    "device-manager", seat->device_manager,
                                    "seat", seat,
                                    NULL);
      _gdk_device_set_associated_device (seat->pointer, seat->master_pointer);

      device_manager->devices =
        g_list_prepend (device_manager->devices, seat->pointer);

      if (wayland_display->pointer_gestures)
        {
          seat->wp_pointer_gesture_swipe =
            zwp_pointer_gestures_v1_get_swipe_gesture (wayland_display->pointer_gestures,
                                                       seat->wl_pointer);
          zwp_pointer_gesture_swipe_v1_set_user_data (seat->wp_pointer_gesture_swipe,
                                                      seat);
          zwp_pointer_gesture_swipe_v1_add_listener (seat->wp_pointer_gesture_swipe,
                                                     &gesture_swipe_listener, seat);

          seat->wp_pointer_gesture_pinch =
            zwp_pointer_gestures_v1_get_pinch_gesture (wayland_display->pointer_gestures,
                                                       seat->wl_pointer);
          zwp_pointer_gesture_pinch_v1_set_user_data (seat->wp_pointer_gesture_pinch,
                                                      seat);
          zwp_pointer_gesture_pinch_v1_add_listener (seat->wp_pointer_gesture_pinch,
                                                     &gesture_pinch_listener, seat);
        }

      g_signal_emit_by_name (device_manager, "device-added", seat->pointer);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && seat->wl_pointer)
    {
      wl_pointer_release (seat->wl_pointer);
      seat->wl_pointer = NULL;
      _gdk_device_set_associated_device (seat->pointer, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, seat->pointer);

      g_signal_emit_by_name (device_manager, "device-removed", seat->pointer);
      g_object_unref (seat->pointer);
      seat->pointer = NULL;
    }

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !seat->wl_keyboard)
    {
      seat->wl_keyboard = wl_seat_get_keyboard (wl_seat);
      wl_keyboard_set_user_data (seat->wl_keyboard, seat);
      wl_keyboard_add_listener (seat->wl_keyboard, &keyboard_listener, seat);

      seat->keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                     "name", "Wayland Keyboard",
                                     "type", GDK_DEVICE_TYPE_SLAVE,
                                     "input-source", GDK_SOURCE_KEYBOARD,
                                     "input-mode", GDK_MODE_SCREEN,
                                     "has-cursor", FALSE,
                                     "display", seat->display,
                                     "device-manager", seat->device_manager,
                                     "seat", seat,
                                     NULL);
      _gdk_device_set_associated_device (seat->keyboard, seat->master_keyboard);

      device_manager->devices =
        g_list_prepend (device_manager->devices, seat->keyboard);

      g_signal_emit_by_name (device_manager, "device-added", seat->keyboard);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && seat->wl_keyboard)
    {
      wl_keyboard_release (seat->wl_keyboard);
      seat->wl_keyboard = NULL;
      _gdk_device_set_associated_device (seat->keyboard, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, seat->keyboard);

      g_signal_emit_by_name (device_manager, "device-removed", seat->keyboard);
      g_object_unref (seat->keyboard);
      seat->keyboard = NULL;
    }

  if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !seat->wl_touch)
    {
      seat->wl_touch = wl_seat_get_touch (wl_seat);
      wl_touch_set_user_data (seat->wl_touch, seat);
      wl_touch_add_listener (seat->wl_touch, &touch_listener, seat);

      seat->touch_master = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                         "name", "Wayland Touch Master Pointer",
                                         "type", GDK_DEVICE_TYPE_MASTER,
                                         "input-source", GDK_SOURCE_MOUSE,
                                         "input-mode", GDK_MODE_SCREEN,
                                         "has-cursor", TRUE,
                                         "display", seat->display,
                                         "device-manager", seat->device_manager,
                                         "seat", seat,
                                         NULL);
      _gdk_device_set_associated_device (seat->touch_master, seat->master_keyboard);

      device_manager->devices =
        g_list_prepend (device_manager->devices, seat->touch_master);
      g_signal_emit_by_name (device_manager, "device-added", seat->touch_master);

      seat->touch = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                  "name", "Wayland Touch",
                                  "type", GDK_DEVICE_TYPE_SLAVE,
                                  "input-source", GDK_SOURCE_TOUCHSCREEN,
                                  "input-mode", GDK_MODE_SCREEN,
                                  "has-cursor", FALSE,
                                  "display", seat->display,
                                  "device-manager", seat->device_manager,
                                  "seat", seat,
                                  NULL);
      _gdk_device_set_associated_device (seat->touch, seat->touch_master);

      device_manager->devices =
        g_list_prepend (device_manager->devices, seat->touch);

      g_signal_emit_by_name (device_manager, "device-added", seat->touch);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && seat->wl_touch)
    {
      wl_touch_release (seat->wl_touch);
      seat->wl_touch = NULL;
      _gdk_device_set_associated_device (seat->touch_master, NULL);
      _gdk_device_set_associated_device (seat->touch, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, seat->touch_master);
      device_manager->devices =
        g_list_remove (device_manager->devices, seat->touch);

      g_signal_emit_by_name (device_manager, "device-removed", seat->touch_master);
      g_signal_emit_by_name (device_manager, "device-removed", seat->touch);
      g_object_unref (seat->touch_master);
      g_object_unref (seat->touch);
      seat->touch_master = NULL;
      seat->touch = NULL;
    }

  if (seat->master_pointer)
    gdk_drag_context_set_device (seat->drop_context, seat->master_pointer);
  else if (seat->touch_master)
    gdk_drag_context_set_device (seat->drop_context, seat->touch_master);
}

static void
seat_handle_name (void           *data,
                  struct wl_seat *seat,
                  const char     *name)
{
  /* We don't care about the name. */
  GDK_NOTE (MISC,
            g_message ("seat %p name %s", seat, name));
}

static const struct wl_seat_listener seat_listener = {
  seat_handle_capabilities,
  seat_handle_name,
};

static void
init_devices (GdkWaylandSeat *seat)
{
  GdkWaylandDeviceManager *device_manager = GDK_WAYLAND_DEVICE_MANAGER (seat->device_manager);

  /* pointer */
  seat->master_pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                       "name", "Core Pointer",
                                       "type", GDK_DEVICE_TYPE_MASTER,
                                       "input-source", GDK_SOURCE_MOUSE,
                                       "input-mode", GDK_MODE_SCREEN,
                                       "has-cursor", TRUE,
                                       "display", seat->display,
                                       "device-manager", device_manager,
                                       "seat", seat,
                                       NULL);

  device_manager->devices =
    g_list_prepend (device_manager->devices, seat->master_pointer);
  g_signal_emit_by_name (device_manager, "device-added", seat->master_pointer);

  /* keyboard */
  seat->master_keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                        "name", "Core Keyboard",
                                        "type", GDK_DEVICE_TYPE_MASTER,
                                        "input-source", GDK_SOURCE_KEYBOARD,
                                        "input-mode", GDK_MODE_SCREEN,
                                        "has-cursor", FALSE,
                                        "display", seat->display,
                                        "device-manager", device_manager,
                                        "seat", seat,
                                        NULL);

  device_manager->devices =
    g_list_prepend (device_manager->devices, seat->master_keyboard);
  g_signal_emit_by_name (device_manager, "device-added", seat->master_keyboard);

  /* link both */
  _gdk_device_set_associated_device (seat->master_pointer, seat->master_keyboard);
  _gdk_device_set_associated_device (seat->master_keyboard, seat->master_pointer);
}

static void
pointer_surface_update_scale (GdkWaylandSeat *seat)
{
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (seat->display);
  guint32 scale;
  GSList *l;

  if (wayland_display->compositor_version < WL_SURFACE_HAS_BUFFER_SCALE)
    {
      /* We can't set the scale on this surface */
      return;
    }

  scale = 1;
  for (l = seat->pointer_surface_outputs; l != NULL; l = l->next)
    {
      guint32 output_scale =
        _gdk_wayland_screen_get_output_scale (wayland_display->screen,
                                              l->data);
      scale = MAX (scale, output_scale);
    }

  seat->current_output_scale = scale;

  if (seat->cursor)
    _gdk_wayland_cursor_set_scale (seat->cursor, scale);

  gdk_wayland_seat_update_window_cursor (seat);
}

static void
pointer_surface_enter (void              *data,
                       struct wl_surface *wl_surface,
                       struct wl_output  *output)

{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("pointer surface of seat %p entered output %p",
                       seat, output));

  seat->pointer_surface_outputs =
    g_slist_append (seat->pointer_surface_outputs, output);

  pointer_surface_update_scale (seat);
}

static void
pointer_surface_leave (void              *data,
                       struct wl_surface *wl_surface,
                       struct wl_output  *output)
{
  GdkWaylandSeat *seat = data;

  GDK_NOTE (EVENTS,
            g_message ("pointer surface of seat %p left output %p",
                       seat, output));

  seat->pointer_surface_outputs =
    g_slist_remove (seat->pointer_surface_outputs, output);

  pointer_surface_update_scale (seat);
}

static const struct wl_surface_listener pointer_surface_listener = {
  pointer_surface_enter,
  pointer_surface_leave
};

static GdkWindow *
create_foreign_dnd_window (GdkDisplay *display)
{
  GdkWindowAttr attrs;
  GdkScreen *screen;
  guint mask;

  screen = gdk_display_get_default_screen (display);

  attrs.x = attrs.y = 0;
  attrs.width = attrs.height = 1;
  attrs.wclass = GDK_INPUT_OUTPUT;
  attrs.window_type = GDK_WINDOW_TEMP;
  attrs.visual = gdk_screen_get_system_visual (screen);

  mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL;

  return gdk_window_new (gdk_screen_get_root_window (screen), &attrs, mask);
}

static void
gdk_wayland_seat_finalize (GObject *object)
{
  GdkWaylandSeat *seat = GDK_WAYLAND_SEAT (object);

  seat_handle_capabilities (seat, seat->wl_seat, 0);
  g_object_unref (seat->keymap);
  wl_surface_destroy (seat->pointer_surface);
  /* FIXME: destroy data_device */
  g_clear_object (&seat->keyboard_settings);
  g_clear_object (&seat->drop_context);
  g_hash_table_destroy (seat->touches);
  gdk_window_destroy (seat->foreign_dnd_window);
  stop_key_repeat (seat);

  G_OBJECT_CLASS (gdk_wayland_seat_parent_class)->finalize (object);
}

static GdkSeatCapabilities
gdk_wayland_seat_get_capabilities (GdkSeat *seat)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkSeatCapabilities caps = 0;

  if (wayland_seat->master_pointer)
    caps |= GDK_SEAT_CAPABILITY_POINTER;
  if (wayland_seat->master_keyboard)
    caps |= GDK_SEAT_CAPABILITY_KEYBOARD;
  if (wayland_seat->touch_master)
    caps |= GDK_SEAT_CAPABILITY_TOUCH;

  return caps;
}

static void
gdk_wayland_seat_set_grab_window (GdkWaylandSeat *seat,
                                  GdkWindow      *window)
{
  if (seat->pointer_grab_window)
    {
      _gdk_wayland_window_set_grab_seat (seat->pointer_grab_window, NULL);
      g_object_remove_weak_pointer (G_OBJECT (seat->pointer_grab_window),
                                    (gpointer *) &seat->pointer_grab_window);
      seat->pointer_grab_window = NULL;
    }

  if (window)
    {
      seat->pointer_grab_window = window;
      g_object_add_weak_pointer (G_OBJECT (window),
                                 (gpointer *) &seat->pointer_grab_window);
      _gdk_wayland_window_set_grab_seat (window, GDK_SEAT (seat));
    }
}

static GdkGrabStatus
gdk_wayland_seat_grab (GdkSeat                *seat,
                       GdkWindow              *window,
                       GdkSeatCapabilities     capabilities,
                       gboolean                owner_events,
                       GdkCursor              *cursor,
                       const GdkEvent         *event,
                       GdkSeatGrabPrepareFunc  prepare_func,
                       gpointer                prepare_func_data)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  guint32 evtime = event ? gdk_event_get_time (event) : GDK_CURRENT_TIME;
  GdkDisplay *display = gdk_seat_get_display (seat);
  GdkWindow *native;

  native = gdk_window_get_toplevel (window);

  while (native->window_type == GDK_WINDOW_OFFSCREEN)
    {
      native = gdk_offscreen_window_get_embedder (native);

      if (native == NULL ||
          (!_gdk_window_has_impl (native) &&
           !gdk_window_is_viewable (native)))
        return GDK_GRAB_NOT_VIEWABLE;

      native = gdk_window_get_toplevel (native);
    }

  if (native == NULL || GDK_WINDOW_DESTROYED (native))
    return GDK_GRAB_NOT_VIEWABLE;

  gdk_wayland_seat_set_grab_window (wayland_seat, native);
  wayland_seat->pointer_grab_time = evtime;

  if (prepare_func)
    (prepare_func) (seat, window, prepare_func_data);

  if (!gdk_window_is_visible (window))
    {
      gdk_wayland_seat_set_grab_window (wayland_seat, NULL);
      g_critical ("Window %p has not been made visible in GdkSeatGrabPrepareFunc",
                  window);
      return GDK_GRAB_NOT_VIEWABLE;
    }

  if (wayland_seat->master_pointer &&
      capabilities & GDK_SEAT_CAPABILITY_POINTER)
    {
      GdkWindow *prev_focus = gdk_wayland_device_get_focus (wayland_seat->master_pointer);

      if (prev_focus != native)
        device_emit_grab_crossing (wayland_seat->master_pointer, prev_focus,
                                   native, GDK_CROSSING_GRAB, evtime);

      _gdk_display_add_device_grab (display,
                                    wayland_seat->master_pointer,
                                    window,
                                    native,
                                    GDK_OWNERSHIP_NONE,
                                    owner_events,
                                    GDK_ALL_EVENTS_MASK,
                                    _gdk_display_get_next_serial (display),
                                    evtime,
                                    FALSE);

      gdk_wayland_seat_set_global_cursor (seat, cursor);
      g_set_object (&wayland_seat->cursor, cursor);
      gdk_wayland_seat_update_window_cursor (wayland_seat);
    }

  if (wayland_seat->touch_master &&
      capabilities & GDK_SEAT_CAPABILITY_TOUCH)
    {
      GdkWindow *prev_focus = gdk_wayland_device_get_focus (wayland_seat->touch_master);

      if (prev_focus != native)
        device_emit_grab_crossing (wayland_seat->touch_master, prev_focus,
                                   native, GDK_CROSSING_GRAB, evtime);

      _gdk_display_add_device_grab (display,
                                    wayland_seat->touch_master,
                                    window,
                                    native,
                                    GDK_OWNERSHIP_NONE,
                                    owner_events,
                                    GDK_ALL_EVENTS_MASK,
                                    _gdk_display_get_next_serial (display),
                                    evtime,
                                    FALSE);
    }

  if (wayland_seat->master_keyboard &&
      capabilities & GDK_SEAT_CAPABILITY_KEYBOARD)
    {
      GdkWindow *prev_focus = gdk_wayland_device_get_focus (wayland_seat->master_keyboard);

      if (prev_focus != native)
        device_emit_grab_crossing (wayland_seat->master_keyboard, prev_focus,
                                   native, GDK_CROSSING_GRAB, evtime);

      _gdk_display_add_device_grab (display,
                                    wayland_seat->master_keyboard,
                                    window,
                                    native,
                                    GDK_OWNERSHIP_NONE,
                                    owner_events,
                                    GDK_ALL_EVENTS_MASK,
                                    _gdk_display_get_next_serial (display),
                                    evtime,
                                    FALSE);
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_wayland_seat_ungrab (GdkSeat *seat)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkDisplay *display = gdk_seat_get_display (seat);
  GdkDeviceGrabInfo *grab;

  g_clear_object (&wayland_seat->grab_cursor);

  gdk_wayland_seat_set_grab_window (wayland_seat, NULL);

  if (wayland_seat->master_pointer)
    {
      GdkWindow *focus, *prev_focus = NULL;

      grab = _gdk_display_get_last_device_grab (display, wayland_seat->master_pointer);

      if (grab)
        {
          grab->serial_end = grab->serial_start;
          prev_focus = grab->window;
        }

      focus = gdk_wayland_device_get_focus (wayland_seat->master_pointer);

      if (focus != prev_focus)
        device_emit_grab_crossing (wayland_seat->master_pointer, prev_focus,
                                   focus, GDK_CROSSING_UNGRAB,
                                   GDK_CURRENT_TIME);

      gdk_wayland_seat_update_window_cursor (wayland_seat);
    }

  if (wayland_seat->master_keyboard)
    {
      grab = _gdk_display_get_last_device_grab (display, wayland_seat->master_keyboard);

      if (grab)
        grab->serial_end = grab->serial_start;
    }

  if (wayland_seat->touch_master)
    {
      grab = _gdk_display_get_last_device_grab (display, wayland_seat->touch_master);

      if (grab)
        grab->serial_end = grab->serial_start;
    }
}

static GdkDevice *
gdk_wayland_seat_get_master (GdkSeat             *seat,
                             GdkSeatCapabilities  capabilities)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);

  if (capabilities == GDK_SEAT_CAPABILITY_POINTER)
    return wayland_seat->master_pointer;
  else if (capabilities == GDK_SEAT_CAPABILITY_KEYBOARD)
    return wayland_seat->master_keyboard;
  else if (capabilities == GDK_SEAT_CAPABILITY_TOUCH)
    return wayland_seat->touch_master;

  return NULL;
}

static GList *
gdk_wayland_seat_get_slaves (GdkSeat             *seat,
                             GdkSeatCapabilities  capabilities)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GList *slaves = NULL;

  if (wayland_seat->pointer && (capabilities & GDK_SEAT_CAPABILITY_POINTER))
    slaves = g_list_prepend (slaves, wayland_seat->pointer);
  if (wayland_seat->keyboard && (capabilities & GDK_SEAT_CAPABILITY_KEYBOARD))
    slaves = g_list_prepend (slaves, wayland_seat->keyboard);
  if (wayland_seat->touch && (capabilities & GDK_SEAT_CAPABILITY_TOUCH))
    slaves = g_list_prepend (slaves, wayland_seat->touch);

  return slaves;
}

static void
gdk_wayland_seat_class_init (GdkWaylandSeatClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GdkSeatClass *seat_class = GDK_SEAT_CLASS (klass);

  object_class->finalize = gdk_wayland_seat_finalize;

  seat_class->get_capabilities = gdk_wayland_seat_get_capabilities;
  seat_class->grab = gdk_wayland_seat_grab;
  seat_class->ungrab = gdk_wayland_seat_ungrab;
  seat_class->get_master = gdk_wayland_seat_get_master;
  seat_class->get_slaves = gdk_wayland_seat_get_slaves;
}

static void
gdk_wayland_seat_init (GdkWaylandSeat *seat)
{
}

void
_gdk_wayland_device_manager_add_seat (GdkDeviceManager *device_manager,
                                      guint32           id,
				      struct wl_seat   *wl_seat)
{
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;
  GdkWaylandSeat *seat;

  display = gdk_device_manager_get_display (device_manager);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  seat = g_object_new (GDK_TYPE_WAYLAND_SEAT,
                       "display", display,
                       NULL);
  seat->id = id;
  seat->keymap = _gdk_wayland_keymap_new ();
  seat->display = display;
  seat->device_manager = device_manager;
  seat->touches = g_hash_table_new_full (NULL, NULL, NULL,
                                         (GDestroyNotify) g_free);
  seat->foreign_dnd_window = create_foreign_dnd_window (display);
  seat->wl_seat = wl_seat;

  seat->pending_selection = GDK_NONE;

  wl_seat_add_listener (seat->wl_seat, &seat_listener, seat);
  wl_seat_set_user_data (seat->wl_seat, seat);

  if (display_wayland->primary_selection_manager)
    {
      seat->primary_data_device =
        gtk_primary_selection_device_manager_get_device (display_wayland->primary_selection_manager,
                                                         seat->wl_seat);
      gtk_primary_selection_device_add_listener (seat->primary_data_device,
                                                 &primary_selection_device_listener, seat);
    }

  seat->data_device =
    wl_data_device_manager_get_data_device (display_wayland->data_device_manager,
                                            seat->wl_seat);
  seat->drop_context = _gdk_wayland_drop_context_new (seat->data_device);
  wl_data_device_add_listener (seat->data_device,
                               &data_device_listener, seat);

  seat->current_output_scale = 1;
  seat->pointer_surface =
    wl_compositor_create_surface (display_wayland->compositor);
  wl_surface_add_listener (seat->pointer_surface,
                           &pointer_surface_listener,
                           seat);

  init_devices (seat);

  gdk_display_add_seat (display, GDK_SEAT (seat));
}

void
_gdk_wayland_device_manager_remove_seat (GdkDeviceManager *manager,
                                         guint32           id)
{
  GdkDisplay *display = gdk_device_manager_get_display (manager);
  GList *l, *seats;

  seats = gdk_display_list_seats (display);

  for (l = seats; l != NULL; l = l->next)
    {
      GdkWaylandSeat *seat = l->data;

      if (seat->id != id)
        continue;

      gdk_display_remove_seat (display, GDK_SEAT (seat));
      break;
    }

  g_list_free (seats);
}

static void
free_device (gpointer data)
{
  g_object_unref (data);
}

static void
gdk_wayland_device_manager_finalize (GObject *object)
{
  GdkWaylandDeviceManager *device_manager;

  device_manager = GDK_WAYLAND_DEVICE_MANAGER (object);

  g_list_free_full (device_manager->devices, free_device);

  G_OBJECT_CLASS (gdk_wayland_device_manager_parent_class)->finalize (object);
}

static GList *
gdk_wayland_device_manager_list_devices (GdkDeviceManager *device_manager,
                                         GdkDeviceType     type)
{
  GdkWaylandDeviceManager *wayland_device_manager;
  GList *devices = NULL, *l;

  wayland_device_manager = GDK_WAYLAND_DEVICE_MANAGER (device_manager);

  for (l = wayland_device_manager->devices; l; l = l->next)
    {
      if (gdk_device_get_device_type (l->data) == type)
        devices = g_list_prepend (devices, l->data);
    }

  return devices;
}

static GdkDevice *
gdk_wayland_device_manager_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkWaylandDeviceManager *wayland_device_manager;
  GdkWaylandSeat *seat;
  GdkDevice *device;

  wayland_device_manager = GDK_WAYLAND_DEVICE_MANAGER (device_manager);

  /* Find the master pointer of the first GdkWaylandSeat we find */
  device = wayland_device_manager->devices->data;
  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (device));

  return seat->master_pointer;
}

static void
gdk_wayland_device_manager_class_init (GdkWaylandDeviceManagerClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_wayland_device_manager_finalize;
  device_manager_class->list_devices = gdk_wayland_device_manager_list_devices;
  device_manager_class->get_client_pointer = gdk_wayland_device_manager_get_client_pointer;
}

static void
gdk_wayland_device_manager_init (GdkWaylandDeviceManager *device_manager)
{
}

GdkDeviceManager *
_gdk_wayland_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_WAYLAND_DEVICE_MANAGER,
                       "display", display,
                       NULL);
}

uint32_t
_gdk_wayland_device_get_implicit_grab_serial (GdkWaylandDevice *device,
                                              const GdkEvent   *event)
{
  GdkSeat *seat = gdk_device_get_seat (GDK_DEVICE (device));
  GdkEventSequence *sequence = NULL;
  GdkWaylandTouchData *touch = NULL;

  if (event)
    sequence = gdk_event_get_event_sequence (event);

  if (sequence)
    touch = gdk_wayland_seat_get_touch (GDK_WAYLAND_SEAT (seat),
                                        GDK_EVENT_SEQUENCE_TO_SLOT (sequence));
  if (touch)
    return touch->touch_down_serial;
  else
    return GDK_WAYLAND_SEAT (seat)->button_press_serial;
}

uint32_t
_gdk_wayland_seat_get_last_implicit_grab_serial (GdkSeat           *seat,
                                                 GdkEventSequence **sequence)
{
  GdkWaylandSeat *wayland_seat;
  GdkWaylandTouchData *touch;
  GHashTableIter iter;
  uint32_t serial = 0;

  wayland_seat = GDK_WAYLAND_SEAT (seat);
  g_hash_table_iter_init (&iter, wayland_seat->touches);

  if (sequence)
    *sequence = NULL;

  if (wayland_seat->button_press_serial > serial)
    serial = wayland_seat->button_press_serial;

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
    {
      if (touch->touch_down_serial > serial)
        {
          if (sequence)
            *sequence = GDK_SLOT_TO_EVENT_SEQUENCE (touch->id);
          serial = touch->touch_down_serial;
        }
    }

  return serial;
}

void
gdk_wayland_device_unset_touch_grab (GdkDevice        *gdk_device,
                                     GdkEventSequence *sequence)
{
  GdkWaylandSeat *seat;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  g_return_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device));

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  touch = gdk_wayland_seat_get_touch (seat,
                                      GDK_EVENT_SEQUENCE_TO_SLOT (sequence));

  if (GDK_WAYLAND_DEVICE (seat->touch_master)->emulating_touch == touch)
    {
      GDK_WAYLAND_DEVICE (seat->touch_master)->emulating_touch = NULL;
      emulate_touch_crossing (touch->window, NULL,
                              seat->touch_master, seat->touch,
                              touch, GDK_LEAVE_NOTIFY, GDK_CROSSING_NORMAL,
                              GDK_CURRENT_TIME);
    }

  event = _create_touch_event (seat, touch, GDK_TOUCH_CANCEL,
                               GDK_CURRENT_TIME);
  _gdk_wayland_display_deliver_event (seat->display, event);
}

void
gdk_wayland_seat_set_global_cursor (GdkSeat   *seat,
                                    GdkCursor *cursor)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkDevice *pointer;

  pointer = gdk_seat_get_pointer (seat);

  g_set_object (&wayland_seat->grab_cursor, cursor);
  gdk_wayland_device_set_window_cursor (pointer,
                                        gdk_wayland_device_get_focus (pointer),
                                        NULL);
}

struct wl_data_device *
gdk_wayland_device_get_data_device (GdkDevice *gdk_device)
{
  GdkWaylandSeat *seat;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device), NULL);

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  return seat->data_device;
}

void
gdk_wayland_device_set_selection (GdkDevice             *gdk_device,
                                  struct wl_data_source *source)
{
  GdkWaylandSeat *seat;
  GdkWaylandDisplay *display_wayland;

  g_return_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device));

  seat = GDK_WAYLAND_SEAT (gdk_device_get_seat (gdk_device));
  display_wayland = GDK_WAYLAND_DISPLAY (seat->display);

  wl_data_device_set_selection (seat->data_device, source,
                                _gdk_wayland_display_get_serial (display_wayland));
}

void
gdk_wayland_seat_set_primary (GdkSeat                             *seat,
                              struct gtk_primary_selection_source *source)
{
  GdkWaylandSeat *wayland_seat = GDK_WAYLAND_SEAT (seat);
  GdkWaylandDisplay *display_wayland;
  guint32 serial;

  if (source)
    {
      display_wayland = GDK_WAYLAND_DISPLAY (gdk_seat_get_display (seat));
      serial = _gdk_wayland_display_get_serial (display_wayland);
      gtk_primary_selection_device_set_selection (wayland_seat->primary_data_device,
                                                  source, serial);
    }
}

/**
 * gdk_wayland_seat_get_wl_seat:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_seat of a #GdkSeat.
 *
 * Returns: (transfer none): a Wayland wl_seat
 *
 * Since: 3.20
 */
struct wl_seat *
gdk_wayland_seat_get_wl_seat (GdkSeat *seat)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_SEAT (seat), NULL);

  return GDK_WAYLAND_SEAT (seat)->wl_seat;
}

GdkDragContext *
gdk_wayland_device_get_drop_context (GdkDevice *device)
{
  GdkSeat *seat = gdk_device_get_seat (device);

  return GDK_WAYLAND_SEAT (seat)->drop_context;
}
