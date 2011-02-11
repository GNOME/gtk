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
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

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

#include <X11/extensions/XKBcommon.h>
#include <X11/keysym.h>

#define GDK_TYPE_DEVICE_CORE         (gdk_device_core_get_type ())
#define GDK_DEVICE_CORE(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDK_TYPE_DEVICE_CORE, GdkDeviceCore))
#define GDK_DEVICE_CORE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST ((c), GDK_TYPE_DEVICE_CORE, GdkDeviceCoreClass))
#define GDK_IS_DEVICE_CORE(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDK_TYPE_DEVICE_CORE))
#define GDK_IS_DEVICE_CORE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE ((c), GDK_TYPE_DEVICE_CORE))
#define GDK_DEVICE_CORE_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDK_TYPE_DEVICE_CORE, GdkDeviceCoreClass))

typedef struct _GdkDeviceCore GdkDeviceCore;
typedef struct _GdkDeviceCoreClass GdkDeviceCoreClass;
typedef struct _GdkWaylandDevice GdkWaylandDevice;

struct _GdkWaylandDevice
{
  GdkDisplay *display;
  GdkDevice *pointer;
  GdkDevice *keyboard;
  GdkModifierType modifiers;
  GdkWindow *pointer_focus;
  GdkWindow *keyboard_focus;
  struct wl_input_device *device;
  int32_t x, y, surface_x, surface_y;
  uint32_t time;
};

struct _GdkDeviceCore
{
  GdkDevice parent_instance;
  GdkWaylandDevice *device;
};

struct _GdkDeviceCoreClass
{
  GdkDeviceClass parent_class;
};

G_DEFINE_TYPE (GdkDeviceCore, gdk_device_core, GDK_TYPE_DEVICE)

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
gdk_device_core_get_history (GdkDevice      *device,
                             GdkWindow      *window,
                             guint32         start,
                             guint32         stop,
                             GdkTimeCoord ***events,
                             gint           *n_events)
{
  return FALSE;
}

static void
gdk_device_core_get_state (GdkDevice       *device,
                           GdkWindow       *window,
                           gdouble         *axes,
                           GdkModifierType *mask)
{
  gint x_int, y_int;

  gdk_window_get_pointer (window, &x_int, &y_int, mask);

  if (axes)
    {
      axes[0] = x_int;
      axes[1] = y_int;
    }
}

static void
gdk_device_core_set_window_cursor (GdkDevice *device,
                                   GdkWindow *window,
                                   GdkCursor *cursor)
{
  GdkWaylandDevice *wd = GDK_DEVICE_CORE(device)->device;
  struct wl_buffer *buffer;
  int x, y;

  if (cursor)
    {
      buffer = _gdk_wayland_cursor_get_buffer(cursor, &x, &y);
      wl_input_device_attach(wd->device, wd->time, buffer, x, y);
    }
  else
    {
      wl_input_device_attach(wd->device, wd->time, NULL, 0, 0);
    }
}

static void
gdk_device_core_warp (GdkDevice *device,
                      GdkScreen *screen,
                      gint       x,
                      gint       y)
{
}

static gboolean
gdk_device_core_query_state (GdkDevice        *device,
                             GdkWindow        *window,
                             GdkWindow       **root_window,
                             GdkWindow       **child_window,
                             gint             *root_x,
                             gint             *root_y,
                             gint             *win_x,
                             gint             *win_y,
                             GdkModifierType  *mask)
{
  GdkWaylandDevice *wd;
  GdkScreen *default_screen;

  wd = GDK_DEVICE_CORE(device)->device;
  default_screen = gdk_display_get_default_screen (wd->display);

  if (root_window)
    *root_window = gdk_screen_get_root_window (default_screen);
  if (child_window)
    *child_window = wd->pointer_focus;
  if (root_x)
    *root_x = wd->x;
  if (root_y)
    *root_y = wd->y;
  if (win_x)
    *win_x = wd->surface_x;
  if (win_y)
    *win_y = wd->surface_y;
  if (mask)
    *mask = wd->modifiers;

  return TRUE;
}

