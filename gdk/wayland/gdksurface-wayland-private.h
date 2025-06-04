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

#include "gdkwaylandsurface.h"
#include "gdkfractionalscale-private.h"
#include "gdkwaylandcolor-private.h"
#include "gdkwaylandpresentationtime-private.h"
#include "gdkseat-wayland.h"


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
    struct zxdg_surface_v6 *zxdg_surface_v6;
    struct wp_fractional_scale_v1 *fractional_scale;
    struct wp_viewport *viewport;
    GdkWaylandColorSurface *color;
  } display_server;

  struct wl_event_queue *event_queue;
  struct wl_callback *frame_callback;

  GdkWaylandPresentationTime *presentation_time;

  unsigned int color_state_changed : 1;
  unsigned int initial_configure_received : 1;
  unsigned int has_uncommitted_ack_configure : 1;
  unsigned int has_pending_subsurface_commits : 1;
  unsigned int mapped : 1;
  unsigned int awaiting_frame_frozen : 1;

  int pending_buffer_offset_x;
  int pending_buffer_offset_y;

  gint64 pending_frame_counter;
  GdkFractionalScale scale;
  gboolean buffer_is_fractional;
  gboolean buffer_scale_dirty;
  gboolean viewport_dirty;

  int shadow_left;
  int shadow_right;
  int shadow_top;
  int shadow_bottom;

  cairo_region_t *opaque_region;
  gboolean opaque_region_dirty;

  cairo_region_t *input_region;
  gboolean input_region_dirty;

  GdkRectangle last_sent_window_geometry;

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

  void (* handle_configure) (GdkWaylandSurface *surface);

  void (* handle_frame) (GdkWaylandSurface *surface);

  void (* hide_surface) (GdkWaylandSurface *surface);
};

#define GDK_WAYLAND_SURFACE_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST ((klass), GDK_TYPE_WAYLAND_SURFACE, GdkWaylandSurfaceClass))

#define GDK_WAYLAND_SURFACE_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GDK_TYPE_WAYLAND_SURFACE, GdkWaylandSurfaceClass))

void gdk_wayland_surface_update_size       (GdkSurface               *surface,
                                            int32_t                   width,
                                            int32_t                   height,
                                            const GdkFractionalScale *scale);
void gdk_wayland_surface_create_xdg_surface_resources (GdkSurface *surface);
void _gdk_wayland_surface_save_size (GdkSurface *surface);

void gdk_wayland_surface_hide_surface (GdkSurface *surface);
void gdk_wayland_surface_move_resize (GdkSurface *surface,
                                      int         x,
                                      int         y,
                                      int         width,
                                      int         height);
void gdk_wayland_surface_get_window_geometry (GdkSurface   *surface,
                                              GdkRectangle *geometry);
void gdk_wayland_surface_freeze_state (GdkSurface *surface);
void gdk_wayland_surface_thaw_state   (GdkSurface *surface);

void gdk_wayland_surface_frame_callback (GdkSurface *surface,
                                         uint32_t    time);

void            gdk_wayland_surface_sync                   (GdkSurface           *surface);
void            gdk_wayland_surface_handle_empty_frame     (GdkSurface           *surface);
void            gdk_wayland_surface_commit                 (GdkSurface           *surface);
void            gdk_wayland_surface_notify_committed       (GdkSurface           *surface);
void            gdk_wayland_surface_request_frame          (GdkSurface           *surface);
gboolean        gdk_wayland_surface_has_surface            (GdkSurface           *surface);
void            gdk_wayland_surface_attach_image           (GdkSurface           *surface,
                                                            cairo_surface_t      *cairo_surface,
                                                            const cairo_region_t *damage);

GdkDrag        *_gdk_wayland_surface_drag_begin            (GdkSurface *surface,
                                                            GdkDevice *device,
                                                            GdkContentProvider *content,
                                                            GdkDragAction actions,
                                                            double     dx,
                                                            double     dy);
void            _gdk_wayland_surface_offset_next_wl_buffer (GdkSurface *surface,
                                                            int        x,
                                                            int        y);
void _gdk_wayland_surface_set_grab_seat (GdkSurface      *surface,
                                         GdkSeat         *seat);

struct wl_output *gdk_wayland_surface_get_wl_output (GdkSurface *surface);

void gdk_wayland_surface_inhibit_shortcuts (GdkSurface *surface,
                                           GdkSeat   *gdk_seat);
void gdk_wayland_surface_restore_shortcuts (GdkSurface *surface,
                                           GdkSeat   *gdk_seat);

#define XDG_SHELL_CALL(obj,func,surface,...) \
  switch (GDK_WAYLAND_DISPLAY (gdk_surface_get_display (GDK_SURFACE (surface)))->shell_variant) \
    { \
    case GDK_WAYLAND_SHELL_VARIANT_XDG_SHELL: \
      obj ## _ ## func (surface->display_server.obj __VA_OPT__(,) __VA_ARGS__ ); \
      break; \
    case GDK_WAYLAND_SHELL_VARIANT_ZXDG_SHELL_V6: \
      z ## obj ## _v6_ ## func (surface->display_server.z ## obj ## _v6 __VA_OPT__(,) __VA_ARGS__ ); \
      break; \
    default: \
      g_assert_not_reached (); \
    }
