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

#ifdef HAVE_LINUX_MEMFD_H
#include <linux/memfd.h>
#endif

#include <sys/mman.h>
#include <sys/syscall.h>

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
#include "gdkwaylandmonitor.h"
#include "gdk-private.h"
#include "pointer-gestures-unstable-v1-client-protocol.h"
#include "tablet-unstable-v2-client-protocol.h"
#include "xdg-shell-unstable-v6-client-protocol.h"
#include "xdg-foreign-unstable-v1-client-protocol.h"
#include "xdg-foreign-unstable-v2-client-protocol.h"
#include "server-decoration-client-protocol.h"

#ifdef HAVE_TOPLEVEL_STATE_SUSPENDED
#define XDG_WM_BASE_VERSION      6
#else
#define XDG_WM_BASE_VERSION      2
#endif

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

#define MIN_SYSTEM_BELL_DELAY_MS 20

#define GTK_SHELL1_VERSION       5
#ifdef HAVE_XDG_ACTIVATION
#define XDG_ACTIVATION_VERSION   1
#endif

static void _gdk_wayland_display_load_cursor_theme (GdkWaylandDisplay *display_wayland);

G_DEFINE_TYPE (GdkWaylandDisplay, gdk_wayland_display, GDK_TYPE_DISPLAY)

static void
async_roundtrip_callback (void               *data,
                          struct wl_callback *callback,
                          uint32_t            time)
{
  GdkWaylandDisplay *display_wayland = data;

  display_wayland->async_roundtrips =
    g_list_remove (display_wayland->async_roundtrips, callback);
  wl_callback_destroy (callback);
}

static const struct wl_callback_listener async_roundtrip_listener = {
  async_roundtrip_callback
};

static void
_gdk_wayland_display_async_roundtrip (GdkWaylandDisplay *display_wayland)
{
  struct wl_callback *callback;

  callback = wl_display_sync (display_wayland->wl_display);
  wl_callback_add_listener (callback,
                            &async_roundtrip_listener,
                            display_wayland);
  display_wayland->async_roundtrips =
    g_list_append (display_wayland->async_roundtrips, callback);
}

