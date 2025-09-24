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

#define VK_USE_PLATFORM_WAYLAND_KHR

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include <glib.h>
#include <gio/gio.h>
#include "gdkwayland.h"
#include "gdkdisplay.h"
#include "gdkdisplay-wayland.h"
#include "gdkmonitor-wayland.h"
#include "gdkseat-wayland.h"
#include "gdksurfaceprivate.h"
#include "gdkdeviceprivate.h"
#include "gdkdevice-wayland-private.h"
#include "gdksurface-wayland-private.h"
#include "gdkkeymap-wayland.h"
#include "gdkcairocontext-wayland.h"
#include "gdkglcontext-wayland.h"
#include "gdkvulkancontext-wayland.h"
#include "gdkwaylandmonitor.h"
#include "gdkprofilerprivate.h"
#include "gdktoplevel-wayland-private.h"
#include "gdkwaylandcolor-private.h"
#include "gdkshm-private.h"
#include "gdksettings-wayland.h"
#include "gdkapplaunchcontext-wayland.h"
#include "gdkeventsource.h"

#include <wayland/pointer-gestures-unstable-v1-client-protocol.h>
#include "tablet-v2-client-protocol.h"
#include <wayland/xdg-shell-unstable-v6-client-protocol.h>
#include <wayland/xdg-foreign-unstable-v1-client-protocol.h>
#include <wayland/xdg-foreign-unstable-v2-client-protocol.h>
#include <wayland/server-decoration-client-protocol.h>
#include "linux-dmabuf-unstable-v1-client-protocol.h"
#include "presentation-time-client-protocol.h"
#include "color-management-v1-client-protocol.h"
#include "color-representation-v1-client-protocol.h"
#include "xx-session-management-v1-client-protocol.h"

#include "wm-button-layout-translation.h"

#include "gdk/gdkprivate.h"

/* Keep g_assert() defined even if we disable it globally,
 * as we use it in many places as a handy mechanism to check
 * for non-NULL
 */
#ifdef G_DISABLE_ASSERT
# undef g_assert
# define g_assert(expr)                  G_STMT_START { \
                                           if G_LIKELY (expr) ; else \
                                             g_assertion_message_expr (G_LOG_DOMAIN, __FILE__, __LINE__, G_STRFUNC, \
                                                                       #expr); \
                                         } G_STMT_END
#endif

/**
 * GdkWaylandDisplay:
 *
 * The Wayland implementation of `GdkDisplay`.
 *
 * Beyond the regular [class@Gdk.Display] API, the Wayland implementation
 * provides access to Wayland objects such as the `wl_display` with
 * [method@GdkWayland.WaylandDisplay.get_wl_display], the `wl_compositor` with
 * [method@GdkWayland.WaylandDisplay.get_wl_compositor].
 *
 * You can find out what Wayland globals are supported by a display
 * with [method@GdkWayland.WaylandDisplay.query_registry].
 */

#define MIN_SYSTEM_BELL_DELAY_MS 20

#define COMPOSITOR_VERSION              6
#define SHM_VERSION                     1
#define LINUX_DMABUF_MIN_VERSION        4
#define LINUX_DMABUF_VERSION            5
#define DATA_DEVICE_MANAGER_VERSION     3
#define SUBCOMPOSITOR_VERSION           1
#define SEAT_VERSION                    10
#define POINTER_GESTURES_VERSION        3
#define PRIMARY_SELECTION_VERSION       1
#define OUTPUT_MIN_VERSION              2
#define OUTPUT_VERSION                  4
#define XDG_OUTPUT_MIN_VERSION          3
#define XDG_OUTPUT_VERSION              3
#define TABLET_VERSION                  2
#define EXPORTER_V1_VERSION             1
#define EXPORTER_V2_VERSION             1
#define IMPORTER_V1_VERSION             1
#define IMPORTER_V2_VERSION             1
#define SHORTCUTS_INHIBIT_VERSION       1
#define SERVER_DECORATION_VERSION       1
#define XDG_OUTPUT_VERSION              3
#define IDLE_INHIBIT_VERSION            1
#define ACTIVATION_VERSION              1
#define FRACTIONAL_SCALE_VERSION        1
#define VIEWPORTER_VERSION              1
#define PRESENTATION_VERSION            1
#define SINGLE_PIXEL_BUFFER_VERSION     1
#define SYSTEM_BELL_VERSION             1
#define CURSOR_SHAPE_VERSION            1
#define GTK_SHELL1_VERSION              6
#define XDG_WM_DIALOG_VERSION           1
#define XDG_TOPLEVEL_ICON_VERSION       1
#define XDG_WM_BASE_VERSION             7

G_DEFINE_TYPE (GdkWaylandDisplay, gdk_wayland_display, GDK_TYPE_DISPLAY)

G_GNUC_PRINTF (1, 0)
static void
log_handler (const char *format, va_list args)
{
  g_logv (G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, format, args);
}

/* {{{ GDK_WAYLAND_DISABLE filtering */

