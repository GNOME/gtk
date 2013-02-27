/*
 * Copyright © 2010 Intel Corporation
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

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include <glib.h>
#include "gdkwayland.h"
#include "gdkdisplay.h"
#include "gdkdisplay-wayland.h"
#include "gdkscreen.h"
#include "gdkinternals.h"
#include "gdkdeviceprivate.h"
#include "gdkdevicemanager.h"
#include "gdkkeysprivate.h"
#include "gdkprivate-wayland.h"

static void _gdk_wayland_display_load_cursor_theme (GdkWaylandDisplay *wayland_display);

G_DEFINE_TYPE (GdkWaylandDisplay, gdk_wayland_display, GDK_TYPE_DISPLAY)

static void
gdk_input_init (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland;
  GdkDeviceManager *device_manager;
  GdkDevice *device;
  GList *list, *l;

  display_wayland = GDK_WAYLAND_DISPLAY (display);
  device_manager = gdk_display_get_device_manager (display);

  /* For backwards compatibility, just add
   * floating devices that are not keyboards.
   */
  list = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_FLOATING);

  for (l = list; l; l = l->next)
    {
      device = l->data;

      if (gdk_device_get_source (device) == GDK_SOURCE_KEYBOARD)
	continue;

      display_wayland->input_devices = g_list_prepend (display_wayland->input_devices, l->data);
    }

  g_list_free (list);

  /* Now set "core" pointer to the first
   * master device that is a pointer.
   */
  list = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  for (l = list; l; l = l->next)
    {
      device = list->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_MOUSE)
	continue;

      display->core_pointer = device;
      break;
    }

  /* Add the core pointer to the devices list */
  display_wayland->input_devices = g_list_prepend (display_wayland->input_devices, display->core_pointer);

  g_list_free (list);
}

static void
gdk_registry_handle_global(void *data, struct wl_registry *registry, uint32_t id,
					const char *interface, uint32_t version)
{
  GdkWaylandDisplay *display_wayland = data;
  GdkDisplay *gdk_display = GDK_DISPLAY_OBJECT (data);
  struct wl_seat *seat;
  struct wl_output *output;

  if (strcmp(interface, "wl_compositor") == 0) {
    display_wayland->compositor =
	wl_registry_bind(display_wayland->wl_registry, id, &wl_compositor_interface, 1);
  } else if (strcmp(interface, "wl_shm") == 0) {
   display_wayland->shm =
	wl_registry_bind(display_wayland->wl_registry, id, &wl_shm_interface, 1);

    /* SHM interface is prerequisite */
    _gdk_wayland_display_load_cursor_theme(display_wayland);
  } else if (strcmp(interface, "wl_shell") == 0) {
    display_wayland->shell =
	wl_registry_bind(display_wayland->wl_registry, id, &wl_shell_interface, 1);
  } else if (strcmp(interface, "wl_output") == 0) {
    output =
      wl_registry_bind(display_wayland->wl_registry, id, &wl_output_interface, 1);
    _gdk_wayland_screen_add_output(display_wayland->screen, output);
  } else if (strcmp(interface, "wl_seat") == 0) {
    seat = wl_registry_bind(display_wayland->wl_registry, id, &wl_seat_interface, 1);
    _gdk_wayland_device_manager_add_device (gdk_display->device_manager,
					    seat);
  } else if (strcmp(interface, "wl_data_device_manager") == 0) {
      display_wayland->data_device_manager =
        wl_registry_bind(display_wayland->wl_registry, id,
					&wl_data_device_manager_interface, 1);
  }
}

static void
gdk_registry_handle_global_remove(void               *data,
                                  struct wl_registry *registry,
                                  uint32_t            id)
{
  GdkWaylandDisplay *display_wayland = data;

  /* We don't know what this item is - try as an output */
  _gdk_wayland_screen_remove_output_by_id (display_wayland->screen, id);
}

static const struct wl_registry_listener registry_listener = {
    gdk_registry_handle_global,
    gdk_registry_handle_global_remove
};

GdkDisplay *
_gdk_wayland_display_open (const gchar *display_name)
{
  struct wl_display *wl_display;
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;

  wl_display = wl_display_connect(display_name);
  if (!wl_display)
    return NULL;

  display = g_object_new (GDK_TYPE_WAYLAND_DISPLAY, NULL);
  display_wayland = GDK_WAYLAND_DISPLAY (display);

  display_wayland->wl_display = wl_display;

  display_wayland->screen = _gdk_wayland_screen_new (display);

  display->device_manager = _gdk_wayland_device_manager_new (display);

  /* Set up listener so we'll catch all events. */
  display_wayland->wl_registry = wl_display_get_registry(display_wayland->wl_display);
  wl_registry_add_listener(display_wayland->wl_registry, &registry_listener, display_wayland);

  wl_display_dispatch(display_wayland->wl_display);

  display_wayland->event_source =
    _gdk_wayland_display_event_source_new (display);

  gdk_input_init (display);

  g_signal_emit_by_name (display, "opened");
  g_signal_emit_by_name (gdk_display_manager_get(), "display_opened", display);

  return display;
}