static void
xdg_wm_base_ping (void               *data,
                  struct xdg_wm_base *xdg_wm_base,
                  uint32_t            serial)
{
  GDK_NOTE (EVENTS,
            g_message ("ping, shell %p, serial %u\n", xdg_wm_base, serial));

  xdg_wm_base_pong (xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
  xdg_wm_base_ping,
};

static void
zxdg_shell_v6_ping (void                 *data,
                    struct zxdg_shell_v6 *xdg_shell,
                    uint32_t              serial)
{
  GdkWaylandDisplay *display_wayland = data;

  _gdk_wayland_display_update_serial (display_wayland, serial);

  GDK_NOTE (EVENTS,
            g_message ("ping, shell %p, serial %u\n", xdg_shell, serial));

  zxdg_shell_v6_pong (xdg_shell, serial);
}

static const struct zxdg_shell_v6_listener zxdg_shell_v6_listener = {
  zxdg_shell_v6_ping,
};

static gboolean
is_known_global (gpointer key, gpointer value, gpointer user_data)
{
  const char *required_global = user_data;
  const char *known_global = value;

  return g_strcmp0 (required_global, known_global) == 0;
}

static gboolean
has_required_globals (GdkWaylandDisplay *display_wayland,
                      const char *required_globals[])
{
  int i = 0;

  while (required_globals[i])
    {
      if (g_hash_table_find (display_wayland->known_globals,
                             is_known_global,
                             (gpointer)required_globals[i]) == NULL)
        return FALSE;

      i++;
    }

  return TRUE;
}

typedef struct _OnHasGlobalsClosure OnHasGlobalsClosure;

typedef void (*HasGlobalsCallback) (GdkWaylandDisplay *display_wayland,
                                    OnHasGlobalsClosure *closure);

struct _OnHasGlobalsClosure
{
  HasGlobalsCallback handler;
  const char **required_globals;
};

static void
process_on_globals_closures (GdkWaylandDisplay *display_wayland)
{
  GList *iter;

  iter = display_wayland->on_has_globals_closures;
  while (iter != NULL)
    {
      GList *next = iter->next;
      OnHasGlobalsClosure *closure = iter->data;

      if (has_required_globals (display_wayland,
                                closure->required_globals))
        {
          closure->handler (display_wayland, closure);
          g_free (closure);
          display_wayland->on_has_globals_closures =
            g_list_delete_link (display_wayland->on_has_globals_closures, iter);
        }

      iter = next;
    }
}

typedef struct
{
  OnHasGlobalsClosure base;
  uint32_t id;
  uint32_t version;
} SeatAddedClosure;

static void
_gdk_wayland_display_add_seat (GdkWaylandDisplay *display_wayland,
                               uint32_t id,
                               uint32_t version)
{
  GdkDisplay *gdk_display = GDK_DISPLAY_OBJECT (display_wayland);
  struct wl_seat *seat;

  display_wayland->seat_version = MIN (version, 5);
  seat = wl_registry_bind (display_wayland->wl_registry,
                           id, &wl_seat_interface,
                           display_wayland->seat_version);
  _gdk_wayland_device_manager_add_seat (gdk_display->device_manager,
                                        id, seat);
  _gdk_wayland_display_async_roundtrip (display_wayland);
}

static void
seat_added_closure_run (GdkWaylandDisplay *display_wayland,
                        OnHasGlobalsClosure *closure)
{
  SeatAddedClosure *seat_added_closure = (SeatAddedClosure*)closure;

  _gdk_wayland_display_add_seat (display_wayland,
                                 seat_added_closure->id,
                                 seat_added_closure->version);
}

static void
postpone_on_globals_closure (GdkWaylandDisplay *display_wayland,
                             OnHasGlobalsClosure *closure)
{
  display_wayland->on_has_globals_closures =
    g_list_append (display_wayland->on_has_globals_closures, closure);
}

#ifdef G_ENABLE_DEBUG

static const char *
get_format_name (enum wl_shm_format format)
{
  int i;
#define FORMAT(s) { WL_SHM_FORMAT_ ## s, #s }
  struct { int format; const char *name; } formats[] = {
    FORMAT(ARGB8888),
    FORMAT(XRGB8888),
    FORMAT(C8),
    FORMAT(RGB332),
    FORMAT(BGR233),
    FORMAT(XRGB4444),
    FORMAT(XBGR4444),
    FORMAT(RGBX4444),
    FORMAT(BGRX4444),
    FORMAT(ARGB4444),
    FORMAT(ABGR4444),
    FORMAT(RGBA4444),
    FORMAT(BGRA4444),
    FORMAT(XRGB1555),
    FORMAT(XBGR1555),
    FORMAT(RGBX5551),
    FORMAT(BGRX5551),
    FORMAT(ARGB1555),
    FORMAT(ABGR1555),
    FORMAT(RGBA5551),
    FORMAT(BGRA5551),
    FORMAT(RGB565),
    FORMAT(BGR565),
    FORMAT(RGB888),
    FORMAT(BGR888),
    FORMAT(XBGR8888),
    FORMAT(RGBX8888),
    FORMAT(BGRX8888),
    FORMAT(ABGR8888),
    FORMAT(RGBA8888),
    FORMAT(BGRA8888),
    FORMAT(XRGB2101010),
    FORMAT(XBGR2101010),
    FORMAT(RGBX1010102),
    FORMAT(BGRX1010102),
    FORMAT(ARGB2101010),
    FORMAT(ABGR2101010),
    FORMAT(RGBA1010102),
    FORMAT(BGRA1010102),
    FORMAT(YUYV),
    FORMAT(YVYU),
    FORMAT(UYVY),
    FORMAT(VYUY),
    FORMAT(AYUV),
    FORMAT(NV12),
    FORMAT(NV21),
    FORMAT(NV16),
    FORMAT(NV61),
    FORMAT(YUV410),
    FORMAT(YVU410),
    FORMAT(YUV411),
    FORMAT(YVU411),
    FORMAT(YUV420),
    FORMAT(YVU420),
    FORMAT(YUV422),
    FORMAT(YVU422),
    FORMAT(YUV444),
    FORMAT(YVU444),
    { 0xffffffff, NULL }
  };
#undef FORMAT

  for (i = 0; formats[i].name; i++)
    {
      if (formats[i].format == format)
        return formats[i].name;
    }
  return NULL;
}
#endif

static void
wl_shm_format (void          *data,
               struct wl_shm *wl_shm,
               uint32_t       format)
{
  GDK_NOTE (MISC, g_message ("supported pixel format %s", get_format_name (format)));
}

static const struct wl_shm_listener wl_shm_listener = {
  wl_shm_format
};

static void
server_decoration_manager_default_mode (void                                          *data,
                                        struct org_kde_kwin_server_decoration_manager *manager,
                                        uint32_t                                       mode)
{
  g_assert (mode <= ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER);
  const char *modes[] = {
    [ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_NONE]   = "none",
    [ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_CLIENT] = "client",
    [ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER] = "server",
  };
  GdkWaylandDisplay *display_wayland = data;
  g_debug ("Compositor prefers decoration mode '%s'", modes[mode]);
  display_wayland->server_decoration_mode = mode;
}

static const struct org_kde_kwin_server_decoration_manager_listener server_decoration_listener = {
  .default_mode = server_decoration_manager_default_mode
};

gboolean
gdk_wayland_display_prefers_ssd (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  if (display_wayland->server_decoration_manager)
    return display_wayland->server_decoration_mode == ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER;
  return FALSE;
}

static void
gdk_registry_handle_global (void               *data,
                            struct wl_registry *registry,
                            uint32_t            id,
                            const char         *interface,
                            uint32_t            version)
{
  GdkWaylandDisplay *display_wayland = data;
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
      wl_shm_add_listener (display_wayland->shm, &wl_shm_listener, display_wayland);
    }
  else if (strcmp (interface, "xdg_wm_base") == 0)
    {
      display_wayland->xdg_wm_base_id = id;
      display_wayland->xdg_wm_base_version = version;
    }
  else if (strcmp (interface, "zxdg_shell_v6") == 0)
    {
      display_wayland->zxdg_shell_v6_id = id;
    }
  else if (strcmp (interface, "gtk_shell1") == 0)
    {
      display_wayland->gtk_shell =
        wl_registry_bind(display_wayland->wl_registry, id,
                         &gtk_shell1_interface,
                         MIN (version, GTK_SHELL1_VERSION));
      _gdk_wayland_screen_set_has_gtk_shell (display_wayland->screen);
      display_wayland->gtk_shell_version = version;
    }
  else if (strcmp (interface, "wl_output") == 0)
    {
      output =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_output_interface, MIN (version, 2));
      _gdk_wayland_screen_add_output (display_wayland->screen, id, output, MIN (version, 2));
      _gdk_wayland_display_async_roundtrip (display_wayland);
    }
  else if (strcmp (interface, "wl_seat") == 0)
    {
      SeatAddedClosure *closure;
      static const char *required_device_manager_globals[] = {
        "wl_compositor",
        "wl_data_device_manager",
        NULL
      };

      closure = g_new0 (SeatAddedClosure, 1);
      closure->base.handler = seat_added_closure_run;
      closure->base.required_globals = required_device_manager_globals;
      closure->id = id;
      closure->version = version;
      postpone_on_globals_closure (display_wayland, &closure->base);
    }
  else if (strcmp (interface, "wl_data_device_manager") == 0)
    {
      display_wayland->data_device_manager_version = MIN (version, 3);
      display_wayland->data_device_manager =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_data_device_manager_interface,
                          display_wayland->data_device_manager_version);
    }
  else if (strcmp (interface, "wl_subcompositor") == 0)
    {
      display_wayland->subcompositor =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_subcompositor_interface, 1);
    }
  else if (strcmp (interface, "zwp_pointer_gestures_v1") == 0)
    {
      display_wayland->pointer_gestures =
        wl_registry_bind (display_wayland->wl_registry,
                          id, &zwp_pointer_gestures_v1_interface,
                          MIN (version, GDK_ZWP_POINTER_GESTURES_V1_VERSION));
    }
  else if (strcmp (interface, "gtk_primary_selection_device_manager") == 0)
    {
      display_wayland->gtk_primary_selection_manager =
        wl_registry_bind(display_wayland->wl_registry, id,
                         &gtk_primary_selection_device_manager_interface, 1);
    }
  else if (strcmp (interface, "zwp_primary_selection_device_manager_v1") == 0)
    {
      display_wayland->zwp_primary_selection_manager_v1 =
        wl_registry_bind(display_wayland->wl_registry, id,
                         &zwp_primary_selection_device_manager_v1_interface, 1);
    }
  else if (strcmp (interface, "zwp_tablet_manager_v2") == 0)
    {
      display_wayland->tablet_manager =
        wl_registry_bind(display_wayland->wl_registry, id,
                         &zwp_tablet_manager_v2_interface, 1);
    }
  else if (strcmp (interface, "zxdg_exporter_v1") == 0)
    {
      display_wayland->xdg_exporter_v1 =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_exporter_v1_interface, 1);
    }
  else if (strcmp (interface, "zxdg_importer_v1") == 0)
    {
      display_wayland->xdg_importer_v1 =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_importer_v1_interface, 1);
    }
  else if (strcmp (interface, "zxdg_exporter_v2") == 0)
    {
      display_wayland->xdg_exporter_v2 =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_exporter_v2_interface, 1);
    }
  else if (strcmp (interface, "zxdg_importer_v2") == 0)
    {
      display_wayland->xdg_importer_v2 =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_importer_v2_interface, 1);
    }
  else if (strcmp (interface, "zwp_keyboard_shortcuts_inhibit_manager_v1") == 0)
    {
      display_wayland->keyboard_shortcuts_inhibit =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, 1);
    }
  else if (strcmp (interface, "org_kde_kwin_server_decoration_manager") == 0)
    {
      display_wayland->server_decoration_manager =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &org_kde_kwin_server_decoration_manager_interface, 1);
      org_kde_kwin_server_decoration_manager_add_listener (display_wayland->server_decoration_manager,
                                                           &server_decoration_listener,
                                                           display_wayland);
    }
  else if (strcmp(interface, "zxdg_output_manager_v1") == 0)
    {
      display_wayland->xdg_output_manager_version = MIN (version, 3);
      display_wayland->xdg_output_manager =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_output_manager_v1_interface,
                          display_wayland->xdg_output_manager_version);
      display_wayland->xdg_output_version = version;
      _gdk_wayland_screen_init_xdg_output (display_wayland->screen);
      _gdk_wayland_display_async_roundtrip (display_wayland);
    }