static void
init_skip_protocols (GdkWaylandDisplay *display_wayland)
{
  const char *variable, *string, *p, *q;
  gboolean help;
  GStrvBuilder *strv;

  variable = "GDK_WAYLAND_DISABLE";
  string = g_getenv (variable);

  help = FALSE;

  if (string == NULL)
    string = "";

  strv = g_strv_builder_new ();

  p = string;
  while (*p)
    {
      q = strpbrk (p, ":;, \t");
      if (!q)
        q = p + strlen (p);

      if (g_ascii_strncasecmp ("help", p, q - p) == 0)
        help = TRUE;
      else
        g_strv_builder_take (strv, g_strndup (p, q - p));

      p = q;
      if (*p)
        p++;
    }

  if (!GDK_DISPLAY_DEBUG_CHECK (GDK_DISPLAY (display_wayland), COLOR_MANAGEMENT))
    g_strv_builder_add (strv, wp_color_manager_v1_interface.name);

  if (!GDK_DISPLAY_DEBUG_CHECK (GDK_DISPLAY (display_wayland), SESSION_MANAGEMENT))
    g_strv_builder_add (strv, xx_session_manager_v1_interface.name);

  if (help)
    {
      gdk_help_message ("%s can be set to a list of Wayland interfaces to disable.\n", variable);
      if (!GDK_DISPLAY_DEBUG_CHECK (GDK_DISPLAY (display_wayland), MISC))
        {
          gdk_help_message ("Supported %s values:", variable);
          display_wayland->list_protocols = TRUE;
        }
    }

  display_wayland->skip_protocols = g_strv_builder_unref_to_strv (strv);
}

static gboolean
match_global (GdkWaylandDisplay *display_wayland,
              const char        *interface,
              uint32_t           version,
              const char        *name,
              uint32_t           min_version)
{
  if (strcmp (interface, name) != 0)
    return FALSE;

  if (version < min_version)
    {
      GDK_DISPLAY_DEBUG (GDK_DISPLAY (display_wayland), MISC,
                         "Not using %s, too old (%u < %u)",
                         interface, version, min_version);
      return FALSE;
    }

  if (g_strv_contains ((const char * const *) display_wayland->skip_protocols, name))
    {
      GDK_DISPLAY_DEBUG (GDK_DISPLAY (display_wayland), MISC,
                         "Not using %s, disabled",
                         interface);
      return FALSE;
    }

  return TRUE;
}

/* }}} */
/* {{{ Async roundtrips */

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

/* }}} */
/* {{{ Global dependency handling */

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
gdk_wayland_display_add_seat (GdkWaylandDisplay *display_wayland,
                              uint32_t id,
                              uint32_t version)
{
  struct wl_seat *seat;

  seat = wl_registry_bind (display_wayland->wl_registry,
                           id, &wl_seat_interface,
                           MIN (version, SEAT_VERSION));
  gdk_wayland_display_create_seat (display_wayland, id, seat);
  _gdk_wayland_display_async_roundtrip (display_wayland);
}

