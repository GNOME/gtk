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
#include "gdkprivate-wayland.h"

#include <xkbcommon/xkbcommon.h>
#include <X11/keysym.h>

#include <sys/time.h>
#include <sys/mman.h>

typedef struct _GdkWaylandDeviceData GdkWaylandDeviceData;

typedef struct _DataOffer DataOffer;

typedef struct _GdkWaylandSelectionOffer GdkWaylandSelectionOffer;

struct _GdkWaylandDeviceData
{
  struct wl_seat *wl_seat;
  struct wl_pointer *wl_pointer;
  struct wl_keyboard *wl_keyboard;

  GdkDisplay *display;
  GdkDeviceManager *device_manager;

  GdkDevice *pointer;
  GdkDevice *keyboard;

  GdkKeymap *keymap;

  GdkModifierType modifiers;
  GdkWindow *pointer_focus;
  GdkWindow *keyboard_focus;
  struct wl_data_device *data_device;
  double surface_x, surface_y;
  uint32_t time;
  GdkWindow *pointer_grab_window;
  uint32_t pointer_grab_time;
  guint32 repeat_timer;
  guint32 repeat_key;
  guint32 repeat_count;

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

#define GDK_TYPE_DEVICE_MANAGER_CORE         (gdk_device_manager_core_get_type ())
#define GDK_DEVICE_MANAGER_CORE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE_MANAGER_CORE, GdkDeviceManagerCore))
#define GDK_DEVICE_MANAGER_CORE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_MANAGER_CORE, GdkDeviceManagerCoreClass))
#define GDK_IS_DEVICE_MANAGER_CORE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE_MANAGER_CORE))
#define GDK_IS_DEVICE_MANAGER_CORE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_MANAGER_CORE))
#define GDK_DEVICE_MANAGER_CORE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_MANAGER_CORE, GdkDeviceManagerCoreClass))

typedef struct _GdkDeviceManagerCore GdkDeviceManagerCore;
typedef struct _GdkDeviceManagerCoreClass GdkDeviceManagerCoreClass;

struct _GdkDeviceManagerCore
{
  GdkDeviceManager parent_object;
  GdkDevice *core_pointer;
  GdkDevice *core_keyboard;
  GList *devices;
};

struct _GdkDeviceManagerCoreClass
{
  GdkDeviceManagerClass parent_class;
};

G_DEFINE_TYPE (GdkDeviceManagerCore,
	       gdk_device_manager_core, GDK_TYPE_DEVICE_MANAGER)

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
  gint x_int, y_int;

  gdk_window_get_device_position (window, device, &x_int, &y_int, mask);

  if (axes)
    {
      axes[0] = x_int;
      axes[1] = y_int;
    }
}

static void
gdk_wayland_device_set_window_cursor (GdkDevice *device,
                                      GdkWindow *window,
                                      GdkCursor *cursor)
{
  GdkWaylandDeviceData *wd = GDK_WAYLAND_DEVICE(device)->device;
  GdkWaylandDisplay *wayland_display =
    GDK_WAYLAND_DISPLAY (gdk_window_get_display (window));
  struct wl_buffer *buffer;
  int x, y, w, h;

  if (cursor)
    g_object_ref (cursor);

  /* Setting the cursor to NULL means that we should use the default cursor */
  if (!cursor)
    {
      /* FIXME: Is this the best sensible default ? */
      cursor = _gdk_wayland_display_get_cursor_for_type (device->display,
                                                         GDK_LEFT_PTR);
    }

  buffer = _gdk_wayland_cursor_get_buffer (cursor, &x, &y, &w, &h);
  wl_pointer_set_cursor (wd->wl_pointer,
                         _gdk_wayland_display_get_serial (wayland_display),
                         wd->pointer_surface,
                         x, y);
  wl_surface_attach (wd->pointer_surface, buffer, 0, 0);
  wl_surface_damage (wd->pointer_surface,  0, 0, w, h);
  wl_surface_commit(wd->pointer_surface);

  g_object_unref (cursor);
}

static void
gdk_wayland_device_warp (GdkDevice *device,
                         GdkScreen *screen,
                         gint       x,
                         gint       y)
{
}