#ifdef HAVE_XDG_ACTIVATION
  else if (strcmp (interface, "xdg_activation_v1") == 0)
    {
      display_wayland->xdg_activation_version =
        MIN (version, XDG_ACTIVATION_VERSION);
      display_wayland->xdg_activation =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &xdg_activation_v1_interface,
                          display_wayland->xdg_activation_version);
    }
#endif
  else if (strcmp (interface, wp_cursor_shape_manager_v1_interface.name) == 0)
    {
      display_wayland->cursor_shape =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wp_cursor_shape_manager_v1_interface, 1);
    }

  g_hash_table_insert (display_wayland->known_globals,
                       GUINT_TO_POINTER (id), g_strdup (interface));
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

  g_hash_table_remove (display_wayland->known_globals, GUINT_TO_POINTER (id));

  /* FIXME: the object needs to be destroyed here, we're leaking */
}

static const struct wl_registry_listener registry_listener = {
    gdk_registry_handle_global,
    gdk_registry_handle_global_remove
};

static void
log_handler (const char *format, va_list args)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
}

static void
load_cursor_theme_closure_run (GdkWaylandDisplay *display_wayland,
                               OnHasGlobalsClosure *closure)
{
  _gdk_wayland_display_load_cursor_theme (display_wayland);
}