static void
seat_added_closure_run (GdkWaylandDisplay *display_wayland,
                        OnHasGlobalsClosure *closure)
{
  SeatAddedClosure *seat_added_closure = (SeatAddedClosure*)closure;

  gdk_wayland_display_add_seat (display_wayland,
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

/* }}} */
/* {{{ gtk_shell listener */

static void
gtk_shell_handle_capabilities (void              *data,
                               struct gtk_shell1 *shell,
                               uint32_t           capabilities)
{
  GdkDisplay *display = data;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (data);

  display_wayland->shell_capabilities = capabilities;

  gdk_display_setting_changed (display, "gtk-shell-shows-app-menu");
  gdk_display_setting_changed (display, "gtk-shell-shows-menubar");
  gdk_display_setting_changed (display, "gtk-shell-shows-desktop");
}

struct gtk_shell1_listener gdk_display_gtk_shell_listener = {
  gtk_shell_handle_capabilities
};

/* }}} */
/* {{{ xdg_wm_base listener */

static void
xdg_wm_base_ping (void               *data,
                  struct xdg_wm_base *xdg_wm_base,
                  uint32_t            serial)
{
  GDK_DEBUG (EVENTS, "ping, shell %p, serial %u", xdg_wm_base, serial);

  xdg_wm_base_pong (xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
  xdg_wm_base_ping,
};

/* }}} */
/* {{{ zxdg_shell_v6 listener */

static void
zxdg_shell_v6_ping (void                 *data,
                    struct zxdg_shell_v6 *xdg_shell,
                    uint32_t              serial)
{
  GDK_DISPLAY_DEBUG (GDK_DISPLAY (data), EVENTS,
                     "ping, shell %p, serial %u", xdg_shell, serial);

  zxdg_shell_v6_pong (xdg_shell, serial);
}

static const struct zxdg_shell_v6_listener zxdg_shell_v6_listener = {
  zxdg_shell_v6_ping,
};

/* }}} */
/* {{{ wl_shm listener */

static void
wl_shm_format (void          *data,
               struct wl_shm *wl_shm,
               uint32_t       format)
{
  GDK_DEBUG (MISC, "supported shm pixel format %.4s (0x%X)",
             format == 0 ? "ARGB8888"
               : (format == 1 ? "XRGB8888"
               : (char *) &format), format);
}

static const struct wl_shm_listener wl_shm_listener = {
  wl_shm_format
};

 /* }}} */
/* {{{ server_decoration listener */

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
  GDK_DISPLAY_DEBUG (GDK_DISPLAY (data), MISC, "Compositor prefers decoration mode '%s'", modes[mode]);
  display_wayland->server_decoration_mode = mode;
}

static const struct org_kde_kwin_server_decoration_manager_listener server_decoration_listener = {
  .default_mode = server_decoration_manager_default_mode
};

/* }}} */
/* {{{ session listener */

static void
session_listener_created (void                 *data,
                          struct xx_session_v1 *xx_session_v1,
                          const char           *id)
{
  GdkWaylandDisplay *display_wayland = data;

  GDK_DEBUG (MISC, "session created: %s", id);

  if (g_strcmp0 (display_wayland->session_id, id) != 0)
    {
      g_clear_pointer (&display_wayland->session_id, g_free);
      display_wayland->session_id = g_strdup (id);
    }
}

static void
session_listener_restored (void                 *data,
                           struct xx_session_v1 *xx_session_v1)
{
  GdkWaylandDisplay *display_wayland = data;

  GDK_DEBUG (MISC, "session restored: %s", display_wayland->session_id);
}

static void
session_listener_replaced (void                 *data,
                           struct xx_session_v1 *xx_session_v1)
{
  GdkWaylandDisplay *display_wayland = data;

  GDK_DEBUG (MISC, "session replaced: %s", display_wayland->session_id);
}

static const struct xx_session_v1_listener xx_session_listener = {
  .created = session_listener_created,
  .restored = session_listener_restored,
  .replaced = session_listener_replaced,
};

/* }}} */
/* {{{ wl_registry listener */

static void
gdk_registry_handle_global (void               *data,
                            struct wl_registry *registry,
                            uint32_t            id,
                            const char         *interface,
                            uint32_t            version)
{
  GdkWaylandDisplay *display_wayland = data;
  struct wl_output *output;

  if (display_wayland->list_protocols)
    gdk_debug_message ("    %s", interface);
  else
    GDK_DEBUG (MISC, "add global %u, interface %s, version %u", id, interface, version);

  if (match_global (display_wayland, interface, version, wl_compositor_interface.name, 0))
    {
      display_wayland->compositor =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wl_compositor_interface, MIN (version, COMPOSITOR_VERSION));
    }
  else if (match_global (display_wayland, interface, version, wl_shm_interface.name, 0))
    {
      display_wayland->shm =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_shm_interface, SHM_VERSION);
      wl_shm_add_listener (display_wayland->shm, &wl_shm_listener, display_wayland);
    }
  else if (match_global (display_wayland, interface, version, zwp_linux_dmabuf_v1_interface.name, LINUX_DMABUF_MIN_VERSION))
    {
      struct zwp_linux_dmabuf_feedback_v1 *feedback;

      display_wayland->linux_dmabuf =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zwp_linux_dmabuf_v1_interface,
                          MIN (version, LINUX_DMABUF_VERSION));
      feedback = zwp_linux_dmabuf_v1_get_default_feedback (display_wayland->linux_dmabuf);
      display_wayland->dmabuf_formats_info = dmabuf_formats_info_new (GDK_DISPLAY (display_wayland),
                                                                      "default",
                                                                      feedback);
      _gdk_wayland_display_async_roundtrip (display_wayland);
    }
  else if (match_global (display_wayland, interface, version, xdg_wm_base_interface.name, 0))
    {
      display_wayland->xdg_wm_base_id = id;
      display_wayland->xdg_wm_base_version = version;
    }
  else if (match_global (display_wayland, interface, version, zxdg_shell_v6_interface.name, 0))
    {
      display_wayland->zxdg_shell_v6_id = id;
    }
  else if (match_global (display_wayland, interface, version, xdg_wm_dialog_v1_interface.name, 0))
    {
      display_wayland->xdg_wm_dialog =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &xdg_wm_dialog_v1_interface,
                          MIN (version, XDG_WM_DIALOG_VERSION));
    }
  else if (match_global (display_wayland, interface, version, xx_session_manager_v1_interface.name, 0))
    {
      display_wayland->xx_session_manager =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &xx_session_manager_v1_interface,
                          version);
    }
  else if (match_global (display_wayland, interface, version, gtk_shell1_interface.name, 0))
    {
      display_wayland->gtk_shell =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &gtk_shell1_interface,
                          MIN (version, GTK_SHELL1_VERSION));
      gtk_shell1_add_listener (display_wayland->gtk_shell, &gdk_display_gtk_shell_listener, display_wayland);
    }
  else if (match_global (display_wayland, interface, version, wl_output_interface.name, OUTPUT_MIN_VERSION))
    {
      output =
       wl_registry_bind (display_wayland->wl_registry, id, &wl_output_interface,
                         MIN (version, OUTPUT_VERSION));
      gdk_wayland_display_add_output (display_wayland, id, output);
      _gdk_wayland_display_async_roundtrip (display_wayland);
    }
  else if (match_global (display_wayland, interface, version, wl_seat_interface.name, 0))
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
  else if (match_global (display_wayland, interface, version, wl_data_device_manager_interface.name, 0))
    {
      display_wayland->data_device_manager =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_data_device_manager_interface,
                          MIN (version, DATA_DEVICE_MANAGER_VERSION));
    }
  else if (match_global (display_wayland, interface, version, wl_subcompositor_interface.name, 0))
    {
      display_wayland->subcompositor =
        wl_registry_bind (display_wayland->wl_registry, id, &wl_subcompositor_interface, SUBCOMPOSITOR_VERSION);
    }
  else if (match_global (display_wayland, interface, version, zwp_pointer_gestures_v1_interface.name, 0))
    {
      display_wayland->pointer_gestures =
        wl_registry_bind (display_wayland->wl_registry,
                          id, &zwp_pointer_gestures_v1_interface,
                          MIN (version, POINTER_GESTURES_VERSION));
    }
  else if (match_global (display_wayland, interface, version, zwp_primary_selection_device_manager_v1_interface.name, 0))
    {
      display_wayland->primary_selection_manager =
        wl_registry_bind(display_wayland->wl_registry, id,
                         &zwp_primary_selection_device_manager_v1_interface, PRIMARY_SELECTION_VERSION);
    }
  else if (match_global (display_wayland, interface, version, zwp_tablet_manager_v2_interface.name, 0))
    {
      display_wayland->tablet_manager =
        wl_registry_bind(display_wayland->wl_registry, id,
                         &zwp_tablet_manager_v2_interface, MIN (version, TABLET_VERSION));
    }
  else if (match_global (display_wayland, interface, version, zxdg_exporter_v1_interface.name, 0))
    {
      display_wayland->xdg_exporter =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_exporter_v1_interface, EXPORTER_V1_VERSION);
    }
  else if (match_global (display_wayland, interface, version, zxdg_exporter_v2_interface.name, 0))
    {
      display_wayland->xdg_exporter_v2 =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_exporter_v2_interface, EXPORTER_V2_VERSION);
    }
  else if (match_global (display_wayland, interface, version, zxdg_importer_v1_interface.name, 0))
    {
      display_wayland->xdg_importer =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_importer_v1_interface, IMPORTER_V1_VERSION);
    }
  else if (match_global (display_wayland, interface, version, zxdg_importer_v2_interface.name, 0))
    {
      display_wayland->xdg_importer_v2 =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_importer_v2_interface, IMPORTER_V2_VERSION);
    }
  else if (match_global (display_wayland, interface, version, zwp_keyboard_shortcuts_inhibit_manager_v1_interface.name, 0))
    {
      display_wayland->keyboard_shortcuts_inhibit =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zwp_keyboard_shortcuts_inhibit_manager_v1_interface, SHORTCUTS_INHIBIT_VERSION);
    }
  else if (match_global (display_wayland, interface, version, org_kde_kwin_server_decoration_manager_interface.name, 0))
    {
      display_wayland->server_decoration_manager =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &org_kde_kwin_server_decoration_manager_interface, SERVER_DECORATION_VERSION);
      org_kde_kwin_server_decoration_manager_add_listener (display_wayland->server_decoration_manager,
                                                           &server_decoration_listener,
                                                           display_wayland);
    }
  else if (match_global (display_wayland, interface, version, zxdg_output_manager_v1_interface.name, XDG_OUTPUT_MIN_VERSION))
    {
      display_wayland->xdg_output_manager =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zxdg_output_manager_v1_interface,
                          MIN (version, XDG_OUTPUT_VERSION));
      gdk_wayland_display_init_xdg_output (display_wayland);
      _gdk_wayland_display_async_roundtrip (display_wayland);
    }
  else if (match_global (display_wayland, interface, version, zwp_idle_inhibit_manager_v1_interface.name, 0))
    {
      display_wayland->idle_inhibit_manager =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &zwp_idle_inhibit_manager_v1_interface, IDLE_INHIBIT_VERSION);
    }
  else if (match_global (display_wayland, interface, version, xdg_activation_v1_interface.name, 0))
    {
      display_wayland->xdg_activation =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &xdg_activation_v1_interface, ACTIVATION_VERSION);
    }
  else if (match_global (display_wayland, interface, version, wp_fractional_scale_manager_v1_interface.name, 0))
    {
      display_wayland->fractional_scale =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wp_fractional_scale_manager_v1_interface, FRACTIONAL_SCALE_VERSION);
    }
  else if (match_global (display_wayland, interface, version, wp_viewporter_interface.name, 0))
    {
      display_wayland->viewporter =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wp_viewporter_interface, VIEWPORTER_VERSION);
    }
  else if (match_global (display_wayland, interface, version, wp_presentation_interface.name, 0))
    {
      display_wayland->presentation =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wp_presentation_interface,
                          MIN (version, PRESENTATION_VERSION));
    }
  else if (match_global (display_wayland, interface, version, wp_color_manager_v1_interface.name, 0))
    {
      gdk_wayland_color_set_color_manager (display_wayland->color, registry, id, version);
    }
  else if (match_global (display_wayland, interface, version, wp_color_representation_manager_v1_interface.name, 0))
    {
      gdk_wayland_color_set_color_representation (display_wayland->color, registry, id, version);
    }
  else if (match_global (display_wayland, interface, version, wp_single_pixel_buffer_manager_v1_interface.name, 0))
    {
      display_wayland->single_pixel_buffer =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wp_single_pixel_buffer_manager_v1_interface,
                          MIN (version, SINGLE_PIXEL_BUFFER_VERSION));
    }
  else if (match_global (display_wayland, interface, version, xdg_system_bell_v1_interface.name, 0))
    {
      display_wayland->system_bell =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &xdg_system_bell_v1_interface, SYSTEM_BELL_VERSION);
    }
  else if (match_global (display_wayland, interface, version, wp_cursor_shape_manager_v1_interface.name, 0))
    {
      display_wayland->cursor_shape =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &wp_cursor_shape_manager_v1_interface, CURSOR_SHAPE_VERSION);
    }
  else if (match_global (display_wayland, interface, version, xdg_toplevel_icon_manager_v1_interface.name, 0))
    {
      display_wayland->toplevel_icon =
        wl_registry_bind (display_wayland->wl_registry, id,
                          &xdg_toplevel_icon_manager_v1_interface, XDG_TOPLEVEL_ICON_VERSION);
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

  GDK_DEBUG (MISC, "remove global %u", id);

  gdk_wayland_display_remove_seat (display_wayland, id);
  gdk_wayland_display_remove_output (display_wayland, id);

  g_hash_table_remove (display_wayland->known_globals, GUINT_TO_POINTER (id));

  /* FIXME: the object needs to be destroyed here, we're leaking */
}

