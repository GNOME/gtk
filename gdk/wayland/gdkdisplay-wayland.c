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
#include <sys/mman.h>

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
#include "gdkglcontext-wayland.h"

/**
 * SECTION:wayland_interaction
 * @Short_description: Wayland backend-specific functions
 * @Title: Wayland Interaction
 *
 * The functions in this section are specific to the GDK Wayland backend.
 * To use them, you need to include the `<gdk/gdkwayland.h>` header and use
 * the Wayland-specific pkg-config files to build your application (either
 * `gdk-wayland-3.0` or `gtk+-wayland-3.0`).
 *
 * To make your code compile with other GDK backends, guard backend-specific
 * calls by an ifdef as follows. Since GDK may be built with multiple
 * backends, you should also check for the backend that is in use (e.g. by
 * using the GDK_IS_WAYLAND_DISPLAY() macro).
 * |[<!-- language="C" -->
 * #ifdef GDK_WINDOWING_WAYLAND
 *   if (GDK_IS_WAYLAND_DISPLAY (display))
 *     {
 *       // make Wayland-specific calls here
 *     }
 *   else
 * #endif
 * #ifdef GDK_WINDOWING_X11
 *   if (GDK_IS_X11_DISPLAY (display))
 *     {
 *       // make X11-specific calls here
 *     }
 *   else
 * #endif
 *   g_error ("Unsupported GDK backend");
 * ]|
 */

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
      device = l->data;

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
xdg_shell_ping (void             *data,
                struct xdg_shell *xdg_shell,
                uint32_t          serial)
{
  GdkWaylandDisplay *wayland_display = data;

  _gdk_wayland_display_update_serial (wayland_display, serial);

  GDK_NOTE (EVENTS,
            g_message ("ping, shell %p, serial %u\n", xdg_shell, serial));

  xdg_shell_pong (xdg_shell, serial);
}

static const struct xdg_shell_listener xdg_shell_listener = {
  xdg_shell_ping,
};

static void
gdk_registry_handle_global (void               *data,
                            struct wl_registry *registry,
                            uint32_t            id,
                            const char         *interface,
                            uint32_t            version)
{
  GdkWaylandDisplay *display_wayland = data;
  GdkDisplay *gdk_display = GDK_DISPLAY_OBJECT (data);
  struct wl_seat *seat;
  struct wl_output *output;

  GDK_NOTE (MISC,
            g_message ("add global %u, interface %s, version %u", id, interface, version));

  if (strcmp (interface, "wl_compositor") == 0)
    {
      display_wayland->compositor =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_compositor_interface, MIN (version, 3));
      display_wayland->compositor_version = MIN (version, 3);
    }
  else if (strcmp (interface, "wl_shm") == 0)
    {
      display_wayland->shm =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_shm_interface, 1);

      /* SHM interface is prerequisite */
      _gdk_wayland_display_load_cursor_theme (display_wayland);
    }
  else if (strcmp (interface, "xdg_shell") == 0)
    {
      display_wayland->xdg_shell =
        wl_registry_bind (display_wayland->wl_registry, id, &xdg_shell_interface, 1);
      xdg_shell_use_unstable_version (display_wayland->xdg_shell, XDG_SHELL_VERSION_CURRENT);
      xdg_shell_add_listener (display_wayland->xdg_shell, &xdg_shell_listener, display_wayland);
    }
  else if (strcmp (interface, "gtk_shell") == 0)
    {
      if (version == SUPPORTED_GTK_SHELL_VERSION)
        {
          display_wayland->gtk_shell =
            wl_registry_bind(display_wayland->wl_registry, id,
                             &gtk_shell_interface, SUPPORTED_GTK_SHELL_VERSION);
          _gdk_wayland_screen_set_has_gtk_shell (display_wayland->screen);
        }
    }
  else if (strcmp (interface, "wl_output") == 0)
    {
      output =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_output_interface, MIN (version, 2));
      _gdk_wayland_screen_add_output (display_wayland->screen, id, output, MIN (version, 2));
    }
  else if (strcmp (interface, "wl_seat") == 0)
    {
      seat = wl_registry_bind (display_wayland->wl_registry, id, &wl_seat_interface, MIN (version, 4));
      _gdk_wayland_device_manager_add_seat (gdk_display->device_manager, id, seat);
    }
  else if (strcmp (interface, "wl_data_device_manager") == 0)
    {
      display_wayland->data_device_manager =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_data_device_manager_interface, 1);
    }
  else if (strcmp (interface, "wl_subcompositor") == 0)
    {
      display_wayland->subcompositor =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_subcompositor_interface, 1);
    }
}