static GdkGrabStatus
gdk_device_core_grab (GdkDevice    *device,
                      GdkWindow    *window,
                      gboolean      owner_events,
                      GdkEventMask  event_mask,
                      GdkWindow    *confine_to,
                      GdkCursor    *cursor,
                      guint32       time_)
{
  return GDK_GRAB_SUCCESS;
}

static void
gdk_device_core_ungrab (GdkDevice *device,
                        guint32    time_)
{
}

static GdkWindow *
gdk_device_core_window_at_position (GdkDevice       *device,
                                    gint            *win_x,
                                    gint            *win_y,
                                    GdkModifierType *mask,
                                    gboolean         get_toplevel)
{
  GdkWaylandDevice *wd;

  wd = GDK_DEVICE_CORE(device)->device;
  if (win_x)
    *win_x = wd->surface_x;
  if (win_y)
    *win_y = wd->surface_y;
  if (mask)
    *mask = wd->modifiers;

  return wd->pointer_focus;
}

static void
gdk_device_core_select_window_events (GdkDevice    *device,
                                      GdkWindow    *window,
                                      GdkEventMask  event_mask)
{
}

static void
gdk_device_core_class_init (GdkDeviceCoreClass *klass)
{
  GdkDeviceClass *device_class = GDK_DEVICE_CLASS (klass);

  device_class->get_history = gdk_device_core_get_history;
  device_class->get_state = gdk_device_core_get_state;
  device_class->set_window_cursor = gdk_device_core_set_window_cursor;
  device_class->warp = gdk_device_core_warp;
  device_class->query_state = gdk_device_core_query_state;
  device_class->grab = gdk_device_core_grab;
  device_class->ungrab = gdk_device_core_ungrab;
  device_class->window_at_position = gdk_device_core_window_at_position;
  device_class->select_window_events = gdk_device_core_select_window_events;
}

static void
gdk_device_core_init (GdkDeviceCore *device_core)
{
  GdkDevice *device;

  device = GDK_DEVICE (device_core);

  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_X, 0, 0, 1);
  _gdk_device_add_axis (device, GDK_NONE, GDK_AXIS_Y, 0, 0, 1);
}

struct wl_input_device *
_gdk_wayland_device_get_device (GdkDevice *device)
{
  return GDK_DEVICE_CORE (device)->device->device;
}

