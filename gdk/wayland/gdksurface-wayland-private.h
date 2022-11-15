/*
 * Copyright © 2022 Red Hat, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

typedef enum _PopupState
{
  POPUP_STATE_IDLE,
  POPUP_STATE_WAITING_FOR_REPOSITIONED,
  POPUP_STATE_WAITING_FOR_CONFIGURE,
  POPUP_STATE_WAITING_FOR_FRAME,
} PopupState;

struct _GdkWaylandSurface
{
  GdkSurface parent_instance;

  struct {
    GSList *outputs;
    struct wl_surface *wl_surface;
    struct xdg_surface *xdg_surface;
    struct xdg_toplevel *xdg_toplevel;
    struct xdg_popup *xdg_popup;

    struct zxdg_surface_v6 *zxdg_surface_v6;
    struct zxdg_toplevel_v6 *zxdg_toplevel_v6;
    struct zxdg_popup_v6 *zxdg_popup_v6;

    struct wl_egl_window *egl_window;
  } display_server;

  struct wl_event_queue *event_queue;

  unsigned int initial_configure_received : 1;
  unsigned int has_uncommitted_ack_configure : 1;
  unsigned int mapped : 1;
  unsigned int awaiting_frame : 1;
  unsigned int awaiting_frame_frozen : 1;

  int pending_buffer_offset_x;
  int pending_buffer_offset_y;

  gint64 pending_frame_counter;
  guint32 scale;

  int shadow_left;
  int shadow_right;
  int shadow_top;
  int shadow_bottom;

  cairo_region_t *opaque_region;
  gboolean opaque_region_dirty;

  cairo_region_t *input_region;
  gboolean input_region_dirty;

  GdkRectangle last_sent_window_geometry;

  int saved_width;
  int saved_height;

  struct {
    gboolean is_initial_configure;
    uint32_t serial;
    gboolean is_dirty;
  } pending;

  struct {
    int configured_width;
    int configured_height;
    gboolean surface_geometry_dirty;
  } next_layout;

  uint32_t last_configure_serial;

  int state_freeze_count;
};

typedef struct _GdkWaylandSurfaceClass GdkWaylandSurfaceClass;
struct _GdkWaylandSurfaceClass
{
  GdkSurfaceClass parent_class;
};

void gdk_wayland_surface_create_wl_surface (GdkSurface *surface);
void gdk_wayland_surface_update_size       (GdkSurface *surface,
                                            int32_t     width,
                                            int32_t     height,
                                            int         scale);

#define GDK_TYPE_WAYLAND_DRAG_SURFACE (gdk_wayland_drag_surface_get_type ())
GType gdk_wayland_drag_surface_get_type (void) G_GNUC_CONST;