static void
gdk_registry_handle_global_remove (void               *data,
                                   struct wl_registry *registry,
                                   uint32_t            id)
{
  GdkWaylandDisplay *display_wayland = data;
  GdkDisplay *display = GDK_DISPLAY (display_wayland);

  GDK_NOTE (MISC, g_message ("remove global %u", id));
  _gdk_wayland_device_manager_remove_seat (display->device_manager, id);
  _gdk_wayland_screen_remove_output (display_wayland->screen, id);

  /* FIXME: the object needs to be destroyed here, we're leaking */
}

static const struct wl_registry_listener registry_listener = {
    gdk_registry_handle_global,
    gdk_registry_handle_global_remove
};

static void
log_handler (const char *format, va_list args)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_ERROR, format, args);
}

GdkDisplay *
_gdk_wayland_display_open (const gchar *display_name)
{
  struct wl_display *wl_display;
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;

  GDK_NOTE (MISC, g_message ("opening display %s", display_name ? display_name : ""));

  /* If this variable is unset then wayland initialisation will surely
   * fail, logging a fatal error in the process.  Save ourselves from
   * that.
   */
  if (g_getenv ("XDG_RUNTIME_DIR") == NULL)
    return NULL;

  wl_log_set_handler_client (log_handler);

  wl_display = wl_display_connect (display_name);
  if (!wl_display)
    return NULL;

  display = g_object_new (GDK_TYPE_WAYLAND_DISPLAY, NULL);
  display->device_manager = _gdk_wayland_device_manager_new (display);

  display_wayland = GDK_WAYLAND_DISPLAY (display);
  display_wayland->wl_display = wl_display;
  display_wayland->screen = _gdk_wayland_screen_new (display);
  display_wayland->event_source = _gdk_wayland_display_event_source_new (display);
  _gdk_wayland_display_init_cursors (display_wayland);

  display_wayland->wl_registry = wl_display_get_registry (display_wayland->wl_display);
  wl_registry_add_listener (display_wayland->wl_registry, &registry_listener, display_wayland);

  /* Wait until the dust has settled during init... */
  wl_display_roundtrip (display_wayland->wl_display);

  gdk_input_init (display);

  display_wayland->selection = gdk_wayland_selection_new ();

  g_signal_emit_by_name (display, "opened");

  return display;
}

static void
gdk_wayland_display_dispose (GObject *object)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (object);

  g_list_foreach (display_wayland->input_devices,
		  (GFunc) g_object_run_dispose, NULL);

  _gdk_screen_close (display_wayland->screen);

  if (display_wayland->event_source)
    {
      g_source_destroy (display_wayland->event_source);
      g_source_unref (display_wayland->event_source);
      display_wayland->event_source = NULL;
    }

  if (display_wayland->selection)
    {
      gdk_wayland_selection_free (display_wayland->selection);
      display_wayland->selection = NULL;
    }

  G_OBJECT_CLASS (gdk_wayland_display_parent_class)->dispose (object);
}

static void
gdk_wayland_display_finalize (GObject *object)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (object);

  _gdk_wayland_display_finalize_cursors (display_wayland);

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

  wl_display_roundtrip (display_wayland->wl_display);
}

