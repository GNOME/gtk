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

#define _GNU_SOURCE
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include "config.h"

#include <string.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdktypes.h>
#include "gdkprivate-wayland.h"
#include "gdkwayland.h"
#include "gdkkeysyms.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanagerprivate.h"

#include <xkbcommon/xkbcommon.h>

#include <sys/time.h>
#include <sys/mman.h>

typedef struct _DataOffer DataOffer;

typedef struct _GdkWaylandSelectionOffer GdkWaylandSelectionOffer;

typedef struct _GdkWaylandTouchData GdkWaylandTouchData;

struct _GdkWaylandTouchData
{
  uint32_t id;
  gdouble x;
  gdouble y;
  GdkWindow *window;
  guint initial_touch : 1;
};

struct _GdkWaylandDeviceData
{
  guint32 id;
  struct wl_seat *wl_seat;
  struct wl_pointer *wl_pointer;
  struct wl_keyboard *wl_keyboard;
  struct wl_touch *wl_touch;

  GdkDisplay *display;
  GdkDeviceManager *device_manager;

  GdkDevice *master_pointer;
  GdkDevice *master_keyboard;
  GdkDevice *pointer;
  GdkDevice *keyboard;
  GdkDevice *touch;
  GdkCursor *cursor;
  GdkKeymap *keymap;

  GHashTable *touches;

  GdkModifierType modifiers;
  GdkWindow *pointer_focus;
  GdkWindow *keyboard_focus;
  struct wl_data_device *data_device;
  double surface_x, surface_y;
  uint32_t time;
  uint32_t enter_serial;
  uint32_t button_press_serial;
  GdkWindow *pointer_grab_window;
  uint32_t pointer_grab_time;
  guint32 repeat_timer;
  guint32 repeat_key;
  guint32 repeat_count;
  GSettings *keyboard_settings;

  guint cursor_timeout_id;
  guint cursor_image_index;

  DataOffer *drag_offer;
  DataOffer *selection_offer;

  GdkWaylandSelectionOffer *selection_offer_out;

  struct wl_surface *pointer_surface;
};