static void
gdk_wayland_display_dispose (GObject *object)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (object);

  _gdk_wayland_display_manager_remove_display (gdk_display_manager_get (),
					       GDK_DISPLAY (display_wayland));
  g_list_foreach (display_wayland->input_devices,
		  (GFunc) g_object_run_dispose, NULL);

  _gdk_screen_close (display_wayland->screen);

  if (display_wayland->event_source)
    {
      g_source_destroy (display_wayland->event_source);
      g_source_unref (display_wayland->event_source);
      display_wayland->event_source = NULL;
    }

  G_OBJECT_CLASS (gdk_wayland_display_parent_class)->dispose (object);
}

static void
gdk_wayland_display_finalize (GObject *object)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (object);

  /* Keymap */
  if (display_wayland->keymap)
    g_object_unref (display_wayland->keymap);

  /* input GdkDevice list */
  g_list_free_full (display_wayland->input_devices, g_object_unref);

  g_object_unref (display_wayland->screen);

  g_free (display_wayland->startup_notification_id);

  G_OBJECT_CLASS (gdk_wayland_display_parent_class)->finalize (object);
}

static const gchar *
gdk_wayland_display_get_name (GdkDisplay *display)
{
  return "Wayland";
}

static gint
gdk_wayland_display_get_n_screens (GdkDisplay *display)
{
  return 1;
}

static GdkScreen *
gdk_wayland_display_get_screen (GdkDisplay *display, 
				gint        screen_num)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);
  g_return_val_if_fail (screen_num == 0, NULL);

  return GDK_WAYLAND_DISPLAY (display)->screen;
}

static GdkScreen *
gdk_wayland_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_WAYLAND_DISPLAY (display)->screen;
}

static void
gdk_wayland_display_beep (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));
}

static void
gdk_wayland_display_sync (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  display_wayland = GDK_WAYLAND_DISPLAY (display);

  wl_display_roundtrip(display_wayland->wl_display);
}

static void
gdk_wayland_display_flush (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (!display->closed)
    wl_display_flush(GDK_WAYLAND_DISPLAY (display)->wl_display);;
}

static gboolean
gdk_wayland_display_has_pending (GdkDisplay *display)
{
  return FALSE;
}

static GdkWindow *
gdk_wayland_display_get_default_group (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return NULL;
}


static gboolean
gdk_wayland_display_supports_selection_notification (GdkDisplay *display)
{
  return TRUE;
}

static gboolean
gdk_wayland_display_request_selection_notification (GdkDisplay *display,
						    GdkAtom     selection)

{
    return FALSE;
}

static gboolean
gdk_wayland_display_supports_clipboard_persistence (GdkDisplay *display)
{
  return FALSE;
}

static void
gdk_wayland_display_store_clipboard (GdkDisplay    *display,
				     GdkWindow     *clipboard_window,
				     guint32        time_,
				     const GdkAtom *targets,
				     gint           n_targets)
{
}

static gboolean
gdk_wayland_display_supports_shapes (GdkDisplay *display)
{
  return TRUE;
}

static gboolean
gdk_wayland_display_supports_input_shapes (GdkDisplay *display)
{
  return TRUE;
}

static gboolean
gdk_wayland_display_supports_composite (GdkDisplay *display)
{
  return TRUE;
}

static GList *
gdk_wayland_display_list_devices (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_WAYLAND_DISPLAY (display)->input_devices;
}

static void
gdk_wayland_display_before_process_all_updates (GdkDisplay *display)
{
}

static void
gdk_wayland_display_after_process_all_updates (GdkDisplay *display)
{
  /* Post the damage here instead? */
}

static gulong
gdk_wayland_display_get_next_serial (GdkDisplay *display)
{
  static gulong serial = 0;
  return ++serial;
}

void
_gdk_wayland_display_make_default (GdkDisplay *display)
{
}

/**
 * gdk_wayland_display_broadcast_startup_message:
 * @display: a #GdkDisplay
 * @message_type: startup notification message type ("new", "change",
 * or "remove")
 * @...: a list of key/value pairs (as strings), terminated by a
 * %NULL key. (A %NULL value for a key will cause that key to be
 * skipped in the output.)
 *
 * Sends a startup notification message of type @message_type to
 * @display. 
 *
 * This is a convenience function for use by code that implements the
 * freedesktop startup notification specification. Applications should
 * not normally need to call it directly. See the <ulink
 * url="http://standards.freedesktop.org/startup-notification-spec/startup-notification-latest.txt">Startup
 * Notification Protocol specification</ulink> for
 * definitions of the message types and keys that can be used.
 *
 * Since: 2.12
 **/