static void
gdk_wayland_display_flush (GdkDisplay *display)
{
  g_return_if_fail (GDK_IS_DISPLAY (display));

  if (!display->closed)
    wl_display_flush (GDK_WAYLAND_DISPLAY (display)->wl_display);
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
  return FALSE;
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
  return FALSE;
}

static gboolean
gdk_wayland_display_supports_input_shapes (GdkDisplay *display)
{
  return TRUE;
}

static gboolean
gdk_wayland_display_supports_composite (GdkDisplay *display)
{
  return FALSE;
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

static void
gdk_wayland_display_notify_startup_complete (GdkDisplay  *display,
					     const gchar *startup_id)
{
}

static GdkKeymap *
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
      device = l->data;

      if (gdk_device_get_source (device) != GDK_SOURCE_KEYBOARD)
	continue;

      core_keyboard = device;
      break;
    }

  g_list_free (list);

  if (core_keyboard && tmp_keymap)
    {
      g_object_unref (tmp_keymap);
      tmp_keymap = NULL;
    }

  if (core_keyboard)
    return _gdk_wayland_device_get_keymap (core_keyboard);

  if (!tmp_keymap)
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
gdk_wayland_display_class_init (GdkWaylandDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_wayland_display_dispose;
  object_class->finalize = gdk_wayland_display_finalize;

  display_class->window_type = gdk_wayland_window_get_type ();
  display_class->get_name = gdk_wayland_display_get_name;
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
  display_class->get_cursor_for_surface = _gdk_wayland_display_get_cursor_for_surface;
  display_class->supports_cursor_alpha = _gdk_wayland_display_supports_cursor_alpha;
  display_class->supports_cursor_color = _gdk_wayland_display_supports_cursor_color;
  display_class->before_process_all_updates = gdk_wayland_display_before_process_all_updates;
  display_class->after_process_all_updates = gdk_wayland_display_after_process_all_updates;
  display_class->get_next_serial = gdk_wayland_display_get_next_serial;
  display_class->notify_startup_complete = gdk_wayland_display_notify_startup_complete;
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

  display_class->make_gl_context_current = gdk_wayland_display_make_gl_context_current;
}

static void
gdk_wayland_display_init (GdkWaylandDisplay *display)
{
  display->xkb_context = xkb_context_new (0);
}

void
gdk_wayland_display_set_cursor_theme (GdkDisplay  *display,
                                      const gchar *name,
                                      gint         size)
{
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY(display);
  struct wl_cursor_theme *theme;
  int i;

  g_assert (wayland_display);
  g_assert (wayland_display->shm);

  if (g_strcmp0 (name, wayland_display->cursor_theme_name) == 0 &&
      wayland_display->cursor_theme_size == size)
    return;

  theme = wl_cursor_theme_load (name, size, wayland_display->shm);
  if (theme == NULL)
    {
      g_warning ("Failed to load cursor theme %s\n", name);
      return;
    }

  for (i = 0; i < GDK_WAYLAND_THEME_SCALES_COUNT; i++)
    {
      if (wayland_display->scaled_cursor_themes[i])
        {
          wl_cursor_theme_destroy (wayland_display->scaled_cursor_themes[i]);
          wayland_display->scaled_cursor_themes[i] = NULL;
        }
    }
  wayland_display->scaled_cursor_themes[0] = theme;
  if (wayland_display->cursor_theme_name != NULL)
    g_free (wayland_display->cursor_theme_name);
  wayland_display->cursor_theme_name = g_strdup (name);
  wayland_display->cursor_theme_size = size;

  _gdk_wayland_display_update_cursors (wayland_display);
}

struct wl_cursor_theme *
_gdk_wayland_display_get_scaled_cursor_theme (GdkWaylandDisplay *wayland_display,
                                              guint              scale)
{
  struct wl_cursor_theme *theme;

  g_assert (wayland_display->cursor_theme_name);
  g_assert (scale <= GDK_WAYLAND_MAX_THEME_SCALE);
  g_assert (scale >= 1);

  theme = wayland_display->scaled_cursor_themes[scale - 1];
  if (!theme)
    {
      theme = wl_cursor_theme_load (wayland_display->cursor_theme_name,
                                    wayland_display->cursor_theme_size * scale,
                                    wayland_display->shm);
      if (theme == NULL)
        {
          g_warning ("Failed to load cursor theme %s with scale %u\n",
                     wayland_display->cursor_theme_name, scale);
          return NULL;
        }
      wayland_display->scaled_cursor_themes[scale - 1] = theme;
    }

  return theme;
}