struct _GdkWaylandDevice
{
  GdkDevice parent_instance;
  GdkWaylandDeviceData *device;
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
gdk_wayland_device_stop_window_cursor_animation (GdkWaylandDeviceData *wd)
{
  if (wd->cursor_timeout_id > 0)
    {
      g_source_remove (wd->cursor_timeout_id);
      wd->cursor_timeout_id = 0;
    }
  wd->cursor_image_index = 0;
}

static gboolean
gdk_wayland_device_update_window_cursor (GdkWaylandDeviceData *wd)
{
  struct wl_buffer *buffer;
  int x, y, w, h, scale;
  guint next_image_index, next_image_delay;

  buffer = _gdk_wayland_cursor_get_buffer (wd->cursor, wd->cursor_image_index,
                                           &x, &y, &w, &h, &scale);

  if (!wd->wl_pointer)
    return FALSE;

  wl_pointer_set_cursor (wd->wl_pointer,
                         wd->enter_serial,
                         wd->pointer_surface,
                         x, y);
  wl_surface_attach (wd->pointer_surface, buffer, 0, 0);
  wl_surface_set_buffer_scale (wd->pointer_surface, scale);
  wl_surface_damage (wd->pointer_surface,  0, 0, w, h);
  wl_surface_commit (wd->pointer_surface);

  next_image_index =
    _gdk_wayland_cursor_get_next_image_index (wd->cursor,
                                              wd->cursor_image_index,
                                              &next_image_delay);

  if (next_image_index != wd->cursor_image_index)
    {
      guint id;

      /* Queue timeout for next frame */
      id = g_timeout_add (next_image_delay,
                          (GSourceFunc)gdk_wayland_device_update_window_cursor,
                          wd);
      g_source_set_name_by_id (id, "[gtk+] gdk_wayland_device_update_window_cursor");

      wd->cursor_timeout_id = id;
      wd->cursor_image_index = next_image_index;
    }
  else
    wd->cursor_timeout_id = 0;

  return FALSE;
}

static void
gdk_wayland_device_set_window_cursor (GdkDevice *device,
                                      GdkWindow *window,
                                      GdkCursor *cursor)
{
  GdkWaylandDeviceData *wd = GDK_WAYLAND_DEVICE (device)->device;

  /* Setting the cursor to NULL means that we should use
   * the default cursor
   */
  if (!cursor)
    {
      /* FIXME: Is this the best sensible default ? */
      cursor = _gdk_wayland_display_get_cursor_for_type (device->display,
                                                         GDK_LEFT_PTR);
    }

  if (cursor == wd->cursor)
    return;

  gdk_wayland_device_stop_window_cursor_animation (wd);

  if (wd->cursor)
    g_object_unref (wd->cursor);

  wd->cursor = g_object_ref (cursor);

  gdk_wayland_device_update_window_cursor (wd);
}

static void
gdk_wayland_device_warp (GdkDevice *device,
                         GdkScreen *screen,
                         gdouble    x,
                         gdouble    y)
{
}

static void
get_coordinates (GdkWaylandDeviceData *data,
                 double *x, double *y,
                 double *x_root, double *y_root)
{
  int root_x, root_y;

  if (x)
    *x = data->surface_x;
  if (y)
    *y = data->surface_y;

  if (data->pointer_focus)
    {
      gdk_window_get_root_coords (data->pointer_focus,
                                  data->surface_x,
                                  data->surface_y,
                                  &root_x, &root_y);
    }
  else
    {
      root_x = data->surface_x;
      root_y = data->surface_y;
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
  GdkWaylandDeviceData *wd;
  GdkScreen *default_screen;

  wd = GDK_WAYLAND_DEVICE (device)->device;
  default_screen = gdk_display_get_default_screen (wd->display);

  if (root_window)
    *root_window = gdk_screen_get_root_window (default_screen);
  if (child_window)
    *child_window = wd->pointer_focus;
  if (mask)
    *mask = wd->modifiers;

  get_coordinates (wd, win_x, win_y, root_x, root_y);
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
  GdkWaylandDeviceData *wayland_device = GDK_WAYLAND_DEVICE (device)->device;

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
      return GDK_GRAB_SUCCESS;
    }
  else
    {
      /* Device is a pointer */

      if (wayland_device->pointer_grab_window != NULL &&
          time_ != 0 && wayland_device->pointer_grab_time > time_)
        {
          return GDK_GRAB_ALREADY_GRABBED;
        }

      if (time_ == 0)
        time_ = wayland_device->time;

      wayland_device->pointer_grab_window = window;
      wayland_device->pointer_grab_time = time_;

      /* FIXME: This probably breaks if you end up with multiple grabs
       * on the same window - but we need to know the input device for
       * when we are asked to map a popup window so that the grab can
       * be managed by the compositor.
       */
      _gdk_wayland_window_set_device_grabbed (window,
                                              device,
                                              wayland_device->wl_seat,
                                              time_);
    }

  return GDK_GRAB_SUCCESS;
}

static void
gdk_wayland_device_ungrab (GdkDevice *device,
                           guint32    time_)
{
  GdkWaylandDeviceData *wayland_device = GDK_WAYLAND_DEVICE (device)->device;
  GdkDisplay *display;
  GdkDeviceGrabInfo *grab;

  display = gdk_device_get_display (device);

  if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
    {
      /* Device is a keyboard */
    }
  else
    {
      /* Device is a pointer */
      grab = _gdk_display_get_last_device_grab (display, device);

      if (grab)
        grab->serial_end = grab->serial_start;

      if (wayland_device->pointer_grab_window)
        _gdk_wayland_window_set_device_grabbed (wayland_device->pointer_grab_window,
                                                NULL,
                                                NULL,
                                                0);
    }
}

static GdkWindow *
gdk_wayland_device_window_at_position (GdkDevice       *device,
                                       gdouble         *win_x,
                                       gdouble         *win_y,
                                       GdkModifierType *mask,
                                       gboolean         get_toplevel)
{
  GdkWaylandDeviceData *wd;

  wd = GDK_WAYLAND_DEVICE(device)->device;
  if (win_x)
    *win_x = wd->surface_x;
  if (win_y)
    *win_y = wd->surface_y;
  if (mask)
    *mask = wd->modifiers;

  return wd->pointer_focus;
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
 * Returns the Wayland wl_seat of a #GdkDevice
 *
 * Returns: (transfer none): a Wayland wl_seat
 *
 * Since: 3.10
 */
struct wl_seat *
gdk_wayland_device_get_wl_seat (GdkDevice *device)
{
  g_return_val_if_fail(GDK_IS_WAYLAND_DEVICE (device), NULL);

  return GDK_WAYLAND_DEVICE (device)->device->wl_seat;
}

/**
 * gdk_wayland_device_get_wl_pointer:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_pointer of a #GdkDevice
 *
 * Returns: (transfer none): a Wayland wl_pointer
 *
 * Since: 3.10
 */
struct wl_pointer *
gdk_wayland_device_get_wl_pointer (GdkDevice *device)
{
  g_return_val_if_fail(GDK_IS_WAYLAND_DEVICE (device), NULL);

  return GDK_WAYLAND_DEVICE (device)->device->wl_pointer;
}

/**
 * gdk_wayland_device_get_wl_keyboard:
 * @device: (type GdkWaylandDevice): a #GdkDevice
 *
 * Returns the Wayland wl_keyboard of a #GdkDevice
 *
 * Returns: (transfer none): a Wayland wl_keyboard
 *
 * Since: 3.10
 */
struct wl_keyboard *
gdk_wayland_device_get_wl_keyboard (GdkDevice *device)
{
  g_return_val_if_fail(GDK_IS_WAYLAND_DEVICE (device), NULL);

  return GDK_WAYLAND_DEVICE (device)->device->wl_keyboard;
}

GdkKeymap *
_gdk_wayland_device_get_keymap (GdkDevice *device)
{
  return GDK_WAYLAND_DEVICE (device)->device->keymap;
}

struct _DataOffer {
  struct wl_data_offer *offer;
  gint ref_count;
  GPtrArray *types;
};

static void
data_offer_offer (void                 *data,
                  struct wl_data_offer *wl_data_offer,
                  const char           *type)
{
  DataOffer *offer = (DataOffer *)data;
  g_debug (G_STRLOC ": %s wl_data_offer = %p type = %s",
           G_STRFUNC, wl_data_offer, type);

  g_ptr_array_add (offer->types, g_strdup (type));
}

static void
data_offer_unref (DataOffer *offer)
{
  offer->ref_count--;

  if (offer->ref_count == 0)
    {
      g_ptr_array_free (offer->types, TRUE);
      g_free (offer);
    }
}

static const struct wl_data_offer_listener data_offer_listener = {
  data_offer_offer,
};

static void
data_device_data_offer (void                  *data,
                        struct wl_data_device *data_device,
                        struct wl_data_offer  *_offer)
{
  DataOffer *offer;

  /* This structure is reference counted to handle the case where you get a
   * leave but are in the middle of transferring data
   */
  offer = g_new0 (DataOffer, 1);
  offer->ref_count = 1;
  offer->types = g_ptr_array_new_with_free_func (g_free);
  offer->offer = _offer;

  /* The DataOffer structure is then retrieved later since this sets the user
   * data.
   */
  wl_data_offer_add_listener (offer->offer,
                              &data_offer_listener,
                              offer);
}

static void
data_device_enter (void                  *data,
                   struct wl_data_device *data_device,
                   uint32_t               serial,
                   struct wl_surface     *surface,
                   int32_t                x,
                   int32_t                y,
                   struct wl_data_offer  *offer)
{
  GdkWaylandDeviceData *device = (GdkWaylandDeviceData *)data;

  g_debug (G_STRLOC ": %s data_device = %p serial = %u, surface = %p, x = %d y = %d, offer = %p",
           G_STRFUNC, data_device, serial, surface, x, y, offer);

  /* Retrieve the DataOffer associated with with the wl_data_offer - this
   * association is made when the listener is attached.
   */
  g_assert (device->drag_offer == NULL);
  device->drag_offer = wl_data_offer_get_user_data (offer);
}

static void
data_device_leave (void                  *data,
                   struct wl_data_device *data_device)
{
  GdkWaylandDeviceData *device = (GdkWaylandDeviceData *)data;

  g_debug (G_STRLOC ": %s data_device = %p",
           G_STRFUNC, data_device);

  data_offer_unref (device->drag_offer);
  device->drag_offer = NULL;
}

static void
data_device_motion (void                  *data,
                    struct wl_data_device *data_device,
                    uint32_t               time,
                    int32_t                x,
                    int32_t                y)
{
  g_debug (G_STRLOC ": %s data_device = %p, time = %d, x = %d, y = %d",
           G_STRFUNC, data_device, time, x, y);
}

static void
data_device_drop (void                  *data,
                  struct wl_data_device *data_device)
{
  g_debug (G_STRLOC ": %s data_device = %p",
           G_STRFUNC, data_device);
}

static void
data_device_selection (void                  *data,
                       struct wl_data_device *wl_data_device,
                       struct wl_data_offer  *offer)
{
  GdkWaylandDeviceData *device = (GdkWaylandDeviceData *)data;

  g_debug (G_STRLOC ": %s wl_data_device = %p wl_data_offer = %p",
           G_STRFUNC, wl_data_device, offer);

  if (!offer)
    {
      if (device->selection_offer)
        {
          data_offer_unref (device->selection_offer);
          device->selection_offer = NULL;
        }

      return;
    }

  if (device->selection_offer)
    {
      data_offer_unref (device->selection_offer);
      device->selection_offer = NULL;
    }

  /* Retrieve the DataOffer associated with with the wl_data_offer -
   * this association is made when the listener is attached.
   */
  g_assert (device->selection_offer == NULL);
  device->selection_offer = wl_data_offer_get_user_data (offer);
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
pointer_handle_enter (void              *data,
                      struct wl_pointer *pointer,
                      uint32_t           serial,
                      struct wl_surface *surface,
                      wl_fixed_t         sx,
                      wl_fixed_t         sy)
{
  GdkWaylandDeviceData *device = data;
  GdkEvent *event;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (device->display);

  if (!surface)
    return;
  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  device->pointer_focus = wl_surface_get_user_data(surface);
  g_object_ref(device->pointer_focus);

  device->surface_x = wl_fixed_to_double (sx);
  device->surface_y = wl_fixed_to_double (sy);
  device->enter_serial = serial;

  event = gdk_event_new (GDK_ENTER_NOTIFY);
  event->crossing.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->crossing.subwindow = NULL;
  event->crossing.time = (guint32)(g_get_monotonic_time () / 1000);
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  event->crossing.focus = TRUE;
  event->crossing.state = 0;

  get_coordinates (device,
                   &event->crossing.x,
                   &event->crossing.y,
                   &event->crossing.x_root,
                   &event->crossing.y_root);

  _gdk_wayland_display_deliver_event (device->display, event);

  GDK_NOTE (EVENTS,
            g_message ("enter, device %p surface %p",
                       device, device->pointer_focus));
}

static void
pointer_handle_leave (void              *data,
                      struct wl_pointer *pointer,
                      uint32_t           serial,
                      struct wl_surface *surface)
{
  GdkWaylandDeviceData *device = data;
  GdkEvent *event;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (device->display);

  if (!surface)
    return;
  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;
  if (!device->pointer_focus)
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  event = gdk_event_new (GDK_LEAVE_NOTIFY);
  event->crossing.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->crossing.subwindow = NULL;
  event->crossing.time = (guint32)(g_get_monotonic_time () / 1000);
  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  event->crossing.focus = TRUE;
  event->crossing.state = 0;

  get_coordinates (device,
                   &event->crossing.x,
                   &event->crossing.y,
                   &event->crossing.x_root,
                   &event->crossing.y_root);

  _gdk_wayland_display_deliver_event (device->display, event);

  GDK_NOTE (EVENTS,
            g_message ("leave, device %p surface %p",
                       device, device->pointer_focus));

  g_object_unref(device->pointer_focus);
  if (device->cursor)
    {
      gdk_wayland_device_stop_window_cursor_animation (device);
      g_object_unref (device->cursor);
      device->cursor = NULL;
    }

  device->pointer_focus = NULL;
}

static void
pointer_handle_motion (void              *data,
                       struct wl_pointer *pointer,
                       uint32_t           time,
                       wl_fixed_t         sx,
                       wl_fixed_t         sy)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkEvent *event;

  if (!device->pointer_focus)
    return;

  event = gdk_event_new (GDK_NOTHING);

  device->time = time;
  device->surface_x = wl_fixed_to_double (sx);
  device->surface_y = wl_fixed_to_double (sy);

  event->motion.type = GDK_MOTION_NOTIFY;
  event->motion.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->motion.time = time;
  event->motion.axes = NULL;
  event->motion.state = device->modifiers;
  event->motion.is_hint = 0;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (device,
                   &event->motion.x,
                   &event->motion.y,
                   &event->motion.x_root,
                   &event->motion.y_root);

  GDK_NOTE (EVENTS,
            g_message ("motion %d %d, state %d",
                       sx, sy, event->button.state));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
pointer_handle_button (void              *data,
                       struct wl_pointer *pointer,
                       uint32_t           serial,
                       uint32_t           time,
                       uint32_t           button,
                       uint32_t           state)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkEvent *event;
  uint32_t modifier;
  int gdk_button;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (device->display);

  if (!device->pointer_focus)
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  switch (button) {
  case 273:
    gdk_button = 3;
    break;
  case 274:
    gdk_button = 2;
    break;
  default:
    gdk_button = button - 271;
    break;
  }

  device->time = time;
  if (state)
    device->button_press_serial = serial;

  event = gdk_event_new (state ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE);
  event->button.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->button.time = time;
  event->button.axes = NULL;
  event->button.state = device->modifiers;
  event->button.button = gdk_button;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (device,
                   &event->button.x,
                   &event->button.y,
                   &event->button.x_root,
                   &event->button.y_root);

  modifier = 1 << (8 + gdk_button - 1);
  if (state)
    device->modifiers |= modifier;
  else
    device->modifiers &= ~modifier;

  GDK_NOTE (EVENTS,
	    g_message ("button %d %s, state %d",
		       event->button.button,
		       state ? "press" : "release", event->button.state));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
pointer_handle_axis (void              *data,
                     struct wl_pointer *pointer,
                     uint32_t           time,
                     uint32_t           axis,
                     wl_fixed_t         value)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  GdkEvent *event;
  gdouble delta_x, delta_y;

  if (!device->pointer_focus)
    return;

  /* get the delta and convert it into the expected range */
  switch (axis) {
  case WL_POINTER_AXIS_VERTICAL_SCROLL:
    delta_x = 0;
    delta_y = wl_fixed_to_double (value) / 10.0;
    break;
  case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
    delta_x = wl_fixed_to_double (value) / 10.0;
    delta_y = 0;
    break;
  default:
    g_return_if_reached ();
  }

  device->time = time;
  event = gdk_event_new (GDK_SCROLL);
  event->scroll.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->pointer);
  event->scroll.time = time;
  event->scroll.direction = GDK_SCROLL_SMOOTH;
  event->scroll.delta_x = delta_x;
  event->scroll.delta_y = delta_y;
  event->scroll.state = device->modifiers;
  gdk_event_set_screen (event, display->screen);

  get_coordinates (device,
                   &event->scroll.x,
                   &event->scroll.y,
                   &event->scroll.x_root,
                   &event->scroll.y_root);

  GDK_NOTE (EVENTS,
            g_message ("scroll %f %f",
                       event->scroll.delta_x, event->scroll.delta_y));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
keyboard_handle_keymap (void               *data,
                        struct wl_keyboard *keyboard,
                        uint32_t            format,
                        int                 fd,
                        uint32_t            size)
{
  GdkWaylandDeviceData *device = data;

  _gdk_wayland_keymap_update_from_fd (device->keymap, format, fd, size);

  g_signal_emit_by_name (device->keymap, "keys-changed");
  g_signal_emit_by_name (device->keymap, "state-changed");
  g_signal_emit_by_name (device->keymap, "direction-changed");
}

static void
keyboard_handle_enter (void               *data,
                       struct wl_keyboard *keyboard,
                       uint32_t            serial,
                       struct wl_surface  *surface,
                       struct wl_array    *keys)
{
  GdkWaylandDeviceData *device = data;
  GdkEvent *event;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (device->display);

  if (!surface)
    return;
  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  device->keyboard_focus = wl_surface_get_user_data(surface);
  g_object_ref(device->keyboard_focus);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (device->keyboard_focus);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = TRUE;
  gdk_event_set_device (event, device->master_keyboard);
  gdk_event_set_source_device (event, device->keyboard);

  GDK_NOTE (EVENTS,
            g_message ("focus int, device %p surface %p",
                       device, device->keyboard_focus));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
keyboard_handle_leave (void               *data,
                       struct wl_keyboard *keyboard,
                       uint32_t            serial,
                       struct wl_surface  *surface)
{
  GdkWaylandDeviceData *device = data;
  GdkEvent *event;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (device->display);

  if (!surface)
    return;
  if (!GDK_IS_WINDOW (wl_surface_get_user_data (surface)))
    return;
  if (!device->keyboard_focus)
    return;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (device->keyboard_focus);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = FALSE;
  gdk_event_set_device (event, device->master_keyboard);
  gdk_event_set_source_device (event, device->keyboard);

  g_object_unref(device->keyboard_focus);
  device->keyboard_focus = NULL;

  GDK_NOTE (EVENTS,
            g_message ("focus out, device %p surface %p",
                       device, device->keyboard_focus));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static gboolean
keyboard_repeat (gpointer data);

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

      /* Apply the control key - Taken from Xlib
       */
      if (event->state & GDK_CONTROL_MASK)
        {
          if ((c >= '@' && c < '\177') || c == ' ') c &= 0x1F;
          else if (c == '2')
            {
              event->string = g_memdup ("\0\0", 2);
              event->length = 1;
              buf[0] = '\0';
              return;
            }
          else if (c >= '3' && c <= '7') c -= ('3' - '\033');
          else if (c == '8') c = '\177';
          else if (c == '/') c = '_' & 0x1F;
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
get_keyboard_settings (GdkWaylandDeviceData *device)
{
  if (!device->keyboard_settings)
    {
      GSettingsSchemaSource *source;
      GSettingsSchema *schema;

      source = g_settings_schema_source_get_default ();
      schema = g_settings_schema_source_lookup (source, "org.gnome.settings-daemon.peripherals.keyboard", FALSE);
      if (schema != NULL)
        {
          device->keyboard_settings = g_settings_new_full (schema, NULL, NULL);
          g_settings_schema_unref (schema);
        }
    }

  return device->keyboard_settings;
}

static gboolean
get_key_repeat (GdkWaylandDeviceData *device,
                guint                *delay,
                guint                *interval)
{
  gboolean repeat;

  GSettings *keyboard_settings = get_keyboard_settings (device);

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

  return repeat;
}

static gboolean
deliver_key_event (GdkWaylandDeviceData *device,
                   uint32_t              time_,
                   uint32_t              key,
                   uint32_t              state)
{
  GdkEvent *event;
  struct xkb_state *xkb_state;
  struct xkb_keymap *xkb_keymap;
  GdkKeymap *keymap;
  xkb_keysym_t sym;
  guint delay, interval;

  keymap = device->keymap;
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);
  xkb_keymap = _gdk_wayland_keymap_get_xkb_keymap (keymap);

  sym = xkb_state_key_get_one_sym (xkb_state, key);

  device->time = time_;
  device->modifiers = gdk_keymap_get_modifier_state (keymap);

  event = gdk_event_new (state ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
  event->key.window = device->keyboard_focus ? g_object_ref (device->keyboard_focus) : NULL;
  gdk_event_set_device (event, device->master_keyboard);
  gdk_event_set_source_device (event, device->keyboard);
  event->key.time = time_;
  event->key.state = device->modifiers;
  event->key.group = 0;
  event->key.hardware_keycode = key;
  event->key.keyval = sym;
  event->key.is_modifier = _gdk_wayland_keymap_key_is_modifier (keymap, key);

  translate_keyboard_string (&event->key);

  _gdk_wayland_display_deliver_event (device->display, event);

  GDK_NOTE (EVENTS,
            g_message ("keyboard event, code %d, sym %d, "
                       "string %s, mods 0x%x",
                       event->key.hardware_keycode, event->key.keyval,
                       event->key.string, event->key.state));

  if (!xkb_keymap_key_repeats (xkb_keymap, key))
    return FALSE;

  if (!get_key_repeat (device, &delay, &interval))
    return FALSE;

  device->repeat_count++;
  device->repeat_key = key;

  if (state == 0)
    {
      if (device->repeat_timer)
        {
          g_source_remove (device->repeat_timer);
          device->repeat_timer = 0;
        }
      return FALSE;
    }
  else
    {
      switch (device->repeat_count)
        {
        case 1:
          if (device->repeat_timer)
            {
              g_source_remove (device->repeat_timer);
              device->repeat_timer = 0;
            }

          device->repeat_timer =
            gdk_threads_add_timeout (delay, keyboard_repeat, device);
          g_source_set_name_by_id (device->repeat_timer, "[gtk+] keyboard_repeat");
          return TRUE;
        case 2:
          device->repeat_timer =
            gdk_threads_add_timeout (interval, keyboard_repeat, device);
          g_source_set_name_by_id (device->repeat_timer, "[gtk+] keyboard_repeat");
          return FALSE;
        default:
          return TRUE;
        }
    }
}

static gboolean
keyboard_repeat (gpointer data)
{
  GdkWaylandDeviceData *device = data;

  return deliver_key_event (device, device->time, device->repeat_key, 1);
}

static void
keyboard_handle_key (void               *data,
                     struct wl_keyboard *keyboard,
                     uint32_t            serial,
                     uint32_t            time,
                     uint32_t            key,
                     uint32_t            state_w)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (device->display);

  if (!device->keyboard_focus)
    return;

  device->repeat_count = 0;
  _gdk_wayland_display_update_serial (wayland_display, serial);
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
  GdkWaylandDeviceData *device = data;
  GdkKeymap *keymap;
  struct xkb_state *xkb_state;
  PangoDirection direction;

  keymap = device->keymap;
  direction = gdk_keymap_get_direction (keymap);
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);
  device->modifiers = mods_depressed | mods_latched | mods_locked;

  xkb_state_update_mask (xkb_state, mods_depressed, mods_latched, mods_locked, group, 0, 0);

  g_signal_emit_by_name (keymap, "state-changed");
  if (direction != gdk_keymap_get_direction (keymap))
    g_signal_emit_by_name (keymap, "direction-changed");
}

static GdkWaylandTouchData *
_device_manager_add_touch (GdkWaylandDeviceData *device,
                           uint32_t              id,
                           struct wl_surface    *surface)
{
  GdkWaylandTouchData *touch;

  touch = g_new0 (GdkWaylandTouchData, 1);
  touch->id = id;
  touch->window = wl_surface_get_user_data (surface);
  touch->initial_touch = (g_hash_table_size (device->touches) == 0);

  g_hash_table_insert (device->touches, GUINT_TO_POINTER (id), touch);

  return touch;
}

static GdkWaylandTouchData *
_device_manager_get_touch (GdkWaylandDeviceData *device,
                           uint32_t              id)
{
  return g_hash_table_lookup (device->touches, GUINT_TO_POINTER (id));
}

static void
_device_manager_remove_touch (GdkWaylandDeviceData *device,
                              uint32_t              id)
{
  g_hash_table_remove (device->touches, GUINT_TO_POINTER (id));
}

static GdkEvent *
_create_touch_event (GdkWaylandDeviceData *device,
                     GdkWaylandTouchData  *touch,
                     GdkEventType          evtype,
                     uint32_t              time)
{
  GdkWaylandDisplay *display = GDK_WAYLAND_DISPLAY (device->display);
  gint x_root, y_root;
  GdkEvent *event;

  event = gdk_event_new (evtype);
  event->touch.window = g_object_ref (touch->window);
  gdk_event_set_device (event, device->master_pointer);
  gdk_event_set_source_device (event, device->touch);
  event->touch.time = time;
  event->touch.state = device->modifiers;
  gdk_event_set_screen (event, display->screen);
  event->touch.sequence = GUINT_TO_POINTER (touch->id + 1);

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
  GdkWaylandDeviceData *device = data;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  touch = _device_manager_add_touch (device, id, wl_surface);
  touch->x = wl_fixed_to_double (x);
  touch->y = wl_fixed_to_double (y);

  event = _create_touch_event (device, touch, GDK_TOUCH_BEGIN, time);

  GDK_NOTE (EVENTS,
            g_message ("touch begin %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static void
touch_handle_up (void            *data,
                 struct wl_touch *wl_touch,
                 uint32_t         serial,
                 uint32_t         time,
                 int32_t          id)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  touch = _device_manager_get_touch (device, id);
  event = _create_touch_event (device, touch, GDK_TOUCH_END, time);

  GDK_NOTE (EVENTS,
            g_message ("touch end %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (device->display, event);
  _device_manager_remove_touch (device, id);
}

static void
touch_handle_motion (void            *data,
                     struct wl_touch *wl_touch,
                     uint32_t         time,
                     int32_t          id,
                     wl_fixed_t       x,
                     wl_fixed_t       y)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandTouchData *touch;
  GdkEvent *event;

  touch = _device_manager_get_touch (device, id);
  touch->x = wl_fixed_to_double (x);
  touch->y = wl_fixed_to_double (y);

  event = _create_touch_event (device, touch, GDK_TOUCH_UPDATE, time);

  GDK_NOTE (EVENTS,
            g_message ("touch update %f %f", event->touch.x, event->touch.y));

  _gdk_wayland_display_deliver_event (device->display, event);
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
  GdkWaylandDeviceData *device = data;
  GdkWaylandTouchData *touch;
  GHashTableIter iter;
  GdkEvent *event;

  g_hash_table_iter_init (&iter, device->touches);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer *) &touch))
    {
      event = _create_touch_event (device, touch, GDK_TOUCH_CANCEL,
                                   GDK_CURRENT_TIME);
      _gdk_wayland_display_deliver_event (device->display, event);

      g_hash_table_iter_remove (&iter);
    }

  GDK_NOTE (EVENTS, g_message ("touch cancel"));
}

static const struct wl_pointer_listener pointer_listener = {
  pointer_handle_enter,
  pointer_handle_leave,
  pointer_handle_motion,
  pointer_handle_button,
  pointer_handle_axis,
};

static const struct wl_keyboard_listener keyboard_listener = {
  keyboard_handle_keymap,
  keyboard_handle_enter,
  keyboard_handle_leave,
  keyboard_handle_key,
  keyboard_handle_modifiers,
};

static const struct wl_touch_listener touch_listener = {
  touch_handle_down,
  touch_handle_up,
  touch_handle_motion,
  touch_handle_frame,
  touch_handle_cancel
};

static void
seat_handle_capabilities (void                    *data,
                          struct wl_seat          *seat,
                          enum wl_seat_capability  caps)
{
  GdkWaylandDeviceData *device = data;
  GdkWaylandDeviceManager *device_manager =
    GDK_WAYLAND_DEVICE_MANAGER (device->device_manager);

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !device->wl_pointer)
    {
      device->wl_pointer = wl_seat_get_pointer (seat);
      wl_pointer_set_user_data (device->wl_pointer, device);
      wl_pointer_add_listener (device->wl_pointer, &pointer_listener, device);

      device->pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                      "name", "Wayland Pointer",
                                      "type", GDK_DEVICE_TYPE_SLAVE,
                                      "input-source", GDK_SOURCE_MOUSE,
                                      "input-mode", GDK_MODE_SCREEN,
                                      "has-cursor", TRUE,
                                      "display", device->display,
                                      "device-manager", device->device_manager,
                                      NULL);
      _gdk_device_set_associated_device (device->pointer, device->master_pointer);
      GDK_WAYLAND_DEVICE (device->pointer)->device = device;

      device_manager->devices =
        g_list_prepend (device_manager->devices, device->pointer);

      g_signal_emit_by_name (device_manager, "device-added", device->pointer);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && device->wl_pointer)
    {
      wl_pointer_release (device->wl_pointer);
      device->wl_pointer = NULL;
      _gdk_device_set_associated_device (device->pointer, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, device->pointer);

      g_signal_emit_by_name (device_manager, "device-removed", device->pointer);
      g_object_unref (device->pointer);
      device->pointer = NULL;
    }

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !device->wl_keyboard)
    {
      device->wl_keyboard = wl_seat_get_keyboard (seat);
      wl_keyboard_set_user_data (device->wl_keyboard, device);
      wl_keyboard_add_listener (device->wl_keyboard, &keyboard_listener, device);

      device->keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                       "name", "Wayland Keyboard",
                                       "type", GDK_DEVICE_TYPE_SLAVE,
                                       "input-source", GDK_SOURCE_KEYBOARD,
                                       "input-mode", GDK_MODE_SCREEN,
                                       "has-cursor", FALSE,
                                       "display", device->display,
                                       "device-manager", device->device_manager,
                                       NULL);
      _gdk_device_set_associated_device (device->keyboard, device->master_keyboard);
      GDK_WAYLAND_DEVICE (device->keyboard)->device = device;

