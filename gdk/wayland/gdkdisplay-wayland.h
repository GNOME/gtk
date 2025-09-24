/*
 * gdkdisplay-wayland.h
 *
 * Copyright 2001 Sun Microsystems Inc.
 *
 * Erwann Chenede <erwann.chenede@sun.com>
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

#pragma once

#include "config.h"

#include <stdint.h>
#include <wayland-client.h>
#include <wayland-egl.h>
#include <gdk/wayland/tablet-v2-client-protocol.h>
#include <gdk/wayland/gtk-shell-client-protocol.h>
#include <gdk/wayland/xdg-shell-client-protocol.h>
#include <gdk/wayland/xdg-shell-unstable-v6-client-protocol.h>
#include <gdk/wayland/xdg-foreign-unstable-v1-client-protocol.h>
#include <gdk/wayland/keyboard-shortcuts-inhibit-unstable-v1-client-protocol.h>
#include <gdk/wayland/server-decoration-client-protocol.h>
#include <gdk/wayland/xdg-output-unstable-v1-client-protocol.h>
#include <gdk/wayland/idle-inhibit-unstable-v1-client-protocol.h>
#include <gdk/wayland/primary-selection-unstable-v1-client-protocol.h>
#include <gdk/wayland/xdg-activation-v1-client-protocol.h>
#include <gdk/wayland/fractional-scale-v1-client-protocol.h>
#include <gdk/wayland/viewporter-client-protocol.h>
#include <gdk/wayland/presentation-time-client-protocol.h>
#include <gdk/wayland/single-pixel-buffer-v1-client-protocol.h>
#include <gdk/wayland/xdg-dialog-v1-client-protocol.h>
#include <gdk/wayland/xdg-system-bell-v1-client-protocol.h>
#include <gdk/wayland/cursor-shape-v1-client-protocol.h>
#include <gdk/wayland/xdg-toplevel-icon-v1-client-protocol.h>
#include <gdk/wayland/xx-session-management-v1-client-protocol.h>

#include <glib.h>
#include <gdk/gdkkeys.h>
#include <gdk/gdksurface.h>
#include <gdk/gdk.h>

#include "gdkdisplayprivate.h"
#include "gdkwaylanddevice.h"
#include "gdkwaylanddisplay.h"
#include "gdkdmabuf-wayland-private.h"

#include <epoxy/egl.h>

G_BEGIN_DECLS

typedef struct _GdkWaylandColor GdkWaylandColor;
typedef struct _GdkWaylandSelection GdkWaylandSelection;

typedef struct {
        gboolean     antialias;
        gboolean     hinting;
        int          dpi;
        const char *rgba;
        const char *hintstyle;
} GsdXftSettings;

typedef enum _GdkWaylandShellVariant
{
  GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL,
  GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6
} GdkWaylandShellVariant;

struct _GdkWaylandDisplay
{
  GdkDisplay parent_instance;
  GList *toplevels;

  GsdXftSettings xft_settings;
  GDBusProxy *settings_portal;

  gboolean list_protocols;
  char **skip_protocols;

  uint32_t shell_capabilities;

  /* Startup notification */
  char *startup_notification_id;

  uint32_t xdg_wm_base_id;
  int xdg_wm_base_version;
  uint32_t zxdg_shell_v6_id;
  GdkWaylandShellVariant shell_variant;

  /* Wayland fields below */
  struct wl_display *wl_display;
  struct wl_registry *wl_registry;
  struct wl_compositor *compositor;
  struct wl_shm *shm;
  struct zwp_linux_dmabuf_v1 *linux_dmabuf;
  DmabufFormatsInfo *dmabuf_formats_info;
  struct xdg_wm_base *xdg_wm_base;
  struct zxdg_shell_v6 *zxdg_shell_v6;
  struct xdg_wm_dialog_v1 *xdg_wm_dialog;
  struct gtk_shell1 *gtk_shell;
  struct xdg_system_bell_v1 *system_bell;
  struct wl_data_device_manager *data_device_manager;
  struct wl_subcompositor *subcompositor;
  struct zwp_pointer_gestures_v1 *pointer_gestures;
  struct zwp_primary_selection_device_manager_v1 *primary_selection_manager;
  struct zwp_tablet_manager_v2 *tablet_manager;
  struct zxdg_exporter_v1 *xdg_exporter;
  struct zxdg_exporter_v2 *xdg_exporter_v2;
  struct zxdg_importer_v1 *xdg_importer;
  struct zxdg_importer_v2 *xdg_importer_v2;
  struct zwp_keyboard_shortcuts_inhibit_manager_v1 *keyboard_shortcuts_inhibit;
  struct org_kde_kwin_server_decoration_manager *server_decoration_manager;
  struct zxdg_output_manager_v1 *xdg_output_manager;
  struct zwp_idle_inhibit_manager_v1 *idle_inhibit_manager;
  struct xdg_activation_v1 *xdg_activation;
  struct wp_fractional_scale_manager_v1 *fractional_scale;
  struct wp_viewporter *viewporter;
  struct wp_presentation *presentation;
  struct wp_single_pixel_buffer_manager_v1 *single_pixel_buffer;
  struct wp_cursor_shape_manager_v1 *cursor_shape;
  struct xdg_toplevel_icon_manager_v1 *toplevel_icon;
  struct xx_session_manager_v1 *xx_session_manager;
  struct xx_session_v1 *xx_session;

  GdkWaylandColor *color;

  GList *async_roundtrips;

  /* Keep track of the ID's of the known globals and their corresponding
   * names. This way we can check whether an interface is known, and
   * remove globals given its ID. This table is not expected to be very
   * large, meaning the lookup by interface name time is insignificant. */
  GHashTable *known_globals;
  GList *on_has_globals_closures;

  GList *event_queues;

  GList *current_popups;
  GList *current_grabbing_popups;

  int cursor_theme_size;

  GSource *event_source;
  GSource *poll_source;

  uint32_t server_decoration_mode;

  struct xkb_context *xkb_context;

  GListStore *monitors;

  char *session_id;

  gint64 last_bell_time_ms;
};

struct _GdkWaylandDisplayClass
{
  GdkDisplayClass parent_class;
};

gboolean   gdk_wayland_display_prefers_ssd    (GdkDisplay            *display);

void       gdk_wayland_display_system_bell    (GdkDisplay            *display,
                                               GdkSurface            *surface);

void       gdk_wayland_display_dispatch_queue (GdkDisplay            *display,
                                               struct wl_event_queue *event_queue);

GdkDisplay *_gdk_wayland_display_open (const char *display_name);

void                    gdk_wayland_display_register_session    (GdkDisplay *display,
                                                                 enum xx_session_manager_v1_reason reason,
                                                                 const char *name);

void                    gdk_wayland_display_unregister_session  (GdkDisplay *display);

const char *            gdk_wayland_display_get_session_id      (GdkDisplay *display);


G_END_DECLS