static const struct wl_registry_listener registry_listener = {
    gdk_registry_handle_global,
    gdk_registry_handle_global_remove
};

/* }}} */
/* {{{ GObject implementation */

GdkDisplay *
_gdk_wayland_display_open (const char *display_name)
{
  struct wl_display *wl_display;
  GdkDisplay *display;
  GdkWaylandDisplay *display_wayland;

  GDK_DEBUG (MISC, "opening display %s", display_name ? display_name : "");

  wl_log_set_handler_client (log_handler);

  wl_display = wl_display_connect (display_name);
  if (!wl_display)
    return NULL;

  display = g_object_new (GDK_TYPE_WAYLAND_DISPLAY, NULL);
  display_wayland = GDK_WAYLAND_DISPLAY (display);
  display_wayland->wl_display = wl_display;
  gdk_wayland_display_install_gsources (display_wayland);

  display_wayland->color = gdk_wayland_color_new (display_wayland);

  init_skip_protocols (display_wayland);

  gdk_wayland_display_init_settings (display);

  display_wayland->known_globals =
    g_hash_table_new_full (NULL, NULL, NULL, g_free);

  display_wayland->wl_registry = wl_display_get_registry (display_wayland->wl_display);
  wl_registry_add_listener (display_wayland->wl_registry, &registry_listener, display_wayland);
  if (wl_display_roundtrip (display_wayland->wl_display) < 0)
    {
      g_object_unref (display);
      return NULL;
    }

  process_on_globals_closures (display_wayland);

  /* Wait for initializing to complete. This means waiting for all
   * asynchronous roundtrips that were triggered during initial roundtrip.
   */
  while (display_wayland->async_roundtrips != NULL)
    {
      if (wl_display_dispatch (display_wayland->wl_display) < 0)
        {
          g_object_unref (display);
          return NULL;
        }
    }

  /* Check that we got all the required globals */
  if (display_wayland->compositor == NULL ||
      display_wayland->shm == NULL ||
      display_wayland->data_device_manager == NULL)
    {
      g_warning ("The Wayland compositor does not provide one or more of the required interfaces, "
                 "not using Wayland display");
      g_object_unref (display);

      return NULL;
    }

  if (display_wayland->xdg_wm_base_id)
    {
      display_wayland->shell_variant = GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL;
      display_wayland->xdg_wm_base =
        wl_registry_bind (display_wayland->wl_registry,
                          display_wayland->xdg_wm_base_id,
                          &xdg_wm_base_interface,
                          MIN (display_wayland->xdg_wm_base_version,
                               XDG_WM_BASE_VERSION));
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

  gdk_wayland_color_prepare (display_wayland->color);

  gdk_display_emit_opened (display);

  return display;
}

static void
destroy_toplevel (gpointer data)
{
  _gdk_surface_destroy (GDK_SURFACE (data), FALSE);
}

static void
gdk_wayland_display_dispose (GObject *object)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (object);

  g_list_free_full (display_wayland->toplevels, destroy_toplevel);

  gdk_wayland_display_uninstall_gsources (display_wayland);

  g_list_free_full (display_wayland->async_roundtrips, (GDestroyNotify) wl_callback_destroy);

  if (display_wayland->known_globals)
    {
      g_hash_table_destroy (display_wayland->known_globals);
      display_wayland->known_globals = NULL;
    }

  g_list_free_full (display_wayland->on_has_globals_closures, g_free);

  g_clear_pointer (&display_wayland->compositor, wl_compositor_destroy);
  g_clear_pointer (&display_wayland->xdg_wm_base, xdg_wm_base_destroy);
  g_clear_pointer (&display_wayland->zxdg_shell_v6, zxdg_shell_v6_destroy);
  g_clear_pointer (&display_wayland->gtk_shell, gtk_shell1_destroy);
  g_clear_pointer (&display_wayland->data_device_manager, wl_data_device_manager_destroy);
  g_clear_pointer (&display_wayland->subcompositor, wl_subcompositor_destroy);
  g_clear_pointer (&display_wayland->pointer_gestures, zwp_pointer_gestures_v1_destroy);
  g_clear_pointer (&display_wayland->primary_selection_manager, zwp_primary_selection_device_manager_v1_destroy);
  g_clear_pointer (&display_wayland->tablet_manager, zwp_tablet_manager_v2_destroy);
  g_clear_pointer (&display_wayland->xdg_exporter, zxdg_exporter_v1_destroy);
  g_clear_pointer (&display_wayland->xdg_exporter_v2, zxdg_exporter_v2_destroy);
  g_clear_pointer (&display_wayland->xdg_importer, zxdg_importer_v1_destroy);
  g_clear_pointer (&display_wayland->xdg_importer_v2, zxdg_importer_v2_destroy);
  g_clear_pointer (&display_wayland->keyboard_shortcuts_inhibit, zwp_keyboard_shortcuts_inhibit_manager_v1_destroy);
  g_clear_pointer (&display_wayland->server_decoration_manager, org_kde_kwin_server_decoration_manager_destroy);
  g_clear_pointer (&display_wayland->xdg_output_manager, zxdg_output_manager_v1_destroy);
  g_clear_pointer (&display_wayland->idle_inhibit_manager, zwp_idle_inhibit_manager_v1_destroy);
  g_clear_pointer (&display_wayland->xdg_activation, xdg_activation_v1_destroy);
  g_clear_pointer (&display_wayland->fractional_scale, wp_fractional_scale_manager_v1_destroy);
  g_clear_pointer (&display_wayland->viewporter, wp_viewporter_destroy);
  g_clear_pointer (&display_wayland->presentation, wp_presentation_destroy);
  g_clear_pointer (&display_wayland->single_pixel_buffer, wp_single_pixel_buffer_manager_v1_destroy);
  g_clear_pointer (&display_wayland->linux_dmabuf, zwp_linux_dmabuf_v1_destroy);
  g_clear_pointer (&display_wayland->dmabuf_formats_info, dmabuf_formats_info_free);
  g_clear_pointer (&display_wayland->color, gdk_wayland_color_free);
  g_clear_pointer (&display_wayland->system_bell, xdg_system_bell_v1_destroy);
  g_clear_pointer (&display_wayland->toplevel_icon, xdg_toplevel_icon_manager_v1_destroy);
  g_clear_pointer (&display_wayland->xx_session, xx_session_v1_destroy);
  g_clear_pointer (&display_wayland->xx_session_manager, xx_session_manager_v1_destroy);

  g_clear_pointer (&display_wayland->shm, wl_shm_destroy);
  g_clear_pointer (&display_wayland->wl_registry, wl_registry_destroy);

  g_list_store_remove_all (display_wayland->monitors);

  G_OBJECT_CLASS (gdk_wayland_display_parent_class)->dispose (object);

  g_clear_pointer (&display_wayland->wl_display, wl_display_disconnect);
}