      device_manager->devices =
        g_list_prepend (device_manager->devices, device->keyboard);

      g_signal_emit_by_name (device_manager, "device-added", device->keyboard);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && device->wl_keyboard)
    {
      wl_keyboard_release (device->wl_keyboard);
      device->wl_keyboard = NULL;
      _gdk_device_set_associated_device (device->keyboard, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, device->keyboard);

      g_signal_emit_by_name (device_manager, "device-removed", device->keyboard);
      g_object_unref (device->keyboard);
      device->keyboard = NULL;
    }

  if ((caps & WL_SEAT_CAPABILITY_TOUCH) && !device->wl_touch)
    {
      device->wl_touch = wl_seat_get_touch (seat);
      wl_touch_set_user_data (device->wl_touch, device);
      wl_touch_add_listener (device->wl_touch, &touch_listener, device);

      device->touch = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                    "name", "Wayland Touch",
                                    "type", GDK_DEVICE_TYPE_SLAVE,
                                    "input-source", GDK_SOURCE_TOUCHSCREEN,
                                    "input-mode", GDK_MODE_SCREEN,
                                    "has-cursor", FALSE,
                                    "display", device->display,
                                    "device-manager", device->device_manager,
                                    NULL);
      _gdk_device_set_associated_device (device->touch, device->master_pointer);
      GDK_WAYLAND_DEVICE (device->touch)->device = device;

      device_manager->devices =
        g_list_prepend (device_manager->devices, device->touch);

      g_signal_emit_by_name (device_manager, "device-added", device->touch);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_TOUCH) && device->wl_touch)
    {
      wl_touch_release (device->wl_touch);
      device->wl_touch = NULL;
      _gdk_device_set_associated_device (device->touch, NULL);

      device_manager->devices =
        g_list_remove (device_manager->devices, device->touch);

      g_signal_emit_by_name (device_manager, "device-removed", device->touch);
      g_object_unref (device->touch);
      device->touch = NULL;
    }
}

