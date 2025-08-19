/*
 * Copyright © 2016 Red Hat, Inc
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

#include <glib.h>
#include <gio/gio.h>
#include "gdkdisplay-wayland.h"
#include "gdkdihedralprivate.h"
#include "gdkmonitor-wayland.h"

/* {{{ GObject implementation */

/**
 * GdkWaylandMonitor:
 *
 * The Wayland implementation of `GdkMonitor`.
 *
 * Beyond the [class@Gdk.Monitor] API, the Wayland implementation
 * offers access to the Wayland `wl_output` object with
 * [method@GdkWayland.WaylandMonitor.get_wl_output].
 */

G_DEFINE_TYPE (GdkWaylandMonitor, gdk_wayland_monitor, GDK_TYPE_MONITOR)

static void
gdk_wayland_monitor_init (GdkWaylandMonitor *monitor)
{
}

static void
gdk_wayland_monitor_finalize (GObject *object)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)object;

  g_clear_pointer (&monitor->xdg_output, zxdg_output_v1_destroy);

  if (wl_output_get_version (monitor->output) >= WL_OUTPUT_RELEASE_SINCE_VERSION)
    g_clear_pointer (&monitor->output, wl_output_release);
  else
    g_clear_pointer (&monitor->output, wl_output_destroy);

  G_OBJECT_CLASS (gdk_wayland_monitor_parent_class)->finalize (object);
}

static void
gdk_wayland_monitor_class_init (GdkWaylandMonitorClass *class)
{
  G_OBJECT_CLASS (class)->finalize = gdk_wayland_monitor_finalize;
}

/* }}} */
/* {{{ xdg_output listener */

static gboolean
display_has_xdg_output_support (GdkWaylandDisplay *display_wayland)
{
  return (display_wayland->xdg_output_manager != NULL);
}

static gboolean
monitor_has_xdg_output (GdkWaylandMonitor *monitor)
{
  return (monitor->xdg_output != NULL);
}

static void
xdg_output_handle_logical_position (void                  *data,
                                    struct zxdg_output_v1 *xdg_output,
                                    int32_t                x,
                                    int32_t                y)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_DEBUG (MISC, "handle logical position xdg-output %d, position %d %d",
                   monitor->id, x, y);

  monitor->logical_geometry.x = x;
  monitor->logical_geometry.y = y;
}

static void
xdg_output_handle_logical_size (void                  *data,
                                struct zxdg_output_v1 *xdg_output,
                                int32_t                width,
                                int32_t                height)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_DEBUG (MISC, "handle logical size xdg-output %d, size %d %d",
                   monitor->id, width, height);

  monitor->logical_geometry.width = width;
  monitor->logical_geometry.height = height;
}

static void
xdg_output_handle_done (void                  *data,
                        struct zxdg_output_v1 *xdg_output)
{
}

static void
xdg_output_handle_name (void                  *data,
                        struct zxdg_output_v1 *xdg_output,
                        const char            *name)
{
}

static void
xdg_output_handle_description (void                  *data,
                               struct zxdg_output_v1 *xdg_output,
                               const char            *description)
{
}

static const struct zxdg_output_v1_listener xdg_output_listener = {
    xdg_output_handle_logical_position,
    xdg_output_handle_logical_size,
    xdg_output_handle_done,
    xdg_output_handle_name,
    xdg_output_handle_description,
};

/* }}} */
/* {{{ wl_output listener */

static const char *
subpixel_to_string (int layout)
{
  int i;
  struct { int layout; const char *name; } layouts[] = {
    { WL_OUTPUT_SUBPIXEL_UNKNOWN, "unknown" },
    { WL_OUTPUT_SUBPIXEL_NONE, "none" },
    { WL_OUTPUT_SUBPIXEL_HORIZONTAL_RGB, "rgb" },
    { WL_OUTPUT_SUBPIXEL_HORIZONTAL_BGR, "bgr" },
    { WL_OUTPUT_SUBPIXEL_VERTICAL_RGB, "vrgb" },
    { WL_OUTPUT_SUBPIXEL_VERTICAL_BGR, "vbgr" },
    { 0xffffffff, NULL }
  };

  for (i = 0; layouts[i].name; i++)
    {
      if (layouts[i].layout == layout)
        return layouts[i].name;
    }
  return NULL;
}