static void
gdk_wayland_display_finalize (GObject *object)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (object);

  g_object_unref (display_wayland->monitors);

  g_free (display_wayland->startup_notification_id);
  xkb_context_unref (display_wayland->xkb_context);

  g_clear_object (&display_wayland->settings_portal);

  g_strfreev (display_wayland->skip_protocols);

  G_OBJECT_CLASS (gdk_wayland_display_parent_class)->finalize (object);
}

static const char *
gdk_wayland_display_get_name (GdkDisplay *display)
{
  const char *name;

  name = g_getenv ("WAYLAND_DISPLAY");
  if (name == NULL)
    name = "wayland-0";

  return name;
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
  const char *startup_id;

  g_free (display_wayland->startup_notification_id);
  display_wayland->startup_notification_id = NULL;

  startup_id = gdk_get_startup_notification_id ();
  if (startup_id)
    display_wayland->startup_notification_id = g_strdup (startup_id);
}

static gulong
gdk_wayland_display_get_next_serial (GdkDisplay *display)
{
  static gulong serial = 0;
  return ++serial;
}

static void
gdk_wayland_display_notify_startup_complete (GdkDisplay  *display,
                                             const char *startup_id)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  char *free_this = NULL;

  /* Will be signaled with focus activation */
  if (display_wayland->xdg_activation)
    return;

  if (startup_id == NULL)
    {
      startup_id = free_this = display_wayland->startup_notification_id;
      display_wayland->startup_notification_id = NULL;

      if (startup_id == NULL)
        return;
    }

  if (!display_wayland->xdg_activation && display_wayland->gtk_shell)
    gtk_shell1_set_startup_id (display_wayland->gtk_shell, startup_id);

  g_free (free_this);
}