static void
seat_handle_name (void                    *data,
                  struct wl_seat          *seat,
                  const char              *name)
{
  /* We don't care about the name. */
}

static const struct wl_seat_listener seat_listener = {
  seat_handle_capabilities,
  seat_handle_name,
};

static void
init_devices (GdkWaylandDeviceData *device)
{
  GdkWaylandDeviceManager *device_manager =
    GDK_WAYLAND_DEVICE_MANAGER (device->device_manager);

  /* pointer */
  device->master_pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                         "name", "Core Pointer",
                                         "type", GDK_DEVICE_TYPE_MASTER,
                                         "input-source", GDK_SOURCE_MOUSE,
                                         "input-mode", GDK_MODE_SCREEN,
                                         "has-cursor", TRUE,
                                         "display", device->display,
                                         "device-manager", device_manager,
                                         NULL);
  GDK_WAYLAND_DEVICE (device->master_pointer)->device = device;

  device_manager->devices =
    g_list_prepend (device_manager->devices, device->master_pointer);
  g_signal_emit_by_name (device_manager, "device-added", device->master_pointer);

  /* keyboard */
  device->master_keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                          "name", "Core Keyboard",
                                          "type", GDK_DEVICE_TYPE_MASTER,
                                          "input-source", GDK_SOURCE_KEYBOARD,
                                          "input-mode", GDK_MODE_SCREEN,
                                          "has-cursor", FALSE,
                                          "display", device->display,
                                          "device-manager", device_manager,
                                          NULL);
  GDK_WAYLAND_DEVICE (device->master_keyboard)->device = device;

  device_manager->devices =
    g_list_prepend (device_manager->devices, device->master_keyboard);
  g_signal_emit_by_name (device_manager, "device-added", device->master_keyboard);

  /* link both */
  _gdk_device_set_associated_device (device->master_pointer, device->master_keyboard);
  _gdk_device_set_associated_device (device->master_keyboard, device->master_pointer);
}