static void
output_handle_geometry (void             *data,
                        struct wl_output *wl_output,
                        int               x,
                        int               y,
                        int               physical_width,
                        int               physical_height,
                        int               subpixel,
                        const char       *make,
                        const char       *model,
                        int32_t           transform)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_DEBUG (MISC, "handle geometry output %d, position %d %d, phys. size %d x %d mm, subpixel layout %s, manufacturer %s, model %s, transform %s",
                   monitor->id, x, y,
                   physical_width, physical_height,
                   subpixel_to_string (subpixel),
                   make, model,
                   gdk_dihedral_get_name ((GdkDihedral) transform));

  monitor->output_geometry.x = x;
  monitor->output_geometry.y = y;

  switch (transform)
    {
    case WL_OUTPUT_TRANSFORM_90:
    case WL_OUTPUT_TRANSFORM_270:
    case WL_OUTPUT_TRANSFORM_FLIPPED_90:
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      gdk_monitor_set_physical_size (GDK_MONITOR (monitor),
                                     physical_height, physical_width);
      break;
    default:
      gdk_monitor_set_physical_size (GDK_MONITOR (monitor),
                                     physical_width, physical_height);
    }

  gdk_monitor_set_subpixel_layout (GDK_MONITOR (monitor), subpixel);
  gdk_monitor_set_manufacturer (GDK_MONITOR (monitor), make);
  gdk_monitor_set_model (GDK_MONITOR (monitor), model);
}

static void
apply_monitor_change (GdkWaylandMonitor *monitor)
{
  double scale;

  GDK_DEBUG (MISC, "monitor %d changed position %d %d, size %d %d",
                   monitor->id,
                   monitor->output_geometry.x, monitor->output_geometry.y,
                   monitor->output_geometry.width, monitor->output_geometry.height);

  if (monitor_has_xdg_output (monitor))
    {
      scale = MAX (monitor->output_geometry.width / (double) monitor->logical_geometry.width,
                   monitor->output_geometry.height / (double) monitor->logical_geometry.height);
    }
  else
    {
      scale = gdk_monitor_get_scale_factor (GDK_MONITOR (monitor));

      monitor->logical_geometry.x = monitor->output_geometry.x / scale;
      monitor->logical_geometry.y = monitor->output_geometry.y / scale;
      monitor->logical_geometry.width = monitor->output_geometry.width / scale;
      monitor->logical_geometry.height = monitor->output_geometry.height / scale;
    }

  gdk_monitor_set_geometry (GDK_MONITOR (monitor), &monitor->logical_geometry);
  gdk_monitor_set_scale (GDK_MONITOR (monitor), scale);
}

static void
output_handle_done (void             *data,
                    struct wl_output *wl_output)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_DEBUG (MISC, "handle done output %d", monitor->id);

  apply_monitor_change (monitor);
}

static void
output_handle_scale (void             *data,
                     struct wl_output *wl_output,
                     int32_t           scale)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_DEBUG (MISC, "handle scale output %d, scale %d", monitor->id, scale);

  gdk_monitor_set_scale_factor (GDK_MONITOR (monitor), scale);
}

static void
output_handle_mode (void             *data,
                    struct wl_output *wl_output,
                    uint32_t          flags,
                    int               width,
                    int               height,
                    int               refresh)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *)data;

  GDK_DEBUG (MISC, "handle mode output %d, size %d %d, rate %d",
                   monitor->id, width, height, refresh);

  if ((flags & WL_OUTPUT_MODE_CURRENT) == 0)
    return;

  monitor->output_geometry.width = width;
  monitor->output_geometry.height = height;

  gdk_monitor_set_refresh_rate (GDK_MONITOR (monitor), refresh);
}

static void
output_handle_name (void *data,
                    struct wl_output *wl_output,
                    const char *name)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_DEBUG (MISC, "handle name output %d: %s", monitor->id, name);

  gdk_monitor_set_connector (GDK_MONITOR (monitor), name);
}

static void
output_handle_description (void *data,
                           struct wl_output *wl_output,
                           const char *description)
{
  GdkWaylandMonitor *monitor = (GdkWaylandMonitor *) data;

  GDK_DEBUG (MISC, "handle description output %d: %s", monitor->id, description);

  gdk_monitor_set_description (GDK_MONITOR (monitor), description);
}