static GdkKeymap *
_gdk_wayland_display_get_keymap (GdkDisplay *display)
{
  GdkSeat *seat;
  GdkDevice *core_keyboard = NULL;
  static GdkKeymap *tmp_keymap = NULL;

  seat = gdk_display_get_default_seat (display);
  if (seat)
    core_keyboard = gdk_seat_get_keyboard (seat);

  if (core_keyboard && tmp_keymap)
    {
      g_object_unref (tmp_keymap);
      tmp_keymap = NULL;
    }

  if (core_keyboard)
    return _gdk_wayland_device_get_keymap (core_keyboard);

  if (!tmp_keymap)
    tmp_keymap = _gdk_wayland_keymap_new (display);

  return tmp_keymap;
}

static GListModel *
gdk_wayland_display_get_monitors (GdkDisplay *display)
{
  GdkWaylandDisplay *self = GDK_WAYLAND_DISPLAY (display);

  return G_LIST_MODEL (self->monitors);
}

static GdkMonitor *
gdk_wayland_display_get_monitor_at_surface (GdkDisplay *display,
                                            GdkSurface *window)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  struct wl_output *output;
  uint32_t i, n;

  g_return_val_if_fail (GDK_IS_WAYLAND_SURFACE (window), NULL);

  output = gdk_wayland_surface_get_wl_output (window);
  if (output == NULL)
    return NULL;

  n = g_list_model_get_n_items (G_LIST_MODEL (display_wayland->monitors));
  for (i = 0; i < n; i++)
    {
      GdkMonitor *monitor = g_list_model_get_item (G_LIST_MODEL (display_wayland->monitors), i);

      g_object_unref (monitor);

      if (gdk_wayland_monitor_get_wl_output (monitor) == output)
        return monitor;
    }

  return NULL;
}