void
_gdk_wayland_device_manager_add_seat (GdkDeviceManager *device_manager,
                                      guint32           id,
				      struct wl_seat   *wl_seat)
{
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;
  GdkWaylandDeviceData *device;

  display = gdk_device_manager_get_display (device_manager);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  device = g_new0 (GdkWaylandDeviceData, 1);
  device->id = id;
  device->keymap = _gdk_wayland_keymap_new ();
  device->display = display;
  device->device_manager = device_manager;
  device->touches = g_hash_table_new_full (NULL, NULL, NULL,
                                           (GDestroyNotify) g_free);
  device->wl_seat = wl_seat;

  wl_seat_add_listener (device->wl_seat, &seat_listener, device);
  wl_seat_set_user_data (device->wl_seat, device);

  device->data_device =
    wl_data_device_manager_get_data_device (display_wayland->data_device_manager,
                                            device->wl_seat);
  wl_data_device_add_listener (device->data_device,
                               &data_device_listener, device);

  device->pointer_surface =
    wl_compositor_create_surface (display_wayland->compositor);

  init_devices (device);
}

void
_gdk_wayland_device_manager_remove_seat (GdkDeviceManager *manager,
                                         guint32           id)
{
  GdkWaylandDeviceManager *device_manager = GDK_WAYLAND_DEVICE_MANAGER (manager);
  GList *l;

  for (l = device_manager->devices; l != NULL; l = l->next)
    {
      GdkWaylandDevice *wayland_device = l->data;
      GdkWaylandDeviceData *device = wayland_device->device;

      if (device->id == id)
        {
          seat_handle_capabilities (device, device->wl_seat, 0);
          g_object_unref (device->keymap);
          wl_surface_destroy (device->pointer_surface);
          /* FIXME: destroy data_device */
          g_clear_object (&device->keyboard_settings);
          g_hash_table_destroy (device->touches);
          g_free (device);

          break;
        }
    }
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

  wayland_device_manager = (GdkWaylandDeviceManager *) device_manager;

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
  GList *l;

  wayland_device_manager = (GdkWaylandDeviceManager *) device_manager;

  /* Find the first pointer device */
  for (l = wayland_device_manager->devices; l != NULL; l = l->next)
    {
      GdkDevice *device = l->data;

      if (gdk_device_get_source (device) == GDK_SOURCE_MOUSE)
        return device;
    }

  return NULL;
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
_gdk_wayland_device_get_button_press_serial (GdkWaylandDeviceData *device)
{
  return device->button_press_serial;
}

static GdkAtom
mime_type_to_gdk_atom (char *mime_type)
{
  if (strcmp (mime_type, "text/plain;charset=utf8"))
    return gdk_atom_intern_static_string ("UTF8_STRING");

  return GDK_NONE;
}

gint
gdk_wayland_device_get_selection_type_atoms (GdkDevice  *gdk_device,
                                             GdkAtom   **atoms_out)
{
  gint i;
  GdkAtom *atoms;
  GdkWaylandDeviceData *device;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device), 0);
  g_return_val_if_fail (atoms_out != NULL, 0);

  device = GDK_WAYLAND_DEVICE (gdk_device)->device;

  if (!device->selection_offer || device->selection_offer->types->len == 0)
    {
      *atoms_out = NULL;
      return 0;
    }

  atoms = g_new0 (GdkAtom, device->selection_offer->types->len);

  /* Convert list of targets to atoms */
  for (i = 0; i < device->selection_offer->types->len; i++)
    atoms[i] = mime_type_to_gdk_atom (device->selection_offer->types->pdata[i]);

  *atoms_out = atoms;
  return device->selection_offer->types->len;
}