static void
_gdk_wayland_display_prepare_cursor_themes (GdkWaylandDisplay *display_wayland)
{
  OnHasGlobalsClosure *closure;
  static const char *required_cursor_theme_globals[] = {
      "wl_shm",
      NULL
  };

  closure = g_new0 (OnHasGlobalsClosure, 1);
  closure->handler = load_cursor_theme_closure_run;
  closure->required_globals = required_cursor_theme_globals;
  postpone_on_globals_closure (display_wayland, closure);
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

  display_wayland->known_globals =
    g_hash_table_new_full (NULL, NULL, NULL, g_free);

  _gdk_wayland_display_init_cursors (display_wayland);
  _gdk_wayland_display_prepare_cursor_themes (display_wayland);

  display_wayland->wl_registry = wl_display_get_registry (display_wayland->wl_display);
  wl_registry_add_listener (display_wayland->wl_registry, &registry_listener, display_wayland);
  if (wl_display_roundtrip (display_wayland->wl_display) < 0)
    {
      g_object_unref (display);
      return NULL;
    }

  process_on_globals_closures (display_wayland);
  display_wayland->selection = gdk_wayland_selection_new ();

  /* Wait for initializing to complete. This means waiting for all
   * asynchrounous roundtrips that were triggered during initial roundtrip. */
  while (g_list_length (display_wayland->async_roundtrips) > 0)
    {
      if (wl_display_dispatch (display_wayland->wl_display) < 0)
        {
          g_object_unref (display);
          return NULL;
        }
    }

  if (display_wayland->xdg_wm_base_id)
    {
      display_wayland->shell_variant = GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL;
      display_wayland->xdg_wm_base =
        wl_registry_bind (display_wayland->wl_registry,
                          display_wayland->xdg_wm_base_id,
                          &xdg_wm_base_interface,
                          MIN (display_wayland->xdg_wm_base_version, XDG_WM_BASE_VERSION));
      xdg_wm_base_add_listener (display_wayland->xdg_wm_base,
                                &xdg_wm_base_listener,
                                display_wayland);
    }
  else if (display_wayland->zxdg_shell_v6_id)
    {
      display_wayland->shell_variant = GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6;
      display_wayland->zxdg_shell_v6 =
        wl_registry_bind (display_wayland->wl_registry,
                          display_wayland->zxdg_shell_v6_id,
                          &zxdg_shell_v6_interface, 1);
      zxdg_shell_v6_add_listener (display_wayland->zxdg_shell_v6,
                                  &zxdg_shell_v6_listener,
                                  display_wayland);
    }
  else
    {
      g_warning ("The Wayland compositor does not provide any supported shell interface, "
                 "not using Wayland display");
      g_object_unref (display);

      return NULL;
    }

  g_signal_emit_by_name (display, "opened");

  return display;
}