static void
_gdk_wayland_display_load_cursor_theme (GdkWaylandDisplay *wayland_display)
{
  guint size;
  const gchar *name;
  GValue v = G_VALUE_INIT;

  g_assert (wayland_display);
  g_assert (wayland_display->shm);

  g_value_init (&v, G_TYPE_INT);
  if (gdk_screen_get_setting (wayland_display->screen, "gtk-cursor-theme-size", &v))
    size = g_value_get_int (&v);
  else
    size = 32;
  g_value_unset (&v);

  g_value_init (&v, G_TYPE_STRING);
  if (gdk_screen_get_setting (wayland_display->screen, "gtk-cursor-theme-name", &v))
    name = g_value_get_string (&v);
  else
    name = "default";

  gdk_wayland_display_set_cursor_theme (GDK_DISPLAY (wayland_display), name, size);
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
  wayland_display->serial = serial;
}

/**
 * gdk_wayland_display_get_wl_display:
 * @display: (type GdkWaylandDisplay): a #GdkDisplay
 *
 * Returns the Wayland wl_display of a #GdkDisplay.
 *
 * Returns: (transfer none): a Wayland wl_display
 *
 * Since: 3.8
 */
struct wl_display *
gdk_wayland_display_get_wl_display (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), NULL);

  return GDK_WAYLAND_DISPLAY (display)->wl_display;
}

/**
 * gdk_wayland_display_get_wl_compositor:
 * @display: (type GdkWaylandDisplay): a #GdkDisplay
 *
 * Returns the Wayland global singleton compositor of a #GdkDisplay.
 *
 * Returns: (transfer none): a Wayland wl_compositor
 *
 * Since: 3.8
 */
struct wl_compositor *
gdk_wayland_display_get_wl_compositor (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), NULL);

  return GDK_WAYLAND_DISPLAY (display)->compositor;
}

/**
 * gdk_wayland_display_get_xdg_shell:
 * @display: (type GdkWaylandDisplay): a #GdkDisplay
 *
 * Returns the Wayland global singleton shell of a #GdkDisplay.
 *
 * Returns: (transfer none): a Wayland xdg_shell
 *
 * Since: 3.8
 */
struct xdg_shell *
gdk_wayland_display_get_xdg_shell (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), NULL);

  return GDK_WAYLAND_DISPLAY (display)->xdg_shell;
}

static const cairo_user_data_key_t gdk_wayland_cairo_key;

typedef struct _GdkWaylandCairoSurfaceData {
  gpointer buf;
  size_t buf_length;
  struct wl_shm_pool *pool;
  struct wl_buffer *buffer;
  GdkWaylandDisplay *display;
  uint32_t scale;
  gboolean busy;
} GdkWaylandCairoSurfaceData;

static void
buffer_release_callback (void             *_data,
                         struct wl_buffer *wl_buffer)
{
  cairo_surface_t *surface = _data;
  GdkWaylandCairoSurfaceData *data = cairo_surface_get_user_data (surface, &gdk_wayland_cairo_key);

  data->busy = FALSE;
  cairo_surface_destroy (surface);
}

static const struct wl_buffer_listener buffer_listener = {
  buffer_release_callback
};