typedef struct
{
  GdkWaylandDeviceData *device;
  DataOffer *offer;
  GIOChannel *channel;
  GdkDeviceWaylandRequestContentCallback cb;
  gpointer userdata;
} RequestContentClosure;

static gboolean
_request_content_io_func (GIOChannel   *channel,
                          GIOCondition  condition,
                          gpointer      userdata)
{
  RequestContentClosure *closure = (RequestContentClosure *)userdata;
  gchar *data = NULL;
  gsize len = 0;
  GError *error = NULL;

  /* FIXME: We probably want to do something better than this
   * to avoid blocking on the transfer of large pieces of data:
   * call the callback multiple times I should think.
   */
  if (g_io_channel_read_to_end (channel, &data, &len, &error) != G_IO_STATUS_NORMAL)
    {
      g_warning (G_STRLOC ": Error reading content from pipe: %s", error->message);
      g_clear_error (&error);
    }

  /* Since we use _read_to_end we've got a guaranteed EOF and thus can go
   * ahead and close the fd
   */
  g_io_channel_shutdown (channel, TRUE, NULL);

  closure->cb (closure->device->pointer, data, len, closure->userdata);

  g_free (data);
  data_offer_unref (closure->offer);
  g_io_channel_unref (channel);
  g_free (closure);

  return FALSE;
}