static void
gdk_wayland_display_dispose (GObject *object)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (object);

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

  g_list_free_full (display_wayland->async_roundtrips, (GDestroyNotify) wl_callback_destroy);

  if (display_wayland->known_globals)
    {
      g_hash_table_destroy (display_wayland->known_globals);
      display_wayland->known_globals = NULL;
    }

  g_list_free_full (display_wayland->on_has_globals_closures, g_free);

  G_OBJECT_CLASS (gdk_wayland_display_parent_class)->dispose (object);
}

static void
gdk_wayland_display_finalize (GObject *object)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (object);
  guint i;

  _gdk_wayland_display_finalize_cursors (display_wayland);

  g_object_unref (display_wayland->screen);

  g_free (display_wayland->startup_notification_id);
  g_free (display_wayland->cursor_theme_name);
  xkb_context_unref (display_wayland->xkb_context);

  for (i = 0; i < GDK_WAYLAND_THEME_SCALES_COUNT; i++)
    {
      if (display_wayland->scaled_cursor_themes[i])
        {
          wl_cursor_theme_destroy (display_wayland->scaled_cursor_themes[i]);
          display_wayland->scaled_cursor_themes[i] = NULL;
        }
    }

  g_ptr_array_free (display_wayland->monitors, TRUE);

  wl_display_disconnect(display_wayland->wl_display);

  G_OBJECT_CLASS (gdk_wayland_display_parent_class)->finalize (object);
}

static const gchar *
gdk_wayland_display_get_name (GdkDisplay *display)
{
  const gchar *name;

  name = g_getenv ("WAYLAND_DISPLAY");
  if (name == NULL)
    name = "wayland-0";

  return name;
}

static GdkScreen *
gdk_wayland_display_get_default_screen (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  return GDK_WAYLAND_DISPLAY (display)->screen;
}