static void
gdk_wayland_device_query_state (GdkDevice        *device,
                                GdkWindow        *window,
                                GdkWindow       **root_window,
                                GdkWindow       **child_window,
                                gint             *root_x,
                                gint             *root_y,
                                gint             *win_x,
                                gint             *win_y,
                                GdkModifierType  *mask)
{
  GdkWaylandDeviceData *wd;
  GdkScreen *default_screen;

  wd = GDK_WAYLAND_DEVICE(device)->device;
  default_screen = gdk_display_get_default_screen (wd->display);

  if (root_window)
    *root_window = gdk_screen_get_root_window (default_screen);
  if (child_window)
    *child_window = wd->pointer_focus;
  /* Do something clever for relative here */
#if 0
  if (root_x)
    *root_x = wd->x;
  if (root_y)
    *root_y = wd->y;
#endif
  if (win_x)
    *win_x = wd->surface_x;
  if (win_y)
    *win_y = wd->surface_y;
  if (mask)
    *mask = wd->modifiers;
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

      /* FIXME: This probably breaks if you end up with multiple grabs on the
       * same window - but we need to know the input device for when we are
       * asked to map a popup window so that the grab can be managed by the
       * compositor.
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
                                       gint            *win_x,
                                       gint            *win_y,
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

struct wl_seat *
gdk_wayland_device_get_wl_seat (GdkDevice *device)
{
  g_return_val_if_fail(GDK_IS_WAYLAND_DEVICE (device), NULL);

  return GDK_WAYLAND_DEVICE (device)->device->wl_seat;
}

struct wl_pointer *
gdk_wayland_device_get_wl_pointer (GdkDevice *device)
{
  g_return_val_if_fail(GDK_IS_WAYLAND_DEVICE (device), NULL);

  return GDK_WAYLAND_DEVICE (device)->device->wl_pointer;
}


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
                   uint32_t               time,
                   struct wl_surface     *surface,
                   int32_t                x,
                   int32_t                y,
                   struct wl_data_offer  *offer)
{
  GdkWaylandDeviceData *device = (GdkWaylandDeviceData *)data;

  g_debug (G_STRLOC ": %s data_device = %p time = %d, surface = %p, x = %d y = %d, offer = %p",
           G_STRFUNC, data_device, time, surface, x, y, offer);

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

  /* Retrieve the DataOffer associated with with the wl_data_offer - this
   * association is made when the listener is attached.
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

  _gdk_wayland_display_update_serial (wayland_display, serial);

  device->pointer_focus = wl_surface_get_user_data(surface);
  g_object_ref(device->pointer_focus);

  event = gdk_event_new (GDK_ENTER_NOTIFY);
  event->crossing.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->pointer);
  event->crossing.subwindow = NULL;
  event->crossing.time = (guint32)(g_get_monotonic_time () / 1000);
  event->crossing.x = wl_fixed_to_double (sx);
  event->crossing.y = wl_fixed_to_double (sy);

  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  event->crossing.focus = TRUE;
  event->crossing.state = 0;

  device->surface_x = wl_fixed_to_double (sx);
  device->surface_y = wl_fixed_to_double (sy);

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

  _gdk_wayland_display_update_serial (wayland_display, serial);

  event = gdk_event_new (GDK_LEAVE_NOTIFY);
  event->crossing.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->pointer);
  event->crossing.subwindow = NULL;
  event->crossing.time = (guint32)(g_get_monotonic_time () / 1000);
  event->crossing.x = device->surface_x;
  event->crossing.y = device->surface_y;

  event->crossing.mode = GDK_CROSSING_NORMAL;
  event->crossing.detail = GDK_NOTIFY_ANCESTOR;
  event->crossing.focus = TRUE;
  event->crossing.state = 0;

  _gdk_wayland_display_deliver_event (device->display, event);

  GDK_NOTE (EVENTS,
            g_message ("leave, device %p surface %p",
                       device, device->pointer_focus));

  g_object_unref(device->pointer_focus);
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

  event = gdk_event_new (GDK_NOTHING);

  device->time = time;
  device->surface_x = wl_fixed_to_double (sx);
  device->surface_y = wl_fixed_to_double (sy);

  event->motion.type = GDK_MOTION_NOTIFY;
  event->motion.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->pointer);
  event->motion.time = time;
  event->motion.x = wl_fixed_to_double (sx);
  event->motion.y = wl_fixed_to_double (sy);
  event->motion.axes = NULL;
  event->motion.state = device->modifiers;
  event->motion.is_hint = 0;
  gdk_event_set_screen (event, display->screen);

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

  event = gdk_event_new (state ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE);
  event->button.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->pointer);
  event->button.time = time;
  event->button.x = device->surface_x;
  event->button.y = device->surface_y;
  event->button.axes = NULL;
  event->button.state = device->modifiers;
  event->button.button = gdk_button;
  gdk_event_set_screen (event, display->screen);

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

  /* get the delta and convert it into the expected range */
  switch (axis) {
  case WL_POINTER_AXIS_VERTICAL_SCROLL:
    delta_x = 0;
    delta_y = wl_fixed_to_double (value) / 10.0;
    break;
  case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
    delta_x = wl_fixed_to_double (value) / 10.0;
    delta_y = 0;
  default:
    g_return_if_reached ();
  }

  device->time = time;
  event = gdk_event_new (GDK_SCROLL);
  event->scroll.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->pointer);
  event->scroll.time = time;
  event->scroll.x = (gdouble) device->surface_x;
  event->scroll.y = (gdouble) device->surface_y;
  event->scroll.direction = GDK_SCROLL_SMOOTH;
  event->scroll.delta_x = delta_x;
  event->scroll.delta_y = delta_y;
  event->scroll.state = device->modifiers;
  gdk_event_set_screen (event, display->screen);

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
  if (device->keymap)
    g_object_unref (device->keymap);

  device->keymap = _gdk_wayland_keymap_new_from_fd (format, fd, size);
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

  _gdk_wayland_display_update_serial (wayland_display, serial);

  device->keyboard_focus = wl_surface_get_user_data(surface);
  g_object_ref(device->keyboard_focus);

  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (device->keyboard_focus);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = TRUE;
  gdk_event_set_device (event, device->keyboard);

  GDK_NOTE (EVENTS,
            g_message ("focus int, device %p surface %p",
                       device, device->keyboard_focus));

  _gdk_wayland_display_deliver_event (device->display, event);

  _gdk_wayland_window_add_focus (device->keyboard_focus);
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

  _gdk_wayland_display_update_serial (wayland_display, serial);

  _gdk_wayland_window_remove_focus (device->keyboard_focus);
  event = gdk_event_new (GDK_FOCUS_CHANGE);
  event->focus_change.window = g_object_ref (device->keyboard_focus);
  event->focus_change.send_event = FALSE;
  event->focus_change.in = FALSE;
  gdk_event_set_device (event, device->keyboard);

  g_object_unref(device->keyboard_focus);
  device->keyboard_focus = NULL;

  GDK_NOTE (EVENTS,
            g_message ("focus out, device %p surface %p",
                       device, device->keyboard_focus));

  _gdk_wayland_display_deliver_event (device->display, event);
}