gboolean
gdk_wayland_device_request_selection_content (GdkDevice                              *gdk_device,
                                              const gchar                            *requested_mime_type,
                                              GdkDeviceWaylandRequestContentCallback  cb,
                                              gpointer                                userdata)
{
  int pipe_fd[2];
  RequestContentClosure *closure;
  GdkWaylandDeviceData *device;
  GError *error = NULL;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device), FALSE);
  g_return_val_if_fail (requested_mime_type != NULL, FALSE);
  g_return_val_if_fail (cb != NULL, FALSE);

  device = GDK_WAYLAND_DEVICE (gdk_device)->device;

  if (!device->selection_offer)
    return FALSE;

  /* TODO: Check mimetypes */

  closure = g_new0 (RequestContentClosure, 1);

  device->selection_offer->ref_count++;

  pipe2 (pipe_fd, O_CLOEXEC);
  wl_data_offer_receive (device->selection_offer->offer,
                         requested_mime_type,
                         pipe_fd[1]);
  close (pipe_fd[1]);

  closure->device = device;
  closure->offer = device->selection_offer;
  closure->channel = g_io_channel_unix_new (pipe_fd[0]);
  closure->cb = cb;
  closure->userdata = userdata;

  if (!g_io_channel_set_encoding (closure->channel, NULL, &error))
    {
      g_warning (G_STRLOC ": Error setting encoding on channel: %s",
                 error->message);
      g_clear_error (&error);
      goto error;
    }

  g_io_add_watch (closure->channel,
                  G_IO_IN,
                  _request_content_io_func,
                  closure);

  return TRUE;

 error:
  data_offer_unref (closure->offer);
  g_io_channel_unref (closure->channel);
  close (pipe_fd[1]);
  g_free (closure);

  return FALSE;
}