void
gdk_wayland_display_system_bell (GdkDisplay *display,
                                 GdkWindow  *window)
{
  GdkWaylandDisplay *display_wayland;
  struct gtk_surface1 *gtk_surface;
  gint64 now_ms;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (!display_wayland->gtk_shell)
    return;

  if (window)
    gtk_surface = gdk_wayland_window_get_gtk_surface (window);
  else
    gtk_surface = NULL;

  now_ms = g_get_monotonic_time () / 1000;
  if (now_ms - display_wayland->last_bell_time_ms < MIN_SYSTEM_BELL_DELAY_MS)
    return;

  display_wayland->last_bell_time_ms = now_ms;

  gtk_shell1_system_bell (display_wayland->gtk_shell, gtk_surface);
}

static void
gdk_wayland_display_beep (GdkDisplay *display)
{
  gdk_wayland_display_system_bell (display, NULL);
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

static void
gdk_wayland_display_make_default (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  const gchar *startup_id;

  g_free (display_wayland->startup_notification_id);
  display_wayland->startup_notification_id = NULL;

  startup_id = gdk_get_desktop_startup_id ();
  if (startup_id)
    display_wayland->startup_notification_id = g_strdup (startup_id);
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

/**
 * gdk_wayland_display_set_startup_notification_id:
 * @display: (type GdkWaylandDisplay): a #GdkDisplay
 * @startup_id: the startup notification ID (must be valid utf8)
 *
 * Sets the startup notification ID for a display.
 *
 * This is usually taken from the value of the DESKTOP_STARTUP_ID
 * environment variable, but in some cases (such as the application not
 * being launched using exec()) it can come from other sources.
 *
 * The startup ID is also what is used to signal that the startup is
 * complete (for example, when opening a window or when calling
 * gdk_notify_startup_complete()).
 *
 * Since: 3.22
 **/
void
gdk_wayland_display_set_startup_notification_id (GdkDisplay *display,
                                                 const char *startup_id)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  g_free (display_wayland->startup_notification_id);
  display_wayland->startup_notification_id = g_strdup (startup_id);
}

static void
gdk_wayland_display_notify_startup_complete (GdkDisplay  *display,
					     const gchar *startup_id)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

#ifdef HAVE_XDG_ACTIVATION
  /* Will be signaled with focus activation */
  if (display_wayland->xdg_activation)
    return;
#endif

  if (startup_id == NULL)
    {
      startup_id = display_wayland->startup_notification_id;

      if (startup_id == NULL)
        return;
    }

#ifdef HAVE_XDG_ACTIVATION
  if (display_wayland->xdg_activation) /* FIXME: Isn't this redundant? */
    return;
#endif
  if (display_wayland->gtk_shell)
    gtk_shell1_set_startup_id (display_wayland->gtk_shell, startup_id);
}

static GdkKeymap *
_gdk_wayland_display_get_keymap (GdkDisplay *display)
{
  GdkDevice *core_keyboard = NULL;
  static GdkKeymap *tmp_keymap = NULL;

  core_keyboard = gdk_seat_get_keyboard (gdk_display_get_default_seat (display));

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

static int
gdk_wayland_display_get_n_monitors (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  return display_wayland->monitors->len;
}

static GdkMonitor *
gdk_wayland_display_get_monitor (GdkDisplay *display,
                                 int         monitor_num)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (monitor_num < 0 || monitor_num >= display_wayland->monitors->len)
    return NULL;

  return (GdkMonitor *)display_wayland->monitors->pdata[monitor_num];
}

static GdkMonitor *
gdk_wayland_display_get_monitor_at_window (GdkDisplay *display,
                                           GdkWindow  *window)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  struct wl_output *output;
  int i;

  g_return_val_if_fail (GDK_IS_WAYLAND_WINDOW (window), NULL);

  output = gdk_wayland_window_get_wl_output (window);
  if (output == NULL)
    return NULL;

  for (i = 0; i < display_wayland->monitors->len; i++)
    {
      GdkMonitor *monitor = display_wayland->monitors->pdata[i];

      if (gdk_wayland_monitor_get_wl_output (monitor) == output)
        return monitor;
    }

  return NULL;
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
  display_class->make_default = gdk_wayland_display_make_default;
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

  display_class->get_n_monitors = gdk_wayland_display_get_n_monitors;
  display_class->get_monitor = gdk_wayland_display_get_monitor;
  display_class->get_monitor_at_window = gdk_wayland_display_get_monitor_at_window;
}