static gboolean
keyboard_repeat (gpointer data);

static GdkModifierType
get_modifier (struct xkb_state *state)
{
  GdkModifierType modifiers = 0;
  modifiers |= (xkb_state_mod_name_is_active (state, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0)?GDK_SHIFT_MASK:0;
  modifiers |= (xkb_state_mod_name_is_active (state, XKB_MOD_NAME_CAPS, XKB_STATE_MODS_EFFECTIVE) > 0)?GDK_LOCK_MASK:0;
  modifiers |= (xkb_state_mod_name_is_active (state, XKB_MOD_NAME_CTRL, XKB_STATE_MODS_EFFECTIVE) > 0)?GDK_CONTROL_MASK:0;
  modifiers |= (xkb_state_mod_name_is_active (state, XKB_MOD_NAME_ALT, XKB_STATE_MODS_EFFECTIVE) > 0)?GDK_MOD1_MASK:0;
  modifiers |= (xkb_state_mod_name_is_active (state, "Mod2", XKB_STATE_MODS_EFFECTIVE) > 0)?GDK_MOD2_MASK:0;
  modifiers |= (xkb_state_mod_name_is_active (state, "Mod3", XKB_STATE_MODS_EFFECTIVE) > 0)?GDK_MOD3_MASK:0;
  modifiers |= (xkb_state_mod_name_is_active (state, XKB_MOD_NAME_LOGO, XKB_STATE_MODS_EFFECTIVE) > 0)?GDK_MOD4_MASK:0;
  modifiers |= (xkb_state_mod_name_is_active (state, "Mod5", XKB_STATE_MODS_EFFECTIVE) > 0)?GDK_MOD5_MASK:0;

  return modifiers;
}

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

static gboolean
deliver_key_event(GdkWaylandDeviceData *device,
                  uint32_t time, uint32_t key, uint32_t state)
{
  GdkEvent *event;
  struct xkb_state *xkb_state;
  GdkKeymap *keymap;
  xkb_keysym_t sym;

  keymap = device->keymap;
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);

  sym = xkb_state_key_get_one_sym (xkb_state, key);

  device->time = time;
  device->modifiers = get_modifier (xkb_state);

  event = gdk_event_new (state ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
  event->key.window = device->keyboard_focus?g_object_ref (device->keyboard_focus):NULL;
  gdk_event_set_device (event, device->keyboard);
  event->button.time = time;
  event->key.state = device->modifiers;
  event->key.group = 0;
  event->key.hardware_keycode = sym;
  event->key.keyval = sym;

  event->key.is_modifier = device->modifiers > 0;

  translate_keyboard_string (&event->key);

  _gdk_wayland_display_deliver_event (device->display, event);

  GDK_NOTE (EVENTS,
            g_message ("keyboard event, code %d, sym %d, "
                       "string %s, mods 0x%x",
                       event->key.hardware_keycode, event->key.keyval,
                       event->key.string, event->key.state));

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
  else if (device->modifiers)
    {
      return FALSE;
    }
  else switch (device->repeat_count)
    {
    case 1:
      if (device->repeat_timer)
        {
          g_source_remove (device->repeat_timer);
          device->repeat_timer = 0;
        }

      device->repeat_timer =
        gdk_threads_add_timeout (400, keyboard_repeat, device);
      return TRUE;
    case 2:
      device->repeat_timer =
        gdk_threads_add_timeout (80, keyboard_repeat, device);
      return FALSE;
    default:
      return TRUE;
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

  keymap = device->keymap;
  xkb_state = _gdk_wayland_keymap_get_xkb_state (keymap);
  device->modifiers = mods_depressed | mods_latched | mods_locked;

  xkb_state_update_mask (xkb_state, mods_depressed, mods_latched, mods_locked, group, 0, 0);
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

static void
seat_handle_capabilities(void *data, struct wl_seat *seat,
                         enum wl_seat_capability caps)
{
  GdkWaylandDeviceData *device = data;
  GdkDeviceManagerCore *device_manager_core =
    GDK_DEVICE_MANAGER_CORE(device->device_manager);

  if ((caps & WL_SEAT_CAPABILITY_POINTER) && !device->wl_pointer)
    {
      device->wl_pointer = wl_seat_get_pointer(seat);
      wl_pointer_set_user_data(device->wl_pointer, device);
      wl_pointer_add_listener(device->wl_pointer, &pointer_listener,
                              device);

      device->pointer = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                      "name", "Core Pointer",
                                      "type", GDK_DEVICE_TYPE_MASTER,
                                      "input-source", GDK_SOURCE_MOUSE,
                                      "input-mode", GDK_MODE_SCREEN,
                                      "has-cursor", TRUE,
                                      "display", device->display,
                                      "device-manager", device->device_manager,
                                      NULL);
      GDK_WAYLAND_DEVICE (device->pointer)->device = device;

      device_manager_core->devices =
        g_list_prepend (device_manager_core->devices, device->pointer);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_POINTER) && device->wl_pointer)
    {
      wl_pointer_destroy(device->wl_pointer);
      device->wl_pointer = NULL;

      device_manager_core->devices =
        g_list_remove (device_manager_core->devices, device->pointer);

      g_object_unref (device->pointer);
      device->pointer = NULL;
    }

  if ((caps & WL_SEAT_CAPABILITY_KEYBOARD) && !device->wl_keyboard) 
    {
      device->wl_keyboard = wl_seat_get_keyboard(seat);
      wl_keyboard_set_user_data(device->wl_keyboard, device);
      wl_keyboard_add_listener(device->wl_keyboard, &keyboard_listener,
                               device);

      device->keyboard = g_object_new (GDK_TYPE_WAYLAND_DEVICE,
                                       "name", "Core Keyboard",
                                       "type", GDK_DEVICE_TYPE_MASTER,
                                       "input-source", GDK_SOURCE_KEYBOARD,
                                       "input-mode", GDK_MODE_SCREEN,
                                       "has-cursor", FALSE,
                                       "display", device->display,
                                       "device-manager", device->device_manager,
                                       NULL);
      GDK_WAYLAND_DEVICE (device->keyboard)->device = device;

      device_manager_core->devices =
        g_list_prepend (device_manager_core->devices, device->keyboard);
    }
  else if (!(caps & WL_SEAT_CAPABILITY_KEYBOARD) && device->wl_keyboard) 
    {
      wl_keyboard_destroy(device->wl_keyboard);
      device->wl_keyboard = NULL;

      device_manager_core->devices =
        g_list_remove (device_manager_core->devices, device->keyboard);

      g_object_unref (device->keyboard);
      device->keyboard = NULL;
    }

  if (device->keyboard && device->pointer)
    {
      _gdk_device_set_associated_device (device->pointer, device->keyboard);
      _gdk_device_set_associated_device (device->keyboard, device->pointer);
    }
}

static const struct wl_seat_listener seat_listener = {
    seat_handle_capabilities,
};

void
_gdk_wayland_device_manager_add_device (GdkDeviceManager *device_manager,
					struct wl_seat *wl_seat)
{
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;
  GdkWaylandDeviceData *device;

  display = gdk_device_manager_get_display (device_manager);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  device = g_new0 (GdkWaylandDeviceData, 1);
  device->keymap = _gdk_wayland_keymap_new ();
  device->display = display;
  device->device_manager = device_manager;

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
}

static void
free_device (gpointer data)
{
  g_object_unref (data);
}

static void
gdk_device_manager_core_finalize (GObject *object)
{
  GdkDeviceManagerCore *device_manager_core;

  device_manager_core = GDK_DEVICE_MANAGER_CORE (object);

  g_list_free_full (device_manager_core->devices, free_device);

  G_OBJECT_CLASS (gdk_device_manager_core_parent_class)->finalize (object);
}

static GList *
gdk_device_manager_core_list_devices (GdkDeviceManager *device_manager,
                                      GdkDeviceType     type)
{
  GdkDeviceManagerCore *device_manager_core;
  GList *devices = NULL;

  if (type == GDK_DEVICE_TYPE_MASTER)
    {
      device_manager_core = (GdkDeviceManagerCore *) device_manager;
      devices = g_list_copy(device_manager_core->devices);
    }

  return devices;
}

static GdkDevice *
gdk_device_manager_core_get_client_pointer (GdkDeviceManager *device_manager)
{
  GdkDeviceManagerCore *device_manager_core;
  GList *l;

  device_manager_core = (GdkDeviceManagerCore *) device_manager;

  /* Find the first pointer device */
  for (l = device_manager_core->devices; l != NULL; l = l->next)
    {
      GdkDevice *device = l->data;

      if (gdk_device_get_source (device) == GDK_SOURCE_MOUSE)
        return device;
    }

  return NULL;
}

static void
gdk_device_manager_core_class_init (GdkDeviceManagerCoreClass *klass)
{
  GdkDeviceManagerClass *device_manager_class = GDK_DEVICE_MANAGER_CLASS (klass);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = gdk_device_manager_core_finalize;
  device_manager_class->list_devices = gdk_device_manager_core_list_devices;
  device_manager_class->get_client_pointer = gdk_device_manager_core_get_client_pointer;
}

static void
gdk_device_manager_core_init (GdkDeviceManagerCore *device_manager)
{
}

GdkDeviceManager *
_gdk_wayland_device_manager_new (GdkDisplay *display)
{
  return g_object_new (GDK_TYPE_DEVICE_MANAGER_CORE,
                       "display", display,
                       NULL);
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
    {
      atoms[i] = gdk_atom_intern (device->selection_offer->types->pdata[i],
                                  FALSE);
      GDK_NOTE (MISC,
                g_message (G_STRLOC ": Adding atom for %s",
                           (char *)device->selection_offer->types->pdata[i]));
    }

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
_request_content_io_func (GIOChannel *channel,
                          GIOCondition condition,
                          gpointer userdata)
{
  RequestContentClosure *closure = (RequestContentClosure *)userdata;
  gchar *data = NULL;
  gsize len = 0;
  GError *error = NULL;

  /* FIXME: We probably want to do something better than this to avoid
   * blocking on the transfer of large pieces of data: call the callback
   * multiple times I should think.
   */
  if (g_io_channel_read_to_end (channel,
                                &data,
                                &len,
                                &error) != G_IO_STATUS_NORMAL)
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
  GdkWaylandSelectionOffer *offer = (GdkWaylandSelectionOffer *)data;;
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

  gettimeofday(&tv, NULL);

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
    {
      wl_data_source_offer (offer->source,
                            mime_types[i]);
    }

  wl_data_source_add_listener (offer->source,
                               &data_source_listener,
                               offer);

  wl_data_device_set_selection (device->data_device,
                                offer->source,
                                _wl_time_now ());

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