void
gdk_wayland_display_broadcast_startup_message (GdkDisplay *display,
					       const char *message_type,
					       ...)
{
  GString *message;
  va_list ap;
  const char *key, *value, *p;

  message = g_string_new (message_type);
  g_string_append_c (message, ':');

  va_start (ap, message_type);
  while ((key = va_arg (ap, const char *)))
    {
      value = va_arg (ap, const char *);
      if (!value)
	continue;

      g_string_append_printf (message, " %s=\"", key);
      for (p = value; *p; p++)
	{
	  switch (*p)
	    {
	    case ' ':
	    case '"':
	    case '\\':
	      g_string_append_c (message, '\\');
	      break;
	    }

	  g_string_append_c (message, *p);
	}
      g_string_append_c (message, '\"');
    }
  va_end (ap);

  g_string_free (message, TRUE);
}

static void
gdk_wayland_display_notify_startup_complete (GdkDisplay  *display,
					     const gchar *startup_id)
{
  gdk_wayland_display_broadcast_startup_message (display, "remove",
						 "ID", startup_id,
						 NULL);
}

static void
gdk_wayland_display_event_data_copy (GdkDisplay     *display,
				     const GdkEvent *src,
				     GdkEvent       *dst)
{
}

static void
gdk_wayland_display_event_data_free (GdkDisplay *display,
				     GdkEvent   *event)
{
}

GdkKeymap *
_gdk_wayland_display_get_keymap (GdkDisplay *display)
{
  GdkDeviceManager *device_manager;
  GList *list, *l;
  GdkDevice *core_keyboard = NULL;
  static GdkKeymap *tmp_keymap = NULL;

  device_manager = gdk_display_get_device_manager (display);
  list = gdk_device_manager_list_devices (device_manager, GDK_DEVICE_TYPE_MASTER);

  for (l = list; l; l = l->next)
    {
      GdkDevice *device;
      device = list->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
	continue;

      core_keyboard = device;
      break;
    }

  if (core_keyboard && tmp_keymap)
    {
      g_object_unref (tmp_keymap);
      tmp_keymap = NULL;
    }

  if (core_keyboard)
    return _gdk_wayland_device_get_keymap (core_keyboard);

  tmp_keymap = _gdk_wayland_keymap_new ();

  return tmp_keymap;
}

static void
gdk_wayland_display_push_error_trap (GdkDisplay *display)
{
}

static gint
gdk_wayland_display_pop_error_trap (GdkDisplay *display,
				    gboolean    ignored)
{
  return 0;
}