static void
gdk_wayland_display_init (GdkWaylandDisplay *display)
{
  display->xkb_context = xkb_context_new (0);

  display->monitors = g_ptr_array_new_with_free_func (g_object_unref);
}

void
gdk_wayland_display_set_cursor_theme (GdkDisplay  *display,
                                      const gchar *name,
                                      gint         size)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY(display);
  struct wl_cursor_theme *theme;
  int i;

  g_assert (display_wayland);
  g_assert (display_wayland->shm);

  if (size == 0)
    size = 24;

  if (g_strcmp0 (name, display_wayland->cursor_theme_name) == 0 &&
      display_wayland->cursor_theme_size == size)
    return;

  theme = wl_cursor_theme_load (name, size, display_wayland->shm);
  if (theme == NULL)
    {
      g_warning ("Failed to load cursor theme %s", name);
      return;
    }

  for (i = 0; i < GDK_WAYLAND_THEME_SCALES_COUNT; i++)
    {
      if (display_wayland->scaled_cursor_themes[i])
        {
          wl_cursor_theme_destroy (display_wayland->scaled_cursor_themes[i]);
          display_wayland->scaled_cursor_themes[i] = NULL;
        }
    }
  display_wayland->scaled_cursor_themes[0] = theme;
  if (display_wayland->cursor_theme_name != NULL)
    g_free (display_wayland->cursor_theme_name);
  display_wayland->cursor_theme_name = g_strdup (name);
  display_wayland->cursor_theme_size = size;

  _gdk_wayland_display_update_cursors (display_wayland);
}

struct wl_cursor_theme *
_gdk_wayland_display_get_scaled_cursor_theme (GdkWaylandDisplay *display_wayland,
                                              guint              scale)
{
  struct wl_cursor_theme *theme;

  g_assert (display_wayland->cursor_theme_name);
  g_assert (scale <= GDK_WAYLAND_MAX_THEME_SCALE);
  g_assert (scale >= 1);

  theme = display_wayland->scaled_cursor_themes[scale - 1];
  if (!theme)
    {
      theme = wl_cursor_theme_load (display_wayland->cursor_theme_name,
                                    display_wayland->cursor_theme_size * scale,
                                    display_wayland->shm);
      if (theme == NULL)
        {
          g_warning ("Failed to load cursor theme %s with scale %u",
                     display_wayland->cursor_theme_name, scale);
          return NULL;
        }
      display_wayland->scaled_cursor_themes[scale - 1] = theme;
    }

  return theme;
}

static void
_gdk_wayland_display_load_cursor_theme (GdkWaylandDisplay *display_wayland)
{
  guint size;
  const gchar *name;
  GValue v = G_VALUE_INIT;

  g_assert (display_wayland);
  g_assert (display_wayland->shm);

  g_value_init (&v, G_TYPE_INT);
  if (gdk_screen_get_setting (display_wayland->screen, "gtk-cursor-theme-size", &v))
    size = g_value_get_int (&v);
  else
    size = 24;
  g_value_unset (&v);

  g_value_init (&v, G_TYPE_STRING);
  if (gdk_screen_get_setting (display_wayland->screen, "gtk-cursor-theme-name", &v))
    name = g_value_get_string (&v);
  else
    name = "default";

  gdk_wayland_display_set_cursor_theme (GDK_DISPLAY (display_wayland), name, size);
  g_value_unset (&v);
}

guint32
_gdk_wayland_display_get_serial (GdkWaylandDisplay *display_wayland)
{
  return display_wayland->serial;
}