static void
_gdk_wayland_display_set_cursor_theme (GdkDisplay *display,
                                       const char *name,
                                       int         size)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY(display);

  display_wayland->cursor_theme_size = size ? size : 24;
}

static void
gdk_wayland_display_class_init (GdkWaylandDisplayClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);
  GdkDisplayClass *display_class = GDK_DISPLAY_CLASS (class);

  object_class->dispose = gdk_wayland_display_dispose;
  object_class->finalize = gdk_wayland_display_finalize;

  display_class->toplevel_type = GDK_TYPE_WAYLAND_TOPLEVEL;
  display_class->popup_type = GDK_TYPE_WAYLAND_POPUP;
  display_class->cairo_context_type = GDK_TYPE_WAYLAND_CAIRO_CONTEXT;

#ifdef GDK_RENDERING_VULKAN
  display_class->vk_context_type = GDK_TYPE_WAYLAND_VULKAN_CONTEXT;
  display_class->vk_extension_name = VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME;
#endif

  display_class->get_name = gdk_wayland_display_get_name;
  display_class->beep = gdk_wayland_display_beep;
  display_class->sync = gdk_wayland_display_sync;
  display_class->flush = gdk_wayland_display_flush;
  display_class->make_default = gdk_wayland_display_make_default;
  display_class->queue_events = _gdk_wayland_display_queue_events;
  display_class->get_app_launch_context = gdk_wayland_display_get_app_launch_context;
  display_class->get_next_serial = gdk_wayland_display_get_next_serial;
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  display_class->get_startup_notification_id = gdk_wayland_display_get_startup_notification_id;
G_GNUC_END_IGNORE_DEPRECATIONS
  display_class->notify_startup_complete = gdk_wayland_display_notify_startup_complete;
  display_class->get_keymap = _gdk_wayland_display_get_keymap;

  display_class->init_gl = gdk_wayland_display_init_gl;

  display_class->get_monitors = gdk_wayland_display_get_monitors;
  display_class->get_monitor_at_surface = gdk_wayland_display_get_monitor_at_surface;
  display_class->get_setting = gdk_wayland_display_get_setting;
  display_class->set_cursor_theme = _gdk_wayland_display_set_cursor_theme;
}

static void
gdk_wayland_display_init (GdkWaylandDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  display_wayland->cursor_theme_size = 24;
  display->xkb_context = xkb_context_new (0);
  display->monitors = g_list_store_new (GDK_TYPE_MONITOR);
}

/* }}} */
/* {{{ Private API */

void
gdk_wayland_display_system_bell (GdkDisplay *display,
                                 GdkSurface *surface)
{
  GdkWaylandDisplay *display_wayland;
  struct gtk_surface1 *gtk_surface = NULL;
  struct wl_surface *wl_surface = NULL;
  gint64 now_ms;

  g_return_if_fail (GDK_IS_DISPLAY (display));

  display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (!display_wayland->gtk_shell &&
      !display_wayland->system_bell)
    return;

  if (surface)
    {
      if (GDK_IS_WAYLAND_TOPLEVEL (surface))
        {
          GdkWaylandToplevel *toplevel = GDK_WAYLAND_TOPLEVEL (surface);

          gtk_surface = gdk_wayland_toplevel_get_gtk_surface (toplevel);
        }

      wl_surface = gdk_wayland_surface_get_wl_surface (surface);
    }

  now_ms = g_get_monotonic_time () / 1000;
  if (now_ms - display_wayland->last_bell_time_ms < MIN_SYSTEM_BELL_DELAY_MS)
    return;

  display_wayland->last_bell_time_ms = now_ms;

  if (display_wayland->system_bell)
    xdg_system_bell_v1_ring (display_wayland->system_bell, wl_surface);
  else
    gtk_shell1_system_bell (display_wayland->gtk_shell, gtk_surface);
}

void
gdk_wayland_display_dispatch_queue (GdkDisplay            *display,
                                    struct wl_event_queue *event_queue)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (wl_display_dispatch_queue (display_wayland->wl_display, event_queue) == -1)
    {
      g_message ("Error %d (%s) dispatching to Wayland display.",
                 errno, g_strerror (errno));
      _exit (1);
    }
}

/* }}} */
/* {{{ Public API */