static void
input_handle_motion(void *data, struct wl_input_device *input_device,
		    uint32_t time,
		    int32_t x, int32_t y, int32_t sx, int32_t sy)
{
  GdkWaylandDevice *device = data;
  GdkDisplayWayland *display = GDK_DISPLAY_WAYLAND (device->display);
  GdkEvent *event;

  event = gdk_event_new (GDK_NOTHING);

  device->time = time;
  device->x = x;
  device->y = y;
  device->surface_x = sx;
  device->surface_y = sy;

  event->motion.type = GDK_MOTION_NOTIFY;
  event->motion.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->pointer);
  event->motion.time = time;
  event->motion.x = (gdouble) sx;
  event->motion.y = (gdouble) sy;
  event->motion.x_root = (gdouble) x;
  event->motion.y_root = (gdouble) y;
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
input_handle_button(void *data, struct wl_input_device *input_device,
		     uint32_t time, uint32_t button, uint32_t state)
{
  GdkWaylandDevice *device = data;
  GdkDisplayWayland *display = GDK_DISPLAY_WAYLAND (device->display);
  GdkEvent *event;
  uint32_t modifier;

  device->time = time;
  event = gdk_event_new (state ? GDK_BUTTON_PRESS : GDK_BUTTON_RELEASE);
  event->button.window = g_object_ref (device->pointer_focus);
  gdk_event_set_device (event, device->pointer);
  event->button.time = time;
  event->button.x = (gdouble) device->surface_x;
  event->button.y = (gdouble) device->surface_y;
  event->button.x_root = (gdouble) device->x;
  event->button.y_root = (gdouble) device->y;
  event->button.axes = NULL;
  event->button.state = device->modifiers;
  event->button.button = button - 271;
  gdk_event_set_screen (event, display->screen);

  modifier = 1 << (8 + button - 272);
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

static void
input_handle_key(void *data, struct wl_input_device *input_device,
		 uint32_t time, uint32_t key, uint32_t state)
{
  GdkWaylandDevice *device = data;
  GdkEvent *event;
  uint32_t code, modifier, level;
  struct xkb_desc *xkb;
  GdkKeymap *keymap;

  keymap = gdk_keymap_get_for_display (device->display);
  xkb = _gdk_wayland_keymap_get_xkb_desc (keymap);

  device->time = time;
  event = gdk_event_new (state ? GDK_KEY_PRESS : GDK_KEY_RELEASE);
  event->key.window = g_object_ref (device->keyboard_focus);
  gdk_event_set_device (event, device->keyboard);
  event->button.time = time;
  event->key.state = device->modifiers;
  event->key.group = 0;
  code = key + xkb->min_key_code;
  event->key.hardware_keycode = code;

  level = 0;
  if (device->modifiers & XKB_COMMON_SHIFT_MASK &&
      XkbKeyGroupWidth(xkb, code, 0) > 1)
    level = 1;

  event->key.keyval = XkbKeySymEntry(xkb, code, level, 0);

  modifier = xkb->map->modmap[code];
  if (state)
    device->modifiers |= modifier;
  else
    device->modifiers &= ~modifier;

  event->key.is_modifier = modifier > 0;

  translate_keyboard_string (&event->key);

  _gdk_wayland_display_deliver_event (device->display, event);

  GDK_NOTE (EVENTS,
	    g_message ("keyboard event, code %d, sym %d, "
		       "string %s, mods 0x%x",
		       code, event->key.keyval,
		       event->key.string, event->key.state));
}

static void
input_handle_pointer_focus(void *data,
			   struct wl_input_device *input_device,
			   uint32_t time, struct wl_surface *surface,
			   int32_t x, int32_t y, int32_t sx, int32_t sy)
{
  GdkWaylandDevice *device = data;
  GdkEvent *event;

  device->time = time;
  if (device->pointer_focus)
    {
      event = gdk_event_new (GDK_LEAVE_NOTIFY);
      event->crossing.window = g_object_ref (device->pointer_focus);
      gdk_event_set_device (event, device->pointer);
      event->crossing.subwindow = NULL;
      event->crossing.time = time;
      event->crossing.x = (gdouble) device->surface_x;
      event->crossing.y = (gdouble) device->surface_y;
      event->crossing.x_root = (gdouble) device->x;
      event->crossing.y_root = (gdouble) device->y;

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

  if (surface)
    {
      device->pointer_focus = wl_surface_get_user_data(surface);
      g_object_ref(device->pointer_focus);

      event = gdk_event_new (GDK_ENTER_NOTIFY);
      event->crossing.window = g_object_ref (device->pointer_focus);
      gdk_event_set_device (event, device->pointer);
      event->crossing.subwindow = NULL;
      event->crossing.time = time;
      event->crossing.x = (gdouble) sx;
      event->crossing.y = (gdouble) sy;
      event->crossing.x_root = (gdouble) x;
      event->crossing.y_root = (gdouble) y;

      event->crossing.mode = GDK_CROSSING_NORMAL;
      event->crossing.detail = GDK_NOTIFY_ANCESTOR;
      event->crossing.focus = TRUE;
      event->crossing.state = 0;

      device->surface_x = sx;
      device->surface_y = sy;
      device->x = x;
      device->y = y;

      _gdk_wayland_display_deliver_event (device->display, event);

      GDK_NOTE (EVENTS,
		g_message ("enter, device %p surface %p",
			   device, device->pointer_focus));
    }
}

static void
update_modifiers(GdkWaylandDevice *device, struct wl_array *keys)
{
  uint32_t *k, *end;
  GdkKeymap *keymap;
  struct xkb_desc *xkb;

  keymap = gdk_keymap_get_for_display (device->display);
  xkb = _gdk_wayland_keymap_get_xkb_desc (keymap);

  end = keys->data + keys->size;
  device->modifiers = 0;
  for (k = keys->data; k < end; k++)
    device->modifiers |= xkb->map->modmap[*k];
}

static void
input_handle_keyboard_focus(void *data,
			    struct wl_input_device *input_device,
			    uint32_t time,
			    struct wl_surface *surface,
			    struct wl_array *keys)
{
  GdkWaylandDevice *device = data;
  GdkEvent *event;

  device->time = time;
  if (device->keyboard_focus)
    {
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

  if (surface)
    {
      device->keyboard_focus = wl_surface_get_user_data(surface);
      g_object_ref(device->keyboard_focus);

      event = gdk_event_new (GDK_FOCUS_CHANGE);
      event->focus_change.window = g_object_ref (device->keyboard_focus);
      event->focus_change.send_event = FALSE;
      event->focus_change.in = TRUE;
      gdk_event_set_device (event, device->keyboard);

      update_modifiers (device, keys);

      GDK_NOTE (EVENTS,
		g_message ("focus int, device %p surface %p",
			   device, device->keyboard_focus));

      _gdk_wayland_display_deliver_event (device->display, event);
    }
}

static const struct wl_input_device_listener input_device_listener = {
  input_handle_motion,
  input_handle_button,
  input_handle_key,
  input_handle_pointer_focus,
  input_handle_keyboard_focus,
};

void
_gdk_wayland_device_manager_add_device (GdkDeviceManager *device_manager,
					struct wl_input_device *wl_device)
{
  GdkDisplay *display;
  GdkDeviceManagerCore *device_manager_core =
    GDK_DEVICE_MANAGER_CORE(device_manager);
  GdkWaylandDevice *device;

  device = g_new0 (GdkWaylandDevice, 1);
  display = gdk_device_manager_get_display (device_manager);

  device->display = display;
  device->pointer = g_object_new (GDK_TYPE_DEVICE_CORE,
				  "name", "Core Pointer",
				  "type", GDK_DEVICE_TYPE_MASTER,
				  "input-source", GDK_SOURCE_MOUSE,
				  "input-mode", GDK_MODE_SCREEN,
				  "has-cursor", TRUE,
				  "display", display,
				  "device-manager", device_manager,
				  NULL);

  device->keyboard = g_object_new (GDK_TYPE_DEVICE_CORE,
				   "name", "Core Keyboard",
				   "type", GDK_DEVICE_TYPE_MASTER,
				   "input-source", GDK_SOURCE_KEYBOARD,
				   "input-mode", GDK_MODE_SCREEN,
				   "has-cursor", FALSE,
				   "display", display,
				   "device-manager", device_manager,
				   NULL);

  GDK_DEVICE_CORE (device->pointer)->device = device;
  GDK_DEVICE_CORE (device->keyboard)->device = device;
  device->device = wl_device;

  wl_input_device_add_listener(device->device,
			       &input_device_listener, device);

  device_manager_core->devices =
    g_list_prepend (device_manager_core->devices, device->keyboard);
  device_manager_core->devices =
    g_list_prepend (device_manager_core->devices, device->pointer);

  _gdk_device_set_associated_device (device->pointer, device->keyboard);
  _gdk_device_set_associated_device (device->keyboard, device->pointer);
}

static void
free_device (void *data, void *user_data)
{
  g_object_unref (data);
}

static void
gdk_device_manager_core_finalize (GObject *object)
{
  GdkDeviceManagerCore *device_manager_core;

  device_manager_core = GDK_DEVICE_MANAGER_CORE (object);

  g_list_foreach (device_manager_core->devices, free_device, NULL);
  g_list_free (device_manager_core->devices);

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

  device_manager_core = (GdkDeviceManagerCore *) device_manager;
  return device_manager_core->devices->data;
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