static const struct wl_output_listener output_listener =
{
  output_handle_geometry,
  output_handle_mode,
  output_handle_done,
  output_handle_scale,
  output_handle_name,
  output_handle_description,
};

/* }}} */
/* {{{ Private API */

void
gdk_wayland_display_init_xdg_output (GdkWaylandDisplay *self)
{
  uint32_t i, n;

  GDK_DEBUG (MISC, "init xdg-output support, %d monitor(s) already present",
                   g_list_model_get_n_items (G_LIST_MODEL (self->monitors)));

  n = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));
  for (i = 0; i < n; i++)
    {
      GdkWaylandMonitor *monitor = g_list_model_get_item (G_LIST_MODEL (self->monitors), i);
      gdk_wayland_monitor_get_xdg_output (monitor);
      g_object_unref (monitor);
    }
}

void
gdk_wayland_monitor_get_xdg_output (GdkWaylandMonitor *monitor)
{
  GdkDisplay *display = GDK_MONITOR (monitor)->display;
  GdkWaylandDisplay *display_wayland = GDK_WAYLAND_DISPLAY (display);

  GDK_DEBUG (MISC, "get xdg-output for monitor %d", monitor->id);

  monitor->xdg_output =
    zxdg_output_manager_v1_get_xdg_output (display_wayland->xdg_output_manager,
                                           monitor->output);

  zxdg_output_v1_add_listener (monitor->xdg_output,
                               &xdg_output_listener,
                               monitor);
}

void
gdk_wayland_display_add_output (GdkWaylandDisplay *display_wayland,
                                uint32_t           id,
                                struct wl_output  *output)
{
  GdkWaylandMonitor *monitor;
  uint32_t version;

  version = wl_proxy_get_version ((struct wl_proxy *) output);

  monitor = g_object_new (GDK_TYPE_WAYLAND_MONITOR,
                          "display", GDK_DISPLAY (display_wayland),
                          NULL);

  monitor->id = id;
  monitor->output = output;

  wl_output_add_listener (output, &output_listener, monitor);

  GDK_DEBUG (MISC, "add output %u, version %u", id, version);

  if (display_has_xdg_output_support (display_wayland))
    gdk_wayland_monitor_get_xdg_output (monitor);

  g_list_store_append (display_wayland->monitors, monitor);

  g_object_unref (monitor);
}

void
gdk_wayland_display_remove_output (GdkWaylandDisplay *self,
                                   uint32_t           id)
{
  uint32_t i, n;

  GDK_DEBUG (MISC, "remove output %u", id);

  n = g_list_model_get_n_items (G_LIST_MODEL (self->monitors));
  for (i = 0; i < n; i++)
    {
      GdkWaylandMonitor *monitor = g_list_model_get_item (G_LIST_MODEL (self->monitors), i);

      if (monitor->id == id)
        {
          g_list_store_remove (self->monitors, i);
          gdk_monitor_invalidate (GDK_MONITOR (monitor));
          g_object_unref (monitor);
          break;
        }

      g_object_unref (monitor);
    }
}

GdkMonitor *
gdk_wayland_display_get_monitor (GdkWaylandDisplay *display,
                                 struct wl_output  *output)
{
  uint32_t i, n;

  n = g_list_model_get_n_items (G_LIST_MODEL (display->monitors));
  for (i = 0; i < n; i++)
    {
      GdkWaylandMonitor *monitor = g_list_model_get_item (G_LIST_MODEL (display->monitors), i);

      g_object_unref (monitor);

      if (monitor->output == output)
        return (GdkMonitor *) monitor;
    }

  return NULL;
}

/* }}} */
/* {{{ Public API */

/**
 * gdk_wayland_monitor_get_wl_output: (skip)
 * @monitor: (type GdkWaylandMonitor): a `GdkMonitor`
 *
 * Returns the Wayland `wl_output` of a `GdkMonitor`.
 *
 * Returns: (transfer none): a Wayland `wl_output`
 */
struct wl_output *
gdk_wayland_monitor_get_wl_output (GdkMonitor *monitor)
{
  g_return_val_if_fail (GDK_IS_WAYLAND_MONITOR (monitor), NULL);

  return GDK_WAYLAND_MONITOR (monitor)->output;
}

/* }}} */
/* vim:set foldmethod=marker: */