static struct wl_shm_pool *
create_shm_pool (struct wl_shm  *shm,
                 int             width,
                 int             height,
                 size_t         *buf_length,
                 void          **data_out)
{
  char filename[] = "/tmp/wayland-shm-XXXXXX";
  struct wl_shm_pool *pool;
  int fd, size, stride;
  void *data;

  fd = mkstemp (filename);
  if (fd < 0)
    {
      g_critical (G_STRLOC ": Unable to create temporary file (%s): %s",
                  filename, g_strerror (errno));
      return NULL;
    }

  stride = width * 4;
  size = stride * height;
  if (ftruncate (fd, size) < 0)
    {
      g_critical (G_STRLOC ": Truncating temporary file failed: %s",
                  g_strerror (errno));
      close (fd);
      return NULL;
    }

  data = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  unlink (filename);

  if (data == MAP_FAILED)
    {
      g_critical (G_STRLOC ": mmap'ping temporary file failed: %s",
                  g_strerror (errno));
      close (fd);
      return NULL;
    }

  pool = wl_shm_create_pool (shm, fd, size);

  close (fd);

  *data_out = data;
  *buf_length = size;

  return pool;
}

static void
gdk_wayland_cairo_surface_destroy (void *p)
{
  GdkWaylandCairoSurfaceData *data = p;

  if (data->buffer)
    wl_buffer_destroy (data->buffer);

  if (data->pool)
    wl_shm_pool_destroy (data->pool);

  munmap (data->buf, data->buf_length);
  g_free (data);
}

cairo_surface_t *
_gdk_wayland_display_create_shm_surface (GdkWaylandDisplay *display,
                                         int                width,
                                         int                height,
                                         guint              scale)
{
  GdkWaylandCairoSurfaceData *data;
  cairo_surface_t *surface = NULL;
  cairo_status_t status;
  int stride;

  data = g_new (GdkWaylandCairoSurfaceData, 1);
  data->display = display;
  data->buffer = NULL;
  data->scale = scale;
  data->busy = FALSE;

  stride = width * 4;

  data->pool = create_shm_pool (display->shm,
                                width*scale, height*scale,
                                &data->buf_length,
                                &data->buf);

  surface = cairo_image_surface_create_for_data (data->buf,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width*scale,
                                                 height*scale,
                                                 stride*scale);

  data->buffer = wl_shm_pool_create_buffer (data->pool, 0,
                                            width*scale, height*scale,
                                            stride*scale, WL_SHM_FORMAT_ARGB8888);
  wl_buffer_add_listener (data->buffer, &buffer_listener, surface);

  cairo_surface_set_user_data (surface, &gdk_wayland_cairo_key,
                               data, gdk_wayland_cairo_surface_destroy);

  cairo_surface_set_device_scale (surface, scale, scale);

  status = cairo_surface_status (surface);
  if (status != CAIRO_STATUS_SUCCESS)
    {
      g_critical (G_STRLOC ": Unable to create Cairo image surface: %s",
                  cairo_status_to_string (status));
    }

  return surface;
}

struct wl_buffer *
_gdk_wayland_shm_surface_get_wl_buffer (cairo_surface_t *surface)
{
  GdkWaylandCairoSurfaceData *data = cairo_surface_get_user_data (surface, &gdk_wayland_cairo_key);
  return data->buffer;
}

void
_gdk_wayland_shm_surface_set_busy (cairo_surface_t *surface)
{
  GdkWaylandCairoSurfaceData *data = cairo_surface_get_user_data (surface, &gdk_wayland_cairo_key);
  data->busy = TRUE;
  cairo_surface_reference (surface);
}

gboolean
_gdk_wayland_shm_surface_get_busy (cairo_surface_t *surface)
{
  GdkWaylandCairoSurfaceData *data = cairo_surface_get_user_data (surface, &gdk_wayland_cairo_key);
  return data->busy;
}

gboolean
_gdk_wayland_is_shm_surface (cairo_surface_t *surface)
{
  return cairo_surface_get_user_data (surface, &gdk_wayland_cairo_key) != NULL;
}

GdkWaylandSelection *
gdk_wayland_display_get_selection (GdkDisplay *display)
{
  GdkWaylandDisplay *wayland_display = GDK_WAYLAND_DISPLAY (display);

  return wayland_display->selection;
}