static void
gdk_wayland_display_class_init (GdkWaylandDisplayClass * class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_wayland_display_dispose;
  object_class->finalize = gdk_wayland_display_finalize;

  display_class->window_type = gdk_wayland_window_get_type ();
  display_class->get_name = gdk_wayland_display_get_name;
  display_class->get_n_screens = gdk_wayland_display_get_n_screens;
  display_class->get_screen = gdk_wayland_display_get_screen;
  display_class->get_default_screen = gdk_wayland_display_get_default_screen;
  display_class->beep = gdk_wayland_display_beep;
  display_class->sync = gdk_wayland_display_sync;
  display_class->flush = gdk_wayland_display_flush;
  display_class->has_pending = gdk_wayland_display_has_pending;
  display_class->queue_events = _gdk_wayland_display_queue_events;
  display_class->get_default_group = gdk_wayland_display_get_default_group;
  display_class->supports_selection_notification = gdk_wayland_display_supports_selection_notification;
  display_class->request_selection_notification = gdk_wayland_display_request_selection_notification;
  display_class->supports_clipboard_persistence = gdk_wayland_display_supports_clipboard_persistence;
  display_class->store_clipboard = gdk_wayland_display_store_clipboard;
  display_class->supports_shapes = gdk_wayland_display_supports_shapes;
  display_class->supports_input_shapes = gdk_wayland_display_supports_input_shapes;
  display_class->supports_composite = gdk_wayland_display_supports_composite;
  display_class->list_devices = gdk_wayland_display_list_devices;
  display_class->get_app_launch_context = _gdk_wayland_display_get_app_launch_context;
  display_class->get_default_cursor_size = _gdk_wayland_display_get_default_cursor_size;
  display_class->get_maximal_cursor_size = _gdk_wayland_display_get_maximal_cursor_size;
  display_class->get_cursor_for_type = _gdk_wayland_display_get_cursor_for_type;
  display_class->get_cursor_for_name = _gdk_wayland_display_get_cursor_for_name;
  display_class->get_cursor_for_pixbuf = _gdk_wayland_display_get_cursor_for_pixbuf;
  display_class->supports_cursor_alpha = _gdk_wayland_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_wayland_display_supports_cursor_color;
  display_class->before_process_all_updates = gdk_wayland_display_before_process_all_updates;
  display_class->after_process_all_updates = gdk_wayland_display_after_process_all_updates;
  display_class->get_next_serial = gdk_wayland_display_get_next_serial;
  display_class->notify_startup_complete = gdk_wayland_display_notify_startup_complete;
  display_class->event_data_copy = gdk_wayland_display_event_data_copy;
  display_class->event_data_free = gdk_wayland_display_event_data_free;
  display_class->create_window_impl = _gdk_wayland_display_create_window_impl;
  display_class->get_keymap = _gdk_wayland_display_get_keymap;
  display_class->push_error_trap = gdk_wayland_display_push_error_trap;
  display_class->pop_error_trap = gdk_wayland_display_pop_error_trap;
  display_class->get_selection_owner = _gdk_wayland_display_get_selection_owner;
  display_class->set_selection_owner = _gdk_wayland_display_set_selection_owner;
  display_class->send_selection_notify = _gdk_wayland_display_send_selection_notify;
  display_class->get_selection_property = _gdk_wayland_display_get_selection_property;
  display_class->convert_selection = _gdk_wayland_display_convert_selection;
  display_class->text_property_to_utf8_list = _gdk_wayland_display_text_property_to_utf8_list;
  display_class->utf8_to_string_target = _gdk_wayland_display_utf8_to_string_target;
}

static void
gdk_wayland_display_init (GdkWaylandDisplay *display)
{
  _gdk_wayland_display_manager_add_display (gdk_display_manager_get (),
					    GDK_DISPLAY (display));

  display->xkb_context = xkb_context_new (0);
}

static void
_gdk_wayland_display_load_cursor_theme (GdkWaylandDisplay *wayland_display)
{
  guint w, h;
  const gchar *theme_name;
  GValue v = G_VALUE_INIT;

  g_assert (wayland_display);
  g_assert (wayland_display->shm);

  _gdk_wayland_display_get_default_cursor_size (GDK_DISPLAY (wayland_display),
                                                &w, &h);
  g_value_init (&v, G_TYPE_STRING);
  if (gdk_setting_get ("gtk-cursor-theme-name", &v))
    theme_name = g_value_get_string (&v);
  else
    theme_name = "default";

  wayland_display->cursor_theme = wl_cursor_theme_load (theme_name,
                                                        w,
                                                        wayland_display->shm);
  g_value_unset (&v);
}

guint32
_gdk_wayland_display_get_serial (GdkWaylandDisplay *wayland_display)
{
  return wayland_display->serial;
}

void
_gdk_wayland_display_update_serial (GdkWaylandDisplay *wayland_display,
                                    guint32            serial)
{
  if (serial > wayland_display->serial)
    wayland_display->serial = serial;
}

/**
 * gdk_wayland_display_get_wl_display
 * @display: (type GdkWaylandDisplay): a #GdkDisplay
 *
 * Returns the Wayland wl_display of a #GdkDisplay
 *
 * Returns: (transfer none): a Wayland wl_display
 *
 * Since: 3.8
 */
struct wl_display *
gdk_wayland_display_get_wl_display(GdkDisplay *display)
{
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY(display);

  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY(display), NULL);

  return wayland_display->wl_display;
}

/**
 * gdk_wayland_display_get_wl_compositor
 * @display: (type GdkWaylandDisplay): a #GdkDisplay
 *
 * Returns the Wayland global singleton compositor of a #GdkDisplay
 *
 * Returns: (transfer none): a Wayland wl_compositor
 *
 * Since: 3.8
 */
struct wl_compositor *
gdk_wayland_display_get_wl_compositor (GdkDisplay *display)
{
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY(display);

  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY(display), NULL);

  return wayland_display->compositor;
}

/**
 * gdk_wayland_display_get_wl_shell
 * @display: (type GdkWaylandDisplay): a #GdkDisplay
 *
 * Returns the Wayland global singleton shell of a #GdkDisplay
 *
 * Returns: (transfer none): a Wayland wl_shell
 *
 * Since: 3.8
 */
struct wl_shell *
gdk_wayland_display_get_wl_shell (GdkDisplay *display)
{
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY(display);

  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY(display), NULL);

  return wayland_display->shell;
}