/*
 * gdk_wayland_display_prefers_ssd:
 * @display: (type GdkWaylandDisplay): a display
 *
 * Checks whether the Wayland compositor prefers to draw the window
 * decorations or if it leaves decorations to the application.
 *
 * Returns: true if the compositor prefers server-side decorations
 */
gboolean
gdk_wayland_display_prefers_ssd (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (display_wayland->server_decoration_manager)
    return display_wayland->server_decoration_mode == ORG_KDE_KWIN_SERVER_DECORATION_MANAGER_MODE_SERVER;

  return FALSE;
}

/**
 * gdk_wayland_display_get_startup_notification_id:
 * @display: (type GdkWaylandDisplay): a display
 *
 * Gets the startup notification ID for a Wayland display, or `NULL`
 * if no ID has been defined.
 *
 * Returns: (nullable): the startup notification ID for @display
 *
 * Deprecated: 4.10.
 */
const char *
gdk_wayland_display_get_startup_notification_id (GdkDisplay  *display)
{
  return GDK_WAYLAND_DISPLAY (display)->startup_notification_id;
}

/**
 * gdk_wayland_display_set_startup_notification_id:
 * @display: (type GdkWaylandDisplay): a display
 * @startup_id: the startup notification ID (must be valid utf8)
 *
 * Sets the startup notification ID for a display.
 *
 * This is usually taken from the value of the `DESKTOP_STARTUP_ID`
 * environment variable, but in some cases (such as the application not
 * being launched using exec()) it can come from other sources.
 *
 * The startup ID is also what is used to signal that the startup is
 * complete (for example, when opening a window or when calling
 * [method@Gdk.Display.notify_startup_complete]).
 *
 * Deprecated: 4.10. Use [method@Gdk.Toplevel.set_startup_id]
 */
void
gdk_wayland_display_set_startup_notification_id (GdkDisplay *display,
                                                 const char *startup_id)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  g_free (display_wayland->startup_notification_id);
  display_wayland->startup_notification_id = g_strdup (startup_id);
}

/**
 * gdk_wayland_display_set_cursor_theme:
 * @display: (type GdkWaylandDisplay): a display
 * @name: the new cursor theme
 * @size: the size to use for cursors
 *
 * Sets the cursor theme for the given @display.
 *
 * Deprecated: 4.16: Use the cursor-related properties of
 *   [GtkSettings](../gtk4/class.Settings.html) to set the cursor theme
 */
void
gdk_wayland_display_set_cursor_theme (GdkDisplay *display,
                                      const char *name,
                                      int         size)
{
  _gdk_wayland_display_set_cursor_theme (display, name, size);
}

/**
 * gdk_wayland_display_get_wl_display: (skip)
 * @display: (type GdkWaylandDisplay): a display
 *
 * Returns the Wayland `wl_display` of a display.
 *
 * Returns: (transfer none): a Wayland `wl_display`
 */
struct wl_display *
gdk_wayland_display_get_wl_display (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), NULL);

  return GDK_WAYLAND_DISPLAY (display)->wl_display;
}

/**
 * gdk_wayland_display_get_wl_compositor: (skip)
 * @display: (type GdkWaylandDisplay): a display
 *
 * Returns the Wayland `wl_compositor` of a display.
 *
 * Returns: (transfer none): a Wayland `wl_compositor`
 */
struct wl_compositor *
gdk_wayland_display_get_wl_compositor (GdkDisplay *display)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_DISPLAY (display), NULL);

  return GDK_WAYLAND_DISPLAY (display)->compositor;
}

/**
 * gdk_wayland_display_query_registry:
 * @display: (type GdkWaylandDisplay): a display
 * @global: global interface to query in the registry
 *
 * Returns true if the interface was found in the display
 * `wl_registry.global` handler.
 *
 * Returns: true if the global is offered by the compositor
 */
gboolean
gdk_wayland_display_query_registry (GdkDisplay *display,
                                    const char *global)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);
  GHashTableIter iter;
  char *value;

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

void
gdk_wayland_display_register_session (GdkDisplay                        *display,
                                      enum xx_session_manager_v1_reason  reason,
                                      const char                        *name)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  GDK_DEBUG (MISC, "register session %s, reason %u", name, reason);

  if (!display_wayland->xx_session_manager)
    return;

  g_clear_pointer (&display_wayland->session_id, g_free);
  display_wayland->session_id = g_strdup (name);

  display_wayland->xx_session =
    xx_session_manager_v1_get_session (display_wayland->xx_session_manager,
                                       reason,
                                       name);
  xx_session_v1_add_listener (display_wayland->xx_session,
                              &xx_session_listener,
                              display_wayland);

  wl_display_roundtrip (display_wayland->wl_display);
}

void
gdk_wayland_display_unregister_session (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  if (!display_wayland->xx_session_manager)
    return;

  if (display_wayland->xx_session)
    xx_session_v1_remove (display_wayland->xx_session);

  g_clear_pointer (&display_wayland->session_id, g_free);
  g_clear_pointer (&display_wayland->xx_session, xx_session_v1_destroy);
}

const char *
gdk_wayland_display_get_session_id (GdkDisplay *display)
{
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  return display_wayland->session_id;
}

/* }}} */
/* vim:set foldmethod=marker: */