void
_gdk_wayland_display_update_serial (GdkWaylandDisplay *display_wayland,
                                    guint32            serial)
{
  display_wayland->serial = serial;
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

static const cairo_user_data_key_t gdk_wayland_shm_surface_cairo_key;

typedef struct _GdkWaylandCairoSurfaceData {
  gpointer buf;
  size_t buf_length;
  struct wl_shm_pool *pool;
  struct wl_buffer *buffer;
  GdkWaylandDisplay *display;
  uint32_t scale;
} GdkWaylandCairoSurfaceData;

static int
open_shared_memory (void)
{
  static gboolean force_shm_open = FALSE;
  int ret = -1;

#if !defined (__NR_memfd_create)
  force_shm_open = TRUE;
#endif

  do
    {
#if defined (__NR_memfd_create)
      if (!force_shm_open)
        {
          ret = syscall (__NR_memfd_create, "gdk-wayland", MFD_CLOEXEC);

          /* fall back to shm_open until debian stops shipping 3.16 kernel
           * See bug 766341
           */
          if (ret < 0 && errno == ENOSYS)
            force_shm_open = TRUE;
        }
#endif

      if (force_shm_open)
        {
#if defined (__FreeBSD__)
          ret = shm_open (SHM_ANON, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);
#else
          char name[NAME_MAX - 1] = "";

          sprintf (name, "/gdk-wayland-%x", g_random_int ());

          ret = shm_open (name, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);

          if (ret >= 0)
            shm_unlink (name);
          else if (errno == EEXIST)
            continue;
#endif
        }
    }
  while (ret < 0 && errno == EINTR);

  if (ret < 0)
    g_critical (G_STRLOC ": creating shared memory file (using %s) failed: %m",
                force_shm_open? "shm_open" : "memfd_create");

  return ret;
}

static struct wl_shm_pool *
create_shm_pool (struct wl_shm  *shm,
                 int             size,
                 size_t         *buf_length,
                 void          **data_out)
{
  struct wl_shm_pool *pool;
  int fd;
  void *data;

  fd = open_shared_memory ();

  if (fd < 0)
    return NULL;

  if (ftruncate (fd, size) < 0)
    {
      g_critical (G_STRLOC ": Truncating shared memory file failed: %m");
      close (fd);
      return NULL;
    }

  data = mmap (NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  if (data == MAP_FAILED)
    {
      g_critical (G_STRLOC ": mmap'ping shared memory file failed: %m");
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

  stride = cairo_format_stride_for_width (CAIRO_FORMAT_ARGB32, width*scale);

  data->pool = create_shm_pool (display->shm,
                                height*scale*stride,
                                &data->buf_length,
                                &data->buf);

  surface = cairo_image_surface_create_for_data (data->buf,
                                                 CAIRO_FORMAT_ARGB32,
                                                 width*scale,
                                                 height*scale,
                                                 stride);

  data->buffer = wl_shm_pool_create_buffer (data->pool, 0,
                                            width*scale, height*scale,
                                            stride, WL_SHM_FORMAT_ARGB8888);

  cairo_surface_set_user_data (surface, &gdk_wayland_shm_surface_cairo_key,
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
  GdkWaylandCairoSurfaceData *data = cairo_surface_get_user_data (surface, &gdk_wayland_shm_surface_cairo_key);
  return data->buffer;
}

gboolean
_gdk_wayland_is_shm_surface (cairo_surface_t *surface)
{
  return cairo_surface_get_user_data (surface, &gdk_wayland_shm_surface_cairo_key) != NULL;
}

GdkWaylandSelection *
gdk_wayland_display_get_selection (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  return display_wayland->selection;
}

/**
 * gdk_wayland_display_query_registry:
 * @display: a wayland #GdkDisplay
 * @interface: global interface to query in the registry
 *
 * Returns %TRUE if the the interface was found in the display
 * wl_registry.global handler.
 *
 * Returns: %TRUE if the global is offered by the compositor
 *
 * Since: 3.22.27
 **/
gboolean
gdk_wayland_display_query_registry (GdkDisplay  *display,
				    const gchar *global)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GHashTableIter iter;
  gchar *value;

  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), FALSE);
  g_return_val_if_fail (global != NULL, FALSE);

  g_hash_table_iter_init (&iter, display_wayland->known_globals);

  while (g_hash_table_iter_next (&iter, NULL, (gpointer*) &value))
    {
      if (strcmp (value, global) == 0)
        return TRUE;
    }

  return FALSE;
}