struct _GdkWaylandSelectionOffer {
  GdkDeviceWaylandOfferContentCallback cb;
  gpointer userdata;
  struct wl_data_source *source;
  GdkWaylandDeviceData *device;
};

static void
data_source_target (void                  *data,
                    struct wl_data_source *source,
                    const char            *mime_type)
{
  g_debug (G_STRLOC ": %s source = %p, mime_type = %s",
           G_STRFUNC, source, mime_type);
}

static void
data_source_send (void                  *data,
                  struct wl_data_source *source,
                  const char            *mime_type,
                  int32_t                fd)
{
  GdkWaylandSelectionOffer *offer = (GdkWaylandSelectionOffer *)data;
  gchar *buf;
  gssize len, bytes_written = 0;

  g_debug (G_STRLOC ": %s source = %p, mime_type = %s fd = %d",
           G_STRFUNC, source, mime_type, fd);

  buf = offer->cb (offer->device->pointer, mime_type, &len, offer->userdata);

  while (len > 0)
    {
      bytes_written += write (fd, buf + bytes_written, len);
      if (bytes_written == -1)
        goto error;
      len -= bytes_written;
    }

  close (fd);
  g_free (buf);

  return;
 error:

  g_warning (G_STRLOC ": Error writing data to client: %s",
             g_strerror (errno));

  close (fd);
  g_free (buf);
}

static void
data_source_cancelled (void                  *data,
                       struct wl_data_source *source)
{
  g_debug (G_STRLOC ": %s source = %p",
           G_STRFUNC, source);
}

static const struct wl_data_source_listener data_source_listener = {
  data_source_target,
  data_source_send,
  data_source_cancelled
};

static guint32
_wl_time_now (void)
{
  struct timeval tv;

  gettimeofday (&tv, NULL);

  return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

gboolean
gdk_wayland_device_offer_selection_content (GdkDevice                             *gdk_device,
                                            const gchar                          **mime_types,
                                            gint                                   nr_mime_types,
                                            GdkDeviceWaylandOfferContentCallback   cb,
                                            gpointer                               userdata)
{
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;
  GdkWaylandSelectionOffer *offer;
  GdkWaylandDeviceData *device;
  gint i;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device), 0);
  device = GDK_WAYLAND_DEVICE (gdk_device)->device;

  display = device->display;
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  offer = g_new0 (GdkWaylandSelectionOffer, 1);
  offer->cb = cb;
  offer->userdata = userdata;
  offer->source =
    wl_data_device_manager_create_data_source (display_wayland->data_device_manager);
  offer->device = device;

  for (i = 0; i < nr_mime_types; i++)
    wl_data_source_offer (offer->source, mime_types[i]);

  wl_data_source_add_listener (offer->source,
                               &data_source_listener,
                               offer);

  wl_data_device_set_selection (device->data_device,
                                offer->source,
                                _gdk_wayland_display_get_serial (display_wayland));

  device->selection_offer_out = offer;

  return TRUE;
}

gboolean
gdk_wayland_device_clear_selection_content (GdkDevice *gdk_device)
{
  GdkWaylandDeviceData *device;

  g_return_val_if_fail (GDK_IS_WAYLAND_DEVICE (gdk_device), 0);
  device = GDK_WAYLAND_DEVICE (gdk_device)->device;

  if (!device->selection_offer_out)
    return FALSE;

  wl_data_device_set_selection (device->data_device,
                                NULL,
                                _wl_time_now ());

  wl_data_source_destroy (device->selection_offer_out->source);
  g_free (device->selection_offer_out);
  device->selection_offer_out = NULL;

  return TRUE;
}
