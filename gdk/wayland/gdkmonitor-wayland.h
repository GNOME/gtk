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

#pragma once

#include <glib.h>
#include "gdkwaylandmonitor.h"
#include "gdkmonitorprivate.h"

struct _GdkWaylandMonitor {
  GdkMonitor parent;

  uint32_t id;
  struct wl_output *output;
  gboolean added;

  struct zxdg_output_v1 *xdg_output;

  /* Raw wl_output and xdg_output data */
  GdkRectangle output_geometry;
  GdkRectangle logical_geometry;
};

struct _GdkWaylandMonitorClass {
  GdkMonitorClass parent_class;
};

void gdk_wayland_display_init_xdg_output (GdkWaylandDisplay *display_wayland);
void gdk_wayland_monitor_get_xdg_output  (GdkWaylandMonitor *monitor);

GdkMonitor *gdk_wayland_display_get_monitor (GdkWaylandDisplay *display,
                                             struct wl_output  *output);

void gdk_wayland_display_add_output      (GdkWaylandDisplay *display_wayland,
                                          uint32_t           id,
                                          struct wl_output  *output);
void gdk_wayland_display_remove_output   (GdkWaylandDisplay *self,
                                          uint32_t           id);
